/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streamsettings.h"
#include "layersrendererqtitem.h"
#include "application.h"
#include "layersmodel.h"
#include "slidesmodel.h"
#include "gridsettings.h"
#include "presentationsettings.h"
#include "mpvobject.h"
#include "userinterfacesettings.h"
#include <animation/maskglsl.h>
#include <ndi/ndisendermodel.h>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QQuickGraphicsDevice>
#include <QSGSimpleTextureNode>
#include <QtQuick/qsgtexture_platform.h>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/qquickwindow.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
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
uniform mat4 quadMatrix;

out vec2 tr_uv;
out vec2 mask_uv;

void main() {
    gl_Position = quadMatrix * vec4(in_position, 0.0, 1.0);
    mask_uv = in_texCoord;
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
out vec2 mask_uv;

uniform mat4 circularModel;
uniform float circularHalfWidth;
uniform vec2 circularSourceBounds;

void main() {
    float circularX = (circularModel * vec4(in_position, 1.0)).x;
    gl_ClipDistance[0] = circularX + circularHalfWidth;
    gl_ClipDistance[1] = circularHalfWidth - circularX;
    gl_ClipDistance[2] = circularX - circularSourceBounds.x;
    gl_ClipDistance[3] = circularSourceBounds.y - circularX;
    gl_Position = mvp * vec4(in_position, 1.0);
    mask_uv = in_texCoord;
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

// Fragment shaders are split around the cplayPost() definition (see cplay::anim::postGlsl()).
constexpr const char* VideoFragHead = R"(
#version 460 core

uniform sampler2D tex;
uniform float alpha;
uniform bool outside;

in vec2 tr_uv;
in vec3 tr_normals;
in vec2 mask_uv;
out vec4 out_color;
)" CPLAY_MASK_GLSL;

constexpr const char* VideoFragMain = R"(
void main() {
    vec2 texCoods = tr_uv;
    if(outside){
        texCoods = vec2(1.0-tr_uv.x, tr_uv.y);
    }
   
    out_color = cplayPost(texCoods, mask_uv) * vec4(1.0, 1.0, 1.0, alpha * cplayMask(mask_uv));
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

// Shared EAC (equi-angular cubemap) fragment mapping, used by both the perspective and the
// fisheye EAC programs. Combined with one of the main() chunks below to form a full shader.
constexpr const char* EACFragHead = R"(
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
)" CPLAY_MASK_GLSL;

constexpr const char* EACFragCommon = R"(
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

vec4 eac_shade() {
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

    vec4 shaded = cplayPost(uv, uv) * vec4(1.0, 1.0, 1.0, alpha);
    if (maskType != 0) {
        vec3 n = normalize(tr_normal);
        vec2 eqr = vec2(atan(n.x, n.z) / (2.0 * M_PI) + 0.5, asin(clamp(n.y, -1.0, 1.0)) / M_PI + 0.5);
        shaded.a *= cplayMask(eqr);
    }
    return shaded;
}
)";

constexpr const char* EACFragMain = R"(
void main() {
    out_color = eac_shade();
}
)";

// One-pass 180-degree fisheye (fulldome) projection.
// Instead of the perspective camera, every vertex is projected through an equidistant fisheye
// lens centered on the dome zenith (+Y): the angle between the vertex direction and the zenith
// maps linearly to the radius of the output disk, and the azimuth maps to the angle on that
// disk. The vertex *position* (transformed by a model matrix, without any camera) is used as
// the direction, so the same projection works for the dome cap, a full sphere and an arbitrary
// plane. The normalized radius is passed on so the fragment stage can discard everything that
// falls outside the dome cap.
// With lens == 1 (sphere view) the same direction is instead placed on a unit sphere that a
// perspective camera looks at from outside, on the +Z axis at 1 / sphereHorizon radii. The depth
// is mirrored so content reads the same way as from inside the dome (the dome front faces the
// camera), then the sphere rotation turns it. The radius output becomes > 1 on the far side of
// the sphere, so the same fragment discard hides it.
constexpr const char* FisheyeProjection = R"(
#version 460 core

uniform mat4 model;      // layer transform only (rotation/translation), no camera
uniform float halfFov;   // half field of view of the dome, in radians
uniform int lens;                    // 0 = fisheye, 1 = sphere view
uniform mat4 sphereRotation;         // sphere view: rotation of the sphere
uniform mat4 sphereViewProjection;   // sphere view: camera outside the sphere
uniform float sphereHorizon;         // sphere view: z of the visible horizon on the sphere

vec4 fisheye_project(vec3 pos, out float radius) {
    vec3 dir = (model * vec4(pos, 1.0)).xyz;
    float len = length(dir);
    if (len < 1e-6) {
        radius = 0.0;
        return vec4(0.0, 0.0, 0.0, 1.0);
    }
    dir /= len;

    if (lens == 1) {
        vec3 s = mat3(sphereRotation) * vec3(dir.x, dir.y, -dir.z);
        radius = 1.0 + sphereHorizon - s.z;
        return sphereViewProjection * vec4(s, 1.0);
    }

    float phi = acos(clamp(dir.y, -1.0, 1.0));   // angle from the zenith (+Y)
    radius = phi / max(halfFov, 1e-6);

    // Azimuth direction on the disk, matching the DomeGrid texcoord convention
    // (sin(az), -cos(az)) == normalize(dir.xz).
    vec2 azimuth = vec2(dir.x, dir.z);
    float azimuthLen = length(azimuth);
    vec2 p = (azimuthLen > 1e-6) ? (azimuth / azimuthLen) * radius : vec2(0.0);

    return vec4(p, 0.0, 1.0);
}
)";

constexpr const char* FisheyeMeshVert = R"(
layout (location = 0) in vec2 in_texCoord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_position;

out vec2 tr_texCoord;
out float tr_radius;

uniform mat4 circularModel;
uniform float circularHalfWidth;
uniform vec2 circularSourceBounds;

void main() {
    float circularX = (circularModel * vec4(in_position, 1.0)).x;
    gl_ClipDistance[0] = circularX + circularHalfWidth;
    gl_ClipDistance[1] = circularHalfWidth - circularX;
    gl_ClipDistance[2] = circularX - circularSourceBounds.x;
    gl_ClipDistance[3] = circularSourceBounds.y - circularX;
    gl_Position = fisheye_project(in_position, tr_radius);
    tr_texCoord = in_texCoord;
}
)";

// Fisheye vertex stage for EAC content: same lens, but it forwards the direction data the EAC
// fragment mapping needs instead of texture coordinates.
constexpr const char* FisheyeEACMeshVert = R"(
layout (location = 0) in vec2 in_texCoord;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_position;

uniform float scaleToUnitCube;
uniform bool outside;

out vec3 tr_position;
out vec3 tr_normal;
out float tr_radius;

void main() {
    gl_Position = fisheye_project(in_position, tr_radius);
    tr_position = in_position * scaleToUnitCube;

    if(outside)
        tr_normal = -in_normal;
    else
        tr_normal = in_normal;
}
)";

constexpr const char* FisheyeEACFragMain = R"(
in float tr_radius;

void main() {
    // Geometry outside the dome cap has no place on the fulldome disk.
    if (tr_radius > 1.0)
        discard;

    out_color = eac_shade();
}
)";

constexpr const char* FisheyeVideoFragHead = R"(
#version 460 core

uniform sampler2D tex;
uniform int eye;
uniform int stereoscopicMode;
uniform float alpha;
uniform bool outside;
uniform bool flipY;
uniform vec4 roi;

in vec2 tr_texCoord;
in float tr_radius;
out vec4 out_color;
)" CPLAY_MASK_GLSL;

constexpr const char* FisheyeVideoFragMain = R"(
void main() {
    // Geometry outside the dome cap has no place on the fulldome disk.
    if (tr_radius > 1.0)
        discard;

    vec2 uv = tr_texCoord;

    if(flipY)
        uv.y = 1.0 - uv.y;

    uv = (uv * roi.zw) + roi.xy;

    if(outside)
        uv.x = 1.0 - uv.x;

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

    out_color = cplayPost(uv, tr_texCoord) * vec4(1.0, 1.0, 1.0, alpha * cplayMask(tr_texCoord));
}
)";

// -------------------------------------------------------------------------
// LayersRendererQtItem
// -------------------------------------------------------------------------

std::atomic_bool LayersRendererQtItem::s_shuttingDown = false;
std::mutex LayersRendererQtItem::s_layerAccessMutex;

namespace {
// Sphere view camera and interaction. Distances are in sphere radii.
constexpr float kSphereViewFovDeg = 40.0f;
constexpr float kSphereDefaultDistance = 3.5f;
constexpr float kSphereMinDistance = 1.2f;
constexpr float kSphereMaxDistance = 12.0f;
constexpr float kSphereDefaultTiltDeg = 25.0f;     // tips the zenith toward the viewer
constexpr float kSphereDegreesPerPixel = 0.3f;

QQuaternion defaultSphereRotation() {
    return QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, kSphereDefaultTiltDeg);
}
} // namespace

LayersRendererQtItem::LayersRendererQtItem()
    : m_renderer(nullptr), 
    m_timer(nullptr), 
    m_fieldOfView(90.0f),
    m_cameraPosition(0.0f, 0.0f, 0.0f),
    m_cameraEulerRotation(0.0f, 0.0f, 0.0f),
    m_sphereRotation(defaultSphereRotation()),
    m_sphereDistance(kSphereDefaultDistance),
    m_mirrorLink(std::make_shared<LayersRendererMirrorLink>()) {

    m_meshRadius = GridSettings::surfaceRadius();
    m_meshAngle = GridSettings::surfaceAngle();

    // Let the NDI output publish this view when the master view state selects it.
    if (NdiSenderModel::instance())
        NdiSenderModel::instance()->setLayersRendererItem(this);

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

bool LayersRendererQtItem::renderAsFisheye() const {
    return m_renderAsFisheye;
}

void LayersRendererQtItem::setRenderAsFisheye(bool value) {
    if (m_renderAsFisheye == value)
        return;
    m_renderAsFisheye = value;
    // The NDI target is 16:9 with the perspective camera and 1:1 as fisheye.
    updateNdiTarget();
    Q_EMIT renderAsFisheyeChanged();
}

bool LayersRendererQtItem::renderAsSphere() const {
    return m_renderAsSphere;
}

void LayersRendererQtItem::setRenderAsSphere(bool value) {
    if (m_renderAsSphere == value)
        return;
    m_renderAsSphere = value;
    Q_EMIT renderAsSphereChanged();
    Q_EMIT cameraChanged();
}

QQuaternion LayersRendererQtItem::sphereRotation() const {
    return m_sphereRotation;
}

void LayersRendererQtItem::setSphereRotation(const QQuaternion& rotation) {
    if (qFuzzyCompare(m_sphereRotation, rotation))
        return;
    m_sphereRotation = rotation;
    Q_EMIT cameraChanged();
}

float LayersRendererQtItem::sphereDistance() const {
    return m_sphereDistance;
}

void LayersRendererQtItem::setSphereDistance(float distance) {
    distance = std::clamp(distance, kSphereMinDistance, kSphereMaxDistance);
    if (qFuzzyCompare(m_sphereDistance, distance))
        return;
    m_sphereDistance = distance;
    Q_EMIT cameraChanged();
}

void LayersRendererQtItem::rotateSphere(float dx, float dy) {
    // The camera looks along -Z with +Y up: turning about Y moves the facing surface right,
    // turning about X moves it down, so the sphere follows the pointer. Turns are applied in
    // screen space (pre-multiplied), so dragging behaves the same whatever the current pose.
    const QQuaternion turn = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, dx * kSphereDegreesPerPixel)
                           * QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, dy * kSphereDegreesPerPixel);
    setSphereRotation((turn * m_sphereRotation).normalized());
}

void LayersRendererQtItem::zoomSphere(float wheelDelta) {
    // One wheel notch (120) moves the camera about 11% closer (scroll up) or farther.
    setSphereDistance(m_sphereDistance * std::pow(0.999f, wheelDelta));
}

void LayersRendererQtItem::resetSphereView() {
    setSphereRotation(defaultSphereRotation());
    setSphereDistance(kSphereDefaultDistance);
}

#ifdef CLUX_SUPPORT
bool LayersRendererQtItem::cluxPreviewVisible() const {
    return m_cluxPreviewVisible;
}

void LayersRendererQtItem::setCluxPreviewVisible(bool visible) {
    if (m_cluxPreviewVisible == visible)
        return;
    m_cluxPreviewVisible = visible;
    Q_EMIT cluxPreviewVisibleChanged();
}

void LayersRendererQtItem::setCluxPreviewFrame(const QVariantList& frame) {
    // Store the latest live frame; it is pushed to the renderer in sync() so the render thread
    // picks it up on the next frame (same requested-state pattern as the other properties).
    m_cluxPreviewFrame = frame;
}
#endif

void LayersRendererQtItem::setNdiCaptureEnabled(bool enabled) {
    if (m_ndiCaptureEnabled == enabled)
        return;
    m_ndiCaptureEnabled = enabled;
    updateNdiTarget();
}

bool LayersRendererQtItem::isNdiCaptureEnabled() const {
    return m_ndiCaptureEnabled;
}

unsigned int LayersRendererQtItem::ndiTextureId() const {
    return m_renderer ? m_renderer->ndiTextureId() : 0u;
}

int LayersRendererQtItem::ndiWidth() const {
    return m_renderer ? m_renderer->ndiWidth() : 0;
}

int LayersRendererQtItem::ndiHeight() const {
    return m_renderer ? m_renderer->ndiHeight() : 0;
}

