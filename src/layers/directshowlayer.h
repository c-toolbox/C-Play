/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTSHOWLAYER_H
#define DIRECTSHOWLAYER_H

#include <layers/baselayer.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
#  include <mmreg.h> // WAVEFORMATEX / WAVE_FORMAT_* / WAVEFORMATEXTENSIBLE (not pulled in by dshow.h)
#  include <portaudio.h>

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

// One COM callback object per sample grabber (see DirectShowLayer::m_videoCallback /
// m_audioCallback). Samples are routed by which grabber delivered them - not by the
// sample's own media type, which WDM/KS capture devices often do not attach. Each holds
// an immortal base reference: destruction is owned by the layer, which clears the
// grabber callback (releaseGraph()) before it is destroyed.
class DirectShowLayer; // forward declaration for the back-pointer below
class GrabberCallback : public ISampleGrabberCB {
public:
    enum class Role { Video, Audio };

    explicit GrabberCallback(DirectShowLayer* layer, Role role);

    HRESULT QueryInterface(REFIID riid, void** ppv) override;
    ULONG AddRef() override;
    ULONG Release() override;

    // Called on the DirectShow streaming thread.
    HRESULT SampleCallback(double sampleTime, IMediaSample* pMediaSample) override;
    HRESULT BufferCallback(double /*sampleTime*/, BYTE* /*pBuffer*/, DWORD /*bufferLength*/) override { return E_NOTIMPL; }

private:
    DirectShowLayer* m_layer = nullptr; // non-owning - the layer outlives its callbacks
    Role m_role = Role::Video;
    std::atomic<ULONG> m_refCount{1};   // immortal base reference held by the layer
};
#pragma warning(pop)
#endif

// Renders a media file through a DirectShow filter graph (File Source Filter +
// Sample Grabber, both part of the Windows SDK quartz.dll). The grabbed frame
// is converted to RGBA8 and handed to the render thread which uploads it as an
// OpenGL texture. Still images show their single frame; video files keep
// playing through the same pipeline. When audio is enabled, a second sample
// grabber captures the source's audio (or an audio capture device), which is
// converted to interleaved float32 and played out through PortAudio - mirroring
// NdiLayer/OmtLayer.
#ifdef _WIN32
// Video pixel formats understood by DirectShowLayer::handleVideoSample().
enum class PixelFormat : int { Unknown = 0, RGB24, RGB32, YUY2, NV12, I420, YV12, UYVY };

class DirectShowLayer : public BaseLayer {
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
    // "no capture" - the layer plays back filepath() as a media file instead, unless an
    // audio device was chosen too, which selects audio-only mode (microphone to the
    // PortAudio output, no video at all). A non-empty audio device routes that microphone
    // to the PortAudio output; otherwise the source's own audio track is used when available.
    // Must be called before initialize().
    void setCaptureDevices(const std::string& videoDevice, const std::string& audioDevice);

    // Stable key identifying the predefined DirectShow setup this layer was created from (the entry's title). Empty for custom device selections and file playback. Each machine resolves its own local capture devices from this key via its local data/predefined-directshows.json; a resolution with both devices empty means no capture on that machine.
    std::string presetKey() const;
    void setPresetKey(const std::string& key);

    // Type-specific sync of the preset key to/from the other machines (mirrors StreamLayer).
    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;

#ifdef _WIN32
    // Audio output via PortAudio - mirrors NdiLayer/OmtLayer.
    void start() override;
    void stop() override;
    bool hasAudio() const override;
    bool isAudioEnabled() const override;
    void enableAudio(bool enabled = true) override;
    void updateAudioOutput() override;
    void setVolume(int v, bool storeLevel = true) override;
    void setVolumeMute(bool v) override;
    // The audio level is reported from the PortAudio output callback, so a meter in the
    // LayerView can show live levels while the image renders.
    bool hasAudioLevels() const override { return true; }
#endif

private:
#ifdef _WIN32
    friend class GrabberCallback; // forwards grabber samples to handleVideoSample()/handleAudioSample()

