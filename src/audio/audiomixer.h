/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_AUDIOMIXER_H
#define CPLAY_AUDIOMIXER_H

#include <glm/glm.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>

// Channel mapping shared by every layer that plays audio through PortAudio (NDI, OMT,
// DirectShow, Datapath capture and WebRTC).
//
// Standard mapping: up/down mixing between the supported layouts (1.0, 2.0, 2.1, 4.0, 5.0,
// 5.1, 7.0 and 7.1), chosen by "Perform audio mixing (up/down) to output device" in the
// audio settings. Stereo/mono sources keep the established stereo -> surround upmix gains.
//
// Spatial mapping (optional, Audio mapping dialog), one of:
//  - Surround panning: the speakers of the output layout are placed around the dome and each
//    channel of a layer is panned toward the speakers closest to where the layer is shown.
//  - Speaker array: the layer is summed to mono and played on any number of speakers, each
//    on its own output channel, with a share of the sound based on the distance between the
//    layer and the speaker. The speakers are placed on the dome (azimuth/elevation) or along
//    the circular screen of the circular surface mapping (degrees/height).
//
// Dome positions use the dome frame of the 3D view, without the dome tilt: x right, y up and
// -z front (azimuth 0, elevation 0). Azimuth grows clockwise seen from above (toward +x).
namespace cplay::audio {

constexpr int kMaxChannels = 8;         // surround layouts and mixer inputs
constexpr int kMaxOutputChannels = 64;  // outputs (speaker array)
constexpr int kMaxArraySpeakers = 64;

enum class SpatialMode : int32_t { Off = 0, Surround = 1, SpeakerArray = 2 };
enum class ArrayLayout : int32_t { Dome = 0, Circular = 1 };

enum class ChannelRole : uint8_t { FL, FR, C, LFE, SL, SR, RL, RR, Mono };

// Channel order of the PortAudio outputs for a given channel count (1..8):
// 1: C | 2: FL FR | 3: FL FR LFE | 4: FL FR RL RR | 5: FL FR C SL SR | 6: FL FR C LFE SL SR
// 7: FL FR C SL SR RL RR | 8: FL FR C LFE SL SR RL RR
ChannelRole channelRole(int channelCount, int index);
const char *channelLabel(int channelCount, int index);
bool isLfeChannel(int channelCount, int index);
// Nominal (default) speaker direction of a channel in degrees; LFE channels report 0/0.
void nominalDirection(int channelCount, int index, float &azimuthDeg, float &elevationDeg);

// Unit direction in the dome frame for an azimuth/elevation in degrees, and back.
glm::vec3 directionFromAngles(float azimuthDeg, float elevationDeg);
void anglesFromDirection(const glm::vec3 &dir, float &azimuthDeg, float &elevationDeg);

struct MixMatrix {
    int inChannels = 0;
    int outChannels = 0;
    std::array<float, kMaxOutputChannels * kMaxChannels> gains{}; // gains[out * kMaxChannels + in]

    float &at(int out, int in) { return gains[static_cast<std::size_t>(out * kMaxChannels + in)]; }
    float at(int out, int in) const { return gains[static_cast<std::size_t>(out * kMaxChannels + in)]; }
};

// Where a layer's sound comes from, in the dome frame (metres).
struct SpatialSource {
    enum Kind : int32_t {
        None = 0,  // no position (2D layers): standard mapping, or every array speaker alike
        Plane = 1, // flat layer: the channels spread across the plane
        Field = 2  // dome/sphere layer: the channels' nominal directions rotate with the content
    };
    int32_t kind = None;
    glm::vec3 centre{0.f, 0.f, -1.f}; // Plane: centre of the plane
    glm::vec3 halfRight{0.f};         // Plane: from the centre to the right edge of the plane
    glm::vec3 forward{0.f, 0.f, -1.f};// Plane: facing direction, used when the centre is at the origin
    glm::mat3 rotation{1.f};          // Field: content orientation

    // Position on the circular screen (plane layers while the circular surface mapping is on).
    int32_t circularValid = 0;
    float circularDegrees = 0.f;      // centre angle
    float circularHeightCm = 0.f;     // vertical position
    float circleRadiusCm = 0.f;       // circular surface width / 2pi: 360 degrees around the screen

