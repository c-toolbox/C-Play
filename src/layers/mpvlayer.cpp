#include "mpvlayer.h"
#include <sgct/sgct.h>
#include <sgct/opengl.h>
#include <fmt/core.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "qthelper.h"
#include "application.h"

void on_mpv_render_update(void* ctx) {
    MpvLayer* mpvLayer = static_cast<MpvLayer*>(ctx);
    sgct::Window::makeSharedContextCurrent();
    mpvLayer->updateFbo();
}

void on_mpv_events(MpvLayer::mpvData& vd, BaseLayer::RenderParams& rp) {
    while (vd.handle) {
        mpv_event* event = mpv_wait_event(vd.handle, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {

        case MPV_EVENT_VIDEO_RECONFIG: {
            // Retrieve the new video size.
            int64_t w, h;
            if (mpv_get_property(vd.handle, "dwidth", MPV_FORMAT_INT64, &w) >= 0 &&
                mpv_get_property(vd.handle, "dheight", MPV_FORMAT_INT64, &h) >= 0 &&
                w > 0 && h > 0)
            {
                rp.width = w;
                rp.height = h;
                vd.reconfigs++;
                vd.updateRendering = (vd.reconfigs > vd.reconfigsBeforeUpdate);
                mpv::qt::set_property_async(vd.handle, QStringLiteral("time-pos"), vd.timePos);
            }
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property* prop = (mpv_event_property*)event->data;
            if (strcmp(prop->name, "video-params") == 0) {
                if (prop->format == MPV_FORMAT_NODE) {
                    const QVariant videoParams = mpv::qt::node_to_variant(reinterpret_cast<mpv_node*>(prop->data));
                    auto vm = videoParams.toMap();
                    rp.width = vm[QStringLiteral("w")].toInt();
                    rp.height = vm[QStringLiteral("h")].toInt();
                }
            }
            else if (strcmp(prop->name, "pause") == 0) {
                if (prop->format == MPV_FORMAT_FLAG) {
                    bool isPaused = *reinterpret_cast<bool*>(prop->data);
                    if (isPaused != vd.videoIsPaused)
                        mpv::qt::set_property_async(vd.handle, QStringLiteral("pause"), vd.videoIsPaused);
                }
            }
            else if (strcmp(prop->name, "duration") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    vd.videoDuration = *reinterpret_cast<double*>(prop->data);
                }
            }
            else if (strcmp(prop->name, "time-pos") == 0) {
                if (SyncHelper::instance().variables.timeThresholdEnabled) {
                    double timeToSet = vd.timePos;
                    //We do not want to "over-force" seeks, as this will slow down and might cause continued stutter.
                    //Normally, playback is syncronized, however looping depends on seek speed.
                    //Seek speeds (thus loop speed) is faster when no audio is present, thus nodes might be faster then master.
                    //Hence, we might need to correct things after a loop, between master and nodes.
                    if (!SyncHelper::instance().variables.timeThresholdOnLoopOnly
                        || (vd.eofMode > 1 && timeToSet < SyncHelper::instance().variables.timeThresholdOnLoopCheckTime)
                        || (vd.eofMode > 1 && timeToSet > (vd.videoDuration - SyncHelper::instance().variables.timeThresholdOnLoopCheckTime) && (vd.videoDuration > 0))
                        || (SyncHelper::instance().variables.loopTimeEnabled && timeToSet < (SyncHelper::instance().variables.loopTimeA + SyncHelper::instance().variables.timeThresholdOnLoopCheckTime))
                        || (SyncHelper::instance().variables.loopTimeEnabled && SyncHelper::instance().variables.loopTimeB < (timeToSet + SyncHelper::instance().variables.timeThresholdOnLoopCheckTime))) {
                        if (prop->format == MPV_FORMAT_DOUBLE) {
                            double latestPosition = *reinterpret_cast<double*>(prop->data);
                            if (SyncHelper::instance().variables.timeThreshold > 0 && (abs(latestPosition - timeToSet) > SyncHelper::instance().variables.timeThreshold))
                            {
                                mpv::qt::set_property_async(vd.handle, QStringLiteral("time-pos"), timeToSet);
                            }
                        }
                    }
                }
            }
            break;
        }

        case MPV_EVENT_LOG_MESSAGE: {
            mpv_event_log_message* message = (mpv_event_log_message*)event->data;
            if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_FATAL) {
                sgct::Log::Error(fmt::format("FATAL: {}", message->text));
            }
            else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_ERROR) {
                sgct::Log::Error(message->text);
            }
            else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_WARN) {
                sgct::Log::Warning(message->text);
            }
            else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_INFO) {
                sgct::Log::Info(message->text);
            }
            else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_V) {
                sgct::Log::Info(message->text);
            }
            else if (message->log_level == mpv_log_level::MPV_LOG_LEVEL_DEBUG) {
                sgct::Log::Debug(message->text);
            }
            break;
        }
        default: {
            // Ignore uninteresting or unknown events.
            break;
        }
        }
    }
}

