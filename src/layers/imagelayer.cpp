/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "imagelayer.h"
#include "imagesettings.h"
#include <sgct/opengl.h>
#include <filesystem>

#ifdef SAIL_SUPPORT
#include <sail-c++/sail-c++.h>
#endif

std::atomic_int ImageLayer::s_lastKnownGpuMemoryKB{0};
std::mutex ImageLayer::s_pendingTexDeleteMutex;
std::vector<unsigned int> ImageLayer::s_pendingTexToDelete;

static int queryTotalGpuMemoryKB() {
    const int GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX = 0x9048;
    const int GL_VBO_FREE_MEMORY_ATI = 0x87FB;
    GLint totalMemKB = 0;
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &totalMemKB);
    if (glGetError() == GL_NO_ERROR && totalMemKB > 0) {
        ImageLayer::s_lastKnownGpuMemoryKB = totalMemKB;
        return totalMemKB;
    }
    while (glGetError() != GL_NO_ERROR) {}
    GLint amdInfo[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, amdInfo);
    if (glGetError() == GL_NO_ERROR && amdInfo[0] > 0) {
        ImageLayer::s_lastKnownGpuMemoryKB = amdInfo[0];
        return amdInfo[0];
    }
    while (glGetError() != GL_NO_ERROR) {}
    return 0;
}

#ifdef SAIL_SUPPORT
static void resolveGLFormats(sail::image& img, GLenum& glFormat, GLenum& glInternalFormat, int& pixelSize) {
    SailPixelFormat fmt = img.pixel_format();
    switch (fmt) {
        case SAIL_PIXEL_FORMAT_BPP24_RGB:  glFormat = GL_RGB;  glInternalFormat = GL_RGB8;  pixelSize = 3; break;
        case SAIL_PIXEL_FORMAT_BPP24_BGR:  glFormat = GL_BGR;  glInternalFormat = GL_RGB8;  pixelSize = 3; break;
        case SAIL_PIXEL_FORMAT_BPP32_RGBA: glFormat = GL_RGBA; glInternalFormat = GL_RGBA8; pixelSize = 4; break;
        case SAIL_PIXEL_FORMAT_BPP32_BGRA: glFormat = GL_BGRA; glInternalFormat = GL_RGBA8; pixelSize = 4; break;
        default:
            img.convert(SAIL_PIXEL_FORMAT_BPP32_RGBA);
            glFormat = GL_RGBA; glInternalFormat = GL_RGBA8; pixelSize = 4; break;
    }
}
#endif

