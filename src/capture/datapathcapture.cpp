/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GPU direct transfer flow based on the Datapath RGBEasy SAMPLE5 (AMD DirectGMA / NVIDIA GPUDirect)
 * and the SGCT RGBEasyCaptureExample.
 */

#include "datapathcapture.h"
#include <sgct/opengl.h>
#include <sgct/sgct.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <rgb.h>
#include <rgbapi.h>
#include <rgberror.h>
#include <audio.h>
#include <audioapi.h>

#include <portaudio.h>
#include "audiosettings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>
#include <utility>

namespace {

constexpr auto kRetryInterval = std::chrono::seconds(5);
constexpr auto kSignalPollInterval = std::chrono::seconds(1);
constexpr GLuint64 kFenceTimeoutNs = 1'000'000'000;
// Audio stops when the layer stops asking for it (hidden, removed) for this long.
constexpr auto kAudioKeepAliveTimeout = std::chrono::milliseconds(750);
constexpr unsigned long kAudioFramesPerBuffer = 256;

std::mutex s_sdkMutex;
int s_sdkRefs = 0;
HRGBDLL s_hRgbDll = 0;

// RGBEasy.dll is delay-loaded: probe for it before the first SDK call, which would otherwise
// raise a structured exception on machines without the Datapath driver.
bool dllPresent() {
    static const bool present = LoadLibraryW(L"RGBEasy.dll") != nullptr;
    return present;
}

bool acquireSdk(unsigned long& error) {
    std::lock_guard<std::mutex> lock(s_sdkMutex);
    error = 0;
    if (!dllPresent())
        return false;
    if (s_sdkRefs == 0) {
        error = RGBLoad(&s_hRgbDll);
        if (error != 0)
            return false;
    }
    ++s_sdkRefs;
    return true;
}

void releaseSdk() {
    std::lock_guard<std::mutex> lock(s_sdkMutex);
    if (s_sdkRefs > 0 && --s_sdkRefs == 0) {
        RGBFree(s_hRgbDll);
        s_hRgbDll = 0;
    }
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string wideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide)
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1)
        return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr);
    return out;
}

bool parseGanging(const std::string& ganging, RGBGANG_TYPE& type) {
    static const std::pair<const char*, RGBGANG_TYPE> kTypes[] = {
        {"off", RGBGANG_TYPE_DISABLED}, {"disabled", RGBGANG_TYPE_DISABLED},
        {"2x1", RGBGANG_TYPE_2x1}, {"1x2", RGBGANG_TYPE_1x2}, {"2x2", RGBGANG_TYPE_2x2},
        {"3x1", RGBGANG_TYPE_3x1}, {"1x3", RGBGANG_TYPE_1x3},
        {"4x1", RGBGANG_TYPE_4x1}, {"1x4", RGBGANG_TYPE_1x4},
    };
    for (const auto& [name, value] : kTypes) {
        if (ganging == name) {
            type = value;
            return true;
        }
    }
    return false;
}

bool isValidSignal(SIGNALTYPE type, unsigned long width, unsigned long height) {
    return type != RGB_SIGNALTYPE_NOSIGNAL && type != RGB_SIGNALTYPE_OUTOFRANGE && width > 0 && height > 0;
}

void RGBCBKAPI frameCapturedFn(HWND, HRGB, PRGBFRAMEDATA frameData, ULONG_PTR userData) {
    if (frameData && userData)
        reinterpret_cast<DatapathCaptureBackend*>(userData)->onFrameCaptured(frameData->PBitmapBits, frameData->PBitmapInfo);
}

void RGBCBKAPI modeChangedFn(HWND, HRGB, PRGBMODECHANGEDINFO, ULONG_PTR userData) {
    if (userData)
        reinterpret_cast<DatapathCaptureBackend*>(userData)->onModeChanged();
}

void RGBCBKAPI noSignalFn(HWND, HRGB, ULONG_PTR userData) {
    if (userData)
        reinterpret_cast<DatapathCaptureBackend*>(userData)->onNoSignal();
}

int audioOutputCallback(const void*, void* output, unsigned long frames, const PaStreamCallbackTimeInfo*,
                        PaStreamCallbackFlags, void* userData) {
    if (output && userData)
        static_cast<DatapathAudioCapture*>(userData)->fillOutput(static_cast<float*>(output), frames);
    return paContinue;
}

