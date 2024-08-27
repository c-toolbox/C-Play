/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layerqtitem.h"
#include "application.h"

#include <QtQuick/qquickwindow.h>
#include <QOpenGLContext>
#include <QtCore/QRunnable>
#include <QTimer>
#include <array>

LayerQtItem::LayerQtItem()
    : m_layerIdx(-1)
    , m_layer(nullptr)
    , m_renderer(nullptr)
{
    connect(this, &QQuickItem::windowChanged, this, &LayerQtItem::handleWindowChanged);
}

int LayerQtItem::layerIdx()
{
    return m_layerIdx;
}

void LayerQtItem::setLayerIdx(int idx)
{
    m_layerIdx = idx;
    m_layer = Application::instance().layersModel()->layer(m_layerIdx);
    Q_EMIT layerChanged();
    if (window()) {
        window()->update();
    }
}

QString LayerQtItem::layerTitle()
{
    if (m_layer)
        return QString::fromStdString(m_layer->title());
    else
        return QStringLiteral("");
}

void LayerQtItem::handleWindowChanged(QQuickWindow *win)
{
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

void LayerQtItem::cleanup()
{
    delete m_renderer;
    m_renderer = nullptr;
}

class CleanupJob : public QRunnable
{
public:
    CleanupJob(LayerQtItemRenderer *renderer) : m_renderer(renderer) { }
    void run() override { delete m_renderer; }
private:
    LayerQtItemRenderer *m_renderer;
};

void LayerQtItem::releaseResources()
{
    window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

LayerQtItemRenderer::~LayerQtItemRenderer()
{
    delete m_program;
}

void LayerQtItemRenderer::setViewportSize(const QSize& size)
{ 
    m_viewportSize = size; 
}

void LayerQtItemRenderer::setWindow(QQuickWindow* window) { 
    m_window = window; 
}

BaseLayer* LayerQtItemRenderer::layer()
{
    return m_layer;
}

void LayerQtItemRenderer::setLayer(BaseLayer* l)
{
    m_layer = l;
}

void LayerQtItem::sync()
{
    if (!m_renderer) {
        m_renderer = new LayerQtItemRenderer();
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &LayerQtItemRenderer::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &LayerQtItemRenderer::paint, Qt::DirectConnection);
    }
    m_renderer->setLayer(m_layer);
    m_renderer->setViewportSize(window()->size() * window()->devicePixelRatio());
    m_renderer->setWindow(window());
}

void LayerQtItemRenderer::init()
{
    if (!m_program) {
        QSGRendererInterface *rif = m_window->rendererInterface();
        Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

        initializeOpenGLFunctions();

        m_program = new QOpenGLShaderProgram();

        m_program->addCacheableShaderFromSourceCode(QOpenGLShader::Vertex,
            "attribute highp vec3 vertices;"
            "attribute highp vec2 texcoords;"
            "varying highp vec2 coords;"
            "void main() {"
            "    gl_Position = vec4(vertices, 1.0);"
            "    coords = texcoords;"
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
                -1.f, -1.f, -1.f,   0.f, 0.f,   1.f, 1.f, 1.f, 1.f,
                 1.f, -1.f, -1.f,   1.f, 0.f,   1.f, 1.f, 1.f, 1.f,
                -1.f,  1.f, -1.f,   0.f, 1.f,   1.f, 1.f, 1.f, 1.f,
                 1.f,  1.f, -1.f,   1.f, 1.f,   1.f, 1.f, 1.f, 1.f
        };

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
            reinterpret_cast<void*>(3 * sizeof(float))
        );

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,
            4,
            GL_FLOAT,
            GL_FALSE,
            9 * sizeof(float),
            reinterpret_cast<void*>(5 * sizeof(float))
        );

        m_vao.release();
    }
}

void LayerQtItemRenderer::paint()
{
    if (!m_layer) {
        return;
    }

    m_window->beginExternalCommands();

    m_layer->update();

    if (!m_layer->ready()) {
        m_window->endExternalCommands();
        return;
    }

    m_program->bind();

    m_program->enableAttributeArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_layer->textureId());

    m_program->setUniformValue("tex", 1);

    int viewW = m_viewportSize.width();
    int viewH = m_viewportSize.height();
    int offsetX = 0;
    int offsetY = 0;
    float ratioWindow = static_cast<float>(viewW) / static_cast<float>(viewH);
    float ratioLayer = static_cast<float>(m_layer->width()) / static_cast<float>(m_layer->height());
    if (ratioLayer > ratioWindow) {
        //Use full width of window
        if (ratioLayer > 1.f) {
            viewH = static_cast<int>(static_cast<float>(m_viewportSize.width()) / ratioLayer);
        }
        else {
            viewH = static_cast<int>(static_cast<float>(m_viewportSize.width()) * ratioLayer);
        }
        offsetY = static_cast<int>(0.5f * (static_cast<float>(m_viewportSize.height() - viewH)));
    }
    else if(ratioLayer < ratioWindow) {
        //Use full height of window
        if (ratioLayer > 1.f) {
            viewW = static_cast<int>(static_cast<float>(m_viewportSize.height()) * ratioLayer);
        }
        else {
            viewW = static_cast<int>(static_cast<float>(m_viewportSize.height()) / ratioLayer);
        }
        offsetX = static_cast<int>(0.5f * (static_cast<float>(m_viewportSize.width() - viewW)));
    }

    glViewport(offsetX, offsetY, viewW, viewH);

    glDisable(GL_DEPTH_TEST);

    m_vao.bind();

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_vao.release();

    m_program->disableAttributeArray(0);
    m_program->release();

    m_window->endExternalCommands();
}
