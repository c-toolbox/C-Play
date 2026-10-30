/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mediasizeprobe.h"

#include <QElapsedTimer>
#include <client.h>

bool MediaSizeProbe::probe(const QString &file, int &width, int &height, int timeoutMs) {
    width = 0;
    height = 0;

    mpv_handle *mpv = mpv_create();
    if (!mpv)
        return false;

    mpv_set_option_string(mpv, "vo", "null");
    mpv_set_option_string(mpv, "ao", "null");
    mpv_set_option_string(mpv, "pause", "yes");
    mpv_set_option_string(mpv, "aid", "no");
    mpv_set_option_string(mpv, "sid", "no");
    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "terminal", "no");
    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        return false;
    }

    const QByteArray path = file.toUtf8();
    const char *cmd[] = {"loadfile", path.constData(), nullptr};
    bool found = false;
    if (mpv_command(mpv, cmd) >= 0) {
        QElapsedTimer timer;
        timer.start();
        while (!timer.hasExpired(timeoutMs)) {
            const double waitSec = (timeoutMs - timer.elapsed()) / 1000.0;
            mpv_event *event = mpv_wait_event(mpv, waitSec > 0.0 ? waitSec : 0.0);
            if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN)
                break;
            if (event->event_id != MPV_EVENT_FILE_LOADED)
                continue;
            // Before a frame is decoded, width/height are the size the container reports.
            int64_t w = 0, h = 0;
            if (mpv_get_property(mpv, "width", MPV_FORMAT_INT64, &w) >= 0
                && mpv_get_property(mpv, "height", MPV_FORMAT_INT64, &h) >= 0
                && w > 0 && h > 0) {
                width = static_cast<int>(w);
                height = static_cast<int>(h);
                found = true;
            }
            break;
        }
    }

    mpv_terminate_destroy(mpv);
    return found;
}
