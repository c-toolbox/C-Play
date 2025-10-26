/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "baselayer.h"
#include <layers/imagelayer.h>
#include <layers/videolayer.h>
#include <layers/audiolayer.h>
#include <sgct/opengl.h>
#include <sgct/shareddata.h>
#include "audiosettings.h"
#include "presentationsettings.h"
#ifdef MDK_SUPPORT
#include <layers/adaptivevideolayer.h>
#endif
#ifdef NDI_SUPPORT
#include <ndi/ndilayer.h>
#endif
#ifdef PDF_SUPPORT
#include <layers/pdflayer.h>
#endif
#include <layers/streamlayer.h>
#ifdef SPOUT_SUPPORT
#include <layers/spoutlayer.h>
#endif
#include <layers/textlayer.h>

std::atomic_uint32_t BaseLayer::m_id_gen = 1;

std::string BaseLayer::typeDescription(BaseLayer::LayerType e) {
    switch (e) {
    case BASE:
        return "Base";
    case IMAGE:
        return "Image";
    case VIDEO:
        return "Video";
    case AUDIO:
        return "Audio";
#ifdef PDF_SUPPORT
    case PDF:
        return "PDF";
#endif
#ifdef NDI_SUPPORT
    case NDI:
        return "NDI";
#endif
    case STREAM:
        return "Stream";
#ifdef SPOUT_SUPPORT
    case SPOUT:
        return "Spout";
#endif
#ifdef SGCT_HAS_TEXT
    case TEXT:
        return "Text";
#endif
    default:
        return "";
    }
}

//Not supporting MDK in layers just yet...
#undef MDK_SUPPORT
#ifdef MDK_SUPPORT
#define FUNC_V2 gl_adress_func_v2 opa2
#else
#define FUNC_V2 gl_adress_func_v2
#endif

BaseLayer *BaseLayer::createLayer(bool isMaster, int layerType, gl_adress_func_v1 opa1, FUNC_V2, std::string strId, uint32_t numID) {
    BaseLayer *newLayer = nullptr;
    switch (layerType) {
    case static_cast<int>(BaseLayer::LayerType::IMAGE): {
        ImageLayer *newImg = new ImageLayer(strId);
        newLayer = newImg;
        break;
    }
    case static_cast<int>(BaseLayer::LayerType::VIDEO): {
#ifdef MDK_SUPPORT
        if (!isMaster) {
            AdaptiveVideoLayer* newVideo = new AdaptiveVideoLayer(opa1, opa2);
            newLayer = newVideo;
        }
        else {
            VideoLayer* newVideo = new VideoLayer(opa1);
            newLayer = newVideo;
        }
#else
        VideoLayer* newVideo = new VideoLayer(opa1);
        newLayer = newVideo;
#endif
        break;
    }
    case static_cast<int>(BaseLayer::LayerType::AUDIO): {
        AudioLayer* newAudio = new AudioLayer(opa1);
        newLayer = newAudio;
        break;
    }
#ifdef PDF_SUPPORT
    case static_cast<int>(BaseLayer::LayerType::PDF): {
        PdfLayer* newPDF = new PdfLayer();
        newLayer = newPDF;
        break;
    }
#endif
#ifdef NDI_SUPPORT
    case static_cast<int>(BaseLayer::LayerType::NDI): {
        NdiLayer *newNDI = new NdiLayer();
        newLayer = newNDI;
        break;
    }
#endif
    case static_cast<int>(BaseLayer::LayerType::STREAM): {
        StreamLayer* newStream = new StreamLayer(opa1);
        newLayer = newStream;
        break;
    }
#ifdef SPOUT_SUPPORT
    case static_cast<int>(BaseLayer::LayerType::SPOUT): {
        SpoutLayer* newSpout = new SpoutLayer();
        newLayer = newSpout;
        break;
    }
#endif
#ifdef SGCT_HAS_TEXT
    case static_cast<int>(BaseLayer::LayerType::TEXT): {
        TextLayer* newText = new TextLayer();
        newLayer = newText;
        break;
    }
#endif
    default:
        break;
    }

    if (newLayer) {
        newLayer->setIsMaster(isMaster);
        if (AudioSettings::enableAudioOnMaster() || AudioSettings::enableAudioOnNodes()) {
            newLayer->enableAudio(true);
        }
        if (numID != 0)
            newLayer->setIdentifier(numID);
        else
            newLayer->updateIdentifierBasedOnCount();
    }

    return newLayer;
}

