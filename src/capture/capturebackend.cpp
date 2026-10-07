/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "capturebackend.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi.h>

#include <sgct/sgct.h>
#include <algorithm>
#include <cctype>
#include <format>
#include <map>
#include <regex>
#include <sstream>

#ifdef CAPTURE_DATAPATH
#include <capture/datapathcapture.h>
#endif

namespace {

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

// Shared captures keyed by "<source>@<GL context>".
std::mutex s_registryMutex;
std::map<std::string, std::weak_ptr<CaptureBackend>> s_registry;
// Captures released on a thread without their GL context current; closed by processPendingClose().
std::vector<CaptureBackend*> s_pendingClose;

CaptureBackend* createBackend(const CaptureSource& src) {
#ifdef CAPTURE_DATAPATH
    if (src.backend == "datapath")
        return new DatapathCaptureBackend(src);
#endif
    (void)src;
    return nullptr;
}

} // namespace

std::string CaptureSource::toString() const {
    std::string s = std::format("{}:{}", backend, input);
    std::string query;
    if (!ganging.empty())
        query += "ganging=" + ganging;
    if (!directGpu)
        query += std::string(query.empty() ? "" : "&") + "directgpu=0";
    if (audio)
        query += std::string(query.empty() ? "" : "&") + "audio=1";
    if (!query.empty())
        s += "?" + query;
    return s;
}

CaptureSource CaptureSource::fromString(const std::string& str) {
    CaptureSource src;
    const size_t colon = str.find(':');
    if (colon == std::string::npos)
        return CaptureSource{"", 0, "", true};

    src.backend = toLower(str.substr(0, colon));
    const size_t q = str.find('?', colon + 1);
    try {
        src.input = std::stoi(str.substr(colon + 1, q == std::string::npos ? std::string::npos : q - colon - 1));
    } catch (...) {
        src.input = 0;
    }

    if (q != std::string::npos) {
        std::stringstream ss(str.substr(q + 1));
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            const size_t eq = pair.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string k = toLower(pair.substr(0, eq));
            const std::string v = pair.substr(eq + 1);
            if (k == "ganging")
                src.ganging = toLower(v);
            else if (k == "directgpu")
                src.directGpu = !(v == "0" || toLower(v) == "false");
            else if (k == "audio")
                src.audio = v == "1" || toLower(v) == "true";
        }
    }
    return src;
}

bool isProfessionalGpuName(const std::string& name) {
    const std::string n = toLower(name);
    if (n.find("nvs") != std::string::npos)
        return false; // NVS display cards lack GPUDirect for Video
    static const std::regex pro(
        R"(quadro|rtx\s*a\d|ada generation|blackwell|rtx\s*pro|tesla|nvidia\s+a\d|nvidia\s+l\d|grid|firepro|radeon(\(tm\))?\s*pro)",
        std::regex::icase);
    return std::regex_search(n, pro);
}

CaptureGpuInfo detectCaptureGpu() {
    CaptureGpuInfo result;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) || !factory)
        return result;

    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            CaptureGpuInfo info;
            info.name = wideToUtf8(desc.Description);
            info.vendorId = desc.VendorId;
            info.professional = isProfessionalGpuName(info.name);
            if (result.name.empty() || (info.professional && !result.professional))
                result = info;
        }
        adapter->Release();
        if (result.professional)
            break;
    }
    factory->Release();
    return result;
}

CaptureBackend::CaptureBackend(const CaptureSource& src)
    : m_source(src) {
    m_glContext = wglGetCurrentContext();
}

std::string CaptureBackend::error() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_error;
}

void CaptureBackend::setError(const std::string& err) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_error = err;
}

const char* CaptureBackend::transferName(Transfer t) {
    switch (t) {
    case Transfer::Cpu:
        return "CPU copy";
    case Transfer::DirectGpuAmd:
        return "AMD DirectGMA";
    case Transfer::DirectGpuNvidia:
        return "NVIDIA GPUDirect for Video";
    default:
        return "none";
    }
}

std::shared_ptr<CaptureBackend> CaptureBackend::acquire(const CaptureSource& src) {
    if (!src.valid())
        return nullptr;

    // A capture of the same input that is still waiting to be closed would block re-opening it.
    processPendingClose();

    const HGLRC ctx = wglGetCurrentContext();
    const std::string key = std::format("{}@{}", src.toString(), static_cast<const void*>(ctx));

    std::lock_guard<std::mutex> lock(s_registryMutex);
    for (auto it = s_registry.begin(); it != s_registry.end();) {
        if (it->second.expired())
            it = s_registry.erase(it);
        else
            ++it;
    }
    if (auto existing = s_registry[key].lock())
        return existing;

    CaptureBackend* raw = createBackend(src);
    if (!raw) {
        sgct::Log::Error(std::format("CaptureLayer: backend '{}' is not available in this build", src.backend));
        return nullptr;
    }

    std::shared_ptr<CaptureBackend> backend(raw, [](CaptureBackend* b) {
        if (wglGetCurrentContext() == static_cast<HGLRC>(b->m_glContext)) {
            b->close();
            delete b;
        } else {
            b->stopCallbacks();
            std::lock_guard<std::mutex> pendingLock(s_registryMutex);
            s_pendingClose.push_back(b);
        }
    });
    s_registry[key] = backend;
    return backend;
}

void CaptureBackend::processPendingClose() {
    const HGLRC ctx = wglGetCurrentContext();
    if (!ctx)
        return;

    std::vector<CaptureBackend*> toClose;
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        if (s_pendingClose.empty())
            return;
        for (auto it = s_pendingClose.begin(); it != s_pendingClose.end();) {
            if (static_cast<HGLRC>((*it)->m_glContext) == ctx) {
                toClose.push_back(*it);
                it = s_pendingClose.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (CaptureBackend* b : toClose) {
        b->close();
        delete b;
    }
}

std::vector<CaptureInputInfo> CaptureBackend::listInputs() {
    std::vector<CaptureInputInfo> inputs;
#ifdef CAPTURE_DATAPATH
    const auto datapath = DatapathCaptureBackend::listInputs();
    inputs.insert(inputs.end(), datapath.begin(), datapath.end());
#endif
    return inputs;
}

std::vector<std::string> CaptureBackend::availableSdks() {
    std::vector<std::string> sdks;
#ifdef CAPTURE_DATAPATH
    if (DatapathCaptureBackend::sdkAvailable())
        sdks.push_back("Datapath RGBEasy");
#endif
    return sdks;
}
