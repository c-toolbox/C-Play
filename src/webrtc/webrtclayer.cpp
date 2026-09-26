/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "application.h"
#include "webrtc/webrtclayer.h"

#include "audiosettings.h"
#include "webrtc/videodecoder.h"
#include "webrtc/webrtchub.h"
#include "webrtc/webrtcrelayclient.h"
#include "webrtc/webrtcsource.h"

#include <sgct/sgct.h>
#include <sgct/opengl.h>

#include <QCoreApplication>
#include <QObject>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <string>
namespace {

// Bounds of the Annex-B queue. It holds the (compressed) playout delay in sync mode, since
// the decoder only runs a few frames ahead of the presentation target; beyond that span
// the queue restarts at the next keyframe instead of growing the latency.
constexpr std::size_t kMaxQueuedUnits = 256;
constexpr std::int64_t kMaxQueueSpanUnsyncedMs = 250;
constexpr std::int64_t kQueueSpanSyncMarginMs = 500;
constexpr std::size_t kMaxQueuedAudioPackets = 64;  // bounded Opus payload queue (~1.3 s at 20 ms frames)
constexpr int kReconnectDelayMs = 2000;          // wait before retrying a failed session
constexpr auto kStarvedForKeyframe = std::chrono::seconds(3);
// A decoded frame this far ahead of the presentation target belongs to another timeline
// (sender restart); it must not block the decoder waiting for its turn.
constexpr std::int64_t kMaxLookaheadTicks = 2 * kWebRtcVideoClockRate;
constexpr auto kSlotWaitStep = std::chrono::milliseconds(5);
constexpr auto kUnshownLogInterval = std::chrono::seconds(10);

std::size_t planeBytes(VideoPixelLayout layout, int width, int height, int plane) {
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    const std::size_t cw = (w + 1) / 2;
    const std::size_t ch = (h + 1) / 2;
    switch (layout) {
    case VideoPixelLayout::Rgba:
        return plane == 0 ? w * h * 4 : 0;
    case VideoPixelLayout::Nv12:
        return plane == 0 ? w * h : plane == 1 ? cw * 2 * ch : 0;
    case VideoPixelLayout::Yuv420p:
        return plane == 0 ? w * h : cw * ch;
    }
    return 0;
}

/// Bytes per row of a packed plane (see planeBytes()).
int planeRowBytes(VideoPixelLayout layout, int width, int plane) {
    const int cw = (width + 1) / 2;
    switch (layout) {
    case VideoPixelLayout::Rgba:
        return width * 4;
    case VideoPixelLayout::Nv12:
        return plane == 0 ? width : cw * 2;
    case VideoPixelLayout::Yuv420p:
        return plane == 0 ? width : cw;
    }
    return 0;
}

int planeCount(VideoPixelLayout layout) {
    return layout == VideoPixelLayout::Rgba ? 1 : layout == VideoPixelLayout::Nv12 ? 2 : 3;
}

// Full-screen triangle generated from gl_VertexID; no vertex buffer needed.
constexpr const char *kYuvVertexShader = R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Row 0 of every plane is the top image row, exactly like the RGBA upload, so the output
// keeps the layer's flipY convention.
constexpr const char *kYuvFragmentShader = R"(#version 330 core
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D texY;
uniform sampler2D texU; // NV12: interleaved UV in .rg; I420: U in .r
uniform sampler2D texV; // I420 only
uniform int nv12;
uniform mat3 yuvToRgb;
uniform vec3 yuvOffset;
void main() {
    float y = texture(texY, vUv).r;
    vec2 uv = nv12 == 1 ? texture(texU, vUv).rg : vec2(texture(texU, vUv).r, texture(texV, vUv).r);
    vec3 rgb = yuvToRgb * (vec3(y, uv) - yuvOffset);
    fragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";

GLuint compileShader(GLenum type, const char *source, std::string &log) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char buffer[1024] = {};
        glGetShaderInfoLog(shader, sizeof(buffer), nullptr, buffer);
        log = buffer;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

/// Column-major YCbCr -> RGB matrix and offset for the given matrix and range.
void yuvToRgbCoefficients(VideoColorMatrix matrix, bool fullRange, float (&m)[9], float (&offset)[3]) {
    float kr = 0.2126f;
    float kb = 0.0722f;
    if (matrix == VideoColorMatrix::Bt601) {
        kr = 0.299f;
        kb = 0.114f;
    } else if (matrix == VideoColorMatrix::Bt2020) {
        kr = 0.2627f;
        kb = 0.0593f;
    }
    const float kg = 1.f - kr - kb;
    const float ys = fullRange ? 1.f : 255.f / 219.f;
    const float cs = fullRange ? 1.f : 255.f / 224.f;

    const float crToR = 2.f * (1.f - kr);
    const float cbToB = 2.f * (1.f - kb);
    const float cbToG = 2.f * kb * (1.f - kb) / kg;
    const float crToG = 2.f * kr * (1.f - kr) / kg;

    // column 0: Y, column 1: Cb, column 2: Cr
    m[0] = ys;           m[1] = ys;            m[2] = ys;
    m[3] = 0.f;          m[4] = -cbToG * cs;   m[5] = cbToB * cs;
    m[6] = crToR * cs;   m[7] = -crToG * cs;   m[8] = 0.f;

    offset[0] = fullRange ? 0.f : 16.f / 255.f;
    offset[1] = 128.f / 255.f;
    offset[2] = 128.f / 255.f;
}

/// Saves and restores the GL state touched by the YUV conversion pass. The layer is updated
/// inside SGCT's frame and inside Qt's render pass, and neither expects its state to change.
class ScopedGlState {
public:
    ScopedGlState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFbo);
        glGetIntegerv(GL_VIEWPORT, m_viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &m_program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &m_activeTexture);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &m_unpackBuffer);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &m_unpackAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &m_unpackRowLength);
        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_textures[i]);
            glGetIntegerv(GL_SAMPLER_BINDING, &m_samplers[i]);
        }
        glActiveTexture(static_cast<GLenum>(m_activeTexture));
        m_blend = glIsEnabled(GL_BLEND);
        m_scissor = glIsEnabled(GL_SCISSOR_TEST);
        m_depth = glIsEnabled(GL_DEPTH_TEST);
        m_stencil = glIsEnabled(GL_STENCIL_TEST);
        m_cull = glIsEnabled(GL_CULL_FACE);
        glGetBooleanv(GL_COLOR_WRITEMASK, m_colorMask);
    }

    ~ScopedGlState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_drawFbo));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_readFbo));
        glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
        glUseProgram(static_cast<GLuint>(m_program));
        glBindVertexArray(static_cast<GLuint>(m_vao));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(m_unpackBuffer));
        glPixelStorei(GL_UNPACK_ALIGNMENT, m_unpackAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, m_unpackRowLength);
        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_textures[i]));
            glBindSampler(i, static_cast<GLuint>(m_samplers[i]));
        }
        glActiveTexture(static_cast<GLenum>(m_activeTexture));
        setEnabled(GL_BLEND, m_blend);
        setEnabled(GL_SCISSOR_TEST, m_scissor);
        setEnabled(GL_DEPTH_TEST, m_depth);
        setEnabled(GL_STENCIL_TEST, m_stencil);
        setEnabled(GL_CULL_FACE, m_cull);
        glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    }

    ScopedGlState(const ScopedGlState &) = delete;
    ScopedGlState &operator=(const ScopedGlState &) = delete;

