/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTSHOWLAYER_H
#define DIRECTSHOWLAYER_H

#include <layers/baselayer.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dshow.h>

// The Sample Grabber interfaces are not declared by this machine's Windows SDK.
// These match the classic DirectShow layout (samplegrabber.h); the symbols
// CLSID_SampleGrabber / IID_ISampleGrabber / IID_ISampleGrabberCB come from
// strmiids.lib, which is linked in CMake.
#pragma warning(push)
#pragma warning(disable : 5204) // COM interfaces have no destructor by design
struct ISampleGrabberCB : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SampleCallback(double sampleTime, IMediaSample* pMediaSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCallback(double sampleTime, BYTE* pBuffer, DWORD bufferLength) = 0;
};

struct ISampleGrabber : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL oneShot) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* pFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE* pFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL buffer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* pBuffer, long bufSize) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample** ppSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB* callback, int interfaceMode) = 0;
};
#pragma warning(pop)
#endif

// Renders a media file through a DirectShow filter graph (File Source Filter +
// Sample Grabber, both part of the Windows SDK quartz.dll). The grabbed frame
// is converted to RGBA8 and handed to the render thread which uploads it as an
// OpenGL texture. Still images show their single frame; video files keep
// playing through the same pipeline.
#ifdef _WIN32
// Video pixel formats understood by DirectShowLayer::SampleCallback().
enum class PixelFormat : int { Unknown = 0, RGB24, RGB32, YUY2, NV12, I420, YV12, UYVY };

class DirectShowLayer : public BaseLayer, public ISampleGrabberCB {
#else
class DirectShowLayer : public BaseLayer {
#endif
public:
    // Frame state shared between the DirectShow streaming thread (producer)
    // and the render thread (consumer). Heap-allocated via shared_ptr so a
    // sample callback can safely outlive the layer by microseconds.
    struct FrameHandoff {
        std::mutex mutex;
        std::vector<unsigned char> pixels; // RGBA8, top-down rows
        int width = 0;
        int height = 0;
        uint64_t frameCount = 0;           // bumped for every produced frame
    };

    DirectShowLayer();
    ~DirectShowLayer();

    void cleanup() override;
    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

    // Must be called from the render thread every frame to flush deferred GL deletions.
    static void processPendingGLCleanup();

    // Capture devices chosen in the UI (friendly names). An empty video device means
    // "no capture" - the layer plays back filepath() as a media file instead. The audio
    // device is stored for future use; this layer renders video only. Must be called
    // before initialize().
    void setCaptureDevices(const std::string& videoDevice, const std::string& audioDevice);

#ifdef _WIN32
    // IUnknown. The layer holds an immortal base reference, so Release() can
    // never drop the count to zero - destruction is owned by BaseLayer's
    // shared_ptr machinery (see cleanup()).
    HRESULT QueryInterface(REFIID riid, void** ppv) override;
    ULONG AddRef() override;
    ULONG Release() override;

    // ISampleGrabberCB - called on the DirectShow streaming thread.
    HRESULT SampleCallback(double sampleTime, IMediaSample* pSample) override;
    HRESULT BufferCallback(double sampleTime, BYTE* pBuffer, DWORD bufferLength) override;
#endif

private:
#ifdef _WIN32
    bool buildAndRunGraph(const std::string& pathUtf8); // render thread only
    void ensureGraph();                                  // render thread only
    void releaseGraph();                                 // any thread (MTA COM)
#endif
    bool consumeNewFrame(std::vector<unsigned char>& pixels, int& width, int& height);
    void uploadFrame(const std::vector<unsigned char>& pixels, int width, int height);

#ifdef _WIN32
    IGraphBuilder* m_graphBuilder = nullptr;
    IMediaControl* m_mediaControl = nullptr;
    IBaseFilter* m_fileSourceFilter = nullptr;
    IBaseFilter* m_sampleGrabberFilter = nullptr;
    std::string m_loadedFile;      // source the current graph is rendering (file path or "capture:<device>") - render thread
    std::string m_attemptedFile;   // last source a build was attempted for (render thread)
    bool m_buildFailed = false;    // don't retry a failed/stalled build for the same file
    std::chrono::steady_clock::time_point m_graphStart{};

    // Cached from the grabber's connection media type after Run(), used when a sample carries no media type.
    int m_cachedWidth = 0;
    int m_cachedHeight = 0;
    PixelFormat m_cachedPixelType = PixelFormat::Unknown;

    std::atomic<ULONG> m_comRefCount{1}; // immortal base reference (see QueryInterface)
#endif

    std::shared_ptr<FrameHandoff> m_handoff;
    uint64_t m_consumedFrameCount = 0; // render thread only

    // Capture devices chosen in the UI (see setCaptureDevices). Written before
    // initialize(), read by the render thread while running.
    std::string m_captureVideoDevice;
    std::string m_captureAudioDevice;

    mutable std::mutex m_graphMutex;   // guards graph build/release

    // Deferred GL cleanup: texture IDs queued here from any thread,
    // deleted on the render thread by processPendingGLCleanup().
    static std::mutex s_pendingTexDeleteMutex;
    static std::vector<unsigned int> s_pendingTexToDelete;
};

#endif // DIRECTSHOWLAYER_H
