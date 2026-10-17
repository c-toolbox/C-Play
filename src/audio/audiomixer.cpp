/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio/audiomixer.h"
#include "audio/speakersettings.h"
#include "audiosettings.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace cplay::audio {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinus3dB = 0.70710678f;

constexpr ChannelRole kLayouts[kMaxChannels][kMaxChannels] = {
    {ChannelRole::Mono},
    {ChannelRole::FL, ChannelRole::FR},
    {ChannelRole::FL, ChannelRole::FR, ChannelRole::LFE},
    {ChannelRole::FL, ChannelRole::FR, ChannelRole::RL, ChannelRole::RR},
    {ChannelRole::FL, ChannelRole::FR, ChannelRole::C, ChannelRole::SL, ChannelRole::SR},
    {ChannelRole::FL, ChannelRole::FR, ChannelRole::C, ChannelRole::LFE, ChannelRole::SL, ChannelRole::SR},
    {ChannelRole::FL, ChannelRole::FR, ChannelRole::C, ChannelRole::SL, ChannelRole::SR, ChannelRole::RL, ChannelRole::RR},
    {ChannelRole::FL, ChannelRole::FR, ChannelRole::C, ChannelRole::LFE, ChannelRole::SL, ChannelRole::SR, ChannelRole::RL, ChannelRole::RR},
};

// Index of the first output channel with the given role, or -1.
int findRole(int channelCount, ChannelRole role) {
    for (int i = 0; i < channelCount; ++i)
        if (channelRole(channelCount, i) == role)
            return i;
    return -1;
}

void setIdentity(MixMatrix &m) {
    for (int i = 0; i < std::min(m.inChannels, m.outChannels); ++i)
        m.at(i, i) = 1.f;
}

// Mono/stereo sources onto a larger layout: the established upmix (front channels unchanged,
// a phantom centre, some surround fill and the low end on the LFE).
void stereoUpmix(MixMatrix &m) {
    const bool mono = m.inChannels == 1;
    const bool hasSides = findRole(m.outChannels, ChannelRole::SL) >= 0;
    for (int oc = 0; oc < m.outChannels; ++oc) {
        float l = 0.f, r = 0.f;
        switch (channelRole(m.outChannels, oc)) {
        case ChannelRole::FL: l = 1.f; break;
        case ChannelRole::FR: r = 1.f; break;
        case ChannelRole::C: l = r = 0.35355339f; break;
        case ChannelRole::LFE: l = r = 0.25f; break;
        case ChannelRole::SL: l = 0.5f; break;
        case ChannelRole::SR: r = 0.5f; break;
        case ChannelRole::RL: l = hasSides ? 0.25f : 0.5f; break;
        case ChannelRole::RR: r = hasSides ? 0.25f : 0.5f; break;
        case ChannelRole::Mono: l = r = 0.5f; break;
        }
        if (mono)
            m.at(oc, 0) = (l + r) * kMinus3dB;
        else {
            m.at(oc, 0) = l;
            m.at(oc, 1) = r;
        }
    }
}

// Any other combination: every source channel goes to the output channel with the same role,
// or folds into the closest one(s) that exist (ITU style downmix).
void roleMix(MixMatrix &m) {
    const int out = m.outChannels;
    auto add = [&](ChannelRole role, int ic, float gain) {
        const int oc = findRole(out, role);
        if (oc < 0)
            return false;
        m.at(oc, ic) += gain;
        return true;
    };
    const bool monoOut = out == 1;
    for (int ic = 0; ic < m.inChannels; ++ic) {
        const ChannelRole role = channelRole(m.inChannels, ic);
        if (monoOut) {
            switch (role) {
            case ChannelRole::C:
            case ChannelRole::Mono: m.at(0, ic) = 1.f; break;
            case ChannelRole::FL:
            case ChannelRole::FR: m.at(0, ic) = kMinus3dB; break;
            default: m.at(0, ic) = 0.5f; break;
            }
            continue;
        }
        if (role != ChannelRole::Mono && add(role, ic, 1.f))
            continue;
        switch (role) {
        case ChannelRole::Mono:
        case ChannelRole::C:
            if (!add(ChannelRole::C, ic, 1.f)) {
                add(ChannelRole::FL, ic, kMinus3dB);
                add(ChannelRole::FR, ic, kMinus3dB);
            }
            break;
        case ChannelRole::LFE:
            add(ChannelRole::FL, ic, 0.5f);
            add(ChannelRole::FR, ic, 0.5f);
            break;
        case ChannelRole::SL:
            if (!add(ChannelRole::RL, ic, kMinus3dB))
                add(ChannelRole::FL, ic, kMinus3dB);
            break;
        case ChannelRole::SR:
            if (!add(ChannelRole::RR, ic, kMinus3dB))
                add(ChannelRole::FR, ic, kMinus3dB);
            break;
        case ChannelRole::RL:
            if (!add(ChannelRole::SL, ic, kMinus3dB))
                add(ChannelRole::FL, ic, kMinus3dB);
            break;
        case ChannelRole::RR:
            if (!add(ChannelRole::SR, ic, kMinus3dB))
                add(ChannelRole::FR, ic, kMinus3dB);
            break;
        default:
            break;
        }
    }
}