    bool buildAndRunGraph(const std::string& pathUtf8, const std::string& videoDevice, const std::string& audioDevice); // graph worker thread only; devices are the per-machine resolved pair (see presetKey())
    bool buildVideoPath(const std::string& videoDevice); // graph worker thread only, during graph build: source -> video grabber; videoDevice is the per-machine resolved pair's video part (empty = file source)
    void ensureGraph();                                  // render thread only - enqueues work for the graph worker, never blocks on COM/PortAudio I/O
    void releaseGraph();                                 // graph worker thread only (MTA COM)
    void setupSignalDetection();                         // graph worker thread only, during graph build for capture devices
    void pollSignalPresence();                           // graph worker thread only - throttled internally

    // Graph worker thread (mirrors MpvLayer/ImageLayer): owns the DirectShow filter graph and all
    // PortAudio device I/O so the render/main thread never blocks on device enumeration, filter
    // negotiation or stream opens. Commands are single-slot latest-wins: a newer request supersedes
    // an older one that has not started yet.
    enum class GraphOp { None = 0, Build, Release };
    struct GraphCommand {
        GraphOp op = GraphOp::None;
        std::string sourceKey;   // "capture:<device>", "audio-capture:<device>" or the media file path
        std::string videoDevice; // per-machine resolved pair (empty = file source)
        std::string audioDevice;
        bool markFailed = false; // Release: remember this source as failed so ensureGraph() stops retrying it
    };
    void startWorker();          // render thread, idempotent - spawns the worker if not running/terminated
    void runGraphWorker();       // worker thread entry: COM init + command loop + final teardown
    void executeCommand(const GraphCommand& cmd); // worker thread only
    void audioMaintenance();     // worker idle loop: PortAudio init / WASAPI input open + output stream retry
    void enqueueGraphCommand(GraphOp op, const std::string& sourceKey,
                             const std::string& videoDevice = {}, const std::string& audioDevice = {},
                             bool markFailed = false); // any thread (latest wins)
    void publishStatus(bool hasGraph, const std::string& failedSource = {}); // worker thread only - updates the render-visible snapshot

    // Snapshot of the running graph for the render thread's decisions (ensureGraph/updateFrame).
    // Published by the worker after each build/release; read under m_statusMutex.
    struct GraphStatus {
        bool hasGraph = false;             // a graph is built and Run() succeeded
        std::string loadedFile;            // source key of the running graph
        bool audioPathBuilt = false;       // current graph contains an audio path
        bool audioPathUnavailable = false; // last build found no usable audio pin
        long long graphStartMs = 0;        // steady_clock ms when Run() succeeded (0 = none)
        std::string lastFailedSource;      // source key whose build failed/stalled - don't retry it
    };

    // Audio path / PortAudio output (mirrors NdiLayer/OmtLayer).
    bool buildAudioPath(const std::string& audioDevice);  // graph worker thread only, during graph build; non-fatal on failure. audioDevice is the per-machine resolved microphone name (empty = use the source's own audio pin)
    HRESULT handleVideoSample(IMediaSample* pSample);      // DirectShow streaming thread (video grabber callback)
    void handleAudioSample(const BYTE* pData, DWORD length, const AM_MEDIA_TYPE* pMt);  // DirectShow streaming thread (audio grabber callback)
    void ensurePortAudioInitialized();                       // graph worker thread only (idempotent)
    bool openAudioStreamLocked(int sampleRate, int channels); // caller holds m_audioStreamMutex
    void closeAudioStreamLocked();                            // caller holds m_audioStreamMutex
    void closeAudioStream();                                  // any thread
    PaDeviceIndex GetChosenApplicationAudioDevice();          // any thread

    // Low-latency WASAPI input for an explicit capture microphone: the WDM/DirectShow audio filter
    // delivers ~500ms blocks and adds that much steady-state latency, so the mic is captured through
    // a small-buffer PortAudio input stream instead (see openAudioInput). Graph worker thread only;
    // the PortAudio callback just pushes into m_audioRing (internally synchronized).
    bool openAudioInput(const std::string& deviceName);       // graph worker thread only; false -> caller falls back to buildAudioPath()
    void closeAudioInput();                                   // any thread
    void closeAudioInputLocked();                             // caller holds m_audioStreamMutex
    static int audioInputCallback(const void* inputBuffer, void*, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData);

    // Preset-aware capture device resolution shared by ensureGraph() (render thread) and the graph
    // worker's audio maintenance. Returns true when the names came from this machine's
    // predefined-directshows.json entry. Any thread - the resolver is internally synchronized.
    bool resolveCaptureDevices(std::string& videoDevice, std::string& audioDevice) const;

