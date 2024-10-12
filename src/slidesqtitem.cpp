/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "slidesqtitem.h"
#include "slidesmodel.h"

#include <QOpenGLContext>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/qquickwindow.h>

SlidesQtItem::SlidesQtItem()
    : m_renderer(nullptr) {
    connect(this, &QQuickItem::windowChanged, this, &SlidesQtItem::handleWindowChanged);
}

void SlidesQtItem::handleWindowChanged(QQuickWindow *win) {
    if (win) {
        connect(win, &QQuickWindow::beforeSynchronizing, this, &SlidesQtItem::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &SlidesQtItem::cleanup, Qt::DirectConnection);
        win->setColor(Qt::black);

        if (m_timer == nullptr) {
            m_timer = new QTimer();
            m_timer->setInterval((1.0f / 60.0f) * 1000.0f);

            connect(m_timer, &QTimer::timeout, win, &QQuickWindow::update);

            m_timer->start();
        }
    }
}

void SlidesQtItem::cleanup() {
    delete m_renderer;
    m_renderer = nullptr;
}

SlidesModel* SlidesQtItem::slidesModel() const {
    if (m_renderer)
        return m_renderer->slidesModel();
    else
        return nullptr;
}

void SlidesQtItem::setSlidesModel(SlidesModel* sm) {
    if (m_renderer)
        m_renderer->setSlidesModel(sm);
}

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
}

void SlidesQtItemRenderer::setWindow(QQuickWindow *window) {
    m_window = window;
}

SlidesModel* SlidesQtItemRenderer::slidesModel() const {
    return m_slidesModel;
}

void SlidesQtItemRenderer::setSlidesModel(SlidesModel* sm) {
    m_slidesModel = sm;
}

void SlidesQtItem::sync() {
    if (!m_renderer) {
        m_renderer = new SlidesQtItemRenderer();
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &SlidesQtItemRenderer::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &SlidesQtItemRenderer::paint, Qt::DirectConnection);
    }
    m_renderer->setWindow(window());
}

void SlidesQtItemRenderer::init() {
    QSGRendererInterface *rif = m_window->rendererInterface();
    Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

    initializeOpenGLFunctions();
}

void SlidesQtItemRenderer::paint() {
    m_window->beginExternalCommands();

    // Update all layers that need update
    // But avoid rendering, as this is a non-visible context window
    if (m_slidesModel) {
        m_slidesModel->runRenderOnLayersThatShouldUpdate(false);
    }

    m_window->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}