void initMPV(MpvLayer::mpvData& vd) {
    vd.handle = mpv_create();
    if (!vd.handle)
        sgct::Log::Error("mpv context init failed");

    mpv_set_option_string(vd.handle, "vo", "libmpv");
    if (vd.loggingOn) {
        mpv_set_option_string(vd.handle, "terminal", "yes");
        mpv_set_option_string(vd.handle, "msg-level", "all=v");
        mpv_request_log_messages(vd.handle, vd.logLevel.c_str());
    }

    // Some minor options can only be set before mpv_initialize().
    if (mpv_initialize(vd.handle) < 0)
        sgct::Log::Error("mpv init failed");

    // Set default settings
    mpv::qt::set_property(vd.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
    mpv::qt::set_property(vd.handle, QStringLiteral("loop-file"), QStringLiteral("inf"));
    mpv::qt::set_property(vd.handle, QStringLiteral("aid"), QStringLiteral("no")); //No audio on nodes.

    // Load mpv configurations for nodes
    mpv::qt::load_configurations(vd.handle, QString::fromStdString(SyncHelper::instance().configuration.confAll));
    mpv::qt::load_configurations(vd.handle, QString::fromStdString(SyncHelper::instance().configuration.confNodesOnly));

    if (vd.allowDirectRendering) {
        //Run with direct rendering if requested
        if (mpv::qt::get_property(vd.handle, QStringLiteral("vd-lavc-dr")).toBool()) {
            vd.advancedControl = 1;
            vd.reconfigsBeforeUpdate = 0;
        }
        else {
            vd.advancedControl = 0;
            vd.reconfigsBeforeUpdate = 0;
        }
    }
    else {
        //Do not allow direct rendering (EVER).
        mpv::qt::set_property(vd.handle, QStringLiteral("vd-lavc-dr"), QStringLiteral("no"));
        vd.advancedControl = 0;
        vd.reconfigsBeforeUpdate = 0;
    }
}

auto runMpvAsync = [](MpvLayer::mpvData& data, BaseLayer::RenderParams& rp) {
    data.threadRunning = true;
    initMPV(data);
    data.mpvInitialized = true;
    while (!data.terminate) {
        on_mpv_events(data, rp);
    }
    mpv_destroy(data.handle);
    data.threadDone = true;
};

static void* get_proc_address_mpv(void*, const char* name) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

MpvLayer::MpvLayer(bool allowDirectRendering, bool loggingOn, std::string logLevel) {
    videoData.allowDirectRendering = allowDirectRendering;
    videoData.loggingOn = loggingOn;
}

MpvLayer::~MpvLayer() {
}

void MpvLayer::initialize() {
    //Run MPV on another thread
    videoData.trd = std::make_unique<std::thread>(runMpvAsync, std::ref(videoData), std::ref(renderData));
    while (!videoData.mpvInitialized) {}

    //Observe video parameters
    mpv_observe_property(videoData.handle, 0, "video-params", MPV_FORMAT_NODE);
    mpv_observe_property(videoData.handle, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(videoData.handle, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(videoData.handle, 0, "duration", MPV_FORMAT_DOUBLE);

    // Setup OpenGL MPV settings
    mpv_opengl_init_params gl_init_params[1] = { get_proc_address_mpv, nullptr };
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        // Tell libmpv that you will call mpv_render_context_update() on render
        // context update callbacks, and that you will _not_ block on the core
        // ever (see <libmpv/render.h> "Threading" section for what libmpv
        // functions you can call at all when this is active).
        // In particular, this means you must call e.g. mpv_command_async()
        // instead of mpv_command().
        // If you want to use synchronous calls, either make them on a separate
        // thread, or remove the option below (this will disable features like
        // DR and is not recommended anyway).
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &videoData.advancedControl},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    // This makes mpv use the currently set GL context. It will use the callback
    // (passed via params) to resolve GL builtin functions, as well as extensions.
    if (mpv_render_context_create(&videoData.renderContext, videoData.handle, params) < 0)
        sgct::Log::Error("failed to initialize mpv GL context");

    // When there is a need to call mpv_render_context_update(), which can
    // request a new frame to be rendered.
    // (Separate from the normal event handling mechanism for the sake of
    //  users which run OpenGL on a different thread.)
    mpv_render_context_set_update_callback(videoData.renderContext, on_mpv_render_update, reinterpret_cast<void*>(this));

    //Creating new FBO to render mpv into
    createMpvFBO(512, 512);
}

void MpvLayer::cleanup() {
    // Destroy the GL renderer and all of the GL objects it allocated. If video
    // is still running, the video track will be deselected.
    mpv_render_context_free(videoData.renderContext);

    //End Mpv running on separate thread
    videoData.terminate = true;
    while (!videoData.threadDone) {}
    videoData.trd->join();
    videoData.trd = nullptr;

    glDeleteFramebuffers(1, &videoData.fboId);
}

void MpvLayer::updateFrame() {
    updateFbo();

    // See render_gl.h on what OpenGL environment mpv expects, and
    // other API details.
    if (videoData.updateRendering) {
        mpv_opengl_fbo mpfbo{ static_cast<int>(videoData.fboId), videoData.fboWidth, videoData.fboHeight, GL_RGBA16F };
        int flip_y{ 1 };

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context_render(videoData.renderContext, params);
    }
    else {
        int skip_rendering{ 1 };
        mpv_render_param params[] = {
                {MPV_RENDER_PARAM_SKIP_RENDERING, &skip_rendering},
                {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        mpv_render_context_render(videoData.renderContext, params);
    }
}

void MpvLayer::loadFile(std::string filePath, bool reload) {
    if (!filePath.empty() && (reload || videoData.loadedFile != filePath)) {
        sgct::Log::Info(fmt::format("Loading new file with mpv: {}", filePath));
        videoData.reconfigs = 0;
        videoData.updateRendering = false;
        videoData.loadedFile = filePath;
        mpv::qt::command_async(videoData.handle, QStringList() << QStringLiteral("loadfile") << QString::fromStdString(filePath));
    }
}

std::string MpvLayer::loadedFile() {
    return videoData.loadedFile;
}

bool MpvLayer::hasLoadedFile() {
    return videoData.loadedFile.empty();
}

void MpvLayer::updateFbo() {
    checkNeededMpvFboResize();
}

void MpvLayer::skipRendering(bool skipRendering) {
    videoData.updateRendering = !skipRendering;
}

bool MpvLayer::renderingIsOn() {
    return videoData.updateRendering;
}

void MpvLayer::setPause(bool paused) {
    if (paused != videoData.videoIsPaused) {
        videoData.videoIsPaused = paused;
        mpv::qt::set_property_async(videoData.handle, QStringLiteral("pause"), videoData.videoIsPaused);
        if (videoData.videoIsPaused) {
            sgct::Log::Info("Video paused.");
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("time-pos"), videoData.timePos);
        }
        else {
            sgct::Log::Info("Video playing...");
        }   
    }
}

void MpvLayer::setEOFMode(int eofMode){
    if (eofMode != videoData.eofMode) {
        videoData.eofMode = eofMode;

        if (videoData.eofMode == 0) { //Pause
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
        }
        else if (videoData.eofMode == 1) { //Continue
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("keep-open"), QStringLiteral("no"));
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("loop-file"), QStringLiteral("no"));
        }
        else { //Loop
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("keep-open"), QStringLiteral("yes"));
            mpv::qt::set_property_async(videoData.handle, QStringLiteral("loop-file"), QStringLiteral("inf"));
        }
    }
}

