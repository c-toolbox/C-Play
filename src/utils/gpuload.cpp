/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "gpuload.h"

#ifdef Q_OS_WIN

#include <Windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwctype>
#include <iterator>
#include <map>
#include <string>
#include <vector>

// The "GPU Engine" counter instances are named e.g.
//   pid_12345_luid_0x00000000_0x0000B5A3_phys_0_eng_0_engtype_3D
// so filtering the enumerated instances for this process's PID yields exactly the
// engines this process uses. Task Manager reports the busiest of those engines as the
// process's GPU %; the engines run in parallel, so their loads are never summed.
GpuLoadProbe::GpuLoadProbe() = default;

GpuLoadProbe::~GpuLoadProbe() {
    if (m_query) {
        PdhCloseQuery(static_cast<PDH_HQUERY>(m_query));
    }
}

GpuLoadSample GpuLoadProbe::sample() {
    if (m_failed)
        return {};

    // Lazily create the query on first use: building the PDH query enumerates all
    // counter instances and can take a few milliseconds, which we do not want to pay
    // during node startup.
    if (!m_query) {
        PDH_HQUERY query = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS || !query) {
            m_failed = true;
            return {};
        }

        PDH_HCOUNTER counter = nullptr;
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0,
                                  &counter) != ERROR_SUCCESS || !counter) {
            PdhCloseQuery(query);
            m_failed = true;
            return {};
        }

        // First collection primes the delta calculation; the values only become valid
        // on the collection after this one (we are called at 1 Hz, so the second call
        // already returns real data).
        if (PdhCollectQueryData(query) != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            m_failed = true;
            return {};
        }

        m_query = query;
        m_counter = counter;
        return {}; // no delta available yet
    }

    if (PdhCollectQueryData(static_cast<PDH_HQUERY>(m_query)) != ERROR_SUCCESS)
        return {};

    // Determine the required buffer size, then read all instances of the wildcard counter.
    DWORD bufSize = 0;
    DWORD itemCount = 0;
    LONG status = PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(m_counter),
                                               PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
    if (status != PDH_MORE_DATA && status != ERROR_SUCCESS)
        return {};
    if (bufSize == 0 || itemCount == 0)
        return {};

    std::vector<std::byte> buffer(bufSize);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());

    status = PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(m_counter),
                                          PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
    if (status != ERROR_SUCCESS || !items)
        return {};

    // Instance names look like "pid_12345_luid_0x..._phys_0_eng_0_engtype_3D"; keep only
    // the engines belonging to this process. The same engine can be listed more than once
    // (one instance per process context), so the values are summed per physical engine
    // ("luid_..._eng_N") first and only then the busiest engine is taken.
    // Matching is done on a lower-cased copy: the casing of instance names is not
    // guaranteed across drivers/Windows versions.
    const std::wstring pidTag = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";
    static constexpr wchar_t kTypeTag[] = L"_engtype_";

    struct Engine {
        std::string type;  // as reported, for display ("3D", "VideoDecode", "Copy", ...)
        std::string kind;  // lower-cased, spaces/underscores removed, for classification
        double load = 0.0;
    };
    std::map<std::wstring, Engine> engines; // key: luid/phys/eng part of the instance name

    for (DWORD i = 0; i < itemCount; ++i) {
        const PDH_FMT_COUNTERVALUE_ITEM_W &item = items[i];
        if (!item.szName ||
            (item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
             item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA))
            continue;
        const std::wstring name(item.szName);
        std::wstring lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        if (lower.rfind(pidTag, 0) != 0)
            continue; // belongs to another process

        const size_t typePos = lower.find(kTypeTag);
        const std::wstring key = lower.substr(pidTag.size(), typePos == std::wstring::npos
                                                                 ? std::wstring::npos
                                                                 : typePos - pidTag.size());
        Engine &engine = engines[key];
        if (engine.type.empty()) {
            if (typePos != std::wstring::npos) {
                // Engine type names are ASCII and driver specific, e.g. "3D", "Copy",
                // "Compute_0", "Cuda", "VideoDecode", "Video Decode 1", "Video Codec 0",
                // "JPEG_Decode_1", "High Priority 3D".
                for (wchar_t c : name.substr(typePos + std::size(kTypeTag) - 1)) {
                    const char ch = c < 0x80 ? static_cast<char>(c) : '?';
                    engine.type.push_back(ch);
                    if (ch != ' ' && ch != '_')
                        engine.kind.push_back(static_cast<char>(std::tolower(
                            static_cast<unsigned char>(ch))));
                }
            } else {
                engine.type = "Unknown";
                engine.kind = "unknown";
            }
        }
        engine.load += item.FmtValue.doubleValue;
    }

    const auto isRender = [](const std::string &kind) {
        return kind == "3d" || kind == "highpriority3d";
    };
    // "VideoDecode" (most drivers), "Video Decode N" (Intel/AMD with several decoders) and
    // "Video Codec N" (NVIDIA GPUs whose NVDEC is exposed as a shared codec engine).
    // JPEG decoders ("JPEG_Decode", "Video JPEG") are not video decode.
    const auto isVideoDecode = [](const std::string &kind) {
        return kind.rfind("videodecode", 0) == 0 || kind.rfind("videocodec", 0) == 0;
    };

    if (engines.empty())
        return {}; // this process has no GPU engines (e.g. software rendering)

    GpuLoadSample result;
    result.overall = 0.0;
    result.render = 0.0;
    result.videoDecode = 0.0;
    std::map<std::string, double> perType;
    for (const auto &[key, engine] : engines) {
        const double load = std::clamp(engine.load, 0.0, 100.0);
        result.overall = std::max(result.overall, load);
        if (isRender(engine.kind))
            result.render = std::max(result.render, load);
        else if (isVideoDecode(engine.kind))
            result.videoDecode = std::max(result.videoDecode, load);
        double &typeLoad = perType[engine.type];
        typeLoad = std::max(typeLoad, load);
    }
    result.engineTypes.assign(perType.begin(), perType.end());
    return result;
}

#else // !Q_OS_WIN

GpuLoadProbe::GpuLoadProbe() = default;
GpuLoadProbe::~GpuLoadProbe() = default;

GpuLoadSample GpuLoadProbe::sample() {
    return {};
}

#endif // Q_OS_WIN
