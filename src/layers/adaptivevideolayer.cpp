#include "adaptivevideolayer.h"
#include "videolayer.h"
#include "mdklayer.h"
#include <filesystem>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <fmt/core.h>
#include <sgct/sgct.h>

using namespace MDK_NS;

AdaptiveVideoLayer::AdaptiveVideoLayer(
    gl_adress_func_v1 opa1,
    gl_adress_func_v2 opa2,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel) 
    : mpvVideoLayer(new VideoLayer(opa1, allowDirectRendering, loggingOn, logLevel, std::bind(&AdaptiveVideoLayer::updateUsedMediaLibrary, this, std::placeholders::_1))),
    mdkVideoLayer(new MdkLayer(opa2, loggingOn, logLevel, std::bind(&AdaptiveVideoLayer::updateUsedMediaLibrary, this, std::placeholders::_1)))
{
    m_mpl = AdaptiveVideoLayer::MediaPlayerLibrary::MPV;
    m_am = AdaptiveVideoLayer::AdaptiveMethod::USE_EXTENSION;
    m_cc = AdaptiveVideoLayer::CodecChecker::USE_MDK;
    setType(BaseLayer::LayerType::VIDEO);
}

AdaptiveVideoLayer::~AdaptiveVideoLayer() = default;

void AdaptiveVideoLayer::initialize() {
    mpvVideoLayer->setIsMaster(isMaster());
    mdkVideoLayer->setIsMaster(isMaster());
    mpvVideoLayer->setIdentifier(identifier());
    mdkVideoLayer->setIdentifier(identifier());
    mpvVideoLayer->initialize();
    mdkVideoLayer->initialize();

    QFile amConfFile(QStringLiteral("./data/adaptive-video-conf.json"));

    if (!amConfFile.open(QIODevice::ReadOnly)) {
        sgct::Log::Warning(std::format("Couldn't open configuration file: {}", amConfFile.fileName().toStdString()));
    }
    else {
        sgct::Log::Info(std::format("Loading adaptive method configuration file: {}", amConfFile.fileName().toStdString()));
    }

    QByteArray amCommandsArray = amConfFile.readAll();
    QJsonDocument amCommandsDoc(QJsonDocument::fromJson(amCommandsArray));
    QJsonObject amCommands = amCommandsDoc.object();

    if (amCommands.contains(QStringLiteral("default_decoder_library"))) {
        QString ddl = amCommands.value(QStringLiteral("default_decoder_library")).toString();
        if(ddl == QStringLiteral("mdk")) {
            m_mpl_default = MediaPlayerLibrary::MDK;
        }
        else {
            m_mpl_default = MediaPlayerLibrary::MPV;
        }
        m_mpl = m_mpl_default;
    }

    if (amCommands.contains(QStringLiteral("adaptive_method"))) {
        QString am = amCommands.value(QStringLiteral("adaptive_method")).toString();
        if (am == QStringLiteral("codec")) {
            m_am = AdaptiveMethod::USE_CODEC;
        }
        else {
            m_am = AdaptiveMethod::USE_EXTENSION;
        }
    }

    if (amCommands.contains(QStringLiteral("lib_for_codec_check"))) {
        QString lfcc = amCommands.value(QStringLiteral("lib_for_codec_check")).toString();
        if (lfcc == QStringLiteral("mdk")) {
            m_cc = CodecChecker::USE_MDK;
        }
        else if (lfcc == QStringLiteral("mpv")) {
            m_cc = CodecChecker::USE_MPV;
        }
        else {
            m_cc = CodecChecker::USE_CURRENT_LIB;
        }
    }

    if (amCommands.contains(QStringLiteral("extension_priority_mpv"))) {
        QJsonArray a = amCommands[QStringLiteral("extension_priority_mpv")].toArray();
        for (int i = 0; i < a.size(); i++) {
            extPrioMpv.push_back(a.at(i).toString().toStdString());
        }
    }

    if (amCommands.contains(QStringLiteral("extension_priority_mdk"))) {
        QJsonArray a = amCommands[QStringLiteral("extension_priority_mdk")].toArray();
        for (int i = 0; i < a.size(); i++) {
            extPrioMdk.push_back(a.at(i).toString().toStdString());
        }
    }

    if (amCommands.contains(QStringLiteral("codec_priority_mpv"))) {
        QJsonArray a = amCommands[QStringLiteral("codec_priority_mpv")].toArray();
        for (int i = 0; i < a.size(); i++) {
            codecPrioMpv.push_back(a.at(i).toString().toStdString());
        }
    }

    if (amCommands.contains(QStringLiteral("codec_priority_mdk"))) {
        QJsonArray a = amCommands[QStringLiteral("codec_priority_mdk")].toArray();
        for (int i = 0; i < a.size(); i++) {
            codecPrioMdk.push_back(a.at(i).toString().toStdString());
        }
    }

    m_hasInitialized = true;
}