// The output device from the audio settings (same choice as the DirectShow/NDI layers).
PaDeviceIndex chooseOutputDevice() {
    PaDeviceIndex chosen = Pa_GetDefaultOutputDevice();
    if (!AudioSettings::portAudioCustomOutput() || AudioSettings::portAudioOutputDevice().isEmpty()
        || AudioSettings::portAudioOutputApi().isEmpty())
        return chosen;

    const int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo* api = info ? Pa_GetHostApiInfo(info->hostApi) : nullptr;
        if (info && api && info->maxOutputChannels > 1
            && QString::fromUtf8(info->name) == AudioSettings::portAudioOutputDevice()
            && QString::fromUtf8(api->name) == AudioSettings::portAudioOutputApi())
            return i;
    }
    sgct::Log::Info("CaptureLayer: configured audio output device not found - using the default device");
    return chosen;
}

// Keeps the caller's (Qt / SGCT) pixel unpack state intact around our texture uploads.
struct UnpackStateGuard {
    GLint pbo = 0, texture = 0, alignment = 4, rowLength = 0;
    UnpackStateGuard() {
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // DIB rows are DWORD aligned
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    ~UnpackStateGuard() {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(pbo));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    }
};

void allocateTexture(GLuint tex, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

} // namespace

DatapathCaptureBackend::DatapathCaptureBackend(const CaptureSource& src)
    : CaptureBackend(src) {
}

DatapathCaptureBackend::~DatapathCaptureBackend() = default;

bool DatapathCaptureBackend::sdkAvailable() {
    return dllPresent();
}

std::vector<CaptureInputInfo> DatapathCaptureBackend::listInputs() {
    std::vector<CaptureInputInfo> inputs;
    unsigned long error = 0;
    if (!acquireSdk(error))
        return inputs;

    unsigned long count = 0;
    if (RGBGetNumberOfInputs(&count) == 0) {
        for (unsigned long i = 0; i < count; ++i) {
            CaptureInputInfo info;
            info.source.backend = "datapath";
            info.source.input = static_cast<int>(i) + 1;

            RGBINPUTINFOW inputInfo{};
            inputInfo.Size = sizeof(inputInfo);
            std::string device = RGBGetInputInfoW(i, &inputInfo) == 0 ? wideToUtf8(inputInfo.DeviceName) : std::string();

            SIGNALTYPE signal = RGB_SIGNALTYPE_NOSIGNAL;
            unsigned long w = 0, h = 0, rate = 0;
            std::string mode = "no signal";
            if (RGBGetInputSignalType(i, &signal, &w, &h, &rate) == 0 && isValidSignal(signal, w, h))
                mode = rate > 0 ? std::format("{}x{}@{}Hz", w, h, rate) : std::format("{}x{}", w, h);

            info.name = std::format("Datapath {}: {} ({})", info.source.input, device.empty() ? "Vision input" : device, mode);
            inputs.push_back(std::move(info));
        }
    }
    releaseSdk();
    return inputs;
}

void DatapathCaptureBackend::onFrameCaptured(void* bits, const void* bitmapInfoHeader) {
    if (!m_callbacksEnabled.load() || !bitmapInfoHeader)
        return;

    const Transfer transfer = m_transfer.load();
    if (transfer == Transfer::Cpu) {
        const auto* bih = static_cast<const BITMAPINFOHEADER*>(bitmapInfoHeader);
        const int width = static_cast<int>(bih->biWidth);
        const int height = std::abs(static_cast<int>(bih->biHeight));
        if (!bits || bih->biBitCount != 24 || width <= 0 || height <= 0)
            return;
        const size_t stride = (static_cast<size_t>(width) * 3 + 3) & ~static_cast<size_t>(3);
        const size_t size = stride * static_cast<size_t>(height);
        if (bih->biSizeImage != 0 && bih->biSizeImage < size)
            return;

        std::lock_guard<std::mutex> lock(m_cpuMutex);
        m_cpuFrame.resize(size);
        std::memcpy(m_cpuFrame.data(), bits, size);
        m_cpuWidth = width;
        m_cpuHeight = height;
        m_cpuTopDown = bih->biHeight < 0;
        m_cpuNewFrame = true;
        return;
    }

    if (transfer == Transfer::DirectGpuAmd || transfer == Transfer::DirectGpuNvidia) {
        for (int i = 0; i < kNumBuffers; ++i) {
            if (bits != bufferPointer(i))
                continue;
            std::lock_guard<std::mutex> lock(m_bufferMutex);
            // The renderer has not picked up the previous frame yet: hand it straight back to
            // the driver to keep the latency at one frame.
            if (m_pendingIndex >= 0)
                chainBuffer(m_pendingIndex);
            m_pendingIndex = i;
            break;
        }
    }
}

void DatapathCaptureBackend::onModeChanged() {
    m_modeChanged.store(true);
    m_hasSignal.store(true);
}

