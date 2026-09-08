/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layersrendererqtitem.h"
#include "application.h"
#include "layersmodel.h"
#include "slidesmodel.h"
#include "gridsettings.h"
#include "mpvobject.h"
#include "userinterfacesettings.h"
#include <QOpenGLContext>
#include <QQuickGraphicsDevice>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/qquickwindow.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <cmath>

 // Shader sources (same as LayersRenderer but compatible with QOpenGLShaderProgram)
constexpr const char* VideoVert = R"(
#version 460 core

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec2 in_texCoord;

uniform int eye;
uniform int stereoscopicMode;
uniform vec4 roi;
uniform bool flipY;

out vec2 tr_uv;

void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    tr_uv = flipY ? vec2(in_texCoord.x, 1.0-in_texCoord.y) : in_texCoord;
    tr_uv = (tr_uv * roi.zw) + roi.xy;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (tr_uv * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = tr_uv * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = tr_uv * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = tr_uv * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
}
)";

constexpr const char* MeshVert = R"(
#version 460 core

layout (location = 0) in vec2 in_texCoord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_position;

uniform mat4 mvp;
uniform int eye;
uniform int stereoscopicMode;
uniform vec4 roi;
uniform bool flipY;

out vec2 tr_uv;
out vec3 tr_normals;

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_uv = flipY ? vec2(in_texCoord.x, 1.0-in_texCoord.y) : in_texCoord;
    tr_uv = (tr_uv * roi.zw) + roi.xy;
    tr_normals = in_normal;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (tr_uv * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = tr_uv * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = tr_uv * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { // Left Eye or Mono
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = tr_uv * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
}
)";

constexpr const char* VideoFrag = R"(
#version 460 core

uniform sampler2D tex;
uniform float alpha;
uniform bool outside;

in vec2 tr_uv;
in vec3 tr_normals;
out vec4 out_color;

void main() {
    vec2 texCoods = tr_uv;
    if(outside){
        texCoods = vec2(1.0-tr_uv.x, tr_uv.y);
    }
   
    out_color = texture(tex, texCoods) * vec4(1.0, 1.0, 1.0, alpha);
}
)";

constexpr const char* EACMeshVert = R"(
#version 460 core

layout (location = 0) in vec2 in_texCoord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_position;

uniform mat4 mvp;

uniform float scaleToUnitCube;
uniform bool outside;

out vec3 tr_position;
out vec3 tr_normal;

void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_position = in_position * scaleToUnitCube;

    if(outside)
        tr_normal = -in_normal;
    else
        tr_normal = in_normal;
}
)";

constexpr const char* EACVideoFrag = R"(
#version 460 core

uniform sampler2D tex;
uniform int eye;
uniform int stereoscopicMode;
uniform float alpha;
uniform int videoWidth;
uniform int videoHeight;
uniform bool flipUpDown;
uniform bool flipY;

in vec3 tr_position;
in vec3 tr_normal;
out vec4 out_color;

const float M_PI_2 = 1.57079632679489661923;   // pi/2
const float M_PI_4 = 0.785398163397448309616;  // pi/4
const float M_1_PI = 0.318309886183790671538;  // 1/pi
const float M_2_PI = 0.636619772367581343076;  // 2/pi
const float M_PI = 3.14159265358979323846264338327950288;

const int TOP_LEFT = 0;
const int TOP_MIDDLE = 1;
const int TOP_RIGHT = 2;
const int BOTTOM_LEFT = 3;
const int BOTTOM_MIDDLE = 4;
const int BOTTOM_RIGHT = 5;

const int RIGHT = 0; ///< Axis +X
const int LEFT = 1; ///< Axis -X
const int UP = 2; ///< Axis +Y
const int DOWN = 3; ///< Axis -Y
const int FRONT = 4; ///< Axis -Z
const int BACK = 5; ///< Axis +Z

const int ROT_0 = 0;
const int ROT_90 = 1;
const int ROT_180 = 2;
const int ROT_270 = 3;

vec2 rotate_cube_face(vec2 uv_in, int rotation)
{
    vec2 uv_out;

    switch (rotation) {
        case ROT_0:
            uv_out = uv_in;
            break;
        case ROT_90:
            uv_out.x = -uv_in.y;
            uv_out.y =  uv_in.x;
            break;
        case ROT_180:
            uv_out.x = -uv_in.x;
            uv_out.y = -uv_in.y;
            break;
        case ROT_270:
            uv_out.x = uv_in.y;
            uv_out.y = -uv_in.x;
            break;
    }

    return uv_out;
}

int xyz_to_direction(vec3 xyz)
{
    int direction;
    float phi = atan(xyz.x, xyz.z);
    float theta = asin(xyz.y);
    float phi_norm, theta_threshold;
    int face;

    if (phi >= -M_PI_4 && phi < M_PI_4) {
        direction = FRONT;
        phi_norm = phi;
    } else if (phi >= -(M_PI_2 + M_PI_4) && phi < -M_PI_4) {
        direction = LEFT;
        phi_norm = phi + M_PI_2;
    } else if (phi >= M_PI_4 && phi < M_PI_2 + M_PI_4) {
        direction = RIGHT;
        phi_norm = phi - M_PI_2;
    } else {
        direction = BACK;
        phi_norm = phi + ((phi > 0.f) ? -M_PI : M_PI);
    }

    theta_threshold = atan(cos(phi_norm));
    if (theta > theta_threshold) {
        direction = DOWN;
    } else if (theta < -theta_threshold) {
        direction = UP;
    }

    return direction;
}

vec2 xyz_to_eac(vec3 xyz, int width, int height, bool flip)
{
    float pixel_pad = 2;
    float u_pad = pixel_pad / width;
    float v_pad = pixel_pad / height;

    int in_cubemap_face_order[6];
    int in_cubemap_face_rotation[6];

    in_cubemap_face_order[RIGHT] = TOP_LEFT;
    in_cubemap_face_order[LEFT]  = TOP_RIGHT;
    in_cubemap_face_order[UP]    = BOTTOM_LEFT;
    in_cubemap_face_order[DOWN]  = BOTTOM_RIGHT;
    in_cubemap_face_order[FRONT] = TOP_MIDDLE;
    in_cubemap_face_order[BACK]  = BOTTOM_MIDDLE;

    in_cubemap_face_rotation[TOP_LEFT]      = ROT_180;
    in_cubemap_face_rotation[TOP_MIDDLE]    = ROT_180;
    in_cubemap_face_rotation[TOP_RIGHT]     = ROT_180;
    in_cubemap_face_rotation[BOTTOM_LEFT]   = ROT_90;
    in_cubemap_face_rotation[BOTTOM_MIDDLE] = ROT_270;
    in_cubemap_face_rotation[BOTTOM_RIGHT]  = ROT_90;

    if(flip) {
        in_cubemap_face_order[RIGHT] = TOP_RIGHT;
        in_cubemap_face_order[LEFT]  = TOP_LEFT;
        in_cubemap_face_order[UP] = BOTTOM_LEFT;
        in_cubemap_face_order[DOWN] = BOTTOM_RIGHT;
        in_cubemap_face_order[FRONT] = TOP_MIDDLE;
        in_cubemap_face_order[BACK]  = BOTTOM_MIDDLE;

        in_cubemap_face_rotation[TOP_LEFT]      = ROT_0;
        in_cubemap_face_rotation[TOP_MIDDLE]    = ROT_0;
        in_cubemap_face_rotation[TOP_RIGHT]     = ROT_0;
        in_cubemap_face_rotation[BOTTOM_LEFT]   = ROT_90;
        in_cubemap_face_rotation[BOTTOM_MIDDLE] = ROT_270;
        in_cubemap_face_rotation[BOTTOM_RIGHT]  = ROT_90;
    }

    int direction = xyz_to_direction(xyz);

    vec2 uv = vec2(0.0, 0.0);
    switch (direction) {
        case LEFT:
            uv.x = -xyz.z / xyz.x;
            uv.y =  xyz.y / xyz.x;
            break;
        case RIGHT:
            uv.x = -xyz.z  / xyz.x;
            uv.y = -xyz.y / xyz.x;
            break;
        case DOWN:
            uv.x = -xyz.x / xyz.y;
            uv.y = -xyz.z  / xyz.y;
            break;
        case UP:
            uv.x =  xyz.x / xyz.y;
            uv.y = -xyz.z  / xyz.y;
            break;
        case BACK:
            uv.x =  -xyz.x / xyz.z;
            uv.y =  -xyz.y / xyz.z;
            break;
        case FRONT:
            uv.x =  xyz.x / xyz.z;
            uv.y = -xyz.y / xyz.z;
            break;
    }

    int face = in_cubemap_face_order[direction];
    uv = rotate_cube_face(uv, in_cubemap_face_rotation[face]);

    int u_face = face % 3;
    int v_face = face / 3;

    uv = M_2_PI * atan(uv) + 0.5;

    uv.x = (uv.x + u_face) * (1.0 - 2.0 * u_pad) / 3.0 + u_pad;
    uv.y = uv.y * (0.5 - (2.0 * v_pad)) + v_pad + (0.5 * v_face);

    return uv;
}