private:
    static void setEnabled(GLenum cap, GLboolean enabled) {
        if (enabled) {
            glEnable(cap);
        } else {
            glDisable(cap);
        }
    }

    GLint m_drawFbo = 0;
    GLint m_readFbo = 0;
    GLint m_viewport[4] = {};
    GLint m_program = 0;
    GLint m_vao = 0;
    GLint m_activeTexture = GL_TEXTURE0;
    GLint m_unpackBuffer = 0;
    GLint m_unpackAlignment = 4;
    GLint m_unpackRowLength = 0;
    GLint m_textures[3] = {};
    GLint m_samplers[3] = {};
    GLboolean m_blend = GL_FALSE;
    GLboolean m_scissor = GL_FALSE;
    GLboolean m_depth = GL_FALSE;
    GLboolean m_stencil = GL_FALSE;
    GLboolean m_cull = GL_FALSE;
    GLboolean m_colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
};

/// Chooses the PortAudio output device, mirroring the NDI/OMT layers: the default
/// device unless a custom one is configured in the audio settings.
PaDeviceIndex chosenAudioDevice() {
    PaDeviceIndex choseDeviceIdx = Pa_GetDefaultOutputDevice();
    if (choseDeviceIdx == paNoDevice) {
        sgct::Log::Error("WebRTCLayer: no default audio output device.\n");
    }

    if (AudioSettings::portAudioCustomOutput()) {
        if (!AudioSettings::portAudioOutputDevice().isEmpty()
            && !AudioSettings::portAudioOutputApi().isEmpty()) {
            const int numDevices = Pa_GetDeviceCount();
            if (numDevices < 0) {
                return choseDeviceIdx;
            }

            bool foundDevice = false;
            for (int i = 0; i < numDevices; ++i) {
                const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
                const PaHostApiInfo *apiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
                if (!deviceInfo || !apiInfo || deviceInfo->maxOutputChannels <= 1) {
                    continue;
                }

                const QString deviceName = QString::fromUtf8(deviceInfo->name);
                const QString apiName = QString::fromUtf8(apiInfo->name);
                if (deviceName == AudioSettings::portAudioOutputDevice()
                    && apiName == AudioSettings::portAudioOutputApi()) {
                    choseDeviceIdx = i;
                    foundDevice = true;
                }
            }

            if (!foundDevice) {
                sgct::Log::Info("WebRTCLayer: did not find the desired audio device, "
                                "sticking with the default.\n");
            }
        }
    }

    return choseDeviceIdx;
}

} // namespace

WebRTCLayer::WebRTCLayer() {
    m_type = WEBRTC;
    // swscale writes RGBA top-down, OpenGL samples bottom-up.
    renderData.flipY = true;

    // The decoded-PCM callback fires on the audio decode worker thread (synchronously
    // inside AudioDecoder::decode) and pushes straight into the PortAudio stream. Set it
    // once here: startSource() may run again mid-session (URL change/reconnect) while
    // that worker is already decoding, so re-setting it there would race with it.
    std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;
    m_audioDecoder.setOnFrame([this, weakAlive](const float *pcm, int sampleRate, int channels, int frames) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        pushDecodedPcm(pcm, sampleRate, channels, frames);
    });
}

WebRTCLayer::~WebRTCLayer() {
    *m_alive = false;
    stop();
}

void WebRTCLayer::cleanup() {
    // Always release the NDI output first (base class behavior).
    BaseLayer::cleanup();

    stop();

    releaseGlResources();
}

void WebRTCLayer::releaseGlResources() {
    if (renderData.texId && renderData.width > 0) {
        glDeleteTextures(1, &renderData.texId);
        renderData.texId = 0;
        renderData.width = 0;
        renderData.height = 0;
    }
    for (std::size_t i = 0; i < m_planeTextures.size(); ++i) {
        if (m_planeTextures[i]) {
            glDeleteTextures(1, &m_planeTextures[i]);
            m_planeTextures[i] = 0;
        }
        m_planeWidths[i] = 0;
        m_planeHeights[i] = 0;
    }
    if (m_yuvProgram) {
        glDeleteProgram(m_yuvProgram);
        m_yuvProgram = 0;
    }
}

void WebRTCLayer::initialize() {
    m_hasInitialized = true;
}

void WebRTCLayer::update(bool updateRendering) {
    ensureStarted();

    if (updateRendering && ready()) {
        updateFrame();
    }
}

void WebRTCLayer::ensureStarted() {
    const float visibility = alpha();
    if (visibility > 0.f && m_lastSeenAlpha <= 0.f) {
        // Becoming visible again overrides an earlier stop request.
        m_explicitlyStopped.store(false, std::memory_order_relaxed);
    }
    m_lastSeenAlpha = visibility;

    if (visibility <= 0.f
        || m_shouldRun.load(std::memory_order_relaxed)
        || m_startPending.load(std::memory_order_relaxed)
        || m_explicitlyStopped.load(std::memory_order_relaxed)
        || filepath().empty()) {
        return;
    }

    // update() runs on the render thread, but the WHEP client and its
    // QNetworkAccessManager must live on the main thread.
    m_startPending.store(true, std::memory_order_relaxed);
    std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;
    QMetaObject::invokeMethod(qApp, [this, weakAlive] {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        m_startPending.store(false, std::memory_order_relaxed);
        if (!m_explicitlyStopped.load(std::memory_order_relaxed)) {
            start();
        }
    }, Qt::QueuedConnection);
}

bool WebRTCLayer::ready() const {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    return m_latestWidth > 0 && m_latestHeight > 0;
}

bool WebRTCLayer::hasTexture() const {
    return renderData.texId != 0 && renderData.width > 0 && renderData.height > 0;
}

bool WebRTCLayer::renderingIsOn() const {
    return ready();
}

void WebRTCLayer::start() {
    m_explicitlyStopped.store(false, std::memory_order_relaxed);

    if (m_shouldRun.load(std::memory_order_relaxed)) {
        return;
    }

    const std::string url = filepath();
    if (url.empty()) {
        sgct::Log::Error("WebRTCLayer: no WHEP URL set, cannot start.\n");
        return;
    }

    m_shouldRun.store(true, std::memory_order_relaxed);

    // Reset the pipeline before the source can deliver anything.
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopRequested = false;
        m_queueNeedsKeyframe = true;
        m_queue.clear();
    }
    m_decodeStop.store(false, std::memory_order_relaxed);
    resetFrameRing();

    startSource();

    m_decodeThread = std::thread([this] { decodeLoop(); });

    // The audio decode worker mirrors the video one: payloads are queued from the media
    // thread and decoded here.
    {
        std::lock_guard<std::mutex> lock(m_audioQueueMutex);
        m_audioStopRequested = false;
        m_audioQueue.clear();
    }
    m_audioReopenRequested.store(false, std::memory_order_relaxed);
    m_audioDecodeThread = std::thread([this] { audioDecodeLoop(); });
}