    // PortAudio output callback - drains m_audioRing into the output buffer. Runs on a
    // (possibly real-time) PortAudio thread: no allocation, no logging, minimal work.
    static int audioOutputCallback(const void* /*inputBuffer*/, void* outputBuffer,
                                   unsigned long framesPerBuffer,
                                   const PaStreamCallbackTimeInfo* /*streamTime*/,
                                   PaStreamCallbackFlags /*statusFlags*/,
                                   void* userData);
#endif
    bool consumeNewFrame(std::vector<unsigned char>& pixels, int& width, int& height);
    void uploadFrame(const std::vector<unsigned char>& pixels, int width, int height);

#ifdef _WIN32
    IGraphBuilder* m_graphBuilder = nullptr;
    IMediaControl* m_mediaControl = nullptr;
    IBaseFilter* m_fileSourceFilter = nullptr;
    IBaseFilter* m_sampleGrabberFilter = nullptr;
    std::string m_loadedFile;      // source the current graph is rendering (file path or "capture:<device>") - worker thread only, published via m_status
    std::chrono::steady_clock::time_point m_graphStart{}; // worker thread only, published via m_status

    // Graph worker state (see runGraphWorker). The command slot is single-slot latest-wins so rapid
    // source changes collapse into one build of the newest request.
    std::unique_ptr<std::thread> m_workerThread;
    std::mutex m_cmdMutex;                 // guards the command slot + worker spawn/terminate
    std::condition_variable m_cmdCv;
    GraphCommand m_pendingCmd{};           // guarded by m_cmdMutex
    bool m_hasPendingCmd = false;          // guarded by m_cmdMutex
    std::atomic<bool> m_workerRunning{false};
    std::atomic<bool> m_terminateWorker{false};

    std::mutex m_statusMutex;              // guards m_status + m_inFlightSource (worker publishes, render thread reads)
    GraphStatus m_status{};
    std::string m_inFlightSource;          // source key of the Build currently executing - guarded by m_statusMutex
    std::atomic<bool> m_releasePending{false}; // a Release command is queued/in flight (dedupes requests)

    // "No signal" detection for DeltaCast / Datapath capture cards (see setupSignalDetection()).
    // The card's WDM filter exposes IKsPropertySet; when it supports one of the vendor property
    // sets we poll its signal-status property once per second and log state transitions.
    enum class SignalVendor : int { None = 0, DeltaCast, Datapath };
    enum class SignalState : int { Unknown = 0, Absent, Present };

    IKsPropertySet* m_signalPropSet = nullptr; // owned reference for the graph's lifetime (released in releaseGraph)
    SignalVendor m_signalVendor = SignalVendor::None;
    GUID m_signalSetGuid{};   // vendor property set to poll
    DWORD m_signalPropId = 0; // signal-status property within that set
    std::atomic<SignalState> m_signalState{SignalState::Unknown};
    std::chrono::steady_clock::time_point m_lastSignalPoll{};

    // Cached from the grabber's connection media type after Run(), used when a sample carries no media type.
    int m_cachedWidth = 0;
    int m_cachedHeight = 0;
    PixelFormat m_cachedPixelType = PixelFormat::Unknown;

    // One callback object per grabber - samples are routed by which grabber delivered them,
    // not by the sample's own media type (see GrabberCallback above).
    GrabberCallback m_videoCallback{this, GrabberCallback::Role::Video};
    GrabberCallback m_audioCallback{this, GrabberCallback::Role::Audio};

    // Audio path: a second sample grabber fed from the source's audio pin (file playback, or a
    // capture filter with a built-in mic) or from an audio capture filter. Grabbed samples are
    // converted to interleaved float32, buffered in m_audioRing and played out through PortAudio.
    IBaseFilter* m_audioGrabberFilter = nullptr;
    IBaseFilter* m_audioSourceFilter = nullptr;  // audio capture filter (when a device is chosen)
    bool m_audioPathBuilt = false;               // current graph contains an audio path - worker thread only, published via m_status
    bool m_audioPathUnavailable = false;         // last build found no usable audio pin - worker thread only, published via m_status

    // Cached format of the connected audio pin, used when a sample carries no media type.
    int m_cachedAudioRate = 0;
    int m_cachedAudioChannels = 0;
    std::array<BYTE, sizeof(WAVEFORMATEXTENSIBLE)> m_cachedAudioFormat{};
    DWORD m_cachedAudioFormatSize = 0; // valid bytes in m_cachedAudioFormat (>= sizeof(WAVEFORMATEX))

