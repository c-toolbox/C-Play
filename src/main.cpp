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
std::string loadedVideoFile = "";

int videoWidth = 0;
int videoHeight = 0;
unsigned int mpvFBO = 0;
unsigned int mpvTex = 0;
bool paused = true;
int videoEyeModeLoc = -1;
int videoAlphaLoc = -1;
int meshMatrixLoc = -1;
int meshEyeModeLoc = -1;
int meshAlphaLoc = -1;
bool fadeDurationOngoing = false;

constexpr const char* VideoVert = R"(
  #version 330 core

  layout (location = 0) in vec2 in_position;
  layout (location = 1) in vec2 in_texCoords;

  uniform int eye;

  out vec2 tr_uv;

  void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    tr_uv = in_texCoords;

    if(eye==1) { //Left Eye
      tr_uv = in_texCoords * vec2(0.5, 1.0);
    }
    else if(eye==2) { //Right Eye
      tr_uv = (in_texCoords * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
    }
    else { //Mono eye=0
      tr_uv = in_texCoords;
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

  out vec2 tr_uv;

  void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_uv = in_texCoords;

    if(eye==1) { //Left Eye
      tr_uv = in_texCoords * vec2(0.5, 1.0);
    }
    else if(eye==2) { //Right Eye
      tr_uv = (in_texCoords * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
    }
    else { //Mono eye=0
      tr_uv = in_texCoords;
    }
  }
)";

constexpr const char* VideoFrag = R"(
  #version 330 core

  uniform sampler2D tex;
  uniform float alpha;

  in vec2 tr_uv;
  out vec4 out_color;

  void main() {
    out_color = texture(tex, tr_uv) * vec4(1.0, 1.0, 1.0, alpha);
  }
)";

} // namespace

using namespace sgct;

const ShaderProgram* videoPrg;
const ShaderProgram* meshPrg;

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

#ifdef SGCT_ONLY
    SyncHelper::instance().variables.loadedFile = "G:/Splits/Life_of_trees_3D_bravo/Life_of_trees_3D.mp4";
#endif
    Log::Info(fmt::format("Loading new file: {}", SyncHelper::instance().variables.loadedFile));
    mpv::qt::command_async(mpvHandle, QStringList() << "loadfile" << SyncHelper::instance().variables.loadedFile.c_str());

#ifndef ONLY_RENDER_TO_SCREEN
    //Observe video parameters
    mpv_observe_property(mpvHandle, 0, "video-params", MPV_FORMAT_NODE);
    mpv_observe_property(mpvHandle, 0, "pause", MPV_FORMAT_FLAG);

    //Creating new FBO to render mpv into
    createMpvFBO(512, 512);

    // Create shaders
    ShaderManager::instance().addShaderProgram("mesh", MeshVert, VideoFrag);
    ShaderManager::instance().addShaderProgram("video", VideoVert, VideoFrag);

    //OBS: Need to create all shaders befor using any of them. Bug?
    meshPrg = &ShaderManager::instance().shaderProgram("mesh");
    meshPrg->bind();
    glUniform1i(glGetUniformLocation(meshPrg->id(), "tex"), 0);
    meshMatrixLoc = glGetUniformLocation(meshPrg->id(), "mvp");
    meshEyeModeLoc = glGetUniformLocation(meshPrg->id(), "eye");
    meshAlphaLoc = glGetUniformLocation(meshPrg->id(), "alpha");
    meshPrg->unbind();

    videoPrg = &ShaderManager::instance().shaderProgram("video");
    videoPrg->bind();
    glUniform1i(glGetUniformLocation(videoPrg->id(), "tex"), 0);
    videoEyeModeLoc = glGetUniformLocation(videoPrg->id(), "eye");
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
    serializeObject(data, SyncHelper::instance().variables.paused);
    serializeObject(data, SyncHelper::instance().variables.timePosition);
    serializeObject(data, SyncHelper::instance().variables.timeThreshold);
    serializeObject(data, SyncHelper::instance().variables.sbs3DVideo);
    serializeObject(data, SyncHelper::instance().variables.gridToMapOn);
    serializeObject(data, SyncHelper::instance().variables.radius);
    serializeObject(data, SyncHelper::instance().variables.fov);
    serializeObject(data, SyncHelper::instance().variables.rotateX);
    serializeObject(data, SyncHelper::instance().variables.rotateY);
    serializeObject(data, SyncHelper::instance().variables.rotateZ);
    return data;
}