void WebRTCLayer::stop() {
    m_explicitlyStopped.store(true, std::memory_order_relaxed);

    if (!m_shouldRun.load(std::memory_order_relaxed) && !m_source) {
        return;
    }

    m_shouldRun.store(false, std::memory_order_relaxed);
    stopSource();

    // Stop the audio decode worker first: it owns the PortAudio stream and the Opus
    // decoder, so they can only be torn down on the main thread once it has exited.
    {
        std::lock_guard<std::mutex> lock(m_audioQueueMutex);
        m_audioStopRequested = true;
    }
    m_audioQueueCv.notify_all();

    if (m_audioDecodeThread.joinable()) {
        m_audioDecodeThread.join();
    }

    // The decoder is reopened lazily on the next session's first payload.
    stopAudioOutput();
    m_audioDecoder.close();
    m_audioDecodeDisabled.store(false, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopRequested = true;
    }
    m_queueCv.notify_all();
    // The decoder may be waiting for a free ring slot.
    {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        m_decodeStop.store(true, std::memory_order_relaxed);
    }
    m_slotFreedCv.notify_all();

    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }

    resetFrameRing();
    std::lock_guard<std::mutex> frameLock(m_frameMutex);
    m_latestWidth = 0;
    m_latestHeight = 0;
}

bool WebRTCLayer::pause() {
    return !m_shouldRun.load(std::memory_order_relaxed);
}

void WebRTCLayer::setPause(bool value) {
    if (value) {
        stop();
    } else {
        start();
    }
}

void WebRTCLayer::setWhepUrl(std::string url) {
    if (url == filepath()) {
        return;
    }

    BaseLayer::setFilePath(url); // stores the URL and marks the layer for sync
    if (m_shouldRun.load(std::memory_order_relaxed)) {
        startSource(); // restarts with the new URL (stopSource() runs inside)
    }
}

void WebRTCLayer::setAuthUsername(std::string username) {
    if (username == m_authUsername) {
        return;
    }

    m_authUsername = std::move(username);
    setNeedSync();
    if (m_shouldRun.load(std::memory_order_relaxed)) {
        startSource(); // reconnect so the new Authorization header takes effect
    }
}

void WebRTCLayer::setAuthPassword(std::string password) {
    if (password == m_authPassword) {
        return;
    }

    m_authPassword = std::move(password);
    setNeedSync();
    if (m_shouldRun.load(std::memory_order_relaxed)) {
        startSource(); // reconnect so the new Authorization header takes effect
    }
}

void WebRTCLayer::setMasterRelayEnabled(bool enabled) {
    if (enabled == m_masterRelayEnabled) {
        return;
    }

    m_masterRelayEnabled = enabled;
    setNeedSync();
    if (m_shouldRun.load(std::memory_order_relaxed)) {
        startSource(); // switch transport: hub relay <-> direct WHEP pull
    }
}

void WebRTCLayer::setSyncDelayMs(int delayMs) {
    delayMs = std::clamp(delayMs, kWebRtcMinSyncDelayMs, kWebRtcMaxSyncDelayMs);
    if (delayMs == m_syncDelayMs.load(std::memory_order_relaxed)) {
        return;
    }
    m_syncDelayMs.store(delayMs, std::memory_order_relaxed);
    setNeedSync();
}

void WebRTCLayer::encodeTypeCore(std::vector<std::byte> &data) {
    sgct::serializeObject(data, m_authUsername);
    sgct::serializeObject(data, m_authPassword);
    sgct::serializeObject(data, m_masterRelayEnabled);
    sgct::serializeObject(data, m_syncDelayMs.load(std::memory_order_relaxed));
}

void WebRTCLayer::decodeTypeCore(const std::vector<std::byte> &data, unsigned int &pos) {
    // Master and nodes always run the same build (the relay protocol version is checked), so
    // the fields are read unconditionally; a size check cannot detect missing fields here
    // anyway, since the buffer continues with the next section.
    sgct::deserializeObject(data, pos, m_authUsername);
    sgct::deserializeObject(data, pos, m_authPassword);
    sgct::deserializeObject(data, pos, m_masterRelayEnabled);
    int syncDelayMs = kWebRtcDefaultSyncDelayMs;
    sgct::deserializeObject(data, pos, syncDelayMs);
    m_syncDelayMs.store(std::clamp(syncDelayMs, kWebRtcMinSyncDelayMs, kWebRtcMaxSyncDelayMs),
                        std::memory_order_relaxed);
}

void WebRTCLayer::encodeTypeAlways(std::vector<std::byte> &data) {
    // Evaluated once per SGCT frame on the master, right before the nodes render it.
    bool valid = false;
    std::int64_t target = 0;
    if (isMaster() && m_syncMode.load(std::memory_order_relaxed) == SyncMode::MasterClock) {
        const auto now = WebRtcPlayoutClock::Clock::now();
        valid = m_playoutClock.target(now, syncDelayMs(), target);
        // The master's own SGCT window renders this frame too: show exactly what the nodes show.
        m_nodeTargetPts.store(target, std::memory_order_relaxed);
        m_nodeTargetValid.store(valid, std::memory_order_release);
        m_sentTargetTicks.store(now.time_since_epoch().count(), std::memory_order_relaxed);
    }
    sgct::serializeObject(data, valid);
    sgct::serializeObject(data, target);
}

void WebRTCLayer::decodeTypeAlways(const std::vector<std::byte> &data, unsigned int &pos) {
    bool valid = false;
    std::int64_t target = 0;
    sgct::deserializeObject(data, pos, valid);
    sgct::deserializeObject(data, pos, target);
    if (isMaster()) {
        return; // in-process layer copies: the master runs its own clock
    }
    m_nodeTargetPts.store(target, std::memory_order_relaxed);
    m_nodeTargetValid.store(valid, std::memory_order_release);
    if (valid) {
        m_slotFreedCv.notify_all(); // the decoder may be waiting for the target to advance
    }
}

bool WebRTCLayer::presentationTarget(std::int64_t &target) {
    switch (m_syncMode.load(std::memory_order_relaxed)) {
    case SyncMode::MasterClock: {
        const auto now = WebRtcPlayoutClock::Clock::now();
        // Within the SGCT frame that sent a target, use that one (frame-exact with the nodes);
        // otherwise (no SGCT frame running, e.g. only previews) sample the clock directly.
        const auto sentAt = WebRtcPlayoutClock::Clock::time_point(
            WebRtcPlayoutClock::Clock::duration(m_sentTargetTicks.load(std::memory_order_relaxed)));
        if (now - sentAt < std::chrono::milliseconds(100) && m_nodeTargetValid.load(std::memory_order_acquire)) {
            target = m_nodeTargetPts.load(std::memory_order_relaxed);
            return true;
        }
        return m_playoutClock.target(now, syncDelayMs(), target);
    }
    case SyncMode::NodeTarget:
        if (!m_nodeTargetValid.load(std::memory_order_acquire)) {
            return false;
        }
        target = m_nodeTargetPts.load(std::memory_order_relaxed);
        return true;
    case SyncMode::None:
        break;
    }
    return false;
}

bool WebRTCLayer::existOnMasterOnly() const {
    return m_existOnMasterOnly;
}

WebRtcStreamConfig WebRTCLayer::buildConfig() const {
    WebRtcStreamConfig config;
    config.whepUrl = QUrl(QString::fromStdString(filepath()));
    // HTTP Basic auth is carried as an Authorization header on the WHEP request. The explicit
    // layer credentials take precedence; if they are empty, WhepClient falls back to any
    // credentials embedded in the URL (user:pass@host).
    config.username = QString::fromStdString(m_authUsername);
    config.password = QString::fromStdString(m_authPassword);
    return config;
}

