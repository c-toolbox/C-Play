/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
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
#include "application.h"
#include <client.h>
#include <render_gl.h>
#include "qthelper.h"

//#define SGCT_ONLY
//#define ONLY_RENDER_TO_SCREEN

namespace {
mpv_handle *mpvHandle;
mpv_render_context *mpvRenderContext;
std::unique_ptr<sgct::utils::Dome> dome;
std::unique_ptr<sgct::utils::Sphere> sphere;
std::unique_ptr<sgct::utils::Plane> plane;

int domeRadius = 740;
int domeFov = 165;
float planeWidth = 0.f;
float planeHeight = 0.f;
std::string loadedFile = "";
std::string videoFilters = "";
glm::vec3 bgRotate(0.f);
glm::vec3 bgTranslate(0.f);

int videoWidth = 0;
int videoHeight = 0;
unsigned int mpvFBO = 0;
unsigned int mpvTex = 0;

struct RenderParams {
    unsigned int tex;
    float alpha;
    int gridMode;
    int stereoMode;
    glm::vec3 rotate;
    glm::vec3 translate;
};
std::vector<RenderParams> renderParams;

struct ImageData {
    std::string filename;
    sgct::Image img;
    std::thread trd;
    unsigned int texId = 0;
    std::atomic_bool threadRunning = false;
    std::atomic_bool imageDone = false;
    std::atomic_bool uploadDone = false;
    std::atomic_bool threadDone = false;
};

ImageData backgroundImageData;
ImageData overlayImageData;

auto loadImageAsync = [](ImageData& data) {
    data.threadRunning = true;
    data.img.load(data.filename);
    data.imageDone = true;
    while (!data.uploadDone) {}
    data.img = sgct::Image();
    data.threadDone = true;
};

int mpvVideoReconfigs = 0;
bool updateRendering = true;

bool paused = true;
int loopMode = -1;
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

void handleAsyncImageUpload(ImageData& imgData) {
    if (imgData.threadRunning) {
        if (imgData.imageDone && !imgData.uploadDone) {
            imgData.texId = TextureManager::instance().loadTexture(std::move(imgData.img));
            imgData.uploadDone = true;
        }
        else if (imgData.threadDone) {
            imgData.threadRunning = false;
            imgData.imageDone = false;
            imgData.uploadDone = false;
            imgData.threadDone = false;
            imgData.trd.join();
        }
    }
}

bool fileIsImage(std::string& filePath) {
    if (!filePath.empty()) {
        //Load background file
        if (std::filesystem::exists(filePath)) {
            std::filesystem::path bgPath = std::filesystem::path(filePath);
            if (bgPath.has_extension()) {
                std::filesystem::path bgPathExt = bgPath.extension();
                if (bgPathExt == ".png" ||
                    bgPathExt == ".jpg" ||
                    bgPathExt == ".jpeg" ||
                    bgPathExt == ".tga") {
                    return true;
                }
                else {
                    Log::Warning(fmt::format("Image file extension is not supported: {}", filePath));
                }
            }
            else {
                Log::Warning(fmt::format("Image file has no extension: {}", filePath));
            }
        }
        else {
            Log::Warning(fmt::format("Could not find image file: {}", filePath));
        }
    }
    else {
        Log::Warning(fmt::format("Image file is empty: {}", filePath));
    }

    return false;
}

void generateTexture(unsigned int& id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void createMpvFBO(int width, int height){
    videoWidth = width;
    videoHeight = height;

    sgct::Window::makeSharedContextCurrent();

    glGenFramebuffers(1, &mpvFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mpvFBO);

    generateTexture(mpvTex, width, height);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        mpvTex,
        0
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void resizeMpvFBO(int width, int height){
    if(width == videoWidth && height == videoHeight)
        return;

    Log::Info(fmt::format("New MPV FBO width:{} and height:{}", width, height));

    glDeleteFramebuffers(1, &mpvFBO);
    glDeleteTextures(1, &mpvTex);

    createMpvFBO(width, height);
}

static void *get_proc_address_mpv(void*, const char *name)
{
    return reinterpret_cast<void *>(glfwGetProcAddress(name));
}

void on_mpv_render_update(void*)
{
}

void on_mpv_events(void*)
{
    while (mpvHandle) {
        mpv_event *event = mpv_wait_event(mpvHandle, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
#ifndef ONLY_RENDER_TO_SCREEN
            case MPV_EVENT_VIDEO_RECONFIG: {
                // Retrieve the new video size.
                int64_t w, h;
                if (mpv_get_property(mpvHandle, "dwidth", MPV_FORMAT_INT64, &w) >= 0 &&
                    mpv_get_property(mpvHandle, "dheight", MPV_FORMAT_INT64, &h) >= 0 &&
                    w > 0 && h > 0)
                {
                    resizeMpvFBO((int)w, (int)h);
                    mpvVideoReconfigs++;
                    updateRendering = (mpvVideoReconfigs > 0);
                    mpv::qt::set_property(mpvHandle, "time-pos", SyncHelper::instance().variables.timePosition);
                }
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE: {
                    mpv_event_property *prop = (mpv_event_property *)event->data;
                    if (strcmp(prop->name, "video-params") == 0) {
                        if (prop->format == MPV_FORMAT_NODE) {
                            const QVariant videoParams = mpv::qt::get_property(mpvHandle, "video-params");
                            auto vm = videoParams.toMap();
                            int w = vm["w"].toInt();
                            int h = vm["h"].toInt();
                            resizeMpvFBO((int)w, (int)h);
                        }
                    }
                    else if (strcmp(prop->name, "pause") == 0) {
                        if (prop->format == MPV_FORMAT_FLAG) {
                            paused = mpv::qt::get_property(mpvHandle, "pause").toBool();
                            if (SyncHelper::instance().variables.paused != paused)
                                mpv::qt::set_property(mpvHandle, "pause", SyncHelper::instance().variables.paused);
                        }
                    }
                    else if (strcmp(prop->name, "time-pos") == 0) {
                        if (prop->format == MPV_FORMAT_DOUBLE) {
                            //mpv::qt::set_property(mpvHandle, "pause", SyncHelper::instance().variables.paused);
                        }
                    }
                    break;
            }
#endif
            default: {
                // Ignore uninteresting or unknown events.
                break;
            }
        }
    }
}

void initOGL(GLFWwindow*) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif
    mpvHandle = mpv_create();
    if (!mpvHandle)
        Log::Error("mpv context init failed");

    // Some minor options can only be set before mpv_initialize().
    if (mpv_initialize(mpvHandle) < 0)
        Log::Error("mpv init failed");

    /*mpv_set_option_string(mpvHandle, "terminal", "yes");
    mpv_set_option_string(mpvHandle, "msg-level", "all=v");
    mpv_request_log_messages(mpvHandle, "debug");*/

    mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr};
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    // This makes mpv use the currently set GL context. It will use the callback
    // (passed via params) to resolve GL builtin functions, as well as extensions.
    if (mpv_render_context_create(&mpvRenderContext, mpvHandle, params) < 0)
        Log::Error("failed to initialize mpv GL context");

    // When there is a need to call mpv_render_context_update(), which can
    // request a new frame to be rendered.
    // (Separate from the normal event handling mechanism for the sake of
    //  users which run OpenGL on a different thread.)
    mpv_render_context_set_update_callback(mpvRenderContext, on_mpv_render_update, NULL);

    // Load mpv configurations for nodes
    mpv::qt::load_configurations(mpvHandle, QString::fromStdString(SyncHelper::instance().configuration.confAll));
    mpv::qt::load_configurations(mpvHandle, QString::fromStdString(SyncHelper::instance().configuration.confNodesOnly));

    // Set default settings
    mpv::qt::set_property(mpvHandle, "keep-open", "yes");
    mpv::qt::set_property(mpvHandle, "loop-file", "inf");
    mpv::qt::set_property(mpvHandle, "aid", "no"); //No audio on nodes.

    Log::Info(fmt::format("Loading new file: {}", SyncHelper::instance().variables.loadedFile));
    if(!SyncHelper::instance().variables.loadedFile.empty())
        mpv::qt::command_async(mpvHandle, QStringList() << "loadfile" << SyncHelper::instance().variables.loadedFile.c_str());

#ifndef ONLY_RENDER_TO_SCREEN
    //Observe video parameters
    mpv_observe_property(mpvHandle, 0, "video-params", MPV_FORMAT_NODE);
    mpv_observe_property(mpvHandle, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpvHandle, 0, "time-pos", MPV_FORMAT_DOUBLE);

    //Creating new FBO to render mpv into
    createMpvFBO(512, 512);

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
    dome = std::make_unique<utils::Dome>(float(domeRadius)/100.f, float(domeFov), 256, 128);
    sphere = std::make_unique<utils::Sphere>(float(domeRadius) / 100.f, 256);

    planeWidth = float(SyncHelper::instance().variables.planeWidth);
    planeHeight = float(SyncHelper::instance().variables.planeHeight);
    plane = std::make_unique<utils::Plane>(planeWidth / 100.f, planeHeight / 100.f);

    // Set up backface culling
    glCullFace(GL_BACK);
    // our polygon winding is clockwise since we are inside of the dome
    glFrontFace(GL_CW);
#endif
}

void preSync() {

}

std::vector<std::byte> encode() {
    std::vector<std::byte> data;
    serializeObject(data, SyncHelper::instance().variables.syncOn);
    serializeObject(data, SyncHelper::instance().variables.alpha);
    serializeObject(data, SyncHelper::instance().variables.alphaBg);
    serializeObject(data, SyncHelper::instance().variables.loadedFile);
    serializeObject(data, SyncHelper::instance().variables.bgImageFile);
    serializeObject(data, SyncHelper::instance().variables.overlayFile);
    serializeObject(data, SyncHelper::instance().variables.loadFile);
    serializeObject(data, SyncHelper::instance().variables.paused);
    serializeObject(data, SyncHelper::instance().variables.timePosition);
    serializeObject(data, SyncHelper::instance().variables.timeThreshold);
    serializeObject(data, SyncHelper::instance().variables.timeDirty);
    serializeObject(data, SyncHelper::instance().variables.gridToMapOn);
    serializeObject(data, SyncHelper::instance().variables.gridToMapOnBg);
    serializeObject(data, SyncHelper::instance().variables.stereoscopicMode);
    serializeObject(data, SyncHelper::instance().variables.stereoscopicModeBg);
    serializeObject(data, SyncHelper::instance().variables.loopMode);
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
    serializeObject(data, SyncHelper::instance().variables.eqDirty);
    serializeObject(data, SyncHelper::instance().variables.eqContrast);
    serializeObject(data, SyncHelper::instance().variables.eqBrightness);
    serializeObject(data, SyncHelper::instance().variables.eqGamma);
    serializeObject(data, SyncHelper::instance().variables.eqSaturation);
    serializeObject(data, SyncHelper::instance().variables.loopTimeDirty);
    serializeObject(data, SyncHelper::instance().variables.loopTimeEnabled);
    serializeObject(data, SyncHelper::instance().variables.loopTimeA);
    serializeObject(data, SyncHelper::instance().variables.loopTimeB);

    //Reset flags every frame cycle
    SyncHelper::instance().variables.loadFile = false;
    SyncHelper::instance().variables.timeDirty = false;
    SyncHelper::instance().variables.eqDirty = false;
    SyncHelper::instance().variables.loopTimeDirty = false;

    return data;
}

void decode(const std::vector<std::byte>& data) {
    unsigned pos = 0;
    deserializeObject(data, pos, SyncHelper::instance().variables.syncOn);
    deserializeObject(data, pos, SyncHelper::instance().variables.alpha);
    deserializeObject(data, pos, SyncHelper::instance().variables.alphaBg);
    if (SyncHelper::instance().variables.syncOn) {
        deserializeObject(data, pos, SyncHelper::instance().variables.loadedFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.overlayFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.loadFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.paused);
        deserializeObject(data, pos, SyncHelper::instance().variables.timePosition);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThreshold);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeDirty);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOn);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOnBg);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicModeBg);
        deserializeObject(data, pos, SyncHelper::instance().variables.loopMode);
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
        deserializeObject(data, pos, SyncHelper::instance().variables.eqDirty);
        deserializeObject(data, pos, SyncHelper::instance().variables.eqContrast);
        deserializeObject(data, pos, SyncHelper::instance().variables.eqBrightness);
        deserializeObject(data, pos, SyncHelper::instance().variables.eqGamma);
        deserializeObject(data, pos, SyncHelper::instance().variables.eqSaturation);
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeDirty);
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeEnabled);
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeA);
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeB);
    }
}

