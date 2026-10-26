/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio/sharedaudiooutput.h"

#ifdef CPLAY_PORTAUDIO

#include "audio/audiomixer.h"
#include "audiosettings.h"
#include <sgct/sgct.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <format>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>
#if defined(_WIN32) && __has_include(<pa_win_wasapi.h>)
#include <pa_win_wasapi.h>
#define CPLAY_PA_WASAPI 1
#endif
#ifdef JACK_SUPPORT
#include <jack/jack.h>
#endif

namespace cplay::audio {

namespace {

constexpr int kMaxClients = 32;
// The device callback has not run for this long: blocking writes drop their audio instead
// of waiting (device removed or stopped), so the layers' threads do not hang.
constexpr auto kStallTimeout = std::chrono::milliseconds(200);
constexpr auto kWriteWait = std::chrono::milliseconds(5);
constexpr auto kIdleTimeout = std::chrono::milliseconds(500);
// Device callbacks are assumed to ask for at most this many frames (sizes the FIFOs).
constexpr std::size_t kMaxDeviceFrames = 8192;
constexpr std::size_t kDefaultChunkFrames = 4096;
// Key of the JACK output in engines() (not a PortAudio device index).
constexpr PaDeviceIndex kJackEngineKey = -100;
constexpr const char *kJackClientName = "cplay-out";

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Catmull-Rom interpolation between y1 and y2.
inline float hermite(float y0, float y1, float y2, float y3, float t) {
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

void toFloat(const void *src, PaSampleFormat format, std::size_t samples, float *dst) {
    if (format == paInt16) {
        const auto *in = static_cast<const int16_t *>(src);
        for (std::size_t i = 0; i < samples; ++i)
            dst[i] = static_cast<float>(in[i]) * (1.f / 32768.f);
    } else {
        std::copy_n(static_cast<const float *>(src), samples, dst);
    }
}

std::size_t sampleBytes(PaSampleFormat format) {
    return format == paInt16 ? sizeof(int16_t) : sizeof(float);
}

class Engine;

// The output stream of one layer, mixed into the device stream of an Engine.
struct VirtualStream {
    Engine *engine = nullptr;
    int channels = 0;
    PaSampleFormat format = paFloat32;
    double sampleRate = 0.0;
    unsigned long framesPerBuffer = 0;
    PaStreamCallback *callback = nullptr; // nullptr: blocking stream (Pa_WriteStream)
    void *userData = nullptr;

    // Interleaved float FIFO at the stream's own rate. Blocking streams: written by the
    // layer, read by the device callback. Callback streams: both on the device callback.
    std::vector<float> fifo;
    std::size_t fifoFrames = 0;
    std::atomic<std::size_t> writePos{0};
    std::atomic<std::size_t> readPos{0};

    // Callback streams: one chunk from the layer callback, in the layer's format.
    std::vector<unsigned char> scratch;
    std::size_t scratchFrames = 0;

    std::atomic<bool> running{false};
    std::atomic<bool> finished{false}; // the layer callback returned paComplete/paAbort

    // Device callback only: resampler from the stream rate to the device rate.
    double step = 1.0; // stream frames per device frame
    double phase = 1.0;
    std::array<float, 4 * kMaxOutputChannels> window{}; // the last 4 stream frames
    int windowHead = 0;                                  // oldest of them

    // Blocking writes wait here for the device callback to free space.
    std::mutex waitMutex;
    std::condition_variable spaceCv;
    std::atomic<bool> writerWaiting{false};
    // Writers inside sharedWriteStream; close waits for zero before deleting the stream.
    std::atomic<int> activeWriters{0};

    // Only while not running: the device callback skips the stream then.
    void reset() {
        writePos.store(0);
        readPos.store(0);
        phase = 1.0;
        window.fill(0.f);
        windowHead = 0;
        finished.store(false);
    }

    void pushFrames(const void *src, std::size_t frames, std::size_t write) {
        // At most two contiguous runs: up to the end of the FIFO, then from its start.
        const auto *bytes = static_cast<const unsigned char *>(src);
        const std::size_t frameBytes = static_cast<std::size_t>(channels) * sampleBytes(format);
        std::size_t done = 0;
        while (done < frames) {
            const std::size_t slot = (write + done) % fifoFrames;
            const std::size_t run = std::min(frames - done, fifoFrames - slot);
            toFloat(bytes + done * frameBytes, format, run * static_cast<std::size_t>(channels),
                    fifo.data() + slot * static_cast<std::size_t>(channels));
            done += run;
        }
    }

    // Device callback: pulls from the layer callback until this buffer can be rendered.
    void refill(unsigned long frames) {
        if (finished.load(std::memory_order_relaxed))
            return;
        std::size_t write = writePos.load(std::memory_order_relaxed);
        const std::size_t read = readPos.load(std::memory_order_relaxed);
        const auto needed = static_cast<std::size_t>(std::ceil(phase + static_cast<double>(frames) * step));
        while (write - read < needed) {
            std::size_t chunk = framesPerBuffer > 0 ? framesPerBuffer
                                                    : std::max<std::size_t>(needed - (write - read), 64);
            chunk = std::min(chunk, scratchFrames);
            if (fifoFrames - (write - read) < chunk)
                break;
            PaStreamCallbackTimeInfo timeInfo{};
            const int result = callback(nullptr, scratch.data(), static_cast<unsigned long>(chunk), &timeInfo, 0, userData);
            pushFrames(scratch.data(), chunk, write);
            write += chunk;
            if (result != paContinue) {
                finished.store(true, std::memory_order_relaxed);
                break;
            }
        }
        writePos.store(write, std::memory_order_release);
    }

    // Device callback: resamples and adds this stream to the device buffer.
    void render(float *out, int outChannels, unsigned long frames) {
        if (callback)
            refill(frames);

        const auto ch = static_cast<std::size_t>(channels);
        const int mapped = std::min(channels, outChannels);
        std::size_t read = readPos.load(std::memory_order_relaxed);
        const std::size_t write = writePos.load(std::memory_order_acquire);
        for (unsigned long f = 0; f < frames; ++f) {
            bool underrun = false;
            while (phase >= 1.0) {
                if (read == write) {
                    underrun = true; // the rest of the buffer stays silent for this stream
                    break;
                }
                std::copy_n(fifo.data() + (read % fifoFrames) * ch, ch, window.data() + windowHead * ch);
                windowHead = (windowHead + 1) & 3;
                ++read;
                phase -= 1.0;
            }
            if (underrun)
                break;
            const float *y0 = window.data() + static_cast<std::size_t>(windowHead) * ch;
            const float *y1 = window.data() + static_cast<std::size_t>((windowHead + 1) & 3) * ch;
            const float *y2 = window.data() + static_cast<std::size_t>((windowHead + 2) & 3) * ch;
            const float *y3 = window.data() + static_cast<std::size_t>((windowHead + 3) & 3) * ch;
            const auto t = static_cast<float>(phase);
            float *o = out + static_cast<std::size_t>(f) * static_cast<std::size_t>(outChannels);
            for (int c = 0; c < mapped; ++c)
                o[c] += hermite(y0[c], y1[c], y2[c], y3[c], t);
            phase += step;
        }
        readPos.store(read, std::memory_order_release);
        if (!callback && writerWaiting.load(std::memory_order_relaxed))
            spaceCv.notify_one();
    }
};

// One real output stream on a device (or the JACK client), mixing all virtual streams
// opened on it.
class Engine {
public:
    Engine(PaDeviceIndex device, bool jack) : m_device(device), m_jack(jack) {}

    bool open(int wantChannels, double wantRate);
    void close();
    // Makes room for a stream with this many channels: the JACK client registers more
    // ports, a device stream has the channels it was opened with.
    bool reserveChannels(int channels);
    bool add(VirtualStream *stream);
    void remove(VirtualStream *stream);
    // Returns once the device callback no longer uses a stream that was just removed/stopped.
    void waitIdle() const;
    bool stalled() const { return nowMs() - m_lastCallbackMs.load(std::memory_order_relaxed) > kStallTimeout.count(); }

    bool isJack() const { return m_jack; }
    // The JACK server shut the client down: the streams on it must be reopened.
    bool dead() const { return m_dead.load(); }
    double sampleRate() const { return m_sampleRate; }
    double outputLatency() const { return m_outputLatency; }
    int clientCount() const { return m_clientCount; }

private:
    bool openDevice(int wantChannels, double wantRate);
    // Device callback (any backend): renders all running streams into out.
    void mix(float *out, int outChannels, unsigned long frames);
    static int deviceCallback(const void *, void *output, unsigned long frames, const PaStreamCallbackTimeInfo *,
                              PaStreamCallbackFlags, void *userData);

    PaDeviceIndex m_device = paNoDevice;
    const bool m_jack = false;
    PaStream *m_stream = nullptr;
    bool m_paInitialized = false;
    int m_channels = 0;
    double m_sampleRate = 0.0;
    double m_outputLatency = 0.0;
    int m_clientCount = 0;
    std::array<std::atomic<VirtualStream *>, kMaxClients> m_slots{};
    std::atomic<bool> m_inCallback{false};
    std::atomic<int64_t> m_lastCallbackMs{0};
    std::atomic<bool> m_dead{false};

#ifdef JACK_SUPPORT
    bool openJack(int wantChannels);
    static int jackProcess(jack_nframes_t frames, void *arg);
    static void jackShutdown(void *arg);

    jack_client_t *m_client = nullptr;
    std::array<jack_port_t *, kMaxOutputChannels> m_ports{};
    std::atomic<int> m_portCount{0}; // ports [0, count) are registered; read by the process callback
    std::vector<float> m_jackBuffer; // interleaved mix, de-interleaved into the ports
#endif
};

bool Engine::open(int wantChannels, double wantRate) {
#ifdef JACK_SUPPORT
    if (m_jack)
        return openJack(wantChannels);
#endif
    return !m_jack && openDevice(wantChannels, wantRate);
}

bool Engine::reserveChannels(int channels) {
#ifdef JACK_SUPPORT
    if (m_jack) {
        if (!m_client || m_dead.load())
            return false;
        channels = std::min(channels, kMaxOutputChannels);
        const int have = m_portCount.load();
        if (channels <= have)
            return true;
        // New ports are connected to the physical playback port of the same channel, as mpv
        // does by default; they can be rewired in any JACK patchbay.
        const char **physical = jack_get_ports(m_client, nullptr, JACK_DEFAULT_AUDIO_TYPE,
                                               JackPortIsPhysical | JackPortIsInput);
        int physicalCount = 0;
        while (physical && physical[physicalCount])
            ++physicalCount;
        for (int i = have; i < channels; ++i) {
            const std::string name = "out_" + std::to_string(i + 1);
            jack_port_t *port = jack_port_register(m_client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
            if (!port) {
                sgct::Log::Error(std::format("SharedAudioOutput: could not register the JACK port {}:{}",
                                             kJackClientName, name));
                break;
            }
            m_ports[static_cast<std::size_t>(i)] = port;
            m_portCount.store(i + 1, std::memory_order_release);
            if (i < physicalCount)
                jack_connect(m_client, jack_port_name(port), physical[i]);
        }
        if (physical)
            jack_free(physical);
        return m_portCount.load() >= channels;
    }
#endif
    return channels <= m_channels;
}

#ifdef JACK_SUPPORT
bool Engine::openJack(int wantChannels) {
    jack_status_t status{};
    m_client = jack_client_open(kJackClientName, JackNoStartServer, &status);
    if (!m_client) {
        sgct::Log::Error("SharedAudioOutput: could not open the JACK output client (is the JACK server running?).");
        return false;
    }
    m_sampleRate = static_cast<double>(jack_get_sample_rate(m_client));
    m_outputLatency = 2.0 * static_cast<double>(jack_get_buffer_size(m_client)) / m_sampleRate;
    m_jackBuffer.assign(kMaxDeviceFrames * kMaxOutputChannels, 0.f);
    jack_set_process_callback(m_client, &Engine::jackProcess, this);
    jack_on_shutdown(m_client, &Engine::jackShutdown, this);
    m_lastCallbackMs.store(nowMs());
    if (jack_activate(m_client) != 0) {
        sgct::Log::Error("SharedAudioOutput: could not activate the JACK output client.");
        jack_client_close(m_client);
        m_client = nullptr;
        return false;
    }
    // The configured layout (or speaker array) at once, so the port set stays stable.
    reserveChannels(std::max(wantChannels, configuredOutputChannels(kMaxOutputChannels)));
    sgct::Log::Info(std::format("SharedAudioOutput: JACK client \"{}\" open with {} output port(s) at {} Hz. "
                                "All PortAudio layers are mixed into it.", jack_get_client_name(m_client),
                                m_portCount.load(), m_sampleRate));
    return m_portCount.load() > 0;
}

int Engine::jackProcess(jack_nframes_t frames, void *arg) {
    // JACK real-time thread: no locks, no allocation.
    auto *self = static_cast<Engine *>(arg);
    const int channels = self->m_portCount.load(std::memory_order_acquire);
    if (channels <= 0)
        return 0;
    std::array<float *, kMaxOutputChannels> ports{};
    for (int c = 0; c < channels; ++c)
        ports[static_cast<std::size_t>(c)] = static_cast<float *>(
            jack_port_get_buffer(self->m_ports[static_cast<std::size_t>(c)], frames));

    std::size_t done = 0;
    while (done < frames) {
        const std::size_t n = std::min<std::size_t>(frames - done, kMaxDeviceFrames);
        float *mixed = self->m_jackBuffer.data();
        self->mix(mixed, channels, static_cast<unsigned long>(n));
        for (int c = 0; c < channels; ++c) {
            float *port = ports[static_cast<std::size_t>(c)] + done;
            for (std::size_t f = 0; f < n; ++f)
                port[f] = std::clamp(mixed[f * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)], -1.f, 1.f);
        }
        done += n;
    }
    return 0;
}

void Engine::jackShutdown(void *arg) {
    // JACK notification thread: the client is unusable, but still closed in close().
    auto *self = static_cast<Engine *>(arg);
    self->m_dead.store(true);
}
#endif

bool Engine::openDevice(int wantChannels, double wantRate) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(m_device);
    const PaHostApiInfo *api = info ? Pa_GetHostApiInfo(info->hostApi) : nullptr;
    if (!info || !api)
        return false;

    // Own reference, so a layer terminating PortAudio cannot close the device stream under us.
    if (Pa_Initialize() != paNoError)
        return false;
    m_paInitialized = true;

    const int maxChannels = std::min(info->maxOutputChannels, kMaxOutputChannels);
    // All channels of the device, so layers with any channel count can share the stream, and
    // preferably the rate of the first layer, so it plays without resampling.
    const std::array<int, 3> channelChoices{maxChannels, wantChannels, 2};
    const std::array<double, 4> rateChoices{wantRate, info->defaultSampleRate, 48000.0, 44100.0};

    PaStreamParameters params{};
    params.device = m_device;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = info->defaultLowOutputLatency;

    std::vector<bool> exclusivePasses{false};
#ifdef CPLAY_PA_WASAPI
    PaWasapiStreamInfo wasapi{};
    wasapi.size = sizeof(PaWasapiStreamInfo);
    wasapi.hostApiType = paWASAPI;
    wasapi.version = 1;
    wasapi.flags = paWinWasapiExclusive;
    if (api->type == paWASAPI) {
        exclusivePasses = {true, false}; // shared mode if the device cannot be opened exclusively
        params.suggestedLatency = std::max(info->defaultLowOutputLatency, 0.01);
    }
#endif

    for (const bool exclusive : exclusivePasses) {
#ifdef CPLAY_PA_WASAPI
        params.hostApiSpecificStreamInfo = exclusive ? &wasapi : nullptr;
#endif
        for (const int ch : channelChoices) {
            if (ch < 1 || ch > maxChannels)
                continue;
            params.channelCount = ch;
            for (const double rate : rateChoices) {
                if (rate <= 0.0 || Pa_IsFormatSupported(nullptr, &params, rate) != paFormatIsSupported)
                    continue;
                PaError err = Pa_OpenStream(&m_stream, nullptr, &params, rate, paFramesPerBufferUnspecified, paNoFlag,
                                            &Engine::deviceCallback, this);
                if (err != paNoError) {
                    m_stream = nullptr;
                    continue;
                }
                m_lastCallbackMs.store(nowMs());
                err = Pa_StartStream(m_stream);
                if (err != paNoError) {
                    Pa_CloseStream(m_stream);
                    m_stream = nullptr;
                    continue;
                }
                m_channels = ch;
                m_sampleRate = rate;
                const PaStreamInfo *streamInfo = Pa_GetStreamInfo(m_stream);
                m_outputLatency = streamInfo ? streamInfo->outputLatency : params.suggestedLatency;
                if (exclusivePasses.size() > 1 && !exclusive)
                    sgct::Log::Warning(std::format("SharedAudioOutput: \"{}\" could not be opened in WASAPI exclusive "
                                                   "mode, using shared mode.", info->name));
                sgct::Log::Info(std::format("SharedAudioOutput: \"{}\" ({}{}) open with {} channel(s) at {} Hz, {:.1f} ms. "
                                            "All PortAudio layers are mixed into it.", info->name, api->name,
                                            exclusive ? ", exclusive" : "", ch, rate, m_outputLatency * 1000.0));
                return true;
            }
        }
    }

    sgct::Log::Error(std::format("SharedAudioOutput: could not open \"{}\" ({}).", info->name, api->name));
    Pa_Terminate();
    m_paInitialized = false;
    return false;
}

void Engine::close() {
#ifdef JACK_SUPPORT
    if (m_client) {
        if (!m_dead.load())
            jack_deactivate(m_client);
        jack_client_close(m_client); // also unregisters the ports
        m_client = nullptr;
        m_portCount.store(0);
        sgct::Log::Info("SharedAudioOutput: JACK output client closed.");
    }
#endif
    if (m_stream) {
        if (Pa_StopStream(m_stream) != paNoError)
            Pa_AbortStream(m_stream);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        sgct::Log::Info("SharedAudioOutput: device stream closed.");
    }
    if (m_paInitialized) {
        Pa_Terminate();
        m_paInitialized = false;
    }
}

bool Engine::add(VirtualStream *stream) {
    for (auto &slot : m_slots) {
        if (!slot.load()) {
            slot.store(stream);
            ++m_clientCount;
            return true;
        }
    }
    return false;
}

void Engine::remove(VirtualStream *stream) {
    for (auto &slot : m_slots) {
        if (slot.load() == stream) {
            slot.store(nullptr);
            --m_clientCount;
        }
    }
    waitIdle();
}

void Engine::waitIdle() const {
    // Sequentially consistent with the callback: it sets m_inCallback before it reads the
    // slots and running flags, so once it is seen clear the change is visible to every later
    // callback.
    const auto deadline = std::chrono::steady_clock::now() + kIdleTimeout;
    while (m_inCallback.load()) {
        if (std::chrono::steady_clock::now() > deadline) {
            sgct::Log::Warning("SharedAudioOutput: the device callback did not return in time.");
            return;
        }
        std::this_thread::yield();
    }
}

int Engine::deviceCallback(const void *, void *output, unsigned long frames, const PaStreamCallbackTimeInfo *,
                           PaStreamCallbackFlags, void *userData) {
    // PortAudio real-time thread: no locks, no allocation.
    auto *self = static_cast<Engine *>(userData);
    self->mix(static_cast<float *>(output), self->m_channels, frames);
    return paContinue; // the sum is clipped by PortAudio (opened without paClipOff)
}

void Engine::mix(float *out, int outChannels, unsigned long frames) {
    m_inCallback.store(true);
    std::fill_n(out, static_cast<std::size_t>(frames) * static_cast<std::size_t>(outChannels), 0.f);
    for (auto &slot : m_slots) {
        VirtualStream *stream = slot.load();
        if (stream && stream->running.load())
            stream->render(out, outChannels, frames);
    }
    m_lastCallbackMs.store(nowMs(), std::memory_order_relaxed);
    m_inCallback.store(false);
}

// Engines by device; guarded by portAudioMutex(). Never destroyed, so nothing touches
// PortAudio during static destruction.
std::map<PaDeviceIndex, std::unique_ptr<Engine>> &engines() {
    static auto *map = new std::map<PaDeviceIndex, std::unique_ptr<Engine>>();
    return *map;
}

// Engines that died (JACK server shutdown) while streams were still open on them: no new
// streams are opened on them, they are closed with their last stream. Guarded as engines().
std::vector<std::unique_ptr<Engine>> &retiredEngines() {
    static auto *list = new std::vector<std::unique_ptr<Engine>>();
    return *list;
}

// Open virtual streams; read without portAudioMutex() by the write path.
std::mutex g_registryMutex;
std::unordered_set<PaStream *> g_registry;

VirtualStream *asVirtual(PaStream *stream) {
    std::lock_guard<std::mutex> lock(g_registryMutex);
    return g_registry.contains(stream) ? static_cast<VirtualStream *>(stream) : nullptr;
}

} // namespace

bool jackOutputSelected() {
#ifdef JACK_SUPPORT
    return AudioSettings::portAudioCustomOutput()
           && AudioSettings::portAudioOutputApi() == QLatin1String(kJackOutputApi);
#else
    return false;
#endif
}

int outputMaxChannels(PaDeviceIndex device) {
    if (jackOutputSelected())
        return kMaxOutputChannels;
    const PaDeviceInfo *info = device != paNoDevice ? Pa_GetDeviceInfo(device) : nullptr;
    return info ? info->maxOutputChannels : 0;
}

bool shouldShareOutput(PaDeviceIndex device) {
    if (jackOutputSelected())
        return true; // every output goes to the JACK client, whatever device stands in for it
    const PaDeviceInfo *info = device != paNoDevice ? Pa_GetDeviceInfo(device) : nullptr;
    const PaHostApiInfo *api = info ? Pa_GetHostApiInfo(info->hostApi) : nullptr;
    if (!api || info->maxOutputChannels <= 0)
        return false;
    if (api->type == paASIO)
        return true; // the ASIO host API only allows one open stream per process
#ifdef CPLAY_PA_WASAPI
    if (api->type == paWASAPI)
        return AudioSettings::portAudioCustomOutput() && AudioSettings::portAudioWasapiExclusive();
#endif
    return false;
}

bool isSharedStream(PaStream *stream) {
    return stream && asVirtual(stream) != nullptr;
}

bool outputRoutingChanged(PaStream *stream, PaDeviceIndex device) {
    if (!stream)
        return false;
    // The stream is this layer's own and open, so its engine stays alive during the check.
    const VirtualStream *virt = asVirtual(stream);
    const bool share = shouldShareOutput(device);
    if (!virt || !share)
        return (virt != nullptr) != share;
    return virt->engine->isJack() != jackOutputSelected() || virt->engine->dead();
}

PaError sharedOpenStream(PaStream **stream, const PaStreamParameters *outputParameters, double sampleRate,
                         unsigned long framesPerBuffer, PaStreamCallback *streamCallback, void *userData) {
    if (!stream)
        return paBadStreamPtr;
    if (!outputParameters)
        return paInvalidDevice;
    const PaSampleFormat format = outputParameters->sampleFormat;
    if (format != paFloat32 && format != paInt16)
        return paSampleFormatNotSupported;
    const bool jack = jackOutputSelected();
    if (!jack && !Pa_GetDeviceInfo(outputParameters->device))
        return paInvalidDevice;
    const int channels = outputParameters->channelCount;
    if (channels < 1 || channels > std::min(outputMaxChannels(outputParameters->device), kMaxOutputChannels))
        return paInvalidChannelCount;
    if (sampleRate <= 0.0)
        return paInvalidSampleRate;

    auto &map = engines();
    const PaDeviceIndex key = jack ? kJackEngineKey : outputParameters->device;
    auto it = map.find(key);
    if (it != map.end() && it->second->dead()) {
        // Keep it for the streams still open on it, and start over with a new one.
        retiredEngines().push_back(std::move(it->second));
        map.erase(it);
        it = map.end();
    }
    if (it == map.end()) {
        auto engine = std::make_unique<Engine>(outputParameters->device, jack);
        if (!engine->open(channels, sampleRate)) {
            engine->close();
            return paDeviceUnavailable;
        }
        it = map.emplace(key, std::move(engine)).first;
    }
    Engine *engine = it->second.get();
    const auto closeUnusedEngine = [&] {
        if (engine->clientCount() == 0) {
            engine->close();
            map.erase(it);
        }
    };
    if (!engine->reserveChannels(channels)) {
        closeUnusedEngine();
        return paInvalidChannelCount;
    }

    auto virt = std::make_unique<VirtualStream>();
    virt->engine = engine;
    virt->channels = channels;
    virt->format = format;
    virt->sampleRate = sampleRate;
    virt->framesPerBuffer = framesPerBuffer == paFramesPerBufferUnspecified ? 0 : framesPerBuffer;
    virt->callback = streamCallback;
    virt->userData = userData;
    virt->step = std::clamp(sampleRate / engine->sampleRate(), 0.125, 8.0);

    const auto ch = static_cast<std::size_t>(channels);
    if (streamCallback) {
        virt->scratchFrames = virt->framesPerBuffer > 0 ? virt->framesPerBuffer : kDefaultChunkFrames;
        virt->scratch.resize(virt->scratchFrames * ch * sampleBytes(format));
        virt->fifoFrames = virt->scratchFrames + static_cast<std::size_t>(std::ceil(kMaxDeviceFrames * virt->step)) + 64;
    } else {
        // Like the buffer of a real blocking stream: Pa_WriteStream blocks while it is full.
        const double latency = std::max({outputParameters->suggestedLatency, engine->outputLatency(), 0.03});
        virt->fifoFrames = std::max<std::size_t>(static_cast<std::size_t>(latency * sampleRate), 256);
    }
    virt->fifo.assign(virt->fifoFrames * ch, 0.f);

    if (!engine->add(virt.get())) {
        sgct::Log::Error("SharedAudioOutput: too many layer streams on one device.");
        closeUnusedEngine();
        return paInsufficientMemory;
    }
    {
        std::lock_guard<std::mutex> lock(g_registryMutex);
        g_registry.insert(virt.get());
    }
    *stream = virt.release();
    return paNoError;
}

PaError sharedStartStream(PaStream *stream) {
    VirtualStream *virt = asVirtual(stream);
    if (!virt)
        return paBadStreamPtr;
    if (virt->running.load())
        return paStreamIsNotStopped;
    virt->reset();
    virt->running.store(true);
    return paNoError;
}

PaError sharedStopStream(PaStream *stream) {
    VirtualStream *virt = asVirtual(stream);
    if (!virt)
        return paBadStreamPtr;
    if (!virt->running.load())
        return paStreamIsStopped;
    virt->running.store(false);
    virt->spaceCv.notify_all();
    virt->engine->waitIdle();
    return paNoError;
}

PaError sharedCloseStream(PaStream *stream) {
    VirtualStream *virt = asVirtual(stream);
    if (!virt)
        return paBadStreamPtr;
    virt->running.store(false);
    virt->spaceCv.notify_all();
    {
        std::lock_guard<std::mutex> lock(g_registryMutex);
        g_registry.erase(stream);
    }

    Engine *engine = virt->engine;
    engine->remove(virt);
    // No new writer can find the stream now; wait for those still inside sharedWriteStream.
    while (virt->activeWriters.load(std::memory_order_acquire) > 0) {
        virt->spaceCv.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    delete virt;

    if (engine->clientCount() == 0) {
        engine->close();
        auto &map = engines();
        auto &retired = retiredEngines();
        std::erase_if(map, [engine](const auto &entry) { return entry.second.get() == engine; });
        std::erase_if(retired, [engine](const auto &entry) { return entry.get() == engine; });
    }
    return paNoError;
}

PaError sharedWriteStream(PaStream *stream, const void *buffer, unsigned long frames) {
    VirtualStream *virt = nullptr;
    {
        // Register as a writer under the registry lock so close cannot delete the stream meanwhile.
        std::lock_guard<std::mutex> lock(g_registryMutex);
        if (g_registry.contains(stream)) {
            virt = static_cast<VirtualStream *>(stream);
            virt->activeWriters.fetch_add(1, std::memory_order_acq_rel);
        }
    }
    if (!virt)
        return paBadStreamPtr;
    struct WriterGuard {
        VirtualStream *v;
        ~WriterGuard() { v->activeWriters.fetch_sub(1, std::memory_order_acq_rel); }
    } writerGuard{virt};
    if (virt->callback)
        return paCanNotWriteToACallbackStream;

    const std::size_t frameBytes = static_cast<std::size_t>(virt->channels) * sampleBytes(virt->format);
    const auto *bytes = static_cast<const unsigned char *>(buffer);
    std::size_t done = 0;
    while (done < frames) {
        if (!virt->running.load())
            return paStreamIsStopped;
        const std::size_t write = virt->writePos.load(std::memory_order_relaxed);
        const std::size_t read = virt->readPos.load(std::memory_order_acquire);
        const std::size_t space = virt->fifoFrames - (write - read);
        if (space == 0) {
            if (virt->engine->stalled())
                return paNoError; // the device is not pulling audio: drop instead of hanging the layer
            virt->writerWaiting.store(true);
            {
                std::unique_lock<std::mutex> lock(virt->waitMutex);
                virt->spaceCv.wait_for(lock, kWriteWait);
            }
            virt->writerWaiting.store(false);
            continue;
        }
        const std::size_t n = std::min<std::size_t>(space, frames - done);
        virt->pushFrames(bytes + done * frameBytes, n, write);
        virt->writePos.store(write + n, std::memory_order_release);
        done += n;
    }
    return paNoError;
}

} // namespace cplay::audio

#endif // CPLAY_PORTAUDIO
