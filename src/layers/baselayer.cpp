#include "baselayer.h"
#include <layers/imagelayer.h>
#include <layers/mpvlayer.h>
#include <sgct/opengl.h>
#include <sgct/shareddata.h>
#ifdef NDI_SUPPORT
#include <ndi/ndilayer.h>
#endif

std::atomic_uint32_t BaseLayer::m_id_gen = 1;

std::string BaseLayer::typeDescription(BaseLayer::LayerType e) {
    switch (e) {
    case BASE:
        return "Base";
    case IMAGE:
        return "Image";
    case VIDEO:
        return "Video";
#ifdef NDI_SUPPORT
    case NDI:
        return "NDI";
#endif
    default:
        return "";
    }
}

BaseLayer *BaseLayer::createLayer(int layerType, opengl_func_adress_ptr opa, std::string strId, uint32_t numID) {
    BaseLayer *newLayer = nullptr;
    switch (layerType) {
    case static_cast<int>(BaseLayer::LayerType::IMAGE): {
        ImageLayer *newImg = new ImageLayer(strId);
        newLayer = newImg;
        break;
    }
    case static_cast<int>(BaseLayer::LayerType::VIDEO): {
        MpvLayer *newMpv = new MpvLayer(opa);
        newLayer = newMpv;
        break;
    }
#ifdef NDI_SUPPORT
    case static_cast<int>(BaseLayer::LayerType::NDI): {
        NdiLayer *newNDI = new NdiLayer();
        newLayer = newNDI;
        break;
    }
#endif
    default:
        break;
    }

    if (newLayer) {
        if (numID != 0)
            newLayer->setIdentifier(numID);
        else
            newLayer->updateIdentifierBasedOnCount();
    }

    return newLayer;
}

BaseLayer::BaseLayer() {
    m_title = "";
    m_type = BASE;
    m_identifier = 0;
    m_needSync = true;
}

BaseLayer::~BaseLayer() {
}

void BaseLayer::update() {
    // Overwrite in subclasses
}

bool BaseLayer::ready() {
    // Overwrite in subclasses
    return false;
}

uint32_t BaseLayer::identifier() const {
    return m_identifier;
}

void BaseLayer::setIdentifier(uint32_t id) {
    m_identifier = id;
}

void BaseLayer::updateIdentifierBasedOnCount() {
    m_identifier = m_id_gen++;
}

bool BaseLayer::needSync() const {
    return m_needSync;
}

void BaseLayer::setHasSynced() {
    m_needSync = false;
}

void BaseLayer::encode(std::vector<std::byte> &data) {
    sgct::serializeObject(data, m_title);
    sgct::serializeObject(data, m_filepath);
    sgct::serializeObject(data, renderData.gridMode);
    sgct::serializeObject(data, renderData.stereoMode);
    sgct::serializeObject(data, renderData.alpha);
}

void BaseLayer::decode(const std::vector<std::byte> &data, unsigned int &pos) {
    sgct::deserializeObject(data, pos, m_title);
    sgct::deserializeObject(data, pos, m_filepath);
    sgct::deserializeObject(data, pos, renderData.gridMode);
    sgct::deserializeObject(data, pos, renderData.stereoMode);
    sgct::deserializeObject(data, pos, renderData.alpha);
}

BaseLayer::LayerType BaseLayer::type() const {
    return m_type;
}

void BaseLayer::setType(LayerType t) {
    m_type = t;
    m_needSync = true;
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
    m_needSync = true;
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
    renderData.alpha = a;

    // Is handled always right now anyway.
    // If slideModel or layerModel changed
    // m_needSync = true;
}

int BaseLayer::gridMode() const {
    return renderData.gridMode;
}

void BaseLayer::setGridMode(int g) {
    renderData.gridMode = g;
    m_needSync = true;
}

int BaseLayer::stereoMode() const {
    return renderData.stereoMode;
}

void BaseLayer::setStereoMode(int s) {
    renderData.stereoMode = s;
    updatePlane();
    m_needSync = true;
}

const glm::vec3 &BaseLayer::rotate() const {
    return renderData.rotate;
}

void BaseLayer::setRotate(glm::vec3 &r) {
    renderData.rotate = r;
    m_needSync = true;
}

const glm::vec3 &BaseLayer::translate() const {
    return renderData.translate;
}

void BaseLayer::setTranslate(glm::vec3 &t) {
    renderData.translate = t;
    m_needSync = true;
}

double BaseLayer::planeAzimuth() const {
    return planeData.azimuth;
}

void BaseLayer::setPlaneAzimuth(double pA) {
    planeData.azimuth = pA;
    m_needSync = true;
}

double BaseLayer::planeElevation() const {
    return planeData.elevation;
}

void BaseLayer::setPlaneElevation(double pE) {
    planeData.elevation = pE;
    m_needSync = true;
}

double BaseLayer::planeDistance() const {
    return planeData.distance;
}

void BaseLayer::setPlaneDistance(double pD) {
    planeData.distance = pD;
    m_needSync = true;
}

double BaseLayer::planeRoll() const {
    return planeData.roll;
}

void BaseLayer::setPlaneRoll(double pR) {
    planeData.roll = pR;
    m_needSync = true;
}

void BaseLayer::setPlaneSize(glm::vec2 pS, int parc) {
    planeData.specifiedSize = pS;
    planeData.aspectRatioConsideration = parc;
    updatePlane();
    m_needSync = true;
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

    glm::vec2 calculatedPlaneSize = planeData.specifiedSize;
    int sm = renderData.stereoMode;
    if (planeData.aspectRatioConsideration == 1) { // Calculate width from video
        float ratio = float(renderData.width) / float(renderData.height);

        if (sm == 1) { // Side-by-side
            ratio *= 0.5f;
        } else if (sm == 2) { // Top-bottom
            ratio *= 2.0f;
        } else if (sm == 3) { // Top-bottom-flip
            ratio = float(renderData.height) / float(renderData.width);
            ratio *= 2.0f;
        }

        calculatedPlaneSize.x = ratio * planeData.specifiedSize.y;
    } else if (planeData.aspectRatioConsideration == 2) { // Calculate height from video
        float ratio = float(renderData.height) / float(renderData.width);

        if (sm == 1) { // Side-by-side
            ratio *= 0.5f;
        } else if (sm == 2) { // Top-bottom
            ratio *= 2.0f;
        } else if (sm == 3) { // Top-bottom-flip
            ratio = float(renderData.width) / float(renderData.height);
            ratio *= 2.0f;
        }

        calculatedPlaneSize.y = ratio * planeData.specifiedSize.x;
    }

    // Re-create plane if it isn't correct size
    if (calculatedPlaneSize.x != planeData.actualSize.x || calculatedPlaneSize.y != planeData.actualSize.y) {
        planeData.mesh = nullptr;
        planeData.actualSize = calculatedPlaneSize;
        planeData.mesh = std::make_unique<sgct::utils::Plane>(calculatedPlaneSize.x / 100.f, calculatedPlaneSize.y / 100.f);
        m_needSync = true;
    }
}