void DatapathCaptureBackend::onNoSignal() {
    m_hasSignal.store(false);
}

void* DatapathCaptureBackend::bufferPointer(int index) const {
    // RGBDirectGPUInit hands out 32-bit buffer identifiers, which the SDK compares as pointers.
    return m_dataBuffer ? reinterpret_cast<void*>(static_cast<uintptr_t>(m_dataBuffer[index])) : nullptr;
}

bool DatapathCaptureBackend::chainBuffer(int index) {
    return RGBChainOutputBufferEx(static_cast<HRGB>(m_hrgb), reinterpret_cast<LPBITMAPINFO>(m_bitmapInfos[index].bytes.data()),
                                  bufferPointer(index), RGB_BUFFERTYPE_DIRECTGMA) == 0;
}

void DatapathCaptureBackend::update() {
    const auto now = std::chrono::steady_clock::now();
    const auto report = [this](const std::string& msg, bool isError) {
        if (isError)
            setError(msg);
        if (msg != m_lastLoggedError) {
            m_lastLoggedError = msg;
            if (isError)
                sgct::Log::Warning(std::format("CaptureLayer: {}", msg));
            else
                sgct::Log::Info(std::format("CaptureLayer: {}", msg));
        }
    };

    if (!m_sdkLoaded) {
        if (now < m_nextAttempt)
            return;
        unsigned long error = 0;
        if (!acquireSdk(error)) {
            m_nextAttempt = now + kRetryInterval;
            report(dllPresent() ? std::format("Datapath RGBLoad failed (0x{:08x})", error)
                                : std::string("Datapath RGBEasy driver (RGBEasy.dll) is not installed on this machine"),
                   true);
            return;
        }
        m_sdkLoaded = true;
    }

    if (!m_gangingApplied) {
        applyGanging();
        m_gangingApplied = true;
    }

    const unsigned long input = static_cast<unsigned long>(m_source.input - 1);
    const Transfer transfer = m_transfer.load();

    // GPU direct buffers have a fixed size: restart when the source resolution changes.
    if (m_hrgb && m_modeChanged.exchange(false)
        && (transfer == Transfer::DirectGpuAmd || transfer == Transfer::DirectGpuNvidia)) {
        SIGNALTYPE signal = RGB_SIGNALTYPE_NOSIGNAL;
        unsigned long w = 0, h = 0, rate = 0;
        const unsigned long error = RGBGetInputSignalType(input, &signal, &w, &h, &rate);
        if (error == 0 && isValidSignal(signal, w, h) && static_cast<int>(w) == m_width && static_cast<int>(h) == m_height) {
            // Same resolution (e.g. the initial mode report) - keep running.
        } else {
            sgct::Log::Info(std::format("CaptureLayer: Datapath input {} changed mode - restarting capture", m_source.input));
            closeCapture();
            m_nextAttempt = now;
        }
    }

    if (!m_hrgb) {
        if (now < m_nextAttempt)
            return;
        m_nextAttempt = now + kSignalPollInterval;

        SIGNALTYPE signal = RGB_SIGNALTYPE_NOSIGNAL;
        unsigned long w = 0, h = 0, rate = 0;
        const unsigned long error = RGBGetInputSignalType(input, &signal, &w, &h, &rate);
        if (error != 0) {
            m_nextAttempt = now + kRetryInterval;
            report(std::format("Datapath input {} is not available (0x{:08x})", m_source.input, error), true);
            return;
        }
        if (!isValidSignal(signal, w, h)) {
            m_hasSignal.store(false);
            setError("");
            report(std::format("Datapath input {}: waiting for a signal", m_source.input), false);
            return;
        }
        if (!openCapture(w, h)) {
            m_nextAttempt = now + kRetryInterval;
            return;
        }
        setError("");
    }

    if (m_transfer.load() == Transfer::Cpu)
        consumeCpu();
    else
        consumeDirectGpu();
}

