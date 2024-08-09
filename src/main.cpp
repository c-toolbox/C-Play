/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sgct/sgct.h>
#include <sgct/opengl.h>
#include <sgct/utils/dome.h>
#include <sgct/utils/sphere.h>
#include <sgct/utils/plane.h>
#include <sgct/offscreenbuffer.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <layers/baselayer.h>
#include <layers/imagelayer.h>
#include <layers/mpvlayer.h>
#include "application.h"
#include <client.h>
#include <render_gl.h>
#include <fstream>   
#include <mutex>

//#define SGCT_ONLY

namespace {

bool allowDirectRendering = false;

std::mutex logMutex;
std::ofstream logFile;
std::string logFilePath = "";
std::string logLevel = "";
std::string startupFile = "";

std::unique_ptr<sgct::utils::Dome> domeMesh;
std::unique_ptr<sgct::utils::Sphere> sphereMesh;

int domeRadius = 740;
int domeFov = 165;

std::vector<BaseLayer*> allLayers;
std::vector<BaseLayer*> layers2render;
ImageLayer* backgroundImageLayer;
ImageLayer* foregroundImageLayer;
ImageLayer* overlayImageLayer;
MpvLayer* mainMpvLayer;

bool fadeDurationOngoing = false;
// video
int videoAlphaLoc = -1;
int videoEyeModeLoc = -1;
int videoStereoscopicModeLoc = -1;
// mesh
int meshAlphaLoc = -1;
int meshEyeModeLoc = -1;
int meshMatrixLoc = -1;
int meshOutsideLoc = -1;
int meshStereoscopicModeLoc = -1;
// EAC
int EACAlphaLoc = -1;
int EACMatrixLoc = -1;
int EACScaleLoc = -1;
int EACOutsideLoc = -1;
int EACVideoWidthLoc = -1;
int EACVideoHeightLoc = -1;
int EACEyeModeLoc = -1;
int EACStereoscopicModeLoc = -1;

constexpr std::string_view VideoVert = R"(
  #version 410 core

  layout (location = 0) in vec2 in_position;
  layout (location = 1) in vec2 in_texCoord;

  uniform int eye;
  uniform int stereoscopicMode;

  out vec2 tr_uv;

