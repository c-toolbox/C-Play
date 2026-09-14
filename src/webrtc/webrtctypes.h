/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QUrl>

#include <cstdint>
#include <vector>

/// Video codec negotiated with the WHEP endpoint.
enum class WebRtcVideoCodec {
    Unknown,
    H264,
    H265,
};

/// High level state of one WHEP pull session.
enum class WebRtcStreamState {
    Idle,
    Connecting,
    Running,
    Failed,
    Stopping,
};

/// Everything needed to pull one WHEP stream. The URL is stored in the layer's
/// filepath property; credentials may be embedded in it (user:pass@host).
struct WebRtcStreamConfig {
    QUrl whepUrl;
    QString username;
    QString password;

    // Order matters: the first entry is offered with the highest priority.
    std::vector<WebRtcVideoCodec> preferredCodecs = { WebRtcVideoCodec::H264, WebRtcVideoCodec::H265 };
};
