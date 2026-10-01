/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERSRENDERERQTITEM_H
#define LAYERSRENDERERQTITEM_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions_4_5_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QTimer>
#include <QVector3D>
#include <QVector2D>
#include <QMatrix4x4>
#include <QVariantList>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <layers/baselayer.h>
#include <layers/imagelayer.h>
#include <utils/domegrid.h>
#include <utils/spheregrid.h>
#include "mpvobject.h"

class LayersRendererQtOpenGLObject : public QObject, protected QOpenGLFunctions_4_5_Core {
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

    // Projection used when the scene is rendered into the NDI capture target, which has its
    // own aspect ratio (16:9 for the perspective camera, 1:1 for fisheye).
    void setNdiProjectionMatrix(const QMatrix4x4& projectionMatrix);

    // When capture is enabled the scene is rendered once into an offscreen target of the
    // requested size and that target is then blitted onto the screen, so the layers are
    // never drawn twice.
    void setNdiCaptureEnabled(bool enabled);
    void setNdiCaptureSize(int width, int height);

    // Valid only after a frame has been rendered into the capture target.
    unsigned int ndiTextureId() const;
    int ndiWidth() const;
    int ndiHeight() const;

    void setRenderAsFisheye(bool value);

#ifdef CLUX_SUPPORT
    // C-Lux preview overlay (top layer): renders the live light colors as a dome-grid ring
    // with a radial alpha fade from 1.0 at the rim toward 0 at the center.
    void setCluxPreviewVisible(bool visible);
    void setCluxPreviewFrame(const QVariantList& frame);
#endif

    void setMpvObject(MpvObject* mpv);
    void setBackgroundImageFile(const QString& file);
    void setForegroundImageFile(const QString& file);

    void renderLayer(const BaseLayer* layer, int eyeMode, float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);
    void renderMpvObject(MpvObject* mpv, int eyeMode, float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);


    void updateLayers();
    // includeCluxPreview=false keeps the C-Lux preview overlay out of this pass; the NDI capture
    // path uses it so the broadcast never contains the overlay, and re-applies it on top after
    // publishing (see renderFrame).
    void renderLayers(float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, bool includeCluxPreview = true);

#ifdef CLUX_SUPPORT
    // C-Lux preview overlay helpers (render thread).
    void renderCluxPreview(float angle, const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix);
    void ensureCluxDiskTexture(int nLights);
#endif

    void reportSwap();

    Q_INVOKABLE void init();
    Q_INVOKABLE void firstPass();
    Q_INVOKABLE void secondPass();

    void setViewportRect(const QRect& rect);
    void setItemVisible(bool visible);
    void setDivideUpdateAndRender(bool divide);
    void shutdown();

Q_SIGNALS:
    void initialized();

private:
    void createShaders();
    void renderQuad();

    // Region of the framebuffer actually rendered into. In fisheye mode this is the largest
    // centered square inside the item rect, so the fulldome image keeps a 1:1 aspect ratio.
    QRect renderViewportRect() const;

    // Shared body of firstPass/secondPass: renders the layers either straight to the
    // framebuffer Qt has bound, or into the NDI capture target followed by a blit.
    void renderFrame();
    // Creates/resizes the capture target to the requested size. Returns false when there is
    // nothing usable to render into.
    bool ensureNdiTarget();
    void releaseNdiTarget();
    // Scales the capture target into the largest centered rect inside the item rect that
    // preserves the capture aspect ratio.
    void blitNdiTargetToScreen(GLuint targetFramebuffer);

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
    std::vector<std::shared_ptr<BaseLayer>> m_layers;
    QString m_backgroundImageFile;
    QString m_foregroundImageFile;
    bool m_backgroundImageDirty = false;
    bool m_foregroundImageDirty = false;
    MpvObject* m_mpvObject = nullptr;

    // Shader programs (Qt equivalents)
    std::unique_ptr<QOpenGLShaderProgram> m_videoPrg;
    std::unique_ptr<QOpenGLShaderProgram> m_meshPrg;
    std::unique_ptr<QOpenGLShaderProgram> m_EACPrg;
    std::unique_ptr<QOpenGLShaderProgram> m_fisheyePrg;
    std::unique_ptr<QOpenGLShaderProgram> m_fisheyeEACPrg;

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
    int m_EACFlipYLoc;
    int m_EACMatrixLoc;
    int m_EACScaleLoc;
    int m_EACOutsideLoc;
    int m_EACVideoWidthLoc;
    int m_EACVideoHeightLoc;
    int m_EACFlipUpDownLoc;
    int m_EACEyeModeLoc;
    int m_EACStereoscopicModeLoc;

