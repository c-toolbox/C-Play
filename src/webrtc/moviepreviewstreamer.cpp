/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/moviepreviewstreamer.h"

#include "webrtc/webrtcmediasource.h"
#include "webrtc/webrtctypes.h"

#include <rtc/rtc.hpp>
#include <sgct/log.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <cstring>
#include <future>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

// The preview pictures fit in this box, never upscaled.
constexpr int kMaxWidth = 1280;
constexpr int kMaxHeight = 720;
constexpr int kLiveFps = 25;
constexpr int kMaxPeersPerFeed = 8;
// While scrubbing the prepare feed decodes at most this often.
constexpr int kPrepareThrottleMs = 120;
// A still that is still decoding after this long is replaced by a newer request.
constexpr int kPrepareStillPatienceMs = 1500;
constexpr int kPrepareRepeatMs = 500;
constexpr const char *kCname = "cplay-preview";

QString ffmpegProgram() {
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ffmpeg.exe"));
    return QFileInfo::exists(bundled) ? bundled : QStringLiteral("ffmpeg");
}

// H.264 the browser decodes everywhere (constrained baseline, no B-frames), with parameter sets
// on every keyframe for late peers and access unit delimiters to split the stream on.
QStringList encodeArguments() {
    return {QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-preset"), QStringLiteral("ultrafast"),
        QStringLiteral("-tune"), QStringLiteral("zerolatency"), QStringLiteral("-profile:v"), QStringLiteral("baseline"),
        QStringLiteral("-crf"), QStringLiteral("23"), QStringLiteral("-maxrate"), QStringLiteral("4M"),
        QStringLiteral("-bufsize"), QStringLiteral("2M"), QStringLiteral("-g"), QString::number(kLiveFps),
        QStringLiteral("-bf"), QStringLiteral("0"), QStringLiteral("-x264-params"), QStringLiteral("repeat-headers=1:aud=1"),
        QStringLiteral("-f"), QStringLiteral("h264"), QStringLiteral("pipe:1")};
}

QString scaleFilter() {
    return QStringLiteral("scale=w=%1:h=%2:force_original_aspect_ratio=decrease:force_divisible_by=2,format=yuv420p")
        .arg(kMaxWidth)
        .arg(kMaxHeight);
}

// Fits width x height in the preview box, keeping the aspect ratio, with even sizes.
QSize previewSize(int width, int height) {
    const double scale = std::min({1.0, double(kMaxWidth) / width, double(kMaxHeight) / height});
    return {std::max(2, int(width * scale) & ~1), std::max(2, int(height * scale) & ~1)};
}

// The H.264 offer of the browser that the encoder output fits best: packetization mode 1 (FU-A,
// needed for frames larger than a packet) and the constrained baseline profile.
int chooseH264PayloadType(const rtc::Description::Media &media, std::string &fmtp) {
    int best = -1;
    int bestScore = -1;
    for (const int payloadType : media.payloadTypes()) {
        const auto *map = media.rtpMap(payloadType);
        if (!map || QString::fromStdString(map->format).compare(QStringLiteral("H264"), Qt::CaseInsensitive) != 0)
            continue;
        std::string parameters;
        for (const auto &part : map->fmtps)
            parameters += (parameters.empty() ? "" : ";") + part;
        const QString lower = QString::fromStdString(parameters).toLower();
        int score = 0;
        if (lower.contains(QStringLiteral("packetization-mode=1")))
            score += 4;
        if (lower.contains(QStringLiteral("profile-level-id=42e0")))
            score += 2;
        else if (lower.contains(QStringLiteral("profile-level-id=4200")))
            score += 1;
        if (score > bestScore) {
            best = payloadType;
            bestScore = score;
            fmtp = parameters;
        }
    }
    return best;
}

const char *feedName(MoviePreviewStreamer::Feed feed) {
    return feed == MoviePreviewStreamer::Feed::Live ? "live" : "prepare";
}

} // namespace

