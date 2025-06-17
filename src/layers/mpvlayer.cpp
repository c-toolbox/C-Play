/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mpvlayer.h"
#include "application.h"
#include "audiosettings.h"
#include "track.h"
#include "qthelper.h"
#include <fmt/core.h>
#include <sgct/sgct.h>

//#define TEST_STREAM_NODE_ONLY

void loadTracks(MpvLayer::mpvData& vd) {
    if (vd.handle && !vd.loadedFile.empty()) {
        vd.audioTracks.clear();
        const QList<QVariant> tracks = mpv::qt::get_property(vd.handle, QStringLiteral("track-list")).toList();
        int audioIndex = 0;
        for (const auto& track : tracks) {
            const auto t = track.toMap();
            if (track.toMap()[QStringLiteral("type")] == QStringLiteral("audio")) {
                auto newTrack = Track();
                newTrack.setCodec(t[QStringLiteral("codec")].toString().toStdString());
                newTrack.setType(t[QStringLiteral("type")].toString().toStdString());
                newTrack.setDefaut(t[QStringLiteral("default")].toBool());
                newTrack.setDependent(t[QStringLiteral("dependent")].toBool());
                newTrack.setForced(t[QStringLiteral("forced")].toBool());
                newTrack.setId(t[QStringLiteral("id")].toLongLong());
                newTrack.setSrcId(t[QStringLiteral("src-id")].toLongLong());
                newTrack.setFfIndex(t[QStringLiteral("ff-index")].toLongLong());
                newTrack.setLang(t[QStringLiteral("lang")].toString().toStdString());
                newTrack.setTitle(t[QStringLiteral("title")].toString().toStdString());
                newTrack.setIndex(audioIndex);

                vd.audioTracks.push_back(newTrack);
                audioIndex++;
            }
        }
    }
}

void loadAudioId(MpvLayer::mpvData& vd) {
    if (vd.audioId < 0) {
        mpv::qt::set_property(vd.handle, QStringLiteral("aid"), QStringLiteral("auto"), vd.loggingOn);
    }
    else {
        mpv::qt::set_property(vd.handle, QStringLiteral("aid"), vd.audioId, vd.loggingOn);
    }
}

void on_mpv_events(MpvLayer::mpvData &vd, BaseLayer::RenderParams &rp) {
    while (vd.handle) {
        mpv_event *event = mpv_wait_event(vd.handle, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED: {
            if (vd.isMaster) {
                loadTracks(vd);
                loadAudioId(vd);
                mpv::qt::set_property(vd.handle, QStringLiteral("volume"), vd.volume, vd.loggingOn);
            }
            if (vd.fileLoadedCallback) {
                vd.fileLoadedCallback(mpv::qt::get_property(vd.handle, QStringLiteral("video-codec")).toString().toStdString());
            }
            break;
        }
        case MPV_EVENT_VIDEO_RECONFIG: {
            // Retrieve the new video size.
            int64_t w, h;
            if (mpv_get_property(vd.handle, "dwidth", MPV_FORMAT_INT64, &w) >= 0 &&
                mpv_get_property(vd.handle, "dheight", MPV_FORMAT_INT64, &h) >= 0 &&
                w > 0 && h > 0) {
                rp.width = w;
                rp.height = h;
                vd.reconfigs++;
                vd.updateRendering = (vd.reconfigs > vd.reconfigsBeforeUpdate);
                mpv::qt::set_property_async(vd.handle, QStringLiteral("time-pos"), vd.timePos);
            }
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop = (mpv_event_property *)event->data;
            if (strcmp(prop->name, "video-params") == 0) {
                if (prop->format == MPV_FORMAT_NODE) {
                    const QVariant videoParams = mpv::qt::node_to_variant(reinterpret_cast<mpv_node *>(prop->data));
                    auto vm = videoParams.toMap();
                    rp.width = vm[QStringLiteral("w")].toInt();
                    rp.height = vm[QStringLiteral("h")].toInt();
                }
            } else if (strcmp(prop->name, "pause") == 0) {
                if (prop->format == MPV_FORMAT_FLAG) {
                    bool isPaused = *reinterpret_cast<bool *>(prop->data);
                    if (isPaused != vd.mediaIsPaused)
                        mpv::qt::set_property_async(vd.handle, QStringLiteral("pause"), vd.mediaIsPaused);
                }
            } else if (strcmp(prop->name, "duration") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    vd.mediaDuration = *reinterpret_cast<double *>(prop->data);
                }
            } else if (strcmp(prop->name, "time-pos") == 0) {
                if (SyncHelper::instance().variables.timeThresholdEnabled && !vd.isStream) {
                    double timeToSet = vd.timePos;
                    // We do not want to "over-force" seeks, as this will slow down and might cause continued stutter.
                    // Normally, playback is syncronized, however looping depends on seek speed.
                    // Seek speeds (thus loop speed) is faster when no audio is present, thus nodes might be faster then master.
                    // Hence, we might need to correct things after a loop, between master and nodes.
                    if (!SyncHelper::instance().variables.timeThresholdOnLoopOnly 
                        || (vd.eofMode > 1 && timeToSet < SyncHelper::instance().variables.timeThresholdOnLoopCheckTime) 
                        || (vd.eofMode > 1 && timeToSet > (vd.mediaDuration - SyncHelper::instance().variables.timeThresholdOnLoopCheckTime) && (vd.mediaDuration > 0)) 
                        || (SyncHelper::instance().variables.loopTimeEnabled && timeToSet < (SyncHelper::instance().variables.loopTimeA + SyncHelper::instance().variables.timeThresholdOnLoopCheckTime)) 
                        || (SyncHelper::instance().variables.loopTimeEnabled && SyncHelper::instance().variables.loopTimeB < (timeToSet + SyncHelper::instance().variables.timeThresholdOnLoopCheckTime))) {
                        if (prop->format == MPV_FORMAT_DOUBLE) {
                            double latestPosition = *reinterpret_cast<double *>(prop->data);
                            if (SyncHelper::instance().variables.timeThreshold > 0 && (abs(latestPosition - timeToSet) > SyncHelper::instance().variables.timeThreshold)) {
                                mpv::qt::set_property_async(vd.handle, QStringLiteral("time-pos"), timeToSet);
                            }
                        }
                    }
                }
            }
            break;
        }

        case MPV_EVENT_LOG_MESSAGE: {
            mpv_event_log_message *message = (mpv_event_log_message *)event->data;
            if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_FATAL) {
                sgct::Log::Error(fmt::format("FATAL: {}", message->text));
            } else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_ERROR) {
                sgct::Log::Error(message->text);
            } else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_WARN) {
                sgct::Log::Warning(message->text);
            } else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_INFO) {
                sgct::Log::Info(message->text);
            } else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_V) {
                sgct::Log::Info(message->text);
            } else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_DEBUG) {
                sgct::Log::Debug(message->text);
            }
            break;
        }

        case MPV_EVENT_SHUTDOWN: {
            vd.terminate = true;
            break;
        }

        default: {
            // Ignore uninteresting or unknown events.
            break;
        }
        }
    }
}