BaseLayer::BaseLayer() {
    m_type = BASE;
    m_hierachy = FRONT;
    m_title = "";
    m_filepath = "";
    m_volume = 100;
    m_volumeScaling = 1.f;
    m_isLocked = false;
    m_isMaster = false;
    m_existOnMasterOnly = false;
    m_shouldUpdate = false;
    m_hasInitialized = false;
    m_keepVisibilityForNumSlides = 0;
    m_identifier = 0;
    setNeedSync();
}

BaseLayer::~BaseLayer() {
}

void BaseLayer::cleanup() {
    // Overwrite in derived class
}

void BaseLayer::initialize() {
    // Overwrite in derived class
}

void BaseLayer::initializeGL() {
    // Overwrite in derived class
}

void BaseLayer::initializeAndLoad(std::string) {
    // Overwrite in derived class
}

void BaseLayer::update(bool) {
    // Overwrite in derived class
}

void BaseLayer::updateFrame() {
    // Overwrite in derived class
}

bool BaseLayer::renderingIsOn() const {
    // Overwrite in derived class
    return false;
}

bool BaseLayer::ready() const {
    // Overwrite in derived class
    return false;
}

void BaseLayer::start() {
    // Overwrite in derived class
}

void BaseLayer::stop() {
    // Overwrite in derived class
}

bool BaseLayer::pause() {
    return true;
    // Overwrite in derived class
}

void BaseLayer::setPause(bool) {
    // Overwrite in derived class
}

double BaseLayer::position() {
    return 0.0;
    // Overwrite in derived class
}

void BaseLayer::setPosition(double) {
    // Overwrite in derived class
}

double BaseLayer::duration() {
    return 0.0;
    // Overwrite in derived class
}

double BaseLayer::remaining() {
    return 0.0;
    // Overwrite in derived class
}

bool BaseLayer::hasAudio() const {
    return false;
}

int BaseLayer::audioId() {
    return -1;
}

void BaseLayer::setAudioId(int) {
    // Overwrite in derived class
}

bool BaseLayer::isAudioEnabled() const {
    // Overwrite in derived class
    return isMaster(); // always support audio on master
}

void BaseLayer::enableAudio(bool) {
    // Overwrite in derived class
}

std::vector<Track>* BaseLayer::audioTracks() {
    // Overwrite in derived class
    return nullptr;
}

void BaseLayer::updateAudioOutput() {
    // Overwrite in derived class
}

void BaseLayer::setVolume(int, bool) {
    // Overwrite in derived class
}

void BaseLayer::setVolumeScaling(float vScale) {
    m_volumeScaling = vScale;
}

void BaseLayer::setVolumeMute(bool) {
    // Overwrite in derived class
}

bool BaseLayer::existOnMasterOnly() const {
    // Overwrite in derived class
    return m_existOnMasterOnly;
}

void BaseLayer::setEOFMode(int) {
    // Overwrite in derived class
}

void BaseLayer::setTimePause(bool, bool) {
    // Overwrite in derived class
}

void BaseLayer::setTimePosition(double, bool) {
    // Overwrite in derived class
}

void BaseLayer::setLoopTime(double, double, bool) {
    // Overwrite in derived class
}

void BaseLayer::setValue(std::string, int) {
    // Overwrite in derived class
}

void BaseLayer::encodeTypeCore(std::vector<std::byte>&) {
    // Overwrite in derived class
}

void BaseLayer::decodeTypeCore(const std::vector<std::byte>&, unsigned int&) {
    // Overwrite in derived class
}

void BaseLayer::encodeTypeAlways(std::vector<std::byte>&) {
    // Overwrite in derived class
}

void BaseLayer::decodeTypeAlways(const std::vector<std::byte>&, unsigned int&) {
    // Overwrite in derived class
}

void BaseLayer::encodeTypeProperties(std::vector<std::byte>&) {
    // Overwrite in derived class
}

void BaseLayer::decodeTypeProperties(const std::vector<std::byte>&, unsigned int&) {
    // Overwrite in derived class
}

void BaseLayer::encodeBaseCore(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, m_hierachy);
    sgct::serializeObject(data, m_filepath);
    sgct::serializeObject(data, renderData.flipY);
}

void BaseLayer::decodeBaseCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_hierachy);
    sgct::deserializeObject(data, pos, m_filepath);
    sgct::deserializeObject(data, pos, renderData.flipY);

    // Marking as needSync means we know update has occured, which we need to clear
    m_needSync = true;
}

void BaseLayer::encodeBaseAlways(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, m_shouldUpdate);
    sgct::serializeObject(data, m_shouldPreLoad);
    sgct::serializeObject(data, renderData.alpha);
}