std::shared_ptr<LayersRendererMirrorLink> LayersRendererQtItem::mirrorLink() const {
    return m_mirrorLink;
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

// Sphere view camera: on the +Z axis outside the unit sphere, looking at its centre.
void buildSphereCameraMatrices(float distance, float widthPx, float heightPx,
                               QMatrix4x4& viewMatrix, QMatrix4x4& projectionMatrix) {
    const float aspectRatio = (heightPx > 0.0f) ? widthPx / heightPx : 1.0f;
    viewMatrix.lookAt(QVector3D(0.0f, 0.0f, distance), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
    projectionMatrix.perspective(kSphereViewFovDeg, aspectRatio, 0.01f, 100.0f);
}

// Sphere view: where a dome-frame direction lands on the turned unit sphere. Mirrors the
// FisheyeProjection shader (depth mirrored so the content reads as seen from inside).
QVector3D spherePoint(const QVector3D& direction, const QQuaternion& rotation) {
    return rotation.rotatedVector(QVector3D(direction.x(), direction.y(), -direction.z()).normalized());
}

// Azimuth/elevation in degrees of a dome-frame direction (azimuth 0 = -Z).
void directionToAngles(const QVector3D& direction, double& azimuthDeg, double& elevationDeg) {
    constexpr double toDeg = 180.0 / 3.14159265358979323846;
    azimuthDeg = std::atan2(direction.x(), -direction.z()) * toDeg;
    elevationDeg = std::atan2(direction.y(), std::hypot(direction.x(), direction.z())) * toDeg;
}

enum class Lens { Perspective, Fisheye, Sphere };
} // namespace

void LayersRendererQtItem::activeCameraMatrices(float widthPx, float heightPx,
                                                QMatrix4x4& viewMatrix, QMatrix4x4& projectionMatrix) const {
    if (m_renderAsSphere)
        buildSphereCameraMatrices(m_sphereDistance, widthPx, heightPx, viewMatrix, projectionMatrix);
    else
        buildCameraMatrices(m_cameraPosition, m_cameraEulerRotation, m_fieldOfView,
                            widthPx, heightPx, viewMatrix, projectionMatrix);
}

void LayersRendererQtItem::updateCameraMatrices() {
    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    activeCameraMatrices(static_cast<float>(width()), static_cast<float>(height()),
                         viewMatrix, projectionMatrix);

    // The NDI target has its own fixed aspect ratio, so it needs its own projection.
    QMatrix4x4 ndiViewMatrix;
    QMatrix4x4 ndiProjectionMatrix;
    activeCameraMatrices(static_cast<float>(m_ndiWidth), static_cast<float>(m_ndiHeight),
                         ndiViewMatrix, ndiProjectionMatrix);

    // The same for the mirror, which follows the size of the window it is shown in.
    QMatrix4x4 mirrorViewMatrix;
    QMatrix4x4 mirrorProjectionMatrix;
    activeCameraMatrices(static_cast<float>(m_mirrorWidth), static_cast<float>(m_mirrorHeight),
                         mirrorViewMatrix, mirrorProjectionMatrix);

    if (m_renderer) {
        m_renderer->setCameraParams(viewMatrix, projectionMatrix);
        m_renderer->setNdiProjectionMatrix(ndiProjectionMatrix);
        m_renderer->setMirrorProjectionMatrix(mirrorProjectionMatrix);
    }
}

void LayersRendererQtItem::updateMirrorTarget() {
    int width = m_mirrorLink->requestedWidth;
    int height = m_mirrorLink->requestedHeight;

    // The fisheye (fulldome) image stays square, as on screen and over NDI.
    if (m_renderAsFisheye)
        width = height = std::min(width, height);
    if (width <= 0 || height <= 0)
        width = height = 0;

    m_mirrorWidth = width;
    m_mirrorHeight = height;

    if (m_renderer)
        m_renderer->setMirrorSize(m_mirrorWidth, m_mirrorHeight);
}

void LayersRendererQtItem::updateNdiTarget() {
    // 0 = 2K, 1 = 4K, 2 = 6K, 3 = 8K. The perspective camera and the sphere view keep a 16:9
    // aspect ratio, the fisheye (fulldome) camera a square one.
    const int tier = std::clamp(StreamSettings::ndiResolution3DView(), 0, 3);
    const int scale = tier + 1;

    const int width = m_renderAsFisheye ? 2048 * scale : 1920 * scale;
    const int height = m_renderAsFisheye ? 2048 * scale : 1080 * scale;

    if (m_ndiWidth != width || m_ndiHeight != height) {
        m_ndiWidth = width;
        m_ndiHeight = height;
        updateCameraMatrices();
    }

    if (m_renderer) {
        m_renderer->setNdiCaptureSize(m_ndiWidth, m_ndiHeight);
        m_renderer->setNdiCaptureEnabled(m_ndiCaptureEnabled);
    }
}

namespace {
constexpr double kPi = 3.14159265358979323846;
// Sphere/dome drag sensitivity in degrees per pixel (matches the QML camera orbitSpeed).
constexpr double kDragDegreesPerPixel = 0.25;

// Layer-drag operations for the selected layer in the view. Each modifier combo (Ctrl/Alt/Shift
// + left drag) is assigned one of them via PresentationSettings::ctrlDragLayerAction /
// altDragLayerAction / shiftDragLayerAction; it decides which grid parameters a drag changes.
constexpr int kDragAimElevationAzimuth = 0;   // default: aim at the pointer
constexpr int kDragAimElevationOnly = 1;
constexpr int kDragAimAzimuthOnly = 2;
constexpr int kDragMoveHorizontalVertical = 3;
constexpr int kDragMoveHorizontalOnly = 4;
constexpr int kDragMoveVerticalOnly = 5;
constexpr int kDragResizePlaneSize = 6;       // scale width & height proportionally
constexpr int kDragMovePlaneDistance = 7;     // move the plane toward/away from the camera

// Plane resize/distance drag limits, matching the Grid Parameters dialog ranges (cm).
constexpr double kPlaneSizeMinCm = 1.0;
constexpr double kPlaneSizeMaxCm = 2000.0;
constexpr double kPlaneDistanceMinCm = 0.0;
constexpr double kPlaneDistanceMaxCm = 2000.0;

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

bool LayersRendererQtItem::sphereDirectionAtPoint(float x, float y, QVector3D& direction) const {
    if (width() <= 0.0 || height() <= 0.0)
        return false;

    const float ndcX = 2.0f * x / static_cast<float>(width()) - 1.0f;
    const float ndcY = 1.0f - 2.0f * y / static_cast<float>(height());

    QMatrix4x4 viewMatrix;
    QMatrix4x4 projectionMatrix;
    buildSphereCameraMatrices(m_sphereDistance, static_cast<float>(width()), static_cast<float>(height()),
                              viewMatrix, projectionMatrix);
    bool invertible = false;
    const QMatrix4x4 invVP = (projectionMatrix * viewMatrix).inverted(&invertible);
    if (!invertible)
        return false;

    const QVector3D origin(0.0f, 0.0f, m_sphereDistance);
    const QVector3D ray = (invVP.map(QVector3D(ndcX, ndcY, 1.0f)) - invVP.map(QVector3D(ndcX, ndcY, -1.0f))).normalized();

    // Nearest intersection with the unit sphere.
    const float b = QVector3D::dotProduct(origin, ray);
    const float disc = b * b - (QVector3D::dotProduct(origin, origin) - 1.0f);
    if (disc < 0.0f)
        return false;   // the pixel is beside the sphere
    const QVector3D hit = origin + ray * (-b - std::sqrt(disc));

    // Undo the sphere rotation and the depth mirroring of spherePoint().
    const QVector3D unrotated = m_sphereRotation.inverted().rotatedVector(hit);
    direction = QVector3D(unrotated.x(), unrotated.y(), -unrotated.z()).normalized();
    return true;
}

bool LayersRendererQtItem::aimAtScreenPointLocked(const BaseLayer* layer, float x, float y, double& azimuthDeg, double& elevationDeg) const {
    if (m_renderAsSphere) {
        // Every direction is shown on the sphere surface, so the pointer aims along the one under it.
        QVector3D direction;
        if (!sphereDirectionAtPoint(x, y, direction))
            return false;
        directionToAngles(direction, azimuthDeg, elevationDeg);
        return true;
    }
    return domeAnglesAtScreenPoint(x, y, layer->planeDistance() / 100.0, azimuthDeg, elevationDeg);
}

bool LayersRendererQtItem::domeAnglesAtScreenPoint(float x, float y, double radiusM, double& azimuthDeg, double& elevationDeg) const {
    QVector3D rayOrigin, rayDir;
    if (!rayFromScreenPoint(x, y, rayOrigin, rayDir))
        return false;

    // World -> dome frame: the plane transform starts with R_x(-meshAngle).
    QMatrix4x4 toDome;
    toDome.rotate(float(m_meshAngle), 1.0f, 0.0f, 0.0f);
    const QVector3D o = toDome.map(rayOrigin);
    const QVector3D dirDome = toDome.mapVector(rayDir).normalized();   // direction: linear part only

    // Intersect the ray with a sphere of radius radiusM centered at the origin; fall back
    // to the closest approach point when it misses so aiming always works.
    const double r = radiusM;
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
            // index is a row of the selected slide, which also lists referenced layers.
            const std::vector<std::shared_ptr<BaseLayer>> rows = slide ? slide->renderOrderShared()
                                                                       : std::vector<std::shared_ptr<BaseLayer>>();
            if (index >= 0 && index < static_cast<int>(rows.size())) {
                std::shared_ptr<BaseLayer> candidate = rows[index];
                if (candidate && candidate->gridMode() != static_cast<uint8_t>(BaseLayer::None)) {
                    layer = std::move(candidate);
                    resolved = index;
                }
            }
        }
    }
    setSelectedPlaneLayer(std::move(layer), resolved);
}

bool LayersRendererQtItem::selectedLayerStillValidLocked() {
    if (!m_selectedPlaneLayer)
        return false;

    // The layer may have been removed or the slide switched since it was selected.
    auto* slides = Application::isCreated() ? Application::instance().slidesModel() : nullptr;
    LayersModel* slide = slides ? slides->selectedSlide() : nullptr;
    const bool valid = slide && slide->containsLayer(m_selectedPlaneLayer.get());
    if (!valid)
        setSelectedPlaneLayer(nullptr, -1);   // stale selection: clear it
    return valid;
}

double LayersRendererQtItem::planeMoveCmPerPixelLocked() const {
    // Sensitivity in cm per pixel, derived so the layer follows the pointer on screen:
    // one pixel subtends 2*tan(fov/2)/height radians at the plane's distance from the
    // camera (which sits at the dome centre). Fall back to the mesh radius when the
    // plane is placed at the centre.
    const double distCm = std::abs(m_selectedPlaneLayer->planeDistance());
    const double refDistCm = (distCm > 1e-6) ? distCm : m_meshRadius;
    const float fovDeg = (m_fieldOfView > 1.0f && m_fieldOfView < 179.0f) ? m_fieldOfView : 90.0f;
    const double hPx = std::max(1.0, static_cast<double>(height()));
    return refDistCm * 2.0 * std::tan(fovDeg * kPi / 360.0) / hPx;
}

namespace {
// Project a plane-local point (metres) through the same transform the renderer uses for the
// selected plane, into the item's pixel space. Mirrors renderLayers(): the perspective camera
// runs the model matrix through the view/projection, while the fisheye (fulldome) lens projects
// the model-space direction around the dome zenith (+Y) with an equidistant radius (see the
// FisheyeProjection shader), and the sphere view places the direction on the turned sphere that
// the sphere camera (viewMatrix/projectionMatrix) looks at. Returns false when the point is behind
// the camera (perspective) or degenerate (fisheye at the zenith), so the caller can keep a safe
// fallback.
bool projectPlaneLocalToPixel(Lens lens, double meshFovDeg, const QQuaternion& sphereRotation,
                              const QMatrix4x4& model,
                              const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix,
                              double widthPx, double heightPx, const QVector3D& localM,
                              QPointF& out) {
    QVector3D p = model.map(localM);   // model space (metres)
    if (lens == Lens::Sphere) {
        const float len = p.length();
        if (len < 1e-6f)
            return false;
        p = spherePoint(p / len, sphereRotation);
    }
    if (lens == Lens::Fisheye) {
        const double len = p.length();
        if (len < 1e-6)
            return false;
        const QVector3D dir = p / float(len);
        const double halfFov = std::max(1e-6, glm::radians(meshFovDeg * 0.5));
        const double phi = std::acos(std::clamp(double(dir.y()), -1.0, 1.0));
        const double radius = phi / halfFov;
        const double azLen = std::hypot(double(dir.x()), double(dir.z()));
        const double ndcX = (azLen > 1e-6) ? (double(dir.x()) / azLen) * radius : 0.0;
        const double ndcY = (azLen > 1e-6) ? (double(dir.z()) / azLen) * radius : 0.0;
        out = QPointF((ndcX * 0.5 + 0.5) * widthPx, (0.5 - ndcY * 0.5) * heightPx);
        return true;
    }
    const QVector4D clip = projectionMatrix * viewMatrix * QVector4D(p, 1.0f);
    if (clip.w() <= 1e-6)
        return false;   // behind the camera
    const double ndcX = clip.x() / clip.w();
    const double ndcY = clip.y() / clip.w();
    out = QPointF((ndcX * 0.5 + 0.5) * widthPx, (0.5 - ndcY * 0.5) * heightPx);
    return true;
}
} // namespace

QRectF LayersRendererQtItem::fisheyeSquare() const {
    const double side = std::min(width(), height());
    return QRectF((width() - side) / 2.0, (height() - side) / 2.0, side, side);
}

QVariantMap LayersRendererQtItem::projectDomePoint(const QVector3D& point) const {
    QVariantMap result{{QStringLiteral("visible"), false}, {QStringLiteral("x"), 0.0}, {QStringLiteral("y"), 0.0}};
    if (width() <= 0.0 || height() <= 0.0)
        return result;

    if (m_renderAsFisheye) {
        // Mirrors the FisheyeProjection shader: equidistant around the zenith, in the square image area.
        const float len = point.length();
        if (len < 1e-6f)
            return result;
        const QVector3D dir = point / len;
        const double halfFov = std::max(1e-6, m_meshFov * 0.5 * kPi / 180.0);
        const double radius = std::acos(std::clamp(double(dir.y()), -1.0, 1.0)) / halfFov;
        if (radius > 1.0)
            return result; // outside the dome image
        const double azLen = std::hypot(double(dir.x()), double(dir.z()));
        const double ndcX = azLen > 1e-6 ? double(dir.x()) / azLen * radius : 0.0;
        const double ndcY = azLen > 1e-6 ? double(dir.z()) / azLen * radius : 0.0;
        const QRectF sq = fisheyeSquare();
        result[QStringLiteral("visible")] = true;
        result[QStringLiteral("x")] = sq.x() + (ndcX * 0.5 + 0.5) * sq.width();
        result[QStringLiteral("y")] = sq.y() + (0.5 - ndcY * 0.5) * sq.height();
        return result;
    }

    QMatrix4x4 viewMatrix, projectionMatrix;
    activeCameraMatrices(static_cast<float>(width()), static_cast<float>(height()), viewMatrix, projectionMatrix);
    QVector3D scenePoint;
    if (m_renderAsSphere) {
        // Mirrors the sphere view of the FisheyeProjection shader (no dome tilt, far side hidden).
        const float len = point.length();
        if (len < 1e-6f)
            return result;
        scenePoint = spherePoint(point / len, m_sphereRotation);
        if (scenePoint.z() <= 1.0f / std::max(1.0f, m_sphereDistance))
            return result; // on the far side of the sphere
    } else {
        QMatrix4x4 tilt;
        tilt.rotate(float(-m_meshAngle), 1.0f, 0.0f, 0.0f);
        scenePoint = tilt.map(point);
    }
    const QVector4D clip = projectionMatrix * viewMatrix * QVector4D(scenePoint, 1.0f);
    if (clip.w() <= 1e-6f)
        return result; // behind the camera
    const double ndcX = clip.x() / clip.w();
    const double ndcY = clip.y() / clip.w();
    result[QStringLiteral("visible")] = std::abs(ndcX) <= 1.0 && std::abs(ndcY) <= 1.0;
    result[QStringLiteral("x")] = (ndcX * 0.5 + 0.5) * width();
    result[QStringLiteral("y")] = (0.5 - ndcY * 0.5) * height();
    return result;
}

QVariantMap LayersRendererQtItem::projectDomeDirection(double azimuthDeg, double elevationDeg) const {
    const double az = azimuthDeg * kPi / 180.0;
    const double el = elevationDeg * kPi / 180.0;
    const QVector3D dir(float(std::sin(az) * std::cos(el)), float(std::sin(el)), float(-std::cos(az) * std::cos(el)));
    return projectDomePoint(dir * float(std::max(0.01, m_meshRadius / 100.0)));
}

cplay::circular::Mapping LayersRendererQtItem::circularScreenMapping() const {
    return {GridSettings::circularEnabled(), GridSettings::circularSurfaceWidthCM(),
            GridSettings::circularSeamDegrees(), GridSettings::circularClockwise()};
}

