/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sgct/sgct.h>
#include <sgct/opengl.h>
#include <sgct/utils/dome.h>
#include <sgct/utils/sphere.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include "application.h"
#include <client.h>
#include <render_gl.h>
#include "qthelper.h"

//#define SGCT_ONLY
//#define ONLY_RENDER_TO_SCREEN

#ifdef ONLY_RENDER_TO_SCREEN
#include <sgct/offscreenbuffer.h>
#endif

namespace {
mpv_handle *mpvHandle;
mpv_render_context *mpvRenderContext;
std::unique_ptr<sgct::utils::Dome> dome;
std::unique_ptr<sgct::utils::Sphere> sphere;

int domeRadius = 740;
int domeFov = 165;
std::string loadedFile = "";

int videoWidth = 0;
int videoHeight = 0;
unsigned int mpvFBO = 0;
unsigned int mpvTex = 0;
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
// meshEAC
int meshEACAlphaLoc = -1;
int meshEACEyeModeLoc = -1;
int meshEACMatrixLoc = -1;
int meshEACOutsideLoc = -1;
int meshEACStereoscopicModeLoc = -1;

constexpr const char* VideoVert = R"(
  #version 330 core

  layout (location = 0) in vec2 in_position;
  layout (location = 1) in vec2 in_texCoords;

  uniform int eye;
  uniform int stereoscopicMode;

  out vec2 tr_uv;

  void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    tr_uv = in_texCoords;