// Downmixes sum several channels into one: scale each output that folds in more than full
// scale, leaving the channels that pass straight through untouched.
void normalizeRows(MixMatrix &m) {
    for (int oc = 0; oc < m.outChannels; ++oc) {
        float sum = 0.f;
        for (int ic = 0; ic < m.inChannels; ++ic)
            sum += std::fabs(m.at(oc, ic));
        if (sum > 1.f)
            for (int ic = 0; ic < m.inChannels; ++ic)
                m.at(oc, ic) /= sum;
    }
}

// Direction a source channel is heard from, before panning onto the speakers.
glm::vec3 virtualDirection(const SpatialSource &src, const SpeakerSetup &setup, int inChannels, int ic) {
    float az = 0.f, el = 0.f;
    nominalDirection(inChannels, ic, az, el);
    if (src.kind == SpatialSource::Field) {
        const glm::vec3 d = src.rotation * directionFromAngles(az, el);
        const float len = glm::length(d);
        return len > 1e-6f ? d / len : glm::vec3(0.f, 0.f, -1.f);
    }
    // Plane: left channels at the left part of the layer, right channels at the right part.
    const float lateral = std::clamp(2.f * std::sin(az * kPi / 180.f), -1.f, 1.f) * std::clamp(setup.width, 0.f, 1.f);
    const glm::vec3 base = glm::length(src.centre) > 0.01f ? src.centre : src.forward;
    glm::vec3 p = base + src.halfRight * lateral;
    float len = glm::length(p);
    if (len < 1e-6f) {
        p = base;
        len = glm::length(p);
    }
    return len > 1e-6f ? p / len : glm::vec3(0.f, 0.f, -1.f);
}

// Panning gains of one direction onto the (non-LFE) speakers, with constant total power.
void panGains(const glm::vec3 &dir, const SpeakerSetup &setup, int outChannels, float *gains) {
    const float focus = std::clamp(setup.focus, 0.5f, 50.f);
    float sumSquares = 0.f;
    int speakers = 0;
    for (int oc = 0; oc < outChannels; ++oc) {
        gains[oc] = 0.f;
        if (isLfeChannel(outChannels, oc))
            continue;
        ++speakers;
        const float closeness = std::clamp(0.5f * (1.f + glm::dot(dir, setup.directions[static_cast<std::size_t>(oc)])), 0.f, 1.f);
        gains[oc] = std::pow(closeness, focus);
        sumSquares += gains[oc] * gains[oc];
    }
    if (speakers == 0)
        return;
    if (sumSquares < 1e-12f) {
        // Opposite of every speaker (cannot happen with a sensible layout): play it everywhere.
        for (int oc = 0; oc < outChannels; ++oc)
            if (!isLfeChannel(outChannels, oc))
                gains[oc] = 1.f / std::sqrt(static_cast<float>(speakers));
        return;
    }
    const float norm = 1.f / std::sqrt(sumSquares);
    for (int oc = 0; oc < outChannels; ++oc)
        gains[oc] *= norm;
}

std::mutex &setupWriteMutex() {
    static std::mutex m;
    return m;
}

SeqLockValue<SpeakerSetup> &globalSetup() {
    static SeqLockValue<SpeakerSetup> setup;
    return setup;
}

inline float sampleToFloat(float v) { return v; }
inline float sampleToFloat(int16_t v) { return static_cast<float>(v); }
inline void floatToSample(float v, float &out) { out = v; }
inline void floatToSample(float v, int16_t &out) {
    const float r = v >= 0.f ? v + 0.5f : v - 0.5f;
    out = static_cast<int16_t>(std::clamp(r, static_cast<float>(INT16_MIN), static_cast<float>(INT16_MAX)));
}
} // namespace

