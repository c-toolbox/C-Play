/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layerqtitem.h"
#include "application.h"
#include "layersmodel.h"
#include "presentationsettings.h"
#include "gridsettings.h"
#include "slidesmodel.h"

#include <QOpenGLContext>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/qquickwindow.h>
#include <array>

LayerQtItem::LayerQtItem()
    : m_layerIdx(-1), m_layer(nullptr), m_renderer(nullptr), 
    m_viewOffset(0, 0), m_viewSize(0, 0), m_roiOffset(0, 0), m_roiSize(0, 0) {
    connect(this, &QQuickItem::windowChanged, this, &LayerQtItem::handleWindowChanged);
}

int LayerQtItem::layerIdx() const{
    return m_layerIdx;
}

void LayerQtItem::setLayerIdx(int idx) {
    m_layerIdx = idx;
    BaseLayer *nl = Application::instance().slidesModel()->selectedSlide()->layer(m_layerIdx);
    if (nl == nullptr) {
        m_layerIdx = -1;
        m_layer = nullptr;
        return;
    }
    if (nl != m_layer) {
        m_layer = nl;
        Q_EMIT layerChanged();
        if (window()) {
            window()->update();
        }
    }
}

int LayerQtItem::layerStereoMode() const{
    if (m_layer)
        return m_layer->stereoMode();
    else
        return PresentationSettings::defaultStereoModeForLayers();
}

