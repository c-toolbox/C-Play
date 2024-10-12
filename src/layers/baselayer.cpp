#include "baselayer.h"
#include <layers/imagelayer.h>
#include <layers/mpvlayer.h>
#include <sgct/opengl.h>
#include <sgct/shareddata.h>
#ifdef NDI_SUPPORT
#include <ndi/ndilayer.h>
#endif
#ifdef PDF_SUPPORT
#include <layers/pdflayer.h>
#endif

std::atomic_uint32_t BaseLayer::m_id_gen = 1;

std::string BaseLayer::typeDescription(BaseLayer::LayerType e) {
    switch (e) {
    case BASE:
        return "Base";
    case IMAGE:
        return "Image";
#ifdef PDF_SUPPORT
    case PDF:
        return "PDF";
#endif
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

BaseLayer *BaseLayer::createLayer(bool isMaster, int layerType, opengl_func_adress_ptr opa, std::string strId, uint32_t numID) {
    BaseLayer *newLayer = nullptr;
    switch (layerType) {
    case static_cast<int>(BaseLayer::LayerType::IMAGE): {
        ImageLayer *newImg = new ImageLayer(strId);
        newLayer = newImg;
        break;
    }
#ifdef PDF_SUPPORT
    case static_cast<int>(BaseLayer::LayerType::PDF): {
        PdfLayer* newPDF = new PdfLayer();
        newLayer = newPDF;
        break;
    }
#endif
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
        newLayer->setIsMaster(isMaster);
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
    m_hierachy = FRONT;
    m_page = 0;
    m_numPages = 0;
    m_isMaster = false;
    m_shouldUpdate = false;
    m_hasInitialized = false;
    m_keepVisibilityForNumSlides = 0;
    m_identifier = 0;
    m_needSync = true;
}

BaseLayer::~BaseLayer() {
}

void BaseLayer::initialize() {
    // Overwrite in subclasses
}

void BaseLayer::update(bool) {
    // Overwrite in subclasses
}

bool BaseLayer::ready() {
    // Overwrite in subclasses
    return false;
}

void BaseLayer::start() {
    // Overwrite in subclasses
}

void BaseLayer::stop() {
    // Overwrite in subclasses
}

bool BaseLayer::hasInitialized() {
    return m_hasInitialized;
}

bool BaseLayer::isMaster() const {
    return m_isMaster;
}

uint32_t BaseLayer::identifier() const {
    return m_identifier;
}

bool BaseLayer::needSync() const {
    return m_needSync;
}

void BaseLayer::setHasSynced() {
    m_needSync = false;
}

void BaseLayer::encodeFull(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_hierachy);
    sgct::serializeObject(data, m_filepath);
    sgct::serializeObject(data, m_page);
    sgct::serializeObject(data, renderData.flipY);
    encodeMinimal(data);
    encodeProperties(data);
}

void BaseLayer::encodeMinimal(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_shouldUpdate);
    sgct::serializeObject(data, renderData.alpha);
}

void BaseLayer::encodeProperties(std::vector<std::byte> &data) {
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
        sgct::serializeObject(data, planeData.distance);
        sgct::serializeObject(data, planeData.roll);
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

void BaseLayer::decodeFull(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_hierachy);
    sgct::deserializeObject(data, pos, m_filepath);
    sgct::deserializeObject(data, pos, m_page);
    sgct::deserializeObject(data, pos, renderData.flipY);
    decodeMinimal(data, pos);
    decodeProperties(data, pos);

    // Marking as needSync means we need know update has occured, which we need to clear
    m_needSync = true;
}

void BaseLayer::decodeMinimal(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_shouldUpdate);
    sgct::deserializeObject(data, pos, renderData.alpha);
}

void BaseLayer::decodeProperties(const std::vector<std::byte> &data, unsigned int &pos) {
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
        sgct::deserializeObject(data, pos, planeData.distance);
        sgct::deserializeObject(data, pos, planeData.roll);
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

BaseLayer::LayerType BaseLayer::type() const {
    return m_type;
}

void BaseLayer::setType(LayerType t) {
    m_type = t;
    m_needSync = true;
}

BaseLayer::LayerHierarchy BaseLayer::hierarchy() const {
    return m_hierachy;
}

void BaseLayer::setHierarchy(LayerHierarchy h) {
    m_hierachy = h;
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

int BaseLayer::page() const {
    return m_page;
}

void BaseLayer::setPage(int p) {
    if (p < 1) {
        m_page = 1;
    }
    else if (p >= numPages()) {
        m_page = numPages();
    }
    else {
        m_page = p;
    }
    m_needSync = true;
}

int BaseLayer::numPages() const {
    return m_numPages;
}

void BaseLayer::setNumPages(int np) {
    if (m_page >= np) {
        sgct::Log::Info("Page number out of page count, resetting to first page");
        m_page = 1;
    }

    m_numPages = np;
    m_needSync = true;
}

int BaseLayer::keepVisibilityForNumSlides() {
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

    renderData.alpha = a;

    // Is handled always right now anyway.
    // If slideModel or layerModel changed
    // m_needSync = true;
}

bool BaseLayer::shouldUpdate() const {
    return m_shouldUpdate;
}

void BaseLayer::setShouldUpdate(bool value) {
    m_shouldUpdate = value;
}

bool BaseLayer::flipY() const {
    return renderData.flipY;
}

uint8_t BaseLayer::gridMode() const {
    return renderData.gridMode;
}

void BaseLayer::setGridMode(uint8_t g) {
    renderData.gridMode = g;
    m_needSync = true;
}

uint8_t BaseLayer::stereoMode() const {
    return renderData.stereoMode;
}

void BaseLayer::setStereoMode(uint8_t s) {
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

bool BaseLayer::roiEnabled() const {
    return renderData.roiEnabled;
}

void BaseLayer::setRoiEnabled(bool value) {
    renderData.roiEnabled = value;
    m_needSync = true;
}

const glm::vec4 &BaseLayer::roi() const {
    return renderData.roi;
}

void BaseLayer::setRoi(glm::vec4 &r) {
    renderData.roi = r;
    m_needSync = true;
}

void BaseLayer::setRoi(float x, float y, float width, float height) {
    renderData.roi.x = x;
    renderData.roi.y = y;
    renderData.roi.z = width;
    renderData.roi.w = height;
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

double BaseLayer::planeWidth() const {
    return planeData.specifiedSize.x;
}

void BaseLayer::setPlaneWidth(double pW) {
    planeData.specifiedSize.x = static_cast<float>(pW);
    updatePlane();
    m_needSync = true;
}

double BaseLayer::planeHeight() const {
    return planeData.specifiedSize.y;
}

void BaseLayer::setPlaneHeight(double pH) {
    planeData.specifiedSize.y = static_cast<float>(pH);
    updatePlane();
    m_needSync = true;
}

uint8_t BaseLayer::planeAspectRatio() const {
    return planeData.aspectRatioConsideration;
}

void BaseLayer::setPlaneAspectRatio(uint8_t parc) {
    planeData.aspectRatioConsideration = parc;
    updatePlane();
    m_needSync = true;
}

void BaseLayer::setPlaneSize(glm::vec2 pS, uint8_t parc) {
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
        planeData.mesh = std::make_unique<sgct::utils::Plane>(calculatedPlaneSize.x / 100.f, calculatedPlaneSize.y / 100.f);
        m_needSync = true;
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