void main() {
    vec2 uv = xyz_to_eac(normalize(tr_normal), videoWidth, videoHeight, flipUpDown);

    if(flipY) {
        uv.y = 1.0 - uv.y;
    }

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            uv = (uv * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            uv = uv * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            uv = uv * vec2(1.0, 0.5);
            uv = vec2(1.0 - uv.y, uv.x);
        }
    }
    else { // Left Eye or Mono
        if(stereoscopicMode==1) { //Side-by-side
            uv = uv * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            uv = (uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            uv = (uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            uv = vec2(1.0 - uv.y, uv.x);
        }
    }
   
    out_color = texture(tex, uv) * vec4(1.0, 1.0, 1.0, alpha);
}
)";

// -------------------------------------------------------------------------
// LayersRendererQtItem
// -------------------------------------------------------------------------

std::atomic_bool LayersRendererQtItem::s_shuttingDown = false;
std::mutex LayersRendererQtItem::s_layerAccessMutex;

LayersRendererQtItem::LayersRendererQtItem()
    : m_renderer(nullptr), 
    m_timer(nullptr), 
    m_fieldOfView(90.0f),
    m_cameraPosition(0.0f, 0.0f, 0.0f),
    m_cameraEulerRotation(0.0f, 0.0f, 0.0f) {

    m_meshRadius = GridSettings::surfaceRadius();
    m_meshAngle = GridSettings::surfaceAngle();

    connect(this, &QQuickItem::windowChanged, this, &LayersRendererQtItem::handleWindowChanged);
}

float LayersRendererQtItem::fieldOfView() const {
    return m_fieldOfView;
}

void LayersRendererQtItem::setFieldOfView(float fov) {
    if (qFuzzyCompare(m_fieldOfView, fov))
        return;
    m_fieldOfView = fov;
    Q_EMIT cameraChanged();
}

QVector3D LayersRendererQtItem::cameraPosition() const {
    return m_cameraPosition;
}

void LayersRendererQtItem::setCameraPosition(const QVector3D& pos) {
    if (m_cameraPosition == pos)
        return;
    m_cameraPosition = pos;
    Q_EMIT cameraChanged();
}

QVector3D LayersRendererQtItem::cameraEulerRotation() const {
    return m_cameraEulerRotation;
}

void LayersRendererQtItem::setCameraEulerRotation(const QVector3D& rot) {
    if (m_cameraEulerRotation == rot)
        return;
    m_cameraEulerRotation = rot;
    Q_EMIT cameraChanged();
}

double LayersRendererQtItem::meshRadius() const {
    return m_meshRadius;
}

void LayersRendererQtItem::setMeshRadius(double value) {
    if (qFuzzyCompare(m_meshRadius, value))
        return;
    m_meshRadius = value;
    Q_EMIT meshRadiusChanged();
}

double LayersRendererQtItem::meshFov() const {
    return m_meshFov;
}

void LayersRendererQtItem::setMeshFov(double value) {
    if (qFuzzyCompare(m_meshFov, value))
        return;
    m_meshFov = value;
    Q_EMIT meshFovChanged();
}

double LayersRendererQtItem::meshAngle() const {
    return m_meshAngle;
}

void LayersRendererQtItem::setMeshAngle(double value) {
    if (qFuzzyCompare(m_meshAngle, value))
        return;
    m_meshAngle = value;
    Q_EMIT meshAngleChanged();
}

MpvObject* LayersRendererQtItem::mpvObject() const {
    return m_mpvObject;
}

void LayersRendererQtItem::setMpvObject(MpvObject* mpv) {
    if (m_mpvObject == mpv)
        return;
    m_mpvObject = mpv;
    Q_EMIT mpvObjectChanged();
}

QString LayersRendererQtItem::backgroundImageFile() const {
    return m_backgroundImageFile;
}

void LayersRendererQtItem::setBackgroundImageFile(const QString& file) {
    if (m_backgroundImageFile == file)
        return;
    m_backgroundImageFile = file;
    Q_EMIT backgroundImageFileChanged();
}

QString LayersRendererQtItem::foregroundImageFile() const {
    return m_foregroundImageFile;
}

void LayersRendererQtItem::setForegroundImageFile(const QString& file) {
    if (m_foregroundImageFile == file)
        return;
    m_foregroundImageFile = file;
    Q_EMIT foregroundImageFileChanged();
}

bool LayersRendererQtItem::isUiPopupOpen() const {
    return m_uiPopupOpen;
}

void LayersRendererQtItem::setUiPopupOpen(bool open) {
    if (m_uiPopupOpen == open)
        return;
    m_uiPopupOpen = open;
    Q_EMIT uiPopupOpenChanged();
}