void AdaptiveVideoLayer::initializeGL() {
    mpvVideoLayer->initializeGL();
    mdkVideoLayer->initializeGL();
}

void AdaptiveVideoLayer::cleanup() {
    mpvVideoLayer->cleanup();
    mdkVideoLayer->cleanup();
}

void AdaptiveVideoLayer::initializeAndLoad(std::string filePath) {
    // Send empty file name, to load through adaptive function afterwards
    mpvVideoLayer->initializeAndLoad("");
    mdkVideoLayer->initializeAndLoad("");
    initialize();
    loadFile(filePath);
}

void AdaptiveVideoLayer::loadFile(std::string filePath, bool reload) {
    if (!filePath.empty()) {
        // Check which library (MPV or MDK) we should start using based on file name.
        if (std::filesystem::exists(filePath)) {
            std::filesystem::path videoPath = std::filesystem::path(filePath);
            if (videoPath.has_extension() && m_am == AdaptiveMethod::USE_EXTENSION) {
                std::filesystem::path videoPathExt = videoPath.extension();
                if (std::find(extPrioMdk.begin(), extPrioMdk.end(), videoPathExt.string().substr(1)) != extPrioMdk.end()) {
                    if (m_mpl == MediaPlayerLibrary::MPV) {
                        mpvVideoLayer->stop();
                        std::vector<std::byte> data;
                        mpvVideoLayer->encodeFull(data);
                        unsigned int pos = 0;
                        mdkVideoLayer->decodeFull(data, pos);
                        sgct::Log::Info(std::format("Switching to mdk as file has extension: {}", videoPathExt.string().substr(1)));
                    }
                    m_mpl = MediaPlayerLibrary::MDK;
                    mdkVideoLayer->loadFile(filePath, reload);
                }
                else if(std::find(extPrioMpv.begin(), extPrioMpv.end(), videoPathExt.string().substr(1)) != extPrioMpv.end()) {
                    if (m_mpl == MediaPlayerLibrary::MDK) {
                        mdkVideoLayer->stop();
                        std::vector<std::byte> data;
                        mdkVideoLayer->encodeFull(data);
                        unsigned int pos = 0;
                        mpvVideoLayer->decodeFull(data, pos);
                        sgct::Log::Info(std::format("Switching to mpv as file has extension: {}", videoPathExt.string().substr(1)));
                    }
                    m_mpl = MediaPlayerLibrary::MPV;
                    mpvVideoLayer->loadFile(filePath, reload);
                }
                else {
                    if (m_mpl_default == MediaPlayerLibrary::MDK) {
                        if (m_mpl == MediaPlayerLibrary::MPV) {
                            mpvVideoLayer->stop();
                            std::vector<std::byte> data;
                            mpvVideoLayer->encodeFull(data);
                            unsigned int pos = 0;
                            mdkVideoLayer->decodeFull(data, pos);
                            sgct::Log::Info("Switching to mdk as it is default player");
                        }
                        m_mpl = MediaPlayerLibrary::MDK;
                        mdkVideoLayer->loadFile(filePath, reload);
                    }
                    else {
                        if (m_mpl == MediaPlayerLibrary::MDK) {
                            mdkVideoLayer->stop();
                            std::vector<std::byte> data;
                            mdkVideoLayer->encodeFull(data);
                            unsigned int pos = 0;
                            mpvVideoLayer->decodeFull(data, pos);
                            sgct::Log::Info("Switching to mpv as it is default player");
                        }
                        m_mpl = MediaPlayerLibrary::MPV;
                        mpvVideoLayer->loadFile(filePath, reload);
                    }
                }
            }
            else if (m_am == AdaptiveMethod::USE_CODEC) {
                if (m_cc == CodecChecker::USE_MDK 
                    || (m_cc == CodecChecker::USE_CURRENT_LIB && m_mpl == MediaPlayerLibrary::MDK)) {
                    mdkVideoLayer->loadFile(filePath, reload);
                }
                else {
                    mpvVideoLayer->loadFile(filePath, reload);
                }
            }
            else {
                sgct::Log::Warning(std::format("Video file has no extension: {}", filePath));
            }
        }
        else {
            sgct::Log::Warning(std::format("Could not find video file: {}", filePath));
        }
    }
    else {
        sgct::Log::Warning(std::format("Video file is empty: {}", filePath));
    }
}

bool AdaptiveVideoLayer::ready() const {
    if (m_mpl == MPV && mpvVideoLayer) return mpvVideoLayer->ready();
    if (m_mpl == MDK && mdkVideoLayer) return mdkVideoLayer->ready();
    return false;
}

bool AdaptiveVideoLayer::hasTexture() const {
    return true;
}