double LayersRendererQtItem::circularScreenDistanceCm() const {
    // The circular screen is drawn at the distance of the circular layers: the selected layer
    // when it is one, else the first circular layer of the current slide, else the dome radius.
    std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
    if (!LayersRendererQtItem::isShuttingDown()) {
        if (m_selectedPlaneLayer && m_selectedPlaneLayer->circularPlaneActive())
            return m_selectedPlaneLayer->renderPlaneDistance();
        auto* slides = Application::isCreated() ? Application::instance().slidesModel() : nullptr;
        LayersModel* slide = slides ? slides->selectedSlide() : nullptr;
        if (slide)
            for (const auto& layer : slide->renderOrderShared())
                if (layer && layer->circularPlaneActive())
                    return layer->renderPlaneDistance();
    }
    return m_meshRadius;
}

QVariantMap LayersRendererQtItem::projectCircularPoint(double degrees, double heightCm) const {
    const cplay::circular::Mapping mapping = circularScreenMapping();
    if (!mapping.valid())
        return QVariantMap{{QStringLiteral("visible"), false}, {QStringLiteral("x"), 0.0}, {QStringLiteral("y"), 0.0}};
    const double xCm = mapping.wrapCm(mapping.angleToCm(degrees));
    return projectDomePoint(QVector3D(float(xCm / 100.0), float(heightCm / 100.0), float(-circularScreenDistanceCm() / 100.0)));
}

QVariantMap LayersRendererQtItem::circularPointAtPoint(float x, float y) const {
    QVariantMap result{{QStringLiteral("valid"), false}, {QStringLiteral("degrees"), 0.0}, {QStringLiteral("height"), 0.0}};
    const cplay::circular::Mapping mapping = circularScreenMapping();
    if (!mapping.valid())
        return result;

    // View ray in the dome frame (no tilt), from the camera (perspective) or the dome centre
    // (fisheye and sphere view, which both show directions from the centre).
    QVector3D origin, dir;
    if (m_renderAsFisheye || m_renderAsSphere) {
        const QVariantMap hit = domeDirectionAtPoint(x, y);
        if (!hit.value(QStringLiteral("valid")).toBool())
            return result;
        const double az = hit.value(QStringLiteral("azimuth")).toDouble() * kPi / 180.0;
        const double el = hit.value(QStringLiteral("elevation")).toDouble() * kPi / 180.0;
        dir = QVector3D(float(std::sin(az) * std::cos(el)), float(std::sin(el)), float(-std::cos(az) * std::cos(el)));
    } else {
        QVector3D rayOrigin, rayDir;
        if (!rayFromScreenPoint(x, y, rayOrigin, rayDir))
            return result;
        QMatrix4x4 toDome;
        toDome.rotate(float(m_meshAngle), 1.0f, 0.0f, 0.0f);
        origin = toDome.map(rayOrigin);
        dir = toDome.mapVector(rayDir).normalized();
    }

    // Intersect with the screen plane z = -distance.
    const float planeZ = float(-circularScreenDistanceCm() / 100.0);
    if (std::abs(dir.z()) < 1e-6f)
        return result;
    const float t = (planeZ - origin.z()) / dir.z();
    if (t <= 0.f)
        return result; // the screen is behind the ray
    const QVector3D p = origin + dir * t;
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("degrees")] = std::fmod(mapping.cmToAngle(mapping.wrapCm(p.x() * 100.0)) + 360.0, 360.0);
    result[QStringLiteral("height")] = p.y() * 100.0;
    return result;
}

QVariantList LayersRendererQtItem::audioSharesForSelectedLayer() const {
    // Speaker array share of each speaker for the layer selected in the Layers panel, in
    // percent: of the power with constant power on, else of the amplitude. Empty without a
    // selected 3D layer.
    QVariantList shares;
    cplay::audio::SpatialSource source;
    {
        std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
        if (LayersRendererQtItem::isShuttingDown() || !m_selectedPlaneLayer)
            return shares;
        source = m_selectedPlaneLayer->audioSpatialSource();
    }
    const cplay::audio::SpeakerSetup setup = cplay::audio::speakerSetup();
    float gains[cplay::audio::kMaxArraySpeakers];
    if (setup.arrayCount <= 0 || !source.useSpatial)
        return shares; // opted out: plays with the standard mapping
    cplay::audio::arraySpeakerGains(source, setup, gains);
    for (int i = 0; i < setup.arrayCount; ++i)
        shares.append(100.0 * (setup.constantPower ? double(gains[i]) * gains[i] : double(gains[i])));
    return shares;
}

QVariantMap LayersRendererQtItem::domeDirectionAtPoint(float x, float y) const {
    QVariantMap result{{QStringLiteral("valid"), false}, {QStringLiteral("azimuth"), 0.0}, {QStringLiteral("elevation"), 0.0}};
    double azimuthDeg = 0.0, elevationDeg = 0.0;
    if (m_renderAsFisheye) {
        const QRectF sq = fisheyeSquare();
        if (sq.width() <= 0.0)
            return result;
        const double ndcX = (x - sq.center().x()) / (sq.width() / 2.0);
        const double ndcY = (sq.center().y() - y) / (sq.height() / 2.0);
        const double radius = std::hypot(ndcX, ndcY);
        if (radius > 1.0)
            return result; // outside the dome image
        const double phi = radius * m_meshFov * 0.5 * kPi / 180.0;
        const double s = radius > 1e-9 ? std::sin(phi) / radius : 0.0;
        const double dx = ndcX * s, dy = std::cos(phi), dz = ndcY * s;
        azimuthDeg = std::atan2(dx, -dz) * 180.0 / kPi;
        elevationDeg = std::atan2(dy, std::hypot(dx, dz)) * 180.0 / kPi;
    } else if (m_renderAsSphere) {
        QVector3D direction;
        if (!sphereDirectionAtPoint(x, y, direction))
            return result; // beside the sphere
        directionToAngles(direction, azimuthDeg, elevationDeg);
    } else if (!domeAnglesAtScreenPoint(x, y, std::max(0.01, m_meshRadius / 100.0), azimuthDeg, elevationDeg)) {
        return result;
    }
    result[QStringLiteral("valid")] = true;
    result[QStringLiteral("azimuth")] = azimuthDeg;
    result[QStringLiteral("elevation")] = elevationDeg;
    return result;
}

void LayersRendererQtItem::planeMoveAxesLocked(double& rightPerCmH, double& downPerCmH,
                                               double& rightPerCmV, double& downPerCmV) const {
    // Neutral fronto-parallel fallback (a plane facing the camera at the mesh radius): a
    // horizontal drag maps 1:1 to plane horizontal and a vertical drag to plane vertical.
    const float fovDeg = (m_fieldOfView > 1.0f && m_fieldOfView < 179.0f) ? m_fieldOfView : 90.0f;
    const double hPx = std::max(1.0, static_cast<double>(height()));
    const double wPx = std::max(1.0, static_cast<double>(width()));
    const double distCm = std::abs(m_selectedPlaneLayer->planeDistance());
    const double refDistCm = (distCm > 1e-6) ? distCm : m_meshRadius;
    const double tanHalf = std::tan(fovDeg * kPi / 360.0);
    const double pxPerCm = hPx / (2.0 * tanHalf * refDistCm);
    rightPerCmH = pxPerCm;
    downPerCmH = 0.0;
    rightPerCmV = 0.0;
    downPerCmV = -pxPerCm;

    // Rebuild the plane's model matrix exactly as the renderer does, including the dome-angle
    // tilt that the fisheye and sphere lenses deliberately omit (their output is locked to the zenith).
    const Lens lens = m_renderAsSphere ? Lens::Sphere : m_renderAsFisheye ? Lens::Fisheye : Lens::Perspective;
    QMatrix4x4 model;
    if (lens == Lens::Perspective)
        model.rotate(float(-m_meshAngle), 1.0f, 0.0f, 0.0f);
    model.rotate(float(m_selectedPlaneLayer->planeAzimuth()), 0.0f, -1.0f, 0.0f);
    model.rotate(float(m_selectedPlaneLayer->planeElevation()), 1.0f, 0.0f, 0.0f);
    model.rotate(float(m_selectedPlaneLayer->planeRoll()), 0.0f, 0.0f, 1.0f);
    model.translate(float(m_selectedPlaneLayer->planeHorizontal()) / 100.0f,
                    float(m_selectedPlaneLayer->planeVertical()) / 100.0f,
                    float(-m_selectedPlaneLayer->planeDistance()) / 100.0f);

    if (m_selectedPlaneLayer->circularPlaneActive()) {
        QMatrix4x4 circularModel(glm::value_ptr(m_selectedPlaneLayer->circularPlaneModel(false)));
        model.setToIdentity();
        if (lens == Lens::Perspective) model.rotate(float(-m_meshAngle), 1, 0, 0);
        model *= circularModel.transposed();
    }

    QMatrix4x4 viewMatrix, projectionMatrix;
    if (lens != Lens::Fisheye)
        activeCameraMatrices(static_cast<float>(width()), static_cast<float>(height()),
                             viewMatrix, projectionMatrix);

    if (m_selectedPlaneLayer->circularPlaneActive()) {
        const auto mapping = m_selectedPlaneLayer->circularMapping();
        const auto copies = cplay::circular::copies(m_selectedPlaneLayer->circularPlaneModel(false),
                                                   m_selectedPlaneLayer->planeActualSize(), mapping.widthCm);
        double best = std::numeric_limits<double>::max();
        for (const auto &copy : copies) {
            QMatrix4x4 candidate(glm::value_ptr(copy.model)); candidate = candidate.transposed();
            QMatrix4x4 tilt; if (lens == Lens::Perspective) tilt.rotate(float(-m_meshAngle), 1, 0, 0);
            candidate = tilt * candidate;
            QPointF centre;
            if (!projectPlaneLocalToPixel(lens, m_meshFov, m_sphereRotation, candidate, viewMatrix,
                                          projectionMatrix, wPx, hPx, QVector3D(), centre)) continue;
            const double dx = centre.x() - m_dragStartX, dy = centre.y() - m_dragStartY;
            if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; model = candidate; }
        }
    }

    // Sample the on-screen position of the plane centre and of two points one centimetre along
    // its local X (plane-horizontal) and Y (plane-vertical) axes. The differences give the pixels
    // per cm along each axis for the *active* projection, so the drag follows the pointer for any
    // plane orientation, camera pose, or lens (perspective, fisheye or sphere) without special-casing.
    QVector3D horizontalAxis(0.01f, 0, 0), verticalAxis(0, 0.01f, 0);
    if (m_selectedPlaneLayer->circularPlaneActive()) {
        QMatrix4x4 orientation;
        orientation.rotate(float(m_selectedPlaneLayer->planeAzimuth()), 0, -1, 0);
        orientation.rotate(float(m_selectedPlaneLayer->planeElevation()), 1, 0, 0);
        orientation.rotate(float(m_selectedPlaneLayer->planeRoll()), 0, 0, 1);
        horizontalAxis = orientation.inverted().mapVector(horizontalAxis);
        verticalAxis = orientation.inverted().mapVector(verticalAxis);
    }
    QPointF c0, cH, cV;
    const bool ok0 = projectPlaneLocalToPixel(lens, m_meshFov, m_sphereRotation, model, viewMatrix,
                                              projectionMatrix, wPx, hPx, QVector3D(0, 0, 0), c0);
    const bool okH = projectPlaneLocalToPixel(lens, m_meshFov, m_sphereRotation, model, viewMatrix,
                                              projectionMatrix, wPx, hPx, horizontalAxis, cH);
    const bool okV = projectPlaneLocalToPixel(lens, m_meshFov, m_sphereRotation, model, viewMatrix,
                                              projectionMatrix, wPx, hPx, verticalAxis, cV);
    if (!ok0 || !okH || !okV)
        return;   // degenerate projection: keep the neutral fallback above

    auto clampScale = [](double s) { return std::clamp(s, -1e6, 1e6); };
    rightPerCmH = clampScale(double(cH.x() - c0.x()));
    downPerCmH = clampScale(double(cH.y() - c0.y()));
    rightPerCmV = clampScale(double(cV.x() - c0.x()));
    downPerCmV = clampScale(double(cV.y() - c0.y()));
}

