/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CAPTUREBACKEND_H
#define CAPTUREBACKEND_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// One capture card input, stored as the Capture layer's file path:
//   "<backend>:<input>[?ganging=<CxR>&directgpu=<0|1>&audio=<0|1>]"     e.g. "datapath:1?ganging=2x1&audio=1"
// The input number is 1-based, as in the vendor tools. input <= 0 means "no capture on this machine".
struct CaptureSource {
    std::string backend = "datapath";
    int input = 0;
    std::string ganging;    // "" (leave the card as configured), "2x1", "1x2", "2x2", "3x1", "1x3", "4x1", "1x4"
    bool directGpu = true;  // try DMA straight into GPU memory (requires a professional GPU)
    bool audio = false;     // also capture the input's audio and play it on this machine

    bool valid() const { return !backend.empty() && input > 0; }
    std::string toString() const;
    static CaptureSource fromString(const std::string& str);
};

struct CaptureInputInfo {
    CaptureSource source;
    std::string name; // human readable, e.g. "Datapath 1: VisionSC-DP2"
};

// Graphics adapter as seen by DXGI (no OpenGL context needed).
struct CaptureGpuInfo {
    std::string name;
    unsigned int vendorId = 0;
    bool professional = false;
};

// GPU direct transfer (AMD DirectGMA / NVIDIA GPUDirect for Video) is only offered on the
// professional product lines. Name based heuristic, also applied to GL_RENDERER strings.
bool isProfessionalGpuName(const std::string& name);
// The professional adapter when one is installed, otherwise the first hardware adapter.
CaptureGpuInfo detectCaptureGpu();

// A running capture of one input, owned through std::shared_ptr and shared by every
// CaptureLayer that shows the same source in the same OpenGL context.
// update()/close() must run on the render thread with that context current.
class CaptureBackend {
public:
    enum class Transfer { None, Cpu, DirectGpuAmd, DirectGpuNvidia };

    virtual ~CaptureBackend() = default;

    // Opens the capture when needed and presents the newest frame. Cheap when called
    // several times per frame (once per sharing layer).
    virtual void update() = 0;
    // Stops the capture and releases all SDK and OpenGL resources.
    virtual void close() = 0;
    // Any thread: stops frame delivery so close() can run later on the render thread.
    virtual void stopCallbacks() = 0;

    // Audio of sources with audio=true. Call every frame while the audio should play; it stops
    // shortly after the calls stop (layer hidden or removed). Volume is linear, 0..1.
    virtual void updateAudio(bool wanted, float volume, bool reportLevels) { (void)wanted; (void)volume; (void)reportLevels; }
    // Reopens the audio output, e.g. after the output device setting changed.
    virtual void restartAudio() {}
    // Peak level since the previous call in [0..1], or -1 when no audio was played meanwhile.
    virtual float takeAudioPeak() { return -1.f; }
    virtual bool audioRunning() const { return false; }

    const CaptureSource& source() const { return m_source; }
    unsigned int textureId() const { return m_textureId; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool flipY() const { return m_flipY; }
    Transfer transfer() const { return m_transfer.load(); }
    bool hasSignal() const { return m_hasSignal.load(); }
    // Non-empty when the source cannot be captured on this machine (no SDK, invalid input).
    std::string error() const;

    static const char* transferName(Transfer t);

    // Render thread: returns the shared capture of src for the current OpenGL context.
    static std::shared_ptr<CaptureBackend> acquire(const CaptureSource& src);
    // Render thread: closes captures released on other threads that belong to the current context.
    static void processPendingClose();
    // All inputs of all compiled-in backends whose SDK is present on this machine.
    static std::vector<CaptureInputInfo> listInputs();
    // Names of the compiled-in backends whose SDK runtime is present on this machine.
    static std::vector<std::string> availableSdks();

protected:
    explicit CaptureBackend(const CaptureSource& src);
    void setError(const std::string& err);

    CaptureSource m_source;
    void* m_glContext = nullptr; // HGLRC the GL resources live in
    unsigned int m_textureId = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_flipY = true;
    std::atomic<Transfer> m_transfer{Transfer::None};
    std::atomic<bool> m_hasSignal{false};

private:
    mutable std::mutex m_errorMutex;
    std::string m_error;
};

#endif // CAPTUREBACKEND_H