void DatapathCaptureBackend::applyGanging() {
    if (m_source.ganging.empty())
        return;

    RGBGANG_TYPE type = RGBGANG_TYPE_DISABLED;
    if (!parseGanging(toLower(m_source.ganging), type)) {
        sgct::Log::Warning(std::format("CaptureLayer: unknown ganging '{}' for Datapath input {}", m_source.ganging, m_source.input));
        return;
    }

    const unsigned long input = static_cast<unsigned long>(m_source.input - 1);
    signed long supported = 0;
    if (RGBInputIsGangingSupported(input, &supported) != 0 || !supported) {
        sgct::Log::Warning(std::format("CaptureLayer: Datapath input {} does not support ganging", m_source.input));
        return;
    }
    if (type != RGBGANG_TYPE_DISABLED
        && (RGBInputIsGangingTypeSupported(input, type, &supported) != 0 || !supported)) {
        sgct::Log::Warning(std::format("CaptureLayer: Datapath input {} does not support ganging {}", m_source.input, m_source.ganging));
        return;
    }

    RGBGANG_TYPE current = RGBGANG_TYPE_DISABLED;
    if (RGBInputGetGangingType(input, &current) == 0 && current == type)
        return;
    const unsigned long error = RGBInputSetGangingType(input, type);
    if (error != 0)
        sgct::Log::Warning(std::format("CaptureLayer: could not set ganging {} on Datapath input {} (0x{:08x})", m_source.ganging, m_source.input, error));
    else
        sgct::Log::Info(std::format("CaptureLayer: Datapath input {} ganging set to {}", m_source.input, m_source.ganging));
}

bool DatapathCaptureBackend::openCapture(unsigned long width, unsigned long height) {
    const unsigned long input = static_cast<unsigned long>(m_source.input - 1);
    HRGB hrgb = 0;
    unsigned long error = RGBOpenInput(input, &hrgb);
    if (error != 0) {
        const std::string msg = std::format("could not open Datapath input {} (0x{:08x})", m_source.input, error);
        setError(msg);
        sgct::Log::Warning("CaptureLayer: " + msg);
        return false;
    }
    m_hrgb = static_cast<uintptr_t>(hrgb);
    m_width = static_cast<int>(width);
    m_height = static_cast<int>(height);

    RGBSetFrameDropping(hrgb, 0); // maximise the capture rate
    signed long liveStream = 0;
    if (RGBInputIsLiveStreamSupported(input, &liveStream) == 0 && liveStream)
        RGBSetLiveStream(hrgb, LIVESTREAM_1);
    RGBSetModeChangedFn(hrgb, modeChangedFn, reinterpret_cast<ULONG_PTR>(this));
    RGBSetNoSignalFn(hrgb, noSignalFn, reinterpret_cast<ULONG_PTR>(this));

    m_callbacksEnabled.store(true);

    const bool directGpu = m_source.directGpu && setupDirectGpu();
    if (!directGpu) {
        RGBSetDMADirect(hrgb, FALSE);
        error = RGBSetPixelFormat(hrgb, RGB_PIXELFORMAT_RGB24);
        if (error == 0)
            error = RGBSetFrameCapturedFnEx(hrgb, frameCapturedFn, reinterpret_cast<ULONG_PTR>(this));
        if (error != 0) {
            sgct::Log::Warning(std::format("CaptureLayer: could not configure Datapath input {} (0x{:08x})", m_source.input, error));
            closeCapture();
            return false;
        }
        m_transfer.store(Transfer::Cpu);
    }

    error = RGBStartCapture(hrgb);
    if (error != 0) {
        const std::string msg = std::format("could not start capture on Datapath input {} (0x{:08x})", m_source.input, error);
        setError(msg);
        sgct::Log::Warning("CaptureLayer: " + msg);
        closeCapture();
        return false;
    }

    m_hasSignal.store(true);
    m_modeChanged.store(false);
    sgct::Log::Info(std::format("CaptureLayer: Datapath input {} capturing {}x{} via {}", m_source.input, width, height,
                                transferName(m_transfer.load())));
    return true;
}

