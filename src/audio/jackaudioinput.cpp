/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio/jackaudioinput.h"

#ifdef CPLAY_JACK_AUDIO_INPUT

#include "audiosettings.h"
#include "layers/baselayer.h"
#include <sgct/sgct.h>
#include <algorithm>
#include <format>
#include <audio/portaudiolock.h>

namespace cplay::audio {

namespace {
constexpr auto kRetryInterval = std::chrono::seconds(5);
constexpr auto kCheckInterval = std::chrono::seconds(1);

// Same choice of output device as the other PortAudio layers.
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
} // namespace

JackAudioInput::JackAudioInput(BaseLayer &layer, std::string clientName)
    : m_layer(layer), m_clientName(std::move(clientName)) {
    const PaError err = cplay::audio::paInitialize(); // reference counted with the other PortAudio users
    if (err == paNoError)
        m_paInitialized = true;
    else
        sgct::Log::Error(std::format("JackAudioInput: PortAudio initialization failed ({})", Pa_GetErrorText(err)));
}

JackAudioInput::~JackAudioInput() {
    closeOutput();
    closeClient();
    if (m_paInitialized)
        cplay::audio::paTerminate();
}

bool JackAudioInput::update() {
    if (!m_paInitialized)
        return false;

    bool reopened = false;
    if (m_serverGone.exchange(false)) {
        sgct::Log::Warning(std::format("JackAudioInput: the JACK server closed \"{}\".", m_clientName));
        closeOutput();
        closeClient();
    }
    const auto now = std::chrono::steady_clock::now();
    if (!m_client) {
        if (m_lastOpenAttempt != std::chrono::steady_clock::time_point{} && now - m_lastOpenAttempt < kRetryInterval)
            return false;
        const bool firstAttempt = m_lastOpenAttempt == std::chrono::steady_clock::time_point{};
        m_lastOpenAttempt = now;
        if (!openClient()) {
            if (firstAttempt)
                sgct::Log::Warning(std::format("JackAudioInput: could not open the JACK client \"{}\" (is the JACK "
                                               "server running?). Retrying every few seconds.", m_clientName));
            return false;
        }
        reopened = true;
        m_recheck.store(true);
    }

    // Follow the connected channels at once; the device and audio mapping at most once a second.
    const int connected = m_connectedChannels.load();
    bool check = m_recheck.exchange(false);
    if (connected != m_openedInChannels) {
        m_openedInChannels = connected;
        check = true;
    }
    if (!check && now - m_lastCheck < kCheckInterval)
        return reopened;
    m_lastCheck = now;

    if (connected <= 0) {
        closeOutput(); // mpv is not connected (no audio or another output driver)
        return reopened;
    }

    const PaDeviceIndex device = chooseOutputDevice();
    const PaDeviceInfo *info = device != paNoDevice ? Pa_GetDeviceInfo(device) : nullptr;
    if (!info || info->maxOutputChannels <= 0) {
        closeOutput();
        return reopened;
    }
    // Up/down mix to the output layout, or the source channels unmixed (as the live layers).
    int outChannels = m_layer.audioMixingEnabled() ? configuredOutputChannels(info->maxOutputChannels) : connected;
    outChannels = std::clamp(outChannels, 1, info->maxOutputChannels);
    if (m_stream && device == m_device && outChannels == m_outChannels)
        return reopened;

    closeOutput();
    if (m_lastOutputFailure != std::chrono::steady_clock::time_point{} && now - m_lastOutputFailure < kRetryInterval)
        return reopened;
    if (!openOutput(device, outChannels))
        m_lastOutputFailure = now;
    return reopened;
}

bool JackAudioInput::openClient() {
    jack_status_t status{};
    // Exact name: mpv finds the ports through it (--jack-port).
    m_client = jack_client_open(m_clientName.c_str(), static_cast<jack_options_t>(JackNoStartServer | JackUseExactName),
                                &status);
    if (!m_client)
        return false;

    m_sampleRate = static_cast<int>(jack_get_sample_rate(m_client));
    m_jackPeriod = static_cast<int>(jack_get_buffer_size(m_client));
    m_ringFrames = static_cast<std::size_t>(std::max(m_sampleRate, 8000)); // ~1 s
    m_ring.assign(m_ringFrames * kMaxChannels, 0.f);
    m_ringWrite.store(0);
    m_ringRead.store(0);

    for (int i = 0; i < kMaxChannels; ++i) {
        const std::string name = "in_" + std::to_string(i + 1);
        m_ports[static_cast<std::size_t>(i)] = jack_port_register(m_client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
                                                                  JackPortIsInput, 0);
        if (!m_ports[static_cast<std::size_t>(i)]) {
            sgct::Log::Error(std::format("JackAudioInput: could not register the JACK port {}:{}", m_clientName, name));
            closeClient();
            return false;
        }
    }

    jack_set_process_callback(m_client, &JackAudioInput::jackProcess, this);
    jack_set_port_connect_callback(m_client, &JackAudioInput::jackPortConnect, this);
    jack_on_shutdown(m_client, &JackAudioInput::jackShutdown, this);
    if (jack_activate(m_client) != 0) {
        sgct::Log::Error(std::format("JackAudioInput: could not activate the JACK client \"{}\".", m_clientName));
        closeClient();
        return false;
    }
    countConnectedChannels();
    sgct::Log::Info(std::format("JackAudioInput: JACK client \"{}\" open ({} Hz, {} frames).", m_clientName,
                                m_sampleRate, m_jackPeriod));
    return true;
}

void JackAudioInput::closeClient() {
    if (m_client) {
        jack_deactivate(m_client);
        jack_client_close(m_client); // also unregisters the ports
        m_client = nullptr;
    }
    m_ports.fill(nullptr);
    m_connectedChannels.store(0);
    m_openedInChannels = 0;
}

bool JackAudioInput::openOutput(PaDeviceIndex device, int outChannels) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(device);
    PaStreamParameters params{};
    params.device = device;
    params.channelCount = outChannels;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = info ? info->defaultLowOutputLatency : 0.05;