bool LayersRendererQtItem::beginLayerDrag(int action, float x, float y) {
    bool ok = false;
    double hitAz = 0.0, hitEl = 0.0;
    uint8_t mode = 0;
    glm::vec3 startRotate{};
    double startWidthCm = 0.0, startHeightCm = 0.0, startDistanceCm = 0.0;
    {
        std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
        if (!LayersRendererQtItem::isShuttingDown() && selectedLayerStillValidLocked()) {
            mode = m_selectedPlaneLayer->gridMode();
            startRotate = m_selectedPlaneLayer->rotate();
            if (mode == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
                m_selectedPlaneLayer->beginPoseEdit();
                m_poseEditLayer = m_selectedPlaneLayer;
                // Flat layer: no hit testing, wherever the pointer is it aims at the layer.
                ok = aimAtScreenPointLocked(m_selectedPlaneLayer.get(), x, y, hitAz, hitEl);
                startWidthCm = m_selectedPlaneLayer->circularPlaneActive()
                    ? m_selectedPlaneLayer->planeWidthDegrees() : m_selectedPlaneLayer->planeWidth();
                startHeightCm = m_selectedPlaneLayer->planeHeight();
                startDistanceCm = m_selectedPlaneLayer->planeDistance();
                if (!ok || (action == kDragResizePlaneSize && (startWidthCm <= 0.0 || startHeightCm <= 0.0)))
                    m_selectedPlaneLayer->endPoseEdit();
            } else {
                // Sphere/dome layer: rotation follows the pointer delta from this point.
                ok = true;
            }
        }
    }

    m_planeDragActive = false;
    if (!ok)
        return false;
    if (mode == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
        // Resizing needs a valid size to scale about.
        if (action == kDragResizePlaneSize && (startWidthCm <= 0.0 || startHeightCm <= 0.0))
            return false;   // no valid plane size yet: QML falls back to orbiting
    }

    m_planeDragAction = action;
    if (mode == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
        // Store the grab offset so the layer does not jump to the cursor on press.
        m_grabAzimuthOffsetDeg = normalizeAngleDegrees(m_selectedPlaneLayer->planeAzimuth() - hitAz);
        m_grabElevationOffsetDeg = m_selectedPlaneLayer->planeElevation() - hitEl;

        // The move/resize/distance operations map the pointer delta since press to plane
        // parameters, so remember the baselines like sphere/dome drags do.
        m_dragStartX = x;
        m_dragStartY = y;
        m_planeDragStartCentreDegrees = m_selectedPlaneLayer->planeCentreDegrees();
        m_planeDragStartHorizontalCm = m_selectedPlaneLayer->planeHorizontalForRendering(false);
        m_planeDragStartVerticalCm = m_selectedPlaneLayer->planeVertical();
        m_planeDragStartWidthCm = startWidthCm;
        m_planeDragStartHeightCm = startHeightCm;
        m_planeDragStartDistanceCm = startDistanceCm;

        // Sensitivity in cm per pixel for the resize/distance drags, plus the screen-space
        // projection of the plane's own axes for the horizontal/vertical move drags, so the layer
        // follows the pointer exactly for any plane orientation or camera pose.
        m_planeMoveCmPerPixel = planeMoveCmPerPixelLocked();
        planeMoveAxesLocked(m_planeDragRightPerCmH, m_planeDragDownPerCmH,
                            m_planeDragRightPerCmV, m_planeDragDownPerCmV);
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

    const uint8_t mode = m_selectedPlaneLayer->gridMode();
    if (mode != static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
        // Sphere/dome: X drag -> yaw; Y drag -> pitch as well for spheres. Domes keep their
        // pitch and rotate in yaw only, regardless of which operation the modifier combo is
        // configured for. The content follows the pointer (a rightward/upward drag moves the
        // layer's front right/up on screen).
        const bool domeYawOnly = (mode == static_cast<uint8_t>(BaseLayer::GridMode::Dome));
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
        return changed;
    }

    switch (m_planeDragAction) {
    case kDragMoveHorizontalVertical:
    case kDragMoveHorizontalOnly:
    case kDragMoveVerticalOnly: {
        // Horizontal/vertical move operations: map the pointer delta since press to plane
        // horizontal/vertical offsets. The content follows the pointer — dragging right moves
        // the layer right, dragging up moves it upward (screen y points down).
        const bool hEnabled = (m_planeDragAction == kDragMoveHorizontalVertical || m_planeDragAction == kDragMoveHorizontalOnly);
        const bool vEnabled = (m_planeDragAction == kDragMoveHorizontalVertical || m_planeDragAction == kDragMoveVerticalOnly);

        // The plane's own axes project onto the screen as (hRx, hRy) for horizontal and
        // (vRx, vRy) for vertical (pixels per cm, dx right / dy down). Solve the pointer delta
        // for the parameter change that reproduces it, so the layer tracks the pointer for any
        // plane orientation instead of a single fixed cm-per-pixel scale.
        const double dx = x - m_dragStartX;
        const double dy = y - m_dragStartY;
        const double hRx = m_planeDragRightPerCmH, vRx = m_planeDragRightPerCmV;
        const double hRy = m_planeDragDownPerCmH, vRy = m_planeDragDownPerCmV;

        double newH = m_planeDragStartHorizontalCm;
        double newV = m_planeDragStartVerticalCm;
        const double det = hRx * vRy - vRx * hRy;
        if (hEnabled && vEnabled && std::abs(det) > 1e-9) {
            // Exact 2x2 solve: both parameters free to match the pointer on screen.
            newH += (vRy * dx - vRx * dy) / det;
            newV += (hRx * dy - hRy * dx) / det;
        } else {
            // One axis (or a degenerate projection): least-squares fit of the pointer delta onto
            // each enabled axis independently.
            if (hEnabled) {
                const double nn = hRx * hRx + hRy * hRy;
                if (nn > 1e-12)
                    newH += (hRx * dx + hRy * dy) / nn;
            }
            if (vEnabled) {
                const double nn = vRx * vRx + vRy * vRy;
                if (nn > 1e-12)
                    newV += (vRx * dx + vRy * dy) / nn;
            }
        }

        const bool hChanged = hEnabled && std::abs(newH - m_selectedPlaneLayer->planeHorizontalForRendering(false)) > 1e-4;
        const bool vChanged = vEnabled && std::abs(newV - m_selectedPlaneLayer->planeVertical()) > 1e-4;
        if (hChanged || vChanged) {
            if (hChanged) {
                if (m_selectedPlaneLayer->circularPlaneActive()) {
                    const auto mapping = m_selectedPlaneLayer->circularMapping();
                    const double deltaCm = newH - m_planeDragStartHorizontalCm;
                    m_selectedPlaneLayer->setPlaneCentreDegrees(m_planeDragStartCentreDegrees
                        + mapping.direction() * deltaCm * 360.0 / mapping.widthCm);
                } else m_selectedPlaneLayer->setPlaneHorizontal(newH);
            }
            if (vChanged)
                m_selectedPlaneLayer->setPlaneVertical(newV);
            changed = true;
        }
        break;
    }
    case kDragResizePlaneSize: {
        // Resize: the vertical pointer delta scales width and height proportionally about the
        // size at press time, so dragging down by one on-screen plane height doubles it.
        const double dyCm = (y - m_dragStartY) * m_planeMoveCmPerPixel;
        const double factor = 1.0 + dyCm / m_planeDragStartHeightCm;
        if (m_selectedPlaneLayer->circularPlaneActive()) {
            m_selectedPlaneLayer->setPlaneWidthDegrees(std::clamp(m_planeDragStartWidthCm * factor, 0.01, 360.0));
            m_selectedPlaneLayer->setPlaneHeight(std::clamp(m_planeDragStartHeightCm * factor, kPlaneSizeMinCm, kPlaneSizeMaxCm));
            changed = true;
            break;
        }
        const double newW = std::clamp(m_planeDragStartWidthCm * factor, kPlaneSizeMinCm, kPlaneSizeMaxCm);
        const double newH = std::clamp(m_planeDragStartHeightCm * factor, kPlaneSizeMinCm, kPlaneSizeMaxCm);
        if (newW != m_selectedPlaneLayer->planeWidth() || newH != m_selectedPlaneLayer->planeHeight()) {
            m_selectedPlaneLayer->setPlaneSize(glm::vec2(static_cast<float>(newW), static_cast<float>(newH)),
                                               m_selectedPlaneLayer->planeAspectRatio());
            changed = true;
        }
        break;
    }
    case kDragMovePlaneDistance: {
        // Dragging down moves the plane away from the camera, dragging up brings it closer. The
        // sensitivity matches the pointer on screen at the plane's depth, like the horizontal/
        // vertical move operations do.
        const double newDist = std::clamp(m_planeDragStartDistanceCm + (y - m_dragStartY) * m_planeMoveCmPerPixel,
                                          kPlaneDistanceMinCm, kPlaneDistanceMaxCm);
        if (std::abs(newDist - m_selectedPlaneLayer->planeDistance()) > 1e-4) {
            m_selectedPlaneLayer->setPlaneDistance(newDist);
            changed = true;
        }
        break;
    }
    default: {   // aim operations (kDragAimElevationAzimuth is the default), or unknown values
        double targetAz = 0.0, targetEl = 0.0;
        if (!aimAtScreenPointLocked(m_selectedPlaneLayer.get(), x, y, targetAz, targetEl))
            return false;

        // Elevation & azimuth is the default; the other aim operations change only one of them
        // and leave the rest untouched.
        const bool azEnabled = (m_planeDragAction == kDragAimElevationAzimuth || m_planeDragAction == kDragAimAzimuthOnly);
        const bool elEnabled = (m_planeDragAction == kDragAimElevationAzimuth || m_planeDragAction == kDragAimElevationOnly);

        double newAz = m_selectedPlaneLayer->planeAzimuth();
        double newEl = m_selectedPlaneLayer->planeElevation();
        if (azEnabled)
            newAz = normalizeAngleDegrees(targetAz + m_grabAzimuthOffsetDeg);
        if (elEnabled)
            newEl = targetEl + m_grabElevationOffsetDeg;

        const bool azChanged = azEnabled && std::abs(newAz - m_selectedPlaneLayer->planeAzimuth()) > 1e-4;
        const bool elChanged = elEnabled && std::abs(newEl - m_selectedPlaneLayer->planeElevation()) > 1e-4;
        if (azChanged || elChanged) {
            if (azChanged)
                m_selectedPlaneLayer->setPlaneAzimuth(newAz);
            if (elChanged)
                m_selectedPlaneLayer->setPlaneElevation(newEl);
            changed = true;
        }
    }
    }
    return changed;
}

void LayersRendererQtItem::endPlaneDrag() {
    {
        std::lock_guard<std::mutex> lock(LayersRendererQtItem::layerAccessMutex());
        if (auto layer = m_poseEditLayer.lock())
            layer->endPoseEdit();
        m_poseEditLayer.reset();
    }
    m_planeDragActive = false;
    m_planeDragAction = -1;
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

    if (NdiSenderModel::instance())
        NdiSenderModel::instance()->setLayersRendererItem(nullptr);

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

    if (NdiSenderModel::instance())
        NdiSenderModel::instance()->setLayersRendererItem(nullptr);

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
        m_renderer->setMirrorLink(m_mirrorLink);
    }
    m_renderer->setWindow(window());
    m_renderer->setItemVisible(isVisible());
    m_renderer->setRenderAsFisheye(m_renderAsFisheye);
    QMatrix4x4 sphereRotation;
    sphereRotation.rotate(m_sphereRotation);
    m_renderer->setSphereView(m_renderAsSphere, sphereRotation, m_sphereDistance);
    m_renderer->updateMeshes(m_meshRadius, m_meshFov, m_meshAngle);
    m_renderer->setMpvObject(m_mpvObject);
    m_renderer->setBackgroundImageFile(m_backgroundImageFile);
    m_renderer->setForegroundImageFile(m_foregroundImageFile);
#ifdef CLUX_SUPPORT
    m_renderer->setCluxPreviewVisible(m_cluxPreviewVisible);
    m_renderer->setCluxPreviewFrame(m_cluxPreviewFrame);
#endif

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

    // Picks up runtime changes of the configured NDI resolution as well.
    updateNdiTarget();
    updateMirrorTarget();

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
    m_ndiProjectionMatrix = m_projectionMatrix;
}

LayersRendererQtOpenGLObject::~LayersRendererQtOpenGLObject() {
    for (ShaderVariant &variant : m_shaderVariants) {
        variant.videoPrg.reset();
        variant.meshPrg.reset();
        variant.EACPrg.reset();
        variant.fisheyePrg.reset();
        variant.fisheyeEACPrg.reset();
    }
    m_domeMesh.reset();
    m_domeMaskMesh.reset();
    m_sphereMesh.reset();
    if (m_maskTexture) {
        glDeleteTextures(1, &m_maskTexture);
        m_maskTexture = 0;
    }
#ifdef CLUX_SUPPORT
    if (m_cluxDiskTexture) {
        glDeleteTextures(1, &m_cluxDiskTexture);
        m_cluxDiskTexture = 0;
    }
#endif
    releaseNdiTarget();
    releaseMirrorTarget();
}

void LayersRendererQtOpenGLObject::setWindow(QQuickWindow* window) {
    m_window = window;
}

void LayersRendererQtOpenGLObject::setCameraParams(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    m_viewMatrix = viewMatrix;
    m_projectionMatrix = projectionMatrix;
}

void LayersRendererQtOpenGLObject::setRenderAsFisheye(bool value) {
    m_renderAsFisheye = value;
}

void LayersRendererQtOpenGLObject::setSphereView(bool enabled, const QMatrix4x4& rotation, float distance) {
    m_renderAsSphere = enabled;
    m_sphereRotation = rotation;
    m_sphereDistance = distance;
}

void LayersRendererQtOpenGLObject::applyLensUniforms(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    // Uniform values stay with the program, so setting them once per pass covers every draw.
    const QMatrix4x4 viewProjection = projectionMatrix * viewMatrix;
    const float horizon = 1.0f / std::max(1.0f, m_sphereDistance);
    for (ShaderVariant &variant : m_shaderVariants) {
        for (QOpenGLShaderProgram *prg : {variant.fisheyePrg.get(), variant.fisheyeEACPrg.get()}) {
            if (!prg)
                continue;
            prg->bind();
            prg->setUniformValue("lens", m_renderAsSphere ? 1 : 0);
            prg->setUniformValue("sphereRotation", m_sphereRotation);
            prg->setUniformValue("sphereViewProjection", viewProjection);
            prg->setUniformValue("sphereHorizon", horizon);
            prg->release();
        }
    }
}

#ifdef CLUX_SUPPORT
void LayersRendererQtOpenGLObject::setCluxPreviewVisible(bool visible) {
    m_cluxPreviewVisible = visible;
}

void LayersRendererQtOpenGLObject::setCluxPreviewFrame(const QVariantList& frame) {
    // The frame is a flat list of nLights*3 ints (r, g, b per light). Store the latest values on
    // the GUI thread; the render thread bakes them into the disk texture when they change. This
    // follows the same requested-state pattern as the other sync() pushes. Only mark dirty when
    // the bytes actually differ so an unchanged frame pushed every sync() doesn't re-upload.
    const int count = static_cast<int>(frame.size());
    if (count <= 0 || count % 3 != 0) {
        m_cluxNLights = 0;
        return;
    }
    const int nLights = count / 3;
    std::vector<unsigned char> bytes(static_cast<size_t>(nLights) * 3);
    for (int i = 0; i < count; ++i) {
        int v = frame.at(i).toInt();
        if (v < 0) v = 0;
        else if (v > 255) v = 255;
        bytes[static_cast<size_t>(i)] = static_cast<unsigned char>(v);
    }
    const bool changed = (nLights != m_cluxNLights) ||
                         !(m_cluxFrameBytes.size() == bytes.size() &&
                           std::equal(m_cluxFrameBytes.begin(), m_cluxFrameBytes.end(), bytes.begin()));
    m_cluxNLights = nLights;
    m_cluxFrameBytes = std::move(bytes);
    if (changed)
        m_cluxColorsDirty = true;
}

void LayersRendererQtOpenGLObject::ensureCluxDiskTexture(int nLights) {
    if (nLights <= 0)
        return;

    // Rebuild the per-pixel light/alpha map when the light count changes. The disk is a square
    // N x N texture whose pixels mirror the DomeGrid texcoord convention: center at (0.5, 0.5),
    // offset proportional to (sin theta, -cos theta) with radius 0.5 at the rim. Each pixel gets
    // the light whose azimuth (2*pi*i/nLights) it falls in. Keep the bright glow in the
    // outer 10% of the fisheye radius (about 9 degrees above the rim on a hemisphere).
    // A faint cubic tail carries the color farther upward, fading out at the crown.
    if (m_cluxPixelMapNLights != nLights) {
        const int N = 512;
        m_cluxPixelLight.assign(static_cast<size_t>(N) * N, -1);
        m_cluxPixelAlpha.assign(static_cast<size_t>(N) * N, 0);
        for (int y = 0; y < N; ++y) {
            const float ty = ((float)y + 0.5f) / static_cast<float>(N);   // 0..1 top->bottom
            for (int x = 0; x < N; ++x) {
                const float tx = ((float)x + 0.5f) / static_cast<float>(N);   // 0..1 left->right
                const float dx = tx - 0.5f;
                const float dy = ty - 0.5f;
                const float r = std::sqrt(dx * dx + dy * dy);    // 0 at center, ~0.354 at corner
                if (r > 0.5f)
                    continue;                                    // outside the disk
                const size_t idx = static_cast<size_t>(y) * N + x;
                const float radius = r / 0.5f;
                const float rimGlow = std::clamp((radius - 0.9f) / 0.1f, 0.0f, 1.0f);
                const float upwardTint = radius * radius * radius;
                const float alpha = 0.92f * rimGlow * rimGlow + 0.08f * upwardTint;
                m_cluxPixelAlpha[idx] = static_cast<unsigned char>(alpha * 255.0f);
                if (r < 1e-6f) {
                    m_cluxPixelLight[idx] = 0;                   // center pixel: arbitrary light
                } else {
                    const float theta = std::atan2(dx, -dy);     // matches DomeGrid texcoord
                    int li = static_cast<int>(theta / (2.0 * kPi) * nLights + 0.5f);
                    if (li < 0)
                        li += nLights;
                    m_cluxPixelLight[idx] = li % nLights;
                }
            }
        }
        m_cluxTexData.assign(static_cast<size_t>(N) * N * 4, 0);
        m_cluxPixelMapNLights = nLights;
    }

    if (!m_cluxDiskTexture) {
        glGenTextures(1, &m_cluxDiskTexture);
    }
}