void decode(const std::vector<std::byte>& data, unsigned int pos) {
    deserializeObject(data, pos, SyncHelper::instance().variables.syncOn);
    deserializeObject(data, pos, SyncHelper::instance().variables.alpha);
    if (SyncHelper::instance().variables.syncOn) {
        deserializeObject(data, pos, SyncHelper::instance().variables.loadedFile);
        deserializeObject(data, pos, SyncHelper::instance().variables.paused);
        deserializeObject(data, pos, SyncHelper::instance().variables.timePosition);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThreshold);
        deserializeObject(data, pos, SyncHelper::instance().variables.sbs3DVideo);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOn);
        deserializeObject(data, pos, SyncHelper::instance().variables.radius);
        deserializeObject(data, pos, SyncHelper::instance().variables.fov);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateZ);
    }
}

void postSyncPreDraw() {
#ifndef SGCT_ONLY
    //Apply synced commands
    if (!Engine::instance().isMaster()) {
        if(!SyncHelper::instance().variables.loadedFile.empty() && loadedVideoFile != SyncHelper::instance().variables.loadedFile){
            //Load new file
            loadedVideoFile = SyncHelper::instance().variables.loadedFile;
            QString newfile = QString::fromStdString(loadedVideoFile);
            Log::Info(fmt::format("Loading new file: {}", newfile.toStdString()));
            mpv::qt::command_async(mpvHandle, QStringList() << "loadfile" << newfile);
        }
        if(SyncHelper::instance().variables.paused != paused){
            paused = SyncHelper::instance().variables.paused;
            if(paused) {
                Log::Info("Video paused.");
            }
            else{
                Log::Info("Video playing...");
            }
            mpv::qt::set_property(mpvHandle, "pause", paused);
        }
        double currentTimePos = mpv::qt::get_property(mpvHandle, "time-pos").toDouble();
        if(SyncHelper::instance().variables.timePosition != currentTimePos && SyncHelper::instance().variables.syncOn){
            if(paused || (abs(currentTimePos-SyncHelper::instance().variables.timePosition)>SyncHelper::instance().variables.timeThreshold)){
                //Always set time pos when paused and not same time
                //Or set time position as it is above sync threshold
                mpv::qt::set_property(mpvHandle, "time-pos", SyncHelper::instance().variables.timePosition);
                Log::Info(fmt::format("New video position: {}", SyncHelper::instance().variables.timePosition));
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

        meshPrg->bind();

        const mat4& mvp = data.modelViewProjectionMatrix;
        glm::mat4 MVP_rot = glm::rotate(glm::make_mat4(mvp.values), glm::radians(float(360-SyncHelper::instance().variables.rotateX)), glm::vec3(1.0f, 0.0f, 0.0f));
        MVP_rot = glm::rotate(MVP_rot, glm::radians(float(SyncHelper::instance().variables.rotateY)), glm::vec3(0.0f, 1.0f, 0.0f));
        MVP_rot = glm::rotate(MVP_rot, glm::radians(float(SyncHelper::instance().variables.rotateZ)), glm::vec3(0.0f, 0.0f, 1.0f));
        glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_rot[0][0]);

        if (SyncHelper::instance().variables.sbs3DVideo) {
            glUniform1i(meshEyeModeLoc, (GLint)data.frustumMode);
        }
        else {
            glUniform1i(meshEyeModeLoc, 0);
        }

        glUniform1f(meshAlphaLoc, SyncHelper::instance().variables.alpha);

        if (SyncHelper::instance().variables.gridToMapOn == 2) {
            sphere->draw();
        }
        else {
            dome->draw();
        }

        meshPrg->unbind();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
    else {
        glEnable(GL_BLEND);

        videoPrg->bind();

        if (SyncHelper::instance().variables.sbs3DVideo) {
            glUniform1i(videoEyeModeLoc, (GLint)data.frustumMode);
        }
        else {
            glUniform1i(videoEyeModeLoc, 0);
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