    // Read by the callback, fixed while the stream runs.
    m_outChannels = outChannels;
    m_targetFrames = std::max(2 * m_jackPeriod, m_sampleRate / 50);       // ~20 ms
    m_limitFrames = m_targetFrames + std::max(2 * m_jackPeriod, m_sampleRate / 12); // + ~80 ms

    PaError err = cplay::audio::paOpenStream(&m_stream, nullptr, &params, m_sampleRate, paFramesPerBufferUnspecified, paClipOff,
                                &JackAudioInput::paCallback, this);
    if (err == paNoError) {
        err = cplay::audio::paStartStream(m_stream);
        if (err != paNoError) {
            cplay::audio::paCloseStream(m_stream);
        }
    }
    if (err != paNoError) {
        m_stream = nullptr;
        m_outChannels = 0;
        sgct::Log::Error(std::format("JackAudioInput: could not open {} channels at {} Hz on \"{}\" ({})", outChannels,
                                     m_sampleRate, info ? info->name : "?", Pa_GetErrorText(err)));
        return false;
    }
    m_device = device;
    m_outputRunning.store(true, std::memory_order_release);
    sgct::Log::Info(std::format("JackAudioInput: \"{}\" plays {} -> {} channel(s) on \"{}\".", m_clientName,
                                m_openedInChannels, outChannels, info ? info->name : "?"));
    return true;
}

void JackAudioInput::closeOutput() {
    m_outputRunning.store(false, std::memory_order_release);
    if (m_stream) {
        if (cplay::audio::paStopStream(m_stream) != paNoError)
            cplay::audio::paAbortStream(m_stream);
        cplay::audio::paCloseStream(m_stream);
        m_stream = nullptr;
    }
    m_device = paNoDevice;
    m_outChannels = 0;
}

void JackAudioInput::countConnectedChannels() {
    int highest = 0;
    for (int i = 0; i < kMaxChannels; ++i) {
        jack_port_t *port = m_ports[static_cast<std::size_t>(i)];
        if (port && jack_port_connected(port) > 0)
            highest = i + 1;
    }
    m_connectedChannels.store(highest);
}