std::atomic<MoviePreviewStreamer *> MoviePreviewStreamer::s_instance = nullptr;

void MoviePreviewStreamer::create() {
    if (!s_instance.load())
        s_instance = new MoviePreviewStreamer(); // lives until the process exits, see shutdown()
}

MoviePreviewStreamer *MoviePreviewStreamer::instance() {
    return s_instance.load();
}

MoviePreviewStreamer::MoviePreviewStreamer() {
    m_liveTimer = new QTimer(this);
    m_liveTimer->setInterval(1000 / kLiveFps);
    m_liveTimer->setTimerType(Qt::PreciseTimer);
    connect(m_liveTimer, &QTimer::timeout, this, &MoviePreviewStreamer::writeLiveFrame);

    m_prepareThrottle = new QTimer(this);
    m_prepareThrottle->setSingleShot(true);
    m_prepareThrottle->setInterval(kPrepareThrottleMs);
    connect(m_prepareThrottle, &QTimer::timeout, this, &MoviePreviewStreamer::runPrepare);

    m_prepareRepeat = new QTimer(this);
    m_prepareRepeat->setInterval(kPrepareRepeatMs);
    connect(m_prepareRepeat, &QTimer::timeout, this, &MoviePreviewStreamer::repeatPrepareFrame);

    m_thread = new QThread();
    m_thread->setObjectName(QStringLiteral("MoviePreview"));
    moveToThread(m_thread);
    m_thread->start();
}