void AdaptiveVideoLayer::update(bool updateRendering) {
    BaseLayer* sub = activeSubLayer();
    if (!sub)
        return;

    // Propagate the file path to the active sub-layer so its own update() cycle
    // (which compares loadedFile against filepath()) triggers loading. The base
    // layer's m_filepath is set by decodeBaseCore/setFilePath on this composite.
    if (sub->filepath() != filepath()) {
        sub->setFilePath(filepath());
    }

    sub->update(updateRendering);
}

void AdaptiveVideoLayer::updateFrame() {
    if (BaseLayer* sub = activeSubLayer())
        sub->updateFrame();
}

bool AdaptiveVideoLayer::renderingIsOn() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->renderingIsOn() : false;
}

void AdaptiveVideoLayer::reportSwap() {
    if (BaseLayer* sub = activeSubLayer())
        sub->reportSwap();
}

void AdaptiveVideoLayer::start() {
    if (BaseLayer* sub = activeSubLayer())
        sub->start();
}

void AdaptiveVideoLayer::stop() {
    if (BaseLayer* sub = activeSubLayer())
        sub->stop();
}

bool AdaptiveVideoLayer::pause() {
    BaseLayer* sub = activeSubLayer();
    return sub ? sub->pause() : true;
}

void AdaptiveVideoLayer::setPause(bool paused) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setPause(paused);
}

double AdaptiveVideoLayer::position() {
    BaseLayer* sub = activeSubLayer();
    return sub ? sub->position() : 0.0;
}

void AdaptiveVideoLayer::setPosition(double pos) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setPosition(pos);
}

double AdaptiveVideoLayer::duration() {
    BaseLayer* sub = activeSubLayer();
    return sub ? sub->duration() : 0.0;
}

double AdaptiveVideoLayer::remaining() {
    BaseLayer* sub = activeSubLayer();
    return sub ? sub->remaining() : 0.0;
}

bool AdaptiveVideoLayer::hasAudio() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->hasAudio() : false;
}

int AdaptiveVideoLayer::audioId() {
    BaseLayer* sub = activeSubLayer();
    return sub ? sub->audioId() : -1;
}

void AdaptiveVideoLayer::setAudioId(int id) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setAudioId(id);
}

bool AdaptiveVideoLayer::isAudioEnabled() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->isAudioEnabled() : isMaster();
}

void AdaptiveVideoLayer::enableAudio(bool enabled) {
    // Enable on both sub-layers so the active one is always in sync,
    // regardless of which library ends up playing the file.
    mpvVideoLayer->enableAudio(enabled);
    mdkVideoLayer->enableAudio(enabled);
}

std::vector<Track>* AdaptiveVideoLayer::audioTracks() {
    BaseLayer* sub = activeSubLayer();
    return sub ? sub->audioTracks() : nullptr;
}

void AdaptiveVideoLayer::updateAudioOutput() {
    if (BaseLayer* sub = activeSubLayer())
        sub->updateAudioOutput();
}

void AdaptiveVideoLayer::setVolume(int v, bool storeLevel) {
    BaseLayer::setVolume(v, storeLevel); // keep own volume level for alpha-based scaling
    mpvVideoLayer->setVolume(v, false);
    mdkVideoLayer->setVolume(v, false);
}

void AdaptiveVideoLayer::setVolumeMute(bool v) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setVolumeMute(v);
}

void AdaptiveVideoLayer::setEOFMode(int eofMode) {
    mpvVideoLayer->setEOFMode(eofMode);
    mdkVideoLayer->setEOFMode(eofMode);
}

void AdaptiveVideoLayer::setTimePause(bool paused, bool updateTime) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setTimePause(paused, updateTime);
}

void AdaptiveVideoLayer::setTimePosition(double timePos, bool updateTime) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setTimePosition(timePos, updateTime);
}

void AdaptiveVideoLayer::setLoopTime(double A, double B, bool enabled) {
    if (BaseLayer* sub = activeSubLayer())
        sub->setLoopTime(A, B, enabled);
}

int AdaptiveVideoLayer::eofMode() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->eofMode() : -1;
}

bool AdaptiveVideoLayer::loopTimeEnabled() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->loopTimeEnabled() : false;
}

double AdaptiveVideoLayer::loopTimeA() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->loopTimeA() : 0.0;
}

double AdaptiveVideoLayer::loopTimeB() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->loopTimeB() : 0.0;
}

void AdaptiveVideoLayer::setValue(std::string param, int val) {
    // MPV-specific property; only forward to the mpv sub-layer when it is active.
    if (m_mpl == MPV && mpvVideoLayer)
        mpvVideoLayer->setValue(param, val);
}

unsigned int AdaptiveVideoLayer::textureId() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->textureId() : 0;
}

int AdaptiveVideoLayer::width() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->width() : 0;
}

