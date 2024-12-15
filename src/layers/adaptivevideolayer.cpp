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
        sgct::Log::Warning(fmt::format("Couldn't open configuration file: {}", amConfFile.fileName().toStdString()));
    }
    else {
        sgct::Log::Info(fmt::format("Loading adaptive method configuration file: {}", amConfFile.fileName().toStdString()));
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
                        sgct::Log::Info(fmt::format("Switching to mdk as file has extension: {}", videoPathExt.string().substr(1)));
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
                        sgct::Log::Info(fmt::format("Switching to mpv as file has extension: {}", videoPathExt.string().substr(1)));
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
                sgct::Log::Warning(fmt::format("Video file has no extension: {}", filePath));
            }
        }
        else {
            sgct::Log::Warning(fmt::format("Could not find video file: {}", filePath));
        }
    }
    else {
        sgct::Log::Warning(fmt::format("Video file is empty: {}", filePath));
    }
}

BaseLayer* AdaptiveVideoLayer::get() {
    if (m_mpl == AdaptiveVideoLayer::MediaPlayerLibrary::MDK) {
        return mdkVideoLayer;
    }
    else {
        return mpvVideoLayer;
    }
}

void AdaptiveVideoLayer::updateUsedMediaLibrary(std::string codecName) {
    sgct::Log::Info(fmt::format("New file has codec: {}", codecName));

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
                sgct::Log::Info(fmt::format("Switching to mdk as file has codec: {}", codecName));
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
                sgct::Log::Info(fmt::format("Switching to mpv as file has codec: {}", codecName));
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
                sgct::Log::Info(fmt::format("Switching to mdk (as default player) with file that has codec: {}", codecName));
            }
            else if (m_mpl_default == MediaPlayerLibrary::MPV && m_mpl == MediaPlayerLibrary::MDK) {
                mdkVideoLayer->stop();
                std::vector<std::byte> data;
                mdkVideoLayer->encodeFull(data);
                unsigned int pos = 0;
                mpvVideoLayer->decodeFull(data, pos);
                mpvVideoLayer->loadFile(mdkVideoLayer->loadedFile(), true);
                m_mpl = MediaPlayerLibrary::MPV;
                sgct::Log::Info(fmt::format("Switching to mpv (as default player) with file that has codec: {}", codecName));
            }
        }
    }
}