  void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    tr_uv = in_texCoord;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (in_texCoord * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = in_texCoord * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = in_texCoord * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = in_texCoord * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

constexpr std::string_view MeshVert = R"(
  #version 410 core

  layout (location = 0) in vec2 in_texCoord;
  layout (location = 1) in vec3 in_normal;
  layout (location = 2) in vec3 in_position;

  uniform mat4 mvp;
  uniform int eye;
  uniform int stereoscopicMode;

  out vec2 tr_uv;
  out vec3 tr_normals;

  void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_uv = in_texCoord;
    tr_normals = in_normal;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (in_texCoord * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = in_texCoord * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = in_texCoord * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { // Left Eye or Mono
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = in_texCoord * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

constexpr std::string_view VideoFrag = R"(
  #version 410 core

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

constexpr std::string_view EACMeshVert = R"(
  #version 410 core

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

constexpr std::string_view EACVideoFrag = R"(
  #version 410 core

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

  /**
  * Calculate direction from corresponding 3D coordinates on sphere.
  * @param vec3 coordinated on sphere
  * @return direction direction of view
  */
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

  /**
   * Calculate frame position in equi-angular cubemap format for corresponding 3D coordinates on sphere.
   *
   * @param xyz coordinates on sphere
   * @param width frame width
   * @param height frame height
   * @return uv texture coordinate
   */
  vec2 xyz_to_eac(vec3 xyz, int width, int height)
  {
    // EAC has 2-pixel padding on faces except between faces on the same row
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

} // namespace

using namespace sgct;

const ShaderProgram* videoPrg;
const ShaderProgram* meshPrg;
const ShaderProgram* EACPrg;
const ShaderProgram* createCubeMapPrg;

void initOGL(GLFWwindow*) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    // Create standard layers
    backgroundImageLayer = new ImageLayer("background");
    allLayers.push_back(backgroundImageLayer);

    mainMpvLayer = new MpvLayer(allowDirectRendering, !logFilePath.empty() || !logLevel.empty(), logLevel);
    mainMpvLayer->initialize();
    mainMpvLayer->loadFile(SyncHelper::instance().variables.loadedFile);
    allLayers.push_back(mainMpvLayer);

    overlayImageLayer = new ImageLayer("overlay");
    allLayers.push_back(overlayImageLayer);

    foregroundImageLayer = new ImageLayer("foreground");
    allLayers.push_back(foregroundImageLayer);

    // Create shaders
    ShaderManager::instance().addShaderProgram("mesh", MeshVert, VideoFrag);
    ShaderManager::instance().addShaderProgram("EAC", EACMeshVert, EACVideoFrag);
    ShaderManager::instance().addShaderProgram("video", VideoVert, VideoFrag);

    //OBS: Need to create all shaders befor using any of them. Bug?
    meshPrg = &ShaderManager::instance().shaderProgram("mesh");
    meshPrg->bind();
    glUniform1i(glGetUniformLocation(meshPrg->id(), "tex"), 0);
    meshMatrixLoc = glGetUniformLocation(meshPrg->id(), "mvp");
    meshEyeModeLoc = glGetUniformLocation(meshPrg->id(), "eye");
    meshStereoscopicModeLoc = glGetUniformLocation(meshPrg->id(), "stereoscopicMode");
    meshAlphaLoc = glGetUniformLocation(meshPrg->id(), "alpha");
    meshOutsideLoc = glGetUniformLocation(meshPrg->id(), "outside");
    meshPrg->unbind();

    EACPrg = &ShaderManager::instance().shaderProgram("EAC");
    EACPrg->bind();
    glUniform1i(glGetUniformLocation(EACPrg->id(), "tex"), 0);
    EACMatrixLoc = glGetUniformLocation(EACPrg->id(), "mvp");
    EACEyeModeLoc = glGetUniformLocation(EACPrg->id(), "eye");
    EACStereoscopicModeLoc = glGetUniformLocation(EACPrg->id(), "stereoscopicMode");
    EACAlphaLoc = glGetUniformLocation(EACPrg->id(), "alpha");
    EACOutsideLoc = glGetUniformLocation(meshPrg->id(), "outside");
    EACScaleLoc = glGetUniformLocation(EACPrg->id(), "scaleToUnitCube");
    EACVideoWidthLoc = glGetUniformLocation(EACPrg->id(), "videoWidth");
    EACVideoHeightLoc = glGetUniformLocation(EACPrg->id(), "videoHeight");
    EACPrg->unbind();

    videoPrg = &ShaderManager::instance().shaderProgram("video");
    videoPrg->bind();
    glUniform1i(glGetUniformLocation(videoPrg->id(), "tex"), 0);
    videoEyeModeLoc = glGetUniformLocation(videoPrg->id(), "eye");
    videoStereoscopicModeLoc = glGetUniformLocation(videoPrg->id(), "stereoscopicMode");
    videoAlphaLoc = glGetUniformLocation(videoPrg->id(), "alpha");
    videoPrg->unbind();

    // create meshes
    domeRadius = SyncHelper::instance().variables.radius;
    domeFov = SyncHelper::instance().variables.fov;
    domeMesh = std::make_unique<utils::Dome>(float(domeRadius)/100.f, float(domeFov), 256, 128);
    sphereMesh = std::make_unique<utils::Sphere>(float(domeRadius) / 100.f, 256);

    // Set up backface culling
    glCullFace(GL_BACK);
    // our polygon winding is clockwise since we are inside of the dome
    glFrontFace(GL_CW);
}

void preSync() {

}

std::vector<std::byte> encode() {
    std::vector<std::byte> data;
    serializeObject(data, SyncHelper::instance().variables.syncOn);
    serializeObject(data, SyncHelper::instance().variables.alpha);
    serializeObject(data, SyncHelper::instance().variables.alphaBg);
    serializeObject(data, SyncHelper::instance().variables.alphaFg);

    if (SyncHelper::instance().variables.syncOn) {
        serializeObject(data, SyncHelper::instance().variables.paused);
        serializeObject(data, SyncHelper::instance().variables.timePosition);
        serializeObject(data, SyncHelper::instance().variables.timeThreshold);
        serializeObject(data, SyncHelper::instance().variables.timeThresholdEnabled);
        serializeObject(data, SyncHelper::instance().variables.timeThresholdOnLoopOnly);
        serializeObject(data, SyncHelper::instance().variables.timeDirty);
        serializeObject(data, SyncHelper::instance().variables.gridToMapOn);
        serializeObject(data, SyncHelper::instance().variables.gridToMapOnBg);
        serializeObject(data, SyncHelper::instance().variables.gridToMapOnFg);
        serializeObject(data, SyncHelper::instance().variables.stereoscopicMode);
        serializeObject(data, SyncHelper::instance().variables.stereoscopicModeBg);
        serializeObject(data, SyncHelper::instance().variables.stereoscopicModeFg);
        serializeObject(data, SyncHelper::instance().variables.eofMode);
        serializeObject(data, SyncHelper::instance().variables.viewMode);
        serializeObject(data, SyncHelper::instance().variables.radius);
        serializeObject(data, SyncHelper::instance().variables.fov);
        serializeObject(data, SyncHelper::instance().variables.angle);
        serializeObject(data, SyncHelper::instance().variables.rotateX);
        serializeObject(data, SyncHelper::instance().variables.rotateY);
        serializeObject(data, SyncHelper::instance().variables.rotateZ);
        serializeObject(data, SyncHelper::instance().variables.translateX);
        serializeObject(data, SyncHelper::instance().variables.translateY);
        serializeObject(data, SyncHelper::instance().variables.translateZ);
        serializeObject(data, SyncHelper::instance().variables.planeWidth);
        serializeObject(data, SyncHelper::instance().variables.planeHeight);
        serializeObject(data, SyncHelper::instance().variables.planeElevation);
        serializeObject(data, SyncHelper::instance().variables.planeDistance);
        serializeObject(data, SyncHelper::instance().variables.planeConsiderAspectRatio);

        //Eq
        serializeObject(data, SyncHelper::instance().variables.eqDirty);
        if (SyncHelper::instance().variables.eqDirty) {
            serializeObject(data, SyncHelper::instance().variables.eqContrast);
            serializeObject(data, SyncHelper::instance().variables.eqBrightness);
            serializeObject(data, SyncHelper::instance().variables.eqGamma);
            serializeObject(data, SyncHelper::instance().variables.eqSaturation);
        }

        //Looptime
        serializeObject(data, SyncHelper::instance().variables.loopTimeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            serializeObject(data, SyncHelper::instance().variables.loopTimeEnabled);
            serializeObject(data, SyncHelper::instance().variables.loopTimeA);
            serializeObject(data, SyncHelper::instance().variables.loopTimeB);
        }

        // As strings can be quiet long.
        // Saving connection load, to only send one URL at a time.
        if (SyncHelper::instance().variables.loadFile) { // ID: 0 = mpv media file
            serializeObject(data, 0);
            serializeObject(data, SyncHelper::instance().variables.loadFile);
            serializeObject(data, SyncHelper::instance().variables.loadedFile);
            SyncHelper::instance().variables.loadFile = false;
        }
        else if (SyncHelper::instance().variables.overlayFileDirty) { // ID: 1 = overlay image file
            serializeObject(data, 1);
            serializeObject(data, SyncHelper::instance().variables.overlayFileDirty);
            serializeObject(data, SyncHelper::instance().variables.overlayFile);
            SyncHelper::instance().variables.overlayFileDirty = false;
        }
        else if (SyncHelper::instance().variables.bgImageFileDirty) { // ID: 2 = background image file
            serializeObject(data, 2);
            serializeObject(data, SyncHelper::instance().variables.bgImageFileDirty);
            serializeObject(data, SyncHelper::instance().variables.bgImageFile);
            SyncHelper::instance().variables.bgImageFileDirty = false;
        }
        else if (SyncHelper::instance().variables.fgImageFileDirty) { // ID: 3 = foreground image file
            serializeObject(data, 3);
            serializeObject(data, SyncHelper::instance().variables.fgImageFileDirty);
            serializeObject(data, SyncHelper::instance().variables.fgImageFile);
            SyncHelper::instance().variables.fgImageFileDirty = false;
        }
        else { // Sending no URL
            serializeObject(data, -1);
        }

        //Reset flags every frame cycle
        SyncHelper::instance().variables.timeDirty = false;
        SyncHelper::instance().variables.eqDirty = false;
        SyncHelper::instance().variables.loopTimeDirty = false;
    }

    return data;
}

void decode(const std::vector<std::byte>& data) {
    unsigned pos = 0;
    deserializeObject(data, pos, SyncHelper::instance().variables.syncOn);
    deserializeObject(data, pos, SyncHelper::instance().variables.alpha);
    deserializeObject(data, pos, SyncHelper::instance().variables.alphaBg);
    deserializeObject(data, pos, SyncHelper::instance().variables.alphaFg);
    if (SyncHelper::instance().variables.syncOn) {
        deserializeObject(data, pos, SyncHelper::instance().variables.paused);
        deserializeObject(data, pos, SyncHelper::instance().variables.timePosition);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThreshold);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThresholdEnabled);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThresholdOnLoopOnly);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeDirty);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOn);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOnBg);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOnFg);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicModeBg);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicModeFg);
        deserializeObject(data, pos, SyncHelper::instance().variables.eofMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.viewMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.radius);
        deserializeObject(data, pos, SyncHelper::instance().variables.fov);
        deserializeObject(data, pos, SyncHelper::instance().variables.angle);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateZ);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateZ);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeWidth);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeHeight);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeElevation);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeDistance);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeConsiderAspectRatio);

        //Eq
        deserializeObject(data, pos, SyncHelper::instance().variables.eqDirty);
        if (SyncHelper::instance().variables.eqDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.eqContrast);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqBrightness);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqGamma);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqSaturation);
        }

        //Looptime
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeEnabled);
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeA);
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeB);
        }

        //Strings
        int transferedImageId = -1;
        deserializeObject(data, pos, transferedImageId);

        if (transferedImageId == 0) {
            deserializeObject(data, pos, SyncHelper::instance().variables.loadFile);
            deserializeObject(data, pos, SyncHelper::instance().variables.loadedFile);
        }
        else if (transferedImageId == 1) {
            deserializeObject(data, pos, SyncHelper::instance().variables.overlayFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.overlayFile);
        }
        else if (transferedImageId == 2) {
            deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFile);
        }
        else if (transferedImageId == 3) {
            deserializeObject(data, pos, SyncHelper::instance().variables.fgImageFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.fgImageFile);
        }
    }
}