void initMPV(MpvLayer::mpvData& vd) {
    vd.handle = mpv_create();
    if (!vd.handle)
        sgct::Log::Error("mpv context init failed");

    mpv_set_option_string(vd.handle, "vo", "libmpv");
    if (vd.loggingOn) {
        mpv_set_option_string(vd.handle, "terminal", "yes");
        mpv_set_option_string(vd.handle, "msg-level", "all=v");
        mpv_request_log_messages(vd.handle, vd.logLevel.c_str());
    }

    // Some minor options can only be set before mpv_initialize().
    if (mpv_initialize(vd.handle) < 0)
        sgct::Log::Error("mpv init failed");

    // Set default settings
    mpv::qt::set_property(vd.handle, QStringLiteral("keep-open"), QStringLiteral("yes"), vd.loggingOn);
    mpv::qt::set_property(vd.handle, QStringLiteral("loop-file"), QStringLiteral("inf"), vd.loggingOn);

    // Set if we support video or not (enabled by default)
    if (!vd.supportVideo) {
        mpv::qt::set_property(vd.handle, QStringLiteral("vid"), QStringLiteral("no"), vd.loggingOn);
    }

    // Set specific values if stream
    if (vd.isStream) {
        mpv::qt::set_property(vd.handle, QStringLiteral("profile"), QStringLiteral("low-latency"), vd.loggingOn);
        mpv::qt::set_property(vd.handle, QStringLiteral("untimed"), QStringLiteral(""), vd.loggingOn);
    }

    // Only audio on master for now
    if (vd.isMaster) {
        mpv::qt::set_property(vd.handle, QStringLiteral("aid"), QStringLiteral("auto"), vd.loggingOn);
        mpv::qt::set_property(vd.handle, QStringLiteral("volume-max"), QStringLiteral("100"), vd.loggingOn);
    }
    else {
        mpv::qt::set_property(vd.handle, QStringLiteral("aid"), QStringLiteral("no"), vd.loggingOn);
    }

    // Load mpv configurations
    mpv::qt::load_configurations(vd.handle, QString::fromStdString(SyncHelper::instance().configuration.confAll));
    if (vd.isMaster) {
        mpv::qt::load_configurations(vd.handle, QString::fromStdString(SyncHelper::instance().configuration.confMasterOnly));
    }
    else {
        mpv::qt::load_configurations(vd.handle, QString::fromStdString(SyncHelper::instance().configuration.confNodesOnly));
    }

    if (vd.supportVideo) {
        if (vd.allowDirectRendering) {
            // Run with direct rendering if requested
            if (mpv::qt::get_property(vd.handle, QStringLiteral("vd-lavc-dr")).toBool()) {
                vd.advancedControl = 1;
                vd.reconfigsBeforeUpdate = 0;
            }
            else {
                vd.advancedControl = 0;
                vd.reconfigsBeforeUpdate = 0;
            }
        }
        else {
            // Do not allow direct rendering (EVER).
            mpv::qt::set_property(vd.handle, QStringLiteral("vd-lavc-dr"), QStringLiteral("no"), vd.loggingOn);
            vd.advancedControl = 0;
            vd.reconfigsBeforeUpdate = 0;
        }
    }

    // Observe media parameters
    if (vd.supportVideo) {
        mpv_observe_property(vd.handle, 0, "video-params", MPV_FORMAT_NODE);
    }
    mpv_observe_property(vd.handle, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(vd.handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(vd.handle, 0, "duration", MPV_FORMAT_DOUBLE);
}

auto runMpvAsync = [](MpvLayer::mpvData& data, BaseLayer::RenderParams& rp) {
    data.threadRunning = true;
    initMPV(data);
    data.mpvInitialized = true;
    while (!data.terminate) {
        on_mpv_events(data, rp);
    }
    data.threadDone = true;
};

MpvLayer::MpvLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel,
    onFileLoadedCallback flc) {
    m_openglProcAdr = opa;
    m_data.allowDirectRendering = allowDirectRendering;
    m_data.loggingOn = loggingOn;
    m_data.fileLoadedCallback = flc;
}

MpvLayer::~MpvLayer() = default;

void MpvLayer::initialize() {
    m_hasInitialized = true;
    m_data.isMaster = isMaster();
}

void MpvLayer::initializeMpv() {
    // Run MPV on another thread
    if (!m_data.threadRunning) {
        m_data.trd = std::make_unique<std::thread>(runMpvAsync, std::ref(m_data), std::ref(renderData));
    }
}

void MpvLayer::initializeGL() {
}

void MpvLayer::cleanup() {
    if (!m_data.mpvInitialized)
        return;

    // Async quit
    mpv::qt::command_async(m_data.handle, QStringList() << QStringLiteral("quit"));

    // Wait for Mpv to end (running on separate thread)
    if (m_data.trd) {
        while (!m_data.threadDone) {
        }
        m_data.trd->join();
        m_data.trd = nullptr;
    }
}

void MpvLayer::updateFrame() {
}

bool MpvLayer::ready() const {
    return false;
}

void MpvLayer::initializeAndLoad(std::string filePath) {
    if (!m_data.mpvInitialized) {
        initializeMpv();

        //Wait for MPV to initialize
        while (!m_data.mpvInitialized) {}
    }

    if (!m_data.mpvInitializedGL) {
        initializeGL();
    }

    loadFile(filePath);
}

void MpvLayer::update(bool updateRendering) {
    if (!m_data.mpvInitialized) {
        initializeMpv();
    }
    else {
        if (!m_data.mpvInitializedGL) {
            initializeGL();
        }

        if (m_data.loadedFile != filepath()) {
            loadFile(filepath());
        }

        if (!isMaster()) {
            setTimePause(m_data.mediaShouldPause, false);
            setTimePosition(m_data.timeToSet, m_data.timeIsDirty);
            m_data.timeIsDirty = false;
        }

        if (updateRendering) {
            updateFrame();
        }
    }
}

void MpvLayer::start() {
    if (ready() && m_data.mediaIsPaused) {
        setPosition(0);
        setPause(false);
    }
}

void MpvLayer::stop() {
    if(ready() && !m_data.mediaIsPaused) {
        setPause(true);
    }
}

bool MpvLayer::pause() {
    return m_data.mediaIsPaused;
}

void MpvLayer::setPause(bool pause) {
    if (isMaster()) {
        setTimePause(pause, false);
        m_data.mediaShouldPause = pause;
    }
}

double MpvLayer::position() {
    if (m_data.handle && !m_data.loadedFile.empty())
        return mpv::qt::get_property(m_data.handle, QStringLiteral("time-pos")).toDouble();
    else
        return 0.0;
}

void MpvLayer::setPosition(double value) {
    if (isMaster() && !m_data.isStream) {
        setTimePosition(value, isMaster());
        m_data.timeToSet = value;
        m_data.timeIsDirty = true;
    }
}

double MpvLayer::duration() {
    if (m_data.handle && !m_data.loadedFile.empty())
        return mpv::qt::get_property(m_data.handle, QStringLiteral("duration")).toDouble();
    else
        return 0.0;
}

double MpvLayer::remaining() {
    if (m_data.handle && !m_data.loadedFile.empty())
        return mpv::qt::get_property(m_data.handle, QStringLiteral("time-remaining")).toDouble();
    else
        return 0.0;
}

bool MpvLayer::hasAudio() {
    return !m_data.audioTracks.empty();
}

int MpvLayer::audioId() {
    if (m_data.handle && !m_data.loadedFile.empty())
        return mpv::qt::get_property(m_data.handle, QStringLiteral("aid")).toInt();
    else
        return m_data.audioId;
}

void MpvLayer::setAudioId(int value) {
    if (value == audioId()) {
        return;
    }

    m_data.audioId = value;
    if (m_data.handle && !m_data.loadedFile.empty()) {
        loadAudioId(m_data);
    }
}

std::vector<Track>* MpvLayer::audioTracks() {
    if (m_data.handle && !m_data.loadedFile.empty()) {
        loadTracks(m_data);
        setAudioId(m_data.audioId);
        return &m_data.audioTracks;
    }
    else {
        return nullptr;
    }
}

void MpvLayer::updateAudioOutput() {
    if (m_data.mpvInitialized) {
        if (AudioSettings::useCustomAudioOutput()) {
            if (AudioSettings::useAudioDevice()) {
                mpv::qt::set_property(m_data.handle, QStringLiteral("audio-device"), AudioSettings::preferredAudioOutputDevice(), m_data.loggingOn);
            }
            else if (AudioSettings::useAudioDriver()) {
                mpv::qt::set_property(m_data.handle, QStringLiteral("ao"), AudioSettings::preferredAudioOutputDriver(), m_data.loggingOn);
            }
        }
    }
}

void MpvLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }

    m_data.volume = v;

    if (isMaster() && m_data.mpvInitialized) {
        mpv::qt::set_property(m_data.handle, QStringLiteral("volume"), v, m_data.loggingOn);
    }
}

void MpvLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_data.mediaShouldPause);
    if (m_data.timeIsDirty) {
        sgct::serializeObject(data, m_data.timeToSet);
    }
    else {
        sgct::serializeObject(data, position());
    }
    sgct::serializeObject(data, m_data.timeIsDirty);
    m_data.timeIsDirty = false;
}

void MpvLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_data.mediaShouldPause);
    sgct::deserializeObject(data, pos, m_data.timeToSet);
    sgct::deserializeObject(data, pos, m_data.timeIsDirty);
}

void MpvLayer::loadFile(std::string filePath, bool reload) {
    if (!filePath.empty() && (reload || m_data.loadedFile != filePath)) {
        sgct::Log::Info(fmt::format("Loading new file with mpv: {}", filePath));
        m_data.reconfigs = 0;
        m_data.updateRendering = false;
        m_data.loadedFile = filePath;
        m_data.audioTracks.clear();
#ifdef TEST_STREAM_NODE_ONLY
        if(m_data.isStream && m_data.isMaster)
            mpv::qt::command_async(m_data.handle, QStringList() << QStringLiteral("loadfile") << QString::fromStdString(filePath + "testfile"));
        else
#endif
            mpv::qt::command_async(m_data.handle, QStringList() << QStringLiteral("loadfile") << QString::fromStdString(filePath));
    }
}

