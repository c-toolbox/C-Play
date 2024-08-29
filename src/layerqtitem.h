/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERQTITEM_H
#define LAYERQTITEM_H

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <layers/baselayer.h>

class LayerQtItemRenderer : public QObject, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    ~LayerQtItemRenderer();

    void setWindowSize(const QSize& size);
    void setViewportSize(const QSize& size);
    void setPosition(const QPoint& position);
    void setWindow(QQuickWindow* window);

    BaseLayer* layer();
    void setLayer(BaseLayer* l);

    Q_INVOKABLE void init();
    Q_INVOKABLE void paint();

private:
    QSize m_windowSize;
    QSize m_viewportSize;
    QPoint m_position;
    QOpenGLShaderProgram* m_program = nullptr;
    QQuickWindow *m_window = nullptr;
    BaseLayer* m_layer = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
};


class LayerQtItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(int layerIdx READ layerIdx WRITE setLayerIdx NOTIFY layerChanged)
    Q_PROPERTY(int layerStereoMode READ layerStereoMode WRITE setLayerStereoMode NOTIFY layerValueChanged)
    Q_PROPERTY(int layerGridMode READ layerGridMode WRITE setLayerGridMode NOTIFY layerValueChanged)
    Q_PROPERTY(int layerVisibility READ layerVisibility WRITE setLayerVisibility NOTIFY layerValueChanged)
    Q_PROPERTY(QString layerTitle READ layerTitle)
    QML_ELEMENT

public:
    LayerQtItem();

    int layerIdx();
    void setLayerIdx(int idx);

    int layerStereoMode();
    void setLayerStereoMode(int mode);

    int layerGridMode();
    void setLayerGridMode(int mode);

    int layerVisibility();
    void setLayerVisibility(int value);

    QString layerTitle();

    Q_INVOKABLE void sync();
    Q_INVOKABLE void cleanup();

Q_SIGNALS:
    void layerChanged();
    void layerValueChanged();

private:
    Q_INVOKABLE void handleWindowChanged(QQuickWindow* win);
    void releaseResources() override;

    int m_layerIdx;
    BaseLayer* m_layer = nullptr;
    LayerQtItemRenderer *m_renderer = nullptr;
    QTimer* m_timer = nullptr;
};

#endif // LAYERQTITEM_H