// Background loader. Captures ThreadContext via shared_ptr so it safely outlives ImageLayer.
auto loadImageAsync = [](std::shared_ptr<ImageLayer::ThreadContext> ctx, int maxBuffered) {
    ctx->threadDone = false;
    ctx->threadRunning = true;
    ctx->totalFrameCount = 0;
    ctx->allFramesLoaded = false;
    ctx->multiFrame = false;
    ctx->animated = false;
    ctx->usingFrameQueue = false;

#ifdef SAIL_SUPPORT
    ctx->usedSail = false;
    try {
        bool firstPass = true;
        int firstPassFrameCount = 0;
        int firstPassAnimatedFrames = 0;

        while (!ctx->abortRequested) {
            sail::image_input input(ctx->filename);
            int passFrameIndex = 0;

            while (!ctx->abortRequested) {
                sail::image sailImg;
                sail_status_t status = input.next_frame(&sailImg);
                if (status == SAIL_ERROR_NO_MORE_FRAMES) break;
                if (status != SAIL_OK) {
                    sgct::Log::Warning(std::format("ImageLayer '{}': next_frame failed with status {}",
                        ctx->identifier, static_cast<int>(status)));
                    break;
                }

                GLenum glFormat = GL_RGBA, glInternalFormat = GL_RGBA8;
                int pixelSize = 4;
                resolveGLFormats(sailImg, glFormat, glInternalFormat, pixelSize);

                int w = sailImg.width(), h = sailImg.height();
                unsigned int bytesPerLine = sailImg.bytes_per_line();
                int delayMs = sailImg.delay();
                size_t rowBytes = static_cast<size_t>(w) * pixelSize;
                size_t dataSize = rowBytes * h;

                ImageLayer::FrameData frame;
                frame.width = w; frame.height = h;
                frame.bytesPerLine = static_cast<unsigned int>(rowBytes);
                frame.glFormat = static_cast<unsigned int>(glFormat);
                frame.glInternalFormat = static_cast<unsigned int>(glInternalFormat);
                frame.delayMs = delayMs;
                frame.pixels.resize(dataSize);

                const unsigned char* src = reinterpret_cast<const unsigned char*>(sailImg.pixels());
                if (bytesPerLine == static_cast<unsigned int>(rowBytes)) {
                    std::memcpy(frame.pixels.data(), src, dataSize);
                } else {
                    for (int y = 0; y < h; y++)
                        std::memcpy(frame.pixels.data() + static_cast<size_t>(y) * rowBytes,
                                    src + static_cast<size_t>(y) * bytesPerLine, rowBytes);
                }

                {
                    std::unique_lock<std::mutex> lock(ctx->queueMutex);
                    ctx->queueNotFull.wait(lock, [&]() {
                        return static_cast<int>(ctx->frameQueue.size()) < maxBuffered || ctx->abortRequested;
                    });
                    if (ctx->abortRequested) break;
                    ctx->frameQueue.push_back(std::move(frame));
                    ctx->usingFrameQueue = true;
                    ctx->usedSail = true;
                }

                ++passFrameIndex;
                if (firstPass) {
                    firstPassFrameCount = passFrameIndex;
                    ctx->totalFrameCount = firstPassFrameCount;
                    if (firstPassFrameCount > 1) ctx->multiFrame = true;
                    if (delayMs >= 0) {
                        ++firstPassAnimatedFrames;
                        if (firstPassFrameCount > 1 || firstPassAnimatedFrames > 1) ctx->animated = true;
                    }
                }
            }

            if (firstPass) {
                ctx->allFramesLoaded = true;
                firstPass = false;
                if (firstPassFrameCount <= 1) break;
                sgct::Log::Info(std::format("ImageLayer '{}': Loaded {} image frame(s), looping enabled",
                    ctx->identifier, firstPassFrameCount));
            }
            if (firstPassFrameCount <= 1) break;
        }
    } catch (...) {
        ctx->usedSail = false;
    }

    if (!ctx->usedSail) {
        try { ctx->img.load(ctx->filename); ctx->imageDone = true; }
        catch (...) { ctx->img = sgct::Image(); }
    }
#else
    try { ctx->img.load(ctx->filename); ctx->imageDone = true; }
    catch (...) { ctx->img = sgct::Image(); }
#endif

    ctx->allFramesLoaded = true;

    if (!ctx->usedSail || ctx->totalFrameCount == 0) {
        if (ctx->imageDone) {
            while (!ctx->uploadDone && !ctx->abortRequested)
                std::this_thread::yield();
        }
        ctx->img = sgct::Image();
    }

    ctx->threadDone = true;
    ctx->threadRunning = false;
};

