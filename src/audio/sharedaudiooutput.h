/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_SHAREDAUDIOOUTPUT_H
#define CPLAY_SHAREDAUDIOOUTPUT_H

#include <portaudio.h>

// Shared output for devices that only allow one stream per process (ASIO), that are opened
// exclusively (WASAPI exclusive mode), or for C-Play's own JACK client ("JACK" in the device
// list of the audio settings). Every PortAudio layer still opens its own output stream
// through the wrappers in portaudiolock.h, but for such an output it gets a virtual stream
// instead: one real stream is opened on the device (or one JACK client with an output port
// per channel, connected to the system playback ports) and the audio of all virtual streams
// is resampled to the device rate and summed into it.
//
// Callback streams are called from the device callback (in chunks of their framesPerBuffer),
// blocking streams (Pa_WriteStream) are buffered and block while their buffer is full, as a
// real PortAudio blocking stream does. Supported formats: paFloat32 and paInt16, interleaved.
//
// All functions except sharedWriteStream() must be called with portAudioMutex() held (the
// wrappers in portaudiolock.h do).
namespace cplay::audio {

// Device list entry (AudioSettings portAudioOutputApi/portAudioOutputDevice) for playing
// through C-Play's own JACK client instead of a PortAudio device.
inline constexpr const char *kJackOutputApi = "JACK";
inline constexpr const char *kJackOutputDevice = "C-Play JACK client";

// True when the audio settings select the JACK output (and JACK support is built in).
// The PortAudio device the layers pick then only stands in for it.
bool jackOutputSelected();
// Most output channels a stream on the device can have: the device's own, or every channel
// of the speaker mapping for the JACK output.
int outputMaxChannels(PaDeviceIndex device);

// True when output streams on the device go through the shared output.
bool shouldShareOutput(PaDeviceIndex device);
// True for a stream returned by sharedOpenStream() and not closed yet.
bool isSharedStream(PaStream *stream);
// True when the stream was opened with another routing than the device needs now (the
// device/host API, the WASAPI exclusive or JACK setting changed, or the JACK server went
// away): the layer must reopen it.
bool outputRoutingChanged(PaStream *stream, PaDeviceIndex device);

PaError sharedOpenStream(PaStream **stream, const PaStreamParameters *outputParameters, double sampleRate,
                         unsigned long framesPerBuffer, PaStreamCallback *streamCallback, void *userData);
PaError sharedCloseStream(PaStream *stream);
PaError sharedStartStream(PaStream *stream);
PaError sharedStopStream(PaStream *stream);
// Any thread, without portAudioMutex(): blocks while the stream's buffer is full.
PaError sharedWriteStream(PaStream *stream, const void *buffer, unsigned long frames);

} // namespace cplay::audio

#endif // CPLAY_SHAREDAUDIOOUTPUT_H