ChannelRole channelRole(int channelCount, int index) {
    if (channelCount < 1 || channelCount > kMaxChannels || index < 0 || index >= channelCount)
        return (index % 2) ? ChannelRole::FR : ChannelRole::FL;
    return kLayouts[channelCount - 1][index];
}

const char *channelLabel(int channelCount, int index) {
    switch (channelRole(channelCount, index)) {
    case ChannelRole::FL: return "FL";
    case ChannelRole::FR: return "FR";
    case ChannelRole::C: return "C";
    case ChannelRole::LFE: return "LFE";
    case ChannelRole::SL: return "SL";
    case ChannelRole::SR: return "SR";
    case ChannelRole::RL: return "RL";
    case ChannelRole::RR: return "RR";
    case ChannelRole::Mono: return "M";
    }
    return "";
}

bool isLfeChannel(int channelCount, int index) {
    return channelCount >= 1 && channelCount <= kMaxChannels && channelRole(channelCount, index) == ChannelRole::LFE;
}

void nominalDirection(int channelCount, int index, float &azimuthDeg, float &elevationDeg) {
    elevationDeg = 0.f;
    const bool sevenChannel = channelCount >= 7;
    switch (channelRole(channelCount, index)) {
    case ChannelRole::FL: azimuthDeg = -30.f; break;
    case ChannelRole::FR: azimuthDeg = 30.f; break;
    case ChannelRole::SL: azimuthDeg = sevenChannel ? -90.f : -110.f; break;
    case ChannelRole::SR: azimuthDeg = sevenChannel ? 90.f : 110.f; break;
    case ChannelRole::RL: azimuthDeg = channelCount == 4 ? -135.f : -150.f; break;
    case ChannelRole::RR: azimuthDeg = channelCount == 4 ? 135.f : 150.f; break;
    default: azimuthDeg = 0.f; break;
    }
}

glm::vec3 directionFromAngles(float azimuthDeg, float elevationDeg) {
    const float az = azimuthDeg * kPi / 180.f;
    const float el = elevationDeg * kPi / 180.f;
    return {std::sin(az) * std::cos(el), std::sin(el), -std::cos(az) * std::cos(el)};
}

void anglesFromDirection(const glm::vec3 &dir, float &azimuthDeg, float &elevationDeg) {
    azimuthDeg = std::atan2(dir.x, -dir.z) * 180.f / kPi;
    elevationDeg = std::atan2(dir.y, std::hypot(dir.x, dir.z)) * 180.f / kPi;
}

bool SpatialSource::operator==(const SpatialSource &o) const {
    return kind == o.kind && useSpatial == o.useSpatial && centre == o.centre && halfRight == o.halfRight && forward == o.forward && rotation == o.rotation
        && circularValid == o.circularValid && circularDegrees == o.circularDegrees
        && circularHeightCm == o.circularHeightCm && circleRadiusCm == o.circleRadiusCm;
}

MixMatrix standardMix(int inChannels, int outChannels) {
    MixMatrix m;
    m.inChannels = std::clamp(inChannels, 0, kMaxChannels);
    m.outChannels = std::clamp(outChannels, 0, kMaxOutputChannels);
    if (m.inChannels == 0 || m.outChannels == 0)
        return m;
    if (m.inChannels == m.outChannels || m.outChannels > kMaxChannels)
        setIdentity(m);
    else if (m.inChannels <= 2 && m.outChannels > m.inChannels)
        stereoUpmix(m);
    else
        roleMix(m);
    if (m.inChannels > m.outChannels)
        normalizeRows(m);
    return m;
}

