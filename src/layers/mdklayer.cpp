#include "mdklayer.h"
#include <fmt/core.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>
#include <mdk/MediaInfo.h>
#include <mdk/RenderAPI.h>
#include <mdk/Player.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace MDK_NS;

MdkLayer::MdkLayer(gl_adress_func_v2 opa,
    bool loggingOn,
    std::string logLevel,
    onFileLoadedCallback flc) : m_player(new Player()) {
    m_openglProcAdr = opa;
    setType(BaseLayer::LayerType::VIDEO);
    m_data.fileLoadedCallback = flc;

    if(loggingOn)
        mdk::SetGlobalOption("logLevel", "all");
}

MdkLayer::~MdkLayer() = default;

void MdkLayer::initialize() {
    QFile mdkConfFile(QStringLiteral("./data/mdk-conf.json"));

    if (!mdkConfFile.open(QIODevice::ReadOnly)) {
        sgct::Log::Warning(std::format("Couldn't open mdk configuration file: {}", mdkConfFile.fileName().toStdString()));
    }
    else {
        sgct::Log::Info(std::format("Loading mdk configuration file: {}", mdkConfFile.fileName().toStdString()));
    }

    QByteArray mdkCommandsArray = mdkConfFile.readAll();
    QJsonDocument mdkCommandsDoc(QJsonDocument::fromJson(mdkCommandsArray));
    QJsonObject mdkCommands = mdkCommandsDoc.object();

    // Set property values.
    for (const QString& key : mdkCommands.keys()) {
        QJsonValue value = mdkCommands.value(key);
        m_player->setProperty(key.toStdString(), value.toString().toStdString());
        sgct::Log::Info(std::format("MDK property {} with value {}", key.toStdString(), value.toString().toStdString()));
    }

    m_hasInitialized = true;
}

void MdkLayer::initializeGL() {
    m_renderAPI.getProcAddress = m_openglProcAdr;
    m_player->setRenderAPI(&m_renderAPI);

    m_player->onMediaStatus([&](MediaStatus oldVal, MediaStatus newVal) {
        if (flags_added(oldVal, newVal, MediaStatus::Loaded)) {
            auto& c = m_player->mediaInfo().video[0].codec;
            renderData.width = c.width;
            renderData.height = c.height;
            m_data.updateRendering = true;
            if (m_data.fileLoadedCallback) {
                m_data.fileLoadedCallback(c.codec);
            }
        }
        return true;
    });

    /*if (sync && i > 0) { // sync to the 1st player
        player->onSync([&] {
            return players[0]->position() / 1000.0;
            });
        player->setMute();
    }*/

    m_data.mdkInitializedGL = true;
}

void MdkLayer::cleanup() {
    if (!m_data.mdkInitializedGL)
        return;

    // Destroy the GL renderer and all of the GL objects it allocated. If video
    // is still running, the video track will be deselected.
    if (m_data.mdkInitializedGL) {
        m_player->setVideoSurfaceSize(-1, -1);
    }

    if (m_data.fboCreated) {
        glDeleteFramebuffers(1, &m_data.fboId);
        glDeleteTextures(1, &renderData.texId);
        m_data.fboCreated = false;
    }
}

void MdkLayer::updateFrame() {
    if (!m_data.mdkInitializedGL)
        return;

    updateFbo();

    if (m_data.updateRendering && m_data.fboCreated) {
        m_player->renderVideo();
    }
}

bool MdkLayer::ready() const {
    return !m_data.loadedFile.empty() && m_data.updateRendering;
}

void MdkLayer::initializeAndLoad(std::string filePath) {
    if (!m_data.mdkInitializedGL) {
        initializeGL();
    }

    loadFile(filePath);
}

void MdkLayer::update(bool updateRendering) {
    if (!m_data.mdkInitializedGL) {
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

void MdkLayer::start() {
    if (ready() && pause()) {
        setPosition(0);
        setPause(false);
    }
}

void MdkLayer::stop() {
    if (ready() && !pause()) {
        setPause(true);
    }
}

bool MdkLayer::pause() {
    return m_data.mediaIsPaused;
}

void MdkLayer::setPause(bool pause) {
    if (isMaster()) {
        setTimePause(pause, false);
        m_data.mediaShouldPause = pause;
    }
}

double MdkLayer::position() {
    if (!m_data.loadedFile.empty())
        return static_cast<double>(m_player->position()) / 1000.0;
    else
        return 0.0;
}

void MdkLayer::setPosition(double value) {
    if (isMaster()) {
        setTimePosition(value, isMaster());
        m_data.timeToSet = value;
        m_data.timeIsDirty = true;
    }
}

double MdkLayer::duration() {
    if (!m_data.loadedFile.empty())
        return static_cast<double>(m_player->mediaInfo().duration) / 1000.0;
    else
        return 0.0;
}

double MdkLayer::remaining() {
    if (!m_data.loadedFile.empty())
        return static_cast<double>(m_player->mediaInfo().duration - m_player->position()) / 1000.0;
    else
        return 0.0;
}

bool MdkLayer::hasAudio() {
    return false;
}

int MdkLayer::audioId() {
    return 0;
}

void MdkLayer::setAudioId(int) {
}

std::vector<Track>* MdkLayer::audioTracks() {
    return nullptr;
}

void MdkLayer::updateAudioOutput() {
}

void MdkLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }
}