    if(eye==1) { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = in_texCoords * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (in_texCoords * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (in_texCoords * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (in_texCoords * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = in_texCoords * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = in_texCoords * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

constexpr const char* MeshVert = R"(
  #version 330 core

  layout (location = 0) in vec2 in_texCoords;
  layout (location = 1) in vec3 in_normals;
  layout (location = 2) in vec3 in_position;

  uniform mat4 mvp;
  uniform int eye;
  uniform int stereoscopicMode;

  out vec2 tr_uv;
  out vec3 tr_normals;

  void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_uv = in_texCoords;
    tr_normals = in_normals;

    if(eye==1) { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = in_texCoords * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (in_texCoords * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (in_texCoords * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (in_texCoords * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = in_texCoords * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = in_texCoords * vec2(1.0, 0.5);
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

  layout (location = 0) in vec2 in_texCoords;
  layout (location = 1) in vec3 in_normals;
  layout (location = 2) in vec3 in_position;

  uniform mat4 mvp;

  out vec3 tr_normals;

  void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_normals = in_normals;
  }
)";

constexpr const char* EACSphereVideoFrag = R"(
  #version 330 core

  // Pi, the ratio of a circle's circumference to its diameter.
  const float M_PI = 3.14159265358979323846264338327950288;

  uniform sampler2D tex;
  uniform float alpha;
  uniform bool outside;
  uniform int eye;
  uniform int stereoscopicMode;

  in vec3 tr_normals;
  out vec4 out_color;

  void main() {
    float pi = M_PI;
	vec3 xyz = normalize(-tr_normals);
    if(outside){
        xyz = normalize(tr_normals);
    }
	float x = xyz.x;
    float y = xyz.y;
    float z = xyz.z;

	float u = 0.0;
    float v = 0.0;
	float scale; // sphere coordinates to cube coordinates according to similar-triangle
	if (abs(x) >= abs(y) && abs(x) >= abs(z)) {
		scale = 1.0 / abs(x); // let's assume that radius of sphere is 1, which means u is 6.0 and v is 4.0
		if (x >= 0.0) { // right
			u = 5.0 - 4.0 * atan(z * scale) / pi;
			v = 3.0 + 4.0 * atan(y * scale) / pi;
		} else { // left
			u = 1.0 + 4.0 * atan(z * scale) / pi;
			v = 3.0 + 4.0 * atan(y * scale) / pi;
		}
	} else if (abs(y) >= abs(x) && abs(y) >= abs(z)) {
		scale = 1.0 / abs(y);
		if (y >= 0.0) { // top
			u = 5.0 + 4.0 * atan(z * scale) / pi;
			v = 1.0 + 4.0 * atan(x * scale) / pi;
		} else { // down
			u = 1.0 - 4.0 * atan(z * scale) / pi;
			v = 1.0 + 4.0 * atan(x * scale) / pi;
		}
	} else if (abs(z) >= abs(x) && abs(z) >= abs(y)) {
		scale = 1.0 / abs(z);
		if (z >= 0.0) { // front
			u = 3.0 + 4.0 * atan(x * scale) / pi;
			v = 3.0 + 4.0 * atan(y * scale) / pi;
		} else { // end
			u = 3.0 + 4.0 * atan(y * scale) / pi;
			v = 1.0 + 4.0 * atan(x * scale) / pi;
		}
	}

    vec2 texCoods = vec2(u / 6.0, v / 4.0);

    if(eye==1) { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            texCoods = texCoods * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            texCoods = (texCoods * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            texCoods = (texCoods * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            texCoods = vec2(1.0 - texCoods.y, texCoods.x);
        }
    }
    else if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            texCoods = (texCoods * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            texCoods = texCoods * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            texCoods = texCoods * vec2(1.0, 0.5);
            texCoods = vec2(1.0 - texCoods.y, texCoods.x);
        }
    }
   
    out_color = texture(tex, texCoods) * vec4(1.0, 1.0, 1.0, alpha);
  }
)";

} // namespace

using namespace sgct;

const ShaderProgram* videoPrg;
const ShaderProgram* meshPrg;
const ShaderProgram* meshEACPrg;

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

    Engine::instance().thisNode().windows().front()->makeSharedContextCurrent();

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

    mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr, nullptr};
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
    mpv::qt::load_configurations(mpvHandle, QStringLiteral("./data/mpv-conf.json"));
    mpv::qt::load_configurations(mpvHandle, QStringLiteral("./data/mpv-conf-nodes.json"));

    // Set default settings
    mpv::qt::set_property(mpvHandle, "keep-open", "yes");
    mpv::qt::set_property(mpvHandle, "loop-file", "inf");

#ifdef SGCT_ONLY
    SyncHelper::instance().variables.loadedFile = "G:/Splits/Life_of_trees_3D_bravo/Life_of_trees_3D.mp4";
#endif
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
    ShaderManager::instance().addShaderProgram("meshEAC", EACMeshVert, EACSphereVideoFrag);
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

    meshEACPrg = &ShaderManager::instance().shaderProgram("meshEAC");
    meshEACPrg->bind();
    glUniform1i(glGetUniformLocation(meshEACPrg->id(), "tex"), 0);
    meshEACMatrixLoc = glGetUniformLocation(meshEACPrg->id(), "mvp");
    meshEACEyeModeLoc = glGetUniformLocation(meshEACPrg->id(), "eye");
    meshEACStereoscopicModeLoc = glGetUniformLocation(meshEACPrg->id(), "stereoscopicMode");
    meshEACAlphaLoc = glGetUniformLocation(meshEACPrg->id(), "alpha");
    meshEACOutsideLoc = glGetUniformLocation(meshEACPrg->id(), "outside");
    meshEACPrg->unbind();

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

    // Set up backface culling
    glCullFace(GL_BACK);
    // our polygon winding is clockwise since we are inside of the dome
    glFrontFace(GL_CW);
#endif
}

void preSync() {
    // Running fade in/out with buttons through QML instead
    /*if (Engine::instance().isMaster()) {
        int fds = Application::instance().getFadeDurationSetting();
        if (fds > 0) {
            // Start a fade duration if new file is loaded.
            bool restartTimer = false;
            if (!SyncHelper::instance().variables.loadedFile.empty() 
                && loadedVideoFile != SyncHelper::instance().variables.loadedFile 
                && SyncHelper::instance().variables.syncOn) {
                fadeDurationOngoing = true;
                restartTimer = true;
                SyncHelper::instance().variables.syncOn = false;
                loadedVideoFile = SyncHelper::instance().variables.loadedFile;
            }
            if (fadeDurationOngoing) {
                int fdct = Application::instance().getFadeDurationCurrentTime(restartTimer);
                int half_fds = fds / 2;
                if (fdct < half_fds - 500) { // Fade out
                    SyncHelper::instance().variables.alpha = 1.f - (float(fdct) / float(half_fds - 500));
                }
                else if (fdct < half_fds + 500) { // Sync On again, Keep black for 1 second
                    SyncHelper::instance().variables.syncOn = true;
                    SyncHelper::instance().variables.alpha = 0.f;

                }
                else if (fdct >= fds) { // Fade complete
                    fadeDurationOngoing = false;
                    SyncHelper::instance().variables.syncOn = true;
                    SyncHelper::instance().variables.alpha = 1.f;
                }
                else { // Fade in
                    SyncHelper::instance().variables.syncOn = true;
                    SyncHelper::instance().variables.alpha = float(fdct - half_fds - 500) / float(half_fds - 500);
                }
            }
        }
    }*/
}

std::vector<std::byte> encode() {
    std::vector<std::byte> data;
    serializeObject(data, SyncHelper::instance().variables.syncOn);
    serializeObject(data, SyncHelper::instance().variables.alpha);
    serializeObject(data, SyncHelper::instance().variables.loadedFile);
    serializeObject(data, SyncHelper::instance().variables.overlayFile);
    serializeObject(data, SyncHelper::instance().variables.loadFile);
    serializeObject(data, SyncHelper::instance().variables.paused);
    serializeObject(data, SyncHelper::instance().variables.timePosition);
    serializeObject(data, SyncHelper::instance().variables.timeThreshold);
    serializeObject(data, SyncHelper::instance().variables.timeDirty);
    serializeObject(data, SyncHelper::instance().variables.stereoscopicMode);
    serializeObject(data, SyncHelper::instance().variables.loopMode);
    serializeObject(data, SyncHelper::instance().variables.gridToMapOn);
    serializeObject(data, SyncHelper::instance().variables.contrast);
    serializeObject(data, SyncHelper::instance().variables.brightness);
    serializeObject(data, SyncHelper::instance().variables.gamma);
    serializeObject(data, SyncHelper::instance().variables.saturation);
    serializeObject(data, SyncHelper::instance().variables.radius);
    serializeObject(data, SyncHelper::instance().variables.fov);
    serializeObject(data, SyncHelper::instance().variables.angle);
    serializeObject(data, SyncHelper::instance().variables.rotateX);
    serializeObject(data, SyncHelper::instance().variables.rotateY);
    serializeObject(data, SyncHelper::instance().variables.rotateZ);
    serializeObject(data, SyncHelper::instance().variables.translateX);
    serializeObject(data, SyncHelper::instance().variables.translateY);
    serializeObject(data, SyncHelper::instance().variables.translateZ);

    //Reset flags every frame cycle
    SyncHelper::instance().variables.loadFile = false;
    SyncHelper::instance().variables.timeDirty = false;

    return data;
}

void decode(const std::vector<std::byte>& data, unsigned int pos) {
    deserializeObject(data, pos, SyncHelper::instance().variables.syncOn);
    deserializeObject(data, pos, SyncHelper::instance().variables.alpha);
    if (SyncHelper::instance().variables.syncOn) {
        deserializeObject(data, pos, SyncHelper::instance().variables.loadedFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.overlayFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.loadFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.paused);
        deserializeObject(data, pos, SyncHelper::instance().variables.timePosition);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThreshold);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeDirty);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.loopMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOn);
        deserializeObject(data, pos, SyncHelper::instance().variables.contrast);
        deserializeObject(data, pos, SyncHelper::instance().variables.brightness);
        deserializeObject(data, pos, SyncHelper::instance().variables.gamma);
        deserializeObject(data, pos, SyncHelper::instance().variables.saturation);
        deserializeObject(data, pos, SyncHelper::instance().variables.radius);
        deserializeObject(data, pos, SyncHelper::instance().variables.fov);
        deserializeObject(data, pos, SyncHelper::instance().variables.angle);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateZ);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateZ);
    }
}