void LayerQtItem::setLayerStereoMode(int mode) {
    if (m_layer) {
        m_layer->setStereoMode(mode);
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerGridMode() const{
    if (m_layer)
        return m_layer->gridMode();
    else
        return PresentationSettings::defaultGridModeForLayers();
}

void LayerQtItem::setLayerGridMode(int mode) {
    if (m_layer) {
        m_layer->setGridMode(mode);
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerVisibility() const{
    if (m_layer)
        return static_cast<int>(m_layer->alpha() * 100.f);
    else
        return PresentationSettings::defaultLayerVisibility();
}

void LayerQtItem::setLayerVisibility(int value) {
    if (m_layer) {
        m_layer->setAlpha(static_cast<float>(value) * 0.01f);
        Q_EMIT layerValueChanged();
    }
}

bool LayerQtItem::layerPause() const {
    if (m_layer)
        return m_layer->pause();
    else
        return 0.0;
}

void LayerQtItem::setLayerPause(bool value) {
    if (m_layer) {
        m_layer->setPause(value);
        Q_EMIT layerPositionChanged();
    }
}

double LayerQtItem::layerPosition() const {
    if (m_layer)
        return m_layer->position();
    else
        return 0.0;
}

void LayerQtItem::setLayerPosition(double value) {
    if (m_layer) {
        m_layer->setPosition(value);
        Q_EMIT layerPositionChanged();
    }
}

double LayerQtItem::layerDuration() const {
    if (m_layer)
        return m_layer->duration();
    else
        return 0.0;
}

double LayerQtItem::layerRemaining() const {
    if (m_layer)
        return m_layer->remaining();
    else
        return 0.0;
}

int LayerQtItem::layerPage() const {
    if (m_layer)
        return m_layer->page();
    else
        return -1;
}

void LayerQtItem::setLayerPage(int value) {
    if (m_layer) {
        m_layer->setPage(value);
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerNumPages() const {
    if (m_layer)
        return m_layer->numPages();
    else
        return -1;
}

double LayerQtItem::layerRotatePitch() const {
    if (m_layer)
        return m_layer->rotate().x;
    else
        return 0.0;
}

void LayerQtItem::setLayerRotatePitch(double x) {
    if (m_layer) {
        glm::vec3 rot = m_layer->rotate();
        rot.x = x;
        m_layer->setRotate(rot);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerRotateYaw() const {
    if (m_layer)
        return m_layer->rotate().y;
    else
        return 0.0;
}

void LayerQtItem::setLayerRotateYaw(double y)
{
    if (m_layer) {
        glm::vec3 rot = m_layer->rotate();
        rot.y = y;
        m_layer->setRotate(rot);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerRotateRoll() const {
    if (m_layer)
        return m_layer->rotate().z;
    else
        return 0.0;
}

void LayerQtItem::setLayerRotateRoll(double z) {
    if (m_layer) {
        glm::vec3 rot = m_layer->rotate();
        rot.z = z;
        m_layer->setRotate(rot);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneWidth() const {
    if (m_layer)
        return m_layer->planeWidth();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneWidth(double pW) {
    if (m_layer) {
        m_layer->setPlaneWidth(pW);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneHeight() const {
    if (m_layer)
        return m_layer->planeHeight();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneHeight(double pH) {
    if (m_layer) {
        m_layer->setPlaneHeight(pH);
        Q_EMIT layerValueChanged();
    }
}

int LayerQtItem::layerPlaneAspectRatio() const {
    if (m_layer)
        return m_layer->planeAspectRatio();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneAspectRatio(int parc) {
    if (m_layer) {
        m_layer->setPlaneAspectRatio(parc);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneAzimuth() const {
    if (m_layer)
        return m_layer->planeAzimuth();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneAzimuth(double pA) {
    if (m_layer) {
        m_layer->setPlaneAzimuth(pA);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneElevation() const {
    if (m_layer)
        return m_layer->planeElevation();
    else
        return GridSettings::plane_Elevation_Degrees();
}

void LayerQtItem::setLayerPlaneElevation(double pE) {
    if (m_layer) {
        m_layer->setPlaneElevation(pE);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneDistance() const {
    if (m_layer)
        return m_layer->planeDistance();
    else
        return GridSettings::plane_Distance_CM();
}

void LayerQtItem::setLayerPlaneDistance(double pD) {
    if (m_layer) {
        m_layer->setPlaneDistance(pD);
        Q_EMIT layerValueChanged();
    }
}

double LayerQtItem::layerPlaneRoll() const {
    if (m_layer)
        return m_layer->planeRoll();
    else
        return 0.0;
}

void LayerQtItem::setLayerPlaneRoll(double pR) {
    if (m_layer) {
        m_layer->setPlaneRoll(pR);
        Q_EMIT layerValueChanged();
    }
}

bool LayerQtItem::layerRoiEnabled() {
    if (m_layer)
        return m_layer->roiEnabled();
    else
        return false;
}

void LayerQtItem::setLayerRoiEnabled(bool value) {
    if (m_layer) {
        m_layer->setRoiEnabled(value);
        Q_EMIT layerValueChanged();
        if (value)
            updateRoi();
    }
}

QString LayerQtItem::layerTypeName() {
    if (m_layer)
        return QString::fromStdString(m_layer->typeName());
    else
        return QStringLiteral("");
}

QString LayerQtItem::layerTitle() {
    if (m_layer)
        return QString::fromStdString(m_layer->title());
    else
        return QStringLiteral("");
}

void LayerQtItem::setLayerTitle(QString value) {
    if (m_layer) {
        m_layer->setTitle(value.toStdString());
        Q_EMIT layerValueChanged();
    }
}

QSize LayerQtItem::textureSize() {
    if (m_layer) {
        return QSize(static_cast<int>(m_layer->width()), static_cast<int>(m_layer->height()));
    }
    else {
        return m_viewSize;
    }
}

QPoint LayerQtItem::viewOffset() {
    return m_viewOffset;
}

void LayerQtItem::setViewOffset(QPoint p) {
    m_viewOffset = p;
    Q_EMIT viewChanged();
}

QSize LayerQtItem::viewSize() {
    return m_viewSize;
}

void LayerQtItem::setViewSize(QSize s) {
    m_viewSize = s;
    Q_EMIT viewChanged();
}

QPoint LayerQtItem::roiOffset() {
    return m_roiOffset;
}

void LayerQtItem::setRoiOffset(QPoint p) {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.x = glm::max(static_cast<float>(p.x() - m_viewOffset.x()) / static_cast<float>(m_viewSize.width()), 0.f);
        
        //UI uses Top-Left coordinate system, while we want to store in Bottom-Left
        float yFromTop = glm::max(static_cast<float>(p.y() - m_viewOffset.y()) / static_cast<float>(m_viewSize.height()), 0.f);
        currentRoi.y = 1.f - currentRoi.w - yFromTop;
        //currentRoi.y = glm::max(static_cast<float>(p.y() - m_viewOffset.y()) / static_cast<float>(m_viewSize.height()), 0.f);

        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

QSize LayerQtItem::roiSize() {
    return m_roiSize;
}

void LayerQtItem::setRoiSize(QSize s) {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        //UI uses Top-Left coordinate system, while we store in Bottom-Left
        float yFromTop = 1.f - currentRoi.w - currentRoi.y;
        currentRoi.z = glm::min(static_cast<float>(s.width()) / static_cast<float>(m_viewSize.width()), 1.f - currentRoi.x);
        currentRoi.w = glm::min(static_cast<float>(s.height()) / static_cast<float>(m_viewSize.height()), 1.f - yFromTop);
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

void LayerQtItem::setRoi(QPoint offset, QSize size) {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.x = glm::max(static_cast<float>(offset.x() - m_viewOffset.x()) / static_cast<float>(m_viewSize.width()), 0.f);
        //UI uses Top-Left coordinate system, while we want to store in Bottom-Left
        float yFromTop = glm::max(static_cast<float>(offset.y() - m_viewOffset.y()) / static_cast<float>(m_viewSize.height()), 0.f);
        currentRoi.z = glm::min(static_cast<float>(size.width()) / static_cast<float>(m_viewSize.width()), 1.f - currentRoi.x);
        currentRoi.w = glm::min(static_cast<float>(size.height()) / static_cast<float>(m_viewSize.height()), 1.f - yFromTop);
        currentRoi.y = 1.f - currentRoi.w - yFromTop;
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

void LayerQtItem::dragLeft(double Xdiff) {
    setRoi(m_roiOffset + QPoint(Xdiff, 0), m_roiSize + QSize(-Xdiff, 0));
}

void LayerQtItem::dragRight(double Xdiff) {
    setRoi(m_roiOffset, m_roiSize + QSize(Xdiff, 0));
}

void LayerQtItem::dragTop(double Ydiff) {
    setRoi(m_roiOffset + QPoint(0, Ydiff), m_roiSize + QSize(0, -Ydiff));
}

void LayerQtItem::dragBottom(double Ydiff) {
    setRoi(m_roiOffset, m_roiSize + QSize(0, Ydiff));
}

QPoint LayerQtItem::roiTexOffset() {
    if (m_layer) {
        QPoint realRoiOffset;
        realRoiOffset.setX(static_cast<int>(m_layer->roi().x * m_layer->width()));
        realRoiOffset.setY(static_cast<int>(m_layer->roi().y * m_layer->height()));
        return realRoiOffset;
    }
    else {
        return m_roiOffset;
    }
}

void LayerQtItem::setRoiTexOffset(QPoint p) {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.x = glm::max(static_cast<float>(p.x()) / static_cast<float>(m_layer->width()), 0.f);
        //This value should already be Bottom-Left
        currentRoi.y = glm::max(static_cast<float>(p.y()) / static_cast<float>(m_layer->height()), 0.f);
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

QSize LayerQtItem::roiTexSize() {
    if (m_layer) {
        QSize realRoiSize;
        realRoiSize.setWidth(static_cast<int>(m_layer->roi().z * m_layer->width()));
        realRoiSize.setHeight(static_cast<int>(m_layer->roi().w * m_layer->height()));
        return realRoiSize;
    }
    else {
        return m_roiSize;
    }
}

void LayerQtItem::setRoiTexSize(QSize s) {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        currentRoi.z = glm::min(static_cast<float>(s.width()) / static_cast<float>(m_layer->width()), 1.f);
        currentRoi.w = glm::min(static_cast<float>(s.height()) / static_cast<float>(m_layer->height()), 1.f);
        m_layer->setRoi(currentRoi);
        updateRoi();
    }
}

void LayerQtItem::updateView() {
    if (m_renderer) {
        m_viewOffset = m_renderer->viewOffset() / window()->devicePixelRatio();
        m_viewSize = m_renderer->viewSize() / window()->devicePixelRatio();
        updateRoi();
        Q_EMIT viewChanged();
    }
}

void LayerQtItem::updateRoi() {
    if (m_layer) {
        glm::vec4 currentRoi = m_layer->roi();
        m_roiOffset.setX((static_cast<int>(currentRoi.x * static_cast<float>(m_viewSize.width()))) + m_viewOffset.x());

        //UI uses Top-Left coordinate system, while we store in Bottom-Left
        float yFromTop = 1.f - currentRoi.w - currentRoi.y;
        m_roiOffset.setY((static_cast<int>(yFromTop * static_cast<float>(m_viewSize.height()))) + m_viewOffset.y());
        //m_roiOffset.setY((static_cast<int>(currentRoi.y * static_cast<float>(m_viewSize.height()))) + m_viewOffset.y());

        m_roiSize.setWidth(static_cast<int>(currentRoi.z * static_cast<float>(m_viewSize.width())));
        m_roiSize.setHeight(static_cast<int>(currentRoi.w * static_cast<float>(m_viewSize.height())));
        Q_EMIT roiChanged();
    }
}

void LayerQtItem::handleWindowChanged(QQuickWindow *win) {
    if (win) {
        connect(win, &QQuickWindow::beforeSynchronizing, this, &LayerQtItem::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &LayerQtItem::cleanup, Qt::DirectConnection);
        win->setColor(Qt::black);

        if (m_timer == nullptr) {
            m_timer = new QTimer();
            m_timer->setInterval((1.0f / 60.0f) * 1000.0f);

            connect(m_timer, &QTimer::timeout, win, &QQuickWindow::update);

            m_timer->start();
        }
    }
}

void LayerQtItem::cleanup() {
    delete m_renderer;
    m_renderer = nullptr;
}

class CleanupJob : public QRunnable {
public:
    CleanupJob(LayerQtItemRenderer *renderer) : m_renderer(renderer) {}
    void run() override { delete m_renderer; }

private:
    LayerQtItemRenderer *m_renderer;
};

void LayerQtItem::releaseResources() {
    window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

LayerQtItemRenderer::~LayerQtItemRenderer() {
    delete m_program;
}

void LayerQtItemRenderer::setWindowSize(const QSize &size) {
    m_windowSize = size;
}

void LayerQtItemRenderer::setViewportSize(const QSize &size) {
    m_viewportSize = size;
}

void LayerQtItemRenderer::setPosition(const QPoint &position) {
    m_position = position;
}

void LayerQtItemRenderer::setWindow(QQuickWindow *window) {
    m_window = window;
}

BaseLayer *LayerQtItemRenderer::layer() {
    return m_layer;
}

void LayerQtItemRenderer::setLayer(BaseLayer *l) {
    m_layer = l;
}

QPoint LayerQtItemRenderer::viewOffset() {
    return m_viewOffset;
}

QSize LayerQtItemRenderer::viewSize() {
    return m_viewSize;
}

void LayerQtItemRenderer::calculateViewParameters() {
    int viewW = m_viewportSize.width();
    int viewH = m_viewportSize.height();
    int offsetX = 0;
    int offsetY = 0;
    float ratioViewport = static_cast<float>(viewW) / static_cast<float>(viewH);
    float ratioLayer = static_cast<float>(m_layer->width()) / static_cast<float>(m_layer->height());
    if (ratioLayer > ratioViewport) {
        // Use full width of viewport
        if (ratioLayer > 1.f) {
            viewH = static_cast<int>(static_cast<float>(m_viewportSize.width()) / ratioLayer);
        }
        else {
            viewH = static_cast<int>(static_cast<float>(m_viewportSize.width()) * ratioLayer);
        }
        offsetY = static_cast<int>(0.5f * (static_cast<float>(m_viewportSize.height() - viewH)));
    }
    else if (ratioLayer < ratioViewport) {
        // Use full height of viewport
        if (ratioLayer > 1.f) {
            viewW = static_cast<int>(static_cast<float>(m_viewportSize.height()) * ratioLayer);
        }
        else {
            viewW = static_cast<int>(static_cast<float>(m_viewportSize.height()) / ratioLayer);
        }
        offsetX = static_cast<int>(0.5f * (static_cast<float>(m_viewportSize.width() - viewW)));
    }

    if (offsetX != m_viewOffset.x() || offsetY != m_viewOffset.y()
        || viewW != m_viewSize.width() || viewH != m_viewSize.height()) {
        m_viewOffset.setX(offsetX);
        m_viewOffset.setY(offsetY);
        m_viewSize.setWidth(viewW);
        m_viewSize.setHeight(viewH);
        Q_EMIT viewChanged();
    }
}

void LayerQtItem::sync() {
    if (!m_renderer) {
        m_renderer = new LayerQtItemRenderer();
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &LayerQtItemRenderer::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &LayerQtItemRenderer::paint, Qt::DirectConnection);
        connect(m_renderer, &LayerQtItemRenderer::viewChanged, this, &LayerQtItem::updateView);
    }
    m_renderer->setLayer(m_layer);
    m_renderer->setWindowSize(window()->size() * window()->devicePixelRatio());
    m_renderer->setViewportSize(this->size().toSize() * window()->devicePixelRatio());
    m_renderer->setPosition(this->position().toPoint());
    m_renderer->setWindow(window());

    if (m_layer) {
        if (m_layer->type() == BaseLayer::LayerType::VIDEO) {
            Q_EMIT layerPositionChanged();
        }
    }
}

void LayerQtItemRenderer::init() {
    if (!m_program) {
        QSGRendererInterface *rif = m_window->rendererInterface();
        Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

        initializeOpenGLFunctions();

        m_program = new QOpenGLShaderProgram();

        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex,
                                                    "attribute highp vec3 vertices;"
                                                    "attribute highp vec2 texcoords;"
                                                    "varying highp vec2 coords;"
                                                    "uniform bool flipY;"
                                                    "void main() {"
                                                    "    gl_Position = vec4(vertices, 1.0);"
                                                    "    coords = flipY ? vec2(texcoords.x, 1.0-texcoords.y) : texcoords;"
                                                    "}");
        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Fragment,
                                                    "varying highp vec2 coords;"
                                                    "uniform sampler2D tex;"
                                                    "void main() {"
                                                    "    gl_FragColor = texture2D(tex, coords);"
                                                    "}");

        m_program->bindAttributeLocation("vertices", 0);
        m_program->bindAttributeLocation("texcoords", 1);

        m_program->link();

        constexpr std::array<const float, 36> QuadVerts = {
            //     x     y     z      u    v      r    g    b    a
            -1.f, -1.f, -1.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f,
            1.f, -1.f, -1.f, 1.f, 0.f, 1.f, 1.f, 1.f, 1.f,
            -1.f, 1.f, -1.f, 0.f, 1.f, 1.f, 1.f, 1.f, 1.f,
            1.f, 1.f, -1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

        m_vao.create();
        m_vao.bind();
        // setting up buffers
        m_vbo.create();
        m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_vbo.bind();
        m_vbo.allocate(QuadVerts.data(), 36 * sizeof(float));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            9 * sizeof(float),
            reinterpret_cast<void *>(3 * sizeof(float)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            4,
            GL_FLOAT,
            GL_FALSE,
            9 * sizeof(float),
            reinterpret_cast<void *>(5 * sizeof(float)));

        m_vao.release();
    }
}

void LayerQtItemRenderer::paint() {
    if (!m_layer) {
        return;
    }

    m_window->beginExternalCommands();

    m_layer->update();

    if (!m_layer->ready()) {
        m_window->endExternalCommands();
        return;
    }

    calculateViewParameters();

    m_program->bind();

    m_program->enableAttributeArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_layer->textureId());

    m_program->setUniformValue("tex", 1);
    m_program->setUniformValue("flipY", (m_layer->flipY() ? 1 : 0 ));

    glViewport(m_viewOffset.x(), m_viewOffset.y(), m_viewSize.width(), m_viewSize.height());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_vao.bind();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vao.release();

    m_program->disableAttributeArray(0);
    m_program->release();

    glDisable(GL_BLEND);

    m_window->endExternalCommands();

    glViewport(0, 0, m_windowSize.width(), m_windowSize.height());

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}