void BaseLayer::decodeBaseAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_shouldUpdate);
    sgct::deserializeObject(data, pos, m_shouldPreLoad);
    sgct::deserializeObject(data, pos, renderData.alpha);
}

void BaseLayer::encodeBaseProperties(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, renderData.gridMode);
    sgct::serializeObject(data, renderData.stereoMode);

    sgct::serializeObject(data, renderData.roiEnabled);
    if (renderData.roiEnabled) {
        sgct::serializeObject(data, renderData.roi.x);
        sgct::serializeObject(data, renderData.roi.y);
        sgct::serializeObject(data, renderData.roi.z);
        sgct::serializeObject(data, renderData.roi.w);
    }

    if (renderData.gridMode == BaseLayer::GridMode::Plane) {
        sgct::serializeObject(data, planeData.azimuth);
        sgct::serializeObject(data, planeData.elevation);
        sgct::serializeObject(data, planeData.roll);
        sgct::serializeObject(data, planeData.distance);
        sgct::serializeObject(data, planeData.horizontal);
        sgct::serializeObject(data, planeData.vertical);
        sgct::serializeObject(data, planeData.specifiedSize.x);
        sgct::serializeObject(data, planeData.specifiedSize.y);
        sgct::serializeObject(data, planeData.aspectRatioConsideration);
    }
    else {
        sgct::serializeObject(data, renderData.rotate.x);
        sgct::serializeObject(data, renderData.rotate.y);
        sgct::serializeObject(data, renderData.rotate.z);
    }
}

void BaseLayer::decodeBaseProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, renderData.gridMode);
    sgct::deserializeObject(data, pos, renderData.stereoMode);

    sgct::deserializeObject(data, pos, renderData.roiEnabled);
    if (renderData.roiEnabled) {
        sgct::deserializeObject(data, pos, renderData.roi.x);
        sgct::deserializeObject(data, pos, renderData.roi.y);
        sgct::deserializeObject(data, pos, renderData.roi.z);
        sgct::deserializeObject(data, pos, renderData.roi.w);
    }

    if (renderData.gridMode == BaseLayer::GridMode::Plane) {
        sgct::deserializeObject(data, pos, planeData.azimuth);
        sgct::deserializeObject(data, pos, planeData.elevation);
        sgct::deserializeObject(data, pos, planeData.roll);
        sgct::deserializeObject(data, pos, planeData.distance);
        sgct::deserializeObject(data, pos, planeData.horizontal);
        sgct::deserializeObject(data, pos, planeData.vertical);
        sgct::deserializeObject(data, pos, planeData.specifiedSize.x);
        sgct::deserializeObject(data, pos, planeData.specifiedSize.y);
        sgct::deserializeObject(data, pos, planeData.aspectRatioConsideration);
    }
    else {
        sgct::deserializeObject(data, pos, renderData.rotate.x);
        sgct::deserializeObject(data, pos, renderData.rotate.y);
        sgct::deserializeObject(data, pos, renderData.rotate.z);
    }
}

void BaseLayer::encodeFull(std::vector<std::byte>& data) {
    encodeBaseCore(data);
    encodeBaseAlways(data);
    encodeBaseProperties(data);
    encodeTypeCore(data);
    encodeTypeAlways(data);
    encodeTypeProperties(data);
}
void BaseLayer::decodeFull(const std::vector<std::byte>& data, unsigned int& pos) {
    decodeBaseCore(data, pos);
    decodeBaseAlways(data, pos);
    decodeBaseProperties(data, pos);
    decodeTypeCore(data, pos);
    decodeTypeAlways(data, pos);
    decodeTypeProperties(data, pos);
}

void BaseLayer::encodeAlways(std::vector<std::byte>& data) {
    encodeBaseAlways(data);
    encodeTypeAlways(data);
}

void BaseLayer::decodeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    decodeBaseAlways(data, pos);
    decodeTypeAlways(data, pos);
}

bool BaseLayer::hasInitialized() const {
    return m_hasInitialized;
}

bool BaseLayer::isMaster() const {
    return m_isMaster;
}

uint32_t BaseLayer::identifier() const {
    return m_identifier;
}

bool BaseLayer::isLocked() const {
    return m_isLocked;
}

void BaseLayer::setIsLocked(bool locked) {
    m_isLocked = locked;
}

bool BaseLayer::needSync() const {
    return m_needSync;
}

