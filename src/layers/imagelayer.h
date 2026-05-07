/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include <layers/baselayer.h>
#include <sgct/sgct.h>
#include <vector>
#include <deque>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>

#ifdef SAIL_SUPPORT
namespace sail { class image; }
#endif

class ImageLayer : public BaseLayer {
public:
    enum class ImageDecoder {
        Auto,
        Wuffs,
        Sail,
        Sgct
    };

    struct FrameData {
        std::vector<unsigned char> pixels;
        int width = 0;
        int height = 0;
        unsigned int bytesPerLine = 0;
        unsigned int glFormat = 0x1908;         // GL_RGBA
        unsigned int glInternalFormat = 0x8058; // GL_RGBA8
        int delayMs = -1; // animation delay (-1 = not animated / multi-page)
    };

    // All state shared between the background decode thread and the render thread.
    // Heap-allocated via shared_ptr so the decode thread can safely outlive ImageLayer.
    struct ThreadContext {
        std::string filename;
        std::string identifier;
        ImageDecoder decoder = ImageDecoder::Wuffs;
        sgct::Image img;
#ifdef WUFFS_SUPPORT
        bool usedWuffs = false;
#endif
#ifdef SAIL_SUPPORT
        std::shared_ptr<sail::image> sailImage;
        int sailWidth = 0;
        int sailHeight = 0;
        unsigned int sailGLFormat = 0x1908;
        unsigned int sailGLInternalFormat = 0x8058;
        bool usedSail = false;
#endif
        std::atomic_bool threadRunning{false};
        std::atomic_bool imageDone{false};
        std::atomic_bool uploadDone{false};
        std::atomic_bool threadDone{false};
        std::atomic_bool abortRequested{false};

        std::deque<FrameData> frameQueue;
        mutable std::mutex queueMutex;
        std::condition_variable queueNotFull;

        std::atomic_int  totalFrameCount{0};
        std::atomic_bool allFramesLoaded{false};
        std::atomic_bool multiFrame{false};
        std::atomic_bool animated{false};
        std::atomic_bool usingFrameQueue{false};
    };

    // Legacy alias kept so call sites outside this file need no changes
    using ImageData = ThreadContext;

    ImageLayer(std::string identifier);
    ~ImageLayer();

    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

    bool processImageUpload(std::string filename, bool forceUpdate);
    std::string loadedFile();
    bool fileIsImage(std::string &filePath, ImageLayer::ImageDecoder &decoder);

    int frameCount() const;
    int currentFrameIndex() const;
    bool isAnimated() const;

    // Must be called from the render thread every frame to flush deferred GL deletions.
    static void processPendingGLCleanup();

    static int lastKnownGpuMemoryKB();
    static std::atomic_int s_lastKnownGpuMemoryKB;

private:
    static const int kMaxBufferedFrames = 8;

    std::string m_identifier;            // Persists across image loads
    std::shared_ptr<ThreadContext> m_ctx; // Shared with the decode thread
    std::unique_ptr<std::thread> m_thread;
    std::mutex m_updateMutex;

    // GPU texture ring buffer
    std::vector<unsigned int> m_texRing;
    int m_texRingIndex = 0;
    int m_texRingSize = 2;
    int m_texWidth = 0;
    int m_texHeight = 0;
    unsigned int m_texGLFormat = 0;
    unsigned int m_texGLInternalFormat = 0;

    // Frame display state (render-thread only)
    bool m_usingTexRing = false;
    bool m_hasFirstFrame = false;
    FrameData m_firstFrame;
    int m_currentDelayMs = 0;
    std::atomic_int m_currentFrameIndex{0};
    std::chrono::steady_clock::time_point m_lastFrameTime;

    // Deferred GL cleanup: texture IDs queued here from any thread,
    // deleted on the render thread by processPendingGLCleanup().
    static std::mutex s_pendingTexDeleteMutex;
    static std::vector<unsigned int> s_pendingTexToDelete;

    void signalAndDetachThread();
    void handleAsyncImageUpload();
    void releaseTexRing();
    void ensureTexRing(int width, int height, unsigned int glInternalFormat, unsigned int glFormat);
    void uploadFrameToGPU(const FrameData& frame);
};

#endif // IMAGELAYER_H