void postSyncPreDraw() {
#ifndef SGCT_ONLY
    //Apply synced commands
    if (!Engine::instance().isMaster()) {
        handleAsyncImageUpload(backgroundImageData);
        handleAsyncImageUpload(overlayImageData);

        if (backgroundImageData.filename != SyncHelper::instance().variables.bgImageFile) {
            if (fileIsImage(SyncHelper::instance().variables.bgImageFile)) {
                //Load background file
                backgroundImageData.filename = SyncHelper::instance().variables.bgImageFile;
                Log::Info(fmt::format("Loading new background image asynchronously: {}", backgroundImageData.filename));
                backgroundImageData.trd = std::thread(loadImageAsync, std::ref(backgroundImageData));

                bgRotate = glm::vec3(float(SyncHelper::instance().variables.rotateX),
                    float(SyncHelper::instance().variables.rotateY),
                    float(SyncHelper::instance().variables.rotateZ));
                bgTranslate = glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f,
                    float(SyncHelper::instance().variables.translateY) / 100.f,
                    float(SyncHelper::instance().variables.translateZ) / 100.f);
            }
            else {
                backgroundImageData.filename = "";
            }
        }

        if (overlayImageData.filename != SyncHelper::instance().variables.overlayFile) {
            if (fileIsImage(SyncHelper::instance().variables.overlayFile)) {
                //Load overlay file
                overlayImageData.filename = SyncHelper::instance().variables.overlayFile;
                Log::Info(fmt::format("Loading new overlay image asynchronously: {}", overlayImageData.filename));
                overlayImageData.trd = std::thread(loadImageAsync, std::ref(overlayImageData));
            }
            else {
                overlayImageData.filename = "";
            }
        }

        if (!SyncHelper::instance().variables.loadedFile.empty() && (SyncHelper::instance().variables.loadFile || loadedFile != SyncHelper::instance().variables.loadedFile)) {            
            //Load new file
            loadedFile = SyncHelper::instance().variables.loadedFile;
            SyncHelper::instance().variables.loadFile = false;
            Log::Info(fmt::format("Loading new file: {}", loadedFile));
            mpvVideoReconfigs = 0;
            updateRendering = false;
            mpv::qt::command_async(mpvHandle, QStringList() << "loadfile" << QString::fromStdString(loadedFile));
        }

        renderParams.clear();

        if (!backgroundImageData.filename.empty() && SyncHelper::instance().variables.alphaBg > 0.f) {
            RenderParams rpBg;
            rpBg.tex = backgroundImageData.texId;
            rpBg.alpha = SyncHelper::instance().variables.alphaBg;
            rpBg.gridMode = SyncHelper::instance().variables.gridToMapOnBg;
            rpBg.stereoMode = SyncHelper::instance().variables.stereoscopicModeBg;
            rpBg.rotate = bgRotate;
            rpBg.translate = bgTranslate;
            renderParams.push_back(rpBg);
        }

        if (updateRendering) {
            if (!loadedFile.empty() && SyncHelper::instance().variables.alpha > 0.f) {
                RenderParams rpMpv;
                rpMpv.tex = mpvTex;
                rpMpv.alpha = SyncHelper::instance().variables.alpha;
                rpMpv.gridMode = SyncHelper::instance().variables.gridToMapOn;
                rpMpv.stereoMode = SyncHelper::instance().variables.stereoscopicMode;
                rpMpv.rotate = glm::vec3(float(SyncHelper::instance().variables.rotateX),
                    float(SyncHelper::instance().variables.rotateY),
                    float(SyncHelper::instance().variables.rotateZ));
                rpMpv.translate = glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f,
                    float(SyncHelper::instance().variables.translateY) / 100.f,
                    float(SyncHelper::instance().variables.translateZ) / 100.f);
                renderParams.push_back(rpMpv);
            }

            if (!overlayImageData.filename.empty() && SyncHelper::instance().variables.alpha > 0.f) {
                RenderParams rpOverlay;
                rpOverlay.tex = overlayImageData.texId;
                rpOverlay.alpha = SyncHelper::instance().variables.alpha;
                rpOverlay.gridMode = SyncHelper::instance().variables.gridToMapOn;
                rpOverlay.stereoMode = SyncHelper::instance().variables.stereoscopicMode;
                rpOverlay.rotate = glm::vec3(float(SyncHelper::instance().variables.rotateX),
                    float(SyncHelper::instance().variables.rotateY),
                    float(SyncHelper::instance().variables.rotateZ));
                rpOverlay.translate = glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f,
                    float(SyncHelper::instance().variables.translateY) / 100.f,
                    float(SyncHelper::instance().variables.translateZ) / 100.f);
                renderParams.push_back(rpOverlay);
            }
        }
        
        paused = mpv::qt::get_property(mpvHandle, "pause").toBool();
        if (SyncHelper::instance().variables.paused != paused) {
            paused = SyncHelper::instance().variables.paused;
            if (paused) {
                Log::Info("Video paused.");
            }
            else {
                Log::Info("Video playing...");
            }
            mpv::qt::set_property(mpvHandle, "pause", paused);
        }

        if (SyncHelper::instance().variables.loopMode != loopMode) {
            loopMode = SyncHelper::instance().variables.loopMode;

            if (loopMode == 0) { //Pause
                mpv::qt::set_property(mpvHandle, "keep-open", "yes");
                mpv::qt::set_property(mpvHandle, "loop-file", "no");
            }
            else if (loopMode == 1) { //Continue
                mpv::qt::set_property(mpvHandle, "keep-open", "no");
                mpv::qt::set_property(mpvHandle, "loop-file", "no");
            }
            else { //Loop
                mpv::qt::set_property(mpvHandle, "keep-open", "yes");
                mpv::qt::set_property(mpvHandle, "loop-file", "inf");
            }
        }

        double currentTimePos = mpv::qt::get_property(mpvHandle, "time-pos").toDouble();
        if (SyncHelper::instance().variables.timePosition != currentTimePos && SyncHelper::instance().variables.syncOn) {
            double timeToSet = SyncHelper::instance().variables.timePosition;
            if (SyncHelper::instance().variables.timeDirty) {
                mpv::qt::set_property(mpvHandle, "time-pos", timeToSet);
                //Log::Info(fmt::format("New video position: {}", timeToSet));     
            }
        }

        if (SyncHelper::instance().variables.loopTimeDirty) {
            if (SyncHelper::instance().variables.loopTimeEnabled) {
                mpv::qt::set_property(mpvHandle, "ab-loop-a", SyncHelper::instance().variables.loopTimeA);
                mpv::qt::set_property(mpvHandle, "ab-loop-b", SyncHelper::instance().variables.loopTimeB);
            }
            else {
                mpv::qt::set_property(mpvHandle, "ab-loop-a", "no");
                mpv::qt::set_property(mpvHandle, "ab-loop-b", "no");
            }
        }

        if (SyncHelper::instance().variables.eqDirty) {
            if (SyncHelper::instance().variables.eqContrast != mpv::qt::get_property(mpvHandle, "contrast").toInt()) {
                mpv::qt::set_property(mpvHandle, "contrast", SyncHelper::instance().variables.eqContrast);
            }

            if (SyncHelper::instance().variables.eqBrightness != mpv::qt::get_property(mpvHandle, "brightness").toInt()) {
                mpv::qt::set_property(mpvHandle, "brightness", SyncHelper::instance().variables.eqBrightness);
            }

            if (SyncHelper::instance().variables.eqGamma != mpv::qt::get_property(mpvHandle, "gamma").toInt()) {
                mpv::qt::set_property(mpvHandle, "gamma", SyncHelper::instance().variables.eqGamma);
            }

            if (SyncHelper::instance().variables.eqSaturation != mpv::qt::get_property(mpvHandle, "saturation").toInt()) {
                mpv::qt::set_property(mpvHandle, "saturation", SyncHelper::instance().variables.eqSaturation);
            }
        }

        if (domeRadius != SyncHelper::instance().variables.radius || domeFov != SyncHelper::instance().variables.fov) {
            dome = nullptr;
            sphere = nullptr;
            domeRadius = SyncHelper::instance().variables.radius;
            domeFov = SyncHelper::instance().variables.fov;
            dome = std::make_unique<utils::Dome>(float(domeRadius) / 100.f, float(domeFov), 256, 128);
        }

        if (planeWidth != SyncHelper::instance().variables.planeWidth || planeHeight != SyncHelper::instance().variables.planeHeight) {
            plane = nullptr;
            planeWidth = float(SyncHelper::instance().variables.planeWidth);
            planeHeight = float(SyncHelper::instance().variables.planeHeight);
            plane = std::make_unique<utils::Plane>(planeWidth / 100.f, planeHeight / 100.f);
        }
    }

    if (Engine::instance().isMaster()) {
        return;
    }