void WebRTCLayer::startSource() {
    stopSource();

    const std::string layerId = std::to_string(identifier());
    // Nodes do not pull WHEP themselves by default (that would make every node hit the same
    // upstream address): the master pulls once and relays to all nodes through its WebRtcHub.
    // The per-layer relay option can switch that off, in which case each machine with the
    // layer pulls its own copy of the stream directly.
    const bool relayToNodes = isMaster() && !existOnMasterOnly() && masterRelayEnabled();

    WebRtcMediaSource *source = nullptr;
    if (isMaster()) {
        auto *whep = new WebRtcSource(); // no parent; we own it and deleteLater() it
        whep->setConfig(buildConfig());
        source = whep;

        if (relayToNodes) {
            std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;
            WebRtcHub::instance().registerFeed(layerId, [weakAlive, whep] {
                auto alive = weakAlive.lock();
                if (!alive || !*alive) {
                    return;
                }
                // A node just joined: let it start decoding at an IDR frame.
                whep->requestKeyframe();
            });
        }
    } else if (masterRelayEnabled()) {
        // The master's address comes from the SGCT cluster config; the node reaches the hub on
        // that host (see WebRtcHub).
        source = new WebRtcRelayClient(sgct::ClusterManager::instance().masterAddress(),
                                       kWebRtcHubPort, layerId);
    } else {
        // Relay disabled: this node pulls its own copy of the stream directly.
        auto *whep = new WebRtcSource();
        whep->setConfig(buildConfig());
        source = whep;
    }

    // The layer may be destroyed while the source is still pending deletion or a
    // signal is queued, so every callback checks the alive flag before touching
    // any member.
    std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;

    // Presentation sync only exists together with the relay: the master's clock is what
    // the nodes follow, and a directly pulled stream has no common timeline anyway.
    if (isMaster()) {
        m_syncMode.store(relayToNodes ? SyncMode::MasterClock : SyncMode::None, std::memory_order_relaxed);
    } else {
        m_syncMode.store(masterRelayEnabled() ? SyncMode::NodeTarget : SyncMode::None, std::memory_order_relaxed);
    }
    m_playoutClock.reset();

    source->setVideoCallback([this, weakAlive, relayToNodes, layerId](const WebRtcVideoFrame &frame) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        if (m_syncMode.load(std::memory_order_relaxed) == SyncMode::MasterClock) {
            m_playoutClock.onFrame(frame.pts, WebRtcPlayoutClock::Clock::now());
        }
        // Fan the frame out to the nodes first (non-blocking, per-node sender threads), then
        // queue the local copy.
        if (relayToNodes) {
            WebRtcHub::instance().publishVideo(layerId, frame.data, frame.size, frame.pts, frame.keyframe);
        }
        pushAnnexB(frame);
    });

    // Audio: depacketized Opus payloads arrive on a libdatachannel thread and are only
    // copied into the bounded queue; decoding runs on the audio worker (audioDecodeLoop()).
    source->setAudioCallback([this, weakAlive, relayToNodes, layerId](const std::uint8_t *data,
                                                                      std::size_t size,
                                                                      std::int64_t pts) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        if (relayToNodes) {
            WebRtcHub::instance().publishAudio(layerId, data, size, pts);
        }
        pushAudioPayload(data, size);
    });

    // The layer is plain C++, so the source itself is the context object: the
    // connections then die with it, and delivery is queued onto the main thread when a
    // signal is emitted from a libdatachannel thread. A null context would create a
    // connection that never fires. weakAlive still guards against the layer being
    // destroyed while a signal is queued.
    QObject::connect(source, &WebRtcMediaSource::videoCodecNegotiated, source, [this, weakAlive, relayToNodes, layerId](WebRtcVideoCodec codec) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        m_negotiatedCodec.store(codec, std::memory_order_relaxed);
        // Units and pictures from a previous session are useless to the fresh decoder.
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_queue.clear();
            m_queueNeedsKeyframe = true;
        }
        m_queueCv.notify_all();
        resetFrameRing();
        if (relayToNodes) {
            WebRtcHub::instance().updateFeedCodec(layerId, codec);
        }
    });

    QObject::connect(source, &WebRtcMediaSource::stateChanged, source, [this, weakAlive](WebRtcStreamState state) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        if (state == WebRtcStreamState::Failed && m_shouldRun.load(std::memory_order_relaxed)) {
            scheduleReconnect();
        }
    });

    QObject::connect(source, &WebRtcMediaSource::errorOccurred, source, [weakAlive](const QString &message) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        sgct::Log::Error("WebRTCLayer: " + message.toStdString() + "\n");
    });

    m_source = source;
    source->start();
}

void WebRTCLayer::stopSource() {
    if (!m_source) {
        return;
    }

    // Detach the media callbacks first so no new units arrive while tearing down.
    // In-flight callbacks are guarded by the alive flag and the queue mutexes.
    m_source->setVideoCallback(nullptr);
    m_source->setAudioCallback(nullptr);
    // Unregister the hub feed before stopping so no new fan-out or keyframe requests arrive.
    if (isMaster()) {
        WebRtcHub::instance().unregisterFeed(std::to_string(identifier()));
    }
    m_source->stop(); // WHEP DELETE + close peer connection / relay session
    m_source->deleteLater();
    m_source = nullptr;
}

void WebRTCLayer::scheduleReconnect() {
    if (!m_shouldRun.load(std::memory_order_relaxed) || m_reconnectScheduled) {
        return;
    }
    m_reconnectScheduled = true;

    std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;
    QTimer::singleShot(kReconnectDelayMs, qApp, [this, weakAlive] {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        m_reconnectScheduled = false;
        if (m_shouldRun.load(std::memory_order_relaxed)) {
            startSource(); // stopSource() inside handles the failed session
        }
    });
}

void WebRTCLayer::requestKeyframeAsync() {
    // m_source is owned by the main thread, so hop there before touching it.
    std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;
    QMetaObject::invokeMethod(qApp, [this, weakAlive] {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        if (m_source && m_shouldRun.load(std::memory_order_relaxed)) {
            m_source->requestKeyframe();
        }
    }, Qt::QueuedConnection);
}

void WebRTCLayer::pushAnnexB(const WebRtcVideoFrame &frame) {
    if (!frame.data || frame.size == 0) {
        return;
    }

    bool overflowed = false;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (m_stopRequested) {
            return;
        }

        // In sync mode the queue holds the playout delay; otherwise it should stay nearly
        // empty. Past the limit the decoder is behind: dropping single units would corrupt
        // every following picture, so restart at the next keyframe instead.
        const bool synced = m_syncMode.load(std::memory_order_relaxed) != SyncMode::None;
        const std::int64_t spanLimitMs = synced ? syncDelayMs() + kQueueSpanSyncMarginMs
                                                : kMaxQueueSpanUnsyncedMs;
        const std::int64_t spanLimit = spanLimitMs * kWebRtcVideoClockRate / 1000;
        if (!m_queue.empty()
            && (m_queue.size() >= kMaxQueuedUnits || frame.pts - m_queue.front().pts > spanLimit)) {
            m_queue.clear();
            if (!m_queueNeedsKeyframe) {
                m_queueNeedsKeyframe = true;
                overflowed = true;
            }
        }

        bool discontinuity = frame.discontinuity;
        if (m_queueNeedsKeyframe && frame.keyframe) {
            m_queueNeedsKeyframe = false;
            discontinuity = true; // the decoder must not reference what was dropped
        }

        if (!m_queueNeedsKeyframe) {
            WebRtcAnnexBUnit unit;
            unit.data.assign(frame.data, frame.data + frame.size); // the RTP buffer dies with the callback
            unit.pts = frame.pts;
            unit.keyframe = frame.keyframe;
            unit.discontinuity = discontinuity;
            m_queue.push_back(std::move(unit));
            m_queueCv.notify_one();
        }
    }

    if (overflowed) {
        sgct::Log::Warning("WebRTCLayer: video decode fell behind the stream; skipping to the next keyframe\n");
        requestKeyframeAsync();
    }
}