void postSyncPreDraw() {
#ifndef SGCT_ONLY
    //Apply synced commands
    if (!Engine::instance().isMaster()) {

        glm::vec3 rotXYZ = glm::vec3(float(SyncHelper::instance().variables.rotateX),
            float(SyncHelper::instance().variables.rotateY),
            float(SyncHelper::instance().variables.rotateZ));

        glm::vec3 translateXYZ = glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f,
            float(SyncHelper::instance().variables.translateY) / 100.f,
            float(SyncHelper::instance().variables.translateZ) / 100.f);

        bool newImage = false;

        //Process background image loading
        newImage = backgroundImageLayer->processImageUpload(SyncHelper::instance().variables.bgImageFile, SyncHelper::instance().variables.bgImageFileDirty);
        if (newImage) {
            backgroundImageLayer->setRotate(rotXYZ);
            backgroundImageLayer->setTranslate(translateXYZ);
        }
        SyncHelper::instance().variables.bgImageFileDirty = false;

        //Process foreground image loading
        newImage = foregroundImageLayer->processImageUpload(SyncHelper::instance().variables.fgImageFile, SyncHelper::instance().variables.fgImageFileDirty);
        if (newImage) {
            foregroundImageLayer->setRotate(rotXYZ);
            foregroundImageLayer->setTranslate(translateXYZ);
        }
        SyncHelper::instance().variables.fgImageFileDirty = false;

        //Process overlay image loading
        newImage = overlayImageLayer->processImageUpload(SyncHelper::instance().variables.overlayFile, SyncHelper::instance().variables.overlayFileDirty);
        SyncHelper::instance().variables.overlayFileDirty = false;

        if (!SyncHelper::instance().variables.loadedFile.empty()) {            
            //Load new MPV file
            mainMpvLayer->loadFile(SyncHelper::instance().variables.loadedFile, SyncHelper::instance().variables.loadFile);
            SyncHelper::instance().variables.loadFile = false;
        }

        layers2render.clear();

        if ((!mainMpvLayer->renderingIsOn() || mainMpvLayer->hasLoadedFile() ||
            (SyncHelper::instance().variables.alpha < 1.f || SyncHelper::instance().variables.gridToMapOn == 1))
            && !backgroundImageLayer->hasLoadedFile() && SyncHelper::instance().variables.alphaBg > 0.f) {
            backgroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaBg);
            backgroundImageLayer->setGridMode(SyncHelper::instance().variables.gridToMapOnBg);
            backgroundImageLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicModeBg);
            layers2render.push_back(backgroundImageLayer);
        }

        if (mainMpvLayer->renderingIsOn()) {
            if (!mainMpvLayer->hasLoadedFile() && SyncHelper::instance().variables.alpha > 0.f) {
                mainMpvLayer->setAlpha(SyncHelper::instance().variables.alpha);
                mainMpvLayer->setGridMode(SyncHelper::instance().variables.gridToMapOn);
                mainMpvLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicMode);
                mainMpvLayer->setRotate(rotXYZ);
                mainMpvLayer->setTranslate(translateXYZ);
                layers2render.push_back(mainMpvLayer);
            }

            if (!overlayImageLayer->hasLoadedFile() && SyncHelper::instance().variables.alpha > 0.f) {
                overlayImageLayer->setAlpha(SyncHelper::instance().variables.alpha);
                overlayImageLayer->setGridMode(SyncHelper::instance().variables.gridToMapOn);
                overlayImageLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicMode);
                overlayImageLayer->setRotate(rotXYZ);
                overlayImageLayer->setTranslate(translateXYZ);
                layers2render.push_back(overlayImageLayer);
            }

            //If we have 2D and 3D viewports defined, deside based on renderParams which to render
            //1. Check what stereo mode we should choose
            //2. For each window, check if there is a mix of viewports (2D and 3D). If not mix, skip step 3 (for that window).
            //3. Enable/disable viewport based on defined stereo mode

            //Step 1
            bool show2Dcontent = false;
            bool show3Dcontent = false;
            bool has3Dplane = false;
            for (const auto& layer : layers2render) {
                if (layer->stereoMode() > 0) {
                    show3Dcontent = true;
                    if (layer->gridMode() == 1) {
                        has3Dplane = true;
                    }
                }
                else {
                    show2Dcontent = true;
                }
            }
            //If we have one 3D renderParam visible, it takes president
            if (show3Dcontent) {
                show2Dcontent = false;
            }
            else { //If not 2D or 3D, still enable 2D viewports
                show2Dcontent = true;
            }

            //If viewMode==1, that means the user has asked to force all content to 2D
            if (SyncHelper::instance().variables.viewMode == 1) {
                show2Dcontent = true;
                show3Dcontent = false;
            }

            for (const std::unique_ptr<Window>& win : Engine::instance().thisNode().windows()) {
                bool exist2Dviewports = false;
                bool exist3Dviewports = false;
                // Step 2
                for (const std::unique_ptr<Viewport>& vp : win->viewports()) {
                    if (vp->eye() == Frustum::Mode::MonoEye) {
                        exist2Dviewports = true;
                    }
                    else if (vp->eye() == Frustum::Mode::StereoLeftEye || vp->eye() == Frustum::Mode::StereoRightEye) {
                        exist3Dviewports = true;
                    }
                }
                // Step 3
                if (exist2Dviewports && exist3Dviewports) {
                    for (const std::unique_ptr<Viewport>& vp : win->viewports()) {
                        if (show2Dcontent && (vp->eye() == Frustum::Mode::MonoEye)) {
                            vp->setEnabled(true);
                        }
                        else if (show3Dcontent && (vp->eye() == Frustum::Mode::StereoLeftEye || vp->eye() == Frustum::Mode::StereoRightEye)) {
                            vp->setEnabled(true);
                        }
                        else {
                            vp->setEnabled(false);
                        }
                    }
                }
            }
            
        }

        if (!foregroundImageLayer->hasLoadedFile() && SyncHelper::instance().variables.alphaFg > 0.f) {
            foregroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaFg);
            foregroundImageLayer->setGridMode(SyncHelper::instance().variables.gridToMapOnFg);
            foregroundImageLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicModeFg);
            layers2render.push_back(foregroundImageLayer);
        }
        
        // Set properties of main mpv layer
        mainMpvLayer->setPause(SyncHelper::instance().variables.paused);
        mainMpvLayer->setEOFMode(SyncHelper::instance().variables.eofMode);
        mainMpvLayer->setTimePosition(
            SyncHelper::instance().variables.timePosition, 
            SyncHelper::instance().variables.timeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            mainMpvLayer->setLoopTime(
                SyncHelper::instance().variables.loopTimeA, 
                SyncHelper::instance().variables.loopTimeB, 
                SyncHelper::instance().variables.loopTimeEnabled);
        }
        if (SyncHelper::instance().variables.eqDirty) {
            mainMpvLayer->setValue("contrast", SyncHelper::instance().variables.eqContrast);
            mainMpvLayer->setValue("brightness", SyncHelper::instance().variables.eqBrightness);
            mainMpvLayer->setValue("gamma", SyncHelper::instance().variables.eqGamma);
            mainMpvLayer->setValue("saturation", SyncHelper::instance().variables.eqSaturation);
        }

        // Set latest plane details for all layers
        glm::vec2 planeSize = glm::vec2(float(SyncHelper::instance().variables.planeWidth), float(SyncHelper::instance().variables.planeHeight));
        for (const auto& layer : allLayers) {
            layer->setPlaneDistance(SyncHelper::instance().variables.planeDistance);
            layer->setPlaneElevation(SyncHelper::instance().variables.planeElevation);
            layer->setPlaneSize(planeSize, SyncHelper::instance().variables.planeConsiderAspectRatio);
        }

        // Set new general dome/sphere details
        if (domeRadius != SyncHelper::instance().variables.radius || domeFov != SyncHelper::instance().variables.fov) {
            domeMesh = nullptr;
            sphereMesh = nullptr;
            domeRadius = SyncHelper::instance().variables.radius;
            domeFov = SyncHelper::instance().variables.fov;
            domeMesh = std::make_unique<utils::Dome>(float(domeRadius) / 100.f, float(domeFov), 256, 128);
            sphereMesh = std::make_unique<utils::Sphere>(float(domeRadius) / 100.f, 256);
        }
    }

    if (Engine::instance().isMaster()) {
        return;
    }
