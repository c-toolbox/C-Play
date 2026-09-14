/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/webrtclayer.h"

#include "webrtc/videodecoder.h"
#include "webrtc/webrtcsource.h"

#include <sgct/sgct.h>
#include <sgct/opengl.h>

#include <QCoreApplication>
#include <QObject>
#include <QMetaObject>
#include <QTimer>

namespace {

constexpr std::size_t kMaxQueuedUnits = 64;      // bounded Annex-B queue (~a few seconds)
constexpr int kReconnectDelayMs = 2000;          // wait before retrying a failed session
constexpr auto kStarvedForKeyframe = std::chrono::seconds(3);

/// True when the access unit carries parameter sets or starts a new GOP, i.e. when the
/// decoder can be handed data without complaining about missing SPS/PPS. WHEP drops us
/// into the middle of a stream, so everything before the first such unit is useless.
bool canStartDecodingAt(const std::vector<std::uint8_t> &data, WebRtcVideoCodec codec) {
    for (std::size_t i = 0; i + 3 < data.size(); ++i) {
        if (data[i] != 0 || data[i + 1] != 0) {
            continue;
        }

        std::size_t header = 0;
        if (data[i + 2] == 1) {
            header = i + 3;
        } else if (data[i + 2] == 0 && data[i + 3] == 1) {
            header = i + 4;
        } else {
            continue;
        }
        if (header >= data.size()) {
            break;
        }

        if (codec == WebRtcVideoCodec::H265) {
            const int type = (data[header] >> 1) & 0x3F;
            // VPS/SPS/PPS, or any IRAP picture (BLA/IDR/CRA).
            if (type == 32 || type == 33 || type == 34 || (type >= 16 && type <= 21)) {
                return true;
            }
        } else {
            const int type = data[header] & 0x1F;
            if (type == 5 || type == 7 || type == 8) { // IDR, SPS, PPS
                return true;
            }
        }
        i = header;
    }
    return false;
}

} // namespace

WebRTCLayer::WebRTCLayer() {
    m_type = WEBRTC;
    // swscale writes RGBA top-down, OpenGL samples bottom-up.
    renderData.flipY = true;
}

WebRTCLayer::~WebRTCLayer() {
    *m_alive = false;
    stop();
}

void WebRTCLayer::cleanup() {
    // Always release the NDI output first (base class behavior).
    BaseLayer::cleanup();

    stop();

    if (renderData.texId && renderData.width > 0) {
        glDeleteTextures(1, &renderData.texId);
        renderData.texId = 0;
        renderData.width = 0;
        renderData.height = 0;
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
    startSource();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopRequested = false;
        while (!m_queue.empty()) {
            m_queue.pop_front();
        }
    }
    m_decodeThread = std::thread([this] { decodeLoop(); });
}

void WebRTCLayer::stop() {
    m_explicitlyStopped.store(true, std::memory_order_relaxed);

    if (!m_shouldRun.load(std::memory_order_relaxed) && !m_source) {
        return;
    }

    m_shouldRun.store(false, std::memory_order_relaxed);
    stopSource();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopRequested = true;
    }
    m_queueCv.notify_all();

    if (m_decodeThread.joinable()) {
        m_decodeThread.join();
    }

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

bool WebRTCLayer::existOnMasterOnly() const {
    return m_existOnMasterOnly;
}

WebRtcStreamConfig WebRTCLayer::buildConfig() const {
    WebRtcStreamConfig config;
    config.whepUrl = QUrl(QString::fromStdString(filepath()));
    // Credentials may be embedded in the URL (user:pass@host); WhepClient lifts
    // them into an Authorization header. Explicit overrides are not exposed yet.
    return config;
}