bool DatapathCaptureBackend::setupDirectGpu() {
    const char* vendorStr = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* rendererStr = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const std::string vendor = toLower(vendorStr ? vendorStr : "");
    const std::string renderer = rendererStr ? rendererStr : "";

    const bool nvidia = vendor.find("nvidia") != std::string::npos;
    // Match "ATI" only as a whole word: NVIDIA reports "NVIDIA Corporation".
    const bool amd = !nvidia
                     && (vendor.rfind("ati ", 0) == 0 || vendor == "ati" || vendor.find("amd") != std::string::npos
                         || vendor.find("advanced micro") != std::string::npos);

    if (!isProfessionalGpuName(renderer)) {
        sgct::Log::Warning(std::format(
            "CaptureLayer: '{}' is not a professional GPU. GPU direct capture needs NVIDIA Quadro/RTX professional "
            "(GPUDirect for Video) or AMD Radeon Pro/FirePro (DirectGMA); otherwise frames are copied through system memory.",
            renderer));
    }
    if (!nvidia && !amd)
        return false;

    const LONG width = static_cast<LONG>(m_width);
    const LONG height = static_cast<LONG>(m_height);
    const DWORD stride = (static_cast<DWORD>(width) * 3 + 3) & ~3u;
    for (auto& info : m_bitmapInfos) {
        info.bytes.fill(0);
        auto* bih = reinterpret_cast<BITMAPINFOHEADER*>(info.bytes.data());
        bih->biSize = sizeof(BITMAPINFOHEADER);
        bih->biWidth = width;
        bih->biHeight = -height; // top-down
        bih->biPlanes = 1;
        bih->biBitCount = 24;
        bih->biCompression = BI_RGB;
        bih->biSizeImage = stride * static_cast<DWORD>(height);
        bih->biXPelsPerMeter = 3000;
        bih->biYPelsPerMeter = 3000;
        auto* masks = reinterpret_cast<DWORD*>(info.bytes.data() + sizeof(BITMAPINFOHEADER));
        masks[0] = 0x00ff0000;
        masks[1] = 0x0000ff00;
        masks[2] = 0x000000ff;
    }

    {
        UnpackStateGuard guard;
        glGenTextures(kNumBuffers, m_textures.data());
        for (GLuint tex : m_textures)
            allocateTexture(tex, width, height);
        if (amd)
            glGenBuffers(kNumBuffers, m_pbos.data());
    }

    GPUTRANSFERDESCRIPTOR desc{};
    desc.Size = sizeof(desc);
    desc.Buffer = &m_dataBuffer;
    desc.Width = static_cast<unsigned long>(width);
    desc.Height = static_cast<unsigned long>(height);
    desc.OglByteFormat = GL_UNSIGNED_BYTE;
    desc.OglColourFormat = GL_BGR;
    desc.FormatSize = 3;
    desc.NumBuffers = kNumBuffers;
    desc.BufferSize = 0;
    // AMD DirectGMA DMAs into the pixel buffer objects, NVIDIA GPUDirect directly into the textures.
    desc.GpuBrand = amd ? GPU_AMD : GPU_NVIDIA;
    desc.OglObject = amd ? m_pbos.data() : m_textures.data();

    const HRGB hrgb = static_cast<HRGB>(m_hrgb);
    unsigned long error = RGBDirectGPUInit(hrgb, &desc);
    if (error != 0) {
        sgct::Log::Warning(std::format("CaptureLayer: {} init failed on '{}' (0x{:08x}) - falling back to CPU copy",
                                       amd ? "AMD DirectGMA" : "NVIDIA GPUDirect for Video", renderer, error));
        releaseDirectGpuGL();
        return false;
    }

    // The driver may pad the pitch; it reports the real buffer size.
    for (auto& info : m_bitmapInfos)
        reinterpret_cast<BITMAPINFOHEADER*>(info.bytes.data())->biSizeImage = desc.BufferSize;

    m_transfer.store(amd ? Transfer::DirectGpuAmd : Transfer::DirectGpuNvidia);
    error = RGBSetPixelFormat(hrgb, RGB_PIXELFORMAT_RGB24);
    if (error == 0)
        error = RGBSetFrameCapturedFnEx(hrgb, frameCapturedFn, reinterpret_cast<ULONG_PTR>(this));
    if (error == 0)
        error = RGBSetOutputSize(hrgb, desc.Width, desc.Height);
    for (int i = 0; error == 0 && i < kNumBuffers; ++i) {
        if (!chainBuffer(i))
            error = 1;
    }
    if (error == 0)
        error = RGBUseOutputBuffers(hrgb, TRUE);

    if (error != 0) {
        sgct::Log::Warning(std::format("CaptureLayer: could not set up GPU direct buffers for Datapath input {} - falling back to CPU copy",
                                       m_source.input));
        RGBSetFrameCapturedFnEx(hrgb, nullptr, 0);
        RGBUseOutputBuffers(hrgb, FALSE);
        RGBDirectGPUClose(hrgb);
        releaseDirectGpuGL();
        m_transfer.store(Transfer::None);
        return false;
    }

    m_flipY = true;
    return true;
}

void DatapathCaptureBackend::releaseDirectGpuGL() {
    if (m_textures[0] != 0)
        glDeleteTextures(kNumBuffers, m_textures.data());
    if (m_pbos[0] != 0)
        glDeleteBuffers(kNumBuffers, m_pbos.data());
    m_textures.fill(0);
    m_pbos.fill(0);
    m_dataBuffer = nullptr;
}

