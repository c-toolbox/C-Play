/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SLIDESQTITEM_H
#define SLIDESQTITEM_H

#include <QOpenGLFunctions>
#include <QtQuick/QQuickItem>
#include <QTimer>

class QQuickRenderControl;
class QQuickWindow;
class SlidesModel;

class SlidesQtItemRenderer : public QObject, protected QOpenGLFunctions {
    Q_OBJECT
public:
    ~SlidesQtItemRenderer();

    void initializeRenderer(QQuickWindow* window, SlidesModel* sm);

    Q_INVOKABLE void init();
    Q_INVOKABLE void update();

private:

    SlidesModel* m_slidesModel = nullptr;
    QQuickWindow *m_window = nullptr;
    QQuickRenderControl* m_renderControl = nullptr;
    QQuickWindow* m_quickPrivateWindow = nullptr;
};

class SlidesQtItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

public:
    SlidesQtItem();

    Q_INVOKABLE void initializeWithControlWindow(QQuickWindow *win, SlidesModel* slides);

    Q_INVOKABLE void cleanup();

Q_SIGNALS:
    void slidesModelChanged();

private:
    Q_INVOKABLE void handleWindowChanged(QQuickWindow *win);
    void releaseResources() override;

    QQuickWindow* m_parentWindow = nullptr;
    SlidesQtItemRenderer *m_renderer = nullptr;
};

#endif // SLIDESQTITEM_H
