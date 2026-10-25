/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class QOpenGLFramebufferObject;
class QProcess;
class QThread;
class QTimer;

namespace rtc {
class PeerConnection;
class Track;
} // namespace rtc

/// Picture previews of the movie view (web/movie_mode), sent to the browser over WebRTC as
/// H.264 video. Never any audio: the peers only get one send-only video track.
///
/// Two feeds:
///  - Live: what the master plays. The render thread downscales the mpv frame and reads it back
///    (captureLiveFrame); the worker thread feeds those frames at a steady rate, the last one
///    repeated while paused, to an ffmpeg subprocess that encodes them.
///  - Prepare: the cued item at the cued position. An ffmpeg subprocess opens the file itself,
///    seeks and encodes one frame (or plays on from there in real time), so jumping around in
///    another file never touches the master's player. The still frame is resent every half
///    second so the browser keeps showing it and late peers get it.
///
/// ffmpeg (next to C-Play.exe, otherwise on the PATH) writes H.264 Annex-B with access unit
/// delimiters to its stdout; each access unit goes to every peer of the feed through
/// libdatachannel's H.264 packetizer. Encoders only run while a peer of their feed is connected.
///
/// Signaling is one HTTP request (see HttpServerThread::setupMovieRoutes): the browser posts an
/// offer with its ICE candidates gathered and gets the answer with ours (no trickle, LAN).
///
/// Threading: answer()/close()/prepare()/status() may be called from any thread (the HTTP server's),
/// captureLiveFrame()/cleanupGL() only on the render thread with the mpv context current. The
/// encoders and timers live on the streamer's own worker thread; libdatachannel calls back on
/// its threads. m_mutex guards the peers.
class MoviePreviewStreamer : public QObject {
    Q_OBJECT

public:
    enum class Feed { Live, Prepare };

    /// Creates the streamer; called once on the GUI thread (by MovieController).
    static void create();
    /// Null until create() ran.
    static MoviePreviewStreamer *instance();

    struct Answer {
        std::string sessionId;
        std::string sdp;
        std::string error; ///< set instead of the two above when it failed
    };

    /// Answers a browser offer (receive-only H.264 video, ICE gathering complete) for \p feed.
    /// Blocks for the local ICE gathering, a few milliseconds on a LAN (at most 3 s).
    Answer answer(Feed feed, const std::string &offerSdp);
    void close(const std::string &sessionId);

    /// Shows \p file at \p position (seconds) in the prepare feed, playing on in real time when
    /// \p play is set. Rapid calls (scrubbing) are coalesced; an empty file clears the picture.
    void prepare(const QString &file, double position, bool play);

    /// Peers per feed, the prepared file and position, and the last encoder errors.
    QJsonObject status() const;

    /// True while a live peer is connected, so the render thread has to capture.
    bool wantsLiveFrames() const { return m_wantsLive.load(std::memory_order_relaxed); }
    /// Render thread, mpv context current: downscales and reads back \p source at most at the
    /// live frame rate. Cheap when no live peer is connected.
    void captureLiveFrame(QOpenGLFramebufferObject *source);
    /// Render thread, context current: releases the capture FBO and PBOs.
    void cleanupGL();

    /// Closes every peer and stops the encoders and the worker thread. Called on shutdown.
    void shutdown();

private:
    MoviePreviewStreamer();
    ~MoviePreviewStreamer() override = default;

    using Clock = std::chrono::steady_clock;

    struct Peer {
        std::string id;
        Feed feed = Feed::Live;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::Track> track;
        bool open = false;              // guarded by m_mutex
        bool waitingForKeyframe = true; // guarded by m_mutex
    };

    /// One ffmpeg encoder subprocess and its stdout parsing. Worker thread only.
    struct Encoder {
        QProcess *process = nullptr;
        QByteArray pending; // stdout not yet split into access units
        QByteArray errors;  // the end of stderr, for the status
        int frames = 0;     // access units produced by the running process
    };

    struct PrepareRequest {
        QString file;
        double position = 0;
        bool play = false;
    };

    // Worker thread.
    void updateEncoders();
    void startLiveEncoder(int width, int height);
    void writeLiveFrame();
    void runPrepare();
    void repeatPrepareFrame();
    void startEncoder(Encoder &encoder, Feed feed, const QStringList &arguments);
    void stopEncoder(Encoder &encoder);
    void readEncoder(Encoder &encoder, Feed feed, bool finished);
    void encoderFinished(Encoder &encoder, Feed feed, QProcess *process);
    void publish(Feed feed, const QByteArray &accessUnit, bool keyframe);
    void setError(Feed feed, const QString &error);

    void removePeer(const std::string &id);
    int peerCount(Feed feed) const; // caller holds m_mutex

    static std::atomic<MoviePreviewStreamer *> s_instance;

    QThread *m_thread = nullptr;
    const Clock::time_point m_epoch = Clock::now();

    mutable std::mutex m_mutex;
    std::vector<std::shared_ptr<Peer>> m_peers;
    std::atomic_bool m_wantsLive = false;
    std::atomic_bool m_shutdown = false;

    // Live feed. m_frameMutex guards the frame handed from the render thread to the worker.
    std::mutex m_frameMutex;
    std::vector<std::uint8_t> m_liveFrame; // RGBA, top row first
    int m_liveFrameWidth = 0;
    int m_liveFrameHeight = 0;
    Encoder m_liveEncoder;     // worker thread
    int m_liveEncoderWidth = 0; // worker thread
    int m_liveEncoderHeight = 0;
    std::vector<std::uint8_t> m_liveWriteBuffer; // worker thread
    Clock::time_point m_liveRetryAt;             // worker thread, after ffmpeg failed
    QTimer *m_liveTimer = nullptr;

    // m_statusMutex guards the prepare request and the errors, read by status().
    mutable std::mutex m_statusMutex;
    PrepareRequest m_prepareRequest;
    bool m_prepareRequested = false;
    QString m_liveError;
    QString m_prepareError;

    // Prepare feed, worker thread.
    Encoder m_prepareEncoder;
    PrepareRequest m_prepareRunning;
    Clock::time_point m_prepareStarted;
    bool m_prepareQueued = false;  // a newer request waits for the running still to finish
    QByteArray m_prepareKeyframe;  // the still, resent to keep it on screen
    QTimer *m_prepareThrottle = nullptr;
    QTimer *m_prepareRepeat = nullptr;

    // Render thread capture.
    QOpenGLFramebufferObject *m_captureFbo = nullptr;
    unsigned int m_pbo[2] = {0, 0};
    int m_pboIndex = 0;
    bool m_pboFilled = false;
    int m_pboWidth = 0;
    int m_pboHeight = 0;
    Clock::time_point m_lastCapture;
};