void WebRTCLayer::decodeLoop() {
    auto loggedFirstFrame = std::make_shared<bool>(false);
    m_decoder.setOnFrame([this, loggedFirstFrame](const DecodedVideoFrame &frame) {
        if (!*loggedFirstFrame) {
            *loggedFirstFrame = true;
            sgct::Log::Info("WebRTCLayer: first decoded frame " + std::to_string(frame.width) + "x"
                            + std::to_string(frame.height)
                            + (m_decoder.isHardwareDecoder() ? " (NVDEC)\n" : " (software)\n"));
        }
        storeDecodedFrame(frame);
    });

    WebRtcVideoCodec openCodec = WebRtcVideoCodec::Unknown;
    bool waitingForKeyframe = true;
    auto lastOpenAttempt = std::chrono::steady_clock::now();
    auto lastKeyframeRequest = lastOpenAttempt;
    auto lastUnshownLog = lastOpenAttempt;
    std::uint64_t loggedUnshown = 0;

    while (true) {
        WebRtcAnnexBUnit unit;
        bool haveUnit = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait_for(lock, std::chrono::milliseconds(200), [this, &openCodec] {
                const auto codec = m_negotiatedCodec.load(std::memory_order_relaxed);
                return m_stopRequested || !m_queue.empty() || codec != openCodec;
            });

            if (m_stopRequested) {
                break;
            }

            if (!m_queue.empty()) {
                unit = std::move(m_queue.front());
                m_queue.pop_front();
                haveUnit = true;
            }
        }

        m_decoder.setForceRgba(m_forceRgbaOutput.load(std::memory_order_relaxed));

        // Decoder management runs outside m_queueMutex: opening a codec can take tens of
        // milliseconds and must not stall the libdatachannel thread feeding the queue.
        const auto codec = m_negotiatedCodec.load(std::memory_order_relaxed);
        if (codec != WebRtcVideoCodec::Unknown && codec != openCodec && m_decoder.isOpen()) {
            m_decoder.close(); // a renegotiated session switched codec
        }
        if (codec != WebRtcVideoCodec::Unknown && !m_decoder.isOpen()) {
            const auto now = std::chrono::steady_clock::now();
            if (codec != openCodec || now - lastOpenAttempt > std::chrono::seconds(2)) {
                lastOpenAttempt = now;
                openCodec = codec; // recorded on attempt so failures fall back to the throttle
                *loggedFirstFrame = false;
                waitingForKeyframe = true;
                QString error;
                if (!m_decoder.open(codec, &error)) {
                    sgct::Log::Error("WebRTCLayer: " + error.toStdString() + "\n");
                }
            }
        }

        // Starved check: no decoded frame for a while (e.g. joined mid-GOP and the
        // sender never sent an IDR) -> ask for a keyframe.
        {
            const auto now = std::chrono::steady_clock::now();
            bool hasFrame = false;
            std::uint64_t unshown = 0;
            {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                hasFrame = m_latestWidth > 0 && m_latestHeight > 0;
                unshown = m_unshownFrames;
            }
            if (!hasFrame && now - lastKeyframeRequest > kStarvedForKeyframe) {
                lastKeyframeRequest = now;
                requestKeyframeAsync();
            }
            if (now - lastUnshownLog > kUnshownLogInterval) {
                lastUnshownLog = now;
                if (unshown != loggedUnshown) {
                    sgct::Log::Info("WebRTCLayer: " + std::to_string(unshown - loggedUnshown)
                                    + " decoded frames were not shown in the last "
                                    + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(kUnshownLogInterval).count())
                                    + " s (render rate below the stream rate, or late frames)\n");
                    loggedUnshown = unshown;
                }
            }
        }

        if (!haveUnit || !m_decoder.isOpen()) {
            continue;
        }

        if (unit.discontinuity) {
            // Units were dropped before this one (here or in the master's relay): the
            // references are gone, so restart the decoder at this random access point.
            m_decoder.flush();
            waitingForKeyframe = true;
        }
        if (waitingForKeyframe) {
            if (!unit.keyframe) {
                continue;
            }
            waitingForKeyframe = false;
        }

        QString error;
        if (!m_decoder.decode(unit.data.data(), unit.data.size(), unit.pts, &error)) {
            // Fatal decode error: drop the context so a fresh one can resync on the
            // next keyframe. openCodec is left alone so the throttle paces the re-open.
            sgct::Log::Error("WebRTCLayer: " + error.toStdString() + "\n");
            m_decoder.close();
        }
    }

    m_decoder.close();
}

void WebRTCLayer::resetFrameRing() {
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        for (auto &slot : m_slots) {
            if (!slot.pinned && !slot.writing) {
                slot.seq = 0;
            }
        }
        m_hasNewestPts = false;
    }
    m_slotFreedCv.notify_all();
}

int WebRTCLayer::reusableSlot(bool hasTarget, std::int64_t target) {
    // 1. Empty, or already handed to the renderer.
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        const auto &slot = m_slots[i];
        if (!slot.pinned && !slot.writing && (slot.seq == 0 || slot.seq <= m_displayedSeq)) {
            return static_cast<int>(i);
        }
    }

    const auto isDue = [&](const WebRtcFrameSlot &slot) { return !hasTarget || slot.pts <= target; };

    // 2. A due picture that a newer due picture already supersedes: the renderer would
    //    skip it anyway.
    int newestDue = -1;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        const auto &slot = m_slots[i];
        if (!slot.pinned && !slot.writing && isDue(slot)
            && (newestDue < 0 || slot.seq > m_slots[newestDue].seq)) {
            newestDue = static_cast<int>(i);
        }
    }
    int victim = -1;
    for (std::size_t i = 0; i < m_slots.size(); ++i) {
        const auto &slot = m_slots[i];
        if (static_cast<int>(i) == newestDue || slot.pinned || slot.writing || !isDue(slot)) {
            continue;
        }
        if (victim < 0 || slot.seq < m_slots[victim].seq) {
            victim = static_cast<int>(i);
        }
    }
    if (victim >= 0) {
        ++m_unshownFrames;
        return victim;
    }

    // 3. Everything is waiting for its turn. Normally the decoder then waits too (that is
    //    what paces it to the presentation target), unless the pictures are so far ahead
    //    that they cannot belong to the target's timeline.
    if (hasTarget) {
        int oldest = -1;
        for (std::size_t i = 0; i < m_slots.size(); ++i) {
            const auto &slot = m_slots[i];
            if (!slot.pinned && !slot.writing && (oldest < 0 || slot.seq < m_slots[oldest].seq)) {
                oldest = static_cast<int>(i);
            }
        }
        if (oldest >= 0 && m_slots[oldest].pts - target > kMaxLookaheadTicks) {
            ++m_unshownFrames;
            return oldest;
        }
    }
    return -1;
}