bool arraySpeakerGains(const SpatialSource &source, const SpeakerSetup &setup, float *gains) {
    const int count = std::clamp(setup.arrayCount, 0, kMaxArraySpeakers);
    if (count == 0)
        return false;

    // Distances relative to the dome (unit sphere) or the circular screen (radius 1), so the
    // blur and the rolloff behave the same in both layouts.
    float distances[kMaxArraySpeakers];
    bool positioned = false;
    if (setup.arrayLayout == static_cast<int32_t>(ArrayLayout::Circular)) {
        if (source.circularValid && source.circleRadiusCm > 0.f) {
            positioned = true;
            for (int i = 0; i < count; ++i) {
                const ArraySpeaker &spk = setup.speakers[static_cast<std::size_t>(i)];
                const float delta = std::remainder(spk.degrees - source.circularDegrees, 360.f) * kPi / 180.f;
                const float chord = 2.f * std::sin(std::fabs(delta) * 0.5f);
                const float dh = (spk.heightCm - source.circularHeightCm) / source.circleRadiusCm;
                distances[i] = std::sqrt(chord * chord + dh * dh);
            }
        }
    } else if (source.kind != SpatialSource::None) {
        glm::vec3 dir(0.f, 0.f, -1.f);
        if (source.kind == SpatialSource::Field)
            dir = source.rotation * glm::vec3(0.f, 0.f, -1.f);
        else
            dir = glm::length(source.centre) > 0.01f ? source.centre : source.forward;
        const float len = glm::length(dir);
        if (len > 1e-6f) {
            dir /= len;
            positioned = true;
            for (int i = 0; i < count; ++i)
                distances[i] = glm::length(dir - setup.speakers[static_cast<std::size_t>(i)].direction);
        }
    }

    if (!positioned) {
        const float g = setup.constantPower ? 1.f / std::sqrt(static_cast<float>(count)) : 1.f / static_cast<float>(count);
        for (int i = 0; i < count; ++i)
            gains[i] = g;
        return false;
    }

    // Distance based amplitude panning: weight 1 / (d^2 + blur^2)^(a/2), with a from the level
    // drop per doubling of the distance (6 dB -> a = 1, the inverse distance law).
    const float a = std::clamp(setup.rolloffDb, 0.f, 48.f) / 6.0206f;
    const float blur = std::clamp(setup.blur, 0.001f, 4.f);
    float sum = 0.f;
    for (int i = 0; i < count; ++i) {
        const float d2 = distances[i] * distances[i] + blur * blur;
        gains[i] = std::pow(d2, -0.5f * a);
        sum += setup.constantPower ? gains[i] * gains[i] : gains[i];
    }
    const float norm = setup.constantPower ? 1.f / std::sqrt(sum) : 1.f / sum;
    for (int i = 0; i < count; ++i)
        gains[i] *= norm;
    return true;
}

namespace {
// Speaker array: the layer summed to mono, shared out over the speakers by distance.
MixMatrix arrayMix(int inChannels, int outChannels, const SpatialSource &source, const SpeakerSetup &setup) {
    MixMatrix m;
    m.inChannels = inChannels;
    m.outChannels = outChannels;
    float gains[kMaxArraySpeakers];
    arraySpeakerGains(source, setup, gains);

    int mainInputs = 0;
    for (int ic = 0; ic < inChannels; ++ic)
        mainInputs += isLfeChannel(inChannels, ic) ? 0 : 1;
    const float mainWeight = mainInputs > 0 ? 1.f / std::sqrt(static_cast<float>(mainInputs)) : 0.f;

    const int count = std::clamp(setup.arrayCount, 0, kMaxArraySpeakers);
    for (int i = 0; i < count; ++i) {
        const int oc = setup.speakers[static_cast<std::size_t>(i)].outputChannel;
        if (oc < 0 || oc >= outChannels)
            continue;
        for (int ic = 0; ic < inChannels; ++ic)
            m.at(oc, ic) += gains[i] * (isLfeChannel(inChannels, ic) ? 0.5f : mainWeight);
    }
    return m;
}
} // namespace