    // Shader uniform locations - fisheye
    int m_fisheyeAlphaLoc;
    int m_fisheyeEyeModeLoc;
    int m_fisheyeFlipYLoc;
    int m_fisheyeStereoscopicModeLoc;
    int m_fisheyeMatrixLoc;
    int m_fisheyeOutsideLoc;
    int m_fisheyeHalfFovLoc;
    int m_fisheyeRoi;

    // Shader uniform locations - fisheye EAC
    int m_fisheyeEACAlphaLoc;
    int m_fisheyeEACEyeModeLoc;
    int m_fisheyeEACFlipYLoc;
    int m_fisheyeEACStereoscopicModeLoc;
    int m_fisheyeEACMatrixLoc;
    int m_fisheyeEACOutsideLoc;
    int m_fisheyeEACHalfFovLoc;
    int m_fisheyeEACScaleLoc;
    int m_fisheyeEACVideoWidthLoc;
    int m_fisheyeEACVideoHeightLoc;
    int m_fisheyeEACFlipUpDownLoc;

    // Meshes
    std::unique_ptr<DomeGrid> m_domeMesh;
    std::unique_ptr<DomeGrid> m_domeMaskMesh;
    std::unique_ptr<SphereGrid> m_sphereMesh;
    bool m_meshesDirty = false;
    unsigned int m_maskTexture = 0;

    // Quad rendering
    QOpenGLVertexArrayObject m_quadVAO;
    QOpenGLBuffer m_quadVBO;

    QRect m_viewportRect;
    bool m_itemVisible = false;
    bool m_divideUpdateAndRender = false;
    bool m_shuttingDown = false;
    bool m_renderAsFisheye = false;

#ifdef CLUX_SUPPORT
    // C-Lux preview overlay (render thread). The live light colors are baked into a dome-grid
    // disk texture on the CPU: each pixel stores the color of the light at its azimuth plus a
    // radial alpha (1.0 at the rim fading to 0 at the center), so it can be drawn with the
    // existing dome/fisheye programs without any new shaders.
    bool m_cluxPreviewVisible = false;
    int m_cluxNLights = 0;
    std::vector<unsigned char> m_cluxFrameBytes;   // nLights * 3, latest RGB values (0..255)
    bool m_cluxColorsDirty = false;
    unsigned int m_cluxDiskTexture = 0;            // N x N RGBA8 disk texture
    std::vector<int> m_cluxPixelLight;             // per-pixel light index (-1 outside the disk)
    std::vector<unsigned char> m_cluxPixelAlpha;   // per-pixel radial alpha (0..255)
    int m_cluxPixelMapNLights = 0;                 // nLights the pixel map was built for
    std::vector<unsigned char> m_cluxTexData;      // staging buffer (N*N*4 bytes)
#endif

    // NDI capture target. The requested state is written from the GUI thread and read on
    // the render thread, the allocated state is only touched on the render thread.
    QMatrix4x4 m_ndiProjectionMatrix;
    std::atomic_bool m_ndiCaptureEnabled = false;
    std::atomic_int m_ndiRequestedWidth = 0;
    std::atomic_int m_ndiRequestedHeight = 0;
    GLuint m_ndiFbo = 0;
    GLuint m_ndiTexture = 0;
    int m_ndiWidth = 0;
    int m_ndiHeight = 0;
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
    Q_PROPERTY(bool renderAsFisheye MEMBER m_renderAsFisheye READ renderAsFisheye WRITE setRenderAsFisheye NOTIFY renderAsFisheyeChanged)
    Q_PROPERTY(MpvObject* mpvObject READ mpvObject WRITE setMpvObject NOTIFY mpvObjectChanged)
    Q_PROPERTY(QString backgroundImageFile READ backgroundImageFile WRITE setBackgroundImageFile NOTIFY backgroundImageFileChanged)
    Q_PROPERTY(QString foregroundImageFile READ foregroundImageFile WRITE setForegroundImageFile NOTIFY foregroundImageFileChanged)
    Q_PROPERTY(bool uiPopupOpen READ isUiPopupOpen WRITE setUiPopupOpen NOTIFY uiPopupOpenChanged)
    Q_PROPERTY(int selectedPlaneLayerIndex READ selectedPlaneLayerIndex NOTIFY planeSelectionChanged)

#ifdef CLUX_SUPPORT
    // C-Lux preview overlay toggle + live frame (driven from the CLux UI / client).
    Q_PROPERTY(bool cluxPreviewVisible MEMBER m_cluxPreviewVisible READ cluxPreviewVisible WRITE setCluxPreviewVisible NOTIFY cluxPreviewVisibleChanged)
    Q_PROPERTY(QVariantList cluxPreviewFrame WRITE setCluxPreviewFrame)
#endif

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