void WebRTCLayer::storeDecodedFrame(const DecodedVideoFrame &frame) {
    if (frame.width <= 0 || frame.height <= 0) {
        return;
    }

    std::unique_lock<std::mutex> lock(m_frameMutex);

    // A sender restart starts a new timeline; the stored pictures cannot be ordered against it.
    if (m_hasNewestPts
        && (frame.pts < m_newestPts - kWebRtcVideoClockRate
            || frame.pts > m_newestPts + 10 * static_cast<std::int64_t>(kWebRtcVideoClockRate))) {
        for (auto &slot : m_slots) {
            if (!slot.pinned && !slot.writing) {
                slot.seq = 0;
            }
        }
    }

    // In sync mode this waits while the ring only holds pictures that are not due yet, which
    // keeps the playout delay in the (small, compressed) Annex-B queue.
    int index = -1;
    while (true) {
        if (m_decodeStop.load(std::memory_order_relaxed)) {
            return;
        }
        std::int64_t target = 0;
        const bool hasTarget = presentationTarget(target);
        index = reusableSlot(hasTarget, target);
        if (index >= 0) {
            break;
        }
        m_slotFreedCv.wait_for(lock, kSlotWaitStep);
    }

    WebRtcFrameSlot &slot = m_slots[static_cast<std::size_t>(index)];
    slot.writing = true;
    slot.seq = 0;
    lock.unlock();

    // Copy the planes tightly packed, outside the lock (the renderer ignores writing slots).
    const int planes = planeCount(frame.layout);
    std::size_t total = 0;
    for (int p = 0; p < planes; ++p) {
        total += planeBytes(frame.layout, frame.width, frame.height, p);
    }
    slot.data.resize(total);
    std::uint8_t *dst = slot.data.data();
    for (int p = 0; p < planes; ++p) {
        const int rowBytes = planeRowBytes(frame.layout, frame.width, p);
        const int rows = (p == 0) ? frame.height : (frame.height + 1) / 2;
        const std::uint8_t *src = frame.planes[p];
        if (!src) {
            std::fill(dst, dst + static_cast<std::size_t>(rowBytes) * rows, std::uint8_t(0));
        } else if (frame.strides[p] == rowBytes) {
            std::memcpy(dst, src, static_cast<std::size_t>(rowBytes) * rows);
        } else {
            for (int y = 0; y < rows; ++y) {
                std::memcpy(dst + static_cast<std::size_t>(y) * rowBytes,
                            src + static_cast<std::ptrdiff_t>(y) * frame.strides[p], static_cast<std::size_t>(rowBytes));
            }
        }
        dst += static_cast<std::size_t>(rowBytes) * rows;
    }

    lock.lock();
    slot.layout = frame.layout;
    slot.width = frame.width;
    slot.height = frame.height;
    slot.pts = frame.pts;
    slot.matrix = frame.matrix;
    slot.fullRange = frame.fullRange;
    slot.writing = false;
    slot.seq = m_nextFrameSeq++;
    m_newestPts = frame.pts;
    m_hasNewestPts = true;
    m_latestWidth = frame.width;
    m_latestHeight = frame.height;
}

void WebRTCLayer::updateFrame() {
    std::lock_guard<std::mutex> lock(m_updateFrameMutex);

    // Pick the newest picture that is due, pin it, and upload it without holding the
    // decoder's lock.
    int index = -1;
    {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        std::int64_t target = 0;
        const bool hasTarget = presentationTarget(target);
        int oldest = -1;
        for (std::size_t i = 0; i < m_slots.size(); ++i) {
            const auto &slot = m_slots[i];
            if (slot.writing || slot.pinned || slot.seq == 0 || slot.seq <= m_displayedSeq) {
                continue;
            }
            if (oldest < 0 || slot.seq < m_slots[oldest].seq) {
                oldest = static_cast<int>(i);
            }
            if (hasTarget && slot.pts > target) {
                continue;
            }
            if (index < 0 || slot.seq > m_slots[index].seq) {
                index = static_cast<int>(i);
            }
        }
        if (index < 0 && renderData.texId == 0) {
            index = oldest; // nothing on screen yet: show the first picture instead of nothing
        }
        if (index < 0) {
            return; // nothing new, or not due yet
        }
        m_slots[index].pinned = true;
        m_displayedSeq = m_slots[index].seq;
    }
    m_slotFreedCv.notify_all(); // older pictures are free now

    const WebRtcFrameSlot &slot = m_slots[static_cast<std::size_t>(index)]; // pinned: read-only for everyone
    if (slot.layout == VideoPixelLayout::Rgba) {
        uploadRgba(slot);
    } else if (!uploadYuv(slot)) {
        // No usable GPU conversion: let the decoder convert on the CPU from now on.
        m_forceRgbaOutput.store(true, std::memory_order_relaxed);
    }

    {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        m_slots[index].pinned = false;
    }
    m_slotFreedCv.notify_all();
}

void WebRTCLayer::ensureOutputTexture(int width, int height) {
    if (renderData.texId && width == renderData.width && height == renderData.height) {
        return;
    }
    if (renderData.texId && renderData.width > 0) {
        glDeleteTextures(1, &renderData.texId);
    }
    glGenTextures(1, &renderData.texId);
    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    renderData.width = width;
    renderData.height = height;
}

bool WebRTCLayer::uploadRgba(const WebRtcFrameSlot &slot) {
    ensureOutputTexture(slot.width, slot.height);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, slot.width, slot.height, GL_RGBA, GL_UNSIGNED_BYTE, slot.data.data());
    return true;
}

bool WebRTCLayer::ensureYuvProgram() {
    if (m_yuvProgram) {
        return true;
    }
    if (m_yuvProgramFailed) {
        return false;
    }

    std::string log;
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kYuvVertexShader, log);
    const GLuint fs = vs ? compileShader(GL_FRAGMENT_SHADER, kYuvFragmentShader, log) : 0;
    GLuint program = 0;
    if (vs && fs) {
        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            char buffer[1024] = {};
            glGetProgramInfoLog(program, sizeof(buffer), nullptr, buffer);
            log = buffer;
            glDeleteProgram(program);
            program = 0;
        }
    }
    if (vs) {
        glDeleteShader(vs);
    }
    if (fs) {
        glDeleteShader(fs);
    }
    if (!program) {
        m_yuvProgramFailed = true;
        sgct::Log::Error("WebRTCLayer: YUV conversion shader unavailable, converting on the CPU: " + log + "\n");
        return false;
    }

    // The caller restores the program binding.
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "texY"), 0);
    glUniform1i(glGetUniformLocation(program, "texU"), 1);
    glUniform1i(glGetUniformLocation(program, "texV"), 2);
    m_yuvMatrixLocation = glGetUniformLocation(program, "yuvToRgb");
    m_yuvOffsetLocation = glGetUniformLocation(program, "yuvOffset");
    m_yuvNv12Location = glGetUniformLocation(program, "nv12");
    m_yuvProgram = program;
    return true;
}

