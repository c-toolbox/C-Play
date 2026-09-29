/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GPULOAD_H
#define GPULOAD_H

#include <QtGlobal>

// Samples the GPU utilization of this process using the Windows PDH "GPU Engine"
// performance counters (the same counters Task Manager reports). The query is created
// lazily on the first sample() call so node startup is never stalled, and the handles
// are cached for subsequent calls. On non-Windows platforms sample() always returns -1.0.
class GpuLoadProbe {
public:
    GpuLoadProbe();
    ~GpuLoadProbe();

    GpuLoadProbe(const GpuLoadProbe &) = delete;
    GpuLoadProbe(GpuLoadProbe &&) = delete;
    GpuLoadProbe &operator=(const GpuLoadProbe &) = delete;
    GpuLoadProbe &operator=(GpuLoadProbe &&) = delete;

    // Summed GPU utilization percentage across all "GPU Engine" instances belonging to
    // this process, or -1.0 when unavailable (non-Windows, no GPU, or missing counters).
    double sample();

private:
#ifdef Q_OS_WIN
    void *m_query = nullptr;   // PDH_HQUERY
    void *m_counter = nullptr; // PDH_HCOUNTER
    bool m_failed = false;     // query creation failed; do not retry every sample
#endif
};

#endif // GPULOAD_H