namespace {
// Build the camera's view/projection matrices from raw camera state. Shared by the render
// path (updateCameraMatrices) and CPU-side picking (rayFromScreenPoint) so both always use
// exactly the same transform, even before the first rendered frame has run.
void buildCameraMatrices(const QVector3D& position, const QVector3D& eulerRotation, float fieldOfViewDeg,
                         float widthPx, float heightPx, QMatrix4x4& viewMatrix, QMatrix4x4& projectionMatrix) {
    // Use the item's own dimensions, not the full window, for correct aspect ratio.
    const float aspectRatio = (heightPx > 0.0f) ? widthPx / heightPx : 1.0f;

    QMatrix4x4 rotMatrix;
    rotMatrix.rotate(eulerRotation.y(), 0.0f, 1.0f, 0.0f);
    rotMatrix.rotate(eulerRotation.x(), 1.0f, 0.0f, 0.0f);
    rotMatrix.rotate(eulerRotation.z(), 0.0f, 0.0f, 1.0f);

    const QVector3D forward = rotMatrix.map(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
    const QVector3D up = rotMatrix.map(QVector3D(0.0f, 1.0f, 0.0f)).normalized();

    viewMatrix.lookAt(position, position + forward, up);

    // Guard against a degenerate field of view (e.g. before the QML binding has been evaluated).
    const float fov = (fieldOfViewDeg > 1.0f && fieldOfViewDeg < 179.0f) ? fieldOfViewDeg : 90.0f;
    projectionMatrix.perspective(fov, aspectRatio, 0.1f, 1000.0f);
}
} // namespace

void LayersRendererQtItem::updateCameraMatrices() {
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    buildCameraMatrices(m_cameraPosition, m_cameraEulerRotation, m_fieldOfView,
                        static_cast<float>(width()), static_cast<float>(height()),
                        viewMatrix, projectionMatrix);

    if (m_renderer) {
        m_renderer->setCameraParams(viewMatrix, projectionMatrix);
    }
}

namespace {
constexpr double kPi = 3.14159265358979323846;
// Sphere/dome drag sensitivity in degrees per pixel (matches the QML camera orbitSpeed).
constexpr double kDragDegreesPerPixel = 0.25;

// Wrap an angle in degrees to (-180, 180].
double normalizeAngleDegrees(double a) {
    while (a <= -180.0)
        a += 360.0;
    while (a > 180.0)
        a -= 360.0;
    return a;
}
} // namespace

int LayersRendererQtItem::selectedPlaneLayerIndex() const {
    return m_selectedPlaneIndex;
}

bool LayersRendererQtItem::rayFromScreenPoint(float x, float y, QVector3D& origin, QVector3D& direction) const {
    if (width() <= 0.0 || height() <= 0.0)
        return false;

    // Map logical pixel coordinates to NDC (QML's y axis points down -> flip).
    const float ndcX = 2.0f * x / static_cast<float>(width()) - 1.0f;
    const float ndcY = 1.0f - 2.0f * y / static_cast<float>(height());

    // Build the matrices from the live camera state instead of using a per-frame cache: the
    // cache is only refreshed while frames render, so before the first frame (or if rendering
    // is paused) it can still be identity and every screen point would unproject to the same
    // ray. The QML-bound camera properties are always current on the GUI thread.
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    buildCameraMatrices(m_cameraPosition, m_cameraEulerRotation, m_fieldOfView,
                        static_cast<float>(width()), static_cast<float>(height()),
                        viewMatrix, projectionMatrix);

    bool invertible = false;
    const QMatrix4x4 invVP = (projectionMatrix * viewMatrix).inverted(&invertible);
    if (!invertible)
        return false;

    // Unproject near/far points of the view frustum to obtain a world space ray.
    // map(QVector3D) treats the input as (x, y, z, w=1) and performs the perspective divide;
    // map(QVector4D) would return raw homogeneous coordinates whose x/y/z parts are identical
    // for both depths (only w differs), which collapses the ray direction to zero.
    const QVector3D pNear = invVP.map(QVector3D(ndcX, ndcY, -1.0f));
    const QVector3D pFar = invVP.map(QVector3D(ndcX, ndcY, 1.0f));

    origin = m_cameraPosition;
    direction = (pFar - pNear).normalized();
    if (direction.length() < 1e-9f)
        return false;   // degenerate ray: cannot aim from this point
    return true;
}

bool LayersRendererQtItem::aimAtScreenPointLocked(const BaseLayer* layer, float x, float y, double& azimuthDeg, double& elevationDeg) const {
    QVector3D rayOrigin, rayDir;
    if (!rayFromScreenPoint(x, y, rayOrigin, rayDir))
        return false;

    // World -> dome frame: the plane transform starts with R_x(-meshAngle).
    QMatrix4x4 toDome;
    toDome.rotate(float(m_meshAngle), 1.0f, 0.0f, 0.0f);
    const QVector3D o = toDome.map(rayOrigin);
    const QVector3D dirDome = toDome.mapVector(rayDir).normalized();   // direction: linear part only

    // Intersect the ray with a sphere of radius distance/100 centered at the origin; fall back
    // to the closest approach point when it misses so aiming always works.
    const double r = layer->planeDistance() / 100.0;
    QVector3D p;
    bool found = false;
    if (r > 1e-6) {
        const double b = QVector3D::dotProduct(o, dirDome);
        const double c = QVector3D::dotProduct(o, o) - r * r;
        const double disc = b * b - c;
        if (disc >= 0.0) {
            const double sq = std::sqrt(disc);
            const double t1 = -b - sq;
            const double t2 = -b + sq;
            if (t1 > 1e-6) {
                p = o + dirDome * t1;
                found = true;
            } else if (t2 > 1e-6) {
                p = o + dirDome * t2;
                found = true;
            }
        }
    }
    if (!found) {
        const double tc = -QVector3D::dotProduct(o, dirDome);
        p = o + dirDome * tc;
        if (p.length() < 1e-6)
            p = dirDome * (r > 1e-3 ? r : 1e-3);   // ray through the center: aim along the view direction
    }

    azimuthDeg = std::atan2(p.x(), -p.z()) * 180.0 / kPi;
    elevationDeg = std::atan2(p.y(), std::hypot(p.x(), p.z())) * 180.0 / kPi;
    return true;
}

void LayersRendererQtItem::setSelectedPlaneLayer(std::shared_ptr<BaseLayer> layer, int index) {
    if (m_selectedPlaneLayer == layer && m_selectedPlaneIndex == index)
        return;
    m_selectedPlaneLayer = std::move(layer);
    m_selectedPlaneIndex = index;
    Q_EMIT planeSelectionChanged();
}

void LayersRendererQtItem::setPlaneSelectionByIndex(int index) {
    std::shared_ptr<BaseLayer> layer;
    int resolved = -1;
    {
        std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
        if (!LayersRendererQtItem::isShuttingDown()) {
            auto* slides = Application::isCreated() ? Application::instance().slidesModel() : nullptr;
            LayersModel* slide = slides ? slides->selectedSlide() : nullptr;
            if (slide && index >= 0 && index < slide->numberOfLayers()) {
                std::shared_ptr<BaseLayer> candidate = slide->layerShared(index);
                if (candidate && candidate->gridMode() != static_cast<uint8_t>(BaseLayer::None)) {
                    layer = std::move(candidate);
                    resolved = index;
                }
            }
        }
    }
    setSelectedPlaneLayer(std::move(layer), resolved);
}

bool LayersRendererQtItem::beginPlaneDrag(float x, float y) {
    bool ok = false;
    double hitAz = 0.0, hitEl = 0.0;
    bool selectionValid = true;
    uint8_t mode = 0;
    glm::vec3 startRotate{};
    {
        std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
        if (!LayersRendererQtItem::isShuttingDown() && m_selectedPlaneLayer) {
            // The layer may have been removed or the slide switched since it was selected.
            auto* slides = Application::isCreated() ? Application::instance().slidesModel() : nullptr;
            LayersModel* slide = slides ? slides->selectedSlide() : nullptr;
            selectionValid = false;
            if (slide) {
                for (int l = 0; l < slide->numberOfLayers(); ++l) {
                    if (slide->layerShared(l) == m_selectedPlaneLayer) {
                        selectionValid = true;
                        break;
                    }
                }
            }
            if (selectionValid) {
                mode = m_selectedPlaneLayer->gridMode();
                startRotate = m_selectedPlaneLayer->rotate();
                if (mode == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
                    // Flat layer: no hit testing, wherever the pointer is it aims at the layer.
                    ok = aimAtScreenPointLocked(m_selectedPlaneLayer.get(), x, y, hitAz, hitEl);
                } else {
                    // Sphere/dome layer: rotation follows the pointer delta from this point.
                    ok = true;
                }
            }
        }
    }

    m_planeDragActive = false;
    if (!ok) {
        const bool hadSelection = (m_selectedPlaneLayer != nullptr);
        if (hadSelection && !selectionValid)
            setSelectedPlaneLayer(nullptr, -1);   // stale selection: clear it
        return false;
    }

    if (mode == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
        // Store the grab offset so the layer does not jump to the cursor on press.
        m_grabAzimuthOffsetDeg = normalizeAngleDegrees(m_selectedPlaneLayer->planeAzimuth() - hitAz);
        m_grabElevationOffsetDeg = m_selectedPlaneLayer->planeElevation() - hitEl;
    } else {
        // Sphere/dome: remember the press point and current rotation as the drag baseline.
        m_dragStartX = x;
        m_dragStartY = y;
        m_dragStartPitchDeg = startRotate.x;
        m_dragStartYawDeg = startRotate.y;
    }
    m_planeDragActive = true;
    return true;
}

bool LayersRendererQtItem::dragPlaneTo(float x, float y) {
    if (!m_planeDragActive || !m_selectedPlaneLayer)
        return false;

    bool changed = false;
    std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
    if (LayersRendererQtItem::isShuttingDown())
        return false;

    if (m_selectedPlaneLayer->gridMode() == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
        double targetAz = 0.0, targetEl = 0.0;
        if (!aimAtScreenPointLocked(m_selectedPlaneLayer.get(), x, y, targetAz, targetEl))
            return false;

        const double newAz = normalizeAngleDegrees(targetAz + m_grabAzimuthOffsetDeg);
        const double newEl = targetEl + m_grabElevationOffsetDeg;

        if (std::abs(newAz - m_selectedPlaneLayer->planeAzimuth()) > 1e-4 ||
            std::abs(newEl - m_selectedPlaneLayer->planeElevation()) > 1e-4) {
            m_selectedPlaneLayer->setPlaneAzimuth(newAz);
            m_selectedPlaneLayer->setPlaneElevation(newEl);
            changed = true;
        }
    } else {
        // Sphere/dome: X drag -> yaw; Y drag -> pitch as well for spheres. Domes keep their
        // pitch and rotate in yaw only. The content follows the pointer (a rightward/upward
        // drag moves the layer's front right/up on screen).
        const bool domeYawOnly = (m_selectedPlaneLayer->gridMode() == static_cast<uint8_t>(BaseLayer::GridMode::Dome));
        const double newYaw   = m_dragStartYawDeg   - (x - m_dragStartX) * kDragDegreesPerPixel;
        const double newPitch = domeYawOnly ? m_dragStartPitchDeg
                                            : m_dragStartPitchDeg - (y - m_dragStartY) * kDragDegreesPerPixel;

        glm::vec3 rot = m_selectedPlaneLayer->rotate();
        if (std::abs(newYaw - static_cast<double>(rot.y)) > 1e-4 ||
            std::abs(newPitch - static_cast<double>(rot.x)) > 1e-4) {
            rot.x = static_cast<float>(newPitch);
            rot.y = static_cast<float>(newYaw);
            m_selectedPlaneLayer->setRotate(rot);
            changed = true;
        }
    }
    return changed;
}

void LayersRendererQtItem::endPlaneDrag() {
    m_planeDragActive = false;
}

void LayersRendererQtItem::handleWindowChanged(QQuickWindow* win) {
    if (win) {
        connect(win, &QQuickWindow::beforeSynchronizing, this, &LayersRendererQtItem::sync, Qt::DirectConnection);
        connect(win, &QQuickWindow::sceneGraphInvalidated, this, &LayersRendererQtItem::cleanup, Qt::DirectConnection);
        win->setColor(Qt::black);

        if (m_timer == nullptr) {
            m_timer = new QTimer();
            m_timer->setInterval((1.0f / 60.0f) * 1000.0f);

            connect(m_timer, &QTimer::timeout, win, &QQuickWindow::update);

            m_timer->start();
        }
    }
}

void LayersRendererQtItem::cleanup() {
    beginShutdown();

    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }

