/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MEDIASIZEPROBE_H
#define MEDIASIZEPROBE_H

#include <QString>

namespace MediaSizeProbe {

// Opens the file in a headless libmpv instance (no video or audio output, paused) and reads
// the video size the container reports. Blocks for up to timeoutMs, so call it off the GUI
// thread. Returns false when the file has no video track or could not be opened in time.
bool probe(const QString &file, int &width, int &height, int timeoutMs = 5000);

}

#endif // MEDIASIZEPROBE_H