// Background sequence loader. Loads individual image files from a numbered sequence.
auto loadImageSequenceAsync = [](std::shared_ptr<ImageLayer::ThreadContext> ctx,
                                 ImageLayer::SequenceParams seqParams, int maxBuffered) {
    ctx->threadDone = false;
    ctx->threadRunning = true;
    ctx->totalFrameCount = 0;
    ctx->allFramesLoaded = false;
    ctx->multiFrame = true;
    ctx->animated = false; // sequence is not self-timed like GIF
    ctx->usingFrameQueue = true;
#ifdef SAIL_SUPPORT
    ctx->usedSail = true;
#endif

    const int step = std::max(1, seqParams.step);
    const int first = std::min(seqParams.startIndex, seqParams.stopIndex);
    const int last = std::max(seqParams.startIndex, seqParams.stopIndex);
    const int totalFrames = ((last - first) / step) + 1;
    ctx->totalFrameCount = totalFrames;

    auto buildPath = [&](int frameIndex) -> std::string {
        // Build zero-padded filename: directory/prefix + digits + .suffix
        std::string digits = std::to_string(frameIndex);
        while (static_cast<int>(digits.size()) < seqParams.digitCount) {
            digits = "0" + digits;
        }
        std::string filename = seqParams.prefix + digits;
        if (!seqParams.suffix.empty()) {
            filename += "." + seqParams.suffix;
        }
        std::filesystem::path p = std::filesystem::path(seqParams.directory) / filename;
        return p.string();
    };

    bool looping = seqParams.loop;
    bool firstPass = true;

    while (!ctx->abortRequested) {
        for (int frameIndex = first; frameIndex <= last && !ctx->abortRequested; frameIndex += step) {
            std::string framePath = buildPath(frameIndex);

            if (!std::filesystem::exists(framePath)) {
                continue; // skip missing frames
            }

            ImageLayer::FrameData frame;
            frame.delayMs = seqParams.delayMs;
            bool loaded = false;

#ifdef SAIL_SUPPORT
            try {
                sail::image sailImg(framePath);
                GLenum glFormat = GL_RGBA, glInternalFormat = GL_RGBA8;
                int pixelSize = 4;
                resolveGLFormats(sailImg, glFormat, glInternalFormat, pixelSize);

                int w = sailImg.width(), h = sailImg.height();
                unsigned int bytesPerLine = sailImg.bytes_per_line();
                size_t rowBytes = static_cast<size_t>(w) * pixelSize;
                size_t dataSize = rowBytes * h;

                frame.width = w; frame.height = h;
                frame.bytesPerLine = static_cast<unsigned int>(rowBytes);
                frame.glFormat = static_cast<unsigned int>(glFormat);
                frame.glInternalFormat = static_cast<unsigned int>(glInternalFormat);
                frame.pixels.resize(dataSize);

                const unsigned char* src = reinterpret_cast<const unsigned char*>(sailImg.pixels());
                if (bytesPerLine == static_cast<unsigned int>(rowBytes)) {
                    std::memcpy(frame.pixels.data(), src, dataSize);
                } else {
                    for (int y = 0; y < h; y++)
                        std::memcpy(frame.pixels.data() + static_cast<size_t>(y) * rowBytes,
                                    src + static_cast<size_t>(y) * bytesPerLine, rowBytes);
                }
                loaded = true;
            } catch (...) {
                // SAIL failed, try sgct::Image fallback
            }
#endif
            if (!loaded) {
                try {
                    sgct::Image img;
                    img.load(framePath);
                    int w = img.size().x, h = img.size().y;
                    int channels = img.channels();
                    size_t dataSize = static_cast<size_t>(w) * h * channels;

                    frame.width = w; frame.height = h;
                    frame.bytesPerLine = static_cast<unsigned int>(w * channels);
                    frame.glFormat = (channels == 4) ? GL_RGBA : GL_RGB;
                    frame.glInternalFormat = (channels == 4) ? GL_RGBA8 : GL_RGB8;
                    frame.pixels.resize(dataSize);
                    std::memcpy(frame.pixels.data(), img.data(), dataSize);
                    loaded = true;
                } catch (...) {
                    sgct::Log::Warning(std::format("ImageLayer '{}': Failed to load sequence frame: {}",
                        ctx->identifier, framePath));
                    continue;
                }
            }

            if (loaded) {
                sgct::Log::Info(std::format("ImageLayer '{}': Loaded sequence frame {} ({}x{}) from: {}",
                    ctx->identifier, frameIndex, frame.width, frame.height, framePath));
                std::unique_lock<std::mutex> lock(ctx->queueMutex);
                ctx->queueNotFull.wait(lock, [&]() {
                    return static_cast<int>(ctx->frameQueue.size()) < maxBuffered || ctx->abortRequested;
                });
                if (ctx->abortRequested) break;
                ctx->frameQueue.push_back(std::move(frame));
            }
        }

        if (firstPass) {
            firstPass = false;
            if (!looping) break;
            sgct::Log::Info(std::format("ImageLayer '{}': Sequence pass complete ({} frames), looping.",
                ctx->identifier, totalFrames));
        }
        if (!looping) break;
    }

    ctx->allFramesLoaded = true;
    ctx->threadDone = true;
    ctx->threadRunning = false;
};

// ---------------------------------------------------------------------------

