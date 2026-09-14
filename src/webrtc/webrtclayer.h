/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <layers/baselayer.h>
#include "webrtc/videodecoder.h"
#include "webrtc/webrtctypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class WebRtcSource;
class VideoDecoder;

/// One Annex-B access unit handed from the libdatachannel media thread to the
/// decode worker. The data is copied because the RTP callback buffer is only
/// valid for the duration of the call.
struct WebRtcAnnexBUnit {
    std::vector<std::uint8_t> data;
    std::int64_t pts = 0;
};

/// Pulls a WHEP (WebRTC) stream, decodes it with FFmpeg (NVDEC when available)
/// and renders the newest frame like the NDI/OMT layers.
///
/// The WHEP URL is stored in the layer's filepath property. Threading:
///  - start()/stop() run on the main thread; they own the WebRtcSource QObject.
///  - a dedicated worker thread drains a bounded Annex-B queue and decodes.
///  - updateFrame() runs on the render thread and uploads the newest RGBA frame.
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

    /// New WebRTC layers default to master-only; uncheck it in the UI to let every
    /// node pull its own copy of the stream.
    bool existOnMasterOnly() const override;

private:
    // Main thread only.
    void startSource();
    void stopSource();
    void scheduleReconnect();

    /// Opens the session from whichever thread drives update(). BaseLayer defers its
    /// own start() until the layer is ready(), which can never happen here because
    /// ready() needs a decoded frame, so the layer has to start itself.
    void ensureStarted();

    // Decode worker thread only (queue access is mutex protected).
    void decodeLoop();
    void pushAnnexB(const std::uint8_t *data, std::size_t size, std::int64_t pts);

    WebRtcStreamConfig buildConfig() const;

    WebRtcSource *m_source = nullptr; // main thread; deleted via deleteLater()
    std::atomic<bool> m_shouldRun { false };
    std::atomic<bool> m_startPending { false };    // a start() is queued on the main thread
    std::atomic<bool> m_explicitlyStopped { false }; // stop() was asked for; do not self-start
    float m_lastSeenAlpha = 0.f;       // ensureStarted() thread only
    bool m_reconnectScheduled = false; // main thread

    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);

    VideoDecoder m_decoder; // decode worker thread only
    std::atomic<WebRtcVideoCodec> m_negotiatedCodec { WebRtcVideoCodec::Unknown };

    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::deque<WebRtcAnnexBUnit> m_queue; // bounded, drop oldest when full
    bool m_stopRequested = false;

    mutable std::mutex m_frameMutex;
    std::vector<std::uint8_t> m_latestFrame; // RGBA8, newest decoded frame
    int m_latestWidth = 0;
    int m_latestHeight = 0;
    std::uint64_t m_frameSeq = 0;

    std::uint64_t m_lastUploadedSeq = 0; // render thread only

    std::thread m_decodeThread;
};