void MoviePreviewStreamer::shutdown() {
    if (m_shutdown.exchange(true))
        return;
    m_wantsLive = false;

    std::vector<std::shared_ptr<Peer>> peers;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        peers.swap(m_peers);
    }
    for (const auto &peer : peers) {
        try {
            peer->pc->close();
        } catch (const std::exception &) {
        }
    }

    if (m_thread->isRunning()) {
        QMetaObject::invokeMethod(this, [this]() {
            m_liveTimer->stop();
            m_prepareThrottle->stop();
            m_prepareRepeat->stop();
            stopEncoder(m_liveEncoder);
            stopEncoder(m_prepareEncoder);
        }, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
}

MoviePreviewStreamer::Answer MoviePreviewStreamer::answer(Feed feed, const std::string &offerSdp) {
    Answer result;
    if (m_shutdown) {
        result.error = "C-Play is shutting down";
        return result;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (peerCount(feed) >= kMaxPeersPerFeed) {
            result.error = "Too many previews are open";
            return result;
        }
    }
    webRtcEnsureLibDataChannelLogger();

    auto peer = std::make_shared<Peer>();
    peer->id = QUuid::createUuid().toString(QUuid::Id128).toStdString();
    peer->feed = feed;
    const std::string id = peer->id;

    try {
        rtc::Description offer(offerSdp, rtc::Description::Type::Offer);
        std::string mid;
        std::string fmtp;
        int payloadType = -1;
        for (int i = 0; i < offer.mediaCount() && payloadType < 0; ++i) {
            auto entry = offer.media(i);
            auto *media = std::get_if<rtc::Description::Media *>(&entry);
            if (!media || (*media)->type() != "video")
                continue;
            payloadType = chooseH264PayloadType(**media, fmtp);
            mid = (*media)->mid();
        }
        if (payloadType < 0) {
            result.error = "The browser offers no H.264 video";
            return result;
        }

        rtc::Configuration config;
        config.portRangeBegin = kWebRtcRelayPortRangeBegin;
        config.portRangeEnd = kWebRtcRelayPortRangeEnd;
        config.mtu = kWebRtcRelayMtu;
        // Answered explicitly below, after the send track is in place.
        config.disableAutoNegotiation = true;
        peer->pc = std::make_shared<rtc::PeerConnection>(config);

        auto gathered = std::make_shared<std::promise<void>>();
        auto gatheredOnce = std::make_shared<std::atomic_bool>(false);
        peer->pc->onGatheringStateChange([gathered, gatheredOnce](rtc::PeerConnection::GatheringState state) {
            if (state == rtc::PeerConnection::GatheringState::Complete && !gatheredOnce->exchange(true))
                gathered->set_value();
        });
        // Callbacks capture the id, not the peer: the peer owns the connection.
        peer->pc->onStateChange([this, id](rtc::PeerConnection::State state) {
            if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed)
                removePeer(id);
        });

        peer->pc->setRemoteDescription(offer);

        const auto ssrc = QRandomGenerator::global()->generate();
        rtc::Description::Video video(mid, rtc::Description::Direction::SendOnly);
        video.addH264Codec(payloadType, fmtp.empty() ? std::nullopt : std::optional<std::string>(fmtp));
        video.addSSRC(ssrc, kCname, kCname, "video");
        peer->track = peer->pc->addTrack(video);

        auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
            ssrc, kCname, static_cast<std::uint8_t>(payloadType), rtc::H264RtpPacketizer::ClockRate);
        auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, rtpConfig);
        packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(rtpConfig));
        // Retransmits what a LAN drops; the browser asks with NACKs.
        packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());
        // The live encoder sends a keyframe every second anyway; the still can be resent at once.
        packetizer->addToChain(std::make_shared<rtc::PliHandler>([this, feed]() {
            if (feed == Feed::Prepare)
                QMetaObject::invokeMethod(this, &MoviePreviewStreamer::repeatPrepareFrame, Qt::QueuedConnection);
        }));
        peer->track->setMediaHandler(packetizer);
        peer->track->onOpen([this, id, feed]() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const auto &candidate : m_peers) {
                    if (candidate->id == id) {
                        candidate->open = true;
                        candidate->waitingForKeyframe = true;
                    }
                }
            }
            if (feed == Feed::Prepare)
                QMetaObject::invokeMethod(this, &MoviePreviewStreamer::repeatPrepareFrame, Qt::QueuedConnection);
        });

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_peers.push_back(peer);
        }
        peer->pc->setLocalDescription(rtc::Description::Type::Answer);

        // The answer carries our candidates, so the browser needs no trickle ICE.
        if (gathered->get_future().wait_for(std::chrono::seconds(3)) != std::future_status::ready)
            sgct::Log::Warning("MoviePreviewStreamer: ICE gathering did not complete, answering with the candidates so far\n");
        const auto local = peer->pc->localDescription();
        if (!local)
            throw std::runtime_error("no local description");
        result.sdp = std::string(*local);
        result.sessionId = id;
    } catch (const std::exception &error) {
        sgct::Log::Warning(std::string("MoviePreviewStreamer: could not answer a ") + feedName(feed) + " preview: " + error.what() + "\n");
        result = {};
        result.error = std::string("Could not set up the preview: ") + error.what();
        removePeer(id);
        if (peer->pc) {
            QMetaObject::invokeMethod(this, [pc = peer->pc]() { pc->close(); }, Qt::QueuedConnection);
        }
        return result;
    }

    QMetaObject::invokeMethod(this, &MoviePreviewStreamer::updateEncoders, Qt::QueuedConnection);
    return result;
}

void MoviePreviewStreamer::close(const std::string &sessionId) {
    removePeer(sessionId);
}

void MoviePreviewStreamer::removePeer(const std::string &id) {
    std::shared_ptr<Peer> removed;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = std::find_if(m_peers.begin(), m_peers.end(), [&](const auto &peer) { return peer->id == id; });
        if (it == m_peers.end())
            return;
        removed = *it;
        m_peers.erase(it);
    }
    // Often called from a libdatachannel callback of this very connection, which must not close
    // or destroy it: the worker thread does both.
    QMetaObject::invokeMethod(this, [this, removed]() {
        try {
            removed->pc->close();
        } catch (const std::exception &) {
        }
        updateEncoders();
    }, Qt::QueuedConnection);
}

