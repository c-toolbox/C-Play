/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mpvlayer.h"
#include "application.h"
#include "audiosettings.h"
#include "track.h"
#include "qthelper.h"
#include "utils/framesynccontroller.h"
#include <sgct/sgct.h>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

void loadTracks(MpvLayer::mpvData& vd) {
    if (vd.handle && vd.mpvInitialized && !vd.loadedFile.empty()) {
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
    if (vd.handle && vd.mpvInitialized) {
        if (vd.audioId < 0) {
            mpv::qt::set_property(vd.handle, QStringLiteral("aid"), QStringLiteral("auto"), vd.loggingOn);
        }
        else {
            mpv::qt::set_property(vd.handle, QStringLiteral("aid"), vd.audioId, vd.loggingOn);
        }
    }
}

// Resolve the root folder containing the per-layer mpv options files.
// Mirrors the logic used by MpvOptionsModel so the UI and the loader agree.
static std::string mpvConfRootFolder() {
    const QString confAll = QString::fromStdString(SyncHelper::instance().configuration.confAll);
    if (!confAll.isEmpty()) {
        QDir dir = QFileInfo(confAll).absoluteDir();
        if (dir.cdUp() && dir.exists())
            return dir.absolutePath().toStdString();
    }

    const QString nextToApp = QCoreApplication::applicationDirPath() + QStringLiteral("/data/mpv-conf");
    if (QDir(nextToApp).exists())
        return nextToApp.toStdString();

    return "./data/mpv-conf";
}

// Build the per-layer mpv options file path based on the layer type suffix.
// Returns an empty string if no options should be applied, or if the file
// does not exist (a missing options file is not an error, we simply keep the
// global configuration).
std::string mpvOptionsFilePath(const MpvLayer::mpvData& vd) {
    if (vd.mpvOptionsName.empty())
        return "";

    std::string suffix;
    switch (static_cast<BaseLayer::LayerType>(vd.layerType)) {
    case BaseLayer::VIDEO:
        suffix = "_video";
        break;
    case BaseLayer::AUDIO:
        suffix = "_audio";
        break;
    case BaseLayer::STREAM:
        suffix = "_stream";
        break;
    default:
        return "";
    }

    const std::string path = mpvConfRootFolder() + "/" + vd.mpvOptionsName + suffix + ".json";
    if (!QFileInfo::exists(QString::fromStdString(path))) {
        sgct::Log::Warning(std::format("Could not find mpv options file: {}. Using global settings only.", path));
        return "";
    }

    return path;
}

void on_mpv_events(MpvLayer::mpvData &vd, BaseLayer::RenderParams) {
    while (vd.handle) {
        mpv_event *event = mpv_wait_event(vd.handle, 0.1);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED: {
            if (vd.audioEnabled) {
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
                vd.pendingWidth = static_cast<int>(w);
                vd.pendingHeight = static_cast<int>(h);
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
                    vd.pendingWidth = vm[QStringLiteral("w")].toInt();
                    vd.pendingHeight = vm[QStringLiteral("h")].toInt();
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
                if (vd.mediaIsPaused)
                    return;

                if (prop->format == MPV_FORMAT_DOUBLE) {
                    if(vd.loopTimeEnabled && vd.eofMode == 0) { // Pause on loop
                        double latestPosition = *reinterpret_cast<double*>(prop->data);
                        if (latestPosition >= vd.loopTimeB) {
                            vd.mediaShouldPause = true;
                            vd.mediaIsPaused = true;
                            mpv::qt::set_property_async(vd.handle, QStringLiteral("pause"), true);
                            mpv::qt::set_property_async(vd.handle, QStringLiteral("time-pos"), vd.loopTimeB);
                            return;
                        }
                    }
                    if (!vd.isMaster && SyncHelper::instance().variables.timeThresholdEnabled && !vd.isStream) {
                        double timeToSet = vd.timePos;
                        double latestPosition = *reinterpret_cast<double*>(prop->data);
                        // We do not want to "over-force" seeks, as this will slow down and might cause continued stutter.
                        // Normally, playback is syncronized, however looping depends on seek speed.
                        // Seek speeds (thus loop speed) is faster when no audio is present, thus nodes might be faster then master.
                        // Hence, we might need to correct things after a loop, between master and nodes.
                        if (vd.timeThresholdSetSkips <= 0 && (!SyncHelper::instance().variables.timeThresholdOnLoopOnly
                            || (vd.eofMode > 1 && timeToSet < SyncHelper::instance().variables.timeThresholdOnLoopCheckTime)
                            || (vd.eofMode > 1 && timeToSet > (vd.mediaDuration - SyncHelper::instance().variables.timeThresholdOnLoopCheckTime) && (vd.mediaDuration > 0))
                            || (SyncHelper::instance().variables.loopTimeEnabled && timeToSet < (SyncHelper::instance().variables.loopTimeA + SyncHelper::instance().variables.timeThresholdOnLoopCheckTime))
                            || (SyncHelper::instance().variables.loopTimeEnabled && SyncHelper::instance().variables.loopTimeB < (timeToSet + SyncHelper::instance().variables.timeThresholdOnLoopCheckTime)))) {
                            if (SyncHelper::instance().variables.timeThreshold > 0 && (abs(latestPosition - timeToSet) > SyncHelper::instance().variables.timeThreshold)) {
                                mpv::qt::set_property_async(vd.handle, QStringLiteral("time-pos"), timeToSet);
                                vd.timeThresholdSetSkips = SyncHelper::instance().variables.timeThresholdSetSkips;
                            }
                        }
                        else if (vd.timeThresholdSetSkips > 0) {
                            vd.timeThresholdSetSkips -= 1;
                        }
                    }
                }
            }
            break;
        }

        case MPV_EVENT_LOG_MESSAGE: {
            mpv_event_log_message *message = (mpv_event_log_message *)event->data;
            if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_FATAL) {
                sgct::Log::Error(std::format("FATAL: {}", message->text));
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

bool initMPV(MpvLayer::mpvData& vd) {
    vd.handle = mpv_create();
    if (!vd.handle) {
        sgct::Log::Error("mpv context init failed");
        return false;
    }

    mpv_set_option_string(vd.handle, "vo", "libmpv");
    if (vd.loggingOn) {
        mpv_set_option_string(vd.handle, "terminal", "yes");
        mpv_set_option_string(vd.handle, "msg-level", "all=v");
        mpv_request_log_messages(vd.handle, vd.logLevel.c_str());
    }

    // Some minor options can only be set before mpv_initialize().
    if (mpv_initialize(vd.handle) < 0) {
        sgct::Log::Error("mpv init failed");
        mpv_destroy(vd.handle);
        vd.handle = nullptr;
        return false;
    }

    // Set EOF mode
    if (vd.eofMode == 0) { // Pause
        mpv::qt::set_property_async(vd.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
        mpv::qt::set_property_async(vd.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
    }
    else if (vd.eofMode == 1) { // Continue
        mpv::qt::set_property_async(vd.handle, QStringLiteral("keep-open"), QStringLiteral("no"));
        mpv::qt::set_property_async(vd.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
    }
    else { // Loop
        mpv::qt::set_property_async(vd.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
        mpv::qt::set_property_async(vd.handle, QStringLiteral("loop-file"), QStringLiteral("inf"));
    }

    //Set Loop time
    if (vd.loopTimeEnabled && vd.eofMode == 2) {
        mpv::qt::set_property_async(vd.handle, QStringLiteral("ab-loop-a"), vd.loopTimeA);
        mpv::qt::set_property_async(vd.handle, QStringLiteral("ab-loop-b"), vd.loopTimeB);
    }
    else {
        mpv::qt::set_property_async(vd.handle, QStringLiteral("ab-loop-a"), QStringLiteral("no"));
        mpv::qt::set_property_async(vd.handle, QStringLiteral("ab-loop-b"), QStringLiteral("no"));
    }

    // Set if we support video or not (enabled by default)
    if (!vd.supportVideo) {
        mpv::qt::set_property(vd.handle, QStringLiteral("vid"), QStringLiteral("no"), vd.loggingOn);
    }

    // Set specific values if stream
    if (vd.isStream) {
        mpv::qt::set_property(vd.handle, QStringLiteral("profile"), QStringLiteral("low-latency"), vd.loggingOn);
        mpv::qt::set_property(vd.handle, QStringLiteral("untimed"), QStringLiteral(""), vd.loggingOn);
    }

    // Set audio properties
    if (vd.audioEnabled) {
        mpv::qt::set_property(vd.handle, QStringLiteral("aid"), QStringLiteral("auto"), vd.loggingOn);
        mpv::qt::set_property(vd.handle, QStringLiteral("volume-max"), QStringLiteral("100"), vd.loggingOn);

        if (AudioSettings::useCustomAudioOutput()) {
            if (AudioSettings::useAudioDevice()) {
                mpv::qt::set_property(vd.handle, QStringLiteral("audio-device"), AudioSettings::preferredAudioOutputDevice(), vd.loggingOn);
            }
            else if (AudioSettings::useAudioDriver()) {
                mpv::qt::set_property(vd.handle, QStringLiteral("ao"), AudioSettings::preferredAudioOutputDriver(), vd.loggingOn);
            }
        }
        if (vd.isMaster) {
            if (vd.audioEnabled && AudioSettings::enableAudioOnMaster()) {
                mpv::qt::set_property(vd.handle, QStringLiteral("mute"), false, vd.loggingOn);
            }
            else if (vd.audioEnabled && !AudioSettings::enableAudioOnMaster()) {
                mpv::qt::set_property(vd.handle, QStringLiteral("mute"), true, vd.loggingOn);
            }
        }
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

    // Apply per-layer options (after global settings so they take precedence)
    std::string layerOptionsPath = mpvOptionsFilePath(vd);
    if (!layerOptionsPath.empty()) {
        mpv::qt::load_configurations(vd.handle, QString::fromStdString(layerOptionsPath));
    }
    vd.mpvOptionsNameApplied = vd.mpvOptionsName;
    vd.mpvOptionsApplied = true;

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
    return true;
}

auto runMpvAsync = [](MpvLayer::mpvData& data, BaseLayer::RenderParams& rp) {
    data.threadRunning = true;
    const bool initialized = initMPV(data);
    data.mpvInitialized = initialized;
    data.initializationDone = true;
    data.initializationCondition.notify_all();
    if (initialized) {
        while (!data.terminate) {
            on_mpv_events(data, rp);
        }
    }
    data.threadDone = true;
    data.threadRunning = false;
    data.initializationCondition.notify_all();
};

MpvLayer::MpvLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel,
    onFileLoadedCallback flc) {
    m_openglProcAdr = opa;
    m_data.allowDirectRendering = allowDirectRendering;
    m_data.loggingOn = loggingOn;
    m_data.logLevel = std::move(logLevel);
    m_data.fileLoadedCallback = flc;
}

MpvLayer::~MpvLayer() = default;

void MpvLayer::initialize() {
    m_hasInitialized = true;
    m_data.isMaster = isMaster();
    m_data.layerType = static_cast<int>(type());
}

void MpvLayer::initializeMpv() {
    // Run MPV on another thread
    if (!m_data.threadRunning && !m_data.trd) {
        m_data.initializationDone = false;
        m_data.mpvInitialized = false;
        m_data.threadDone = false;
        m_data.terminate = false;
        m_data.threadRunning = true;
        m_data.trd = std::make_unique<std::thread>(runMpvAsync, std::ref(m_data), std::ref(renderData));
    }
}

void MpvLayer::initializeGL() {
}

void MpvLayer::cleanup() {
    m_data.terminate = true;
    if (m_data.mpvInitialized && m_data.handle) {
        mpv::qt::command_async(m_data.handle, QStringList() << QStringLiteral("quit"));
        mpv_wakeup(m_data.handle);
    }

    if (m_data.trd) {
        if (m_data.trd->joinable()) {
            m_data.trd->join();
        }
        m_data.trd.reset();
    }

    if (m_data.handle) {
        mpv_destroy(m_data.handle);
        m_data.handle = nullptr;
    }
    m_data.mpvInitialized = false;
    m_data.threadRunning = false;
    m_data.threadDone = true;
}

void MpvLayer::updateFrame() {
}

bool MpvLayer::ready() const {
    return false;
}

bool MpvLayer::hasTexture() const {
    return true;
}

void MpvLayer::initializeAndLoad(std::string filePath) {
    if (!m_data.mpvInitialized) {
        initializeMpv();

        std::unique_lock<std::mutex> lock(m_data.initializationMutex);
        m_data.initializationCondition.wait(lock, [this]() {
            return m_data.initializationDone.load();
        });
        if (!m_data.mpvInitialized) {
            return;
        }
    }

    if (!m_data.mpvInitializedGL) {
        initializeGL();
    }

    loadFile(filePath);
}

void MpvLayer::update(bool updateRendering) {
    std::lock_guard<std::mutex> lock(m_updateMutex);

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
            const SyncHelper::SyncVariables& v = SyncHelper::instance().variables;
            if (v.frameSyncEnabled) {
                updateFrameSyncSettings();
                applyFrameSyncCorrection(m_data.timeToSet, position(), v.playbackSpeed);
            } else {
                setTimePosition(m_data.timeToSet, m_data.timeIsDirty);
            }
            m_data.timeIsDirty = false;
            if (m_data.typePropertiesDecode) {
                enableAudio(m_data.audioEnabled_Dec);
                setLoadAudioInVidFolder(m_data.loadAudioInVidFolder_Dec);
                setAudioId(m_data.audioId_Dec);
                setVolume(m_data.volume_Dec);
                setVolumeMute(m_data.volumeMute_Dec);
                setEOFMode(m_data.eofMode_Dec);
                setLoopTime(m_data.loopTimeA_Dec, m_data.loopTimeB_Dec, m_data.loopTimeEnabled_Dec);
                setMpvOptionsName(m_data.mpvOptionsName_Dec);
                m_data.typePropertiesDecode = false;
            }
        }

        if (updateRendering) {
            updateFrame();
        }
    }
}

void MpvLayer::start() {
    if (ready() && m_data.mediaIsPaused) {
        if (isAudioEnabled()) {
            setAudioId(m_data.audioId);
        }
        if (m_data.loopTimeEnabled) {
            setPosition(m_data.loopTimeA);
        }
        else {
            setPosition(0);
        }
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

bool MpvLayer::hasAudio() const {
    return !m_data.audioTracks.empty();
}

int MpvLayer::audioId() {
    if (m_data.mpvInitialized && m_data.handle && !m_data.loadedFile.empty())
        return mpv::qt::get_property(m_data.handle, QStringLiteral("aid")).toInt();
    else
        return m_data.audioId;
}

void MpvLayer::setAudioId(int value) {
    if (value == audioId() || value < 0) {
        return;
    }

    m_data.audioId = value;
    if (m_data.handle && !m_data.loadedFile.empty()) {
        loadAudioId(m_data);
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void MpvLayer::enableAudio(bool enabled) {
    if (m_data.audioEnabled == enabled)
        return;

    m_data.audioEnabled = enabled;

    if (!m_data.mpvInitialized)
        return;

    // This will also be triggered on mpv initialization
    if (enabled) {
        mpv::qt::set_property(m_data.handle, QStringLiteral("aid"), QStringLiteral("auto"), m_data.loggingOn);
        mpv::qt::set_property(m_data.handle, QStringLiteral("volume-max"), QStringLiteral("100"), m_data.loggingOn);
        updateAudioOutput();
    }
    else {
        mpv::qt::set_property(m_data.handle, QStringLiteral("aid"), QStringLiteral("no"), m_data.loggingOn);
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
        if (isMaster()) {
            if (m_data.audioEnabled && AudioSettings::enableAudioOnMaster()) {
                mpv::qt::set_property(m_data.handle, QStringLiteral("mute"), false, m_data.loggingOn);
            }
            else if (m_data.audioEnabled && !AudioSettings::enableAudioOnMaster()) {
                mpv::qt::set_property(m_data.handle, QStringLiteral("mute"), true, m_data.loggingOn);
            }
        }
    }
}

void MpvLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }

    if (m_data.volume == v)
        return;

    m_data.volume = v;
    if (m_data.mpvInitialized) {
        mpv::qt::set_property(m_data.handle, QStringLiteral("volume"), v, m_data.loggingOn);
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void MpvLayer::setVolumeMute(bool v) {
    if (m_data.volumeMute == v)
        return;

    if (m_data.mpvInitialized) {
        if (isMaster() && AudioSettings::enableAudioOnMaster()) {
            m_data.volumeMute = v;
            mpv::qt::set_property(m_data.handle, QStringLiteral("mute"), v, m_data.loggingOn);
        }
        if (!isMaster() && m_data.audioEnabled) {
            m_data.volumeMute = v;
            mpv::qt::set_property(m_data.handle, QStringLiteral("mute"), v, m_data.loggingOn);
        }
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void MpvLayer::setLoadAudioInVidFolder(bool v) {
    if (m_data.loadAudioInVidFolder == v)
        return;

    if (m_data.mpvInitialized) {
        m_data.loadAudioInVidFolder = v;
        QString loadAudioInVidFolder = m_data.loadAudioInVidFolder ? QStringLiteral("all") : QStringLiteral("no");
        mpv::qt::set_property(m_data.handle, QStringLiteral("audio-file-auto"), loadAudioInVidFolder, m_data.loggingOn);
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
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

void MpvLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, AudioSettings::enableAudioOnNodes());
    if (AudioSettings::enableAudioOnNodes()) {
        sgct::serializeObject(data, m_data.loadAudioInVidFolder);
        sgct::serializeObject(data, m_data.audioId);
        sgct::serializeObject(data, m_data.volume);
        sgct::serializeObject(data, m_data.volumeMute);
    }
    sgct::serializeObject(data, m_data.eofMode);
    sgct::serializeObject(data, m_data.loopTimeEnabled);
    sgct::serializeObject(data, m_data.loopTimeA);
    sgct::serializeObject(data, m_data.loopTimeB);
    sgct::serializeObject(data, m_data.mpvOptionsName);
}

void MpvLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_data.audioEnabled_Dec);
    if (m_data.audioEnabled_Dec) {
        sgct::deserializeObject(data, pos, m_data.loadAudioInVidFolder_Dec);
        sgct::deserializeObject(data, pos, m_data.audioId_Dec);
        sgct::deserializeObject(data, pos, m_data.volume_Dec);
        sgct::deserializeObject(data, pos, m_data.volumeMute_Dec);
    }
    sgct::deserializeObject(data, pos, m_data.eofMode_Dec);
    sgct::deserializeObject(data, pos, m_data.loopTimeEnabled_Dec);
    sgct::deserializeObject(data, pos, m_data.loopTimeA_Dec);
    sgct::deserializeObject(data, pos, m_data.loopTimeB_Dec);
    sgct::deserializeObject(data, pos, m_data.mpvOptionsName_Dec);
    m_data.typePropertiesDecode = true;
}

void MpvLayer::loadFile(std::string filePath, bool reload) {
    if (m_data.mpvInitialized && !filePath.empty() && (reload || m_data.loadedFile != filePath)) {
        sgct::Log::Info(std::format("Loading new file with mpv: {}", filePath));
        m_data.reconfigs = 0;
        m_data.updateRendering = false;
        m_data.loadedFile = filePath;
        m_data.audioTracks.clear();

        // Re-apply global settings first, then the layer-specific options,
        // so options persist across file loads (mpv may reset them per file).
        m_data.mpvOptionsApplied = false;
        applyMpvOptions();

        // Build command options list (mirrors MpvObject::loadItem behavior)
        QStringList optionsList;

        // Apply optional separate audio file via loadfile command
        if (!m_data.audioFile.empty()) {
            optionsList << QStringLiteral("audio-file=") + QString::fromStdString(m_data.audioFile);
        }

        QString options = optionsList.join(QStringLiteral(","));

        QStringList newCommand = QStringList() << QStringLiteral("loadfile")
                                                   << QString::fromStdString(filePath);
#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(2, 3) && MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2, 4)
        newCommand << QStringLiteral("0");
#endif
        if (!options.isEmpty()) {
            newCommand << options;
        }
        mpv::qt::command_async(m_data.handle, newCommand);
    }
}

std::string MpvLayer::loadedFile() {
    return m_data.loadedFile;
}

bool MpvLayer::renderingIsOn() const {
    return m_data.updateRendering;
}

int MpvLayer::eofMode() const {
    return m_data.eofMode;
}

void MpvLayer::setEOFMode(int eofMode) {
    if (eofMode != m_data.eofMode) {
        m_data.eofMode = eofMode;

        if (m_data.mpvInitialized) {
            if (m_data.eofMode == 0) { // Pause
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
            }
            else if (m_data.eofMode == 1) { // Continue
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("keep-open"), QStringLiteral("no"));
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
            }
            else { // Loop
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
                mpv::qt::set_property_async(m_data.handle, QStringLiteral("loop-file"), QStringLiteral("inf"));
            }
            if (loopTimeEnabled())
                setLoopTime(loopTimeA(), loopTimeB(), loopTimeEnabled());
        }

        if (isMaster())
            setNeedSync();
    }
}

void MpvLayer::setTimePause(bool paused, bool updateTime) {
    if (paused != m_data.mediaIsPaused) {
        m_data.mediaIsPaused = paused;
        if (m_data.mpvInitialized) {
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("pause"), m_data.mediaIsPaused);
            if (m_data.mediaIsPaused) {
                sgct::Log::Info("Media paused.");
                if (updateTime && !m_data.isStream)
                    mpv::qt::set_property_async(m_data.handle, QStringLiteral("time-pos"), m_data.timePos);
            }
            else {
                sgct::Log::Info("Media playing...");
            }
        }
    }
}

void MpvLayer::setTimePosition(double timePos, bool updateTime) {
    m_data.timePos = timePos;

    if (updateTime && m_data.mpvInitialized)
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("time-pos"), timePos);
}

void MpvLayer::setLoopTime(double A, double B, bool enabled) {
    m_data.loopTimeEnabled = enabled;
    m_data.loopTimeA = A;
    m_data.loopTimeB = B;

    if (m_data.mpvInitialized) {
        if (enabled && m_data.eofMode == 2) {
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-a"), A);
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-b"), B);
        }
        else {
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-a"), QStringLiteral("no"));
            mpv::qt::set_property_async(m_data.handle, QStringLiteral("ab-loop-b"), QStringLiteral("no"));
        }
    }

    if (isMaster())
        setNeedSync();
}

bool MpvLayer::loopTimeEnabled() const {
    return m_data.loopTimeEnabled;
}

double MpvLayer::loopTimeA() const {
    return m_data.loopTimeA;
}

double MpvLayer::loopTimeB() const {
    return m_data.loopTimeB;
}

void MpvLayer::setValue(std::string param, int val) {
    if (m_data.mpvInitialized) {
        mpv::qt::set_property_async(m_data.handle, QString::fromStdString(param), val);
    }
}

void MpvLayer::setSpeed(double speed) {
    if (m_data.mpvInitialized) {
        mpv::qt::set_property_async(m_data.handle, QStringLiteral("speed"), speed);
    }
}

double MpvLayer::speed() {
    if (m_data.handle && m_data.mpvInitialized) {
        return mpv::qt::get_property(m_data.handle, QStringLiteral("speed")).toDouble();
    }
    return 1.0;
}

void MpvLayer::updateFrameSyncSettings() {
    // Frame sync tunables are mirrored from the master into SyncVariables
    // (see MpvObject::updateFrameSyncSettings), so slaves read the synced values.
    const SyncHelper::SyncVariables& v = SyncHelper::instance().variables;
    m_frameSyncController.configure(
        v.frameSyncSeekThreshold,
        v.frameSyncSpeedAdjustThreshold,
        v.frameSyncMaxSpeedAdjust,
        v.frameSyncInitialOffset);
}

void MpvLayer::applyFrameSyncCorrection(double masterPos, double slavePos, double baseSpeed) {
    FrameSyncController::Decision d = m_frameSyncController.decide(masterPos, slavePos, baseSpeed);
    switch (d.action) {
    case FrameSyncController::Action::None:
        if (baseSpeed != speed()) {
            setSpeed(baseSpeed);
        }
        break;
    case FrameSyncController::Action::SpeedAdjust:
        setSpeed(d.speed);
        break;
    case FrameSyncController::Action::HardSeek:
        setTimePosition(d.seekTarget, true);
        break;
    }
}

void MpvLayer::setAudioFile(const std::string &audioFile) {
    m_data.audioFile = audioFile;
}

std::string MpvLayer::mpvOptionsName() const {
    return m_data.mpvOptionsName;
}

void MpvLayer::setMpvOptionsName(const std::string &name) {
    if (m_data.mpvOptionsName == name)
        return;

    m_data.mpvOptionsName = name;
    // The selection changed, so the currently loaded configuration is stale.
    m_data.mpvOptionsApplied = false;

    applyMpvOptions();

    if (isMaster())
        setNeedSync();
}

void MpvLayer::applyMpvOptions() {
    if (!m_data.mpvInitialized)
        return;

    // Only push configuration to mpv when it has not been applied yet for the
    // current options identifier. This keeps repeated sync updates cheap.
    if (m_data.mpvOptionsApplied && m_data.mpvOptionsNameApplied == m_data.mpvOptionsName)
        return;

    // Re-apply global settings first, then the layer-specific options
    mpv::qt::load_configurations(m_data.handle, QString::fromStdString(SyncHelper::instance().configuration.confAll));
    if (isMaster()) {
        mpv::qt::load_configurations(m_data.handle, QString::fromStdString(SyncHelper::instance().configuration.confMasterOnly));
    }
    else {
        mpv::qt::load_configurations(m_data.handle, QString::fromStdString(SyncHelper::instance().configuration.confNodesOnly));
    }

    std::string optionsPath = mpvOptionsFilePath(m_data);
    if (!optionsPath.empty()) {
        mpv::qt::load_configurations(m_data.handle, QString::fromStdString(optionsPath));
    }

    m_data.mpvOptionsNameApplied = m_data.mpvOptionsName;
    m_data.mpvOptionsApplied = true;
}

void MpvLayer::reportSwap() {
    if (m_data.renderContext) {
        mpv_render_context_report_swap(m_data.renderContext);
    }
}
