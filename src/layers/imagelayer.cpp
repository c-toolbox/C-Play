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
#include <algorithm>
#include <map>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

#ifdef SAIL_SUPPORT
#include <sail-c++/sail-c++.h>
#endif

std::atomic_int ImageLayer::s_lastKnownGpuMemoryKB{0};
std::mutex ImageLayer::s_pendingTexDeleteMutex;
std::vector<unsigned int> ImageLayer::s_pendingTexToDelete;

static std::uint64_t queryTotalSystemMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo))
        return static_cast<std::uint64_t>(memInfo.ullTotalPhys);
#elif defined(__APPLE__)
    std::uint64_t memSize = 0;
    size_t len = sizeof(memSize);
    if (sysctlbyname("hw.memsize", &memSize, &len, nullptr, 0) == 0)
        return memSize;
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0)
        return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize);
#endif
    return 0;
}

std::uint64_t ImageLayer::totalSystemMemoryBytes() {
    static std::uint64_t cached = queryTotalSystemMemoryBytes();
    return cached;
}

static std::uint64_t computeCpuBudgetBytes() {
    int percent = ImageSettings::cpuMemoryForImageBuffering();
    if (percent < 1) percent = 1;
    if (percent > 90) percent = 90;
    std::uint64_t totalRAM = ImageLayer::totalSystemMemoryBytes();
    if (totalRAM == 0) {
        // Fallback: assume 8 GB
        totalRAM = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    }
    return (totalRAM * static_cast<std::uint64_t>(percent)) / 100ULL;
}

static int computeImageBufferingThreadCount() {
    const int configured = ImageSettings::imageBufferingThreadCount();
    return std::clamp(configured, 1, 64);
}

static int framePixelSize(unsigned int glFormat) {
    return (glFormat == GL_RGB || glFormat == GL_BGR) ? 3 : 4;
}

static int computeCpuFrameCapacity(std::uint64_t budgetBytes, std::size_t frameSizeBytes, int totalFrames) {
    if (totalFrames <= 0) return 0;
    if (frameSizeBytes == 0 || budgetBytes == 0) return totalFrames;
    int capacity = static_cast<int>(budgetBytes / frameSizeBytes);
    capacity = std::max(1, capacity);
    return std::min(capacity, totalFrames);
}

static int queryTotalGpuMemoryKB() {
    const int GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX = 0x9048;
    const int GL_VBO_FREE_MEMORY_ATI = 0x87FB;
    // Clear any stale GL errors so they don't interfere with the query
    while (glGetError() != GL_NO_ERROR) {}
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

static std::pair<ImageLayer::FrameData, bool> decodeImageFileToFrame(
    const std::string& framePath, int delayMs, const std::string& identifier)
{
    ImageLayer::FrameData frame;
    frame.delayMs = delayMs;

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
        return {std::move(frame), true};
    } catch (...) {}
#endif
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
        return {std::move(frame), true};
    } catch (...) {
        sgct::Log::Warning(std::format("ImageLayer '{}': Failed to stream sequence frame: {}",
            identifier, framePath));
        return {ImageLayer::FrameData(), false};
    }
}

