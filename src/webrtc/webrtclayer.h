/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <layers/baselayer.h>
#include "webrtc/audiodecoder.h"
#include "webrtc/videodecoder.h"
#include "webrtc/webrtcplayoutclock.h"
#include "webrtc/webrtctypes.h"
#include "webrtc/webrtcmediasource.h"

#include <portaudio.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class VideoDecoder;

/// One Annex-B access unit handed from the libdatachannel media thread to the
/// decode worker. The data is copied because the RTP callback buffer is only
/// valid for the duration of the call.
struct WebRtcAnnexBUnit {
    std::vector<std::uint8_t> data;
    std::int64_t pts = 0;       // unwrapped 90 kHz timestamp
    bool keyframe = false;      // random access point
    bool discontinuity = false; // units were dropped before this one: flush the decoder
};

/// One depacketized Opus payload handed from the libdatachannel media thread to the
/// audio decode worker. The data is copied because the source buffer is only valid for
/// the duration of the callback.
struct WebRtcAudioUnit {
    std::vector<std::uint8_t> data;
};

/// One decoded picture waiting to be shown. The planes are stored tightly packed, one
/// after the other (see VideoPixelLayout).
struct WebRtcFrameSlot {
    std::vector<std::uint8_t> data;
    VideoPixelLayout layout = VideoPixelLayout::Rgba;
    int width = 0;
    int height = 0;
    std::int64_t pts = 0;
    VideoColorMatrix matrix = VideoColorMatrix::Bt709;
    bool fullRange = false;
    std::uint64_t seq = 0;  // decode order, 0 = empty
    bool pinned = false;    // being uploaded by the render thread
    bool writing = false;   // being filled by the decode worker
};

/// Pulls a WHEP (WebRTC) stream, decodes it with FFmpeg (NVDEC when available)
/// and renders it like the NDI/OMT layers.
///
/// The WHEP URL is stored in the layer's filepath property. Threading:
///  - start()/stop() run on the main thread; they own the WebRtcMediaSource QObject.
///  - a dedicated worker thread drains a bounded Annex-B queue and decodes video into a
///    small ring of decoded frames.
///  - a second worker thread drains a bounded Opus payload queue, decodes audio and
///    owns the PortAudio output stream (like MpvLayer/ImageLayer decode off-main).
///  - updateFrame() runs on the render thread, picks the frame to show from the ring,
///    uploads its YUV planes and converts them to RGBA on the GPU.
///
/// Cluster sync (master relay on): the master maps the stream's timestamps onto its own
/// clock (WebRtcPlayoutClock) and sends every SGCT frame the timestamp that should be on
/// screen, syncDelayMs behind the live edge. The master and every node then show the
/// newest decoded frame not later than that target, so all screens change frame together.
class WebRTCLayer : public BaseLayer {
public:
    WebRTCLayer();
    ~WebRTCLayer() override;

    void cleanup() override;
    void initialize() override;
    void update(bool updateRendering = true) override;
    void updateFrame() override;
    bool ready() const override;
    bool hasTexture() const override;
    bool renderingIsOn() const override;

    void start() override;
    void stop() override;

    /// Reported as "paused" while no WHEP session is running, so the generic layer
    /// start/stop machinery (media visibility, the layer list) sees the real state.
    bool pause() override;
    void setPause(bool value) override;

    // The WHEP URL lives in the layer's filepath property. Setting a new URL
    // restarts the connection when the layer is running.
    std::string whepUrl() const { return filepath(); }
    void setWhepUrl(std::string url);

    /// HTTP Basic authentication for the WHEP endpoint, sent as an Authorization header on
    /// the WHEP request (MediaMTX authenticates WebRTC this way). Stored on the layer so it
    /// persists with the presentation and syncs to nodes. An empty username disables auth;
    /// when set it takes precedence over any credentials embedded in the URL.
    std::string authUsername() const { return m_authUsername; }
    void setAuthUsername(std::string username);
    std::string authPassword() const { return m_authPassword; }
    void setAuthPassword(std::string password);

    /// Master relay for this layer (default on): the master pulls the WHEP stream once and
    /// relays it to all nodes through its WebRtcHub, so only one upstream connection is used.
    /// When off, every machine that has the layer pulls its own copy of the stream directly.
    bool masterRelayEnabled() const { return m_masterRelayEnabled; }
    void setMasterRelayEnabled(bool enabled);

