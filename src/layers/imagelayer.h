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
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <thread>

#ifdef SAIL_SUPPORT
namespace sail { class image; }
#endif

class ImageLayer : public BaseLayer {
public:
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
        sgct::Image img;
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
        std::vector<std::string> sequenceFramePaths;
        mutable std::mutex queueMutex;
        std::condition_variable queueNotFull;
        std::atomic<std::uint64_t> cpuQueuedBytes{0};
        std::uint64_t cpuBudgetBytes = 0; // set before thread starts

        std::atomic_int  totalFrameCount{0};
        std::atomic_bool allFramesLoaded{false};
        std::atomic_bool multiFrame{false};
        std::atomic_bool animated{false};
        std::atomic_bool usingFrameQueue{false};
        std::atomic_bool loopPlayback{false};
        std::atomic_bool firstPassComplete{false};
    };

    // Legacy alias kept so call sites outside this file need no changes
    using ImageData = ThreadContext;

    // Image sequence configuration
    struct SequenceParams {
        std::string directory;
        std::string prefix;
        std::string suffix;
        int digitCount = 0;
        int startIndex = 0;
        int stopIndex = 0;
        int step = 1;
        int delayMs = 33; // ~30fps default playback rate
        bool loop = true;
    };

    ImageLayer(std::string identifier);
    ~ImageLayer();

    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

    bool processImageUpload(std::string filename, bool forceUpdate);
    std::string loadedFile();
    bool fileIsImage(std::string &filePath);

    int frameCount() const;
    int currentFrameIndex() const;
    bool isAnimated() const;

    // Image sequence support
    void setSequenceParams(const SequenceParams &params);
    SequenceParams sequenceParams() const;
    bool isSequence() const;
    bool processSequenceUpload(bool forceUpdate);

    // Must be called from the render thread every frame to flush deferred GL deletions.
    static void processPendingGLCleanup();

    static int lastKnownGpuMemoryKB();
    static std::atomic_int s_lastKnownGpuMemoryKB;
    static std::uint64_t totalSystemMemoryBytes();

private:
    std::string m_identifier;            // Persists across image loads
    std::shared_ptr<ThreadContext> m_ctx; // Shared with the decode thread
    std::unique_ptr<std::thread> m_thread;
    std::mutex m_updateMutex;

    // Image sequence state
    SequenceParams m_seqParams;
    bool m_isSequence = false;

    // GPU texture ring buffer
    struct UploadedFrame {
        unsigned int texId = 0;
        int width = 0;
        int height = 0;
        int delayMs = 0;
        bool ready = false;
    };
    std::vector<unsigned int> m_texRing;           // pre-allocated GL texture IDs
    std::vector<UploadedFrame> m_uploadedFrames;   // metadata per texture slot
    int m_gpuFrameCount = 0;                       // number of valid texture slots
    int m_currentGpuSlot = 0;                      // slot currently referenced by renderData
    int m_gpuDisplayIndex = 0;                     // next slot to display
    int m_gpuReusableSlot = -1;                    // slot no longer displayed and safe to re-upload
    int m_texRingSize = 2;
    int m_texWidth = 0;
    int m_texHeight = 0;
    unsigned int m_texGLFormat = 0;
    unsigned int m_texGLInternalFormat = 0;

    // CPU RAM ring buffer — decoded frames kept in system memory
    std::vector<FrameData> m_cpuRing;       // persistent CPU-side buffer
    int m_cpuRingReadIndex = 0;             // next frame to upload from CPU ring to GPU
    bool m_allFramesFitCpu = false;         // all frames fit in CPU budget (no re-read from disk)
    std::vector<std::string> m_sequenceFramePaths;
    int m_nextDiskFrameIndex = 0;            // next sequence frame to stream into CPU ring

    enum class CpuSlotState {
        Empty,
        Ready,
        Loading,
    };
    struct CpuRefillRequest {
        int slot = -1;
        int diskFrameIndex = -1;
    };
    struct CpuRefillResult {
        mutable std::mutex mutex;
        bool done = false;
        bool loaded = false;
        FrameData frame;
    };
    struct CpuRefillJob {
        int slot = -1;
        int diskFrameIndex = -1;
        std::shared_ptr<CpuRefillResult> result;
    };
    std::vector<CpuSlotState> m_cpuSlotStates;
    std::deque<CpuRefillRequest> m_cpuRefillQueue;
    std::vector<CpuRefillJob> m_cpuRefillJobs;

    // Buffering mode (determined after first pass is complete)
    enum class BufferMode {
        None,           // not yet determined
        SingleFrame,    // one texture, no ring buffers
        AllInGpu,       // all frames uploaded to GPU textures, loop through them
        CpuToGpuRing,   // CPU ring → GPU ring streaming
    };
    BufferMode m_bufferMode = BufferMode::None;

    // Frame display state (render-thread only)
    bool m_usingTexRing = false;
    bool m_hasFirstFrame = false;
    bool m_initialPrefillDone = false;
    bool m_allFramesFitGpu = false;
    FrameData m_firstFrame;
    int m_currentDelayMs = 0;
    int m_playbackFrameCount = 0;
    std::atomic_int m_currentFrameIndex{0};
    std::chrono::steady_clock::time_point m_lastFrameTime;

    // Deferred GL cleanup: texture IDs queued here from any thread,
    // deleted on the render thread by processPendingGLCleanup().
    static std::mutex s_pendingTexDeleteMutex;
    static std::vector<unsigned int> s_pendingTexToDelete;

    void signalAndDetachThread();
    void handleAsyncImageUpload();
    void releaseTexRing();
    void ensureTexRing(int width, int height, unsigned int glInternalFormat, unsigned int glFormat, int desiredRingSize);
    void uploadFrameToSlot(int slot, const FrameData& frame);
    void drainCpuQueueAndDetermineMode();
    bool queueCpuRingSlotRefill(int slot);
    void collectCompletedCpuRingRefills();
    void pumpCpuRingRefillJobs();
    void resetCpuRingStreamingJobs();
};

#endif // IMAGELAYER_H