void LayersRendererQtOpenGLObject::renderCluxPreview(float angle, const QMatrix4x4& viewMatrix,
    const QMatrix4x4& projectionMatrix) {
    if (!m_cluxPreviewVisible || m_cluxNLights <= 0)
        return;

    ensureCluxDiskTexture(m_cluxNLights);
    if (!m_cluxDiskTexture || !m_domeMesh)
        return;

    // The NDI path draws the overlay after renderLayers() with its own projection.
    applyLensUniforms(viewMatrix, projectionMatrix);

    // Bake the latest light colors into the disk texture (only when they changed). RGB carries the
    // color, A carries the precomputed radial fade, so the existing programs' per-pixel alpha
    // produces the rim-to-center fade without any new shader.
    if (m_cluxColorsDirty) {
        const size_t nPixels = m_cluxPixelLight.size();
        for (size_t i = 0; i < nPixels; ++i) {
            unsigned char* px = &m_cluxTexData[i * 4];
            const int li = m_cluxPixelLight[i];
            if (li >= 0 && li < m_cluxNLights) {
                const size_t s = static_cast<size_t>(li) * 3;
                px[0] = m_cluxFrameBytes[s];
                px[1] = m_cluxFrameBytes[s + 1];
                px[2] = m_cluxFrameBytes[s + 2];
            } else {
                px[0] = px[1] = px[2] = 0;
            }
            px[3] = m_cluxPixelAlpha[i];
        }
        const int N = static_cast<int>(std::sqrt(static_cast<double>(m_cluxTexData.size() / 4)));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_cluxDiskTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_cluxTexData.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_cluxColorsDirty = false;
    }

    // Draw on top of all content layers using the same dome transform as a zero-rotation dome
    // layer. Depth testing is disabled for the whole pass (see firstPass), so draw order keeps this
    // overlay on top; blending gives the alpha fade over the content below.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_cluxDiskTexture);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (lensProjection()) {
        if (!m_sv->fisheyePrg) {
            glDisable(GL_BLEND);
            return;
        }
        m_sv->fisheyePrg->bind();
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, 0);
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, 0);
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeAlphaLoc, 1.0f);   // per-pixel alpha carries the fade
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeFlipYLoc, false);
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, 0.f, 0.f, 1.f, 1.f);
        QMatrix4x4 model;   // identity: no layer rotation for the preview overlay
        m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeMatrixLoc, model);
        if (m_domeMesh)
            m_domeMesh->draw();
        m_sv->fisheyePrg->release();
    } else {
        if (!m_sv->meshPrg) {
            glDisable(GL_BLEND);
            return;
        }
        m_sv->meshPrg->bind();
        m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, 0);
        m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, 0);
        m_sv->meshPrg->setUniformValue(m_sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
        m_sv->meshPrg->setUniformValue(m_sv->meshAlphaLoc, 1.0f);   // per-pixel alpha carries the fade
        m_sv->meshPrg->setUniformValue(m_sv->meshFlipYLoc, false);
        m_sv->meshPrg->setUniformValue(m_sv->meshOutsideLoc, 0);    // render the dome's inner surface
        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        mvp.rotate(-angle, 1, 0, 0);   // dome tilt (matches a zero-rotation dome layer)
        m_sv->meshPrg->setUniformValue(m_sv->meshMatrixLoc, mvp);
        if (m_domeMesh)
            m_domeMesh->draw();
        m_sv->meshPrg->release();
    }

    glDisable(GL_BLEND);
}
#endif // CLUX_SUPPORT

void LayersRendererQtOpenGLObject::setNdiProjectionMatrix(const QMatrix4x4& projectionMatrix) {
    m_ndiProjectionMatrix = projectionMatrix;
}

void LayersRendererQtOpenGLObject::setNdiCaptureEnabled(bool enabled) {
    m_ndiCaptureEnabled = enabled;
}

void LayersRendererQtOpenGLObject::setNdiCaptureSize(int width, int height) {
    m_ndiRequestedWidth = width;
    m_ndiRequestedHeight = height;
}

unsigned int LayersRendererQtOpenGLObject::ndiTextureId() const {
    return m_ndiTexture;
}

int LayersRendererQtOpenGLObject::ndiWidth() const {
    return m_ndiWidth;
}

int LayersRendererQtOpenGLObject::ndiHeight() const {
    return m_ndiHeight;
}

bool LayersRendererQtOpenGLObject::ensureNdiTarget() {
    const int width = m_ndiRequestedWidth;
    const int height = m_ndiRequestedHeight;

    if (width <= 0 || height <= 0) {
        releaseNdiTarget();
        return false;
    }

    if (m_ndiFbo != 0 && m_ndiWidth == width && m_ndiHeight == height)
        return true;

    releaseNdiTarget();

    glGenTextures(1, &m_ndiTexture);
    glBindTexture(GL_TEXTURE_2D, m_ndiTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_ndiFbo);

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ndiFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ndiTexture, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qWarning() << "LayersRendererQtItem: could not create the" << width << "x" << height
                   << "NDI capture target, framebuffer status" << status;
        releaseNdiTarget();
        return false;
    }

    m_ndiWidth = width;
    m_ndiHeight = height;
    return true;
}

void LayersRendererQtOpenGLObject::releaseNdiTarget() {
    if (m_ndiFbo) {
        glDeleteFramebuffers(1, &m_ndiFbo);
        m_ndiFbo = 0;
    }
    if (m_ndiTexture) {
        glDeleteTextures(1, &m_ndiTexture);
        m_ndiTexture = 0;
    }
    m_ndiWidth = 0;
    m_ndiHeight = 0;
}

void LayersRendererQtOpenGLObject::blitNdiTargetToScreen(GLuint targetFramebuffer) {
    if (!m_ndiFbo || m_ndiWidth <= 0 || m_ndiHeight <= 0)
        return;

    const QRect itemRect = m_viewportRect;
    if (itemRect.width() <= 0 || itemRect.height() <= 0)
        return;

    // Largest centered rect inside the item that keeps the capture aspect ratio, so the
    // on-screen image shows exactly what is broadcast, letterboxed if needed.
    const double captureAspect = double(m_ndiWidth) / double(m_ndiHeight);
    int destWidth = itemRect.width();
    int destHeight = int(std::lround(destWidth / captureAspect));
    if (destHeight > itemRect.height()) {
        destHeight = itemRect.height();
        destWidth = int(std::lround(destHeight * captureAspect));
    }

    const int destX = itemRect.x() + (itemRect.width() - destWidth) / 2;
    const int destY = itemRect.y() + (itemRect.height() - destHeight) / 2;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_ndiFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFramebuffer);

    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0, 0, m_ndiWidth, m_ndiHeight,
                      destX, destY, destX + destWidth, destY + destHeight,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
    glViewport(itemRect.x(), itemRect.y(), itemRect.width(), itemRect.height());
}

void LayersRendererQtOpenGLObject::setMirrorLink(std::shared_ptr<LayersRendererMirrorLink> link) {
    m_mirrorLink = std::move(link);
}

void LayersRendererQtOpenGLObject::setMirrorSize(int width, int height) {
    m_mirrorRequestedWidth = width;
    m_mirrorRequestedHeight = height;
}

void LayersRendererQtOpenGLObject::setMirrorProjectionMatrix(const QMatrix4x4& projectionMatrix) {
    m_mirrorProjectionMatrix = projectionMatrix;
}

bool LayersRendererQtOpenGLObject::ensureMirrorTarget() {
    const int width = m_mirrorRequestedWidth;
    const int height = m_mirrorRequestedHeight;

    if (m_mirrorFbos[0] != 0 && m_mirrorWidth == width && m_mirrorHeight == height)
        return true;

    releaseMirrorTarget();

    // The textures are replaced while the mirror cannot pick them up.
    std::lock_guard<std::mutex> lock(m_mirrorLink->mutex);

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);

    glGenTextures(2, m_mirrorLink->textures);
    glGenFramebuffers(2, m_mirrorFbos);

    bool complete = true;
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, m_mirrorLink->textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, m_mirrorFbos[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mirrorLink->textures[i], 0);
        complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));

    m_mirrorWidth = width;
    m_mirrorHeight = height;
    m_mirrorNext = 0;
    m_mirrorLink->width = width;
    m_mirrorLink->height = height;
    m_mirrorLink->latest = -1;

    if (!complete) {
        qWarning() << "LayersRendererQtItem: could not create the" << width << "x" << height
                   << "mirror target";
        return false;
    }
    return true;
}

void LayersRendererQtOpenGLObject::releaseMirrorTarget() {
    if (m_mirrorFbos[0] == 0)
        return;

    glDeleteFramebuffers(2, m_mirrorFbos);
    m_mirrorFbos[0] = m_mirrorFbos[1] = 0;
    m_mirrorWidth = 0;
    m_mirrorHeight = 0;

    if (m_mirrorLink) {
        std::lock_guard<std::mutex> lock(m_mirrorLink->mutex);
        glDeleteTextures(2, m_mirrorLink->textures);
        m_mirrorLink->textures[0] = m_mirrorLink->textures[1] = 0;
        if (m_mirrorLink->fence) {
            glDeleteSync(m_mirrorLink->fence);
            m_mirrorLink->fence = nullptr;
        }
        m_mirrorLink->width = 0;
        m_mirrorLink->height = 0;
        m_mirrorLink->latest = -1;
    }
}

void LayersRendererQtOpenGLObject::renderMirror() {
    if (!mirrorRequested()) {
        releaseMirrorTarget();
        return;
    }
    if (!ensureMirrorTarget())
        return;

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);

    // Render into the texture the mirror is not showing, so it never sees a half drawn frame.
    const int index = m_mirrorNext;
    glBindFramebuffer(GL_FRAMEBUFFER, m_mirrorFbos[index]);
    glViewport(0, 0, m_mirrorWidth, m_mirrorHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // renderViewportRect() (flat layer aspect, fisheye square) has to describe the mirror target
    // during this pass. Like the NDI output, the mirror never contains the C-Lux preview.
    const QRect viewportRect = m_viewportRect;
    m_viewportRect = QRect(0, 0, m_mirrorWidth, m_mirrorHeight);
    renderLayers(m_meshAngle, m_viewMatrix, m_mirrorProjectionMatrix, /*includeCluxPreview=*/false);
    m_viewportRect = viewportRect;

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
    const QRect vp = renderViewportRect();
    glViewport(vp.x(), vp.y(), vp.width(), vp.height());

    // The flush makes the fence visible to the mirror's context.
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();

    std::lock_guard<std::mutex> lock(m_mirrorLink->mutex);
    if (m_mirrorLink->fence)
        glDeleteSync(m_mirrorLink->fence);
    m_mirrorLink->fence = fence;
    m_mirrorLink->latest = index;
    m_mirrorNext = 1 - index;
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
    createShaderVariant(false);
    createShaderVariant(true);
    m_sv = &m_shaderVariants[0];
}

void LayersRendererQtOpenGLObject::createShaderVariant(bool postEffects) {
    m_sv = &m_shaderVariants[postEffects ? 1 : 0];
    const QByteArray post(cplay::anim::postGlsl(postEffects));
    const QByteArray videoFrag = QByteArray(VideoFragHead) + post + VideoFragMain;
    const QByteArray eacFrag = QByteArray(EACFragHead) + post + EACFragCommon;

    // Create video shader
    m_sv->videoPrg = std::make_unique<QOpenGLShaderProgram>();
    m_sv->videoPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, VideoVert);
    m_sv->videoPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, videoFrag);
    m_sv->videoPrg->link();

    m_sv->videoPrg->bind();
    m_sv->videoPrg->setUniformValue("tex", 0);
    m_sv->videoAlphaLoc = m_sv->videoPrg->uniformLocation("alpha");
    m_sv->videoEyeModeLoc = m_sv->videoPrg->uniformLocation("eye");
    m_sv->videoFlipYLoc = m_sv->videoPrg->uniformLocation("flipY");
    m_sv->videoStereoscopicModeLoc = m_sv->videoPrg->uniformLocation("stereoscopicMode");
    m_sv->videoRoi = m_sv->videoPrg->uniformLocation("roi");
    m_sv->videoMaskLocs.locate([this](const char *n) { return m_sv->videoPrg->uniformLocation(n); });
    m_sv->videoPrg->setUniformValue(m_sv->videoMaskLocs.quadMatrix, QMatrix4x4());
    m_sv->videoPrg->release();

    // Create mesh shader
    m_sv->meshPrg = std::make_unique<QOpenGLShaderProgram>();
    m_sv->meshPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, MeshVert);
    m_sv->meshPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, videoFrag);
    m_sv->meshPrg->link();

    m_sv->meshPrg->bind();
    m_sv->meshPrg->setUniformValue("tex", 0);
    m_sv->meshMatrixLoc = m_sv->meshPrg->uniformLocation("mvp");
    m_sv->meshEyeModeLoc = m_sv->meshPrg->uniformLocation("eye");
    m_sv->meshFlipYLoc = m_sv->meshPrg->uniformLocation("flipY");
    m_sv->meshStereoscopicModeLoc = m_sv->meshPrg->uniformLocation("stereoscopicMode");
    m_sv->meshRoi = m_sv->meshPrg->uniformLocation("roi");
    m_sv->meshAlphaLoc = m_sv->meshPrg->uniformLocation("alpha");
    m_sv->meshOutsideLoc = m_sv->meshPrg->uniformLocation("outside");
    m_sv->meshMaskLocs.locate([this](const char *n) { return m_sv->meshPrg->uniformLocation(n); });
    m_sv->meshPrg->release();

    // Create EAC shader
    m_sv->EACPrg = std::make_unique<QOpenGLShaderProgram>();
    m_sv->EACPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, EACMeshVert);
    m_sv->EACPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, eacFrag + EACFragMain);
    m_sv->EACPrg->link();

    m_sv->EACPrg->bind();
    m_sv->EACPrg->setUniformValue("tex", 0);
    m_sv->EACMatrixLoc = m_sv->EACPrg->uniformLocation("mvp");
    m_sv->EACEyeModeLoc = m_sv->EACPrg->uniformLocation("eye");
    m_sv->EACFlipYLoc = m_sv->EACPrg->uniformLocation("flipY");
    m_sv->EACStereoscopicModeLoc = m_sv->EACPrg->uniformLocation("stereoscopicMode");
    m_sv->EACAlphaLoc = m_sv->EACPrg->uniformLocation("alpha");
    m_sv->EACOutsideLoc = m_sv->EACPrg->uniformLocation("outside");
    m_sv->EACScaleLoc = m_sv->EACPrg->uniformLocation("scaleToUnitCube");
    m_sv->EACVideoWidthLoc = m_sv->EACPrg->uniformLocation("videoWidth");
    m_sv->EACVideoHeightLoc = m_sv->EACPrg->uniformLocation("videoHeight");
    m_sv->EACFlipUpDownLoc = m_sv->EACPrg->uniformLocation("flipUpDown");
    m_sv->EACMaskLocs.locate([this](const char *n) { return m_sv->EACPrg->uniformLocation(n); });
    m_sv->EACPrg->release();

    // Create fisheye shader (one-pass 180-degree fulldome projection).
    // The vertex stage replaces the perspective camera with an equidistant fisheye lens
    // centered on the dome zenith; the fragment samples the mesh texture coordinates.
    // Used for dome, sphere (EQR) and plane content.
    m_sv->fisheyePrg = std::make_unique<QOpenGLShaderProgram>();
    m_sv->fisheyePrg->addShaderFromSourceCode(QOpenGLShader::Vertex, QByteArray(FisheyeProjection) + FisheyeMeshVert);
    m_sv->fisheyePrg->addShaderFromSourceCode(QOpenGLShader::Fragment, QByteArray(FisheyeVideoFragHead) + post + FisheyeVideoFragMain);
    m_sv->fisheyePrg->link();

    m_sv->fisheyePrg->bind();
    m_sv->fisheyePrg->setUniformValue("tex", 0);
    m_sv->fisheyeMatrixLoc = m_sv->fisheyePrg->uniformLocation("model");
    m_sv->fisheyeEyeModeLoc = m_sv->fisheyePrg->uniformLocation("eye");
    m_sv->fisheyeFlipYLoc = m_sv->fisheyePrg->uniformLocation("flipY");
    m_sv->fisheyeStereoscopicModeLoc = m_sv->fisheyePrg->uniformLocation("stereoscopicMode");
    m_sv->fisheyeAlphaLoc = m_sv->fisheyePrg->uniformLocation("alpha");
    m_sv->fisheyeOutsideLoc = m_sv->fisheyePrg->uniformLocation("outside");
    m_sv->fisheyeHalfFovLoc = m_sv->fisheyePrg->uniformLocation("halfFov");
    m_sv->fisheyeRoi = m_sv->fisheyePrg->uniformLocation("roi");
    m_sv->fisheyeMaskLocs.locate([this](const char *n) { return m_sv->fisheyePrg->uniformLocation(n); });
    m_sv->fisheyePrg->release();

    // Create fisheye shader for EAC content: same lens, EAC cubemap texture mapping.
    m_sv->fisheyeEACPrg = std::make_unique<QOpenGLShaderProgram>();
    m_sv->fisheyeEACPrg->addShaderFromSourceCode(QOpenGLShader::Vertex, QByteArray(FisheyeProjection) + FisheyeEACMeshVert);
    m_sv->fisheyeEACPrg->addShaderFromSourceCode(QOpenGLShader::Fragment, eacFrag + FisheyeEACFragMain);
    m_sv->fisheyeEACPrg->link();

    m_sv->fisheyeEACPrg->bind();
    m_sv->fisheyeEACPrg->setUniformValue("tex", 0);
    m_sv->fisheyeEACMatrixLoc = m_sv->fisheyeEACPrg->uniformLocation("model");
    m_sv->fisheyeEACHalfFovLoc = m_sv->fisheyeEACPrg->uniformLocation("halfFov");
    m_sv->fisheyeEACEyeModeLoc = m_sv->fisheyeEACPrg->uniformLocation("eye");
    m_sv->fisheyeEACFlipYLoc = m_sv->fisheyeEACPrg->uniformLocation("flipY");
    m_sv->fisheyeEACStereoscopicModeLoc = m_sv->fisheyeEACPrg->uniformLocation("stereoscopicMode");
    m_sv->fisheyeEACAlphaLoc = m_sv->fisheyeEACPrg->uniformLocation("alpha");
    m_sv->fisheyeEACOutsideLoc = m_sv->fisheyeEACPrg->uniformLocation("outside");
    m_sv->fisheyeEACScaleLoc = m_sv->fisheyeEACPrg->uniformLocation("scaleToUnitCube");
    m_sv->fisheyeEACVideoWidthLoc = m_sv->fisheyeEACPrg->uniformLocation("videoWidth");
    m_sv->fisheyeEACVideoHeightLoc = m_sv->fisheyeEACPrg->uniformLocation("videoHeight");
    m_sv->fisheyeEACFlipUpDownLoc = m_sv->fisheyeEACPrg->uniformLocation("flipUpDown");
    m_sv->fisheyeEACMaskLocs.locate([this](const char *n) { return m_sv->fisheyeEACPrg->uniformLocation(n); });
    m_sv->fisheyeEACPrg->release();
}