void BaseLayer::setHasSynced() {
    if (m_syncIteration > 0) {
        m_syncIteration--;
    }
    else {
        m_needSync = false;
    }
}

BaseLayer::LayerType BaseLayer::type() const {
    return m_type;
}

void BaseLayer::setType(LayerType t) {
    m_type = t;
    setNeedSync();
}

BaseLayer::LayerHierarchy BaseLayer::hierarchy() const {
    return m_hierachy;
}

void BaseLayer::setHierarchy(LayerHierarchy h) {
    m_hierachy = h;
    setNeedSync();
}

std::string BaseLayer::typeName() const {
    return typeDescription(m_type);
}

std::string BaseLayer::title() const {
    return m_title;
}

void BaseLayer::setTitle(std::string t) {
    m_title = t;
}

std::string BaseLayer::filepath() const {
    return m_filepath;
}

void BaseLayer::setFilePath(std::string p) {
    m_filepath = p;
    setNeedSync();
}

int BaseLayer::volume() const {
    return m_volume;
}

int BaseLayer::keepVisibilityForNumSlides() const {
    return m_keepVisibilityForNumSlides;
}

void BaseLayer::setKeepVisibilityForNumSlides(int k) {
    m_keepVisibilityForNumSlides = k;
}

unsigned int BaseLayer::textureId() const {
    return renderData.texId;
}

int BaseLayer::width() const {
    return renderData.width;
}

int BaseLayer::height() const {
    return renderData.height;
}

float BaseLayer::alpha() const {
    return renderData.alpha;
}

void BaseLayer::setAlpha(float a) {
    if (a > 0.f) {
        // Always set as should update if visible.
        // However might still be update if not visible.
        // See slide scheme.
        setShouldUpdate(true);
    }

    // Alpha starts/stop layer depending on visibility changes.
    if (isMaster()) {
        if (alpha() <= 0.f && a > 0.f) {
            start();
        }
        else if (alpha() > 0.f && a <= 0.f) {
            stop();
        }

       //Alpha controls volume level as well, from 0 to desired value (100%)
       float volLevelF = static_cast<float>(volume()) * a;
       if (m_volumeScaling >= 0.f && m_volumeScaling < 1.f) {
           volLevelF *= m_volumeScaling;
       }
       setVolume(static_cast<int>(volLevelF), false);
    }

    renderData.alpha = a;

    // Is handled always right now anyway.
    // If slideModel or layerModel changed
    // setNeedSync();
}

bool BaseLayer::shouldUpdate() const {
    return m_shouldUpdate;
}

void BaseLayer::setShouldUpdate(bool value) {
    m_shouldUpdate = value;
}

bool BaseLayer::shouldPreLoad() const {
    return m_shouldPreLoad;
}

void BaseLayer::setShouldPreLoad(bool value) {
    m_shouldPreLoad = value;
}

bool BaseLayer::flipY() const {
    return renderData.flipY;
}

uint8_t BaseLayer::gridMode() const {
    return renderData.gridMode;
}

void BaseLayer::setGridMode(uint8_t g) {
    renderData.gridMode = g;
    setNeedSync();
}

uint8_t BaseLayer::stereoMode() const {
    return renderData.stereoMode;
}

void BaseLayer::setStereoMode(uint8_t s) {
    renderData.stereoMode = s;
    updatePlane();
    setNeedSync();
}

const glm::vec3 &BaseLayer::rotate() const {
    return renderData.rotate;
}

void BaseLayer::setRotate(glm::vec3 &r) {
    renderData.rotate = r;
    setNeedSync();
}

const glm::vec3 &BaseLayer::translate() const {
    return renderData.translate;
}

void BaseLayer::setTranslate(glm::vec3 &t) {
    renderData.translate = t;
    setNeedSync();
}

bool BaseLayer::roiEnabled() const {
    return renderData.roiEnabled;
}

void BaseLayer::setRoiEnabled(bool value) {
    renderData.roiEnabled = value;
    setNeedSync();
}

const glm::vec4 &BaseLayer::roi() const {
    return renderData.roi;
}

void BaseLayer::setRoi(glm::vec4 &r) {
    renderData.roi = r;
    setNeedSync();
}

void BaseLayer::setRoi(float x, float y, float width, float height) {
    renderData.roi.x = x;
    renderData.roi.y = y;
    renderData.roi.z = width;
    renderData.roi.w = height;
    setNeedSync();
}

double BaseLayer::planeAzimuth() const {
    return planeData.azimuth;
}

void BaseLayer::setPlaneAzimuth(double pA) {
    planeData.azimuth = pA;
    setNeedSync();
}