int AdaptiveVideoLayer::height() const {
    BaseLayer* sub = m_mpl == MDK ? static_cast<BaseLayer*>(mdkVideoLayer.get()) : static_cast<BaseLayer*>(mpvVideoLayer.get());
    return sub ? sub->height() : 0;
}

void AdaptiveVideoLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    // Same wire format as MpvLayer/MdkLayer (mediaShouldPause, timeToSet/position, timeIsDirty).
    if (BaseLayer* sub = activeSubLayer())
        sub->encodeTypeAlways(data);
}

void AdaptiveVideoLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    // Same wire format as MpvLayer/MdkLayer (mediaShouldPause, timeToSet/position, timeIsDirty).
    bool mediaShouldPause = false;
    double timeToSet = 0.0;
    bool timeIsDirty = false;

    sgct::deserializeObject(data, pos, mediaShouldPause);
    sgct::deserializeObject(data, pos, timeToSet);
    sgct::deserializeObject(data, pos, timeIsDirty);

    // Apply to both sub-layers so the active one (whichever library ends up playing)
    // has correct state. Both implementations are safe no-ops when uninitialized.
    mpvVideoLayer->setTimePause(mediaShouldPause, false);
    mpvVideoLayer->setTimePosition(timeToSet, timeIsDirty);
    mdkVideoLayer->setTimePause(mediaShouldPause, false);
    mdkVideoLayer->setTimePosition(timeToSet, timeIsDirty);
}

BaseLayer* AdaptiveVideoLayer::activeSubLayer() const {
    if (m_mpl == MDK)
        return mdkVideoLayer.get();
    return mpvVideoLayer.get();
}

BaseLayer* AdaptiveVideoLayer::get() {
    if (m_mpl == AdaptiveVideoLayer::MediaPlayerLibrary::MDK) {
        return mdkVideoLayer.get();
    }
    else {
        return mpvVideoLayer.get();
    }
}

void AdaptiveVideoLayer::updateUsedMediaLibrary(std::string codecName) {
    sgct::Log::Info(std::format("New file has codec: {}", codecName));

    if (m_am == AdaptiveMethod::USE_CODEC) {
        if (std::find(codecPrioMdk.begin(), codecPrioMdk.end(), codecName) != codecPrioMdk.end()) {
            if (m_mpl == MediaPlayerLibrary::MPV || m_cc == CodecChecker::USE_MPV) {
                mpvVideoLayer->stop();
                std::vector<std::byte> data;
                mpvVideoLayer->encodeFull(data);
                unsigned int pos = 0;
                mdkVideoLayer->decodeFull(data, pos);
                mdkVideoLayer->loadFile(mpvVideoLayer->loadedFile(), true);
                m_mpl = MediaPlayerLibrary::MDK;
                sgct::Log::Info(std::format("Switching to mdk as file has codec: {}", codecName));
            }
        }
        else if (std::find(codecPrioMpv.begin(), codecPrioMpv.end(), codecName) != codecPrioMpv.end()) {
            if (m_mpl == MediaPlayerLibrary::MDK || m_cc == CodecChecker::USE_MDK) {
                mdkVideoLayer->stop();
                std::vector<std::byte> data;
                mdkVideoLayer->encodeFull(data);
                unsigned int pos = 0;
                mpvVideoLayer->decodeFull(data, pos);
                mpvVideoLayer->loadFile(mdkVideoLayer->loadedFile(), true);
                m_mpl = MediaPlayerLibrary::MPV;
                sgct::Log::Info(std::format("Switching to mpv as file has codec: {}", codecName));
            }
        }
        else {
            if (m_mpl_default == MediaPlayerLibrary::MDK && m_mpl == MediaPlayerLibrary::MPV) {
                mpvVideoLayer->stop();
                std::vector<std::byte> data;
                mpvVideoLayer->encodeFull(data);
                unsigned int pos = 0;
                mdkVideoLayer->decodeFull(data, pos);
                mdkVideoLayer->loadFile(mpvVideoLayer->loadedFile(), true);
                m_mpl = MediaPlayerLibrary::MDK;
                sgct::Log::Info(std::format("Switching to mdk (as default player) with file that has codec: {}", codecName));
            }
            else if (m_mpl_default == MediaPlayerLibrary::MPV && m_mpl == MediaPlayerLibrary::MDK) {
                mdkVideoLayer->stop();
                std::vector<std::byte> data;
                mdkVideoLayer->encodeFull(data);
                unsigned int pos = 0;
                mpvVideoLayer->decodeFull(data, pos);
                mpvVideoLayer->loadFile(mdkVideoLayer->loadedFile(), true);
                m_mpl = MediaPlayerLibrary::MPV;
                sgct::Log::Info(std::format("Switching to mpv (as default player) with file that has codec: {}", codecName));
            }
        }
    }
}