    /// Playout delay of the synchronised presentation (master relay on), in milliseconds
    /// behind the master's live edge. Larger values absorb more network/decode jitter on the
    /// nodes; smaller ones reduce the latency. Ignored when the relay is off.
    int syncDelayMs() const { return m_syncDelayMs.load(std::memory_order_relaxed); }
    void setSyncDelayMs(int delayMs);

    /// New WebRTC layers default to master-only; uncheck it in the UI to let every
    /// node pull its own copy of the stream.
    bool existOnMasterOnly() const override;

    // Audio (WHEP Opus -> PortAudio), mirroring the NDI/OMT layer behavior. The
    // stream's audio is optional: hasAudio()/isAudioEnabled() drive the UI, and the
    // output only starts once decoded PCM actually arrives.
    bool hasAudio() const override;
    // The audio level is reported from pushDecodedPcm(), so a meter in the LayerView can
    // show live levels while the image renders.
    bool hasAudioLevels() const override { return true; }
    bool isAudioEnabled() const override;
    void enableAudio(bool enabled = true) override;
    void updateAudioOutput() override;
    void setVolume(int v, bool storeLevel = true) override;
    void setVolumeMute(bool v) override;

 public:
    // Persist the WHEP HTTP auth credentials with the layer (cplayfile + node sync).
    void encodeTypeCore(std::vector<std::byte> &data) override;
    void decodeTypeCore(const std::vector<std::byte> &data, unsigned int &pos) override;

    // Per-frame presentation target (master -> nodes), see the class comment.
    void encodeTypeAlways(std::vector<std::byte> &data) override;
    void decodeTypeAlways(const std::vector<std::byte> &data, unsigned int &pos) override;

private:
    enum class SyncMode {
        None,        // show the newest decoded frame (no relay, or master-only layer)
        MasterClock, // master with relay: target from m_playoutClock
        NodeTarget,  // node with relay: target received from the master
    };

    // Main thread only.
    void startSource();
    void stopSource();
    void scheduleReconnect();
    /// Posts a keyframe request to the main thread, which owns m_source. Any thread.
    void requestKeyframeAsync();

    /// Opens the session from whichever thread drives update(). BaseLayer defers its
    /// own start() until the layer is ready(), which can never happen here because
    /// ready() needs a decoded frame, so the layer has to start itself.
    void ensureStarted();

    // Decode worker thread only (queue access is mutex protected).
    void decodeLoop();
    void pushAnnexB(const WebRtcVideoFrame &frame);
    /// Stores one decoded picture in the frame ring (decode worker thread).
    void storeDecodedFrame(const DecodedVideoFrame &frame);
    /// Index of a ring slot the decoder may overwrite, or -1. Caller holds m_frameMutex.
    int reusableSlot(bool hasTarget, std::int64_t target);
    void resetFrameRing();

    /// The timestamp that should be on screen now; false when frames are simply shown as
    /// soon as they are decoded (no sync, or no target yet). Any thread.
    bool presentationTarget(std::int64_t &target);

    // Render thread only.
    bool uploadRgba(const WebRtcFrameSlot &slot);
    bool uploadYuv(const WebRtcFrameSlot &slot);
    bool ensureYuvProgram();
    void ensureOutputTexture(int width, int height);
    void releaseGlResources();

    // Audio decode worker thread only (queue access is mutex protected). The media
    // callback just copies payloads into m_audioQueue.
    void audioDecodeLoop();
    void pushAudioPayload(const std::uint8_t *data, std::size_t size);
    void maybeReopenAudioOutput();

    WebRtcStreamConfig buildConfig() const;

    // Audio output (audio decode worker thread only). Decoded PCM is pushed straight
    // into the PortAudio stream from that thread.
    void pushDecodedPcm(const float *pcm, int sampleRate, int channels, int frames);
    bool startAudioOutput();
    void stopAudioOutput();

    WebRtcMediaSource *m_source = nullptr; // main thread; deleted via deleteLater()
    std::atomic<bool> m_shouldRun { false };
    std::atomic<bool> m_startPending { false };    // a start() is queued on the main thread
    std::atomic<bool> m_explicitlyStopped { false }; // stop() was asked for; do not self-start
    float m_lastSeenAlpha = 0.f;       // ensureStarted() thread only
    bool m_reconnectScheduled = false; // main thread

    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
    // HTTP Basic authentication for the WHEP endpoint, sent as an Authorization header on the
    // WHEP request. Persisted with the layer and synced to nodes; empty username disables it.
    std::string m_authUsername;
    std::string m_authPassword;