MixMatrix spatialMix(int inChannels, int outChannels, const SpatialSource &source, const SpeakerSetup &setup) {
    if (!source.useSpatial || inChannels < 1 || inChannels > kMaxChannels || outChannels < 1 || outChannels > kMaxOutputChannels)
        return standardMix(inChannels, outChannels);
    if (setup.mode == static_cast<int32_t>(SpatialMode::SpeakerArray) && setup.arrayCount > 0)
        return arrayMix(inChannels, outChannels, source, setup);
    if (setup.mode != static_cast<int32_t>(SpatialMode::Surround) || source.kind == SpatialSource::None
        || outChannels < 2 || outChannels != setup.channelCount || outChannels > kMaxChannels)
        return standardMix(inChannels, outChannels);

    MixMatrix m;
    m.inChannels = inChannels;
    m.outChannels = outChannels;

    int lfeOutputs = 0;
    for (int oc = 0; oc < outChannels; ++oc)
        lfeOutputs += isLfeChannel(outChannels, oc) ? 1 : 0;
    int mainInputs = 0;
    bool lfeInput = false;
    for (int ic = 0; ic < inChannels; ++ic) {
        if (isLfeChannel(inChannels, ic))
            lfeInput = true;
        else
            ++mainInputs;
    }

    float gains[kMaxChannels];
    for (int ic = 0; ic < inChannels; ++ic) {
        if (isLfeChannel(inChannels, ic)) {
            if (lfeOutputs > 0) {
                for (int oc = 0; oc < outChannels; ++oc)
                    if (isLfeChannel(outChannels, oc))
                        m.at(oc, ic) = 1.f;
                continue;
            }
            // No subwoofer: the low end is played from where the layer is.
            SpatialSource centred = source;
            centred.halfRight = glm::vec3(0.f);
            panGains(virtualDirection(centred, setup, 1, 0), setup, outChannels, gains);
            for (int oc = 0; oc < outChannels; ++oc)
                m.at(oc, ic) = 0.5f * gains[oc];
            continue;
        }
        panGains(virtualDirection(source, setup, inChannels, ic), setup, outChannels, gains);
        for (int oc = 0; oc < outChannels; ++oc)
            m.at(oc, ic) = gains[oc];
    }

    // Sources without LFE feed the subwoofer like the standard upmix does.
    if (!lfeInput && lfeOutputs > 0 && mainInputs > 0) {
        const float lfeGain = 0.5f / static_cast<float>(mainInputs);
        for (int oc = 0; oc < outChannels; ++oc)
            if (isLfeChannel(outChannels, oc))
                for (int ic = 0; ic < inChannels; ++ic)
                    m.at(oc, ic) = lfeGain;
    }
    return m;
}

void setSpeakerSetup(const SpeakerSetup &setup) {
    std::lock_guard<std::mutex> lock(setupWriteMutex());
    SpeakerSetup normalized = setup;
    for (auto &d : normalized.directions) {
        const float len = glm::length(d);
        d = len > 1e-6f ? d / len : glm::vec3(0.f, 0.f, -1.f);
    }
    globalSetup().store(normalized);
}

SpeakerSetup speakerSetup() {
    SpeakerSetup setup;
    globalSetup().load(setup);
    return setup;
}

SpeakerSetup speakerSetupFromSettings(const SpeakerPositions &positions, const std::vector<ArraySpeakerSettings> &array) {
    SpeakerSetup setup;
    setup.mode = AudioSettings::portAudioMixInputToOutput() ? std::clamp(AudioSettings::portAudioSpatialMode(), 0, 2) : 0;
    setup.channelCount = std::clamp(AudioSettings::portAudioOutputChannels(), 1, kMaxChannels);
    setup.focus = static_cast<float>(AudioSettings::portAudioSpatialFocus());
    setup.width = static_cast<float>(AudioSettings::portAudioSpatialWidth()) / 100.f;
    for (int i = 0; i < kMaxChannels; ++i)
        setup.directions[static_cast<std::size_t>(i)] = directionFromAngles(positions[static_cast<std::size_t>(i)].x,
                                                                            positions[static_cast<std::size_t>(i)].y);
    setup.arrayLayout = std::clamp(AudioSettings::portAudioArrayLayout(), 0, 1);
    setup.rolloffDb = static_cast<float>(AudioSettings::portAudioArrayRolloff());
    setup.blur = static_cast<float>(AudioSettings::portAudioArrayBlur()) / 100.f;
    setup.constantPower = AudioSettings::portAudioArrayConstantPower() ? 1 : 0;
    setup.arrayCount = std::min<int>(kMaxArraySpeakers, static_cast<int>(array.size()));
    for (int i = 0; i < setup.arrayCount; ++i) {
        const ArraySpeakerSettings &a = array[static_cast<std::size_t>(i)];
        ArraySpeaker &spk = setup.speakers[static_cast<std::size_t>(i)];
        spk.outputChannel = std::clamp(a.channel, 1, kMaxOutputChannels) - 1;
        spk.direction = directionFromAngles(a.azimuth, a.elevation);
        spk.degrees = a.degrees;
        spk.heightCm = a.heightCm;
    }
    return setup;
}

void loadSpeakerSetupFromSettings() {
    const int channels = std::clamp(AudioSettings::portAudioOutputChannels(), 1, kMaxChannels);
    setSpeakerSetup(speakerSetupFromSettings(parseSpeakerPositions(AudioSettings::portAudioSpeakerPositions(), channels),
                                             parseSpeakerArray(AudioSettings::portAudioSpeakerArray())));
}