ImageLayer::ImageLayer(std::string identifier)
    : m_identifier(identifier) {
    setType(BaseLayer::LayerType::IMAGE);
}

ImageLayer::~ImageLayer() {
    // Non-blocking: signal the decode thread and detach it. It holds its own
    // shared_ptr<ThreadContext> and will release resources when it exits.
    signalAndDetachThread();
    m_ctx.reset();

    // Defer GL resource deletion to the next render-thread processPendingGLCleanup() call.
    releaseTexRing();
    if (renderData.texId > 0 && !m_usingTexRing) {
        std::lock_guard<std::mutex> lock(s_pendingTexDeleteMutex);
        s_pendingTexToDelete.push_back(renderData.texId);
        renderData.texId = 0;
    }
}

void ImageLayer::initialize() {
    m_hasInitialized = true;
}

void ImageLayer::update(bool updateRendering) {
    if (m_isSequence) {
        // In sequence mode, check if we need to start loading
        if (updateRendering || !ready()) {
            const bool needsStart = !m_ctx || !m_ctx->threadRunning;
            processSequenceUpload(needsStart && !ready());
        }
        if (updateRendering && ready())
            updateFrame();
        return;
    }

    const std::string currentPath = filepath();
    const bool fileChanged = m_ctx ? (m_ctx->filename != currentPath) : !currentPath.empty();
    if (updateRendering || !ready())
        processImageUpload(currentPath, fileChanged);
    if (updateRendering && ready())
        updateFrame();
}

void ImageLayer::updateFrame() {
    if (!m_ctx || !m_ctx->multiFrame || !ready())
        return;

    // Respect frame delay for both animated images and sequences
    if (m_currentDelayMs > 0) {
        auto now = std::chrono::steady_clock::now();
        int elapsedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameTime).count());
        if (elapsedMs < m_currentDelayMs)
            return;
    }

    FrameData frame;
    bool gotFrame = false;
    {
        std::lock_guard<std::mutex> lock(m_ctx->queueMutex);
        if (!m_ctx->frameQueue.empty()) {
            frame = std::move(m_ctx->frameQueue.front());
            m_ctx->frameQueue.pop_front();
            gotFrame = true;
        }
    }

    if (!gotFrame && m_ctx->allFramesLoaded && m_hasFirstFrame
            && m_ctx->totalFrameCount > 1 && m_currentFrameIndex >= m_ctx->totalFrameCount) {
        frame = m_firstFrame;
        gotFrame = true;
    }

    if (gotFrame) {
        m_ctx->queueNotFull.notify_one();
        uploadFrameToGPU(frame);
        m_currentDelayMs = frame.delayMs;
        m_lastFrameTime = std::chrono::steady_clock::now();

        if (!m_hasFirstFrame) {
            m_firstFrame = frame;
            m_hasFirstFrame = true;
            m_currentFrameIndex = 1;
        } else {
            int next = m_currentFrameIndex + 1;
            if (m_ctx->totalFrameCount > 0 && next > m_ctx->totalFrameCount) next = 1;
            m_currentFrameIndex = next;
        }
    }
}

bool ImageLayer::ready() const {
    if (!m_ctx) return false;
    if (m_ctx->usingFrameQueue) {
        // Once the first frame is uploaded, the layer is ready for rendering.
        // Subsequent frames will be pulled from the queue by updateFrame().
        return renderData.texId != 0;
    }
    return !m_ctx->filename.empty() && m_ctx->threadDone && renderData.texId > 0;
}

bool ImageLayer::hasTexture() const { return true; }

int ImageLayer::frameCount() const {
    return m_ctx ? m_ctx->totalFrameCount.load() : 0;
}

int ImageLayer::currentFrameIndex() const { return m_currentFrameIndex; }

bool ImageLayer::isAnimated() const {
    return m_ctx ? m_ctx->animated.load() : false;
}

int ImageLayer::lastKnownGpuMemoryKB() { return s_lastKnownGpuMemoryKB; }

/*static*/ void ImageLayer::processPendingGLCleanup() {
    std::lock_guard<std::mutex> lock(s_pendingTexDeleteMutex);
    if (!s_pendingTexToDelete.empty()) {
        glDeleteTextures(static_cast<GLsizei>(s_pendingTexToDelete.size()),
                         s_pendingTexToDelete.data());
        s_pendingTexToDelete.clear();
    }
}

