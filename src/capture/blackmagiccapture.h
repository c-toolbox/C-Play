/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BLACKMAGICCAPTURE_H
#define BLACKMAGICCAPTURE_H

#include <capture/capturebackend.h>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

// DeckLink COM interfaces (DeckLinkAPI_h.h, generated from the SDK's IDL); only the .cpp needs them.
struct IDeckLinkInput;
struct IDeckLinkInputCallback;
struct IDeckLinkVideoInputFrame;
struct IDeckLinkAudioInputPacket;
struct IDeckLinkVideoConversion;
class NdiGpuConversion;
class BlackmagicDirectGpu;

// Embedded audio of one DeckLink input (48 kHz stereo), played through PortAudio on the output
// device chosen in the audio settings. DeckLink delivers audio with each video frame, so the
// capture thread pushes it into a short FIFO that the PortAudio callback drains: when the capture
// runs ahead of the output clock the oldest samples are dropped, when the FIFO runs dry it plays
// silence until it is primed again. Opening/closing runs on an own worker thread to keep device
// I/O off the render thread.
class BlackmagicAudioCapture {
public:
    explicit BlackmagicAudioCapture(int input);
    ~BlackmagicAudioCapture();

    void keepAlive(float volume, bool reportLevels); // render thread, every frame while wanted
    cplay::audio::ChannelMixer& mixer() { return m_mixer; }
    // Up/down mix to the output layout; a change reopens the output with the new channel count.
    void setMixToOutput(bool mix);
    void restart();
    float takePeak();
    bool running() const { return m_running.load(); }

    // DeckLink capture thread: interleaved 32-bit stereo samples.
    void push(const int32_t* samples, unsigned long frames);
    // PortAudio callback thread.
    void fillOutput(float* out, unsigned long frames);

private:
    void run();
    bool open();
    void close();
    void clearFifo();

    const int m_input; // 1-based
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_terminate = false;
    bool m_restart = false;
    std::chrono::steady_clock::time_point m_lastKeepAlive{};

    bool m_paInitialized = false;
    void* m_stream = nullptr; // PaStream
    int m_outChannels = 2;
    cplay::audio::ChannelMixer m_mixer; // stereo -> output layout (standard or spatial mapping)
    std::string m_lastLoggedError;

    // Interleaved stereo ring buffer, guarded by m_fifoMutex.
    std::mutex m_fifoMutex;
    std::vector<float> m_fifo;
    size_t m_fifoRead = 0;
    size_t m_fifoFrames = 0;
    bool m_fifoPrimed = false;

    std::atomic<bool> m_running{false};
    std::atomic<float> m_volume{1.f};
    std::atomic<bool> m_reportLevels{false};
    std::atomic<bool> m_mixToOutput{true};
    std::atomic<float> m_peak{-1.f};
};

// Blackmagic DeckLink / UltraStudio / Intensity capture through the DeckLink SDK (COM server
// DeckLinkAPI64.dll, registered by the Blackmagic Desktop Video driver).
//
// Inputs are numbered 1.. in driver order over the devices (sub-devices of multi-channel cards)
// that can capture. The input format is detected automatically when the card supports it,
// otherwise the signal must be present when the capture opens.
//
// YCbCr 4:2:2 frames (8-bit UYVY) are uploaded as-is and converted to RGB by a compute shader;
// RGB 4:4:4 signals are captured as BGRA and uploaded directly. Without OpenGL 4.3 the SDK
// converts YCbCr frames to BGRA on the CPU. Ganging does not apply.
//
// GPU direct (directgpu=1, the default): the card captures into pinned system memory that is
// DMA'd into a GL buffer without a CPU copy - NVIDIA GPUDirect for Video (dvp.dll) on NVIDIA
// professional GPUs, AMD pinned memory (GL_AMD_pinned_memory) on AMD - and converted to RGB by
// the compute shader. Other GPUs, or a failing setup, use the paths above.
class BlackmagicCaptureBackend : public CaptureBackend {
public:
    explicit BlackmagicCaptureBackend(const CaptureSource& src);
    ~BlackmagicCaptureBackend() override;

    void update() override;
    void close() override;
    void stopCallbacks() override;

    void updateAudio(bool wanted, float volume, bool reportLevels, bool mixToOutput,
                     const cplay::audio::SpatialSource& source) override;
    void restartAudio() override;
    float takeAudioPeak() override;
    bool audioRunning() const override;

    // The Desktop Video driver (DeckLink COM server) is installed on this machine.
    static bool sdkAvailable();
    static std::vector<CaptureInputInfo> listInputs();

    // DeckLink capture thread.
    void onFormatChanged(uint32_t displayMode, bool rgb);
    void onFrameArrived(IDeckLinkVideoInputFrame* videoFrame, IDeckLinkAudioInputPacket* audioPacket);

private:
    enum class OpenResult { Opened, Waiting, Failed };

    OpenResult openCapture(std::string& message); // render thread
    void closeCapture();                         // render thread
    bool enableVideo(uint32_t displayMode, bool rgb, bool restartStreams);
    void applyFormatChange();
    void consumeFrame();
    bool gpuConversionReady();
    bool uploadDirectGpu(IDeckLinkVideoInputFrame* frame, int width, int height);
    bool uploadGpu(IDeckLinkVideoInputFrame* frame, int width, int height);
    bool uploadCpu(IDeckLinkVideoInputFrame* frame, int width, int height);
    void ensureTexture(int width, int height);
    void releasePendingFrame();

    bool m_comInitialized = false;
    IDeckLinkInput* m_input = nullptr;
    IDeckLinkInputCallback* m_callback = nullptr;
    IDeckLinkVideoConversion* m_converter = nullptr; // CPU fallback, created on demand
    bool m_formatDetection = false;
    bool m_audioEnabled = false;
    uint32_t m_displayMode = 0;
    bool m_rgb = false;
    std::atomic<bool> m_callbacksEnabled{false};
    std::chrono::steady_clock::time_point m_nextAttempt{};
    std::string m_lastLoggedError;

    // Written by the capture thread, applied on the render thread.
    std::mutex m_formatMutex;
    bool m_formatChanged = false;
    uint32_t m_pendingMode = 0;
    bool m_pendingRgb = false;

    // Newest frame (AddRef'd), handed from the capture thread to the render thread.
    std::mutex m_frameMutex;
    IDeckLinkVideoInputFrame* m_pendingFrame = nullptr;

    // Render thread.
    std::unique_ptr<NdiGpuConversion> m_gpuConversion;
    bool m_gpuConversionFailed = false;
    // Set up at open when GPU direct is wanted and available. Stays while the input captures
    // into its buffers, also after a failed transfer (m_directGpuFailed: system memory path).
    std::unique_ptr<BlackmagicDirectGpu> m_directGpu;
    bool m_directGpuFailed = false;
    unsigned int m_texture = 0;
    int m_texWidth = 0;
    int m_texHeight = 0;

    mutable std::mutex m_audioMutex; // m_audio is read by the capture thread
    std::unique_ptr<BlackmagicAudioCapture> m_audio; // only for sources with audio=true
};

#endif // BLACKMAGICCAPTURE_H