void MpvLayer::setTimePosition(double timePos, bool updateTime) {
    videoData.timePos = timePos;

    if(updateTime)
        mpv::qt::set_property_async(videoData.handle, QStringLiteral("time-pos"), timePos);
}

void MpvLayer::setLoopTime(double A, double B, bool enabled) {
    if (enabled) {
        mpv::qt::set_property_async(videoData.handle, QStringLiteral("ab-loop-a"), A);
        mpv::qt::set_property_async(videoData.handle, QStringLiteral("ab-loop-b"), B);
    }
    else {
        mpv::qt::set_property_async(videoData.handle, QStringLiteral("ab-loop-a"), QStringLiteral("no"));
        mpv::qt::set_property_async(videoData.handle, QStringLiteral("ab-loop-b"), QStringLiteral("no"));
    }
}

void MpvLayer::setValue(std::string param, int val) {
    mpv::qt::set_property_async(videoData.handle, QString::fromStdString(param), val);
}

void MpvLayer::checkNeededMpvFboResize() {
    if (videoData.fboWidth == renderData.width && videoData.fboHeight == renderData.height)
        return;

    int maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if (renderData.width <= 0 || renderData.height <= 0 || renderData.width > maxTexSize || renderData.height > maxTexSize)
        return;

    sgct::Log::Info(fmt::format("New MPV FBO width:{} and height:{}", renderData.width, renderData.height));

    glDeleteFramebuffers(1, &videoData.fboId);
    glDeleteTextures(1, &renderData.texId);

    createMpvFBO(renderData.width, renderData.height);
}

void MpvLayer::createMpvFBO(int width, int height) {
    videoData.fboWidth = width;
    videoData.fboHeight = height;

    sgct::Window::makeSharedContextCurrent();

    glGenFramebuffers(1, &videoData.fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, videoData.fboId);

    generateTexture(renderData.texId, width, height);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        renderData.texId,
        0
    );

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MpvLayer::generateTexture(unsigned int& id, int width, int height) {
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