    if (window()) {
        disconnect(window(), &QQuickWindow::beforeSynchronizing, this, &LayersRendererQtItem::sync);
        disconnect(window(), &QQuickWindow::sceneGraphInvalidated, this, &LayersRendererQtItem::cleanup);
    }

    if (m_renderer) {
        m_renderer->shutdown();
        delete m_renderer;
        m_renderer = nullptr;
    }
}

class CleanupJob : public QRunnable {
public:
    CleanupJob(LayersRendererQtOpenGLObject* renderer) : m_renderer(renderer) {}
    void run() override { delete m_renderer; }

private:
    LayersRendererQtOpenGLObject* m_renderer;
};

void LayersRendererQtItem::releaseResources() {
    beginShutdown();
    if (m_timer) {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }

    if (m_renderer) {
        m_renderer->shutdown();
        if (window()) {
            window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
        }
        else {
            delete m_renderer;
        }
        m_renderer = nullptr;
    }
}

void LayersRendererQtItem::sync() {
    if (s_shuttingDown)
        return;

    if (!m_renderer) {
        m_renderer = new LayersRendererQtOpenGLObject(this);
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &LayersRendererQtOpenGLObject::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &LayersRendererQtOpenGLObject::firstPass, Qt::DirectConnection);
        connect(window(), &QQuickWindow::afterRenderPassRecording, m_renderer, &LayersRendererQtOpenGLObject::secondPass, Qt::DirectConnection);
        connect(window(), &QQuickWindow::frameSwapped, m_renderer, &LayersRendererQtOpenGLObject::reportSwap, Qt::DirectConnection);
    }
    m_renderer->setWindow(window());
    m_renderer->setItemVisible(isVisible());
    m_renderer->updateMeshes(m_meshRadius, m_meshFov, m_meshAngle);
    m_renderer->setMpvObject(m_mpvObject);
    m_renderer->setBackgroundImageFile(m_backgroundImageFile);
    m_renderer->setForegroundImageFile(m_foregroundImageFile);

#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(2, 3)
    m_renderer->setDivideUpdateAndRender(!m_uiPopupOpen);
#endif

    // Map item rect to physical pixels so paint() can set the correct viewport
    const qreal dpr = window()->devicePixelRatio();
    const QPointF origin = mapToScene(QPointF(0, 0));
    const QRectF itemRect(
        origin.x() * dpr,
        (window()->height() - origin.y() - height()) * dpr,  // flip Y for OpenGL
        width()  * dpr,
        height() * dpr
    );
    m_renderer->setViewportRect(itemRect.toRect());

    updateCameraMatrices();
}

void LayersRendererQtItem::beginShutdown() {
    s_shuttingDown = true;
}

bool LayersRendererQtItem::isShuttingDown() {
    return s_shuttingDown;
}

std::mutex& LayersRendererQtItem::layerAccessMutex() {
    return s_layerAccessMutex;
}

// -------------------------------------------------------------------------
// LayersRendererQtOpenGLObject
// -------------------------------------------------------------------------

LayersRendererQtOpenGLObject::LayersRendererQtOpenGLObject(QObject* parent)
    : QObject(parent), m_window(nullptr), m_initialized(false), m_meshRadius(0), m_meshFov(0),
    m_quadVBO(QOpenGLBuffer::VertexBuffer) {
    // Set sensible defaults matching the previous hardcoded values
    m_viewMatrix.lookAt(QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 0.0f, -1.0f), QVector3D(0.0f, 1.0f, 0.0f));
    m_projectionMatrix.perspective(90.0f, 1.0f, 0.1f, 1000.0f);
}

LayersRendererQtOpenGLObject::~LayersRendererQtOpenGLObject() {
    m_videoPrg.reset();
    m_meshPrg.reset();
    m_EACPrg.reset();
    m_domeMesh.reset();
    m_domeMaskMesh.reset();
    m_sphereMesh.reset();
    if (m_maskTexture) {
        glDeleteTextures(1, &m_maskTexture);
        m_maskTexture = 0;
    }
}

void LayersRendererQtOpenGLObject::setWindow(QQuickWindow* window) {
    m_window = window;
}

void LayersRendererQtOpenGLObject::setCameraParams(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    m_viewMatrix = viewMatrix;
    m_projectionMatrix = projectionMatrix;
}

void LayersRendererQtOpenGLObject::setMpvObject(MpvObject* mpv) {
    m_mpvObject = mpv;
}

void LayersRendererQtOpenGLObject::setBackgroundImageFile(const QString& file) {
    if (m_backgroundImageFile != file) {
        m_backgroundImageFile = file;
        m_backgroundImageDirty = true;
    }
}

void LayersRendererQtOpenGLObject::setForegroundImageFile(const QString& file) {
    if (m_foregroundImageFile != file) {
        m_foregroundImageFile = file;
        m_foregroundImageDirty = true;
    }
}

void LayersRendererQtOpenGLObject::createShaders() {
    // Create video shader
    m_videoPrg = std::make_unique<QOpenGLShaderProgram>();
    m_videoPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, VideoVert);
    m_videoPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, VideoFrag);
    m_videoPrg->link();

    m_videoPrg->bind();
    m_videoPrg->setUniformValue("tex", 0);
    m_videoAlphaLoc = m_videoPrg->uniformLocation("alpha");
    m_videoEyeModeLoc = m_videoPrg->uniformLocation("eye");
    m_videoFlipYLoc = m_videoPrg->uniformLocation("flipY");
    m_videoStereoscopicModeLoc = m_videoPrg->uniformLocation("stereoscopicMode");
    m_videoRoi = m_videoPrg->uniformLocation("roi");
    m_videoPrg->release();

    // Create mesh shader
    m_meshPrg = std::make_unique<QOpenGLShaderProgram>();
    m_meshPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, MeshVert);
    m_meshPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, VideoFrag);
    m_meshPrg->link();

    m_meshPrg->bind();
    m_meshPrg->setUniformValue("tex", 0);
    m_meshMatrixLoc = m_meshPrg->uniformLocation("mvp");
    m_meshEyeModeLoc = m_meshPrg->uniformLocation("eye");
    m_meshFlipYLoc = m_meshPrg->uniformLocation("flipY");
    m_meshStereoscopicModeLoc = m_meshPrg->uniformLocation("stereoscopicMode");
    m_meshRoi = m_meshPrg->uniformLocation("roi");
    m_meshAlphaLoc = m_meshPrg->uniformLocation("alpha");
    m_meshOutsideLoc = m_meshPrg->uniformLocation("outside");
    m_meshPrg->release();

    // Create EAC shader
    m_EACPrg = std::make_unique<QOpenGLShaderProgram>();
    m_EACPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, EACMeshVert);
    m_EACPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, EACVideoFrag);
    m_EACPrg->link();

    m_EACPrg->bind();
    m_EACPrg->setUniformValue("tex", 0);
    m_EACMatrixLoc = m_EACPrg->uniformLocation("mvp");
    m_EACEyeModeLoc = m_EACPrg->uniformLocation("eye");
    m_EACFlipYLoc = m_EACPrg->uniformLocation("flipY");
    m_EACStereoscopicModeLoc = m_EACPrg->uniformLocation("stereoscopicMode");
    m_EACAlphaLoc = m_EACPrg->uniformLocation("alpha");
    m_EACOutsideLoc = m_EACPrg->uniformLocation("outside");
    m_EACScaleLoc = m_EACPrg->uniformLocation("scaleToUnitCube");
    m_EACVideoWidthLoc = m_EACPrg->uniformLocation("videoWidth");
    m_EACVideoHeightLoc = m_EACPrg->uniformLocation("videoHeight");
    m_EACFlipUpDownLoc = m_EACPrg->uniformLocation("flipUpDown");
    m_EACPrg->release();
}