int MoviePreviewStreamer::peerCount(Feed feed) const {
    return static_cast<int>(std::count_if(m_peers.begin(), m_peers.end(), [feed](const auto &peer) { return peer->feed == feed; }));
}

void MoviePreviewStreamer::prepare(const QString &file, double position, bool play) {
    if (m_shutdown)
        return;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_prepareRequest = {file, std::max(0.0, position), play};
        m_prepareRequested = true;
    }
    // Throttled rather than debounced, so dragging over the timeline updates the picture.
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_prepareThrottle->isActive())
            m_prepareThrottle->start();
    }, Qt::QueuedConnection);
}

QJsonObject MoviePreviewStreamer::status() const {
    int live = 0;
    int prepare = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        live = peerCount(Feed::Live);
        prepare = peerCount(Feed::Prepare);
    }
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return {{QStringLiteral("available"), true},
        {QStringLiteral("live"), live},
        {QStringLiteral("prepare"), prepare},
        {QStringLiteral("liveError"), m_liveError},
        {QStringLiteral("prepareError"), m_prepareError},
        {QStringLiteral("prepareFile"), QFileInfo(m_prepareRequest.file).fileName()},
        {QStringLiteral("preparePosition"), m_prepareRequest.position},
        {QStringLiteral("preparePlay"), m_prepareRequest.play}};
}

void MoviePreviewStreamer::setError(Feed feed, const QString &error) {
    std::lock_guard<std::mutex> lock(m_statusMutex);
    (feed == Feed::Live ? m_liveError : m_prepareError) = error;
}

void MoviePreviewStreamer::updateEncoders() {
    if (m_shutdown)
        return;
    int live = 0;
    int prepare = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        live = peerCount(Feed::Live);
        prepare = peerCount(Feed::Prepare);
    }

    m_wantsLive = live > 0;
    if (live == 0) {
        m_liveTimer->stop();
        stopEncoder(m_liveEncoder);
        m_liveEncoderWidth = m_liveEncoderHeight = 0;
    } else if (!m_liveTimer->isActive()) {
        m_liveRetryAt = {};
        m_liveTimer->start(); // the encoder starts with the first frame, at its size
    }

    if (prepare == 0) {
        m_prepareRepeat->stop();
        m_prepareThrottle->stop();
        stopEncoder(m_prepareEncoder);
        m_prepareKeyframe.clear();
        m_prepareQueued = false;
    } else if (!m_prepareRepeat->isActive()) {
        m_prepareRepeat->start();
        runPrepare(); // what was prepared before the first peer connected
    }
}

void MoviePreviewStreamer::writeLiveFrame() {
    int width = 0;
    int height = 0;
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (!m_liveFrame.empty()) {
            width = m_liveFrameWidth;
            height = m_liveFrameHeight;
            m_liveWriteBuffer = m_liveFrame;
        }
    }
    if (!width) {
        // Nothing rendered yet: black, so the browser has a stream to show.
        width = 640;
        height = 360;
        m_liveWriteBuffer.assign(static_cast<std::size_t>(width) * height * 4, 0);
    }

    if (width != m_liveEncoderWidth || height != m_liveEncoderHeight) {
        stopEncoder(m_liveEncoder);
        m_liveEncoderWidth = width;
        m_liveEncoderHeight = height;
        startLiveEncoder(width, height);
    } else if (!m_liveEncoder.process && Clock::now() >= m_liveRetryAt) {
        startLiveEncoder(width, height);
    }

    QProcess *process = m_liveEncoder.process;
    if (!process || process->state() == QProcess::NotRunning)
        return;
    // Drop the frame rather than queue up latency when ffmpeg falls behind.
    if (process->bytesToWrite() > static_cast<qint64>(m_liveWriteBuffer.size()))
        return;
    process->write(reinterpret_cast<const char *>(m_liveWriteBuffer.data()), static_cast<qint64>(m_liveWriteBuffer.size()));
}