int JackAudioInput::jackProcess(jack_nframes_t frames, void *arg) {
    // JACK real-time thread: no locks, no allocation.
    auto *self = static_cast<JackAudioInput *>(arg);
    const int channels = self->m_connectedChannels.load(std::memory_order_relaxed);
    if (channels <= 0 || !self->m_outputRunning.load(std::memory_order_acquire))
        return 0;

    std::array<const float *, kMaxChannels> in{};
    for (int c = 0; c < channels; ++c)
        in[static_cast<std::size_t>(c)] = static_cast<const float *>(
            jack_port_get_buffer(self->m_ports[static_cast<std::size_t>(c)], frames));

    // When the ring is full the newest audio is dropped; the reader skips ahead before that happens.
    const std::size_t write = self->m_ringWrite.load(std::memory_order_relaxed);
    const std::size_t read = self->m_ringRead.load(std::memory_order_acquire);
    const std::size_t n = std::min<std::size_t>(frames, self->m_ringFrames - (write - read));
    for (std::size_t f = 0; f < n; ++f) {
        float *frame = self->m_ring.data() + ((write + f) % self->m_ringFrames) * kMaxChannels;
        for (int c = 0; c < channels; ++c)
            frame[c] = in[static_cast<std::size_t>(c)][f];
        for (int c = channels; c < kMaxChannels; ++c)
            frame[c] = 0.f;
    }
    self->m_ringWrite.store(write + n, std::memory_order_release);
    return 0;
}

void JackAudioInput::jackShutdown(void *arg) {
    auto *self = static_cast<JackAudioInput *>(arg);
    self->m_outputRunning.store(false);
    self->m_serverGone.store(true);
}

void JackAudioInput::jackPortConnect(jack_port_id_t, jack_port_id_t, int, void *arg) {
    // JACK notification thread: mpv (re)connected or left.
    static_cast<JackAudioInput *>(arg)->countConnectedChannels();
}

int JackAudioInput::paCallback(const void *, void *output, unsigned long frames, const PaStreamCallbackTimeInfo *,
                               PaStreamCallbackFlags, void *userData) {
    // PortAudio real-time thread: no locks, no allocation.
    auto *self = static_cast<JackAudioInput *>(userData);
    float *out = static_cast<float *>(output);
    const int outChannels = self->m_outChannels;
    const int inChannels = self->m_connectedChannels.load(std::memory_order_relaxed);
    std::size_t done = 0;

    if (inChannels > 0) {
        // JACK and the output device have separate clocks: when more than the limit is
        // buffered (the JACK side runs faster, or the output was just reopened), skip ahead.
        std::size_t read = self->m_ringRead.load(std::memory_order_relaxed);
        const std::size_t write = self->m_ringWrite.load(std::memory_order_acquire);
        std::size_t available = write - read;
        const std::size_t limit = static_cast<std::size_t>(self->m_limitFrames) + frames;
        const std::size_t target = static_cast<std::size_t>(self->m_targetFrames) + frames;
        if (available > limit) {
            read += available - target;
            available = target;
        }

        // At most two contiguous runs: up to the end of the ring, then from its start.
        const std::size_t n = std::min<std::size_t>(frames, available);
        while (done < n) {
            const std::size_t slot = (read + done) % self->m_ringFrames;
            const std::size_t run = std::min(n - done, self->m_ringFrames - slot);
            // Volume and mute are applied by mpv before JACK.
            self->m_layer.audioMixer().process(self->m_ring.data() + slot * kMaxChannels, inChannels, kMaxChannels, 1,
                                               out + done * static_cast<std::size_t>(outChannels), outChannels, run, 1.f);
            done += run;
        }
        self->m_ringRead.store(read + done, std::memory_order_release);
    }
    if (done < frames) // underrun or nothing connected: silence
        std::fill_n(out + done * static_cast<std::size_t>(outChannels),
                    (frames - done) * static_cast<std::size_t>(outChannels), 0.f);

    // Peak for the audio level meter in the LayerView, only while the meter is shown.
    if (self->m_layer.audioLevelsEnabled()) {
        float peak = 0.f;
        const std::size_t total = static_cast<std::size_t>(frames) * static_cast<std::size_t>(outChannels);
        for (std::size_t i = 0; i < total; ++i)
            peak = std::max(peak, out[i] < 0.f ? -out[i] : out[i]);
        self->m_layer.reportAudioLevel(peak);
    }
    return paContinue;
}

} // namespace cplay::audio

#endif // CPLAY_JACK_AUDIO_INPUT