void LayersRendererQtOpenGLObject::initializeGL() {
    createShaders();

    // Create a 1x1 black texture for the dome mask
    unsigned char blackPixel[4] = { 0, 0, 0, 255 };
    glGenTextures(1, &m_maskTexture);
    glBindTexture(GL_TEXTURE_2D, m_maskTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create image layers
    m_backgroundImageLayer = std::make_shared<ImageLayer>("background");
    m_backgroundImageLayer->initialize();
    m_foregroundImageLayer = std::make_shared<ImageLayer>("foreground");
    m_foregroundImageLayer->initialize();
    m_overlayImageLayer = std::make_shared<ImageLayer>("overlay");
    m_overlayImageLayer->initialize();

    // Setup quad for 2D rendering
    m_quadVAO.create();
    m_quadVAO.bind();

    constexpr std::array<const float, 16> QuadVerts = {
        // x     y     u    v
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
        -1.f,  1.f, 0.f, 1.f,
         1.f,  1.f, 1.f, 1.f
    };

    m_quadVBO.create();
    m_quadVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_quadVBO.bind();
    m_quadVBO.allocate(QuadVerts.data(), 16 * sizeof(float));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    m_quadVAO.release();
}

void LayersRendererQtOpenGLObject::updateMeshes(double radius, double fov, double angle) {
    if (m_meshRadius != radius || m_meshFov != fov) {
        m_meshRadius = radius;
        m_meshFov = fov;
        m_meshesDirty = true;
    }
    m_meshAngle = angle;
}

void LayersRendererQtOpenGLObject::addLayer(std::shared_ptr<BaseLayer> layer) {
    if (layer)
        m_layers.push_back(layer);
}

void LayersRendererQtOpenGLObject::clearLayers() {
    m_layers.clear();
}

const std::vector<std::shared_ptr<BaseLayer>>& LayersRendererQtOpenGLObject::getLayers() {
    return m_layers;
}

void LayersRendererQtOpenGLObject::renderQuad() {
    m_quadVAO.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_quadVAO.release();
}

void LayersRendererQtOpenGLObject::renderLayer(const BaseLayer* layer, int eyeMode, float angle,
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    if (!layer || !layer->ready()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->textureId());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int gridMode = layer->gridMode();
    int stereoMode = layer->stereoMode();
    // Master UI specific: if grid is 0, we use the default values
    if (gridMode == 0) {
        gridMode = SyncHelper::instance().variables.gridToMapOnBg;
        stereoMode = SyncHelper::instance().variables.stereoscopicModeBg;
    }

    if (gridMode == 4) {
        m_EACPrg->bind();

        m_EACPrg->setUniformValue(m_EACAlphaLoc, layer->alpha());
        m_EACPrg->setUniformValue(m_EACFlipYLoc, layer->flipY());
        m_EACPrg->setUniformValue(m_EACOutsideLoc, 0);
        m_EACPrg->setUniformValue(m_EACVideoWidthLoc, layer->width());
        m_EACPrg->setUniformValue(m_EACVideoHeightLoc, layer->height());
        m_EACPrg->setUniformValue(m_EACFlipUpDownLoc, false);
        m_EACPrg->setUniformValue(m_EACScaleLoc, static_cast<float>(100.0 / m_meshRadius));

        if (layer->stereoMode() > 0) {
            m_EACPrg->setUniformValue(m_EACEyeModeLoc, eyeMode);
            m_EACPrg->setUniformValue(m_EACStereoscopicModeLoc, stereoMode);
        }
        else {
            m_EACPrg->setUniformValue(m_EACEyeModeLoc, 0);
            m_EACPrg->setUniformValue(m_EACStereoscopicModeLoc, 0);
        }

        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        QVector3D translate(layer->translate().x, layer->translate().y, layer->translate().z);
        mvp.translate(translate);

        QMatrix4x4 mvpRot = mvp;
        mvpRot.rotate(layer->rotate().z, 0, 0, 1);                      // roll
        mvpRot.rotate(layer->rotate().x, 1, 0, 0);                      // pitch
        mvpRot.rotate(layer->rotate().y, 0, 1, 0);                      // yaw
        mvpRot.rotate(-90.f, 0, 0, 1);                                    // roll

        m_EACPrg->setUniformValue(m_EACMatrixLoc, mvpRot);

        glEnable(GL_CULL_FACE);

        glCullFace(GL_BACK);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        glCullFace(GL_FRONT);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        // Restore backface culling
        glCullFace(GL_BACK);

        glDisable(GL_CULL_FACE);

        m_EACPrg->release();
    }
    else if (gridMode == 3) {
        // EQR sphere rendering
        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        QVector3D translate(layer->translate().x, layer->translate().y, layer->translate().z);
        mvp.translate(translate);

        QMatrix4x4 mvpRot = mvp;
        mvpRot.rotate(layer->rotate().z, 0, 0, 1);  // roll
        mvpRot.rotate(layer->rotate().x, 1, 0, 0);  // pitch
        mvpRot.rotate(layer->rotate().y - 90.f, 0, 1, 0);  // yaw

        m_meshPrg->bind();

        if (stereoMode > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, stereoMode);
        }
        else {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glm::vec4 roi = layer->roi();
            m_meshPrg->setUniformValue(m_meshRoi, roi.x, roi.y, roi.z, roi.w);
        }
        else {
            m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        }

        m_meshPrg->setUniformValue(m_meshAlphaLoc, layer->alpha());
        m_meshPrg->setUniformValue(m_meshFlipYLoc, layer->flipY());
        m_meshPrg->setUniformValue(m_meshMatrixLoc, mvpRot);

        // Render inside sphere
        m_meshPrg->setUniformValue(m_meshOutsideLoc, 0);

        glEnable(GL_CULL_FACE);

        glCullFace(GL_BACK);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        glCullFace(GL_FRONT);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        glDisable(GL_CULL_FACE);

        m_meshPrg->release();
    }
    else if (gridMode == 2) {
        // Dome rendering
        m_meshPrg->bind();

        if (stereoMode > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, stereoMode);
        }
        else {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glm::vec4 roi = layer->roi();
            m_meshPrg->setUniformValue(m_meshRoi, roi.x, roi.y, roi.z, roi.w);
        }
        else {
            m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        }

        m_meshPrg->setUniformValue(m_meshAlphaLoc, layer->alpha());
        m_meshPrg->setUniformValue(m_meshFlipYLoc, layer->flipY());

        QMatrix4x4 mvpRot = projectionMatrix * viewMatrix;
        QVector3D translate(layer->translate().x, layer->translate().y, layer->translate().z);
        mvpRot.translate(translate);
        mvpRot.rotate(layer->rotate().z, 0, 0, 1);           // roll
        mvpRot.rotate(layer->rotate().x - angle, 1, 0, 0);   // pitch
        mvpRot.rotate(layer->rotate().y, 0, 1, 0);           // yaw

        m_meshPrg->setUniformValue(m_meshMatrixLoc, mvpRot);

        if (m_domeMesh) {
            m_domeMesh->draw();
        }

        m_meshPrg->release();
    }
    else if (gridMode == 1) {
        // Plane rendering
        m_meshPrg->bind();

        if (stereoMode > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, stereoMode);
        }
        else {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glm::vec4 roi = layer->roi();
            m_meshPrg->setUniformValue(m_meshRoi, roi.x, roi.y, roi.z, roi.w);
        }
        else {
            m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        }

        m_meshPrg->setUniformValue(m_meshAlphaLoc, layer->alpha());
        m_meshPrg->setUniformValue(m_meshFlipYLoc, layer->flipY());

        QMatrix4x4 planeTransform;

        // Respect the dome angle
        planeTransform.rotate(-angle, 1, 0, 0);

        // Specific plane parameters
        planeTransform.rotate(float(layer->planeAzimuth()), 0, -1, 0);    // azimuth
        planeTransform.rotate(float(layer->planeElevation()), 1, 0, 0);   // elevation
        planeTransform.rotate(float(layer->planeRoll()), 0, 0, 1);        // roll
        planeTransform.translate(
            float(layer->planeHorizontal()) / 100.f,
            float(layer->planeVertical()) / 100.f,
            float(-layer->planeDistance()) / 100.f);

        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        QMatrix4x4 finalMvp = mvp * planeTransform;

        m_meshPrg->setUniformValue(m_meshMatrixLoc, finalMvp);

        layer->drawPlane();

        m_meshPrg->release();
    }
    else {
        // 2D rendering (gridMode == 0)
        m_videoPrg->bind();

        if (stereoMode > 0) {
            m_videoPrg->setUniformValue(m_videoEyeModeLoc, eyeMode);
            m_videoPrg->setUniformValue(m_videoStereoscopicModeLoc, stereoMode);
        }
        else {
            m_videoPrg->setUniformValue(m_videoEyeModeLoc, 0);
            m_videoPrg->setUniformValue(m_videoStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glm::vec4 roi = layer->roi();
            m_videoPrg->setUniformValue(m_videoRoi, roi.x, roi.y, roi.z, roi.w);
        }
        else {
            m_videoPrg->setUniformValue(m_videoRoi, 0.f, 0.f, 1.f, 1.f);
        }

        m_videoPrg->setUniformValue(m_videoAlphaLoc, layer->alpha());
        m_videoPrg->setUniformValue(m_videoFlipYLoc, layer->flipY());

        renderQuad();

        m_videoPrg->release();
    }

    glDisable(GL_BLEND);
}

