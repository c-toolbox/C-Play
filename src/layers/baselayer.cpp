#include "baselayer.h"
#include <sgct/opengl.h>

BaseLayer::BaseLayer()
{
}

BaseLayer::~BaseLayer()
{
    glDeleteTextures(1, &renderData.texId);
}

unsigned int BaseLayer::textureId() {
    return renderData.texId;
}

int BaseLayer::width() {
    return renderData.width;
}

int BaseLayer::height() {
    return renderData.height;
}

float BaseLayer::alpha() {
    return renderData.alpha;
}

void BaseLayer::setAlpha(float a) {
    renderData.alpha = a;
}

int BaseLayer::gridMode() {
    return renderData.gridMode;
}

void BaseLayer::setGridMode(int g) {
    renderData.gridMode = g;
}

int BaseLayer::stereoMode() {
    return renderData.stereoMode;
}

void BaseLayer::setStereoMode(int s) {
    renderData.stereoMode = s;
    updatePlane();
}

const glm::vec3& BaseLayer::rotate() {
    return renderData.rotate;
}

void BaseLayer::setRotate(glm::vec3& r) {
    renderData.rotate = r;
}

const glm::vec3& BaseLayer::translate() {
    return renderData.translate;
}

void BaseLayer::setTranslate(glm::vec3& t) {
    renderData.translate = t;
}

double BaseLayer::planeAzimuth() {
    return planeData.azimuth;
}

void BaseLayer::setPlaneAzimuth(double pA) {
    planeData.azimuth = pA;
}

double BaseLayer::planeElevation() {
    return planeData.elevation;
}

void BaseLayer::setPlaneElevation(double pE) {
    planeData.elevation = pE;
}

double BaseLayer::planeDistance() {
    return planeData.distance;
}

void BaseLayer::setPlaneDistance(double pD) {
    planeData.distance = pD;
}

double BaseLayer::planeRoll() {
    return planeData.roll;
}

void BaseLayer::setPlaneRoll(double pR) {
    planeData.roll = pR;
}

void BaseLayer::setPlaneSize(glm::vec2 pS, int parc) {
    planeData.specifiedSize = pS;
    planeData.aspectRatioConsideration = parc;
    updatePlane();
}

void BaseLayer::drawPlane() {
    if (planeData.mesh) {
        planeData.mesh->draw();
    }
}

void BaseLayer::updatePlane() {
    if (planeData.specifiedSize.x <= 0 || planeData.specifiedSize.y <= 0)
        return;

    glm::vec2 calculatedPlaneSize = planeData.specifiedSize;
    int sm = renderData.stereoMode;
    if (planeData.aspectRatioConsideration == 1) { //Calculate width from video
        float ratio = float(renderData.width) / float(renderData.height);

        if (sm == 1) { //Side-by-side
            ratio *= 0.5f;
        }
        else if (sm == 2) { //Top-bottom
            ratio *= 2.0f;
        }
        else if (sm == 3) { //Top-bottom-flip
            ratio = float(renderData.height) / float(renderData.width);
            ratio *= 2.0f;
        }

        calculatedPlaneSize.x = ratio * planeData.specifiedSize.y;
    }
    else if (planeData.aspectRatioConsideration == 2) { //Calculate height from video
        float ratio = float(renderData.height) / float(renderData.width);

        if (sm == 1) { //Side-by-side
            ratio *= 0.5f;
        }
        else if (sm == 2) { //Top-bottom
            ratio *= 2.0f;
        }
        else if (sm == 3) { //Top-bottom-flip
            ratio = float(renderData.width) / float(renderData.height);
            ratio *= 2.0f;
        }

        calculatedPlaneSize.y = ratio * planeData.specifiedSize.x;
    }

    //Re-create plane if it isn't correct size
    if (calculatedPlaneSize != planeData.actualSize) {
        planeData.mesh = nullptr;
        planeData.actualSize = calculatedPlaneSize;
        planeData.mesh = std::make_unique<sgct::utils::Plane>(calculatedPlaneSize.x / 100.f, calculatedPlaneSize.y / 100.f);
    }
}
