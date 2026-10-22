/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio/speakertest.h"
#include "audio/audiomixer.h"
#include "audiosettings.h"
#include <QThread>
#include <algorithm>
#include <cmath>

#ifdef CPLAY_PORTAUDIO
#include <portaudio.h>
#include <audio/portaudiolock.h>
#endif

namespace cplay::audio {

namespace {
constexpr double kPulsePeriod = 1.0;  // seconds per burst
constexpr double kPulseOn = 0.7;      // seconds of noise in each burst
constexpr double kPulseFade = 0.01;   // fade in/out of each burst and of channel switches

#ifdef CPLAY_PORTAUDIO
int speakerTestCallback(const void *, void *output, unsigned long frames, const PaStreamCallbackTimeInfo *,
                        PaStreamCallbackFlags, void *userData) {
    if (output && userData)
        static_cast<SpeakerTest *>(userData)->render(static_cast<float *>(output), frames);
    return paContinue;
}

// The output device from the audio settings (same choice as the PortAudio layers).
PaDeviceIndex chooseOutputDevice() {
    const PaDeviceIndex chosen = Pa_GetDefaultOutputDevice();
    if (!AudioSettings::portAudioCustomOutput() || AudioSettings::portAudioOutputDevice().isEmpty()
        || AudioSettings::portAudioOutputApi().isEmpty())
        return chosen;

    const int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo *api = info ? Pa_GetHostApiInfo(info->hostApi) : nullptr;
        if (info && api && info->maxOutputChannels > 1
            && QString::fromUtf8(info->name) == AudioSettings::portAudioOutputDevice()
            && QString::fromUtf8(api->name) == AudioSettings::portAudioOutputApi())
            return i;
    }
    return chosen;
}
#endif
} // namespace

SpeakerTest::~SpeakerTest() {
    closeStream();
}

bool SpeakerTest::playing() const {
    return m_stream != nullptr && m_requestedChannel.load() >= 0;
}

void SpeakerTest::setLevelDb(double db) {
    m_gain.store(static_cast<float>(std::pow(10.0, std::clamp(db, -60.0, 0.0) / 20.0)));
}

bool SpeakerTest::start(int outputChannel, QString &error) {
    error.clear();
    if (outputChannel < 0 || outputChannel >= kMaxOutputChannels) {
        error = QStringLiteral("Invalid output channel %1.").arg(outputChannel + 1);
        return false;
    }
    if (!m_stream || outputChannel >= m_streamChannels) {
        closeStream();
        if (!openStream(outputChannel + 1, error))
            return false;
    }
    m_requestedChannel.store(outputChannel);
    return true;
}

void SpeakerTest::stop() {
    closeStream();
}

bool SpeakerTest::openStream(int minChannels, QString &error) {
#ifdef CPLAY_PORTAUDIO
    if (!m_paInitialized) {
        const PaError err = cplay::audio::paInitialize();
        if (err != paNoError) {
            error = QStringLiteral("PortAudio could not start: %1").arg(QString::fromUtf8(Pa_GetErrorText(err)));
            return false;
        }
        m_paInitialized = true;
    }

    const PaDeviceIndex device = chooseOutputDevice();
    const PaDeviceInfo *info = device != paNoDevice ? Pa_GetDeviceInfo(device) : nullptr;
    if (!info) {
        error = QStringLiteral("No audio output device.");
        return false;
    }
    if (minChannels > info->maxOutputChannels) {
        error = QStringLiteral("Output channel %1 is beyond the %2 channels of \"%3\".")
                    .arg(minChannels).arg(info->maxOutputChannels).arg(QString::fromUtf8(info->name));
        return false;
    }

    // As many channels as the layers open, so the device sees the same layout.
    const int channels = std::clamp(std::max(minChannels, configuredOutputChannels(info->maxOutputChannels)), 1,
                                     info->maxOutputChannels);
    PaStreamParameters params{};
    params.device = device;
    params.channelCount = channels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = info->defaultHighOutputLatency;

    // Fresh audio thread state (no callback is running yet).
    m_sampleRate = info->defaultSampleRate > 0.0 ? info->defaultSampleRate : 48000.0;
    m_requestedChannel.store(-1);
    m_channel = -1;
    m_switchGain = 0.f;
    m_smoothedGain = 0.f;
    m_position = 0;
    m_pink[0] = m_pink[1] = m_pink[2] = 0.f;
    m_streamChannels = channels; // read by the callback

    PaStream *stream = nullptr;
    PaError err = cplay::audio::paOpenStream(&stream, nullptr, &params, m_sampleRate, paFramesPerBufferUnspecified, paClipOff,
                                speakerTestCallback, this);
    if (err == paNoError) {
        err = cplay::audio::paStartStream(stream);
        if (err != paNoError)
            cplay::audio::paCloseStream(stream);
    }
    if (err != paNoError) {
        m_streamChannels = 0;
        error = QStringLiteral("Could not open %1 channels on \"%2\": %3")
                    .arg(channels).arg(QString::fromUtf8(info->name)).arg(QString::fromUtf8(Pa_GetErrorText(err)));
        return false;
    }
    m_stream = stream;
    return true;
#else
    Q_UNUSED(minChannels)
    error = QStringLiteral("This build has no PortAudio output.");
    return false;
#endif
}