void WebRTCLayer::startSource() {
    stopSource();

    auto *source = new WebRtcSource(); // no parent; we own it and deleteLater() it
    source->setConfig(buildConfig());

    // The layer may be destroyed while the source is still pending deletion or a
    // signal is queued, so every callback checks the alive flag before touching
    // any member.
    std::weak_ptr<std::atomic<bool>> weakAlive = m_alive;

    source->setVideoCallback([this, weakAlive](const std::uint8_t *data, std::size_t size,
                                               quint32 rtpTimestamp) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        pushAnnexB(data, size, static_cast<std::int64_t>(rtpTimestamp));
    });

    // The layer is plain C++, so the source itself is the context object: the
    // connections then die with it, and delivery is queued onto the main thread when a
    // signal is emitted from a libdatachannel thread. A null context would create a
    // connection that never fires. weakAlive still guards against the layer being
    // destroyed while a signal is queued.
    QObject::connect(source, &WebRtcSource::videoCodecNegotiated, source, [this, weakAlive](WebRtcVideoCodec codec) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        m_negotiatedCodec.store(codec, std::memory_order_relaxed);
        // Units from a previous session are useless to the fresh decoder.
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            while (!m_queue.empty()) {
                m_queue.pop_front();
            }
        }
        m_queueCv.notify_all();
    });

    QObject::connect(source, &WebRtcSource::stateChanged, source, [this, weakAlive](WebRtcStreamState state) {
        auto alive = weakAlive.lock();
        if (!alive || !*alive) {
            return;
        }
        if (state == WebRtcStreamState::Failed && m_shouldRun.load(std::memory_order_relaxed)) {
            scheduleReconnect();
        }
    });

    QObject::connect(source, &WebRtcSource::errorOccurred, source, [weakAlive](const QString &message) {
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

    // Detach the media callback first so no new units arrive while tearing down.
    // In-flight callbacks are guarded by the alive flag and the queue mutex.
    m_source->setVideoCallback(nullptr);
    m_source->stop(); // WHEP DELETE + close peer connection
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

void WebRTCLayer::pushAnnexB(const std::uint8_t *data, std::size_t size, std::int64_t pts) {
    if (!data || size == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_stopRequested) {
        return;
    }

    WebRtcAnnexBUnit unit;
    unit.data.assign(data, data + size); // copy: the RTP buffer dies with the callback
    unit.pts = pts;
    m_queue.push_back(std::move(unit));

    while (m_queue.size() > kMaxQueuedUnits) {
        m_queue.pop_front(); // live stream: drop oldest rather than grow latency
    }
    m_queueCv.notify_one();
}

void WebRTCLayer::decodeLoop() {
    auto loggedFirstFrame = std::make_shared<bool>(false);
    m_decoder.setOnFrame([this, loggedFirstFrame](const std::uint8_t *rgba, int width, int height) {
        if (!*loggedFirstFrame) {
            *loggedFirstFrame = true;
            sgct::Log::Info("WebRTCLayer: first decoded frame " + std::to_string(width) + "x"
                            + std::to_string(height)
                            + (m_decoder.isHardwareDecoder() ? " (NVDEC)\n" : " (software)\n"));
        }
        const std::size_t size = static_cast<std::size_t>(width) * height * 4u;
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (m_latestFrame.size() != size) {
            m_latestFrame.resize(size);
        }
        std::copy(rgba, rgba + size, m_latestFrame.begin());
        m_latestWidth = width;
        m_latestHeight = height;
        ++m_frameSeq;
    });

    WebRtcVideoCodec openCodec = WebRtcVideoCodec::Unknown;
    bool waitingForKeyframe = true;
    auto lastOpenAttempt = std::chrono::steady_clock::now();
    auto lastKeyframeRequest = lastOpenAttempt;

    while (true) {
        WebRtcAnnexBUnit unit;
        bool haveUnit = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait_for(lock, std::chrono::milliseconds(200), [this] {
                const auto codec = m_negotiatedCodec.load(std::memory_order_relaxed);
                return m_stopRequested || !m_queue.empty() || codec != WebRtcVideoCodec::Unknown;
            });

            if (m_stopRequested && m_queue.empty()) {
                break;
            }

            if (!m_queue.empty()) {
                unit = std::move(m_queue.front());
                m_queue.pop_front();
                haveUnit = true;
            }
        }

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
        // sender never sent an IDR) -> ask for a keyframe. The source is owned by
        // the main thread, so hop there before touching it.
        {
            const auto now = std::chrono::steady_clock::now();
            bool hasFrame = false;
            {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                hasFrame = m_latestWidth > 0 && m_latestHeight > 0;
            }
            if (!hasFrame && now - lastKeyframeRequest > kStarvedForKeyframe) {
                lastKeyframeRequest = now;
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
        }

        if (!haveUnit || !m_decoder.isOpen()) {
            continue;
        }

        if (waitingForKeyframe) {
            if (!canStartDecodingAt(unit.data, openCodec)) {
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

void WebRTCLayer::updateFrame() {
    std::lock_guard<std::mutex> lock(m_updateFrameMutex);

    // Copy the newest decoded frame out under the frame mutex, then upload from
    // our own staging buffer so the GL call never holds the decoder's lock.
    std::vector<std::uint8_t> staging;
    int width = 0;
    int height = 0;
    std::uint64_t seq = 0;
    {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        if (m_latestWidth <= 0 || m_latestHeight <= 0) {
            return;
        }
        if (m_frameSeq == m_lastUploadedSeq) {
            return; // nothing new to upload
        }
        staging.assign(m_latestFrame.begin(), m_latestFrame.end());
        width = m_latestWidth;
        height = m_latestHeight;
        seq = m_frameSeq;
    }

    if (width != renderData.width || height != renderData.height) {
        if (renderData.texId && renderData.width > 0) {
            glDeleteTextures(1, &renderData.texId);
        }
        glGenTextures(1, &renderData.texId);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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

    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, staging.data());
    m_lastUploadedSeq = seq;
}
