/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GPULOAD_H
#define GPULOAD_H

#include <QtGlobal>

#include <string>
#include <utility>
#include <vector>

// One GPU utilization sample of this process. All values are percentages (0..100) or
// -1.0 when unavailable.
//
// Utilization is per hardware engine (3D, VideoDecode, Copy, Compute, ...). Engines run
// in parallel, so their loads must never be added: a busy NVDEC plus a busy 3D engine is
// not "more than 100 %". Like Task Manager, each figure is the load of the busiest engine
// in its group.
struct GpuLoadSample {
    double overall = -1.0;     // busiest engine of any type (Task Manager's "GPU" column)
    double render = -1.0;      // busiest 3D engine (rendering)
    double videoDecode = -1.0; // busiest VideoDecode engine (NVDEC / hw decoding)
    // Busiest engine per engine type, e.g. {"3D", 35}, {"VideoDecode", 60}, {"Copy", 4}.
    std::vector<std::pair<std::string, double>> engineTypes;

    bool valid() const { return overall >= 0.0; }
};

// Samples the GPU utilization of this process using the Windows PDH "GPU Engine"
// performance counters (the same counters Task Manager reports). The query is created
// lazily on the first sample() call so node startup is never stalled, and the handles
// are cached for subsequent calls. On non-Windows platforms sample() is always invalid.
class GpuLoadProbe {
public:
    GpuLoadProbe();
    ~GpuLoadProbe();

    GpuLoadProbe(const GpuLoadProbe &) = delete;
    GpuLoadProbe(GpuLoadProbe &&) = delete;
    GpuLoadProbe &operator=(const GpuLoadProbe &) = delete;
    GpuLoadProbe &operator=(GpuLoadProbe &&) = delete;

    // Invalid (all -1.0) when unavailable (non-Windows, no GPU, missing counters, or the
    // first call that only primes the counters).
    GpuLoadSample sample();

private:
#ifdef Q_OS_WIN
    void *m_query = nullptr;   // PDH_HQUERY
    void *m_counter = nullptr; // PDH_HCOUNTER
    bool m_failed = false;     // query creation failed; do not retry every sample
#endif
};

#endif // GPULOAD_H