bool ImageLayer::processImageUpload(std::string filename, bool forceUpdate) {
    processPendingGLCleanup(); // called from render thread – safe to delete textures here
    std::lock_guard<std::mutex> lock(m_updateMutex);
    handleAsyncImageUpload();

    if (forceUpdate) {
        if (filename.empty()) {
            sgct::Log::Info("Clearing background");
            signalAndDetachThread();
            m_ctx.reset();
            renderData.texId = 0; renderData.width = 0; renderData.height = 0;
            releaseTexRing();
        } else {
            if (fileIsImage(filename)) {
                signalAndDetachThread();

                m_hasFirstFrame = false;
                m_firstFrame = FrameData();
                m_currentDelayMs = 0;
                m_currentFrameIndex = 0;
                renderData.texId = 0; renderData.width = 0; renderData.height = 0;
                releaseTexRing();

                m_ctx = std::make_shared<ThreadContext>();
                m_ctx->filename = filename;
                m_ctx->identifier = m_identifier;

                sgct::Log::Info(std::format("Loading new {} image asynchronously: {}", m_identifier, filename));
                m_thread = std::make_unique<std::thread>(loadImageAsync, m_ctx, kMaxBufferedFrames);
                return true;
            } else {
                if (m_ctx) m_ctx->filename = "";
            }
        }
    }
    return false;
}

std::string ImageLayer::loadedFile() {
    return m_ctx ? m_ctx->filename : "";
}

bool ImageLayer::fileIsImage(std::string &filePath) {
    if (!filePath.empty()) {
        if (std::filesystem::exists(filePath)) {
            std::filesystem::path bgPath = std::filesystem::path(filePath);
            if (bgPath.has_extension()) {
                std::string bgPathExt = bgPath.extension().generic_string();
#ifdef SAIL_SUPPORT
                std::string extNoDot = bgPathExt.substr(1);
                sail::codec_info ci = sail::codec_info::from_extension(extNoDot);
                if (ci.is_valid()) return true;
#else
                std::transform(bgPathExt.begin(), bgPathExt.end(), bgPathExt.begin(),
                    [](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });
                if (bgPathExt == ".png" || bgPathExt == ".jpg" ||
                    bgPathExt == ".jpeg" || bgPathExt == ".tga") return true;
#endif
                sgct::Log::Warning(std::format("Image file extension is not supported: {}", filePath));
            } else {
                sgct::Log::Warning(std::format("Image file has no extension: {}", filePath));
            }
        } else {
            sgct::Log::Warning(std::format("Could not find image file: {}", filePath));
        }
    } else {
        sgct::Log::Warning(std::format("Image file is empty: {}", filePath));
    }
    return false;
}

void ImageLayer::signalAndDetachThread() {
    if (m_ctx) {
        m_ctx->abortRequested = true;
        m_ctx->uploadDone = true;
        {
            std::lock_guard<std::mutex> lock(m_ctx->queueMutex);
            m_ctx->frameQueue.clear();
        }
        m_ctx->queueNotFull.notify_all();
    }
    if (m_thread) {
        if (m_thread->joinable()) m_thread->detach();
        m_thread.reset();
    }
}