void LayersRendererQtOpenGLObject::renderMpvObject(MpvObject* mpv, int eyeMode, float angle,
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    if (!mpv)
        return;

    if (!mpv->m_fboReady)
        return;

    std::lock_guard<std::mutex> lock(mpv->m_renderMutex);

    const unsigned int texId = mpv->fboTextureId();
    const float alpha = static_cast<float>(mpv->visibility()) / 100.f;
    const int texW = mpv->fboWidth();
    const int texH = mpv->fboHeight();

    if (texId == 0 || alpha <= 0.f || texW <= 0 || texH <= 0)
        return;

    int gridMode = mpv->gridToMapOn();
    int stereoMode = mpv->stereoscopicMode();
    // Master UI specific: if grid is 0, we use the default values
    if (gridMode == 0) {
        gridMode = SyncHelper::instance().variables.gridToMapOnBg;
        stereoMode = SyncHelper::instance().variables.stereoscopicModeBg;
    }

    const QVector3D translate(
        mpv->translate().x() / 100.f,
        mpv->translate().y() / 100.f,
        mpv->translate().z() / 100.f);

    const QVector3D rotateXYZ(
        mpv->rotate().x(),
        mpv->rotate().y(),
        mpv->rotate().z());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (gridMode == 4) {
        // EAC sphere
        m_EACPrg->bind();

        m_EACPrg->setUniformValue(m_EACAlphaLoc, alpha);
        m_EACPrg->setUniformValue(m_EACFlipYLoc, false);
        m_EACPrg->setUniformValue(m_EACVideoWidthLoc, texW);
        m_EACPrg->setUniformValue(m_EACVideoHeightLoc, texH);
        m_EACPrg->setUniformValue(m_EACFlipUpDownLoc, true);
        m_EACPrg->setUniformValue(m_EACScaleLoc, static_cast<float>(100.0 / m_meshRadius));

        if (stereoMode > 0) {
            m_EACPrg->setUniformValue(m_EACEyeModeLoc, eyeMode);
            m_EACPrg->setUniformValue(m_EACStereoscopicModeLoc, stereoMode);
        }
        else {
            m_EACPrg->setUniformValue(m_EACEyeModeLoc, 0);
            m_EACPrg->setUniformValue(m_EACStereoscopicModeLoc, 0);
        }

        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        mvp.translate(translate);

        QMatrix4x4 mvpRot = mvp;
        mvpRot.rotate(rotateXYZ.z(), 0, 0, 1);  // roll
        mvpRot.rotate(rotateXYZ.x(), 1, 0, 0);  // pitch
        mvpRot.rotate(rotateXYZ.y(), 0, 1, 0);  // yaw
        if (stereoMode == 3) {
            mvpRot.rotate(180, 0, 1, 0);  // yaw
            mvpRot.rotate(90.f, 0, 0, 1); // roll
        }
        else {
            mvpRot.rotate(180.f, 0, 0, 1); // roll
        }
        m_EACPrg->setUniformValue(m_EACMatrixLoc, mvpRot);

        m_EACPrg->setUniformValue(m_EACOutsideLoc, 1);
        
        glEnable(GL_CULL_FACE);

        glCullFace(GL_BACK);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        glCullFace(GL_FRONT);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        // Restore backface culling
        glCullFace(GL_BACK);

        glDisable(GL_CULL_FACE);

        m_EACPrg->release();
    }
    else if (gridMode == 3) {
        // EQR sphere
        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        mvp.translate(translate);

        QMatrix4x4 mvpRot = mvp;
        mvpRot.rotate(rotateXYZ.z(), 0, 0, 1);  // roll
        mvpRot.rotate(rotateXYZ.x(), 1, 0, 0);  // pitch
        mvpRot.rotate(rotateXYZ.y() - 90.f, 0, 1, 0);  // yaw

        m_meshPrg->bind();

        if (stereoMode > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, stereoMode);
        }
        else {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        }

        m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        m_meshPrg->setUniformValue(m_meshAlphaLoc, alpha);
        m_meshPrg->setUniformValue(m_meshFlipYLoc, true);
        m_meshPrg->setUniformValue(m_meshMatrixLoc, mvpRot);
        m_meshPrg->setUniformValue(m_meshOutsideLoc, 0);

        // Render back faces first for correct blending
        glEnable(GL_CULL_FACE);
        
        glCullFace(GL_BACK);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        glCullFace(GL_FRONT);
        if (m_sphereMesh)
            m_sphereMesh->draw();

        glDisable(GL_CULL_FACE);
        m_meshPrg->release();
    }
    else if (gridMode == 2) {
        // Dome
        m_meshPrg->bind();

        if (stereoMode > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, stereoMode);
        }
        else {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        }

        m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        m_meshPrg->setUniformValue(m_meshAlphaLoc, alpha);
        m_meshPrg->setUniformValue(m_meshFlipYLoc, true);

        QMatrix4x4 mvpRot = projectionMatrix * viewMatrix;
        mvpRot.translate(translate);
        mvpRot.rotate(rotateXYZ.z(), 0, 0, 1);              // roll
        mvpRot.rotate(rotateXYZ.x() - angle, 1, 0, 0);      // pitch
        mvpRot.rotate(rotateXYZ.y(), 0, 1, 0);              // yaw

        m_meshPrg->setUniformValue(m_meshMatrixLoc, mvpRot);

        if (m_domeMesh)
            m_domeMesh->draw();

        m_meshPrg->release();
    }
    else if (gridMode == 1) {
        // Plane
        m_meshPrg->bind();

        if (stereoMode > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, stereoMode);
        }
        else {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        }

        m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        m_meshPrg->setUniformValue(m_meshAlphaLoc, alpha);
        m_meshPrg->setUniformValue(m_meshFlipYLoc, true);

        QMatrix4x4 planeTransform;
        planeTransform.rotate(-angle, 1, 0, 0);                                          // dome angle
        planeTransform.rotate(float(mpv->planeElevation()), 1, 0, 0);                    // elevation
        planeTransform.translate(0.f, 0.f, float(-mpv->planeDistance()) / 100.f);        // distance

        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        m_meshPrg->setUniformValue(m_meshMatrixLoc, mvp * planeTransform);

        mpv->drawPlane();

        m_meshPrg->release();
    }
    else {
        // 2D rendering (gridMode == 0)
        m_videoPrg->bind();

        if (stereoMode > 0) {
            m_videoPrg->setUniformValue(m_videoEyeModeLoc, eyeMode);
            m_videoPrg->setUniformValue(m_videoStereoscopicModeLoc, stereoMode);
        }
        else {
            m_videoPrg->setUniformValue(m_videoEyeModeLoc, 0);
            m_videoPrg->setUniformValue(m_videoStereoscopicModeLoc, 0);
        }

        m_videoPrg->setUniformValue(m_videoRoi, 0.f, 0.f, 1.f, 1.f);
        m_videoPrg->setUniformValue(m_videoAlphaLoc, alpha);
        m_videoPrg->setUniformValue(m_videoFlipYLoc, true);

        renderQuad();

        m_videoPrg->release();
    }

    glDisable(GL_BLEND);
}