void postSyncPreDraw() {
#ifndef SGCT_ONLY
    //Apply synced commands
    if (!Engine::instance().isMaster()) {
        if (!SyncHelper::instance().variables.loadedFile.empty() && (SyncHelper::instance().variables.loadFile || loadedFile != SyncHelper::instance().variables.loadedFile)) {
            //Load new file
            loadedFile = SyncHelper::instance().variables.loadedFile;
            SyncHelper::instance().variables.loadFile = false;
            Log::Info(fmt::format("Loading new file: {}", SyncHelper::instance().variables.loadedFile));

            if (!SyncHelper::instance().variables.overlayFile.empty()) {
                Log::Info(fmt::format("Loading overlay file: {}", SyncHelper::instance().variables.overlayFile));
                mpv::qt::command_async(mpvHandle, QStringList() << "loadfile" << QString::fromStdString(SyncHelper::instance().variables.loadedFile) << "replace" << QString::fromStdString("external-file=" + SyncHelper::instance().variables.overlayFile));
                mpv::qt::set_property(mpvHandle, "lavfi-complex", "[vid1][vid2]overlay@myoverlay[vo]");
            }
            else {
                mpv::qt::command_async(mpvHandle, QStringList() << "loadfile" << QString::fromStdString(SyncHelper::instance().variables.loadedFile));
                mpv::qt::set_property(mpvHandle, "lavfi-complex", "");
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

            if (loopMode == 0) { //Continue
                mpv::qt::set_property(mpvHandle, "keep-open", "no");
                mpv::qt::set_property(mpvHandle, "loop-file", "no");
            }
            else if (loopMode == 1) { //Pause (1)
                mpv::qt::set_property(mpvHandle, "keep-open", "yes");
                mpv::qt::set_property(mpvHandle, "loop-file", "no");
            }
            else { //Loop
                mpv::qt::set_property(mpvHandle, "keep-open", "yes");
                mpv::qt::set_property(mpvHandle, "loop-file", "inf");
            }
        }

        if (SyncHelper::instance().variables.contrast != mpv::qt::get_property(mpvHandle, "contrast").toInt()) {
            mpv::qt::set_property(mpvHandle, "contrast", SyncHelper::instance().variables.contrast);
        }

        if (SyncHelper::instance().variables.brightness != mpv::qt::get_property(mpvHandle, "brightness").toInt()) {
            mpv::qt::set_property(mpvHandle, "brightness", SyncHelper::instance().variables.brightness);
        }

        if (SyncHelper::instance().variables.gamma != mpv::qt::get_property(mpvHandle, "gamma").toInt()) {
            mpv::qt::set_property(mpvHandle, "gamma", SyncHelper::instance().variables.gamma);
        }

        if (SyncHelper::instance().variables.saturation != mpv::qt::get_property(mpvHandle, "saturation").toInt()) {
            mpv::qt::set_property(mpvHandle, "saturation", SyncHelper::instance().variables.saturation);
        }

        double currentTimePos = mpv::qt::get_property(mpvHandle, "time-pos").toDouble();
        if (SyncHelper::instance().variables.timePosition != currentTimePos && SyncHelper::instance().variables.syncOn) {
            double timeToSet = SyncHelper::instance().variables.timePosition;
            double timeThreshold = SyncHelper::instance().variables.timeThreshold;
            bool timeOff = ((timeToSet - currentTimePos) > timeThreshold);
            if (SyncHelper::instance().variables.timeDirty || paused || timeOff) {
                mpv::qt::set_property(mpvHandle, "time-pos", timeToSet);
                //Always set time pos when paused and not same time
                //Or set time position as it is above sync threshold
                //mpv::qt::command_async(mpvHandle, QVariantList() << "seek" << timeToSet << "absolute");
                Log::Info(fmt::format("New video position: {}", timeToSet));     
            }
        }

        if (domeRadius != SyncHelper::instance().variables.radius || domeFov != SyncHelper::instance().variables.fov) {
            dome = nullptr;
            sphere = nullptr;
            domeRadius = SyncHelper::instance().variables.radius;
            domeFov = SyncHelper::instance().variables.fov;
            dome = std::make_unique<utils::Dome>(float(domeRadius) / 100.f, float(domeFov), 256, 128);
            sphere = std::make_unique<utils::Sphere>(float(domeRadius) / 100.f, 256);
        }
    }

    if (Engine::instance().isMaster()) {
        return;
    }
#endif

    //Check mpv events
    on_mpv_events(mpvHandle);

#ifndef ONLY_RENDER_TO_SCREEN
    mpv_opengl_fbo mpfbo{static_cast<int>(mpvFBO), videoWidth, videoHeight, GL_RGBA16F};
    int flip_y{1};

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    // See render_gl.h on what OpenGL environment mpv expects, and
    // other API details.
    mpv_render_context_render(mpvRenderContext, params);
#endif
}

void draw(const RenderData& data) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif
    glDisable(GL_DEPTH_TEST);
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
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mpvTex);

    if (SyncHelper::instance().variables.gridToMapOn > 0) {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);

        if (SyncHelper::instance().variables.gridToMapOn >= 2) {
            const mat4& mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f, float(SyncHelper::instance().variables.translateY) / 100.f, float(SyncHelper::instance().variables.translateZ) / 100.f));
            
            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(float(SyncHelper::instance().variables.rotateZ)), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(float(SyncHelper::instance().variables.rotateX)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(float(SyncHelper::instance().variables.rotateY)), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(float(360 - SyncHelper::instance().variables.rotateZ)), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(float(360 - SyncHelper::instance().variables.rotateX + SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(float(360 - SyncHelper::instance().variables.rotateY)), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            
            if (SyncHelper::instance().variables.gridToMapOn == 3) {
                meshEACPrg->bind();

                if (SyncHelper::instance().variables.stereoscopicMode > 0) {
                    glUniform1i(meshEACEyeModeLoc, (GLint)data.frustumMode);
                    glUniform1i(meshEACStereoscopicModeLoc, SyncHelper::instance().variables.stereoscopicMode);
                }
                else {
                    glUniform1i(meshEACEyeModeLoc, 0);
                    glUniform1i(meshEACStereoscopicModeLoc, 0);
                }

                glUniform1f(meshEACAlphaLoc, SyncHelper::instance().variables.alpha);

                //EAC is 90 degrees of in the caluclations
                MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(-90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

                glUniformMatrix4fv(meshEACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

                //render inside sphere
                glUniform1i(meshEACOutsideLoc, 0);
                sphere->draw();

                // Set up frontface culling
                glCullFace(GL_FRONT);

                //EAC is 90 degrees of in the caluclations
                MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(-90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

                glUniformMatrix4fv(meshEACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
                //render outside sphere
                glUniform1i(meshEACOutsideLoc, 1);
                sphere->draw();

                // Set up backface culling again
                glCullFace(GL_BACK);
                glUniform1i(meshEACOutsideLoc, 0);

                meshEACPrg->unbind();
            }
            else {
                meshPrg->bind();

                if (SyncHelper::instance().variables.stereoscopicMode > 0) {
                    glUniform1i(meshEyeModeLoc, (GLint)data.frustumMode);
                    glUniform1i(meshStereoscopicModeLoc, (GLint)SyncHelper::instance().variables.stereoscopicMode);
                }
                else {
                    glUniform1i(meshEyeModeLoc, 0);
                    glUniform1i(meshStereoscopicModeLoc, 0);
                }

                glUniform1f(meshAlphaLoc, SyncHelper::instance().variables.alpha);

                glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

                //render inside sphere
                glUniform1i(meshOutsideLoc, 0);
                sphere->draw();

                // Set up frontface culling
                glCullFace(GL_FRONT);

                glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
                //render outside sphere
                glUniform1i(meshOutsideLoc, 1);
                sphere->draw();

                // Set up backface culling again
                glCullFace(GL_BACK);
                glUniform1i(meshOutsideLoc, 0);

                meshPrg->unbind();
            }
        }
        else {
            meshPrg->bind();

            if (SyncHelper::instance().variables.stereoscopicMode > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)data.frustumMode);
                glUniform1i(meshStereoscopicModeLoc, (GLint)SyncHelper::instance().variables.stereoscopicMode);
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, SyncHelper::instance().variables.alpha);

            const mat4& mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f, float(SyncHelper::instance().variables.translateY) / 100.f, float(SyncHelper::instance().variables.translateZ) / 100.f));
            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(float(SyncHelper::instance().variables.rotateZ)), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(float(SyncHelper::instance().variables.rotateX - SyncHelper::instance().variables.angle)), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(float(SyncHelper::instance().variables.rotateY)), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            dome->draw();

            meshPrg->unbind();
        }

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    else {
        glEnable(GL_BLEND);

        videoPrg->bind();

        if (SyncHelper::instance().variables.stereoscopicMode > 0) {
            glUniform1i(videoEyeModeLoc, (GLint)data.frustumMode);
            glUniform1i(videoStereoscopicModeLoc, (GLint)SyncHelper::instance().variables.stereoscopicMode);
        }
        else {
            glUniform1i(videoEyeModeLoc, 0);
            glUniform1i(videoStereoscopicModeLoc, 0);
        }

        glUniform1f(videoAlphaLoc, SyncHelper::instance().variables.alpha);

        data.window.renderScreenQuad();

        videoPrg->unbind();

        glDisable(GL_BLEND);
    }
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

    mpv_detach_destroy(mpvHandle);

    glDeleteFramebuffers(1, &mpvFBO);
    glDeleteTextures(1, &mpvTex);

    dome = nullptr;
    sphere = nullptr;
}

int main(int argc, char *argv[])
{
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);
    if (!cluster.success) {
        return -1;
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
        Log::Info("Start Master");

        //Hide window (as we are not using it on master)
        Engine::instance().thisNode().windows().at(0)->setRenderWhileHidden(true);
        Engine::instance().thisNode().windows().at(0)->setVisible(false);

        //Do not support arguments to QApp, only SGCT
        std::vector<char*> cargv;
        cargv.push_back(argv[0]);
        int cargv_size = cargv.size();

        //Launch master application (which calls Engine::render from thread)
        Application::create(cargv_size, &cargv[0], "C-Play");
        return Application::instance().run();
    }
    else{
#endif
        Log::Info("Start Client");

        Engine::instance().render();
        Engine::destroy();
        return EXIT_SUCCESS;
#ifndef SGCT_ONLY
    }
#endif

}