void DatapathCaptureBackend::consumeDirectGpu() {
    releaseRetired(false);

    int index = -1;
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        index = m_pendingIndex;
        m_pendingIndex = -1;
    }
    if (index < 0)
        return;

    const HRGB hrgb = static_cast<HRGB>(m_hrgb);
    if (m_transfer.load() == Transfer::DirectGpuNvidia) {
        RGBDirectGPUNVIDIAOp(hrgb, static_cast<unsigned int>(index), NVIDIA_GPU_COPY);
        RGBDirectGPUNVIDIAOp(hrgb, static_cast<unsigned int>(index), NVIDIA_GPU_WAIT);
    } else {
        UnpackStateGuard guard;
        glBindTexture(GL_TEXTURE_2D, m_textures[index]);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbos[index]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_BGR, GL_UNSIGNED_BYTE, nullptr);
    }

    // The previously shown buffer goes back to the driver once the GPU is done reading it.
    const int previous = m_displayedIndex;
    m_displayedIndex = index;
    m_textureId = m_textures[index];
    if (previous >= 0)
        m_retired.push_back({previous, glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0)});
}

void DatapathCaptureBackend::releaseRetired(bool wait) {
    const bool nvidia = m_transfer.load() == Transfer::DirectGpuNvidia;
    for (auto it = m_retired.begin(); it != m_retired.end();) {
        GLsync fence = static_cast<GLsync>(it->fence);
        const GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, wait ? kFenceTimeoutNs : 0);
        if (!wait && result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
            ++it;
            continue;
        }
        glDeleteSync(fence);
        if (nvidia)
            RGBDirectGPUNVIDIAOp(static_cast<HRGB>(m_hrgb), static_cast<unsigned int>(it->index), NVIDIA_GPU_END);
        if (!wait)
            chainBuffer(it->index);
        it = m_retired.erase(it);
    }
}

void DatapathCaptureBackend::consumeCpu() {
    int width = 0;
    int height = 0;
    bool topDown = true;
    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);
        if (!m_cpuNewFrame)
            return;
        m_uploadFrame.swap(m_cpuFrame);
        width = m_cpuWidth;
        height = m_cpuHeight;
        topDown = m_cpuTopDown;
        m_cpuNewFrame = false;
    }

    UnpackStateGuard guard;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (m_cpuTexture == 0 || width != m_cpuTexWidth || height != m_cpuTexHeight) {
        if (m_cpuTexture == 0)
            glGenTextures(1, &m_cpuTexture);
        allocateTexture(m_cpuTexture, width, height);
        m_cpuTexWidth = width;
        m_cpuTexHeight = height;
    }
    glBindTexture(GL_TEXTURE_2D, m_cpuTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, m_uploadFrame.data());

    m_textureId = m_cpuTexture;
    m_width = width;
    m_height = height;
    m_flipY = topDown;
}

void DatapathCaptureBackend::closeCapture() {
    m_callbacksEnabled.store(false);
    const Transfer transfer = m_transfer.load();
    const bool directGpu = transfer == Transfer::DirectGpuAmd || transfer == Transfer::DirectGpuNvidia;

    if (m_hrgb) {
        const HRGB hrgb = static_cast<HRGB>(m_hrgb);
        RGBStopCapture(hrgb);
        RGBSetFrameCapturedFnEx(hrgb, nullptr, 0);
        RGBSetModeChangedFn(hrgb, nullptr, 0);
        RGBSetNoSignalFn(hrgb, nullptr, 0);
        if (directGpu) {
            RGBUseOutputBuffers(hrgb, FALSE);
            releaseRetired(true);
            if (transfer == Transfer::DirectGpuNvidia && m_displayedIndex >= 0) {
                glFinish();
                RGBDirectGPUNVIDIAOp(hrgb, static_cast<unsigned int>(m_displayedIndex), NVIDIA_GPU_END);
            }
            RGBDirectGPUClose(hrgb);
        }
        RGBCloseInput(hrgb);
        m_hrgb = 0;
    }

    releaseDirectGpuGL();
    if (m_cpuTexture != 0) {
        glDeleteTextures(1, &m_cpuTexture);
        m_cpuTexture = 0;
        m_cpuTexWidth = 0;
        m_cpuTexHeight = 0;
    }
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_pendingIndex = -1;
    }
    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);
        m_cpuNewFrame = false;
    }
    m_displayedIndex = -1;
    m_textureId = 0;
    m_width = 0;
    m_height = 0;
    m_transfer.store(Transfer::None);
}

void DatapathCaptureBackend::close() {
    m_audio.reset();
    closeCapture();
    if (m_sdkLoaded) {
        releaseSdk();
        m_sdkLoaded = false;
    }
}

void DatapathCaptureBackend::updateAudio(bool wanted, float volume, bool reportLevels) {
    if (!m_source.audio || !wanted)
        return;
    if (!m_audio)
        m_audio = std::make_unique<DatapathAudioCapture>(m_source.input);
    m_audio->keepAlive(volume, reportLevels);
}

void DatapathCaptureBackend::restartAudio() {
    if (m_audio)
        m_audio->restart();
}