void LayersRendererQtOpenGLObject::updateLayers() {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    std::lock_guard<std::mutex> layerAccessLock(LayersRendererQtItem::layerAccessMutex());

    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    glm::vec3 rotXYZ = glm::vec3(0.f);
    glm::vec3 translateXYZ = glm::vec3(0.f);
    if (m_mpvObject) {
        rotXYZ.x = m_mpvObject->rotate().x();
        rotXYZ.y = m_mpvObject->rotate().y();
        rotXYZ.z = m_mpvObject->rotate().z();
        translateXYZ.x = m_mpvObject->translate().x() / 100.f;
        translateXYZ.y = m_mpvObject->translate().y() / 100.f;
        translateXYZ.z = m_mpvObject->translate().z() / 100.f;

        // Update MpvObject plane grid when in plane mode
        int gridMode = m_mpvObject->gridToMapOn();
        if (gridMode == 0) {
            gridMode = SyncHelper::instance().variables.gridToMapOnBg;
        }
        if (gridMode == 1) {
            m_mpvObject->updatePlane();
        }
    }

    // The background/foreground image layers are master layers, so their plane mesh is
    // never created by the setters. Apply the global plane parameters and build the mesh
    // here, otherwise BaseLayer::drawPlane() is a no-op and nothing is rendered.
    auto ensurePlaneMesh = [](BaseLayer* layer, int gridMode, int stereoMode) {
        if (!layer || !layer->ready() || gridMode != static_cast<int>(BaseLayer::GridMode::Plane))
            return;

        const glm::vec2 planeSize(
            float(SyncHelper::instance().variables.planeWidth),
            float(SyncHelper::instance().variables.planeHeight));

        layer->setStereoMode(static_cast<uint8_t>(stereoMode));
        layer->setPlaneSize(planeSize, static_cast<uint8_t>(SyncHelper::instance().variables.planeConsiderAspectRatio));
        layer->setPlaneElevation(SyncHelper::instance().variables.planeElevation);
        layer->setPlaneDistance(SyncHelper::instance().variables.planeDistance);

        if (!layer->hasPlane() || layer->needSync()) {
            layer->updatePlane();
        }
    };

    // Process layer image uploads
    if (m_backgroundImageLayer) {
        m_backgroundImageLayer->processImageUpload(m_backgroundImageFile.toStdString(), m_backgroundImageDirty);
        if (m_backgroundImageDirty) {
            m_backgroundImageLayer->setRotate(rotXYZ);
            m_backgroundImageLayer->setTranslate(translateXYZ);
            m_backgroundImageDirty = false;
        }
        ensurePlaneMesh(m_backgroundImageLayer.get(),
            SyncHelper::instance().variables.gridToMapOnBg,
            SyncHelper::instance().variables.stereoscopicModeBg);
    }
    if (m_foregroundImageLayer) {
        m_foregroundImageLayer->processImageUpload(m_foregroundImageFile.toStdString(), m_foregroundImageDirty);
        if (m_foregroundImageDirty) {
            m_foregroundImageLayer->setRotate(rotXYZ);
            m_foregroundImageLayer->setTranslate(translateXYZ);
            m_foregroundImageDirty = false;
        }
        ensurePlaneMesh(m_foregroundImageLayer.get(),
            SyncHelper::instance().variables.gridToMapOnFg,
            SyncHelper::instance().variables.stereoscopicModeFg);
    }
    if (m_mpvObject && m_overlayImageLayer) {
        bool overlayUpdated = false;
        if(SyncHelper::instance().variables.overlayFile != m_overlayImageLayer->loadedFile()) {
            overlayUpdated = true;
        }
        m_overlayImageLayer->processImageUpload(SyncHelper::instance().variables.overlayFile, overlayUpdated);
    }

    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown() || !Application::isCreated() || !Application::instance().slidesModel()) {
        return;
    }

    // Use try-lock snapshot to avoid deadlocking with the UI thread
    QList<QSharedPointer<LayersModel>> slidesSnapshot;
    if (!Application::instance().slidesModel()->trySnapshotSlides(slidesSnapshot)) {
        return; // Skip this frame if we can't acquire the lock
    }

    // Update master slide
    {
        auto* master = Application::instance().slidesModel()->masterSlide();
        if (master) {
            int numLayers = master->numberOfLayers();
            for (int l = numLayers - 1; l >= 0; l--) {
                std::shared_ptr<BaseLayer> layerPtr = master->layerShared(l);
                BaseLayer* layer = layerPtr.get();
                if (layer) {
                    if (layer->alpha() > 0.f) {
                        if (layer->ready()) {
                            if (layer->gridMode() == BaseLayer::GridMode::Plane) {
                                if (!layer->hasPlane() || layer->needSync()) {
                                    layer->updatePlane();
                                }
                            }
                            layer->updateFrame();
                        }
                        else {
                            layer->update();
                        }
                    }
                }
            }
        }
    }

    for (int s = 0; s < slidesSnapshot.size(); s++) {
        auto& slidePtr = slidesSnapshot[s];
        if (!slidePtr) continue;

        int numLayers = slidePtr->numberOfLayers();
        for (int l = numLayers - 1; l >= 0; l--) {
            std::shared_ptr<BaseLayer> layerPtr = slidePtr->layerShared(l);
            BaseLayer* layer = layerPtr.get();

            if (layer) {
                if (layer->alpha() > 0.f) {
                    if (layer->ready()) {
                        if (layer->gridMode() == BaseLayer::GridMode::Plane) {
                            if (!layer->hasPlane() || layer->needSync()) {
                                layer->updatePlane();
                            }
                        }
                        //SlideModel updates continuously the layers, so all we should need to do is update the frame.
                        layer->updateFrame();
                    }
                    else {
                        layer->update();
                    }
                }
            }
        }
    }
}

