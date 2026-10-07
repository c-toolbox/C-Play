/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DATAPATHCAPTURE_H
#define DATAPATHCAPTURE_H

#include <capture/capturebackend.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

// Audio of one Datapath input (RGBAudio API), played through PortAudio on the output device
// chosen in the audio settings. The PortAudio callback pulls PCM straight from the driver's
// cyclic buffer (RGBAudioLoadOutputBuffer, zero-padded when short), so the output clock paces
// the capture. Opening/closing runs on an own worker thread to keep device I/O off the render thread.
class DatapathAudioCapture {
public:
    explicit DatapathAudioCapture(int input);
    ~DatapathAudioCapture();

    void keepAlive(float volume, bool reportLevels); // render thread, every frame while wanted
    void restart();
    float takePeak();
    bool running() const { return m_running.load(); }

    // PortAudio callback thread.
    void fillOutput(float* out, unsigned long frames);

private:
    void run();
    bool open();
    void close();

    const int m_input; // 1-based
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_terminate = false;
    bool m_restart = false;
    std::chrono::steady_clock::time_point m_lastKeepAlive{};

    bool m_sdkLoaded = false;
    bool m_paInitialized = false;
    uintptr_t m_hAudio = 0;
    void* m_stream = nullptr; // PaStream
    int m_sampleBytes = 2;    // 2 = 16-bit, 3 = 24-bit packed
    int m_outChannels = 2;
    std::vector<uint8_t> m_pcm; // callback scratch buffer
    std::string m_lastLoggedError;

    std::atomic<bool> m_running{false};
    std::atomic<float> m_volume{1.f};
    std::atomic<bool> m_reportLevels{false};
    std::atomic<float> m_peak{-1.f};
};

// Datapath Vision capture through the RGBEasy SDK (RGBEasy.dll, installed with the Datapath driver).
//
// With a professional GPU the card DMAs frames straight into GPU memory, bypassing system memory:
//   AMD    (DirectGMA)           - into bus addressable pixel buffer objects, copied to a texture on the GPU.
//   NVIDIA (GPUDirect for Video) - into the textures themselves, framed by COPY/WAIT/END operations.
// When neither is available (consumer GPU, Intel, or directGpu disabled) frames are delivered in
// system memory and uploaded with glTexSubImage2D.
//
// Frames are RGB24 (BGR byte order, top-down rows).
class DatapathCaptureBackend : public CaptureBackend {
public:
    explicit DatapathCaptureBackend(const CaptureSource& src);
    ~DatapathCaptureBackend() override;

    void update() override;
    void close() override;
    void stopCallbacks() override;

    void updateAudio(bool wanted, float volume, bool reportLevels) override;
    void restartAudio() override;
    float takeAudioPeak() override;
    bool audioRunning() const override;

    // RGBEasy.dll is present on this machine (the Datapath driver is installed).
    static bool sdkAvailable();
    static std::vector<CaptureInputInfo> listInputs();

    // RGBEasy callback threads.
    void onFrameCaptured(void* bits, const void* bitmapInfoHeader);
    void onModeChanged();
    void onNoSignal();

private:
    static constexpr int kNumBuffers = 4;

    // Device-independent bitmap header for one chained buffer (BITMAPINFOHEADER + 3 colour masks).
    struct BitmapInfo {
        std::array<uint8_t, 40 + 3 * 4> bytes{};
    };

    struct RetiredBuffer {
        int index = -1;
        void* fence = nullptr; // GLsync
    };

    bool openCapture(unsigned long width, unsigned long height); // render thread
    void closeCapture();                                         // render thread
    void applyGanging();
    bool setupDirectGpu();
    void releaseDirectGpuGL();
    bool chainBuffer(int index);
    void consumeDirectGpu();
    void consumeCpu();
    void releaseRetired(bool wait);
    void* bufferPointer(int index) const;

    bool m_sdkLoaded = false;
    bool m_gangingApplied = false;
    uintptr_t m_hrgb = 0;
    std::atomic<bool> m_callbacksEnabled{false};
    std::atomic<bool> m_modeChanged{false};
    std::chrono::steady_clock::time_point m_nextAttempt{};
    std::string m_lastLoggedError;

    // GPU direct state (render thread, except m_pendingIndex which the callback writes under m_bufferMutex).
    std::array<unsigned int, kNumBuffers> m_textures{};
    std::array<unsigned int, kNumBuffers> m_pbos{};
    std::array<BitmapInfo, kNumBuffers> m_bitmapInfos{};
    unsigned int* m_dataBuffer = nullptr; // filled by RGBDirectGPUInit
    std::mutex m_bufferMutex;
    int m_pendingIndex = -1;
    int m_displayedIndex = -1;
    std::vector<RetiredBuffer> m_retired;

    // CPU fallback: the callback copies the frame here, the render thread uploads it.
    std::mutex m_cpuMutex;
    std::vector<uint8_t> m_cpuFrame;
    std::vector<uint8_t> m_uploadFrame;
    int m_cpuWidth = 0;
    int m_cpuHeight = 0;
    bool m_cpuTopDown = true;
    bool m_cpuNewFrame = false;
    unsigned int m_cpuTexture = 0;
    int m_cpuTexWidth = 0;
    int m_cpuTexHeight = 0;

    std::unique_ptr<DatapathAudioCapture> m_audio; // only for sources with audio=true
};

#endif // DATAPATHCAPTURE_H