float DatapathCaptureBackend::takeAudioPeak() {
    return m_audio ? m_audio->takePeak() : -1.f;
}

bool DatapathCaptureBackend::audioRunning() const {
    return m_audio && m_audio->running();
}

// ---------------------------------------------------------------------------
// Audio
// ---------------------------------------------------------------------------

DatapathAudioCapture::DatapathAudioCapture(int input)
    : m_input(input) {
    m_lastKeepAlive = std::chrono::steady_clock::now();
    m_thread = std::thread([this] { run(); });
}

DatapathAudioCapture::~DatapathAudioCapture() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_terminate = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void DatapathAudioCapture::keepAlive(float volume, bool reportLevels) {
    m_volume.store(std::clamp(volume, 0.f, 1.f));
    m_reportLevels.store(reportLevels);
    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastKeepAlive = std::chrono::steady_clock::now();
        wake = !m_running.load();
    }
    if (wake)
        m_cv.notify_all();
}

void DatapathAudioCapture::restart() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_restart = true;
    }
    m_cv.notify_all();
}

float DatapathAudioCapture::takePeak() {
    return m_peak.exchange(-1.f);
}

void DatapathAudioCapture::run() {
    auto nextAttempt = std::chrono::steady_clock::now();
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_terminate) {
        m_cv.wait_for(lock, std::chrono::milliseconds(200));
        if (m_terminate)
            break;

        const auto now = std::chrono::steady_clock::now();
        const bool wanted = now - m_lastKeepAlive < kAudioKeepAliveTimeout;
        const bool restart = std::exchange(m_restart, false);

        lock.unlock();
        if (m_running.load() && (!wanted || restart))
            close();
        if (wanted && !m_running.load() && (restart || now >= nextAttempt)) {
            if (!open())
                nextAttempt = now + kRetryInterval;
        }
        lock.lock();
    }
    lock.unlock();

    close();
    if (m_paInitialized)
        Pa_Terminate();
    if (m_sdkLoaded)
        releaseSdk();
}

bool DatapathAudioCapture::open() {
    const auto fail = [this](const std::string& msg) {
        if (msg != m_lastLoggedError) {
            m_lastLoggedError = msg;
            sgct::Log::Warning("CaptureLayer: " + msg);
        }
        close();
        return false;
    };

    if (!m_sdkLoaded) {
        unsigned long error = 0;
        if (!acquireSdk(error))
            return fail("Datapath audio: RGBEasy driver not available");
        m_sdkLoaded = true;
    }

    const unsigned long input = static_cast<unsigned long>(m_input - 1);
    signed long supported = 0;
    if (RGBAudioIsAudioSupported(input, &supported) != 0 || !supported)
        return fail(std::format("Datapath input {} does not support audio capture", m_input));

    HAUDIO hAudio = 0;
    unsigned long error = RGBAudioOpenInput(nullptr, 0, input, &hAudio);
    if (error != 0)
        return fail(std::format("could not open audio on Datapath input {} (0x{:08x})", m_input, error));
    m_hAudio = static_cast<uintptr_t>(hAudio);

    // Prefer 48 kHz 16-bit stereo; otherwise the first stereo 16/24-bit format.
    unsigned long count = 0;
    RGBAudioGetCapabilitiesCount(input, &count);
    int bestIndex = -1;
    int bestScore = -1;
    AUDIOCAPS best{};
    for (unsigned long i = 0; i < count; ++i) {
        AUDIOCAPS caps{};
        caps.Size = sizeof(caps);
        if (RGBAudioGetCapabilities(input, i, &caps) != 0 || caps.Channels != AUDIOCAPTURECHANNELS_STEREO
            || (caps.SampleDepth != AUDIOCAPTURESAMPLEDEPTH_16BPS && caps.SampleDepth != AUDIOCAPTURESAMPLEDEPTH_24PS))
            continue;
        const int score = (caps.SamplesPerSec == AUDIOCAPTURESAMPLESPERSEC_48000 ? 2 : 0)
                          + (caps.SampleDepth == AUDIOCAPTURESAMPLEDEPTH_16BPS ? 1 : 0);
        if (score > bestScore) {
            bestScore = score;
            bestIndex = static_cast<int>(i);
            best = caps;
        }
    }
    if (bestIndex < 0)
        return fail(std::format("Datapath input {} offers no usable audio format", m_input));

    error = RGBAudioSetCapabilities(input, static_cast<unsigned long>(bestIndex));
    if (error == 0)
        error = RGBAudioSetState(hAudio, ACQUIRE);
    if (error != 0)
        return fail(std::format("could not start audio on Datapath input {} (0x{:08x})", m_input, error));

    if (!m_paInitialized) {
        const PaError paErr = Pa_Initialize();
        if (paErr != paNoError)
            return fail(std::format("PortAudio initialization failed ({})", Pa_GetErrorText(paErr)));
        m_paInitialized = true;
    }

    PaStreamParameters params{};
    params.device = chooseOutputDevice();
    const PaDeviceInfo* device = params.device != paNoDevice ? Pa_GetDeviceInfo(params.device) : nullptr;
    if (!device)
        return fail("no audio output device for capture audio");
    m_outChannels = 2;
    if (AudioSettings::portAudioMixInputToOutput())
        m_outChannels = std::max(1, std::min(AudioSettings::portAudioOutputChannels(), device->maxOutputChannels));
    params.channelCount = m_outChannels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = device->defaultLowOutputLatency;

    m_sampleBytes = best.SampleDepth / 8;
    m_pcm.assign(static_cast<size_t>(kAudioFramesPerBuffer) * 2 * m_sampleBytes, 0);

    PaStream* stream = nullptr;
    PaError paErr = Pa_OpenStream(&stream, nullptr, &params, static_cast<double>(best.SamplesPerSec), kAudioFramesPerBuffer,
                                  paClipOff, audioOutputCallback, this);
    if (paErr == paNoError) {
        m_stream = stream;
        paErr = Pa_StartStream(stream);
    }
    if (paErr != paNoError)
        return fail(std::format("could not open the audio output for Datapath input {} ({})", m_input, Pa_GetErrorText(paErr)));

    m_running.store(true);
    m_lastLoggedError.clear();
    sgct::Log::Info(std::format("CaptureLayer: Datapath input {} audio {} Hz {}-bit stereo -> '{}' ({} ch)", m_input,
                                static_cast<int>(best.SamplesPerSec), static_cast<int>(best.SampleDepth), device->name, m_outChannels));
    return true;
}