void LayersRendererQtOpenGLObject::renderLayers(float angle,
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    std::lock_guard<std::mutex> layerAccessLock(LayersRendererQtItem::layerAccessMutex());

    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    int eyeMode = 1; // Default to left eye/mono

    if (!Application::isCreated())
        return;

    glm::vec3 rotXYZ = glm::vec3(0.f);
    glm::vec3 translateXYZ = glm::vec3(0.f);
    if (m_mpvObject) {
        rotXYZ.x = m_mpvObject->rotate().x();
        rotXYZ.y = m_mpvObject->rotate().y();
        rotXYZ.z = m_mpvObject->rotate().z();
        translateXYZ.x = m_mpvObject->translate().x() / 100.f;
        translateXYZ.y = m_mpvObject->translate().y() / 100.f;
        translateXYZ.z = m_mpvObject->translate().z() / 100.f;
    }

    // Render background image layer
    if (m_backgroundImageLayer && m_backgroundImageLayer->ready() && SyncHelper::instance().variables.alphaBg > 0.f) {
        m_backgroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaBg);
        m_backgroundImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOnBg));
        m_backgroundImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicModeBg));
        renderLayer(m_backgroundImageLayer.get(), eyeMode, angle, viewMatrix, projectionMatrix);
    }

    // Render master slide
    if (Application::instance().slidesModel()) {
        auto* slide = Application::instance().slidesModel()->masterSlide();
        if (slide) {
            int numLayers = slide->numberOfLayers();
            for (int l = numLayers - 1; l >= 0; l--) {
                std::shared_ptr<BaseLayer> layerPtr = slide->layerShared(l);
                BaseLayer* layer = layerPtr.get();

                if (layer) {
                    // Layers that only exist on the master can be hidden from the 3D view.
                    if (UserInterfaceSettings::hideMasterOnlyLayersIn3DView() && layer->existOnMasterOnly())
                        continue;
                    if (layer->ready() && layer->hasTexture() && (layer->alpha() > 0.f)) {
                        if (layer->hasSubLayers()) {
                            for (const auto& sublayer : layer->getSubLayers()) {
                                renderLayer(sublayer.get(), eyeMode, angle, viewMatrix, projectionMatrix);
                            }
                        }
                        else if(!layer->isQRCodeDetectionEnabled() || layer->isQRCodeDetectionEnabled() && !layer->hasSubLayers()){
                            renderLayer(layer, eyeMode, angle, viewMatrix, projectionMatrix);
                        }
                    }
                }
            }
        }
    }

    // Render MPV video layer
    if (m_mpvObject && SyncHelper::instance().variables.alpha) {
        renderMpvObject(m_mpvObject, eyeMode, angle, viewMatrix, projectionMatrix);

        // Render overlay image layer
        if (m_overlayImageLayer && m_overlayImageLayer->ready()) {
            m_overlayImageLayer->setRotate(rotXYZ);
            m_overlayImageLayer->setTranslate(translateXYZ);
            m_overlayImageLayer->setAlpha(SyncHelper::instance().variables.alpha);
            m_overlayImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOn));
            m_overlayImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicMode));
            renderLayer(m_overlayImageLayer.get(), eyeMode, angle, viewMatrix, projectionMatrix);
        }
    }

    // Render slides layers - use try-lock snapshot to avoid deadlocking with UI thread
    if (Application::instance().slidesModel()) {
        QList<QSharedPointer<LayersModel>> slidesSnapshot;
        if (Application::instance().slidesModel()->trySnapshotSlides(slidesSnapshot)) {
            for (int s = 0; s < slidesSnapshot.size(); s++) {
                auto& slidePtr = slidesSnapshot[s];
                if (!slidePtr) continue;

                int numLayers = slidePtr->numberOfLayers();
                for (int l = numLayers - 1; l >= 0; l--) {
                    std::shared_ptr<BaseLayer> layerPtr = slidePtr->layerShared(l);
                    BaseLayer* layer = layerPtr.get();
                    if (layer) {
                        // Layers that only exist on the master can be hidden from the 3D view.
                        if (UserInterfaceSettings::hideMasterOnlyLayersIn3DView() && layer->existOnMasterOnly())
                            continue;
                        if (layer->ready() && layer->hasTexture() && (layer->alpha() > 0.f)) {
                            if (layer->hasSubLayers()) {
                                // QR operations active: skip the parent, only render sublayers
                                for (const auto& sublayer : layer->getSubLayers()) {
                                    renderLayer(sublayer.get(), eyeMode, angle, viewMatrix, projectionMatrix);
                                }
                            }
                            else {
                                renderLayer(layer, eyeMode, angle, viewMatrix, projectionMatrix);
                            }
                        }
                    }
                }
            }
        }
    }

    // Render foreground image layer
    if (m_foregroundImageLayer && m_foregroundImageLayer->ready() && SyncHelper::instance().variables.alphaFg > 0.f) {
        m_foregroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaFg);
        m_foregroundImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOnFg));
        m_foregroundImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicModeFg));
        renderLayer(m_foregroundImageLayer.get(), eyeMode, angle, viewMatrix, projectionMatrix);
    }

    // Render black dome mask on top of everything (if enabled).
    if (UserInterfaceSettings::hideDomeOverflowIn3DView() && m_domeMaskMesh && m_maskTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_maskTexture);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_meshPrg->bind();

        m_meshPrg->setUniformValue(m_meshEyeModeLoc, 0);
        m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, 0);
        m_meshPrg->setUniformValue(m_meshRoi, 0.f, 0.f, 1.f, 1.f);
        m_meshPrg->setUniformValue(m_meshAlphaLoc, static_cast<float>(UserInterfaceSettings::domeOverflowOpacity()));
        m_meshPrg->setUniformValue(m_meshFlipYLoc, false);
        m_meshPrg->setUniformValue(m_meshOutsideLoc, 0);

        QMatrix4x4 mvpRot = projectionMatrix * viewMatrix;
        // Invert the tilt angle: 180 + angle
        mvpRot.rotate(-(180.f + angle), 1, 0, 0);

        m_meshPrg->setUniformValue(m_meshMatrixLoc, mvpRot);

        m_domeMaskMesh->draw();

        m_meshPrg->release();
        glDisable(GL_BLEND);
    }
}

void LayersRendererQtOpenGLObject::setViewportRect(const QRect& rect) {
    m_viewportRect = rect;
}

void LayersRendererQtOpenGLObject::setItemVisible(bool visible) {
    m_itemVisible = visible;
}

void LayersRendererQtOpenGLObject::setDivideUpdateAndRender(bool divide) {
    m_divideUpdateAndRender = divide;
}

void LayersRendererQtOpenGLObject::reportSwap() {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    // Report swap on all video layers after the frame has been presented to screen.
    // mpv video layers need report_swap after present for correct frame pacing;
    // the BaseLayer default is a safe no-op for non-video layers.
    std::lock_guard<std::mutex> layerAccessLock(LayersRendererQtItem::layerAccessMutex());

    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    if (Application::isCreated() && Application::instance().slidesModel()) {
        // Master slide layers
        auto* master = Application::instance().slidesModel()->masterSlide();
        if (master) {
            int numLayers = master->numberOfLayers();
            for (int l = numLayers - 1; l >= 0; l--) {
                std::shared_ptr<BaseLayer> layerPtr = master->layerShared(l);
                BaseLayer* layer = layerPtr.get();
                if (layer && layer->isEnabled()) {
                    layer->reportSwap();
                }
            }
        }

        // Slides layers
        QList<QSharedPointer<LayersModel>> slidesSnapshot;
        if (Application::instance().slidesModel()->trySnapshotSlides(slidesSnapshot)) {
            for (int s = 0; s < slidesSnapshot.size(); s++) {
                auto& slidePtr = slidesSnapshot[s];
                if (!slidePtr) continue;

                int numLayers = slidePtr->numberOfLayers();
                for (int l = numLayers - 1; l >= 0; l--) {
                    std::shared_ptr<BaseLayer> layerPtr = slidePtr->layerShared(l);
                    BaseLayer* layer = layerPtr.get();
                    if (layer && layer->isEnabled()) {
                        layer->reportSwap();
                    }
                }
            }
        }
    }
}

void LayersRendererQtOpenGLObject::shutdown() {
    m_shuttingDown = true;
    m_itemVisible = false;
    m_mpvObject = nullptr;
    clearLayers();
}

void LayersRendererQtOpenGLObject::init() {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    if (m_initialized) {
        // Recreate meshes here where GL context is guaranteed to be current
        if (m_meshesDirty) {
            m_domeMesh.reset();
            m_domeMaskMesh.reset();
            m_sphereMesh.reset();
            m_domeMesh = std::make_unique<DomeGrid>(float(m_meshRadius) / 100.f, float(m_meshFov), 256, 128);
            m_domeMaskMesh = std::make_unique<DomeGrid>(float(m_meshRadius) / 100.f, 360.f - float(m_meshFov), 256, 128);
            m_sphereMesh = std::make_unique<SphereGrid>(float(m_meshRadius) / 100.f, 256);
            m_meshesDirty = false;
        }

        updateLayers();

        return;
    }


    QSGRendererInterface* rif = m_window->rendererInterface();
    Q_ASSERT(rif->graphicsApi() == QSGRendererInterface::OpenGL);

    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return;

    // Point the private window at the same OpenGL context that is current on
    // the parent window's render thread. This guarantees that any GL objects
    // (textures, FBOs) created inside update() share the same namespace as
    // LayerQtOpenGLObject and LayersRendererQtOpenGLObject, which are also
    // driven by beforeRendering of the same parent window.
    m_window->setGraphicsDevice(QQuickGraphicsDevice::fromOpenGLContext(ctx));

    initializeOpenGLFunctions();

    // Initialize with default values (can be updated later)
    initializeGL();

    m_initialized = true;
    Q_EMIT initialized();
}

void LayersRendererQtOpenGLObject::firstPass() {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown() || !m_initialized || !m_itemVisible) {
        return;
    }
    
    m_window->beginExternalCommands();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_divideUpdateAndRender) {
        // Use the anchored item rect instead of the full window size
        glViewport(m_viewportRect.x(), m_viewportRect.y(),
            m_viewportRect.width(), m_viewportRect.height());

        renderLayers(m_meshAngle, m_viewMatrix, m_projectionMatrix);
    }

    m_window->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!m_divideUpdateAndRender) {
        m_window->resetOpenGLState();
    }
#endif
}
        
void LayersRendererQtOpenGLObject::secondPass() {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown() || !m_initialized || !m_itemVisible) {
        return;
    }

    if (!m_divideUpdateAndRender) {
        return;
    }

    m_window->beginExternalCommands();

    // Use the anchored item rect instead of the full window size
    glViewport(m_viewportRect.x(), m_viewportRect.y(),
               m_viewportRect.width(), m_viewportRect.height());

    renderLayers(m_meshAngle, m_viewMatrix, m_projectionMatrix);

    m_window->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}