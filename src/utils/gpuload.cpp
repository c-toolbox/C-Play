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
#include <cstring>
#include <string>
#include <vector>

// The "GPU Engine" counter instances are named e.g.
//   pid_12345_luid_0x00000000_0x0000B5A3_phys_0_eng_0_engtype_3D
// so filtering the enumerated instances for this process's PID yields exactly the
// engines this process uses — the same set Task Manager sums for its per-process GPU %.
GpuLoadProbe::GpuLoadProbe() = default;

GpuLoadProbe::~GpuLoadProbe() {
    if (m_query) {
        PdhCloseQuery(static_cast<PDH_HQUERY>(m_query));
    }
}

double GpuLoadProbe::sample() {
    if (m_failed)
        return -1.0;

    // Lazily create the query on first use: building the PDH query enumerates all
    // counter instances and can take a few milliseconds, which we do not want to pay
    // during node startup.
    if (!m_query) {
        PDH_HQUERY query = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS || !query) {
            m_failed = true;
            return -1.0;
        }

        PDH_HCOUNTER counter = nullptr;
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0,
                                  &counter) != ERROR_SUCCESS || !counter) {
            PdhCloseQuery(query);
            m_failed = true;
            return -1.0;
        }

        // First collection primes the delta calculation; the values only become valid
        // on the collection after this one (we are called at 1 Hz, so the second call
        // already returns real data).
        if (PdhCollectQueryData(query) != ERROR_SUCCESS) {
            PdhCloseQuery(query);
            m_failed = true;
            return -1.0;
        }

        m_query = query;
        m_counter = counter;
        return -1.0; // no delta available yet
    }

    if (PdhCollectQueryData(static_cast<PDH_HQUERY>(m_query)) != ERROR_SUCCESS)
        return -1.0;

    // Determine the required buffer size, then read all instances of the wildcard counter.
    DWORD bufSize = 0;
    DWORD itemCount = 0;
    LONG status = PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(m_counter),
                                               PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
    if (status != PDH_MORE_DATA && status != ERROR_SUCCESS)
        return -1.0;
    if (bufSize == 0 || itemCount == 0)
        return 0.0;

    std::vector<std::byte> buffer(bufSize);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());

    status = PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(m_counter),
                                          PDH_FMT_DOUBLE, &bufSize, &itemCount, items);
    if (status != ERROR_SUCCESS || !items)
        return -1.0;

    // Instance names look like "pid_12345_luid_0x..._eng_0_engtype_3D"; keep only the
    // engines belonging to this process.
    const std::wstring pidTag = L"pid_" + std::to_wstring(GetCurrentProcessId()) + L"_";

    double total = 0.0;
    bool any = false;
    for (DWORD i = 0; i < itemCount; ++i) {
        const PDH_FMT_COUNTERVALUE_ITEM_W &item = items[i];
        if (!item.szName ||
            (item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
             item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA))
            continue;
        if (std::wcsstr(item.szName, pidTag.c_str()) == nullptr)
            continue; // belongs to another process
        total += item.FmtValue.doubleValue;
        any = true;
    }

    if (!any)
        return -1.0; // this process has no GPU engines (e.g. software rendering)

    if (total > 100.0)
        total = 100.0;
    return total;
}

#else // !Q_OS_WIN

GpuLoadProbe::GpuLoadProbe() = default;
GpuLoadProbe::~GpuLoadProbe() = default;

double GpuLoadProbe::sample() {
    return -1.0;
}

#endif // Q_OS_WIN