    // Low-latency WASAPI input stream for an explicit capture microphone (see openAudioInput). The
    // render thread owns the stream handle and the device name; the atomics are read by the PortAudio
    // callback and update(). m_audioInputStalled is set by the watchdog in update() when a stream that
    // opened fine stops delivering samples entirely - buildAndRunGraph() then skips WASAPI for the
    // next build so the DirectShow WDM path takes over.
    PaStream* m_audioInputStream = nullptr;
    std::string m_audioInputDevice;
    std::atomic<bool> m_audioInputStalled{false}; // watchdog (render) sets, graph worker consumes on the next build
    std::atomic<bool> m_audioInputOpen{false};
    std::atomic<int> m_audioInputRate{0};
    std::atomic<int> m_audioInputChannels{0};
    std::atomic<long long> m_audioInputLastSampleMs{0}; // steady_clock ms of the last input packet (0 = none yet)

    // Bounded interleaved-float32 handoff between the DirectShow streaming thread (producer) and
    // the PortAudio callback (consumer). When full, oldest frames are dropped so a stalled
    // consumer can never grow memory or latency without bound.
    struct AudioRing {
        std::mutex mutex;
        std::vector<float> data;   // capacity = maxFrames * channels floats
        int channels = 0;          // channel count of the stored (source) layout
        size_t head = 0;           // next frame to write, in [0, maxFrames)
        size_t tail = 0;           // next frame to read, in [0, maxFrames)
        size_t frames = 0;         // frames currently available
        size_t maxFrames = 0;      // capacity in frames

        bool setFormat(int ch, int sampleRate); // (re)allocate for a new layout/rate; drops buffered audio
        void push(const float* interleaved, size_t nFrames);
        size_t pop(float* out, int outChannels, size_t maxNFrames, float volume);
        void clear();
    };
    AudioRing m_audioRing;

    // PortAudio output state (mirrors NdiLayer/OmtLayer).
    std::mutex m_audioStreamMutex;               // guards open/close/restart of m_audioStream
    PaStreamParameters m_audioOutputParameters{};
    PaStream* m_audioStream = nullptr;
    PaError m_audioError = paNoError;
    std::atomic<bool> m_isAudioEnabled{false};  // UI/render thread writes, graph worker + PortAudio callback read
    std::atomic<bool> m_portAudioInitialized{false}; // graph worker writes, render/streaming threads read
    std::atomic<bool> m_receiveAudio{false};         // audio enabled + PortAudio ready: samples are consumed
    std::atomic<bool> m_audioStreamOpen{false};      // start()/stop() read these without the stream mutex
    std::atomic<bool> m_audioStreamStarted{false};
    int m_paSampleRate = 0;                      // sample rate of the open stream (0 = never opened)
    int m_audioSourceChannels = 0;               // channel count of the grabbed audio (ring layout)
    std::atomic<int> m_audioOutputChannels{2};   // read by the PortAudio callback without locking
    std::atomic<float> m_audioVolume{1.0f};      // read by the PortAudio callback without locking
    std::atomic<bool> m_volumeMute{false};       // read by the PortAudio callback without locking
    std::atomic<bool> m_unsupportedAudioLogged{false}; // log unsupported codecs once per graph (streaming thread sets, render resets)
    std::chrono::steady_clock::time_point m_audioOpenLastFailed{};  // retry backoff for failed opens
#endif

    std::shared_ptr<FrameHandoff> m_handoff;
    uint64_t m_consumedFrameCount = 0; // render thread only

    // Capture devices chosen in the UI (see setCaptureDevices). Written before
    // initialize(), read by the render thread while running. When a preset key is set,
    // these act as the fallback for machines without a local predefined-directshows.json entry.
    std::string m_captureVideoDevice;
    std::string m_captureAudioDevice;

    // Stable key of the predefined DirectShow setup this layer was created from (entry title); empty for custom devices/files. Synced to all machines via encodeTypeCore/decodeTypeCore.
    std::string m_presetKey;

    // Deferred GL cleanup: texture IDs queued here from any thread,
    // deleted on the render thread by processPendingGLCleanup().
    static std::mutex s_pendingTexDeleteMutex;
    static std::vector<unsigned int> s_pendingTexToDelete;
};

#endif // DIRECTSHOWLAYER_H
