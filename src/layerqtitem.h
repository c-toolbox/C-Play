/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sund�n <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERQTITEM_H
#define LAYERQTITEM_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <layers/baselayer.h>

class LayerQtItemRenderer : public QObject, protected QOpenGLFunctions {
    Q_OBJECT
public:
    ~LayerQtItemRenderer();

    void setWindowSize(const QSize &size);
    void setViewportSize(const QSize &size);
    void setPosition(const QPoint &position);
    void setWindow(QQuickWindow *window);

    BaseLayer *layer();
    void setLayer(BaseLayer *l);

    QPoint viewOffset();
    QSize viewSize();
    void calculateViewParameters();

    Q_INVOKABLE void init();
    Q_INVOKABLE void paint();

Q_SIGNALS:
    void viewChanged();

private:
    QSize m_windowSize;
    QSize m_viewportSize;
    QPoint m_position;
    QPoint m_viewOffset;
    QSize m_viewSize;
    QOpenGLShaderProgram *m_program = nullptr;
    QQuickWindow *m_window = nullptr;
    BaseLayer *m_layer = nullptr;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
};

class LayerQtItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(int layerIdx READ layerIdx WRITE setLayerIdx NOTIFY layerChanged)
    Q_PROPERTY(int layerStereoMode READ layerStereoMode WRITE setLayerStereoMode NOTIFY layerValueChanged)
    Q_PROPERTY(int layerGridMode READ layerGridMode WRITE setLayerGridMode NOTIFY layerValueChanged)
    Q_PROPERTY(int layerVisibility READ layerVisibility WRITE setLayerVisibility NOTIFY layerValueChanged)
    Q_PROPERTY(int layerPage READ layerPage WRITE setLayerPage NOTIFY layerValueChanged)
    Q_PROPERTY(int layerNumPages READ layerNumPages NOTIFY layerValueChanged)
    Q_PROPERTY(double layerRotatePitch READ layerRotatePitch WRITE setLayerRotatePitch NOTIFY layerValueChanged)
    Q_PROPERTY(double layerRotateYaw READ layerRotateYaw WRITE setLayerRotateYaw NOTIFY layerValueChanged)
    Q_PROPERTY(double layerRotateRoll READ layerRotateRoll WRITE setLayerRotateRoll NOTIFY layerValueChanged)
    Q_PROPERTY(double layerPlaneWidth READ layerPlaneWidth WRITE setLayerPlaneWidth NOTIFY layerValueChanged)
    Q_PROPERTY(double layerPlaneHeight READ layerPlaneHeight WRITE setLayerPlaneHeight NOTIFY layerValueChanged)
    Q_PROPERTY(int layerPlaneAspectRatio READ layerPlaneAspectRatio WRITE setLayerPlaneAspectRatio NOTIFY layerValueChanged)
    Q_PROPERTY(double layerPlaneAzimuth READ layerPlaneAzimuth WRITE setLayerPlaneAzimuth NOTIFY layerValueChanged)
    Q_PROPERTY(double layerPlaneElevation READ layerPlaneElevation WRITE setLayerPlaneElevation NOTIFY layerValueChanged)
    Q_PROPERTY(double layerPlaneDistance READ layerPlaneDistance WRITE setLayerPlaneDistance NOTIFY layerValueChanged)
    Q_PROPERTY(double layerPlaneRoll READ layerPlaneRoll WRITE setLayerPlaneRoll NOTIFY layerValueChanged)
    Q_PROPERTY(bool layerRoiEnabled READ layerRoiEnabled WRITE setLayerRoiEnabled NOTIFY layerValueChanged)
    Q_PROPERTY(QString layerTypeName READ layerTypeName NOTIFY layerValueChanged)
    Q_PROPERTY(QString layerTitle READ layerTitle WRITE setLayerTitle NOTIFY layerValueChanged)
    Q_PROPERTY(QSize textureSize READ textureSize NOTIFY viewChanged)
    Q_PROPERTY(QPoint viewOffset READ viewOffset WRITE setViewOffset NOTIFY viewChanged)
    Q_PROPERTY(QSize viewSize READ viewSize WRITE setViewSize NOTIFY viewChanged)
    Q_PROPERTY(QPoint roiOffset READ roiOffset WRITE setRoiOffset NOTIFY roiChanged)
    Q_PROPERTY(QSize roiSize READ roiSize WRITE setRoiSize NOTIFY roiChanged)
    Q_PROPERTY(QPoint roiTexOffset READ roiTexOffset WRITE setRoiTexOffset NOTIFY roiChanged)
    Q_PROPERTY(QSize roiTexSize READ roiTexSize WRITE setRoiTexSize NOTIFY roiChanged)
    QML_ELEMENT

public:
    LayerQtItem();

    int layerIdx() const;
    void setLayerIdx(int idx);

    int layerStereoMode() const;
    void setLayerStereoMode(int mode);

    int layerGridMode() const;
    void setLayerGridMode(int mode);

    int layerVisibility() const;
    void setLayerVisibility(int value);

    int layerPage() const;
    void setLayerPage(int value);
    int layerNumPages() const;

    double layerRotatePitch() const;
    void setLayerRotatePitch(double x);

    double layerRotateYaw() const;
    void setLayerRotateYaw(double y);

    double layerRotateRoll() const;
    void setLayerRotateRoll(double z);

    double layerPlaneWidth() const;
    void setLayerPlaneWidth(double pW);

    double layerPlaneHeight() const;
    void setLayerPlaneHeight(double pH);

    int layerPlaneAspectRatio() const;
    void setLayerPlaneAspectRatio(int parc);

    double layerPlaneAzimuth() const;
    void setLayerPlaneAzimuth(double pA);

    double layerPlaneElevation() const;
    void setLayerPlaneElevation(double pE);

    double layerPlaneDistance() const;
    void setLayerPlaneDistance(double pD);

    double layerPlaneRoll() const;
    void setLayerPlaneRoll(double pR);

    bool layerRoiEnabled();
    void setLayerRoiEnabled(bool value);

    QString layerTypeName();
    QString layerTitle();
    void setLayerTitle(QString value);

    QSize textureSize();

    QPoint viewOffset();
    void setViewOffset(QPoint p);

    QSize viewSize();
    void setViewSize(QSize s);

    QPoint roiOffset();
    void setRoiOffset(QPoint p);

    QSize roiSize();
    void setRoiSize(QSize s);

    Q_INVOKABLE void setRoi(QPoint offset, QSize size);
    Q_INVOKABLE void dragLeft(double Xdiff);
    Q_INVOKABLE void dragRight(double Xdiff);
    Q_INVOKABLE void dragTop(double Ydiff);
    Q_INVOKABLE void dragBottom(double Ydiff);

    QPoint roiTexOffset();
    void setRoiTexOffset(QPoint p);

    QSize roiTexSize();
    void setRoiTexSize(QSize s);

    Q_INVOKABLE void sync();
    Q_INVOKABLE void cleanup();
    Q_INVOKABLE void updateView();
    Q_INVOKABLE void updateRoi();

Q_SIGNALS:
    void layerChanged();
    void layerValueChanged();
    void viewChanged();
    void roiChanged();

private:
    Q_INVOKABLE void handleWindowChanged(QQuickWindow *win);
    void releaseResources() override;

    int m_layerIdx;
    BaseLayer *m_layer = nullptr;
    LayerQtItemRenderer *m_renderer = nullptr;
    QTimer *m_timer = nullptr;

    QPoint m_viewOffset;
    QSize m_viewSize;

    QPoint m_roiOffset;
    QSize m_roiSize;
};

#endif // LAYERQTITEM_H
