/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "slidesqtitem.h"
#include "slidesmodel.h"

#include <QOpenGLContext>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QQuickRenderControl>
#include <QQuickGraphicsDevice>

class CleanupJob : public QRunnable {
public:
    CleanupJob(SlidesQtItemRenderer *renderer) : m_renderer(renderer) {}
    void run() override { delete m_renderer; }

private:
    SlidesQtItemRenderer *m_renderer;
};

void SlidesQtItem::releaseResources() {
    window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

SlidesQtItemRenderer::~SlidesQtItemRenderer() {
    if (m_quickPrivateWindow) {
        delete m_quickPrivateWindow;
        m_quickPrivateWindow = nullptr;
    }
    if (m_renderControl) {
        delete m_renderControl;
        m_renderControl = nullptr;
    }
}

void SlidesQtItemRenderer::initializeRenderer(QQuickWindow* window, SlidesModel* sm) {
    if (!m_window)
        m_window = window;

    if (!m_slidesModel)
        m_slidesModel = sm;

    if (!m_renderControl || !m_quickPrivateWindow) {
        m_renderControl = new QQuickRenderControl(this);

        // Create a QQuickWindow that is associated with out render control. Note that this
        // window never gets created or shown, meaning that it will never get an underlying
        // native (platform) window.
        m_quickPrivateWindow = new QQuickWindow(m_renderControl);

        // Now hook up the signals. For simplicy we don't differentiate between
        // renderRequested (only render is needed, no sync) and sceneChanged (polish and sync
        // is needed too).
        connect(m_window, &QQuickWindow::sceneGraphInitialized, m_quickPrivateWindow, &QQuickWindow::sceneGraphInitialized);
        connect(m_window, &QQuickWindow::sceneGraphInvalidated, m_quickPrivateWindow, &QQuickWindow::sceneGraphInvalidated);
        connect(m_renderControl, &QQuickRenderControl::renderRequested, this, &SlidesQtItemRenderer::update);

        m_quickPrivateWindow->setGraphicsDevice(m_window->graphicsDevice());
        m_renderControl->initialize();
    }
}

void SlidesQtItemRenderer::init() {
    QSGRendererInterface* rif = m_quickPrivateWindow->rendererInterface();
    Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

    initializeOpenGLFunctions();
}

void SlidesQtItemRenderer::update() {
    m_quickPrivateWindow->beginExternalCommands();

    // Update all layers that need update
    // But avoid rendering, as this is a non-visible context window
    if (m_slidesModel) {
        m_slidesModel->runRenderOnLayersThatShouldUpdate(false);
    }

    m_quickPrivateWindow->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}

SlidesQtItem::SlidesQtItem()
    : m_renderer(nullptr) {
    connect(this, &QQuickItem::windowChanged, this, &SlidesQtItem::handleWindowChanged);
}

void SlidesQtItem::handleWindowChanged(QQuickWindow* win) {
    if (win) {
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &SlidesQtItem::cleanup, Qt::DirectConnection);
    }
}

void SlidesQtItem::cleanup() {
    if (m_renderer) {
        delete m_renderer;
        m_renderer = nullptr;
    }
}

void SlidesQtItem::initializeWithControlWindow(QQuickWindow* win, SlidesModel* sm) {
    m_parentWindow = win;
    if (!m_renderer) {
        m_renderer = new SlidesQtItemRenderer();
        m_renderer->initializeRenderer(m_parentWindow, sm);
        connect(m_parentWindow, &QQuickWindow::sceneGraphInvalidated, this, &SlidesQtItem::cleanup, Qt::DirectConnection);
        connect(m_parentWindow, &QQuickWindow::beforeRendering, m_renderer, &SlidesQtItemRenderer::init, Qt::DirectConnection);
        connect(m_parentWindow, &QQuickWindow::beforeRenderPassRecording, m_renderer, &SlidesQtItemRenderer::update, Qt::DirectConnection);
    }
}
