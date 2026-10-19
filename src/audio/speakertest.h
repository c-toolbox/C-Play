/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_SPEAKERTEST_H
#define CPLAY_SPEAKERTEST_H

#include <QString>
#include <atomic>
#include <cstdint>

// Speaker identification for the Audio mapping dialog (master only): plays pulsed pink noise
// on one output channel of the PortAudio output device from the audio settings, so each
// speaker can be matched to its channel and position. Switching to another channel fades the
// old one out first, so stepping through the speakers does not click.
namespace cplay::audio {

class SpeakerTest {
public:
    SpeakerTest() = default;
    ~SpeakerTest();
    SpeakerTest(const SpeakerTest &) = delete;
    SpeakerTest &operator=(const SpeakerTest &) = delete;

    // Plays on the 0-based output channel, opening the output device when needed. Returns
    // false and sets error when the device cannot play that channel.
    bool start(int outputChannel, QString &error);
    void stop();
    bool playing() const;
    void setLevelDb(double db);

    // PortAudio callback body (audio thread).
    void render(float *out, unsigned long frames);

private:
    bool openStream(int minChannels, QString &error);
    void closeStream();

    void *m_stream = nullptr; // PaStream
    bool m_paInitialized = false;
    int m_streamChannels = 0;
    double m_sampleRate = 48000.0;

    // Written by the GUI thread, read by the audio thread.
    std::atomic<int> m_requestedChannel{-1};
    std::atomic<float> m_gain{0.1f};

    // Audio thread only.
    int m_channel = -1;
    float m_switchGain = 0.f;   // fades out before switching channel, then back in
    float m_smoothedGain = 0.f; // level changes without clicks
    uint64_t m_position = 0;    // samples since the channel started (pulse envelope)
    uint32_t m_rng = 0x12345678u;
    float m_pink[3] = {0.f, 0.f, 0.f};
};

} // namespace cplay::audio

#endif // CPLAY_SPEAKERTEST_H