bool WebRTCLayer::uploadYuv(const WebRtcFrameSlot &slot) {
    const int width = slot.width;
    const int height = slot.height;
    const bool nv12 = slot.layout == VideoPixelLayout::Nv12;
    GLuint fbo = 0;
    GLuint vao = 0;
    bool ok = false;

    {
        ScopedGlState state;
        if (!ensureYuvProgram()) {
            return false;
        }

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glActiveTexture(GL_TEXTURE0);
        ensureOutputTexture(width, height);

        // Upload the planes (R8, plus RG8 for NV12's interleaved chroma).
        const int planes = planeCount(slot.layout);
        const bool layoutChanged = m_planeLayout != slot.layout;
        const std::uint8_t *src = slot.data.data();
        for (int p = 0; p < planes; ++p) {
            const int pw = p == 0 ? width : (width + 1) / 2;
            const int ph = p == 0 ? height : (height + 1) / 2;
            const bool rg = nv12 && p == 1;

            glActiveTexture(GL_TEXTURE0 + p);
            glBindSampler(p, 0);
            if (!m_planeTextures[p]) {
                glGenTextures(1, &m_planeTextures[p]);
                glBindTexture(GL_TEXTURE_2D, m_planeTextures[p]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                m_planeWidths[p] = 0;
            } else {
                glBindTexture(GL_TEXTURE_2D, m_planeTextures[p]);
            }
            if (layoutChanged || m_planeWidths[p] != pw || m_planeHeights[p] != ph) {
                glTexImage2D(GL_TEXTURE_2D, 0, rg ? GL_RG8 : GL_R8, pw, ph, 0, rg ? GL_RG : GL_RED,
                             GL_UNSIGNED_BYTE, nullptr);
                m_planeWidths[p] = pw;
                m_planeHeights[p] = ph;
            }
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pw, ph, rg ? GL_RG : GL_RED, GL_UNSIGNED_BYTE, src);
            src += planeBytes(slot.layout, width, height, p);
        }
        if (nv12) {
            // texV is not sampled for NV12, but keep a valid texture on its unit.
            glActiveTexture(GL_TEXTURE2);
            glBindSampler(2, 0);
            glBindTexture(GL_TEXTURE_2D, m_planeTextures[1]);
        }
        m_planeLayout = slot.layout;

        // Convert into the layer's RGBA texture. Framebuffers and vertex arrays are not
        // shared between GL contexts, so they are created for this pass only.
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderData.texId, 0);
        ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (ok) {
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glViewport(0, 0, width, height);
            glDisable(GL_BLEND);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_STENCIL_TEST);
            glDisable(GL_CULL_FACE);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            float matrix[9];
            float offset[3];
            yuvToRgbCoefficients(slot.matrix, slot.fullRange, matrix, offset);
            glUseProgram(m_yuvProgram);
            glUniformMatrix3fv(m_yuvMatrixLocation, 1, GL_FALSE, matrix);
            glUniform3fv(m_yuvOffsetLocation, 1, offset);
            glUniform1i(m_yuvNv12Location, nv12 ? 1 : 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        } else {
            sgct::Log::Error("WebRTCLayer: YUV conversion framebuffer incomplete, converting on the CPU\n");
        }
    } // restores the caller's GL state before the temporary objects go away

    if (vao) {
        glDeleteVertexArrays(1, &vao);
    }
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
    }
    // Other (shared) contexts sample the texture; make the conversion visible to them.
    glFlush();
    return ok;
}

// ============================================================
// Audio (WHEP Opus -> PortAudio), mirroring the NDI/OMT layers
// ============================================================

bool WebRTCLayer::hasAudio() const {
    return isAudioEnabled() || (isMaster() && AudioSettings::enableAudioOnNodes());
}

bool WebRTCLayer::isAudioEnabled() const {
    return m_isAudioEnabled;
}

void WebRTCLayer::enableAudio(bool enabled) {
    if (m_isAudioEnabled.load(std::memory_order_relaxed) == enabled) {
        return;
    }

    // The audio decode worker reacts on its next iteration: it stops the output and
    // closes the decoder when disabled, and reopens lazily on the first PCM frame.
    m_isAudioEnabled.store(enabled, std::memory_order_release);
}

void WebRTCLayer::updateAudioOutput() {
    // The master follows the global audio settings, like NDI/OMT do.
    if (isMaster()) {
        if (!m_isAudioEnabled && AudioSettings::enableAudioOnMaster()) {
            enableAudio(true);
        } else if (m_isAudioEnabled && !AudioSettings::enableAudioOnMaster()) {
            enableAudio(false);
        }
    }

    // The PortAudio stream belongs to the audio decode worker; ask it to re-check the
    // device/channels and reopen when they changed (nothing to do while no stream is
    // open - it opens lazily on the first PCM frame).
    m_audioReopenRequested.store(true, std::memory_order_relaxed);
    m_audioQueueCv.notify_all();
}

void WebRTCLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }

    m_audioVolume.store(static_cast<float>(v) / 100.f, std::memory_order_relaxed);

    if (isMaster() && AudioSettings::enableAudioOnNodes()) {
        setNeedSync();
    }
}

void WebRTCLayer::setVolumeMute(bool v) {
    if (m_volumeMute.load(std::memory_order_relaxed) == v) {
        return;
    }

    // pushDecodedPcm() reads the flag and zeroes the output while muted.
    m_volumeMute.store(v, std::memory_order_release);
}

void WebRTCLayer::pushAudioPayload(const std::uint8_t *data, std::size_t size) {
    // Media thread: copy the payload into the bounded queue; decoding happens on the audio
    // decode worker.
    // Ignore frames arriving after stop() - they must not (re)open the audio output.
    if (!m_shouldRun.load(std::memory_order_relaxed)) {
        return;
    }

    if (!isAudioEnabled() || !data || size == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_audioQueueMutex);
    if (m_audioStopRequested) {
        return;
    }

    WebRtcAudioUnit unit;
    unit.data.assign(data, data + size); // copy: the source buffer dies with the callback
    m_audioQueue.push_back(std::move(unit));

    while (m_audioQueue.size() > kMaxQueuedAudioPackets) {
        m_audioQueue.pop_front(); // live stream: drop oldest rather than grow latency
    }
    m_audioQueueCv.notify_one();
}

void WebRTCLayer::audioDecodeLoop() {
    while (true) {
        WebRtcAudioUnit unit;
        bool haveUnit = false;

        {
            std::unique_lock<std::mutex> lock(m_audioQueueMutex);
            m_audioQueueCv.wait_for(lock, std::chrono::milliseconds(200), [this] {
                return m_audioStopRequested || !m_audioQueue.empty()
                       || m_audioReopenRequested.load(std::memory_order_relaxed);
            });

            if (m_audioStopRequested && m_audioQueue.empty()) {
                break;
            }

            if (!m_audioQueue.empty()) {
                unit = std::move(m_audioQueue.front());
                m_audioQueue.pop_front();
                haveUnit = true;
            }
        }

        // The audio settings changed on the main thread: re-check device/channels and
        // reopen the output stream here, since it belongs to this thread.
        if (m_audioReopenRequested.exchange(false, std::memory_order_relaxed)) {
            maybeReopenAudioOutput();
        }

        if (!isAudioEnabled()) {
            // Silence immediately; the output reopens lazily on the next PCM frame.
            stopAudioOutput();
            m_audioDecoder.close();
            continue;
        }

        if (m_audioDecodeDisabled.load(std::memory_order_relaxed) || !haveUnit) {
            continue;
        }

        if (!m_audioDecoder.isOpen()) {
            QString error;
            if (!m_audioDecoder.open(&error)) {
                sgct::Log::Error("WebRTCLayer: " + error.toStdString() + "\n");
                m_audioDecodeDisabled.store(true, std::memory_order_relaxed); // do not retry (and spam) on every packet
                continue;
            }
        }

        QString error;
        if (!m_audioDecoder.decode(unit.data.data(), unit.data.size(), &error)) {
            sgct::Log::Error("WebRTCLayer: " + error.toStdString() + "\n");
            m_audioDecoder.close(); // drop the context so a fresh one can resync
        }
    }

    stopAudioOutput();
    m_audioDecoder.close();
}