#endif

    // Update/render the frame from MPV pipeline
    mainMpvLayer->updateFrame();
}

void draw(const RenderData& data) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif
    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);

    glDisable(GL_BLEND);

    Frustum::Mode currentEye = data.frustumMode;

    //Check if we force all viewports to 2D, meaning only show LeftEye if 3D
    if (SyncHelper::instance().variables.viewMode == 1 && currentEye == Frustum::Mode::StereoRightEye) {
        currentEye = Frustum::Mode::StereoLeftEye;
    }

    for (const auto& layer : layers2render) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, layer->textureId());
        glEnable(GL_BLEND);

        if (layer->gridMode() == 4) {
            glEnable(GL_CULL_FACE);

            EACPrg->bind();

            glUniform1f(EACAlphaLoc, layer->alpha());
            glUniform1i(EACOutsideLoc, 0);
            glUniform1i(EACVideoWidthLoc, layer->width());
            glUniform1i(EACVideoHeightLoc, layer->height());

            if (layer->stereoMode() > 0) {
                glUniform1i(EACEyeModeLoc, (GLint)currentEye);
                glUniform1i(EACStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(EACEyeModeLoc, 0);
                glUniform1i(EACStereoscopicModeLoc, 0);
            }

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), layer->translate());

            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().x), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().y + 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(90.f), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            glUniformMatrix4fv(EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            sphereMesh->draw();

            // Set up frontface culling
            glCullFace(GL_FRONT);

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().x + float(SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().y - 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - 90.f), glm::vec3(0.0f, 0.0f, 1.0f)); //roll

            glUniformMatrix4fv(EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
            //render outside sphere
            glUniform1i(EACOutsideLoc, 1);
            sphereMesh->draw();

            // Set up backface culling again
            glCullFace(GL_BACK);
            glUniform1i(EACOutsideLoc, 0);

            EACPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (layer->gridMode() == 3) {
            glEnable(GL_CULL_FACE);

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), layer->translate());

            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().x), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            meshPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)currentEye);
                glUniform1i(meshStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, layer->alpha());

            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            //render inside sphere
            glUniform1i(meshOutsideLoc, 0);
            sphereMesh->draw();

            // Set up frontface culling
            glCullFace(GL_FRONT);

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().x + float(SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
            //render outside sphere
            glUniform1i(meshOutsideLoc, 1);
            sphereMesh->draw();

            // Set up backface culling again
            glCullFace(GL_BACK);
            glUniform1i(meshOutsideLoc, 0);

            meshPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (layer->gridMode() == 2) {
            glEnable(GL_CULL_FACE);

            meshPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)currentEye);
                glUniform1i(meshStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, layer->alpha());

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed_rot = glm::translate(glm::make_mat4(mvp.values), layer->translate());
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().x - float(SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            domeMesh->draw();

            meshPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (layer->gridMode() == 1) {
            glEnable(GL_CULL_FACE);
            // Set up frontface culling
            glCullFace(GL_FRONT);

            meshPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)currentEye);
                glUniform1i(meshStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, layer->alpha());

            const sgct::mat4 mvp = data.projectionMatrix * data.viewMatrix;

            glm::mat4 planeTransform = glm::mat4(1.0f);
            //planeTransform = glm::rotate(planeTransform, glm::radians(float(SyncHelper::instance().variables.planeAzimuth)), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
            planeTransform = glm::rotate(planeTransform, glm::radians(float(SyncHelper::instance().variables.planeElevation)), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
            //planeTransform = glm::rotate(planeTransform, glm::radians(float(SyncHelper::instance().variables.planeRoll)), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            planeTransform = glm::translate(planeTransform, glm::vec3(0.0f, 0.0f, float(-SyncHelper::instance().variables.planeDistance) / 100.f)); //distance
            planeTransform = glm::make_mat4(mvp.values) * planeTransform;
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &planeTransform[0][0]);

            layer->drawPlane();

            meshPrg->unbind();

            // Set up backface culling again
            glCullFace(GL_BACK);

            glDisable(GL_CULL_FACE);
        }
        else {
            videoPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(videoEyeModeLoc, (GLint)currentEye);
                glUniform1i(videoStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(videoEyeModeLoc, 0);
                glUniform1i(videoStereoscopicModeLoc, 0);
            }

            glUniform1f(videoAlphaLoc, layer->alpha());

            data.window.renderScreenQuad();

            videoPrg->unbind();
        }
    }
    glDisable(GL_BLEND);
}