void DatapathAudioCapture::close() {
    if (m_stream) {
        PaStream* stream = static_cast<PaStream*>(m_stream);
        if (Pa_StopStream(stream) != paNoError)
            Pa_AbortStream(stream);
        Pa_CloseStream(stream);
        m_stream = nullptr;
    }
    m_running.store(false);
    if (m_hAudio) {
        const HAUDIO hAudio = static_cast<HAUDIO>(m_hAudio);
        RGBAudioSetState(hAudio, STOP);
        RGBAudioReleaseOutputBuffers(hAudio);
        RGBAudioCloseInput(hAudio);
        m_hAudio = 0;
    }
}

void DatapathAudioCapture::fillOutput(float* out, unsigned long frames) {
    const size_t bytes = static_cast<size_t>(frames) * 2 * m_sampleBytes;
    unsigned long written = 0;
    // The driver zero-pads when it has less data than requested.
    if (bytes > m_pcm.size()
        || RGBAudioLoadOutputBuffer(static_cast<HAUDIO>(m_hAudio), static_cast<unsigned long>(bytes), 0, &written, m_pcm.data()) != 0) {
        std::fill_n(out, static_cast<size_t>(frames) * m_outChannels, 0.f);
        return;
    }

    const float volume = m_volume.load();
    const uint8_t* pcm = m_pcm.data();
    float peak = 0.f;
    for (unsigned long f = 0; f < frames; ++f) {
        float stereo[2];
        for (int c = 0; c < 2; ++c) {
            const uint8_t* s = pcm + (static_cast<size_t>(f) * 2 + c) * m_sampleBytes;
            if (m_sampleBytes == 2) {
                stereo[c] = static_cast<float>(static_cast<int16_t>(s[0] | (s[1] << 8))) / 32768.f;
            } else {
                int32_t v = s[0] | (s[1] << 8) | (s[2] << 16);
                if (v & 0x800000)
                    v |= ~0xFFFFFF;
                stereo[c] = static_cast<float>(v) / 8388608.f;
            }
            stereo[c] *= volume;
            peak = std::max(peak, std::fabs(stereo[c]));
        }
        float* frameOut = out + static_cast<size_t>(f) * m_outChannels;
        for (int oc = 0; oc < m_outChannels; ++oc)
            frameOut[oc] = stereo[oc % 2];
    }

    if (m_reportLevels.load()) {
        float previous = m_peak.load();
        while (peak > previous && !m_peak.compare_exchange_weak(previous, peak)) {
        }
    }
}

void DatapathCaptureBackend::stopCallbacks() {
    m_callbacksEnabled.store(false);
    if (m_hrgb)
        RGBStopCapture(static_cast<HRGB>(m_hrgb));
}