namespace {
struct QtUniformSetter {
    QOpenGLShaderProgram *prg;
    void set1i(int loc, int v) const { prg->setUniformValue(loc, static_cast<GLint>(v)); }
    void set1f(int loc, float v) const { prg->setUniformValue(loc, static_cast<GLfloat>(v)); }
    void set2f(int loc, float a, float b) const { prg->setUniformValue(loc, a, b); }
    void set4fv(int loc, int count, const float *v) const { prg->setUniformValueArray(loc, v, count, 4); }
};
}

void LayersRendererQtOpenGLObject::resetAnimUniforms() {
    const cplay::anim::LayerAnimState neutral;
    const cplay::anim::PostEffects noFx;
    auto reset = [&neutral, &noFx](QOpenGLShaderProgram *prg, const cplay::anim::MaskUniformLocations &locs) {
        if (!prg)
            return;
        prg->bind();
        locs.apply(QtUniformSetter{prg}, neutral, 1.f, noFx);
        prg->release();
    };
    reset(m_sv->meshPrg.get(), m_sv->meshMaskLocs);
    reset(m_sv->EACPrg.get(), m_sv->EACMaskLocs);
    reset(m_sv->fisheyePrg.get(), m_sv->fisheyeMaskLocs);
    reset(m_sv->fisheyeEACPrg.get(), m_sv->fisheyeEACMaskLocs);
    if (m_sv->videoPrg) {
        m_sv->videoPrg->bind();
        m_sv->videoMaskLocs.apply(QtUniformSetter{m_sv->videoPrg.get()}, neutral, 1.f, noFx);
        m_sv->videoPrg->setUniformValue(m_sv->videoMaskLocs.quadMatrix, QMatrix4x4());
        m_sv->videoPrg->release();
    }
}

QRect LayersRendererQtOpenGLObject::renderViewportRect() const {
    if (!m_renderAsFisheye)
        return m_viewportRect;

    // Fulldome output must be square (1:1) so it can be shown, captured and mapped in other
    // applications without any aspect correction. Use the largest centered square that fits.
    const int side = std::min(m_viewportRect.width(), m_viewportRect.height());
    if (side <= 0)
        return m_viewportRect;

    return QRect(
        m_viewportRect.x() + (m_viewportRect.width() - side) / 2,
        m_viewportRect.y() + (m_viewportRect.height() - side) / 2,
        side,
        side);
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
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, const BaseLayer* fxSource) {
    if (!layer || !layer->ready()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->textureId());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float alpha = layer->renderAlpha();
    const glm::vec3 rot = layer->renderRotate();
    const glm::vec3 trans = layer->renderTranslate();
    const cplay::anim::LayerAnimState &anim = layer->animState();
    const bool animActive = layer->animActive();
    const cplay::anim::PostEffects &fx = (fxSource ? fxSource : layer)->postEffects();
    // Layers without post effects use the plain shaders; restore them for the draws that follow.
    struct VariantRestore {
        ShaderVariant *&current;
        ShaderVariant *plain;
        ~VariantRestore() { current = plain; }
    } variantRestore{m_sv, &m_shaderVariants[0]};
    m_sv = &m_shaderVariants[fx.active() ? 1 : 0];
    const float aspect = layer->renderAspectRatio();

    int gridMode = layer->gridMode();
    int stereoMode = layer->renderStereoMode();
    // Master UI specific: if grid is 0, we use the default values
    if (gridMode == 0) {
        gridMode = SyncHelper::instance().variables.gridToMapOnBg;
        stereoMode = SyncHelper::instance().variables.stereoscopicModeBg;
    }

    if (gridMode == 4) {
        // EAC sphere: plain perspective, or one-pass fisheye (fulldome).
        if (lensProjection()) {
            m_sv->fisheyeEACPrg->bind();

            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACAlphaLoc, alpha);
            m_sv->fisheyeEACMaskLocs.apply(QtUniformSetter{m_sv->fisheyeEACPrg.get()}, anim, 2.f, fx);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACFlipYLoc, layer->renderFlipY());
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACOutsideLoc, 0);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACVideoWidthLoc, layer->width());
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACVideoHeightLoc, layer->height());
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACFlipUpDownLoc, false);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACScaleLoc, static_cast<float>(100.0 / m_meshRadius));
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));

            if (layer->renderStereoMode() > 0) {
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACEyeModeLoc, eyeMode);
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACEyeModeLoc, 0);
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACStereoscopicModeLoc, 0);
            }

            QMatrix4x4 model;
            model.rotate(rot.z, 0, 0, 1);   // roll
            model.rotate(rot.x, 1, 0, 0);   // pitch
            model.rotate(rot.y, 0, 1, 0);   // yaw
            model.rotate(-90.f, 0, 0, 1);               // roll
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACMatrixLoc, model);

            glDisable(GL_CULL_FACE);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            m_sv->fisheyeEACPrg->release();
        }
        else {
            m_sv->EACPrg->bind();

            m_sv->EACPrg->setUniformValue(m_sv->EACAlphaLoc, alpha);
            m_sv->EACMaskLocs.apply(QtUniformSetter{m_sv->EACPrg.get()}, anim, 2.f, fx);
            m_sv->EACPrg->setUniformValue(m_sv->EACFlipYLoc, layer->renderFlipY());
            m_sv->EACPrg->setUniformValue(m_sv->EACOutsideLoc, 0);
            m_sv->EACPrg->setUniformValue(m_sv->EACVideoWidthLoc, layer->width());
            m_sv->EACPrg->setUniformValue(m_sv->EACVideoHeightLoc, layer->height());
            m_sv->EACPrg->setUniformValue(m_sv->EACFlipUpDownLoc, false);
            m_sv->EACPrg->setUniformValue(m_sv->EACScaleLoc, static_cast<float>(100.0 / m_meshRadius));

            if (layer->renderStereoMode() > 0) {
                m_sv->EACPrg->setUniformValue(m_sv->EACEyeModeLoc, eyeMode);
                m_sv->EACPrg->setUniformValue(m_sv->EACStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->EACPrg->setUniformValue(m_sv->EACEyeModeLoc, 0);
                m_sv->EACPrg->setUniformValue(m_sv->EACStereoscopicModeLoc, 0);
            }

            QMatrix4x4 mvp = projectionMatrix * viewMatrix;
            QVector3D translate(trans.x, trans.y, trans.z);
            mvp.translate(translate);

            QMatrix4x4 mvpRot = mvp;
            mvpRot.rotate(rot.z, 0, 0, 1);                      // roll
            mvpRot.rotate(rot.x, 1, 0, 0);                      // pitch
            mvpRot.rotate(rot.y, 0, 1, 0);                      // yaw
            mvpRot.rotate(-90.f, 0, 0, 1);                                    // roll

            m_sv->EACPrg->setUniformValue(m_sv->EACMatrixLoc, mvpRot);

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

            m_sv->EACPrg->release();
        }
    }
    else if (gridMode == 3) {
        // EQR sphere rendering: plain perspective, or one-pass fisheye (fulldome).
        if (lensProjection()) {
            m_sv->fisheyePrg->bind();

            if (stereoMode > 0) {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, eyeMode);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, 0);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, 0);
            }

            if (layer->roiEnabled()) {
                glm::vec4 roi = layer->roi();
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, roi.x, roi.y, roi.z, roi.w);
            }
            else {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, 0.f, 0.f, 1.f, 1.f);
            }

            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeAlphaLoc, alpha);
            m_sv->fisheyeMaskLocs.apply(QtUniformSetter{m_sv->fisheyePrg.get()}, anim, 2.f, fx);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeFlipYLoc, layer->renderFlipY());
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));

            QMatrix4x4 model;
            model.rotate(rot.z, 0, 0, 1);         // roll
            model.rotate(rot.x, 1, 0, 0);         // pitch
            model.rotate(rot.y - 90.f, 0, 1, 0);  // yaw
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeMatrixLoc, model);

            // Only the hemisphere above the dome horizon reaches the disk (the fragment
            // stage discards the rest), and the nonlinear projection makes winding order
            // meaningless, so draw the sphere once without culling.
            glDisable(GL_CULL_FACE);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            m_sv->fisheyePrg->release();
        }
        else {
            // EQR sphere rendering
            QMatrix4x4 mvp = projectionMatrix * viewMatrix;
            QVector3D translate(trans.x, trans.y, trans.z);
            mvp.translate(translate);

            QMatrix4x4 mvpRot = mvp;
            mvpRot.rotate(rot.z, 0, 0, 1);  // roll
            mvpRot.rotate(rot.x, 1, 0, 0);  // pitch
            mvpRot.rotate(rot.y - 90.f, 0, 1, 0);  // yaw

            m_sv->meshPrg->bind();

            if (stereoMode > 0) {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, eyeMode);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, 0);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, 0);
            }

            if (layer->roiEnabled()) {
                glm::vec4 roi = layer->roi();
                m_sv->meshPrg->setUniformValue(m_sv->meshRoi, roi.x, roi.y, roi.z, roi.w);
            }
            else {
                m_sv->meshPrg->setUniformValue(m_sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
            }

            m_sv->meshPrg->setUniformValue(m_sv->meshAlphaLoc, alpha);
            m_sv->meshMaskLocs.apply(QtUniformSetter{m_sv->meshPrg.get()}, anim, 2.f, fx);
            m_sv->meshPrg->setUniformValue(m_sv->meshFlipYLoc, layer->renderFlipY());
            m_sv->meshPrg->setUniformValue(m_sv->meshMatrixLoc, mvpRot);

            // Render inside sphere
            m_sv->meshPrg->setUniformValue(m_sv->meshOutsideLoc, 0);

            glEnable(GL_CULL_FACE);

            glCullFace(GL_BACK);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            glCullFace(GL_FRONT);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            glDisable(GL_CULL_FACE);

            m_sv->meshPrg->release();
        }
    }
    else if (gridMode == 2) {
        // Dome rendering: plain perspective, or one-pass 180-degree fisheye (fulldome).
        if (lensProjection()) {
            m_sv->fisheyePrg->bind();

            if (stereoMode > 0) {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, eyeMode);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, 0);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, 0);
            }

            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeAlphaLoc, alpha);
            m_sv->fisheyeMaskLocs.apply(QtUniformSetter{m_sv->fisheyePrg.get()}, anim, 2.f, fx);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeFlipYLoc, layer->renderFlipY());
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));

            if (layer->roiEnabled()) {
                glm::vec4 roi = layer->roi();
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, roi.x, roi.y, roi.z, roi.w);
            }
            else {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, 0.f, 0.f, 1.f, 1.f);
            }

            // The fulldome image is always centered on the zenith, so the camera and the dome
            // tilt are irrelevant here; only the layer's own orientation matters.
            QMatrix4x4 model;
            model.rotate(rot.z, 0, 0, 1);   // roll
            model.rotate(rot.x, 1, 0, 0);   // pitch
            model.rotate(rot.y, 0, 1, 0);   // yaw
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeMatrixLoc, model);

            if (m_domeMesh) {
                m_domeMesh->draw();
            }

            m_sv->fisheyePrg->release();
        }
        else {
            m_sv->meshPrg->bind();

            if (stereoMode > 0) {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, eyeMode);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, 0);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, 0);
            }

            if (layer->roiEnabled()) {
                glm::vec4 roi = layer->roi();
                m_sv->meshPrg->setUniformValue(m_sv->meshRoi, roi.x, roi.y, roi.z, roi.w);
            }
            else {
                m_sv->meshPrg->setUniformValue(m_sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
            }

            m_sv->meshPrg->setUniformValue(m_sv->meshAlphaLoc, alpha);
            m_sv->meshMaskLocs.apply(QtUniformSetter{m_sv->meshPrg.get()}, anim, 2.f, fx);
            m_sv->meshPrg->setUniformValue(m_sv->meshFlipYLoc, layer->renderFlipY());

            QMatrix4x4 mvpRot = projectionMatrix * viewMatrix;
            QVector3D translate(trans.x, trans.y, trans.z);
            mvpRot.translate(translate);
            mvpRot.rotate(rot.z, 0, 0, 1);           // roll
            mvpRot.rotate(rot.x - angle, 1, 0, 0);   // pitch
            mvpRot.rotate(rot.y, 0, 1, 0);           // yaw
            m_sv->meshPrg->setUniformValue(m_sv->meshMatrixLoc, mvpRot);

            if (m_domeMesh) {
                m_domeMesh->draw();
            }

            m_sv->meshPrg->release();
        }
    }
    else if (gridMode == 1) {
        // Plane rendering
        QOpenGLShaderProgram* prg = lensProjection() ? m_sv->fisheyePrg.get() : m_sv->meshPrg.get();
        const int eyeModeLoc = lensProjection() ? m_sv->fisheyeEyeModeLoc : m_sv->meshEyeModeLoc;
        const int stereoLoc = lensProjection() ? m_sv->fisheyeStereoscopicModeLoc : m_sv->meshStereoscopicModeLoc;
        const int roiLoc = lensProjection() ? m_sv->fisheyeRoi : m_sv->meshRoi;
        const int alphaLoc = lensProjection() ? m_sv->fisheyeAlphaLoc : m_sv->meshAlphaLoc;
        const int flipYLoc = lensProjection() ? m_sv->fisheyeFlipYLoc : m_sv->meshFlipYLoc;
        const int matrixLoc = lensProjection() ? m_sv->fisheyeMatrixLoc : m_sv->meshMatrixLoc;

        prg->bind();

        if (stereoMode > 0) {
            prg->setUniformValue(eyeModeLoc, eyeMode);
            prg->setUniformValue(stereoLoc, stereoMode);
        }
        else {
            prg->setUniformValue(eyeModeLoc, 0);
            prg->setUniformValue(stereoLoc, 0);
        }

        if (layer->roiEnabled()) {
            glm::vec4 roi = layer->roi();
            prg->setUniformValue(roiLoc, roi.x, roi.y, roi.z, roi.w);
        }
        else {
            prg->setUniformValue(roiLoc, 0.f, 0.f, 1.f, 1.f);
        }

        prg->setUniformValue(alphaLoc, alpha);
        (lensProjection() ? m_sv->fisheyeMaskLocs : m_sv->meshMaskLocs).apply(QtUniformSetter{prg}, anim, aspect, fx);
        prg->setUniformValue(flipYLoc, layer->renderFlipY());

        if (layer->circularPlaneActive()) {
            const_cast<BaseLayer*>(layer)->updatePlane();
            const auto mapping = layer->circularMapping();
            const auto copies = cplay::circular::copies(layer->circularPlaneModel(), layer->planeActualSize(), mapping.widthCm);
            QMatrix4x4 tilt;
            if (!lensProjection()) tilt.rotate(-angle, 1, 0, 0);
            GLboolean previous[4];
            for (int i = 0; i < 4; ++i) { previous[i] = glIsEnabled(GL_CLIP_DISTANCE0 + i); glEnable(GL_CLIP_DISTANCE0 + i); }
            prg->setUniformValue("circularHalfWidth", float(mapping.widthCm / 200.0));
            if (lensProjection()) {
                prg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
                prg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));
            }
            for (const auto &copy : copies) {
                // QMatrix4x4's pointer constructor takes row-major values.
                QMatrix4x4 model(glm::value_ptr(copy.model));
                model = model.transposed();
                const QMatrix4x4 transform = tilt * model;
                prg->setUniformValue(matrixLoc, lensProjection() ? transform : projectionMatrix * viewMatrix * transform);
                prg->setUniformValue("circularModel", model);
                prg->setUniformValue("circularSourceBounds", QVector2D(copy.sourceBounds.x, copy.sourceBounds.y));
                layer->drawPlane();
            }
            for (int i = 0; i < 4; ++i) if (!previous[i]) glDisable(GL_CLIP_DISTANCE0 + i);
        } else {
            QMatrix4x4 planeTransform;

            // Respect the dome angle. In fisheye mode the output is locked to the zenith, so the
            // dome tilt must not be applied.
            if (!lensProjection())
                planeTransform.rotate(-angle, 1, 0, 0);

            // Specific plane parameters
            planeTransform.rotate(float(layer->renderPlaneAzimuth()), 0, -1, 0);    // azimuth
            planeTransform.rotate(float(layer->renderPlaneElevation()), 1, 0, 0);   // elevation
            planeTransform.rotate(float(layer->renderPlaneRoll()), 0, 0, 1);        // roll
            planeTransform.translate(
                float(layer->renderPlaneHorizontal()) / 100.f,
                float(layer->renderPlaneVertical()) / 100.f,
                float(-layer->renderPlaneDistance()) / 100.f);
            if (animActive && anim.scale != 1.f)
                planeTransform.scale(anim.scale, anim.scale, 1.f);

            if (lensProjection()) {
                // The fisheye lens needs the plane in world space, without any camera.
                prg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
                prg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));
                prg->setUniformValue(matrixLoc, planeTransform);
            }
            else {
                QMatrix4x4 mvp = projectionMatrix * viewMatrix;
                QMatrix4x4 finalMvp = mvp * planeTransform;
                prg->setUniformValue(matrixLoc, finalMvp);
            }

            layer->drawPlane();

        }
        prg->release();
    }
    else {
        // 2D rendering (gridMode == 0)
        m_sv->videoPrg->bind();

        if (stereoMode > 0) {
            m_sv->videoPrg->setUniformValue(m_sv->videoEyeModeLoc, eyeMode);
            m_sv->videoPrg->setUniformValue(m_sv->videoStereoscopicModeLoc, stereoMode);
        }
        else {
            m_sv->videoPrg->setUniformValue(m_sv->videoEyeModeLoc, 0);
            m_sv->videoPrg->setUniformValue(m_sv->videoStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glm::vec4 roi = layer->roi();
            m_sv->videoPrg->setUniformValue(m_sv->videoRoi, roi.x, roi.y, roi.z, roi.w);
        }
        else {
            m_sv->videoPrg->setUniformValue(m_sv->videoRoi, 0.f, 0.f, 1.f, 1.f);
        }

        m_sv->videoPrg->setUniformValue(m_sv->videoAlphaLoc, alpha);
        m_sv->videoMaskLocs.apply(QtUniformSetter{m_sv->videoPrg.get()}, anim, aspect, fx);
        {
            const QRect vp = renderViewportRect();
            const glm::mat4 quad = cplay::anim::flatQuadMatrix(anim, animActive,
                vp.height() > 0 ? static_cast<float>(vp.width()) / static_cast<float>(vp.height()) : 1.f,
                layer->screenRect());
            m_sv->videoPrg->setUniformValue(m_sv->videoMaskLocs.quadMatrix, QMatrix4x4(glm::value_ptr(glm::transpose(quad))));
        }
        m_sv->videoPrg->setUniformValue(m_sv->videoFlipYLoc, layer->renderFlipY());

        renderQuad();

        m_sv->videoPrg->release();
    }

    if (animActive)
        resetAnimUniforms();

    glDisable(GL_BLEND);
}