void MoviePreviewStreamer::startLiveEncoder(int width, int height) {
    m_liveRetryAt = Clock::now() + std::chrono::seconds(5);
    QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("rawvideo"), QStringLiteral("-pix_fmt"), QStringLiteral("rgba"),
        QStringLiteral("-video_size"), QStringLiteral("%1x%2").arg(width).arg(height),
        QStringLiteral("-framerate"), QString::number(kLiveFps), QStringLiteral("-i"), QStringLiteral("pipe:0"),
        QStringLiteral("-an"), QStringLiteral("-vf"), QStringLiteral("format=yuv420p")};
    startEncoder(m_liveEncoder, Feed::Live, arguments + encodeArguments());
}

void MoviePreviewStreamer::runPrepare() {
    if (m_shutdown)
        return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (peerCount(Feed::Prepare) == 0)
            return; // kept for when a peer connects
    }
    PrepareRequest request;
    {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        if (!m_prepareRequested)
            return;
        request = m_prepareRequest;
    }
    if (request.file == m_prepareRunning.file && request.position == m_prepareRunning.position
        && request.play == m_prepareRunning.play && (m_prepareEncoder.process || !m_prepareKeyframe.isEmpty()))
        return;

    // Let a still that is about to be done finish, so scrubbing through a heavy file still shows
    // pictures; the newest request runs once it is.
    if (m_prepareEncoder.process && !m_prepareRunning.play
        && Clock::now() - m_prepareStarted < std::chrono::milliseconds(kPrepareStillPatienceMs)) {
        m_prepareQueued = true;
        return;
    }

    stopEncoder(m_prepareEncoder);
    m_prepareQueued = false;
    m_prepareRunning = request;
    m_prepareKeyframe.clear();
    setError(Feed::Prepare, {});
    if (request.file.isEmpty())
        return;

    QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-nostdin")};
    if (request.play)
        arguments << QStringLiteral("-re");
    arguments << QStringLiteral("-ss") << QString::number(request.position, 'f', 3)
              << QStringLiteral("-i") << request.file
              << QStringLiteral("-map") << QStringLiteral("0:v:0")
              // Never any audio, subtitles or data.
              << QStringLiteral("-an") << QStringLiteral("-sn") << QStringLiteral("-dn");
    if (!request.play)
        arguments << QStringLiteral("-frames:v") << QStringLiteral("1");
    arguments << QStringLiteral("-vf") << scaleFilter();
    m_prepareStarted = Clock::now();
    startEncoder(m_prepareEncoder, Feed::Prepare, arguments + encodeArguments());
}

void MoviePreviewStreamer::repeatPrepareFrame() {
    // While playing the encoder keeps the picture going and sends a keyframe every second.
    if (m_prepareKeyframe.isEmpty() || (m_prepareRunning.play && m_prepareEncoder.process))
        return;
    publish(Feed::Prepare, m_prepareKeyframe, true);
}

void MoviePreviewStreamer::startEncoder(Encoder &encoder, Feed feed, const QStringList &arguments) {
    auto *process = new QProcess(this);
#ifdef Q_OS_WIN
    // Below the master's own priority, and without a console window.
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= BELOW_NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW;
    });
#endif
    process->setProgram(ffmpegProgram());
    process->setArguments(arguments);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, &encoder, feed, process]() {
        if (encoder.process == process)
            readEncoder(encoder, feed, false);
    });
    connect(process, &QProcess::readyReadStandardError, this, [&encoder, process]() {
        if (encoder.process == process)
            encoder.errors = QByteArray(encoder.errors + process->readAllStandardError()).right(400);
    });
    connect(process, &QProcess::finished, this, [this, &encoder, feed, process]() {
        if (encoder.process == process)
            encoderFinished(encoder, feed, process);
    });
    connect(process, &QProcess::errorOccurred, this, [this, &encoder, feed, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || encoder.process != process)
            return;
        setError(feed, QStringLiteral("Could not start %1 to encode the preview").arg(process->program()));
        sgct::Log::Warning("MoviePreviewStreamer: could not start " + process->program().toStdString() + "\n");
        encoder.process = nullptr;
        process->deleteLater();
    });

    encoder.process = process;
    encoder.pending.clear();
    encoder.errors.clear();
    encoder.frames = 0;
    process->start();
}