    bool renderAsFisheye() const;
    void setRenderAsFisheye(bool value);

#ifdef CLUX_SUPPORT
    bool cluxPreviewVisible() const;
    void setCluxPreviewVisible(bool visible);
    void setCluxPreviewFrame(const QVariantList& frame);
#endif

    // NDI output of the 3D view. The size follows the configured resolution tier and the
    // camera mode (16:9 for perspective, 1:1 for fisheye).
    void setNdiCaptureEnabled(bool enabled);
    bool isNdiCaptureEnabled() const;
    unsigned int ndiTextureId() const;
    int ndiWidth() const;
    int ndiHeight() const;

    MpvObject* mpvObject() const;
    void setMpvObject(MpvObject* mpv);

    QString backgroundImageFile() const;
    void setBackgroundImageFile(const QString& file);

    QString foregroundImageFile() const;
    void setForegroundImageFile(const QString& file);

    bool isUiPopupOpen() const;
    void setUiPopupOpen(bool open);

    int selectedPlaneLayerIndex() const;

    // Moving of 3D-grid layers in the view. The selected layer comes from the Layers panel
    // (setPlaneSelectionByIndex); no hit testing is done in the 3D view.
    // Ctrl/Alt/Shift+drag all manipulate the selected layer with an operation chosen per
    // modifier combo in the Presentation settings: flat layers (GridMode::Plane) support
    // aiming, horizontal/vertical moving, resizing and distance moving; spheres rotate with
    // the pointer delta since press (X -> yaw, Y -> pitch), domes in yaw only (X).
    // Coordinates are logical pixels in this item's space, origin top-left, y pointing down.
    Q_INVOKABLE void setPlaneSelectionByIndex(int index);    // sync selection from the Layers panel; -1 clears (2D rows clear too)
    Q_INVOKABLE bool beginLayerDrag(int action, float x, float y);   // start a modifier+drag layer operation (see kDrag* values in the .cpp); true if it can run from this point
    Q_INVOKABLE bool dragPlaneTo(float x, float y);          // continue an active layer drag; returns true when parameters changed
    Q_INVOKABLE void endPlaneDrag();                         // finish an active layer drag

    Q_INVOKABLE void sync();
    Q_INVOKABLE void cleanup();
    static void beginShutdown();
    static bool isShuttingDown();
    static std::mutex& layerAccessMutex();

Q_SIGNALS:
    void layerChanged();
    void cameraChanged();
    void meshRadiusChanged();
    void meshFovChanged();
    void meshAngleChanged();
    void renderAsFisheyeChanged();
#ifdef CLUX_SUPPORT
    void cluxPreviewVisibleChanged();
#endif
    void mpvObjectChanged();
    void backgroundImageFileChanged();
    void foregroundImageFileChanged();
    void uiPopupOpenChanged();
    void planeSelectionChanged();

private:
    Q_INVOKABLE void handleWindowChanged(QQuickWindow* win);
    void releaseResources() override;

    void updateCameraMatrices();

    // Recomputes the NDI target size from the resolution setting and the camera mode, and
    // pushes it (together with the matching projection) to the renderer.
    void updateNdiTarget();

