/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "datapathdshowsignal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dshow.h>
#include <DGC133ST.h>

namespace DatapathDShowSignal {

IUnknown* query(IBaseFilter* filter) {
    IVisionUser* vision = nullptr;
    if (!filter || FAILED(filter->QueryInterface(__uuidof(IVisionUser), reinterpret_cast<void**>(&vision))))
        return nullptr;
    return vision;
}

int poll(IUnknown* vision) {
    if (!vision)
        return -1;
    unsigned long type = VISION_NOSIGNAL;
    if (FAILED(static_cast<IVisionUser*>(vision)->get_SignalType(&type)))
        return -1;
    return type == VISION_NOSIGNAL || type == VISION_INVALID ? 0 : 1;
}

} // namespace DatapathDShowSignal