    bool operator==(const SpatialSource &o) const;
};

// One speaker of the speaker array.
struct ArraySpeaker {
    int32_t outputChannel = 0;        // 0-based output channel
    glm::vec3 direction{0.f, 0.f, -1.f}; // dome layout: unit vector
    float degrees = 0.f;              // circular layout: angle on the circular screen
    float heightCm = 0.f;             // circular layout: vertical position
};

// Process-wide speaker setup for the spatial mapping (from the audio settings).
struct SpeakerSetup {
    int32_t mode = 0;            // SpatialMode
    // Surround panning
    int32_t channelCount = 0;    // output layout the positions belong to
    float focus = 4.f;           // panning sharpness: higher = sound stays closer to the nearest speakers
    float width = 1.f;           // 0..1: how far stereo channels spread across a flat layer
    std::array<glm::vec3, kMaxChannels> directions{}; // unit vectors, dome frame
    // Speaker array
    int32_t arrayLayout = 0;     // ArrayLayout
    int32_t arrayCount = 0;
    float rolloffDb = 6.f;       // level drop per doubling of the distance
    float blur = 0.05f;          // added to every distance (fraction of the dome/circle radius): smooths close passes
    int32_t constantPower = 1;   // 1: squared gains sum to 1 (constant loudness), 0: gains sum to 1
    std::array<ArraySpeaker, kMaxArraySpeakers> speakers{};
};

MixMatrix standardMix(int inChannels, int outChannels);
// The mapping in use: standard, surround panning or speaker array.
MixMatrix spatialMix(int inChannels, int outChannels, const SpatialSource &source, const SpeakerSetup &setup);
// Speaker array gain of each array speaker for a source (setup.arrayCount values). Returns
// false when the source has no position for the layout: every speaker then plays it alike.
bool arraySpeakerGains(const SpatialSource &source, const SpeakerSetup &setup, float *gains);

// Seqlock for small trivially copyable values: one writer at a time (serialised by the
// caller), any number of lock-free readers, including real-time audio callbacks.
template <typename T>
class SeqLockValue {
    static_assert(std::is_trivially_copyable_v<T>);
    static constexpr std::size_t kWords = (sizeof(T) + sizeof(uint32_t) - 1) / sizeof(uint32_t);

public:
    void store(const T &value) {
        std::array<uint32_t, kWords> words{};
        std::memcpy(words.data(), &value, sizeof(T));
        const uint32_t seq = m_seq.load(std::memory_order_relaxed);
        m_seq.store(seq + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (std::size_t i = 0; i < kWords; ++i)
            m_words[i].store(words[i], std::memory_order_relaxed);
        m_seq.store(seq + 2, std::memory_order_release);
    }
    // Returns false when a write was in progress on every attempt (the caller keeps its copy).
    bool load(T &value, uint32_t *version = nullptr) const {
        for (int attempt = 0; attempt < 8; ++attempt) {
            const uint32_t before = m_seq.load(std::memory_order_acquire);
            if (before & 1u)
                continue;
            std::array<uint32_t, kWords> words{};
            for (std::size_t i = 0; i < kWords; ++i)
                words[i] = m_words[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (m_seq.load(std::memory_order_relaxed) != before)
                continue;
            std::memcpy(&value, words.data(), sizeof(T));
            if (version)
                *version = before;
            return true;
        }
        return false;
    }
    uint32_t version() const { return m_seq.load(std::memory_order_acquire); }

private:
    std::atomic<uint32_t> m_seq{0};
    std::array<std::atomic<uint32_t>, kWords> m_words{};
};

// Replaces the speaker setup used by all mixers (any non real-time thread).
void setSpeakerSetup(const SpeakerSetup &setup);
SpeakerSetup speakerSetup();
// Reads AudioSettings into the speaker setup. Call on startup and after the settings changed.
void loadSpeakerSetupFromSettings();
// Output channels to open on a device with deviceMaxChannels when mixing to the output is
// on: the configured layout, or enough channels for the speaker array. Non real-time threads.
int configuredOutputChannels(int deviceMaxChannels);

// Per audio output: maps the source channels to the output channels with the standard or
// the spatial mapping. setSource() is called from the render/GUI thread, process() from the
// audio thread (real-time safe: no locks, no allocation). Gain changes are ramped over one
// buffer so moving a layer does not click.
class ChannelMixer {
public:
    void setSource(const SpatialSource &source);

    // Sample (frame f, channel c) is read from in[f * frameStride + c * channelStride], so the
    // same call handles interleaved (frameStride = channels, channelStride = 1) and planar
    // (frameStride = 1, channelStride = frames) input. out is interleaved.
    void process(const float *in, int inChannels, std::size_t frameStride, std::size_t channelStride,
                 float *out, int outChannels, std::size_t frames, float volume = 1.f);
    void process(const int16_t *in, int inChannels, std::size_t frameStride, std::size_t channelStride,
                 int16_t *out, int outChannels, std::size_t frames, float volume = 1.f);

    void processInterleaved(const float *in, int inChannels, float *out, int outChannels, std::size_t frames, float volume = 1.f) {
        process(in, inChannels, static_cast<std::size_t>(inChannels), 1, out, outChannels, frames, volume);
    }

private:
    template <typename In, typename Out>
    void processImpl(const In *in, int inChannels, std::size_t frameStride, std::size_t channelStride,
                     Out *out, int outChannels, std::size_t frames, float volume);
    void refreshTarget(int inChannels, int outChannels);

    // Writer side (render/GUI thread).
    std::mutex m_writeMutex;
    SpatialSource m_lastWritten;
    bool m_hasWritten = false;
    SeqLockValue<SpatialSource> m_source;

    // Audio thread only.
    MixMatrix m_current;
    MixMatrix m_target;
    bool m_hasCurrent = false;
    bool m_hasTarget = false;
    uint32_t m_targetSourceVersion = ~0u;
    uint32_t m_targetSetupVersion = ~0u;
    SpatialSource m_cachedSource;
    SpeakerSetup m_cachedSetup;
};

} // namespace cplay::audio

#endif // CPLAY_AUDIOMIXER_H
