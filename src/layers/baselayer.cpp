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
