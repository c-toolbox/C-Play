/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_JACKAUDIOINPUT_H
#define CPLAY_JACKAUDIOINPUT_H

// Needs both the JACK client API and PortAudio output.
#if defined(JACK_SUPPORT) && defined(CPLAY_PORTAUDIO)
#define CPLAY_JACK_AUDIO_INPUT 1
#endif

#ifdef CPLAY_JACK_AUDIO_INPUT

#include "audio/audiomixer.h"
#include <array>
#include <atomic>
#include <chrono>
#include <jack/jack.h>
#include <portaudio.h>
#include <string>
#include <vector>

class BaseLayer;

namespace cplay::audio {

// Brings the audio of an mpv layer playing with ao=jack into the channel mapping of the live
// layers (standard up/down mix or spatial mapping, see audiomixer.h).
//
// A JACK client of its own with kMaxChannels input ports ("in_1".."in_8"). mpv is told to
// connect its output ports to them (--jack-port=portPattern()) instead of the system playback
// ports. The connected inputs are mixed with the layer's ChannelMixer and played on the
// PortAudio output device of the audio settings, like the other PortAudio layers.
//
// Threads: the constructor, update() and the destructor run on one control thread (the render
// loop). The JACK process callback writes into a lock-free ring buffer that the PortAudio
// callback reads. JACK and the output device run on their own clocks: the reader keeps the
// buffered latency bounded by skipping ahead when the writer runs faster.
class JackAudioInput {
public:
    JackAudioInput(BaseLayer &layer, std::string clientName);
    ~JackAudioInput();

    JackAudioInput(const JackAudioInput &) = delete;
    JackAudioInput &operator=(const JackAudioInput &) = delete;

    // Opens the JACK client when it is not open (retried every few seconds while the JACK
    // server is unavailable) and keeps the PortAudio output in line with the connected
    // channels, the output device and the layer's audio mapping. Call every frame. Returns
    // true when the JACK client was (re)opened by this call: the ports are new, so mpv has
    // to reconnect (ao-reload).
    bool update();
    // Re-check the output device and channel count on the next update() (audio settings or
    // the layer's audio mapping changed).
    void requestRecheck() { m_recheck.store(true); }

private:
    bool openClient();
    void closeClient();
    bool openOutput(PaDeviceIndex device, int outChannels);
    void closeOutput();

    static int jackProcess(jack_nframes_t frames, void *arg);
    static void jackShutdown(void *arg);
    static void jackPortConnect(jack_port_id_t a, jack_port_id_t b, int connect, void *arg);
    static int paCallback(const void *input, void *output, unsigned long frames,
                          const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags flags, void *userData);
    void countConnectedChannels();

    BaseLayer &m_layer;
    std::string m_clientName;

    // JACK (control thread, except where noted).
    jack_client_t *m_client = nullptr;
    std::array<jack_port_t *, kMaxChannels> m_ports{};
    // Lock-free single producer (JACK) / single consumer (PortAudio) ring of frames of
    // kMaxChannels interleaved floats. Positions count frames and only grow; the slot of a
    // frame is its position modulo m_ringFrames. (Not jack_ringbuffer: the Windows JACK
    // import library does not export it.)
    std::vector<float> m_ring;
    std::size_t m_ringFrames = 0;
    std::atomic<std::size_t> m_ringWrite{0};
    std::atomic<std::size_t> m_ringRead{0};
    int m_sampleRate = 0;
    int m_jackPeriod = 0;
    std::atomic<int> m_connectedChannels{0}; // highest connected input + 1 (notification thread writes)
    std::atomic<bool> m_serverGone{false};   // JACK shut the client down (notification thread writes)
    std::chrono::steady_clock::time_point m_lastOpenAttempt{};
    bool m_paInitialized = false;

    // PortAudio output.
    PaStream *m_stream = nullptr;
    PaDeviceIndex m_device = paNoDevice;
    int m_outChannels = 0;
    int m_openedInChannels = 0;  // connected channels the output was last checked for
    int m_targetFrames = 0;      // buffered frames kept after skipping ahead
    int m_limitFrames = 0;       // buffered frames that make the reader skip ahead
    std::atomic<bool> m_outputRunning{false}; // the JACK callback only writes while the output reads
    std::atomic<bool> m_recheck{true};
    std::chrono::steady_clock::time_point m_lastCheck{};
    std::chrono::steady_clock::time_point m_lastOutputFailure{};
};

} // namespace cplay::audio

#else

namespace cplay::audio {
// Without JACK or PortAudio: never created, complete only so owners can hold a unique_ptr.
class JackAudioInput {};
} // namespace cplay::audio

#endif // CPLAY_JACK_AUDIO_INPUT

#endif // CPLAY_JACKAUDIOINPUT_H