#endif

    //Check mpv events
    on_mpv_events(mpvHandle);

#ifndef ONLY_RENDER_TO_SCREEN
    if (updateRendering) {
        mpv_opengl_fbo mpfbo{ static_cast<int>(mpvFBO), videoWidth, videoHeight, GL_RGBA16F };
        int flip_y{ 1 };

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        // See render_gl.h on what OpenGL environment mpv expects, and
        // other API details.
        mpv_render_context_render(mpvRenderContext, params);
    }
#endif
}

void draw(const RenderData& data) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif
    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);

    glDisable(GL_BLEND);

#ifdef ONLY_RENDER_TO_SCREEN
    OffScreenBuffer *osb = data.window.fbo();
    ivec2 size = osb->size();

    mpv_opengl_fbo mpfbo{static_cast<int>(osb->fboHandle()), size.x, size.y, 0};
    int flip_y{1};

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    // See render_gl.h on what OpenGL environment mpv expects, and
    // other API details.
    mpv_render_context_render(mpvRenderContext, params);
#else

    for (const auto& renderParam : renderParams) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderParam.tex);
        glEnable(GL_BLEND);

        if (renderParam.gridMode == 4) {
            glEnable(GL_CULL_FACE);

            EACPrg->bind();

            glUniform1f(EACAlphaLoc, renderParam.alpha);
            glUniform1i(EACOutsideLoc, 0);
            glUniform1i(EACVideoWidthLoc, videoWidth);
            glUniform1i(EACVideoHeightLoc, videoHeight);

            if (renderParam.stereoMode > 0) {
                glUniform1i(EACEyeModeLoc, (GLint)data.frustumMode);
                glUniform1i(EACStereoscopicModeLoc, (GLint)renderParam.stereoMode);
            }
            else {
                glUniform1i(EACEyeModeLoc, 0);
                glUniform1i(EACStereoscopicModeLoc, 0);
            }

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), renderParam.translate);

            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.x), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.y + 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(90.f), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            glUniformMatrix4fv(EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            sphere->draw();

            // Set up frontface culling
            glCullFace(GL_FRONT);

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - renderParam.rotate.z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - renderParam.rotate.x + float(SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - renderParam.rotate.y - 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - 90.f), glm::vec3(0.0f, 0.0f, 1.0f)); //roll

            glUniformMatrix4fv(EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
            //render outside sphere
            glUniform1i(EACOutsideLoc, 1);
            sphere->draw();

            // Set up backface culling again
            glCullFace(GL_BACK);
            glUniform1i(EACOutsideLoc, 0);

            EACPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (renderParam.gridMode == 3) {
            glEnable(GL_CULL_FACE);

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), renderParam.translate);

            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.x), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            meshPrg->bind();

            if (renderParam.stereoMode > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)data.frustumMode);
                glUniform1i(meshStereoscopicModeLoc, (GLint)renderParam.stereoMode);
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, renderParam.alpha);

            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            //render inside sphere
            glUniform1i(meshOutsideLoc, 0);
            sphere->draw();

            // Set up frontface culling
            glCullFace(GL_FRONT);

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - renderParam.rotate.z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - renderParam.rotate.x + float(SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - renderParam.rotate.y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
            //render outside sphere
            glUniform1i(meshOutsideLoc, 1);
            sphere->draw();

            // Set up backface culling again
            glCullFace(GL_BACK);
            glUniform1i(meshOutsideLoc, 0);

            meshPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (renderParam.gridMode == 2) {
            glEnable(GL_CULL_FACE);

            meshPrg->bind();

            if (renderParam.stereoMode > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)data.frustumMode);
                glUniform1i(meshStereoscopicModeLoc, (GLint)renderParam.stereoMode);
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, renderParam.alpha);

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed_rot = glm::translate(glm::make_mat4(mvp.values), renderParam.translate);
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.x - float(SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(renderParam.rotate.y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            dome->draw();

            meshPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (renderParam.gridMode == 1) {
            glEnable(GL_CULL_FACE);
            // Set up frontface culling
            glCullFace(GL_FRONT);

            meshPrg->bind();

            if (renderParam.stereoMode > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)data.frustumMode);
                glUniform1i(meshStereoscopicModeLoc, (GLint)renderParam.stereoMode);
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, renderParam.alpha);

            const sgct::mat4 mvp = data.projectionMatrix * data.viewMatrix;

            glm::mat4 planeTransform = glm::mat4(1.0f);
            //planeTransform = glm::rotate(planeTransform, glm::radians(float(SyncHelper::instance().variables.planeAzimuth)), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
            planeTransform = glm::rotate(planeTransform, glm::radians(float(SyncHelper::instance().variables.planeElevation)), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
            //planeTransform = glm::rotate(planeTransform, glm::radians(float(SyncHelper::instance().variables.planeRoll)), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            planeTransform = glm::translate(planeTransform, glm::vec3(0.0f, 0.0f, float(-SyncHelper::instance().variables.planeDistance) / 100.f)); //distance
            planeTransform = glm::make_mat4(mvp.values) * planeTransform;
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &planeTransform[0][0]);

            plane->draw();

            meshPrg->unbind();

            // Set up backface culling again
            glCullFace(GL_BACK);

            glDisable(GL_CULL_FACE);
        }
        else {
            videoPrg->bind();

            if (renderParam.stereoMode > 0) {
                glUniform1i(videoEyeModeLoc, (GLint)data.frustumMode);
                glUniform1i(videoStereoscopicModeLoc, (GLint)renderParam.stereoMode);
            }
            else {
                glUniform1i(videoEyeModeLoc, 0);
                glUniform1i(videoStereoscopicModeLoc, 0);
            }

            glUniform1f(videoAlphaLoc, renderParam.alpha);

            data.window.renderScreenQuad();

            videoPrg->unbind();
        }
    }
    glDisable(GL_BLEND);
#endif
}

void cleanup() {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    // Destroy the GL renderer and all of the GL objects it allocated. If video
    // is still running, the video track will be deselected.
    mpv_render_context_free(mpvRenderContext);

    mpv_destroy(mpvHandle);

    glDeleteFramebuffers(1, &mpvFBO);
    glDeleteTextures(1, &mpvTex);
    glDeleteTextures(1, &backgroundImageData.texId);

    dome = nullptr;
    sphere = nullptr;
    plane = nullptr;
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
        Application::create(cargv_size, &cargv[0], "C-Play");
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