void WebRTCLayer::maybeReopenAudioOutput() {
    // Nothing running to update; the stream opens lazily on the first PCM frame.
    if (!isAudioEnabled() || !m_audioStreamOpen) {
        return;
    }

    const PaDeviceIndex currentDeviceIdx = m_audioOutputParameters.device;
    const PaDeviceIndex newDeviceIdx = chosenAudioDevice();

    int channelCount = m_audioOutputChannels;
    if (AudioSettings::portAudioMixInputToOutput()) {
        const PaDeviceInfo *devInfo = Pa_GetDeviceInfo(newDeviceIdx);
        if (devInfo) {
            channelCount = std::min(AudioSettings::portAudioOutputChannels(), devInfo->maxOutputChannels);
        }
    } else {
        channelCount = m_audioChannels;
    }

    const bool restartStream = newDeviceIdx != currentDeviceIdx
                               || m_audioOutputParameters.channelCount != channelCount;
    if (!restartStream) {
        return;
    }

    // The stream was open (and therefore started): reopen it on the new device.
    stopAudioOutput();
    m_audioOutputParameters.device = newDeviceIdx;
    m_audioOutputChannels = channelCount;
    startAudioOutput();
}

void WebRTCLayer::pushDecodedPcm(const float *pcm, int sampleRate, int channels, int frames) {
    // Audio decode worker thread only (fires from the decoder's frame callback).
    if (!isAudioEnabled() || !pcm || channels <= 0 || frames <= 0) {
        return;
    }

    // Open the output lazily on the first PCM frame (rate/channels are known now), or
    // reopen it when the stream parameters change mid-session.
    if (!m_audioStreamOpen) {
        m_audioSampleRate = sampleRate > 0 ? sampleRate : 48000;
        m_audioChannels = channels;
        if (!startAudioOutput()) {
            return;
        }
    } else if (sampleRate != m_audioSampleRate || channels != m_audioChannels) {
        stopAudioOutput();
        m_audioSampleRate = sampleRate > 0 ? sampleRate : 48000;
        m_audioChannels = channels;
        if (!startAudioOutput()) {
            return;
        }
    }

    // The decoder hands out interleaved float32: pcm[s * channels + c]. PortAudio with
    // paFloat32 expects the same layout, so only channel mapping and volume are needed.
    const int outChannels = m_audioOutputChannels;
    const std::size_t totalSamples = static_cast<std::size_t>(frames) * static_cast<std::size_t>(outChannels);
    if (m_interleavedAudioBuf.size() < totalSamples) {
        m_interleavedAudioBuf.resize(totalSamples);
    }

    // Mute zeroes the per-frame volume so no audio is written to the PortAudio stream.
    const float vol = m_volumeMute.load(std::memory_order_relaxed) ? 0.f : m_audioVolume.load(std::memory_order_relaxed);
    for (int s = 0; s < frames; ++s) {
        const float *inSample = pcm + static_cast<std::size_t>(s) * channels;
        float *outSample = m_interleavedAudioBuf.data() + static_cast<std::size_t>(s) * outChannels;
        for (int oc = 0; oc < outChannels; ++oc) {
            // Map output channel to input channel (simple wrap for down/upmix).
            const int ic = (oc < channels) ? oc : (oc % channels);
            outSample[oc] = inSample[ic] * vol;
        }
    }

    // Report the peak of this frame for the audio level meter in the LayerView, but only
    // while the meter is enabled so the per-sample scan stays out of the hot path.
    if (audioLevelsEnabled()) {
        float maxAbs = 0.f;
        for (std::size_t i = 0; i < totalSamples; ++i) {
            const float v = m_interleavedAudioBuf[i] < 0.f ? -m_interleavedAudioBuf[i] : m_interleavedAudioBuf[i];
            if (v > maxAbs) maxAbs = v;
        }
        reportAudioLevel(maxAbs);
    }

    m_audioError = Pa_WriteStream(m_audioStream, m_interleavedAudioBuf.data(), static_cast<unsigned long>(frames));
    if (m_audioError != paNoError && m_audioError != paOutputUnderflowed) {
        sgct::Log::Error("WebRTCLayer: Pa_WriteStream failed.\n");
    }
}

bool WebRTCLayer::startAudioOutput() {
    // PortAudio is process-wide and refcounted; initialize it once for all layers.
    static std::once_flag paInitFlag;
    std::call_once(paInitFlag, [] {
        if (Pa_Initialize() != paNoError) {
            sgct::Log::Error("WebRTCLayer: PortAudio initialization failed.\n");
        }
    });

    m_audioOutputParameters.device = chosenAudioDevice();
    if (m_audioOutputParameters.device == paNoDevice) {
        return false;
    }

    // Determine the output channel count.
    if (AudioSettings::portAudioMixInputToOutput()) {
        const PaDeviceInfo *devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
        if (devInfo) {
            m_audioOutputChannels = std::min(AudioSettings::portAudioOutputChannels(), devInfo->maxOutputChannels);
        }
    } else {
        m_audioOutputChannels = m_audioChannels;
    }

    m_audioOutputParameters.channelCount = m_audioOutputChannels;
    m_audioOutputParameters.sampleFormat = paFloat32;
    const PaDeviceInfo *devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
    if (devInfo) {
        m_audioOutputParameters.suggestedLatency = devInfo->defaultLowOutputLatency;
    } else {
        m_audioOutputParameters.suggestedLatency = 0.05;
    }
    m_audioOutputParameters.hostApiSpecificStreamInfo = nullptr;

    // Open the stream without a callback - we use Pa_WriteStream instead.
    m_audioError = Pa_OpenStream(
        &m_audioStream,
        nullptr,
        &m_audioOutputParameters,
        m_audioSampleRate,
        paFramesPerBufferUnspecified,
        paClipOff,
        nullptr, // no callback
        nullptr  // no userData
    );

    if (m_audioError != paNoError) {
        // The stream is reopened lazily on every PCM frame, so a failing device would log an
        // error ~50 times per second; report it at most once every 5 seconds.
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAudioOpenErrorLog).count() >= 5) {
            m_lastAudioOpenErrorLog = now;
            sgct::Log::Error("WebRTCLayer: failed to open the PortAudio stream (error " + std::to_string(m_audioError)
                             + ").\n");
        }
        return false;
    }
    m_audioStreamOpen = true;

    m_audioError = Pa_StartStream(m_audioStream);
    if (m_audioError != paNoError) {
        sgct::Log::Error("WebRTCLayer: failed to start the PortAudio stream.\n");
        stopAudioOutput();
        return false;
    }
    m_audioStreamStarted = true;

    const PaDeviceInfo *startedDevInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
    sgct::Log::Info("WebRTCLayer: audio output started (" + std::to_string(m_audioSampleRate) + " Hz, "
                    + std::to_string(m_audioOutputChannels) + " channel(s)"
                    + (startedDevInfo ? ", device: " : "")
                    + (startedDevInfo ? QString::fromUtf8(startedDevInfo->name).toStdString() : "")
                    + ")\n");
    return true;
}

void WebRTCLayer::stopAudioOutput() {
    if (m_audioStream && m_audioStreamOpen) {
        if (m_audioStreamStarted) {
            Pa_StopStream(m_audioStream);
            m_audioStreamStarted = false;
        }
        Pa_CloseStream(m_audioStream);
        m_audioStreamOpen = false;
    }

    m_audioStream = nullptr;
}