void cleanup() {
    if (!logFilePath.empty()) {
        logFile.close();
    }

#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    //Cleanup mainMpvLayer
    mainMpvLayer->cleanup();
    delete mainMpvLayer;
    mainMpvLayer = nullptr;

    delete backgroundImageLayer;
    delete foregroundImageLayer;
    delete overlayImageLayer;

    domeMesh = nullptr;
    sphereMesh = nullptr;
}

void logging(Log::Level, std::string_view message) {
    std::lock_guard<std::mutex> lock(logMutex);
    logFile << message << std::endl;
}

int main(int argc, char *argv[])
{
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);
    if (!cluster.success) {
        return -1;
    }

    //Look for C-Play command line specific things
    size_t i = 0;
    while (i < arg.size()) {
        if (arg[i] == "--mpvconf") {
            std::string mpvConfFolder = arg[i + 1]; // for instance, either "decoding_cpu" or "decoding_cpu"
            SyncHelper::instance().configuration.confAll = "./data/mpv-conf/" + mpvConfFolder + "/all.json";
            SyncHelper::instance().configuration.confMasterOnly = "./data/mpv-conf/" + mpvConfFolder + "/master-only.json";
            SyncHelper::instance().configuration.confNodesOnly = "./data/mpv-conf/" + mpvConfFolder + "/nodes-only.json";
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else if (arg[i] == "--allowDirectRendering") {
            allowDirectRendering = true;
            arg.erase(arg.begin() + i);
        }
        else if (arg[i] == "--loglevel") {
            //Valid log levels: error warn info debug
            std::string level = arg[i + 1];
            if(level == "error") {
                Log::instance().setNotifyLevel(Log::Level::Error);
                logLevel = level;
            }
            else if (level == "warn") {
                Log::instance().setNotifyLevel(Log::Level::Warning);
                logLevel = level;
            }
            else if (level == "info") {
                Log::instance().setNotifyLevel(Log::Level::Info);
                logLevel = level;
            }
            else if (level == "debug") {
                Log::instance().setNotifyLevel(Log::Level::Debug);
                logLevel = level;
            }
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else if (arg[i] == "--logfile") {
            std::string logFileName = arg[i + 1]; // for instance, either "log_master.txt" or "log_client.txt"
            logFilePath = "./data/log/" + logFileName;
            logFile.open(logFilePath, std::ofstream::out | std::ofstream::trunc);
            if (logLevel.empty()) { //Set log level to info if we specfied a log file
                Log::instance().setNotifyLevel(Log::Level::Info);
                logLevel = "info";
            }
            Log::instance().setShowLogLevel(true);
            Log::instance().setShowTime(true);
            Log::instance().setLogCallback(logging);
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else if (arg[i] == "--loadfile") {
            startupFile = arg[i + 1];
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else {
            // Ignore unknown commands
            i++;
        }
    }

    Engine::Callbacks callbacks;
    callbacks.initOpenGL = initOGL;
    callbacks.preSync = preSync;
    callbacks.encode = encode;
    callbacks.decode = decode;
    callbacks.postSyncPreDraw = postSyncPreDraw;
    callbacks.draw = draw;
    callbacks.cleanup = cleanup;
    try {
        Engine::create(cluster, callbacks, config);
    }
    catch (const std::runtime_error& e) {
        Log::Error(e.what());
        Engine::destroy();
        return EXIT_FAILURE;
    }

#ifndef SGCT_ONLY
    if (Engine::instance().isMaster()) {
        if(!ClusterManager::instance().ignoreSync() || ClusterManager::instance().numberOfNodes() > 1) {
            if(!NetworkManager::instance().areAllNodesConnected()) {
                Engine::destroy();
                return EXIT_FAILURE;
            }
        }

        Log::Info("Start Master");

        //Hide window (as we are not using it on master)
        Engine::instance().thisNode().windows().at(0)->setRenderWhileHidden(true);
        Engine::instance().thisNode().windows().at(0)->setVisible(false);

        //Do not support arguments to QApp, only SGCT
        std::vector<char*> cargv;
        cargv.push_back(argv[0]);
        int cargv_size = static_cast<int>(cargv.size());

        //Launch master application (which calls Engine::render from thread)
        Application::create(cargv_size, &cargv[0], QStringLiteral("C-Play"));
        Application::instance().setStartupFile(startupFile);
        return Application::instance().run();
    }
    else{
#endif
        Log::Info("Start Client");

        Engine::instance().exec();
        Engine::destroy();
        return EXIT_SUCCESS;
#ifndef SGCT_ONLY
    }
#endif

}