// Background loader for a single image file.
// Reads the first frame, probes for more. Single-frame images are uploaded
// directly (no ring buffer). Multi-frame images (GIF, APNG, etc.) are decoded
// into the CPU frame queue; the render thread decides buffering strategy.
auto loadImageAsync = [](std::shared_ptr<ImageLayer::ThreadContext> ctx) {
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
        // Helper: convert a decoded sail::image into a FrameData (copies pixel data)
        auto sailToFrameData = [](sail::image& sailImg) -> ImageLayer::FrameData {
            GLenum glFormat = GL_RGBA, glInternalFormat = GL_RGBA8;
            int pixelSize = 4;
            resolveGLFormats(sailImg, glFormat, glInternalFormat, pixelSize);

            int w = sailImg.width(), h = sailImg.height();
            unsigned int bytesPerLine = sailImg.bytes_per_line();
            size_t rowBytes = static_cast<size_t>(w) * pixelSize;
            size_t dataSize = rowBytes * h;

            ImageLayer::FrameData frame;
            frame.width = w; frame.height = h;
            frame.bytesPerLine = static_cast<unsigned int>(rowBytes);
            frame.glFormat = static_cast<unsigned int>(glFormat);
            frame.glInternalFormat = static_cast<unsigned int>(glInternalFormat);
            frame.delayMs = sailImg.delay();
            frame.pixels.resize(dataSize);

            const unsigned char* src = reinterpret_cast<const unsigned char*>(sailImg.pixels());
            if (bytesPerLine == static_cast<unsigned int>(rowBytes)) {
                std::memcpy(frame.pixels.data(), src, dataSize);
            } else {
                for (int y = 0; y < h; y++)
                    std::memcpy(frame.pixels.data() + static_cast<size_t>(y) * rowBytes,
                                src + static_cast<size_t>(y) * bytesPerLine, rowBytes);
            }
            return frame;
        };

        // --- Read the first frame ---
        sail::image_input input(ctx->filename);
        sail::image firstSailImg;
        sail_status_t status = input.next_frame(&firstSailImg);
        if (status != SAIL_OK) throw std::runtime_error("no frames");

        // --- Probe for a second frame ---
        sail::image secondSailImg;
        sail_status_t status2 = input.next_frame(&secondSailImg);

        if (status2 == SAIL_ERROR_NO_MORE_FRAMES) {
            // *** Single-frame image - store directly, no ring buffer ***
            GLenum glFormat = GL_RGBA, glInternalFormat = GL_RGBA8;
            int pixelSize = 4;
            resolveGLFormats(firstSailImg, glFormat, glInternalFormat, pixelSize);

            ctx->sailImage = std::make_shared<sail::image>(std::move(firstSailImg));
            ctx->sailWidth = ctx->sailImage->width();
            ctx->sailHeight = ctx->sailImage->height();
            ctx->sailGLFormat = static_cast<unsigned int>(glFormat);
            ctx->sailGLInternalFormat = static_cast<unsigned int>(glInternalFormat);
            ctx->usedSail = true;
            ctx->imageDone = true;
            ctx->totalFrameCount = 1;
            ctx->allFramesLoaded = true;
            ctx->firstPassComplete = true;
            sgct::Log::Info(std::format("ImageLayer '{}': Single-frame image loaded via SAIL ({}x{})",
                ctx->identifier, ctx->sailWidth, ctx->sailHeight));
        } else if (status2 == SAIL_OK) {
            // *** Multi-frame image - keep decoded frames in CPU RAM up to the CPU budget ***
            ctx->usedSail = true;
            ctx->usingFrameQueue = true;
            ctx->multiFrame = true;
            ctx->loopPlayback = true;

            ImageLayer::FrameData frame1 = sailToFrameData(firstSailImg);
            if (frame1.delayMs >= 0) ctx->animated = true;
            ImageLayer::FrameData frame2 = sailToFrameData(secondSailImg);
            if (frame2.delayMs >= 0) ctx->animated = true;

            std::size_t frameSizeBytes = std::max(frame1.pixels.size(), frame2.pixels.size());
            int cpuCapacity = computeCpuFrameCapacity(ctx->cpuBudgetBytes, frameSizeBytes, std::numeric_limits<int>::max());
            cpuCapacity = std::max(2, cpuCapacity); // already proved this is multi-frame

            {
                std::lock_guard<std::mutex> lock(ctx->queueMutex);
                ctx->frameQueue.push_back(std::move(frame1));
                ctx->frameQueue.push_back(std::move(frame2));
            }

            int frameCount = 2;
            ctx->totalFrameCount = frameCount;
            bool reachedCpuCap = (frameCount >= cpuCapacity);

            // Continue reading remaining frames from the same input
            while (!ctx->abortRequested && !reachedCpuCap) {
                sail::image sailImg;
                status = input.next_frame(&sailImg);
                if (status == SAIL_ERROR_NO_MORE_FRAMES) break;
                if (status != SAIL_OK) break;

                ImageLayer::FrameData frame = sailToFrameData(sailImg);
                if (frame.delayMs >= 0) ctx->animated = true;
                {
                    std::lock_guard<std::mutex> lock(ctx->queueMutex);
                    ctx->frameQueue.push_back(std::move(frame));
                }
                ++frameCount;
                ctx->totalFrameCount = frameCount;
                reachedCpuCap = (frameCount >= cpuCapacity);
            }

            bool allFramesLoaded = !reachedCpuCap;
            if (reachedCpuCap && !ctx->abortRequested) {
                sail::image probeImg;
                allFramesLoaded = (input.next_frame(&probeImg) == SAIL_ERROR_NO_MORE_FRAMES);
            }

            ctx->allFramesLoaded = allFramesLoaded;
            ctx->firstPassComplete = true;
            sgct::Log::Info(std::format("ImageLayer '{}': Loaded {} frame(s) from multi-frame image into CPU RAM ({})",
                ctx->identifier, frameCount, ctx->allFramesLoaded ? "all frames loaded" : "CPU budget reached"));
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

    if (!ctx->usingFrameQueue)
        ctx->allFramesLoaded = true;

#ifdef SAIL_SUPPORT
    if (!ctx->usedSail || ctx->totalFrameCount == 0)
#endif
    {
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
// Loads the first image to determine frame size, then calculates how many fit in
// the CPU RAM budget. Loads up to that many frames using multiple decode threads.
// Does NOT loop - the render thread handles looping from whatever buffer was built.
auto loadImageSequenceAsync = [](std::shared_ptr<ImageLayer::ThreadContext> ctx,
                                 ImageLayer::SequenceParams seqParams) {
    ctx->threadDone = false;
    ctx->threadRunning = true;
    ctx->totalFrameCount = 0;
    ctx->allFramesLoaded = false;
    ctx->multiFrame = true;
    ctx->animated = false; // sequence is not self-timed like GIF
    ctx->usingFrameQueue = true;
    ctx->loopPlayback = seqParams.loop;
#ifdef SAIL_SUPPORT
    ctx->usedSail = true;
#endif

    const int step = std::max(1, seqParams.step);
    const int first = std::min(seqParams.startIndex, seqParams.stopIndex);
    const int last = std::max(seqParams.startIndex, seqParams.stopIndex);

    auto buildPath = [&](int frameIndex) -> std::string {
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

    // Helper: decode a single frame from disk into a FrameData
    auto decodeFrame = [&](const std::string& framePath, int delayMs) -> std::pair<ImageLayer::FrameData, bool> {
        ImageLayer::FrameData frame;
        frame.delayMs = delayMs;

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
            return {std::move(frame), true};
        } catch (...) {}
#endif
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
            return {std::move(frame), true};
        } catch (...) {
            sgct::Log::Warning(std::format("ImageLayer '{}': Failed to load sequence frame: {}",
                ctx->identifier, framePath));
            return {ImageLayer::FrameData(), false};
        }
    };

    // Build ordered list of existing frame indices
    std::vector<int> frameIndices;
    for (int i = first; i <= last; i += step) {
        std::string path = buildPath(i);
        if (std::filesystem::exists(path))
            frameIndices.push_back(i);
    }
    const int totalFrames = static_cast<int>(frameIndices.size());
    ctx->totalFrameCount = totalFrames;
    ctx->sequenceFramePaths.clear();
    ctx->sequenceFramePaths.reserve(totalFrames);
    for (int frameIndex : frameIndices) {
        ctx->sequenceFramePaths.push_back(buildPath(frameIndex));
    }

    if (totalFrames == 0) {
        ctx->allFramesLoaded = true;
        ctx->firstPassComplete = true;
        ctx->threadDone = true;
        ctx->threadRunning = false;
        return;
    }

    // Load first frame to determine per-frame size
    auto [firstFrame, firstLoaded] = decodeFrame(buildPath(frameIndices[0]), seqParams.delayMs);
    if (!firstLoaded || ctx->abortRequested) {
        ctx->allFramesLoaded = true;
        ctx->firstPassComplete = true;
        ctx->threadDone = true;
        ctx->threadRunning = false;
        return;
    }

    std::size_t frameSizeBytes = firstFrame.pixels.size();

    // Calculate how many frames fit in CPU budget. This decides whether the
    // CPU RAM tier is the complete sequence or a RAM-resident ring subset.
    int cpuCapacity = computeCpuFrameCapacity(ctx->cpuBudgetBytes, frameSizeBytes, totalFrames);
    if (totalFrames > 1)
        cpuCapacity = std::max(2, cpuCapacity);

    bool allFitCpu = (cpuCapacity >= totalFrames);
    int framesToLoad = allFitCpu ? totalFrames : cpuCapacity;

    sgct::Log::Info(std::format(
        "ImageLayer '{}': Sequence has {} frames, frame size={:.2f} MB, CPU budget={:.2f} GB -> loading {} frames ({})",
        ctx->identifier, totalFrames,
        static_cast<double>(frameSizeBytes) / (1024.0 * 1024.0),
        static_cast<double>(ctx->cpuBudgetBytes) / (1024.0 * 1024.0 * 1024.0),
        framesToLoad, allFitCpu ? "all fit in CPU RAM" : "CPU ring buffer"));

    // Enqueue the first frame we already decoded
    {
        std::lock_guard<std::mutex> lock(ctx->queueMutex);
        ctx->frameQueue.push_back(std::move(firstFrame));
    }

    if (framesToLoad <= 1) {
        ctx->allFramesLoaded = true;
        ctx->firstPassComplete = true;
        ctx->threadDone = true;
        ctx->threadRunning = false;
        return;
    }

    // Load remaining frames (up to framesToLoad) using multiple decode threads
    std::vector<int> indicesToLoad(frameIndices.begin() + 1,
        frameIndices.begin() + std::min(framesToLoad, totalFrames));

    int numWorkers = computeImageBufferingThreadCount();

    if (numWorkers <= 1 || static_cast<int>(indicesToLoad.size()) <= 1) {
        // Single-threaded decode
        for (int frameIndex : indicesToLoad) {
            if (ctx->abortRequested) break;
            auto [frame, loaded] = decodeFrame(buildPath(frameIndex), seqParams.delayMs);
            if (loaded) {
                std::lock_guard<std::mutex> lock(ctx->queueMutex);
                ctx->frameQueue.push_back(std::move(frame));
            }
        }
    } else {
        // Multi-threaded decode with order-preserving commit
        std::mutex pendingMutex;
        std::condition_variable pendingCv;
        std::map<int, ImageLayer::FrameData> pendingFrames;
        std::atomic_int nextToDecode{0};
        int totalIndices = static_cast<int>(indicesToLoad.size());

        auto workerFn = [&]() {
            while (!ctx->abortRequested) {
                int slot = nextToDecode.fetch_add(1);
                if (slot >= totalIndices) break;

                int frameIndex = indicesToLoad[slot];
                auto [frame, loaded] = decodeFrame(buildPath(frameIndex), seqParams.delayMs);
                {
                    std::lock_guard<std::mutex> lock(pendingMutex);
                    pendingFrames[slot] = loaded ? std::move(frame) : ImageLayer::FrameData();
                    pendingCv.notify_one();
                }
            }
        };

        auto commitFn = [&]() {
            int commitIdx = 0;
            while (commitIdx < totalIndices && !ctx->abortRequested) {
                ImageLayer::FrameData frame;
                bool gotFrame = false;
                {
                    std::unique_lock<std::mutex> lock(pendingMutex);
                    pendingCv.wait(lock, [&]() {
                        return pendingFrames.count(commitIdx) > 0 || ctx->abortRequested;
                    });
                    if (ctx->abortRequested) break;
                    auto it = pendingFrames.find(commitIdx);
                    if (it != pendingFrames.end()) {
                        if (!it->second.pixels.empty()) {
                            frame = std::move(it->second);
                            gotFrame = true;
                        }
                        pendingFrames.erase(it);
                    }
                }
                if (gotFrame) {
                    std::lock_guard<std::mutex> lock(ctx->queueMutex);
                    ctx->frameQueue.push_back(std::move(frame));
                }
                ++commitIdx;
            }
        };

        std::thread commitThread(commitFn);
        std::vector<std::thread> workers;
        for (int i = 0; i < numWorkers; ++i)
            workers.emplace_back(workerFn);
        for (auto& w : workers)
            w.join();
        {
            std::lock_guard<std::mutex> lock(pendingMutex);
            pendingCv.notify_all();
        }
        commitThread.join();
    }

    ctx->allFramesLoaded = allFitCpu;
    ctx->firstPassComplete = true;
    sgct::Log::Info(std::format("ImageLayer '{}': Decode complete, {} frames loaded to CPU queue",
        ctx->identifier, framesToLoad));

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
        const bool needsStart = !m_ctx || !m_ctx->threadRunning;
        processSequenceUpload(needsStart && !ready());
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

    int total = m_playbackFrameCount > 0 ? m_playbackFrameCount : m_ctx->totalFrameCount.load();

    // Non-looping: stop after the last frame has been shown
    if (!m_ctx->loopPlayback && m_currentFrameIndex >= total)
        return;

    switch (m_bufferMode) {
    case BufferMode::AllInGpu: {
        int count = m_gpuFrameCount;
        if (count == 0) return;
        if (m_uploadedFrames[m_gpuDisplayIndex].texId == 0 || !m_uploadedFrames[m_gpuDisplayIndex].ready) return;
        renderData.texId = m_uploadedFrames[m_gpuDisplayIndex].texId;
        renderData.width = m_uploadedFrames[m_gpuDisplayIndex].width;
        renderData.height = m_uploadedFrames[m_gpuDisplayIndex].height;
        m_currentDelayMs = m_uploadedFrames[m_gpuDisplayIndex].delayMs;
        m_currentGpuSlot = m_gpuDisplayIndex;
        m_gpuDisplayIndex = (m_gpuDisplayIndex + 1) % count;
        m_lastFrameTime = std::chrono::steady_clock::now();
        break;
    }
    case BufferMode::CpuToGpuRing: {
        if (m_gpuFrameCount <= 0) return;
        if (m_gpuReusableSlot >= 0) return;
        if (m_uploadedFrames[m_gpuDisplayIndex].texId == 0 || !m_uploadedFrames[m_gpuDisplayIndex].ready) return;

        int previousSlot = m_currentGpuSlot;
        renderData.texId = m_uploadedFrames[m_gpuDisplayIndex].texId;
        renderData.width = m_uploadedFrames[m_gpuDisplayIndex].width;
        renderData.height = m_uploadedFrames[m_gpuDisplayIndex].height;
        m_currentDelayMs = m_uploadedFrames[m_gpuDisplayIndex].delayMs;
        m_currentGpuSlot = m_gpuDisplayIndex;
        m_gpuDisplayIndex = (m_gpuDisplayIndex + 1) % m_gpuFrameCount;
        if (previousSlot >= 0 && previousSlot < static_cast<int>(m_uploadedFrames.size()) && previousSlot != m_currentGpuSlot) {
            m_uploadedFrames[previousSlot].ready = false;
            m_gpuReusableSlot = previousSlot;
        }
        m_lastFrameTime = std::chrono::steady_clock::now();
        break;
    }
    default:
        return;
    }

    int idx = m_currentFrameIndex + 1;
    if (m_ctx->loopPlayback && total > 0 && idx > total)
        idx = 1;
    m_currentFrameIndex = idx;
}

bool ImageLayer::ready() const {
    if (!m_ctx) return false;
    if (m_ctx->usingFrameQueue) {
        return m_bufferMode != BufferMode::None && renderData.texId != 0;
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
    processPendingGLCleanup(); // called from render thread � safe to delete textures here
    std::lock_guard<std::mutex> lock(m_updateMutex);
    handleAsyncImageUpload();

    if (forceUpdate) {
        if (filename.empty()) {
            sgct::Log::Info("Clearing background");
            signalAndDetachThread();
            m_ctx.reset();
            renderData.texId = 0; renderData.width = 0; renderData.height = 0;
            releaseTexRing();
            m_cpuRing.clear();
            resetCpuRingStreamingJobs();
        } else {
            if (fileIsImage(filename)) {
                signalAndDetachThread();

                m_hasFirstFrame = false;
                m_firstFrame = FrameData();
                m_currentDelayMs = 0;
                m_currentFrameIndex = 0;
                renderData.texId = 0; renderData.width = 0; renderData.height = 0;
                releaseTexRing();
                m_cpuRing.clear();
                resetCpuRingStreamingJobs();
                m_cpuRingReadIndex = 0;
                m_sequenceFramePaths.clear();
                m_nextDiskFrameIndex = 0;

                m_ctx = std::make_shared<ThreadContext>();
                m_ctx->filename = filename;
                m_ctx->identifier = m_identifier;
                m_ctx->cpuBudgetBytes = computeCpuBudgetBytes();

                sgct::Log::Info(std::format("Loading new {} image asynchronously: {}", m_identifier, filename));
                m_thread = std::make_unique<std::thread>(loadImageAsync, m_ctx);
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

    // Handle frame-queue based loading (image sequences and multi-frame images)
    // Must check usingFrameQueue BEFORE the early return below, because after
    // the decode thread finishes and the queue is drained, threadRunning=false
    // and hasQueuedFrames=false, but we still need drainCpuQueueAndDetermineMode()
    // to determine the buffering mode and refill the GPU ring.
    if (m_ctx->usingFrameQueue || hasQueuedFrames) {
        drainCpuQueueAndDetermineMode();

        if (m_ctx->allFramesLoaded && m_ctx->threadDone) m_ctx->threadRunning = false;
        return;
    }

    if (!m_ctx->threadRunning && !hasQueuedFrames && !m_ctx->imageDone)
        return;

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
        std::lock_guard<std::mutex> lock(s_pendingTexDeleteMutex);
        for (unsigned int id : m_texRing)
            if (id > 0) s_pendingTexToDelete.push_back(id);
        m_texRing.clear();
    }
    if (m_usingTexRing) { renderData.texId = 0; renderData.width = 0; renderData.height = 0; }
    m_usingTexRing = false;
    m_uploadedFrames.clear();
    m_gpuFrameCount = 0;
    m_currentGpuSlot = 0;
    m_gpuDisplayIndex = 0;
    m_gpuReusableSlot = -1;
    m_initialPrefillDone = false;
    m_allFramesFitGpu = false;
    m_allFramesFitCpu = false;
    m_playbackFrameCount = 0;
    m_bufferMode = BufferMode::None;
    m_texWidth = 0; m_texHeight = 0; m_texGLFormat = 0; m_texGLInternalFormat = 0;
}

void ImageLayer::ensureTexRing(int width, int height, unsigned int glInternalFormat, unsigned int glFormat, int desiredRingSize) {
    if (!m_texRing.empty() && m_texWidth == width && m_texHeight == height
        && m_texGLInternalFormat == glInternalFormat && m_texGLFormat == glFormat
        && m_texRingSize == desiredRingSize) return;

    releaseTexRing();
    processPendingGLCleanup(); // flush old IDs before allocating new ring

    m_texWidth = width; m_texHeight = height; m_texGLFormat = glFormat; m_texGLInternalFormat = glInternalFormat;

    m_texRingSize = std::max(1, desiredRingSize);
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

    // All textures start as available
    m_uploadedFrames.resize(m_texRingSize);
    m_gpuFrameCount = 0;
    m_currentGpuSlot = 0;
    m_gpuDisplayIndex = 0;
    m_gpuReusableSlot = -1;
    m_usingTexRing = true;
}

void ImageLayer::uploadFrameToSlot(int slot, const FrameData& frame) {
    if (slot < 0 || slot >= m_texRingSize || m_texRing.empty()) return;
    if (frame.width <= 0 || frame.height <= 0 || frame.width > m_texWidth || frame.height > m_texHeight) {
        sgct::Log::Warning(std::format(
            "ImageLayer '{}': Skipping invalid frame upload {}x{} into {}x{} texture slot {}",
            m_identifier, frame.width, frame.height, m_texWidth, m_texHeight, slot));
        return;
    }

    GLuint texId = m_texRing[slot];

    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        frame.width, frame.height,
        static_cast<GLenum>(frame.glFormat), GL_UNSIGNED_BYTE,
        frame.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    m_uploadedFrames[slot].texId = texId;
    m_uploadedFrames[slot].width = m_texWidth;
    m_uploadedFrames[slot].height = m_texHeight;
    m_uploadedFrames[slot].delayMs = frame.delayMs;
    m_uploadedFrames[slot].ready = true;
}

bool ImageLayer::queueCpuRingSlotRefill(int slot) {
    if (!m_isSequence || m_allFramesFitCpu || slot < 0 || slot >= static_cast<int>(m_cpuRing.size()))
        return false;
    if (m_sequenceFramePaths.empty())
        return false;
    if (slot >= static_cast<int>(m_cpuSlotStates.size()) || m_cpuSlotStates[slot] != CpuSlotState::Ready)
        return false;

    if (m_nextDiskFrameIndex >= static_cast<int>(m_sequenceFramePaths.size())) {
        if (!m_ctx || !m_ctx->loopPlayback)
            return false;
        m_nextDiskFrameIndex = 0;
    }

    m_cpuSlotStates[slot] = CpuSlotState::Empty;
    m_cpuRefillQueue.push_back({slot, m_nextDiskFrameIndex});
    ++m_nextDiskFrameIndex;
    pumpCpuRingRefillJobs();
    return true;
}

void ImageLayer::collectCompletedCpuRingRefills() {
    if (m_cpuRefillJobs.empty())
        return;

    std::vector<CpuRefillJob> activeJobs;
    activeJobs.reserve(m_cpuRefillJobs.size());
    for (CpuRefillJob& job : m_cpuRefillJobs) {
        bool done = false;
        bool loaded = false;
        FrameData frame;
        {
            std::lock_guard<std::mutex> lock(job.result->mutex);
            done = job.result->done;
            if (done) {
                loaded = job.result->loaded;
                if (loaded)
                    frame = std::move(job.result->frame);
            }
        }

        if (!done) {
            activeJobs.push_back(std::move(job));
            continue;
        }

        if (job.slot >= 0 && job.slot < static_cast<int>(m_cpuRing.size()) &&
            job.slot < static_cast<int>(m_cpuSlotStates.size())) {
            if (loaded) {
                m_cpuRing[job.slot] = std::move(frame);
                m_cpuSlotStates[job.slot] = CpuSlotState::Ready;
            } else {
                m_cpuSlotStates[job.slot] = CpuSlotState::Empty;
            }
        }
    }
    m_cpuRefillJobs = std::move(activeJobs);
}

void ImageLayer::pumpCpuRingRefillJobs() {
    const int maxJobs = computeImageBufferingThreadCount();
    while (!m_cpuRefillQueue.empty() && static_cast<int>(m_cpuRefillJobs.size()) < maxJobs) {
        CpuRefillRequest request = m_cpuRefillQueue.front();
        m_cpuRefillQueue.pop_front();
        if (request.slot < 0 || request.slot >= static_cast<int>(m_cpuRing.size()) ||
            request.slot >= static_cast<int>(m_cpuSlotStates.size()) ||
            request.diskFrameIndex < 0 || request.diskFrameIndex >= static_cast<int>(m_sequenceFramePaths.size())) {
            continue;
        }

        m_cpuSlotStates[request.slot] = CpuSlotState::Loading;
        auto result = std::make_shared<CpuRefillResult>();
        const std::string framePath = m_sequenceFramePaths[request.diskFrameIndex];
        const int delayMs = m_seqParams.delayMs;
        const std::string identifier = m_identifier;

        std::thread([result, framePath, delayMs, identifier]() {
            auto [frame, loaded] = decodeImageFileToFrame(framePath, delayMs, identifier);
            std::lock_guard<std::mutex> lock(result->mutex);
            result->frame = std::move(frame);
            result->loaded = loaded;
            result->done = true;
        }).detach();

        m_cpuRefillJobs.push_back({request.slot, request.diskFrameIndex, std::move(result)});
    }
}

void ImageLayer::resetCpuRingStreamingJobs() {
    m_cpuSlotStates.clear();
    m_cpuRefillQueue.clear();
    m_cpuRefillJobs.clear();
}

void ImageLayer::drainCpuQueueAndDetermineMode() {
    collectCompletedCpuRingRefills();
    pumpCpuRingRefillJobs();

    if (m_bufferMode != BufferMode::None) {
        // Mode already determined. In streaming mode, update() refills only the
        // slot that updateFrame() marked as no longer displayed. This keeps
        // updateFrame() fast and avoids overwriting the texture currently used
        // by renderData.
        if (m_bufferMode == BufferMode::CpuToGpuRing && m_gpuReusableSlot >= 0 && !m_cpuRing.empty()) {
            if (m_cpuRingReadIndex >= static_cast<int>(m_cpuRing.size())) {
                if (!m_allFramesFitCpu || m_ctx->loopPlayback)
                    m_cpuRingReadIndex = 0;
                else
                    return;
            }

            int consumedCpuSlot = m_cpuRingReadIndex;
            if (!m_cpuSlotStates.empty() &&
                (consumedCpuSlot >= static_cast<int>(m_cpuSlotStates.size()) ||
                 m_cpuSlotStates[consumedCpuSlot] != CpuSlotState::Ready)) {
                return;
            }

            uploadFrameToSlot(m_gpuReusableSlot, m_cpuRing[consumedCpuSlot]);
            ++m_cpuRingReadIndex;
            if (m_isSequence && !m_allFramesFitCpu)
                queueCpuRingSlotRefill(consumedCpuSlot);
            m_gpuReusableSlot = -1;
        }
        return;
    }

    // Drain decode thread's queue into our persistent CPU ring
    {
        std::lock_guard<std::mutex> lock(m_ctx->queueMutex);
        while (!m_ctx->frameQueue.empty()) {
            m_cpuRing.push_back(std::move(m_ctx->frameQueue.front()));
            m_ctx->frameQueue.pop_front();
        }
    }

    // Not all frames decoded yet - wait
    if (!m_ctx->firstPassComplete) return;
    if (m_cpuRing.empty()) return;

    int totalFrames = m_ctx->totalFrameCount.load();
    int cpuFrameCount = static_cast<int>(m_cpuRing.size());
    m_allFramesFitCpu = m_ctx->allFramesLoaded && cpuFrameCount >= totalFrames;
    m_sequenceFramePaths = m_ctx->sequenceFramePaths;
    m_nextDiskFrameIndex = cpuFrameCount;
    m_playbackFrameCount = (m_isSequence && !m_allFramesFitCpu && totalFrames > 0) ? totalFrames : cpuFrameCount;
    m_cpuSlotStates.assign(cpuFrameCount, CpuSlotState::Ready);

    // Set flipY for SAIL-decoded images
#ifdef SAIL_SUPPORT
    if (m_ctx->usedSail) setFlipY(true);
#endif

    // Determine texture dimensions and GPU capacity from the CPU-resident frames.
    // Some animated formats can expose smaller/larger per-frame rectangles; the
    // GL texture ring must be large enough before any glTexSubImage2D upload.
    const FrameData& firstFrame = m_cpuRing[0];
    int texWidth = firstFrame.width;
    int texHeight = firstFrame.height;
    for (const FrameData& frame : m_cpuRing) {
        texWidth = std::max(texWidth, frame.width);
        texHeight = std::max(texHeight, frame.height);
    }
    int pixelSize = framePixelSize(firstFrame.glFormat);
    size_t frameSizeBytes = std::max(firstFrame.pixels.size(),
        static_cast<size_t>(texWidth) * texHeight * pixelSize);

    int gpuCapacity = std::min(8, cpuFrameCount); // safe fallback when memory query is unavailable
    int totalGpuKB = queryTotalGpuMemoryKB();
    int gpuMemPercent = ImageSettings::gpuMemoryForImageBuffering();
    if (gpuMemPercent < 1) gpuMemPercent = 1;
    if (gpuMemPercent > 90) gpuMemPercent = 90;

    if (frameSizeBytes > 0 && totalGpuKB > 0) {
        size_t budgetBytes = (static_cast<size_t>(totalGpuKB) * 1024 * gpuMemPercent) / 100;
        gpuCapacity = static_cast<int>(budgetBytes / frameSizeBytes);
        gpuCapacity = std::max(1, gpuCapacity);
    }
    if (cpuFrameCount > 1)
        gpuCapacity = std::max(2, gpuCapacity);
    gpuCapacity = std::min(gpuCapacity, cpuFrameCount);

    bool allFitGpu = m_allFramesFitCpu && (gpuCapacity >= cpuFrameCount);
    int gpuRingSize = allFitGpu ? cpuFrameCount : gpuCapacity;

    sgct::Log::Info(std::format(
        "ImageLayer '{}': CPU frames: {} ({}), GPU capacity: {} -> mode: {}",
        m_identifier, cpuFrameCount, m_allFramesFitCpu ? "complete" : "ring subset", gpuCapacity,
        allFitGpu ? "AllInGpu" : "CpuToGpuRing"));

    if (allFitGpu) {
        // === AllInGpu: all CPU frames fit in VRAM. Upload once, then updateFrame()
        // only rotates through texture slots.
        ensureTexRing(texWidth, texHeight,
                      firstFrame.glInternalFormat, firstFrame.glFormat, gpuRingSize);
        m_bufferMode = BufferMode::AllInGpu;
        m_allFramesFitGpu = true;
        m_gpuFrameCount = cpuFrameCount;

        for (int i = 0; i < cpuFrameCount; ++i) {
            uploadFrameToSlot(i, m_cpuRing[i]);
        }

        renderData.texId = m_uploadedFrames[0].texId;
        renderData.width = m_uploadedFrames[0].width;
        renderData.height = m_uploadedFrames[0].height;
        m_currentDelayMs = m_uploadedFrames[0].delayMs;
        m_currentGpuSlot = 0;
        m_gpuDisplayIndex = (cpuFrameCount > 1) ? 1 : 0;
        m_gpuReusableSlot = -1;
        m_currentFrameIndex = 1;
        m_lastFrameTime = std::chrono::steady_clock::now();

        // No CPU RAM copy is needed when every frame is resident in VRAM.
        m_cpuRing.clear();
        m_cpuRing.shrink_to_fit();
        resetCpuRingStreamingJobs();

        sgct::Log::Info(std::format(
            "ImageLayer '{}': All {} frames uploaded to GPU - no further uploads needed",
            m_identifier, cpuFrameCount));
    } else {
        // === CpuToGpuRing: CPU RAM is the playback ring. VRAM keeps a smaller
        // ring; update() uploads into the slot released by updateFrame().
        ensureTexRing(texWidth, texHeight,
                      firstFrame.glInternalFormat, firstFrame.glFormat, gpuRingSize);
        m_bufferMode = BufferMode::CpuToGpuRing;
        m_allFramesFitGpu = false;

        // Initial fill.
        m_cpuRingReadIndex = 0;
        m_gpuFrameCount = 0;
        while (m_gpuFrameCount < m_texRingSize && m_cpuRingReadIndex < cpuFrameCount) {
            int consumedCpuSlot = m_cpuRingReadIndex;
            uploadFrameToSlot(m_gpuFrameCount, m_cpuRing[consumedCpuSlot]);
            ++m_cpuRingReadIndex;
            if (m_isSequence && !m_allFramesFitCpu)
                queueCpuRingSlotRefill(consumedCpuSlot);
            ++m_gpuFrameCount;
        }
        if ((!m_allFramesFitCpu || m_ctx->loopPlayback) && m_cpuRingReadIndex >= cpuFrameCount)
            m_cpuRingReadIndex = 0;

        if (m_gpuFrameCount > 0) {
            renderData.texId = m_uploadedFrames[0].texId;
            renderData.width = m_uploadedFrames[0].width;
            renderData.height = m_uploadedFrames[0].height;
            m_currentDelayMs = m_uploadedFrames[0].delayMs;
            m_currentGpuSlot = 0;
            m_gpuDisplayIndex = (m_gpuFrameCount > 1) ? 1 : 0;
            m_gpuReusableSlot = -1;
            m_currentFrameIndex = 1;
            m_lastFrameTime = std::chrono::steady_clock::now();
        }

        sgct::Log::Info(std::format(
            "ImageLayer '{}': Streaming mode - CPU ring: {}, GPU ring: {}, total: {}",
            m_identifier, cpuFrameCount, m_texRingSize, totalFrames));
    }

    m_hasFirstFrame = true;
    m_initialPrefillDone = true;
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

    if (forceUpdate && ready())
        return false;

    if (forceUpdate && m_isSequence) {
        signalAndDetachThread();

        m_hasFirstFrame = false;
        m_firstFrame = FrameData();
        m_currentDelayMs = 0;
        m_currentFrameIndex = 0;
        renderData.texId = 0; renderData.width = 0; renderData.height = 0;
        releaseTexRing();
        m_cpuRing.clear();
        resetCpuRingStreamingJobs();
        m_cpuRingReadIndex = 0;
        m_sequenceFramePaths.clear();
        m_nextDiskFrameIndex = 0;

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
        m_ctx->cpuBudgetBytes = computeCpuBudgetBytes();

        sgct::Log::Info(std::format("ImageLayer '{}': Loading image sequence from {}, frames {}-{} step {} (CPU budget: {:.2f} GB)",
            m_identifier, m_seqParams.directory,
            m_seqParams.startIndex, m_seqParams.stopIndex, m_seqParams.step,
            static_cast<double>(m_ctx->cpuBudgetBytes) / (1024.0 * 1024.0 * 1024.0)));

        m_thread = std::make_unique<std::thread>(loadImageSequenceAsync, m_ctx, m_seqParams);
        return true;
    }
    return false;
}