void MdkLayer::encodeTypeAlways(std::vector<std::byte>& data) {
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

void MdkLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_data.mediaShouldPause);
    sgct::deserializeObject(data, pos, m_data.timeToSet);
    sgct::deserializeObject(data, pos, m_data.timeIsDirty);
}

void MdkLayer::loadFile(std::string filePath, bool reload) {
    if (!filePath.empty() && (reload || m_data.loadedFile != filePath)) {
        sgct::Log::Info(std::format("Loading new file with mdk: {}", filePath));
        m_data.loadedFile = filePath;

        m_player->setMedia(filePath.c_str());

        //Prepare = Load media and set in pause mode
        m_player->prepare();
    }
}

std::string MdkLayer::loadedFile() {
    return m_data.loadedFile;
}

bool MdkLayer::renderingIsOn() const {
    return m_data.updateRendering;
}

void MdkLayer::setEOFMode(int eofMode) {
    if (eofMode != m_data.eofMode) {
        m_data.eofMode = eofMode;

        if (m_data.eofMode == 0) { // Pause
            m_player->setLoop(0);
            m_player->setProperty("keep_open", "1");
            m_player->setProperty("continue_at_end", "0");
        }
        else if (m_data.eofMode == 1) { // Continue
            m_player->setLoop(0);
            m_player->setProperty("keep_open", "0");
            m_player->setProperty("continue_at_end", "1");
        }
        else { // Loop
            m_player->setLoop(-1);
            m_player->setProperty("keep_open", "1");
            m_player->setProperty("continue_at_end", "0");
        }
    }
}

void MdkLayer::setTimePause(bool paused, bool updateTime) {
    if (paused != m_data.mediaIsPaused) {
        m_data.mediaIsPaused = paused;

        if (m_data.mediaIsPaused)
            m_player->set(State::Paused);
        else
            m_player->set(State::Playing);

        if (m_data.mediaIsPaused) {
            sgct::Log::Info("Media paused.");
            if (updateTime)
                setPosition(m_data.timePos);
        }
        else {
            sgct::Log::Info("Media playing...");
        }
    }
}

void MdkLayer::setTimePosition(double timePos, bool updateTime) {
    m_data.timePos = timePos;

    if (updateTime) {
        auto flags = SeekFlag::FromStart | SeekFlag::InCache;
        m_player->seek(static_cast<int64_t>(timePos * 1000), flags);
    }
}

void MdkLayer::setLoopTime(double A, double B, bool enabled) {
    if (enabled) {
        m_player->setRange(A, B);
    } else {
        m_player->setLoop(0);
    }
}

void MdkLayer::setValue(std::string param, int val) {
    m_player->setProperty(param, val);
}

void MdkLayer::updateFbo() {
    checkNeededMdkFboResize();
}

void MdkLayer::checkNeededMdkFboResize() {
    if (m_data.fboWidth == renderData.width && m_data.fboHeight == renderData.height)
        return;

    int maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if (renderData.width <= 0 || renderData.height <= 0 || renderData.width > maxTexSize || renderData.height > maxTexSize)
        return;

    sgct::Log::Info(std::format("New MDK FBO width:{} and height:{}", renderData.width, renderData.height));

    createMdkFBO(renderData.width, renderData.height);
}

void MdkLayer::createMdkFBO(int width, int height) {
    if (m_data.fboCreated) {
        glDeleteFramebuffers(1, &m_data.fboId);
        glDeleteTextures(1, &renderData.texId);
    }

    m_data.fboWidth = width;
    m_data.fboHeight = height;

    glGenFramebuffers(1, &m_data.fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, m_data.fboId);

    generateTexture(renderData.texId, width, height);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        renderData.texId,
        0);

    m_renderAPI.fbo = m_data.fboId;
    m_player->setRenderAPI(&m_renderAPI);
    m_player->setVideoSurfaceSize(m_data.fboWidth, m_data.fboHeight);

    m_data.fboCreated = true;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MdkLayer::generateTexture(unsigned int& id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
