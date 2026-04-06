/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERSRENDERERQTITEM_H
#define LAYERSRENDERERQTITEM_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QTimer>
#include <QVector3D>
#include <QMatrix4x4>
#include <memory>
#include <vector>
#include <layers/baselayer.h>
#include <layers/imagelayer.h>
#include <utils/domegrid.h>
#include <utils/spheregrid.h>
#include "mpvobject.h"

class LayersRendererQtOpenGLObject : public QObject, protected QOpenGLFunctions {
    Q_OBJECT
public:
    LayersRendererQtOpenGLObject(QObject* parent = nullptr);
    ~LayersRendererQtOpenGLObject();

    void setWindow(QQuickWindow* window);

    void initializeGL();
    void updateMeshes(double radius, double fov, double angle);

    void addLayer(std::shared_ptr<BaseLayer> layer);
    void clearLayers();
    const std::vector<std::shared_ptr<BaseLayer>>& getLayers();

    void setCameraParams(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);

    void setMpvObject(MpvObject* mpv);
    void setBackgroundImageFile(const QString& file);
    void setForegroundImageFile(const QString& file);

    void renderLayer(const BaseLayer* layer, int eyeMode, float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);
    void renderMpvObject(MpvObject* mpv, int eyeMode, float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);


    void updateLayers();
    void renderLayers(float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);

    Q_INVOKABLE void init();
    Q_INVOKABLE void paint();

    void setViewportRect(const QRect& rect);
    void setItemVisible(bool visible);

Q_SIGNALS:
    void initialized();

private:
    void createShaders();
    void renderQuad();

    QQuickWindow* m_window = nullptr;
    bool m_initialized = false;

    // Camera matrices (set from QML camera properties)
    QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_projectionMatrix;

    // Mesh parameters
    double m_meshRadius;
    double m_meshFov;
    double m_meshAngle;

    // Primary layers (background + foreground image, mpv video and overlay)
    std::shared_ptr<ImageLayer> m_backgroundImageLayer;
    std::shared_ptr<ImageLayer> m_foregroundImageLayer;
    std::shared_ptr<ImageLayer> m_overlayImageLayer;
    QString m_backgroundImageFile;
    QString m_foregroundImageFile;
    bool m_backgroundImageDirty = false;
    bool m_foregroundImageDirty = false;
    MpvObject* m_mpvObject = nullptr;

    // Shader programs (Qt equivalents)
    std::unique_ptr<QOpenGLShaderProgram> m_videoPrg;
    std::unique_ptr<QOpenGLShaderProgram> m_meshPrg;
    std::unique_ptr<QOpenGLShaderProgram> m_EACPrg;

    // Shader uniform locations - video
    int m_videoAlphaLoc;
    int m_videoEyeModeLoc;
    int m_videoFlipYLoc;
    int m_videoStereoscopicModeLoc;
    int m_videoRoi;

    // Shader uniform locations - mesh
    int m_meshAlphaLoc;
    int m_meshEyeModeLoc;
    int m_meshFlipYLoc;
    int m_meshMatrixLoc;
    int m_meshOutsideLoc;
    int m_meshStereoscopicModeLoc;
    int m_meshRoi;

    // Shader uniform locations - EAC
    int m_EACAlphaLoc;
    int m_EACMatrixLoc;
    int m_EACScaleLoc;
    int m_EACOutsideLoc;
    int m_EACVideoWidthLoc;
    int m_EACVideoHeightLoc;
    int m_EACEyeModeLoc;
    int m_EACStereoscopicModeLoc;

    // Meshes
    std::unique_ptr<DomeGrid> m_domeMesh;
    std::unique_ptr<SphereGrid> m_sphereMesh;

    // Quad rendering
    QOpenGLVertexArrayObject m_quadVAO;
    QOpenGLBuffer m_quadVBO;

    QRect m_viewportRect;
    bool m_itemVisible = false;
};

class LayersRendererQtItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(float fieldOfView READ fieldOfView WRITE setFieldOfView NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D cameraPosition READ cameraPosition WRITE setCameraPosition NOTIFY cameraChanged)
    Q_PROPERTY(QVector3D cameraEulerRotation READ cameraEulerRotation WRITE setCameraEulerRotation NOTIFY cameraChanged)
    Q_PROPERTY(double meshRadius MEMBER m_meshRadius READ meshRadius WRITE setMeshRadius NOTIFY meshRadiusChanged)
    Q_PROPERTY(double meshFov MEMBER m_meshFov READ meshFov WRITE setMeshFov NOTIFY meshFovChanged)
    Q_PROPERTY(double meshAngle MEMBER m_meshAngle READ meshAngle WRITE setMeshAngle NOTIFY meshAngleChanged)
    Q_PROPERTY(MpvObject* mpvObject READ mpvObject WRITE setMpvObject NOTIFY mpvObjectChanged)
    Q_PROPERTY(QString backgroundImageFile READ backgroundImageFile WRITE setBackgroundImageFile NOTIFY backgroundImageFileChanged)
    Q_PROPERTY(QString foregroundImageFile READ foregroundImageFile WRITE setForegroundImageFile NOTIFY foregroundImageFileChanged)

public:
    LayersRendererQtItem();

    float fieldOfView() const;
    void setFieldOfView(float fov);

    QVector3D cameraPosition() const;
    void setCameraPosition(const QVector3D& pos);

    QVector3D cameraEulerRotation() const;
    void setCameraEulerRotation(const QVector3D& rot);

    double meshRadius() const;
    void setMeshRadius(double value);

    double meshFov() const;
    void setMeshFov(double value);

    double meshAngle() const;
    void setMeshAngle(double value);

    MpvObject* mpvObject() const;
    void setMpvObject(MpvObject* mpv);

    QString backgroundImageFile() const;
    void setBackgroundImageFile(const QString& file);

    QString foregroundImageFile() const;
    void setForegroundImageFile(const QString& file);

    Q_INVOKABLE void sync();
    Q_INVOKABLE void cleanup();

Q_SIGNALS:
    void layerChanged();
    void cameraChanged();
    void meshRadiusChanged();
    void meshFovChanged();
    void meshAngleChanged();
    void mpvObjectChanged();
    void backgroundImageFileChanged();
    void foregroundImageFileChanged();

private:
    Q_INVOKABLE void handleWindowChanged(QQuickWindow* win);
    void releaseResources() override;

    void updateCameraMatrices();

    LayersRendererQtOpenGLObject* m_renderer;
    QTimer* m_timer;

    float m_fieldOfView;
    QVector3D m_cameraPosition;
    QVector3D m_cameraEulerRotation;

    double m_meshRadius;
    double m_meshFov;
    double m_meshAngle;

    MpvObject* m_mpvObject = nullptr;
    QString m_backgroundImageFile;
    QString m_foregroundImageFile;
};

#endif // LAYERSRENDERERQTITEM_H