void ImageLayer::handleAsyncImageUpload() {
    if (!m_ctx) return;

    const bool hasQueuedFrames = [this]() {
        std::lock_guard<std::mutex> lock(m_ctx->queueMutex);
        return !m_ctx->frameQueue.empty();
    }();

    if (!m_ctx->threadRunning && !hasQueuedFrames && !m_ctx->imageDone)
        return;

    // Handle frame-queue based loading (image sequences and multi-frame images)
    if (m_ctx->usingFrameQueue || hasQueuedFrames) {
        if (renderData.texId == 0) {
            FrameData frame; bool gotFrame = false;
            {
                std::lock_guard<std::mutex> lock(m_ctx->queueMutex);
                if (!m_ctx->frameQueue.empty()) {
                    frame = std::move(m_ctx->frameQueue.front());
                    m_ctx->frameQueue.pop_front();
                    gotFrame = true;
                }
            }
            if (gotFrame) {
                m_ctx->queueNotFull.notify_one();
                uploadFrameToGPU(frame);
                m_currentDelayMs = frame.delayMs;
                m_lastFrameTime = std::chrono::steady_clock::now();
                m_currentFrameIndex = 1;
                if (!m_hasFirstFrame) { m_firstFrame = frame; m_hasFirstFrame = true; }
#ifdef SAIL_SUPPORT
                if (m_ctx->usedSail) {
                    setFlipY(true);
                }
#endif
                sgct::Log::Info(std::format("ImageLayer '{}': First frame uploaded ({}x{}, frames so far: {})",
                    m_identifier, frame.width, frame.height, m_ctx->totalFrameCount.load()));
            }
        }
        if (m_ctx->allFramesLoaded && m_ctx->threadDone) m_ctx->threadRunning = false;
        return;
    }

#ifdef SAIL_SUPPORT
    if (m_ctx->imageDone && !m_ctx->uploadDone) {
        if (m_ctx->usedSail && m_ctx->sailImage) {
            GLuint texId = 0;
            glGenTextures(1, &texId);
            glBindTexture(GL_TEXTURE_2D, texId);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            GLenum glFormat = static_cast<GLenum>(m_ctx->sailGLFormat);
            int pixelSize = (glFormat == GL_RGB || glFormat == GL_BGR) ? 3 : 4;
            unsigned int bpl = m_ctx->sailImage->bytes_per_line();
            glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<int>(bpl) / pixelSize);
            glTexImage2D(GL_TEXTURE_2D, 0,
                static_cast<GLenum>(m_ctx->sailGLInternalFormat),
                m_ctx->sailWidth, m_ctx->sailHeight, 0,
                glFormat, GL_UNSIGNED_BYTE, m_ctx->sailImage->pixels());
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_2D, 0);
            renderData.texId = texId;
            renderData.width = m_ctx->sailWidth;
            renderData.height = m_ctx->sailHeight;
            setFlipY(true);
            m_ctx->uploadDone = true;
        } else
#endif
        {
            int imgWidth = m_ctx->img.size().x, imgHeight = m_ctx->img.size().y;
            renderData.texId = sgct::TextureManager::instance().loadTexture(std::move(m_ctx->img));
            renderData.width = imgWidth; renderData.height = imgHeight;
            m_ctx->uploadDone = true;
        }
    } else if (m_ctx->threadDone) {
        m_ctx->threadRunning = false;
        m_ctx->imageDone = false;
        m_ctx->uploadDone = false;
    }
}

void ImageLayer::releaseTexRing() {
    if (!m_texRing.empty()) {
        // Thread-safe deferred deletion: actual glDeleteTextures runs in processPendingGLCleanup().
        std::lock_guard<std::mutex> lock(s_pendingTexDeleteMutex);
        for (unsigned int id : m_texRing)
            if (id > 0) s_pendingTexToDelete.push_back(id);
        m_texRing.clear();
    }
    if (m_usingTexRing) { renderData.texId = 0; renderData.width = 0; renderData.height = 0; }
    m_usingTexRing = false;
    m_texRingIndex = 0; m_texWidth = 0; m_texHeight = 0; m_texGLFormat = 0; m_texGLInternalFormat = 0;
}