void MoviePreviewStreamer::stopEncoder(Encoder &encoder) {
    QProcess *process = encoder.process;
    encoder.process = nullptr; // its signals are ignored from here on
    encoder.pending.clear();
    if (!process)
        return;
    process->disconnect(this);
    if (process->state() != QProcess::NotRunning) {
        process->kill();
        process->waitForFinished(1000);
    }
    process->deleteLater();
}

void MoviePreviewStreamer::encoderFinished(Encoder &encoder, Feed feed, QProcess *process) {
    readEncoder(encoder, feed, true);
    const QString errors = QString::fromUtf8(QByteArray(encoder.errors + process->readAllStandardError())).trimmed();
    const bool failed = process->exitStatus() != QProcess::NormalExit || process->exitCode() != 0;
    if (feed == Feed::Prepare && !encoder.frames)
        setError(feed, errors.isEmpty() ? QStringLiteral("No picture at that position") : errors);
    else if (failed)
        setError(feed, errors.isEmpty() ? QStringLiteral("The preview encoder stopped") : errors);
    if (failed && !errors.isEmpty())
        sgct::Log::Warning(std::string("MoviePreviewStreamer: ffmpeg (") + feedName(feed) + ") stopped: " + errors.toStdString() + "\n");
    encoder.process = nullptr;
    process->deleteLater();

    if (feed == Feed::Prepare && m_prepareQueued) {
        m_prepareQueued = false;
        runPrepare();
    }
}

void MoviePreviewStreamer::readEncoder(Encoder &encoder, Feed feed, bool finished) {
    if (encoder.process)
        encoder.pending += encoder.process->readAllStandardOutput();

    // An access unit runs from one access unit delimiter (NAL type 9) to the next. A start code
    // never occurs inside a NAL unit (emulation prevention), so the search cannot misfire.
    static const QByteArray delimiter("\x00\x00\x01\x09", 4);
    qsizetype from = 1;
    for (;;) {
        const qsizetype found = encoder.pending.indexOf(delimiter, from);
        if (found < 0)
            break;
        const qsizetype boundary = found > 0 && encoder.pending.at(found - 1) == 0 ? found - 1 : found;
        if (boundary > 0) {
            const QByteArray accessUnit = encoder.pending.left(boundary);
            encoder.pending.remove(0, boundary);
            const bool keyframe = webRtcIsRandomAccessPoint(reinterpret_cast<const std::uint8_t *>(accessUnit.constData()),
                static_cast<std::size_t>(accessUnit.size()), WebRtcVideoCodec::H264);
            ++encoder.frames;
            if (feed == Feed::Prepare && keyframe)
                m_prepareKeyframe = accessUnit;
            publish(feed, accessUnit, keyframe);
            from = 4;
        } else {
            from = found + 4;
        }
    }

    if (finished && !encoder.pending.isEmpty()) {
        const QByteArray accessUnit = encoder.pending;
        encoder.pending.clear();
        const bool keyframe = webRtcIsRandomAccessPoint(reinterpret_cast<const std::uint8_t *>(accessUnit.constData()),
            static_cast<std::size_t>(accessUnit.size()), WebRtcVideoCodec::H264);
        ++encoder.frames;
        if (feed == Feed::Prepare && keyframe)
            m_prepareKeyframe = accessUnit;
        publish(feed, accessUnit, keyframe);
    }
}