double BaseLayer::planeElevation() const {
    return planeData.elevation;
}

void BaseLayer::setPlaneElevation(double pE) {
    planeData.elevation = pE;
    setNeedSync();
}

double BaseLayer::planeRoll() const {
    return planeData.roll;
}

void BaseLayer::setPlaneRoll(double pR) {
    planeData.roll = pR;
    setNeedSync();
}

double BaseLayer::planeDistance() const {
    return planeData.distance;
}

void BaseLayer::setPlaneDistance(double pD) {
    planeData.distance = pD;
    setNeedSync();
}

double BaseLayer::planeHorizontal() const {
    return planeData.horizontal;
}

void BaseLayer::setPlaneHorizontal(double pH) {
    planeData.horizontal = pH;
    setNeedSync();
}

double BaseLayer::planeVertical() const {
    return planeData.vertical;
}

void BaseLayer::setPlaneVertical(double pV) {
    planeData.vertical = pV;
    setNeedSync();
}

double BaseLayer::planeWidth() const {
    return planeData.specifiedSize.x;
}

void BaseLayer::setPlaneWidth(double pW) {
    planeData.specifiedSize.x = static_cast<float>(pW);
    updatePlane();
    setNeedSync();
}

double BaseLayer::planeHeight() const {
    return planeData.specifiedSize.y;
}

void BaseLayer::setPlaneHeight(double pH) {
    planeData.specifiedSize.y = static_cast<float>(pH);
    updatePlane();
    setNeedSync();
}

uint8_t BaseLayer::planeAspectRatio() const {
    return planeData.aspectRatioConsideration;
}

void BaseLayer::setPlaneAspectRatio(uint8_t parc) {
    planeData.aspectRatioConsideration = parc;
    updatePlane();
    setNeedSync();
}

void BaseLayer::setPlaneSize(glm::vec2 pS, uint8_t parc) {
    planeData.specifiedSize = pS;
    planeData.aspectRatioConsideration = parc;
    updatePlane();
    setNeedSync();
}

void BaseLayer::drawPlane() {
    if (planeData.mesh) {
        planeData.mesh->draw();
    }
}

void BaseLayer::updatePlane() {
    if (renderData.width <= 0 || renderData.height <= 0)
        return;

    if (planeData.specifiedSize.x <= 0 || planeData.specifiedSize.y <= 0)
        return;

    float width = float(renderData.width);
    float height = float(renderData.height);
    if (renderData.roiEnabled) {
        width *= renderData.roi.z;
        height *= renderData.roi.w;
    }

    glm::vec2 calculatedPlaneSize = planeData.specifiedSize;
    int sm = renderData.stereoMode;
    if (planeData.aspectRatioConsideration == 1) { // Calculate width from video
        float ratio = width / height;

        if (sm == 1) { // Side-by-side
            ratio *= 0.5f;
        } else if (sm == 2) { // Top-bottom
            ratio *= 2.0f;
        } else if (sm == 3) { // Top-bottom-flip
            ratio = height / width;
            ratio *= 2.0f;
        }

        calculatedPlaneSize.x = ratio * planeData.specifiedSize.y;
    } else if (planeData.aspectRatioConsideration == 2) { // Calculate height from video
        float ratio = height / width;

        if (sm == 1) { // Side-by-side
            ratio *= 0.5f;
        } else if (sm == 2) { // Top-bottom
            ratio *= 2.0f;
        } else if (sm == 3) { // Top-bottom-flip
            ratio = width / height;
            ratio *= 2.0f;
        }

        calculatedPlaneSize.y = ratio * planeData.specifiedSize.x;
    }

    // Re-create plane if it isn't correct size
    if (calculatedPlaneSize.x != planeData.actualSize.x || calculatedPlaneSize.y != planeData.actualSize.y) {
        planeData.mesh = nullptr;
        planeData.actualSize = calculatedPlaneSize;
        planeData.mesh = std::make_unique<PlaneGrid>(calculatedPlaneSize.x / 100.f, calculatedPlaneSize.y / 100.f);
        setNeedSync();
    }
}

void BaseLayer::setIsMaster(bool value) {
    m_isMaster = value;
}

void BaseLayer::setIdentifier(uint32_t id) {
    m_identifier = id;
}

void BaseLayer::updateIdentifierBasedOnCount() {
    m_identifier = m_id_gen++;
}

void BaseLayer::setNeedSync() {
    m_needSync = true;
    m_syncIteration = PresentationSettings::networkSyncIterations();
}