void SpeakerTest::closeStream() {
#ifdef CPLAY_PORTAUDIO
    if (m_stream) {
        // Fade out before stopping so the noise does not end with a click.
        m_requestedChannel.store(-1);
        QThread::msleep(static_cast<unsigned long>(kPulseFade * 3000.0));
        cplay::audio::paStopStream(static_cast<PaStream *>(m_stream));
        cplay::audio::paCloseStream(static_cast<PaStream *>(m_stream));
        m_stream = nullptr;
        m_streamChannels = 0;
    }
    if (m_paInitialized) {
        cplay::audio::paTerminate(); // reference counted with the layers' Pa_Initialize
        m_paInitialized = false;
    }
#endif
}

void SpeakerTest::render(float *out, unsigned long frames) {
    const int channels = m_streamChannels;
    std::fill(out, out + static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels), 0.f);

    const int requested = m_requestedChannel.load(std::memory_order_relaxed);
    const float target = m_gain.load(std::memory_order_relaxed);
    const float fadeStep = static_cast<float>(1.0 / (kPulseFade * m_sampleRate));
    const float smooth = static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * m_sampleRate)));
    const uint64_t period = static_cast<uint64_t>(kPulsePeriod * m_sampleRate);
    const uint64_t on = static_cast<uint64_t>(kPulseOn * m_sampleRate);
    const float fadeSamples = static_cast<float>(kPulseFade * m_sampleRate);

    for (unsigned long f = 0; f < frames; ++f) {
        // Switch channel only once the previous one has faded out.
        if (requested != m_channel) {
            m_switchGain -= fadeStep;
            if (m_switchGain <= 0.f) {
                m_switchGain = 0.f;
                m_channel = requested;
                m_position = 0;
            }
        } else if (m_channel >= 0) {
            m_switchGain = std::min(1.f, m_switchGain + fadeStep);
        }
        m_smoothedGain += (target - m_smoothedGain) * smooth;

        if (m_channel < 0 || m_channel >= channels) {
            ++m_position;
            continue;
        }

        // Pink noise (Paul Kellet's economy filter on white noise).
        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 17;
        m_rng ^= m_rng << 5;
        const float white = static_cast<float>(m_rng) * (2.f / 4294967295.f) - 1.f;
        m_pink[0] = 0.99765f * m_pink[0] + white * 0.0990460f;
        m_pink[1] = 0.96300f * m_pink[1] + white * 0.2965164f;
        m_pink[2] = 0.57000f * m_pink[2] + white * 1.0526913f;
        const float pink = (m_pink[0] + m_pink[1] + m_pink[2] + white * 0.1848f) * 0.25f;

        // Bursts with short fades.
        const uint64_t t = m_position % period;
        float envelope = 0.f;
        if (t < on) {
            const float fromStart = static_cast<float>(t);
            const float toEnd = static_cast<float>(on - t);
            envelope = std::min({1.f, fromStart / fadeSamples, toEnd / fadeSamples});
        }
        ++m_position;

        out[f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(m_channel)] =
            pink * envelope * m_switchGain * m_smoothedGain;
    }
}

} // namespace cplay::audio