    // Dragging helpers (GUI thread, pure CPU math on the live camera state).
    bool rayFromScreenPoint(float x, float y, QVector3D& origin, QVector3D& direction) const;
    bool aimAtScreenPointLocked(const BaseLayer* layer, float x, float y, double& azimuthDeg, double& elevationDeg) const;
    void setSelectedPlaneLayer(std::shared_ptr<BaseLayer> layer, int index);
    // True when the selected layer still exists in the current slide; clears a stale selection
    // (slide switched or layer removed) so the next drag starts clean. Caller holds the lock.
    bool selectedLayerStillValidLocked();
    // Pointer sensitivity for flat-layer drags in cm per pixel, derived from the camera FOV and
    // the plane's distance from it, so that resizing/distance drags track the pointer on screen.
    double planeMoveCmPerPixelLocked() const;
    // Screen-space projection (pixels per cm) of the selected plane's own horizontal and vertical
    // axes, so horizontal/vertical move drags follow the pointer exactly regardless of the plane's
    // orientation (azimuth/elevation/roll) or the camera pose. Each pair gives the pointer delta
    // (dx right, dy down) that a unit change of the parameter produces on screen. Caller holds the lock.
    void planeMoveAxesLocked(double& rightPerCmH, double& downPerCmH,
                             double& rightPerCmV, double& downPerCmV) const;

    // 3D-grid layer selection and drag state (GUI thread only).
    std::shared_ptr<BaseLayer> m_selectedPlaneLayer;
    int m_selectedPlaneIndex = -1;
    bool m_planeDragActive = false;
    // Which layer-drag operation is active while m_planeDragActive (see the kDrag* values in
    // the .cpp): decides which grid parameters dragPlaneTo() updates. -1 when no drag.
    int m_planeDragAction = -1;
    double m_grabAzimuthOffsetDeg = 0.0;
    double m_grabElevationOffsetDeg = 0.0;
    // Sphere/dome drag state: rotation is mapped from the pointer delta since press.
    float m_dragStartX = 0.0f, m_dragStartY = 0.0f;
    double m_dragStartPitchDeg = 0.0;   // rotate().x at press time
    double m_dragStartYawDeg = 0.0;     // rotate().y at press time
    // Flat-layer drag state for the horizontal/vertical move modes: plane offsets (cm) at
    // press time and the pointer sensitivity derived from the camera FOV and layer distance,
    // so that dragging moves the layer with the pointer on screen.
    double m_planeDragStartHorizontalCm = 0.0;
    double m_planeDragStartVerticalCm = 0.0;
    double m_planeMoveCmPerPixel = 1.0;
    // Screen-space projection of the plane's own axes captured at press time (pixels per cm),
    // used by the horizontal/vertical move modes so the layer tracks the pointer for any plane
    // orientation: (dx right, dy down) produced by a +1 cm change of plane horizontal/vertical.
    double m_planeDragRightPerCmH = 0.0;
    double m_planeDragDownPerCmH = 0.0;
    double m_planeDragRightPerCmV = 0.0;
    double m_planeDragDownPerCmV = 0.0;
    // Flat-layer size/distance drag baselines captured at press time (cm): plane width/height
    // for the resize action and plane distance for the move-distance action.
    double m_planeDragStartWidthCm = 0.0;
    double m_planeDragStartHeightCm = 0.0;
    double m_planeDragStartDistanceCm = 0.0;

    LayersRendererQtOpenGLObject* m_renderer;
    QTimer* m_timer;

    float m_fieldOfView;
    QVector3D m_cameraPosition;
    QVector3D m_cameraEulerRotation;

    double m_meshRadius;
    double m_meshFov;
    double m_meshAngle;

    bool m_renderAsFisheye = false;

#ifdef CLUX_SUPPORT
    // C-Lux preview overlay (GUI thread), pushed to the renderer in sync().
    bool m_cluxPreviewVisible = false;
    QVariantList m_cluxPreviewFrame;
#endif

    bool m_ndiCaptureEnabled = false;
    int m_ndiWidth = 0;
    int m_ndiHeight = 0;

    MpvObject* m_mpvObject = nullptr;
    QString m_backgroundImageFile;
    QString m_foregroundImageFile;
    bool m_uiPopupOpen = false;

    static std::atomic_bool s_shuttingDown;
    static std::mutex s_layerAccessMutex;
};

#endif // LAYERSRENDERERQTITEM_H