void LayersRendererQtOpenGLObject::renderMpvObject(MpvObject* mpv, int eyeMode, float angle,
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    if (!mpv)
        return;

    // Take the lock *before* inspecting any FBO state: reading m_fboReady
    // outside the mutex was a TOCTOU race against the GUI thread clearing it
    // (MPV_EVENT_START_FILE / VIDEO_RECONFIG) and against the render thread
    // recreating mpv_fbo.
    std::lock_guard<std::recursive_mutex> lock(mpv->m_renderMutex);

    if (!mpv->m_fboReady)
        return;

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
        // EAC sphere: plain perspective, or one-pass fisheye (fulldome).
        if (lensProjection()) {
            m_sv->fisheyeEACPrg->bind();

            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACAlphaLoc, alpha);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACFlipYLoc, false);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACVideoWidthLoc, texW);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACVideoHeightLoc, texH);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACFlipUpDownLoc, true);
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACScaleLoc, static_cast<float>(100.0 / m_meshRadius));
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACOutsideLoc, 1);

            if (stereoMode > 0) {
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACEyeModeLoc, eyeMode);
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACEyeModeLoc, 0);
                m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACStereoscopicModeLoc, 0);
            }

            QMatrix4x4 model;
            model.rotate(rotateXYZ.z(), 0, 0, 1);  // roll
            model.rotate(rotateXYZ.x(), 1, 0, 0);  // pitch
            model.rotate(rotateXYZ.y(), 0, 1, 0);  // yaw
            if (stereoMode == 3) {
                model.rotate(180, 0, 1, 0);  // yaw
                model.rotate(90.f, 0, 0, 1); // roll
            }
            else {
                model.rotate(180.f, 0, 0, 1); // roll
            }
            m_sv->fisheyeEACPrg->setUniformValue(m_sv->fisheyeEACMatrixLoc, model);

            glDisable(GL_CULL_FACE);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            m_sv->fisheyeEACPrg->release();
        }
        else {
            m_sv->EACPrg->bind();

            m_sv->EACPrg->setUniformValue(m_sv->EACAlphaLoc, alpha);
            m_sv->EACPrg->setUniformValue(m_sv->EACFlipYLoc, false);
            m_sv->EACPrg->setUniformValue(m_sv->EACVideoWidthLoc, texW);
            m_sv->EACPrg->setUniformValue(m_sv->EACVideoHeightLoc, texH);
            m_sv->EACPrg->setUniformValue(m_sv->EACFlipUpDownLoc, true);
            m_sv->EACPrg->setUniformValue(m_sv->EACScaleLoc, static_cast<float>(100.0 / m_meshRadius));

            if (stereoMode > 0) {
                m_sv->EACPrg->setUniformValue(m_sv->EACEyeModeLoc, eyeMode);
                m_sv->EACPrg->setUniformValue(m_sv->EACStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->EACPrg->setUniformValue(m_sv->EACEyeModeLoc, 0);
                m_sv->EACPrg->setUniformValue(m_sv->EACStereoscopicModeLoc, 0);
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
            m_sv->EACPrg->setUniformValue(m_sv->EACMatrixLoc, mvpRot);

            m_sv->EACPrg->setUniformValue(m_sv->EACOutsideLoc, 1);

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

            m_sv->EACPrg->release();
        }
    }
    else if (gridMode == 3) {
        // EQR sphere: plain perspective, or one-pass fisheye (fulldome).
        if (lensProjection()) {
            m_sv->fisheyePrg->bind();

            if (stereoMode > 0) {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, eyeMode);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, 0);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, 0);
            }

            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, 0.f, 0.f, 1.f, 1.f);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeAlphaLoc, alpha);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeFlipYLoc, true);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));

            QMatrix4x4 model;
            model.rotate(rotateXYZ.z(), 0, 0, 1);         // roll
            model.rotate(rotateXYZ.x(), 1, 0, 0);         // pitch
            model.rotate(rotateXYZ.y() - 90.f, 0, 1, 0);  // yaw
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeMatrixLoc, model);

            glDisable(GL_CULL_FACE);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            m_sv->fisheyePrg->release();
        }
        else {
            // EQR sphere
            QMatrix4x4 mvp = projectionMatrix * viewMatrix;
            mvp.translate(translate);

            QMatrix4x4 mvpRot = mvp;
            mvpRot.rotate(rotateXYZ.z(), 0, 0, 1);  // roll
            mvpRot.rotate(rotateXYZ.x(), 1, 0, 0);  // pitch
            mvpRot.rotate(rotateXYZ.y() - 90.f, 0, 1, 0);  // yaw

            m_sv->meshPrg->bind();

            if (stereoMode > 0) {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, eyeMode);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, 0);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, 0);
            }

            m_sv->meshPrg->setUniformValue(m_sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
            m_sv->meshPrg->setUniformValue(m_sv->meshAlphaLoc, alpha);
            m_sv->meshPrg->setUniformValue(m_sv->meshFlipYLoc, true);
            m_sv->meshPrg->setUniformValue(m_sv->meshMatrixLoc, mvpRot);
            m_sv->meshPrg->setUniformValue(m_sv->meshOutsideLoc, 0);

            // Render back faces first for correct blending
            glEnable(GL_CULL_FACE);

            glCullFace(GL_BACK);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            glCullFace(GL_FRONT);
            if (m_sphereMesh)
                m_sphereMesh->draw();

            glDisable(GL_CULL_FACE);
            m_sv->meshPrg->release();
        }
    }
    else if (gridMode == 2) {
        // Dome rendering: plain perspective, or one-pass 180-degree fisheye (fulldome).
        if (lensProjection()) {
            m_sv->fisheyePrg->bind();

            if (stereoMode > 0) {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, eyeMode);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeEyeModeLoc, 0);
                m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeStereoscopicModeLoc, 0);
            }

            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeAlphaLoc, alpha);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeFlipYLoc, true);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeRoi, 0.f, 0.f, 1.f, 1.f);

            // Fulldome output is centered on the zenith, independent of the camera and the
            // dome tilt.
            QMatrix4x4 model;
            model.rotate(rotateXYZ.z(), 0, 0, 1);   // roll
            model.rotate(rotateXYZ.x(), 1, 0, 0);   // pitch
            model.rotate(rotateXYZ.y(), 0, 1, 0);   // yaw
            m_sv->fisheyePrg->setUniformValue(m_sv->fisheyeMatrixLoc, model);

            if (m_domeMesh)
                m_domeMesh->draw();

            m_sv->fisheyePrg->release();
        }
        else {
            m_sv->meshPrg->bind();

            if (stereoMode > 0) {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, eyeMode);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, stereoMode);
            }
            else {
                m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, 0);
                m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, 0);
            }

            m_sv->meshPrg->setUniformValue(m_sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
            m_sv->meshPrg->setUniformValue(m_sv->meshAlphaLoc, alpha);
            m_sv->meshPrg->setUniformValue(m_sv->meshFlipYLoc, true);

            QMatrix4x4 mvpRot = projectionMatrix * viewMatrix;
            mvpRot.translate(translate);
            mvpRot.rotate(rotateXYZ.z(), 0, 0, 1);              // roll
            mvpRot.rotate(rotateXYZ.x() - angle, 1, 0, 0);      // pitch
            mvpRot.rotate(rotateXYZ.y(), 0, 1, 0);              // yaw

            m_sv->meshPrg->setUniformValue(m_sv->meshMatrixLoc, mvpRot);

            if (m_domeMesh)
                m_domeMesh->draw();

            m_sv->meshPrg->release();
        }
    }
    else if (gridMode == 1) {
        // Plane
        QOpenGLShaderProgram* prg = lensProjection() ? m_sv->fisheyePrg.get() : m_sv->meshPrg.get();
        const int eyeModeLoc = lensProjection() ? m_sv->fisheyeEyeModeLoc : m_sv->meshEyeModeLoc;
        const int stereoLoc = lensProjection() ? m_sv->fisheyeStereoscopicModeLoc : m_sv->meshStereoscopicModeLoc;
        const int roiLoc = lensProjection() ? m_sv->fisheyeRoi : m_sv->meshRoi;
        const int alphaLoc = lensProjection() ? m_sv->fisheyeAlphaLoc : m_sv->meshAlphaLoc;
        const int flipYLoc = lensProjection() ? m_sv->fisheyeFlipYLoc : m_sv->meshFlipYLoc;
        const int matrixLoc = lensProjection() ? m_sv->fisheyeMatrixLoc : m_sv->meshMatrixLoc;

        prg->bind();

        if (stereoMode > 0) {
            prg->setUniformValue(eyeModeLoc, eyeMode);
            prg->setUniformValue(stereoLoc, stereoMode);
        }
        else {
            prg->setUniformValue(eyeModeLoc, 0);
            prg->setUniformValue(stereoLoc, 0);
        }

        prg->setUniformValue(roiLoc, 0.f, 0.f, 1.f, 1.f);
        prg->setUniformValue(alphaLoc, alpha);
        prg->setUniformValue(flipYLoc, true);

        QMatrix4x4 planeTransform;
        // In fisheye mode the output is zenith-locked, so the dome tilt must not be applied.
        if (!lensProjection())
            planeTransform.rotate(-angle, 1, 0, 0);                                      // dome angle
        planeTransform.rotate(float(mpv->planeElevation()), 1, 0, 0);                    // elevation
        planeTransform.translate(0.f, 0.f, float(-mpv->planeDistance()) / 100.f);        // distance

        if (lensProjection()) {
            // The fisheye lens needs the plane in world space, without any camera.
            prg->setUniformValue(m_sv->fisheyeOutsideLoc, 0);
            prg->setUniformValue(m_sv->fisheyeHalfFovLoc, static_cast<float>(glm::radians(m_meshFov * 0.5)));
            prg->setUniformValue(matrixLoc, planeTransform);
        }
        else {
            QMatrix4x4 mvp = projectionMatrix * viewMatrix;
            prg->setUniformValue(matrixLoc, mvp * planeTransform);
        }

        mpv->drawPlane();

        prg->release();
    }
    else {
        // 2D rendering (gridMode == 0)
        m_sv->videoPrg->bind();

        if (stereoMode > 0) {
            m_sv->videoPrg->setUniformValue(m_sv->videoEyeModeLoc, eyeMode);
            m_sv->videoPrg->setUniformValue(m_sv->videoStereoscopicModeLoc, stereoMode);
        }
        else {
            m_sv->videoPrg->setUniformValue(m_sv->videoEyeModeLoc, 0);
            m_sv->videoPrg->setUniformValue(m_sv->videoStereoscopicModeLoc, 0);
        }

        m_sv->videoPrg->setUniformValue(m_sv->videoRoi, 0.f, 0.f, 1.f, 1.f);
        m_sv->videoPrg->setUniformValue(m_sv->videoAlphaLoc, alpha);
        m_sv->videoPrg->setUniformValue(m_sv->videoFlipYLoc, true);

        renderQuad();

        m_sv->videoPrg->release();
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
        std::string overlayFile;
        {
            std::lock_guard<std::recursive_mutex> lock(SyncHelper::instance().stringsMutex);
            overlayFile = SyncHelper::instance().variables.overlayFile;
        }
        bool overlayUpdated = false;
        if(overlayFile != m_overlayImageLayer->loadedFile()) {
            overlayUpdated = true;
        }
        m_overlayImageLayer->processImageUpload(overlayFile, overlayUpdated);
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
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix, bool includeCluxPreview) {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    std::lock_guard<std::mutex> layerAccessLock(LayersRendererQtItem::layerAccessMutex());

    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown())
        return;

    int eyeMode = 1; // Default to left eye/mono

    if (!Application::isCreated())
        return;

    applyLensUniforms(viewMatrix, projectionMatrix);

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

    // Render master slide layers of one hierarchy: BACK below the video and slides, FRONT above them
    auto renderMasterLayers = [&](BaseLayer::LayerHierarchy hierarchy) {
        if (!Application::instance().slidesModel())
            return;
        auto* slide = Application::instance().slidesModel()->masterSlide();
        if (!slide)
            return;
        int numLayers = slide->numberOfLayers();
        for (int l = numLayers - 1; l >= 0; l--) {
            std::shared_ptr<BaseLayer> layerPtr = slide->layerShared(l);
            BaseLayer* layer = layerPtr.get();

            if (layer && layer->hierarchy() == hierarchy) {
                // Layers that only exist on the master can be hidden from the 3D view.
                if (UserInterfaceSettings::hideMasterOnlyLayersIn3DView() && layer->existOnMasterOnly())
                    continue;
                if (layer->ready() && layer->hasTexture() && (layer->alpha() > 0.f)) {
                    if (layer->hasSubLayers()) {
                        for (const auto& sublayer : layer->getSubLayers()) {
                            renderLayer(sublayer.get(), eyeMode, angle, viewMatrix, projectionMatrix, layer);
                        }
                    }
                    else if(!layer->isQRCodeDetectionEnabled() || layer->isQRCodeDetectionEnabled() && !layer->hasSubLayers()){
                        renderLayer(layer, eyeMode, angle, viewMatrix, projectionMatrix);
                    }
                }
            }
        }
    };

    renderMasterLayers(BaseLayer::LayerHierarchy::BACK);

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

    // Render slides layers bottom to top, following the render order of the triggered slide
    // (see SlidesModel::tryRenderStack) - try-lock snapshot to avoid deadlocking with UI thread
    if (Application::instance().slidesModel()) {
        std::vector<std::shared_ptr<BaseLayer>> renderStack;
        if (Application::instance().slidesModel()->tryRenderStack(renderStack)) {
            for (const auto& layerPtr : renderStack) {
                BaseLayer* layer = layerPtr.get();
                if (layer) {
                    // Layers that only exist on the master can be hidden from the 3D view.
                    if (UserInterfaceSettings::hideMasterOnlyLayersIn3DView() && layer->existOnMasterOnly())
                        continue;
                    if (layer->ready() && layer->hasTexture() && (layer->alpha() > 0.f)) {
                        if (layer->hasSubLayers()) {
                            // QR operations active: skip the parent, only render sublayers
                            for (const auto& sublayer : layer->getSubLayers()) {
                                renderLayer(sublayer.get(), eyeMode, angle, viewMatrix, projectionMatrix, layer);
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

    renderMasterLayers(BaseLayer::LayerHierarchy::FRONT);

    // Render foreground image layer
    if (m_foregroundImageLayer && m_foregroundImageLayer->ready() && SyncHelper::instance().variables.alphaFg > 0.f) {
        m_foregroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaFg);
        m_foregroundImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOnFg));
        m_foregroundImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicModeFg));
        renderLayer(m_foregroundImageLayer.get(), eyeMode, angle, viewMatrix, projectionMatrix);
    }

#ifdef CLUX_SUPPORT
    // Render the C-Lux preview overlay on top of all content layers (if enabled). It uses the same
    // dome transform as a zero-rotation dome layer so it lines up with dome-mapped content; depth
    // testing is off for the pass, so draw order keeps it on top. The NDI capture path passes
    // includeCluxPreview=false and re-applies the overlay after publishing (see renderFrame).
    if (includeCluxPreview)
        renderCluxPreview(angle, viewMatrix, projectionMatrix);
#endif

    // Render black dome mask on top of everything (if enabled).
    // In fisheye mode the output is already a flat fulldome disk, and the sphere view shows
    // the content from outside, so there is no dome overflow to hide and the perspective-
    // projected mask would only cover the image.
    if (!lensProjection() && UserInterfaceSettings::hideDomeOverflowIn3DView() && m_domeMaskMesh && m_maskTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_maskTexture);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_sv->meshPrg->bind();

        m_sv->meshPrg->setUniformValue(m_sv->meshEyeModeLoc, 0);
        m_sv->meshPrg->setUniformValue(m_sv->meshStereoscopicModeLoc, 0);
        m_sv->meshPrg->setUniformValue(m_sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
        m_sv->meshPrg->setUniformValue(m_sv->meshAlphaLoc, static_cast<float>(UserInterfaceSettings::domeOverflowOpacity()));
        m_sv->meshPrg->setUniformValue(m_sv->meshFlipYLoc, false);
        m_sv->meshPrg->setUniformValue(m_sv->meshOutsideLoc, 0);

        QMatrix4x4 mvpRot = projectionMatrix * viewMatrix;
        // Invert the tilt angle: 180 + angle
        mvpRot.rotate(-(180.f + angle), 1, 0, 0);

        m_sv->meshPrg->setUniformValue(m_sv->meshMatrixLoc, mvpRot);

        m_domeMaskMesh->draw();

        m_sv->meshPrg->release();
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

void LayersRendererQtOpenGLObject::renderFrame() {
    if (m_itemVisible)
        renderView();

    // The mirror is rendered even while this view is hidden, so the floating window keeps
    // showing the 3D view when the main window shows the flat main video.
    renderMirror();
}

void LayersRendererQtOpenGLObject::renderView() {
    // Remember the framebuffer Qt has bound for this frame, so it can be restored after an
    // offscreen pass and used as the blit destination.
    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);

    const bool captureToNdi = m_ndiCaptureEnabled && ensureNdiTarget();

    if (!captureToNdi) {
        if (!m_ndiCaptureEnabled && m_ndiFbo)
            releaseNdiTarget();

        // Use the anchored item rect instead of the full window size
        // (a centered square when rendering fulldome fisheye).
        const QRect vp = renderViewportRect();
        glViewport(vp.x(), vp.y(), vp.width(), vp.height());

        renderLayers(m_meshAngle, m_viewMatrix, m_projectionMatrix);
        return;
    }

    // The scene is rendered only once, into the NDI capture target at its configured
    // resolution, and is then scaled onto the screen below.
    glBindFramebuffer(GL_FRAMEBUFFER, m_ndiFbo);
    glViewport(0, 0, m_ndiWidth, m_ndiHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // The C-Lux preview is a local overlay: render the capture target without it so the broadcast
    // never contains it, even while it is enabled for the viewport.
    renderLayers(m_meshAngle, m_viewMatrix, m_ndiProjectionMatrix, /*includeCluxPreview=*/false);

    // Publish the frame while the capture target is still bound and up to date. The readback is
    // enqueued before any later draw in this context, so it captures the preview-free scene.
    if (NdiSenderModel::instance())
        NdiSenderModel::instance()->renderFrameFrom3D();

#ifdef CLUX_SUPPORT
    // Apply the C-Lux preview on top of everything only now, right before the target is blitted to
    // the screen, so the local viewport shows it while the broadcast does not.
    renderCluxPreview(m_meshAngle, m_viewMatrix, m_ndiProjectionMatrix);
#endif

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));

    blitNdiTargetToScreen(static_cast<GLuint>(previousFbo));
}

void LayersRendererQtOpenGLObject::firstPass() {
    // Also runs while hidden as long as the mirror target needs rendering or releasing.
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown() || !m_initialized
        || (!m_itemVisible && !mirrorRequested() && m_mirrorFbos[0] == 0)) {
        return;
    }

    m_window->beginExternalCommands();

    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);

    if (m_itemVisible) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (!m_divideUpdateAndRender) {
        renderFrame();
    }

    m_window->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!m_divideUpdateAndRender) {
        m_window->resetOpenGLState();
    }
