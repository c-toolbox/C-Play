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
#include <QOpenGLContext>
#include <QQuickGraphicsDevice>
#include <QTimer>
#include <QtCore/QRunnable>
#include <QtQuick/qquickwindow.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>

 // Shader sources (same as LayersRenderer but compatible with QOpenGLShaderProgram)
constexpr const char* VideoVert = R"(
#version 330 core

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
#version 330 core

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
#version 330 core

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
#version 330 core

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
#version 330 core

uniform sampler2D tex;
uniform int eye;
uniform int stereoscopicMode;
uniform float alpha;
uniform int videoWidth;
uniform int videoHeight;

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

vec2 xyz_to_eac(vec3 xyz, int width, int height)
{
    float pixel_pad = 2;
    float u_pad = pixel_pad / width;
    float v_pad = pixel_pad / height;

    int in_cubemap_face_order[6];
    in_cubemap_face_order[RIGHT] = TOP_RIGHT;
    in_cubemap_face_order[LEFT]  = TOP_LEFT;
    in_cubemap_face_order[UP]    = BOTTOM_RIGHT;
    in_cubemap_face_order[DOWN]  = BOTTOM_LEFT;
    in_cubemap_face_order[FRONT] = TOP_MIDDLE;
    in_cubemap_face_order[BACK]  = BOTTOM_MIDDLE;

    int in_cubemap_face_rotation[6];
    in_cubemap_face_rotation[TOP_LEFT]      = ROT_0;
    in_cubemap_face_rotation[TOP_MIDDLE]    = ROT_0;
    in_cubemap_face_rotation[TOP_RIGHT]     = ROT_0;
    in_cubemap_face_rotation[BOTTOM_LEFT]   = ROT_270;
    in_cubemap_face_rotation[BOTTOM_MIDDLE] = ROT_90;
    in_cubemap_face_rotation[BOTTOM_RIGHT]  = ROT_270;

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
    vec2 uv = xyz_to_eac(normalize(tr_normal), videoWidth, videoHeight);

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

void LayersRendererQtItem::updateCameraMatrices() {
    // Use the item's own dimensions, not the full window, for correct aspect ratio
    const float w = static_cast<float>(width());
    const float h = static_cast<float>(height());
    const float aspectRatio = (h > 0.0f) ? w / h : 1.0f;

    QMatrix4x4 rotMatrix;
    rotMatrix.rotate(m_cameraEulerRotation.y(), 0.0f, 1.0f, 0.0f);
    rotMatrix.rotate(m_cameraEulerRotation.x(), 1.0f, 0.0f, 0.0f);
    rotMatrix.rotate(m_cameraEulerRotation.z(), 0.0f, 0.0f, 1.0f);

    QVector3D forward = rotMatrix.map(QVector3D(0.0f, 0.0f, -1.0f)).normalized();
    QVector3D up = rotMatrix.map(QVector3D(0.0f, 1.0f, 0.0f)).normalized();

    QMatrix4x4 viewMatrix;
    viewMatrix.lookAt(m_cameraPosition, m_cameraPosition + forward, up);

    QMatrix4x4 projectionMatrix;
    projectionMatrix.perspective(m_fieldOfView, aspectRatio, 0.1f, 1000.0f);

    if (m_renderer) {
        m_renderer->setCameraParams(viewMatrix, projectionMatrix);
    }
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
    if (m_renderer) {
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
    window()->scheduleRenderJob(new CleanupJob(m_renderer), QQuickWindow::BeforeSynchronizingStage);
    m_renderer = nullptr;
}

void LayersRendererQtItem::sync() {
    if (!m_renderer) {
        m_renderer = new LayersRendererQtOpenGLObject(this);
        connect(window(), &QQuickWindow::beforeRendering, m_renderer, &LayersRendererQtOpenGLObject::init, Qt::DirectConnection);
        connect(window(), &QQuickWindow::beforeRenderPassRecording, m_renderer, &LayersRendererQtOpenGLObject::paint, Qt::DirectConnection);
    }
    m_renderer->setWindow(window());
    m_renderer->setItemVisible(isVisible());
    m_renderer->updateMeshes(m_meshRadius, m_meshFov, m_meshAngle);

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
    m_sphereMesh.reset();
}

void LayersRendererQtOpenGLObject::setWindow(QQuickWindow* window) {
    m_window = window;
}

void LayersRendererQtOpenGLObject::setCameraParams(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    m_viewMatrix = viewMatrix;
    m_projectionMatrix = projectionMatrix;
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
    m_EACStereoscopicModeLoc = m_EACPrg->uniformLocation("stereoscopicMode");
    m_EACAlphaLoc = m_EACPrg->uniformLocation("alpha");
    m_EACOutsideLoc = m_EACPrg->uniformLocation("outside");
    m_EACScaleLoc = m_EACPrg->uniformLocation("scaleToUnitCube");
    m_EACVideoWidthLoc = m_EACPrg->uniformLocation("videoWidth");
    m_EACVideoHeightLoc = m_EACPrg->uniformLocation("videoHeight");
    m_EACPrg->release();
}

void LayersRendererQtOpenGLObject::initializeGL() {
    createShaders();

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
        m_domeMesh.reset();
        m_sphereMesh.reset();
        m_meshRadius = radius;
        m_meshFov = fov;
        m_domeMesh = std::make_unique<DomeGrid>(float(m_meshRadius) / 100.f, float(m_meshFov), 256, 128);
        m_sphereMesh = std::make_unique<SphereGrid>(float(m_meshRadius) / 100.f, 256);
    }
    m_meshAngle = angle;
}

void LayersRendererQtOpenGLObject::renderQuad() {
    m_quadVAO.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_quadVAO.release();
}

void LayersRendererQtOpenGLObject::renderLayer(const BaseLayer* layer, int eyeMode, float angle,
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    if (!layer || !layer->ready() || layer->alpha() <= 0.0f) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->textureId());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (layer->gridMode() == 4) {
        m_EACPrg->bind();

        m_EACPrg->setUniformValue(m_EACAlphaLoc, layer->alpha());
        m_EACPrg->setUniformValue(m_EACOutsideLoc, 0);
        m_EACPrg->setUniformValue(m_EACVideoWidthLoc, layer->width());
        m_EACPrg->setUniformValue(m_EACVideoHeightLoc, layer->height());

        if (layer->stereoMode() > 0) {
            m_EACPrg->setUniformValue(m_EACEyeModeLoc, eyeMode);
            m_EACPrg->setUniformValue(m_EACStereoscopicModeLoc, (int)layer->stereoMode());
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
        mvpRot.rotate(layer->rotate().y + 90.f, 0, 1, 0);               // yaw
        mvpRot.rotate(90.f, 0, 0, 1);                                    // roll

        m_EACPrg->setUniformValue(m_EACMatrixLoc, mvpRot);

        if (m_sphereMesh) {
            m_sphereMesh->draw();
        }

        m_EACPrg->release();
    }
    else if (layer->gridMode() == 3) {
        // EQR sphere rendering
        QMatrix4x4 mvp = projectionMatrix * viewMatrix;
        QVector3D translate(layer->translate().x, layer->translate().y, layer->translate().z);
        mvp.translate(translate);

        QMatrix4x4 mvpRot = mvp;
        mvpRot.rotate(layer->rotate().z, 0, 0, 1);  // roll
        mvpRot.rotate(layer->rotate().x, 1, 0, 0);  // pitch
        mvpRot.rotate(layer->rotate().y, 0, 1, 0);  // yaw

        m_meshPrg->bind();

        if (layer->stereoMode() > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, (int)layer->stereoMode());
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

        if (m_sphereMesh) {
            m_sphereMesh->draw();
        }

        m_meshPrg->release();
    }
    else if (layer->gridMode() == 2) {
        // Dome rendering
        m_meshPrg->bind();

        if (layer->stereoMode() > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, (int)layer->stereoMode());
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
    else if (layer->gridMode() == 1) {
        // Plane rendering
        m_meshPrg->bind();

        if (layer->stereoMode() > 0) {
            m_meshPrg->setUniformValue(m_meshEyeModeLoc, eyeMode);
            m_meshPrg->setUniformValue(m_meshStereoscopicModeLoc, (int)layer->stereoMode());
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

        if (layer->stereoMode() > 0) {
            m_videoPrg->setUniformValue(m_videoEyeModeLoc, eyeMode);
            m_videoPrg->setUniformValue(m_videoStereoscopicModeLoc, (int)layer->stereoMode());
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

void LayersRendererQtOpenGLObject::updateLayers() {
    if (!Application::isCreated() || !Application::instance().slidesModel()) {
        return;
    }

    for (int s = -1; s < Application::instance().slidesModel()->numberOfSlides(); s++) {
        auto* slide = Application::instance().slidesModel()->slide(s);
        if (!slide) continue;

        int numLayers = slide->numberOfLayers();
        for (int l = numLayers - 1; l >= 0; l--) {
            BaseLayer* layer = slide->layer(l);
            if (layer) {
                if (layer->ready() && (layer->alpha() > 0.f)) {
                    if (layer->gridMode() == BaseLayer::GridMode::Plane) {
                        if (!layer->hasPlane() || layer->needSync()) {
                            layer->updatePlane();
                        }
                    }
                    //SlideModel updates continuously the layers, so all we should need to do is update the frame.
                    layer->updateFrame();
                }
            }
        }
    }
}

void LayersRendererQtOpenGLObject::renderLayers(float angle,
    const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    int eyeMode = 1; // Default to left eye/mono

    if (!Application::isCreated() || !Application::instance().slidesModel()) {
        return;
    }

    for (int s = -1; s < Application::instance().slidesModel()->numberOfSlides(); s++) {
        auto* slide = Application::instance().slidesModel()->slide(s);
        if (!slide) continue;

        int numLayers = slide->numberOfLayers();
        for (int l = numLayers - 1; l >= 0; l--) {
            BaseLayer* layer = slide->layer(l);
            if (layer) {
                if (layer->ready() && (layer->alpha() > 0.f)) {
                    renderLayer(layer, eyeMode, angle, viewMatrix, projectionMatrix);
                }
            }
        }
    }
}

void LayersRendererQtOpenGLObject::setViewportRect(const QRect& rect) {
    m_viewportRect = rect;
}

void LayersRendererQtOpenGLObject::setItemVisible(bool visible) {
    m_itemVisible = visible;
}

void LayersRendererQtOpenGLObject::init() {
    if (m_initialized)
        return;

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
        
void LayersRendererQtOpenGLObject::paint() {
    if (!m_initialized || !m_itemVisible) {
        return;
    }

    m_window->beginExternalCommands();

    updateLayers();

    // Use the anchored item rect instead of the full window size
    glViewport(m_viewportRect.x(), m_viewportRect.y(),
               m_viewportRect.width(), m_viewportRect.height());

    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    renderLayers(m_meshAngle, m_viewMatrix, m_projectionMatrix);

    m_window->endExternalCommands();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_window->resetOpenGLState();
#endif
}