void ImageLayer::ensureTexRing(int width, int height, unsigned int glInternalFormat, unsigned int glFormat) {
    if (!m_texRing.empty() && m_texWidth == width && m_texHeight == height
        && m_texGLInternalFormat == glInternalFormat && m_texGLFormat == glFormat) return;

    releaseTexRing();
    processPendingGLCleanup(); // flush old IDs before allocating new ring

    m_texWidth = width; m_texHeight = height; m_texGLFormat = glFormat; m_texGLInternalFormat = glInternalFormat;

    int ringSize = 8;
    int gpuMemPercent = ImageSettings::gpuMemoryForImageBuffering();
    if (gpuMemPercent < 1) gpuMemPercent = 1;
    if (gpuMemPercent > 90) gpuMemPercent = 90;

    int pixelSize = (glFormat == GL_RGB || glFormat == GL_BGR) ? 3 : 4;
    size_t frameSizeBytes = static_cast<size_t>(width) * height * pixelSize;

    if (frameSizeBytes > 0) {
        int totalGpuKB = queryTotalGpuMemoryKB();
        if (totalGpuKB > 0) {
            size_t budgetBytes = (static_cast<size_t>(totalGpuKB) * 1024 * gpuMemPercent) / 100;
            int computed = static_cast<int>(budgetBytes / frameSizeBytes);
            if (computed < 2) computed = 2;
            ringSize = computed;
            sgct::Log::Info(std::format("ImageLayer: GPU VRAM={:.2f} GB, budget={}% ({:.2f} GB), frame={:.2f} MB -> ring size={}",
                static_cast<double>(totalGpuKB) / (1024.0 * 1024.0), gpuMemPercent,
                static_cast<double>(budgetBytes) / (1024.0 * 1024.0 * 1024.0),
                static_cast<double>(frameSizeBytes) / (1024.0 * 1024.0), ringSize));
        } else {
            sgct::Log::Info("ImageLayer: Could not query GPU memory, using default ring size of 8");
        }
    }

    m_texRingSize = ringSize;
    m_texRing.resize(m_texRingSize, 0);
    glGenTextures(m_texRingSize, m_texRing.data());

    for (int i = 0; i < m_texRingSize; i++) {
        glBindTexture(GL_TEXTURE_2D, m_texRing[i]);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLenum>(glInternalFormat),
            width, height, 0, static_cast<GLenum>(glFormat), GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    m_texRingIndex = 0;
    m_usingTexRing = true;
}

void ImageLayer::uploadFrameToGPU(const FrameData& frame) {
    ensureTexRing(frame.width, frame.height, frame.glInternalFormat, frame.glFormat);

    m_texRingIndex = (m_texRingIndex + 1) % m_texRingSize;
    GLuint texId = m_texRing[m_texRingIndex];

    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        frame.width, frame.height,
        static_cast<GLenum>(frame.glFormat), GL_UNSIGNED_BYTE,
        frame.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    renderData.texId = texId;
    renderData.width = frame.width;
    renderData.height = frame.height;
}

void ImageLayer::setSequenceParams(const SequenceParams &params) {
    m_seqParams = params;
    m_isSequence = (params.digitCount > 0 && !params.directory.empty() &&
                    !params.prefix.empty() && params.startIndex <= params.stopIndex);
}

ImageLayer::SequenceParams ImageLayer::sequenceParams() const {
    return m_seqParams;
}

bool ImageLayer::isSequence() const {
    return m_isSequence;
}

bool ImageLayer::processSequenceUpload(bool forceUpdate) {
    processPendingGLCleanup();
    std::lock_guard<std::mutex> lock(m_updateMutex);
    handleAsyncImageUpload();

    if (forceUpdate && m_isSequence) {
        signalAndDetachThread();

        m_hasFirstFrame = false;
        m_firstFrame = FrameData();
        m_currentDelayMs = 0;
        m_currentFrameIndex = 0;
        renderData.texId = 0; renderData.width = 0; renderData.height = 0;
        releaseTexRing();

        m_ctx = std::make_shared<ThreadContext>();
        // Use first frame path as the context filename for identification
        std::string digits = std::to_string(m_seqParams.startIndex);
        while (static_cast<int>(digits.size()) < m_seqParams.digitCount) {
            digits = "0" + digits;
        }
        std::string firstFile = m_seqParams.prefix + digits;
        if (!m_seqParams.suffix.empty()) firstFile += "." + m_seqParams.suffix;
        m_ctx->filename = (std::filesystem::path(m_seqParams.directory) / firstFile).string();
        m_ctx->identifier = m_identifier;

        sgct::Log::Info(std::format("ImageLayer '{}': Loading image sequence from {}, frames {}-{} step {}",
            m_identifier, m_seqParams.directory,
            m_seqParams.startIndex, m_seqParams.stopIndex, m_seqParams.step));

        m_thread = std::make_unique<std::thread>(loadImageSequenceAsync, m_ctx, m_seqParams, kMaxBufferedFrames);
        return true;
    }
    return false;
}
