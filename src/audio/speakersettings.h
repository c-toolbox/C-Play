/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_SPEAKERSETTINGS_H
#define CPLAY_SPEAKERSETTINGS_H

#include "audio/audiomixer.h"
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

namespace cplay::audio {

// Speaker positions per output channel: x = azimuth, y = elevation (degrees, dome frame).
using SpeakerPositions = std::array<glm::vec2, kMaxChannels>;

inline SpeakerPositions defaultSpeakerPositions(int channelCount) {
    SpeakerPositions positions{};
    for (int i = 0; i < kMaxChannels; ++i)
        nominalDirection(channelCount, i, positions[static_cast<std::size_t>(i)].x, positions[static_cast<std::size_t>(i)].y);
    return positions;
}

// Stored as "<channel count>|az:el;az:el;..." in AudioSettings::portAudioSpeakerPositions. The
// positions belong to one output layout: another channel count starts from the defaults.
inline SpeakerPositions parseSpeakerPositions(const QString &text, int channelCount) {
    SpeakerPositions positions = defaultSpeakerPositions(channelCount);
    const qsizetype bar = text.indexOf(QLatin1Char('|'));
    if (bar <= 0 || text.left(bar).toInt() != channelCount)
        return positions;
    const QStringList entries = text.mid(bar + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (int i = 0; i < std::min<int>(kMaxChannels, static_cast<int>(entries.size())); ++i) {
        const QStringList parts = entries[i].split(QLatin1Char(':'));
        if (parts.size() != 2)
            continue;
        bool okAz = false, okEl = false;
        const float az = parts[0].toFloat(&okAz);
        const float el = parts[1].toFloat(&okEl);
        if (okAz && okEl && std::isfinite(az) && std::isfinite(el))
            positions[static_cast<std::size_t>(i)] = glm::vec2(az, std::clamp(el, -90.f, 90.f));
    }
    return positions;
}

inline QString formatSpeakerPositions(const SpeakerPositions &positions, int channelCount) {
    QStringList entries;
    for (int i = 0; i < std::clamp(channelCount, 1, kMaxChannels); ++i) {
        const glm::vec2 &p = positions[static_cast<std::size_t>(i)];
        entries << QStringLiteral("%1:%2").arg(p.x, 0, 'f', 1).arg(p.y, 0, 'f', 1);
    }
    return QStringLiteral("%1|%2").arg(channelCount).arg(entries.join(QLatin1Char(';')));
}

// One speaker of the speaker array as stored in AudioSettings::portAudioSpeakerArray:
// "channel:azimuth:elevation:degrees:height;..." (channel 1-based, angles in degrees, height in
// cm). Both positions are kept so switching between the dome and circular layouts is lossless.
struct ArraySpeakerSettings {
    int channel = 1;
    float azimuth = 0.f;
    float elevation = 0.f;
    float degrees = 0.f;
    float heightCm = 0.f;
};

inline std::vector<ArraySpeakerSettings> parseSpeakerArray(const QString &text) {
    std::vector<ArraySpeakerSettings> speakers;
    const QStringList entries = text.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QStringList parts = entry.split(QLatin1Char(':'));
        if (parts.size() != 5)
            continue;
        bool ok[5] = {};
        ArraySpeakerSettings spk;
        spk.channel = parts[0].toInt(&ok[0]);
        spk.azimuth = parts[1].toFloat(&ok[1]);
        spk.elevation = parts[2].toFloat(&ok[2]);
        spk.degrees = parts[3].toFloat(&ok[3]);
        spk.heightCm = parts[4].toFloat(&ok[4]);
        if (!(ok[0] && ok[1] && ok[2] && ok[3] && ok[4]) || spk.channel < 1 || spk.channel > kMaxOutputChannels
            || !std::isfinite(spk.azimuth) || !std::isfinite(spk.elevation) || !std::isfinite(spk.degrees)
            || !std::isfinite(spk.heightCm))
            continue;
        spk.elevation = std::clamp(spk.elevation, -90.f, 90.f);
        speakers.push_back(spk);
        if (static_cast<int>(speakers.size()) >= kMaxArraySpeakers)
            break;
    }
    return speakers;
}

inline QString formatSpeakerArray(const std::vector<ArraySpeakerSettings> &speakers) {
    QStringList entries;
    for (const ArraySpeakerSettings &spk : speakers)
        entries << QStringLiteral("%1:%2:%3:%4:%5").arg(spk.channel).arg(spk.azimuth, 0, 'f', 1).arg(spk.elevation, 0, 'f', 1)
                       .arg(spk.degrees, 0, 'f', 2).arg(spk.heightCm, 0, 'f', 1);
    return entries.join(QLatin1Char(';'));
}

// Speaker setup from the audio settings, with the given (possibly not yet saved) positions.
// Defined in audiomixer.cpp.
SpeakerSetup speakerSetupFromSettings(const SpeakerPositions &positions, const std::vector<ArraySpeakerSettings> &array);

} // namespace cplay::audio

#endif // CPLAY_SPEAKERSETTINGS_H
