/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DATAPATHDSHOWSIGNAL_H
#define DATAPATHDSHOWSIGNAL_H

#include <unknwn.h>

struct IBaseFilter;

// Signal state of a Datapath Vision DirectShow capture filter (IVisionUser, Datapath DirectShow SDK).
// Kept in its own translation unit since the SDK header declares global enumerators (BLACK, RED, ...).
namespace DatapathDShowSignal {
// AddRef'd IVisionUser of the filter, or nullptr when it is no Datapath Vision filter.
IUnknown* query(IBaseFilter* filter);
// 1 = signal present, 0 = no signal, -1 = unknown.
int poll(IUnknown* vision);
}

#endif // DATAPATHDSHOWSIGNAL_H
