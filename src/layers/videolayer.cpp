#include "videolayer.h"
#include "application.h"
#include "audiosettings.h"
#include "track.h"
#include "qthelper.h"
#include <fmt/core.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>

void on_mpv_render_update(void* ctx) {
    VideoLayer* videoLayer = static_cast<VideoLayer*>(ctx);
    videoLayer->updateFbo();
}

VideoLayer::VideoLayer(opengl_func_adress_ptr opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel) : MpvLayer(opa, allowDirectRendering, loggingOn, logLevel) {
    setType(BaseLayer::LayerType::VIDEO);
}

VideoLayer::~VideoLayer() {
    cleanup();
}

void VideoLayer::initializeGL() {
    // Setup OpenGL MPV settings
    mpv_opengl_init_params gl_init_params[1] = {m_openglProcAdr, nullptr};
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
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
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &m_data.advancedControl},
        {MPV_RENDER_PARAM_INVALID, nullptr}};

    // This makes mpv use the currently set GL context. It will use the callback
    // (passed via params) to resolve GL builtin functions, as well as extensions.
    if (mpv_render_context_create(&m_data.renderContext, m_data.handle, params) < 0) {
        sgct::Log::Error("failed to initialize mpv GL context");
        return;
    }

    // When there is a need to call mpv_render_context_update(), which can
    // request a new frame to be rendered.
    // (Separate from the normal event handling mechanism for the sake of
    //  users which run OpenGL on a different thread.)
    mpv_render_context_set_update_callback(m_data.renderContext, on_mpv_render_update, reinterpret_cast<void *>(this));

    // Creating new FBO to render mpv into
    createMpvFBO(512, 512);

    m_data.mpvInitializedGL = true;
}

void VideoLayer::cleanup() {
    if (!m_data.mpvInitialized)
        return;

    // Destroy the GL renderer and all of the GL objects it allocated. If video
    // is still running, the video track will be deselected.
    if (m_data.mpvInitializedGL) {
        mpv_render_context_free(m_data.renderContext);
    }

    // End Mpv running on separate thread
    MpvLayer::cleanup();

    if (m_data.mpvInitializedGL) {
        glDeleteFramebuffers(1, &m_data.fboId);
        glDeleteTextures(1, &renderData.texId);
    }
}

void VideoLayer::updateFrame() {
    if (!m_data.mpvInitializedGL)
        return;

    updateFbo();

    // See render_gl.h on what OpenGL environment mpv expects, and
    // other API details.
    if (m_data.updateRendering) {
        mpv_opengl_fbo mpfbo{static_cast<int>(m_data.fboId), m_data.fboWidth, m_data.fboHeight, GL_RGBA16F};
        int flip_y{1};

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_render(m_data.renderContext, params);
    } else {
        int skip_rendering{1};
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_SKIP_RENDERING, &skip_rendering},
            {MPV_RENDER_PARAM_INVALID, nullptr}};
        mpv_render_context_render(m_data.renderContext, params);
    }
}

bool VideoLayer::ready() {
    return !m_data.loadedFile.empty() && m_data.updateRendering;
}

void VideoLayer::updateFbo() {
    checkNeededMpvFboResize();
}

void VideoLayer::checkNeededMpvFboResize() {
    if (m_data.fboWidth == renderData.width && m_data.fboHeight == renderData.height)
        return;

    int maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    if (renderData.width <= 0 || renderData.height <= 0 || renderData.width > maxTexSize || renderData.height > maxTexSize)
        return;

    sgct::Log::Info(fmt::format("New MPV FBO width:{} and height:{}", renderData.width, renderData.height));

    glDeleteFramebuffers(1, &m_data.fboId);
    glDeleteTextures(1, &renderData.texId);

    createMpvFBO(renderData.width, renderData.height);
}

void VideoLayer::createMpvFBO(int width, int height) {
    m_data.fboWidth = width;
    m_data.fboHeight = height;

    glGenFramebuffers(1, &m_data.fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, m_data.fboId);

    generateTexture(renderData.texId, width, height);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        renderData.texId,
        0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VideoLayer::generateTexture(unsigned int &id, int width, int height) {
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