void MoviePreviewStreamer::publish(Feed feed, const QByteArray &accessUnit, bool keyframe) {
    std::vector<std::shared_ptr<rtc::Track>> tracks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &peer : m_peers) {
            if (peer->feed != feed || !peer->open || !peer->track)
                continue;
            // A decoder cannot start on a predicted frame.
            if (peer->waitingForKeyframe) {
                if (!keyframe)
                    continue;
                peer->waitingForKeyframe = false;
            }
            tracks.push_back(peer->track);
        }
    }
    if (tracks.empty())
        return;

    // Wall clock timestamps: the browser shows each frame as it arrives, repeated stills included.
    const std::chrono::duration<double> timestamp = Clock::now() - m_epoch;
    const auto *data = reinterpret_cast<const std::byte *>(accessUnit.constData());
    for (const auto &track : tracks) {
        try {
            if (track->isOpen())
                track->sendFrame(data, static_cast<std::size_t>(accessUnit.size()), rtc::FrameInfo(timestamp));
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("MoviePreviewStreamer: could not send a frame: ") + error.what() + "\n");
        }
    }
}

void MoviePreviewStreamer::captureLiveFrame(QOpenGLFramebufferObject *source) {
    if (!m_wantsLive.load(std::memory_order_relaxed) || !source || !source->isValid())
        return;
    const auto now = Clock::now();
    if (now - m_lastCapture < std::chrono::milliseconds(1000 / kLiveFps - 5))
        return;
    m_lastCapture = now;

    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context)
        return;
    QOpenGLExtraFunctions *gl = context->extraFunctions();

    const QSize size = previewSize(source->width(), source->height());
    if (!m_captureFbo || m_captureFbo->size() != size) {
        cleanupGL();
        m_captureFbo = new QOpenGLFramebufferObject(size);
    }
    // Downscaled on the GPU, so only the preview size is read back.
    QOpenGLFramebufferObject::blitFramebuffer(m_captureFbo, QRect(QPoint(0, 0), size), source,
        QRect(0, 0, source->width(), source->height()), GL_COLOR_BUFFER_BIT, GL_LINEAR);

    const int width = size.width();
    const int height = size.height();
    const GLsizeiptr bytes = static_cast<GLsizeiptr>(width) * height * 4;
    if (!m_pbo[0] || m_pboWidth != width || m_pboHeight != height) {
        if (m_pbo[0])
            gl->glDeleteBuffers(2, m_pbo);
        gl->glGenBuffers(2, m_pbo);
        for (const GLuint pbo : m_pbo) {
            gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
            gl->glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
        }
        gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        m_pboWidth = width;
        m_pboHeight = height;
        m_pboIndex = 0;
        m_pboFilled = false;
    }

    GLint readFramebuffer = 0;
    GLint packBuffer = 0;
    GLint packAlignment = 4;
    gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
    gl->glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    gl->glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);

    // Asynchronous: this capture lands in one PBO while the previous one (a frame interval
    // ago, long done) is mapped from the other. The mpv FBO is stored top row first, as is
    // the copy, so the rows come out in the order ffmpeg expects.
    gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_captureFbo->handle());
    gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[m_pboIndex]);
    gl->glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    const int previous = 1 - m_pboIndex;
    if (m_pboFilled) {
        gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[previous]);
        if (const void *pixels = gl->glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, bytes, GL_MAP_READ_BIT)) {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            const auto *begin = static_cast<const std::uint8_t *>(pixels);
            m_liveFrame.assign(begin, begin + bytes);
            m_liveFrameWidth = width;
            m_liveFrameHeight = height;
        }
        gl->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }
    m_pboFilled = true;
    m_pboIndex = previous;

    gl->glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(packBuffer));
    gl->glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    gl->glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
}

void MoviePreviewStreamer::cleanupGL() {
    delete m_captureFbo;
    m_captureFbo = nullptr;
    if (m_pbo[0]) {
        if (QOpenGLContext *context = QOpenGLContext::currentContext())
            context->extraFunctions()->glDeleteBuffers(2, m_pbo);
        m_pbo[0] = m_pbo[1] = 0;
    }
    m_pboWidth = m_pboHeight = 0;
    m_pboFilled = false;
}