int configuredOutputChannels(int deviceMaxChannels) {
    const int deviceMax = std::max(1, deviceMaxChannels);
    if (AudioSettings::portAudioSpatialMode() == static_cast<int>(SpatialMode::SpeakerArray)) {
        int highest = 0;
        for (const ArraySpeakerSettings &a : parseSpeakerArray(AudioSettings::portAudioSpeakerArray()))
            highest = std::max(highest, a.channel);
        if (highest > 0)
            return std::min({highest, deviceMax, kMaxOutputChannels});
    }
    return std::min(AudioSettings::portAudioOutputChannels(), deviceMax);
}

void ChannelMixer::setSource(const SpatialSource &source) {
    std::lock_guard<std::mutex> lock(m_writeMutex);
    if (m_hasWritten && m_lastWritten == source)
        return; // unchanged: the audio thread keeps its matrix
    m_lastWritten = source;
    m_hasWritten = true;
    m_source.store(source);
}

void ChannelMixer::refreshTarget(int inChannels, int outChannels) {
    const uint32_t sourceVersion = m_source.version();
    const uint32_t setupVersion = globalSetup().version();
    if (m_hasTarget && m_target.inChannels == inChannels && m_target.outChannels == outChannels
        && sourceVersion == m_targetSourceVersion && setupVersion == m_targetSetupVersion)
        return;

    // A failed read (writer busy) keeps the previous value and version, so it is retried on
    // the next buffer.
    SpatialSource source;
    uint32_t version = 0;
    if (m_source.load(source, &version)) {
        m_cachedSource = source;
        m_targetSourceVersion = version;
    }
    SpeakerSetup setup;
    if (globalSetup().load(setup, &version)) {
        m_cachedSetup = setup;
        m_targetSetupVersion = version;
    }
    m_target = spatialMix(inChannels, outChannels, m_cachedSource, m_cachedSetup);
    m_hasTarget = true;
}

template <typename In, typename Out>
void ChannelMixer::processImpl(const In *in, int inChannels, std::size_t frameStride, std::size_t channelStride,
                               Out *out, int outChannels, std::size_t frames, float volume) {
    if (!in || !out || inChannels <= 0 || outChannels <= 0 || frames == 0)
        return;
    const std::size_t outStride = static_cast<std::size_t>(outChannels);

    if (inChannels > kMaxChannels || outChannels > kMaxOutputChannels) {
        // Beyond the supported layouts: channel n plays on output n.
        for (std::size_t f = 0; f < frames; ++f)
            for (int oc = 0; oc < outChannels; ++oc) {
                const float v = oc < inChannels ? sampleToFloat(in[f * frameStride + static_cast<std::size_t>(oc) * channelStride]) : 0.f;
                floatToSample(v * volume, out[f * outStride + static_cast<std::size_t>(oc)]);
            }
        return;
    }

    refreshTarget(inChannels, outChannels);
    const bool sameShape = m_hasCurrent && m_current.inChannels == inChannels && m_current.outChannels == outChannels;
    const bool ramp = sameShape && m_current.gains != m_target.gains;
    if (!ramp)
        m_current = m_target;

    float x[kMaxChannels];
    const float step = 1.f / static_cast<float>(frames);
    for (std::size_t f = 0; f < frames; ++f) {
        for (int ic = 0; ic < inChannels; ++ic)
            x[ic] = sampleToFloat(in[f * frameStride + static_cast<std::size_t>(ic) * channelStride]);
        const float t = ramp ? static_cast<float>(f + 1) * step : 1.f;
        for (int oc = 0; oc < outChannels; ++oc) {
            float acc = 0.f;
            for (int ic = 0; ic < inChannels; ++ic) {
                const float target = m_target.at(oc, ic);
                const float g = ramp ? m_current.at(oc, ic) + (target - m_current.at(oc, ic)) * t : target;
                acc += g * x[ic];
            }
            floatToSample(acc * volume, out[f * outStride + static_cast<std::size_t>(oc)]);
        }
    }
    m_current = m_target;
    m_hasCurrent = true;
}

void ChannelMixer::process(const float *in, int inChannels, std::size_t frameStride, std::size_t channelStride,
                           float *out, int outChannels, std::size_t frames, float volume) {
    processImpl(in, inChannels, frameStride, channelStride, out, outChannels, frames, volume);
}

void ChannelMixer::process(const int16_t *in, int inChannels, std::size_t frameStride, std::size_t channelStride,
                           int16_t *out, int outChannels, std::size_t frames, float volume) {
    processImpl(in, inChannels, frameStride, channelStride, out, outChannels, frames, volume);
}

} // namespace cplay::audio