std::string MpvLayer::loadedFile() {
    return m_data.loadedFile;
}

bool MpvLayer::renderingIsOn() const {
    return m_data.updateRendering;
}

void MpvLayer::setEOFMode(int eofMode) {
    if (eofMode != m_data.eofMode) {
        m_data.eofMode = eofMode;

        if (m_data.eofMode == 0) { // Pause
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
        } else if (m_data.eofMode == 1) { // Continue
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("keep-open"), QStringLiteral("no"));
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
        } else { // Loop
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("loop-file"), QStringLiteral("inf"));
        }
    }
}

void MpvLayer::setTimePause(bool paused, bool updateTime) {
    if (paused != m_data.mediaIsPaused) {
        m_data.mediaIsPaused = paused;
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("pause"), m_data.mediaIsPaused);
        if (m_data.mediaIsPaused) {
            sgct::Log::Info("Media paused.");
            if(updateTime && !m_data.isStream)
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("time-pos"), m_data.timePos);
        }
        else {
            sgct::Log::Info("Media playing...");
        }
    }
}

void MpvLayer::setTimePosition(double timePos, bool updateTime) {
    m_data.timePos = timePos;

    if (updateTime)
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("time-pos"), timePos);
}

void MpvLayer::setLoopTime(double A, double B, bool enabled) {
    if (enabled) {
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-a"), A);
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-b"), B);
    } else {
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-a"), QStringLiteral("no"));
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-b"), QStringLiteral("no"));
    }
}

void MpvLayer::setValue(std::string param, int val) {
    mpv::qt::set_property_async(m_data.handle, QString::fromStdString(param), val);
}