    // Master relay (default on): the master pulls once and relays to the nodes via WebRtcHub;
    // when off, each node that has this layer pulls its own WHEP copy directly. Synced like
    // the auth fields so master and nodes always agree on the transport.
    bool m_masterRelayEnabled = true;

    // Synchronised presentation (see the class comment).
    std::atomic<int> m_syncDelayMs { kWebRtcDefaultSyncDelayMs };
    std::atomic<SyncMode> m_syncMode { SyncMode::None };
    WebRtcPlayoutClock m_playoutClock;                 // master only
    std::atomic<bool> m_nodeTargetValid { false };     // node: from decodeTypeAlways(); master: last sent
    std::atomic<std::int64_t> m_nodeTargetPts { 0 };
    std::atomic<std::int64_t> m_sentTargetTicks { 0 }; // master: steady_clock ticks of the last send

    VideoDecoder m_decoder; // decode worker thread only
    std::atomic<WebRtcVideoCodec> m_negotiatedCodec { WebRtcVideoCodec::Unknown };

    // Audio state shared between the main thread (UI) and the audio decode worker.
    std::atomic<bool> m_isAudioEnabled { false };
    std::atomic<float> m_audioVolume { 1.f };
    std::atomic<bool> m_volumeMute { false };
    std::atomic<bool> m_audioDecodeDisabled { false }; // set when the Opus decoder cannot be opened

    // PortAudio output stream (audio decode worker thread only). Opened lazily on the
    // first decoded frame and torn down by stop() or enableAudio(false) via that thread.
    PaStream *m_audioStream = nullptr;
    bool m_audioStreamOpen = false;
    bool m_audioStreamStarted = false;
    int m_audioSampleRate = 48000;
    int m_audioChannels = 2;       // channels of the decoded stream
    int m_audioOutputChannels = 2; // channels actually opened (after mix-to-output)
    PaError m_audioError = paNoError;
    PaStreamParameters m_audioOutputParameters{};
    std::vector<float> m_interleavedAudioBuf;

    AudioDecoder m_audioDecoder; // audio decode worker thread only

    std::atomic<bool> m_audioReopenRequested { false }; // updateAudioOutput() -> worker

    std::chrono::steady_clock::time_point m_lastAudioOpenErrorLog{}; // throttles repeated open-failure logs

    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::deque<WebRtcAnnexBUnit> m_queue; // bounded; on overflow it restarts at a keyframe
    bool m_queueNeedsKeyframe = true;     // drop units until the next random access point
    bool m_stopRequested = false;

    std::mutex m_audioQueueMutex;
    std::condition_variable m_audioQueueCv;
    std::deque<WebRtcAudioUnit> m_audioQueue; // bounded, drop oldest when full
    bool m_audioStopRequested = false;        // guarded by m_audioQueueMutex

    // Decoded frame ring, shared by the decode worker (writer) and the render thread.
    static constexpr std::size_t kFrameSlots = 6;
    mutable std::mutex m_frameMutex;
    std::condition_variable m_slotFreedCv;
    std::array<WebRtcFrameSlot, kFrameSlots> m_slots;
    std::uint64_t m_nextFrameSeq = 1;
    std::uint64_t m_displayedSeq = 0;  // newest slot handed to the renderer
    std::int64_t m_newestPts = 0;      // of the newest stored frame (timeline reset check)
    bool m_hasNewestPts = false;
    int m_latestWidth = 0;
    int m_latestHeight = 0;
    std::uint64_t m_unshownFrames = 0; // decoded but never displayed (diagnostics)
    std::atomic<bool> m_decodeStop { false }; // wakes a decoder waiting for a free slot

    // GPU YUV -> RGBA conversion (render thread). Textures and the program are shared
    // between the (shared) GL contexts; framebuffers and vertex arrays are not, so those
    // are created per conversion.
    unsigned int m_yuvProgram = 0;
    bool m_yuvProgramFailed = false;
    int m_yuvMatrixLocation = -1;
    int m_yuvOffsetLocation = -1;
    int m_yuvNv12Location = -1;
    std::array<unsigned int, 3> m_planeTextures { 0, 0, 0 };
    std::array<int, 3> m_planeWidths { 0, 0, 0 };
    std::array<int, 3> m_planeHeights { 0, 0, 0 };
    VideoPixelLayout m_planeLayout = VideoPixelLayout::Rgba;
    std::atomic<bool> m_forceRgbaOutput { false }; // set when the YUV shader is unusable

    std::thread m_decodeThread;
    std::thread m_audioDecodeThread;
};