#endif
}

void LayersRendererQtOpenGLObject::secondPass() {
    if (m_shuttingDown || LayersRendererQtItem::isShuttingDown() || !m_initialized
        || (!m_itemVisible && !mirrorRequested() && m_mirrorFbos[0] == 0)) {
        return;
    }

    if (!m_divideUpdateAndRender) {
        return;
    }

    m_window->beginExternalCommands();

    renderFrame();

    m_window->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}

// -------------------------------------------------------------------------
// LayersMirrorQtItem
// -------------------------------------------------------------------------

LayersMirrorQtItem::LayersMirrorQtItem(QQuickItem* parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);

    // Same refresh rate as the source view.
    m_timer.setInterval((1.0f / 60.0f) * 1000.0f);
    connect(&m_timer, &QTimer::timeout, this, &QQuickItem::update);
}

LayersMirrorQtItem::~LayersMirrorQtItem() {
    if (m_link) {
        m_link->requestedWidth = 0;
        m_link->requestedHeight = 0;
    }
}

LayersRendererQtItem* LayersMirrorQtItem::source() const {
    return m_source;
}

void LayersMirrorQtItem::setSource(LayersRendererQtItem* source) {
    if (m_source == source)
        return;

    // Stop requesting output from the previous source.
    if (m_link) {
        m_link->requestedWidth = 0;
        m_link->requestedHeight = 0;
    }

    m_source = source;
    m_link = source ? source->mirrorLink() : nullptr;
    updateRequest();
    update();
    Q_EMIT sourceChanged();
}

void LayersMirrorQtItem::itemChange(ItemChange change, const ItemChangeData& value) {
    QQuickItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged || change == ItemSceneChange || change == ItemDevicePixelRatioHasChanged)
        updateRequest();
}

void LayersMirrorQtItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    updateRequest();
}

void LayersMirrorQtItem::updateRequest() {
    const bool active = m_link && isVisible() && window() && width() > 0 && height() > 0;

    if (m_link) {
        const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;
        m_link->requestedWidth = active ? int(std::lround(width() * dpr)) : 0;
        m_link->requestedHeight = active ? int(std::lround(height() * dpr)) : 0;
    }

    if (active)
        m_timer.start();
    else
        m_timer.stop();
}

QSGNode* LayersMirrorQtItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGSimpleTextureNode*>(oldNode);

    GLuint texture = 0;
    QSize size;
    if (m_link) {
        std::lock_guard<std::mutex> lock(m_link->mutex);
        if (m_link->latest >= 0) {
            texture = m_link->textures[m_link->latest];
            size = QSize(m_link->width, m_link->height);

            // This window's GPU commands wait until the source has finished rendering the frame.
            QOpenGLContext* ctx = QOpenGLContext::currentContext();
            if (m_link->fence && ctx)
                ctx->extraFunctions()->glWaitSync(m_link->fence, 0, GL_TIMEOUT_IGNORED);
        }
    }

    if (texture == 0 || size.isEmpty() || !window()) {
        delete node;
        return nullptr;
    }

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        // The OpenGL texture is stored bottom-up.
        node->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
        m_shownTexture = 0;
    }

    if (texture != m_shownTexture || size != m_shownSize) {
        // Only wraps the texture; the source keeps owning it.
        node->setTexture(QNativeInterface::QSGOpenGLTexture::fromNative(texture, window(), size));
        m_shownTexture = texture;
        m_shownSize = size;
    }

    // Largest centered rect that keeps the output's aspect ratio (it differs while the window
    // is resized, and the fisheye image is square).
    const QRectF bounds = boundingRect();
    const double aspect = double(size.width()) / double(size.height());
    double w = bounds.width();
    double h = w / aspect;
    if (h > bounds.height()) {
        h = bounds.height();
        w = h * aspect;
    }
    node->setRect(QRectF(bounds.x() + (bounds.width() - w) / 2.0,
                         bounds.y() + (bounds.height() - h) / 2.0, w, h));

    return node;
}
