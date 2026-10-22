/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_SHAREDAUDIOOUTPUT_H
#define CPLAY_SHAREDAUDIOOUTPUT_H

#include <portaudio.h>

// Shared output for devices that only allow one stream per process (ASIO), or that are
// opened exclusively (WASAPI exclusive mode). Every PortAudio layer still opens its own
// output stream through the wrappers in portaudiolock.h, but for such a device it gets a
// virtual stream instead: one real stream is opened on the device and the audio of all
// virtual streams is resampled to the device rate and summed into it.
//
// Callback streams are called from the device callback (in chunks of their framesPerBuffer),
// blocking streams (Pa_WriteStream) are buffered and block while their buffer is full, as a
// real PortAudio blocking stream does. Supported formats: paFloat32 and paInt16, interleaved.
//
// All functions except sharedWriteStream() must be called with portAudioMutex() held (the
// wrappers in portaudiolock.h do).
namespace cplay::audio {

// True when output streams on the device go through the shared output.
bool shouldShareOutput(PaDeviceIndex device);
// True for a stream returned by sharedOpenStream() and not closed yet.
bool isSharedStream(PaStream *stream);
// True when the stream was opened with the other routing than the device needs now (the
// device/host API or the WASAPI exclusive setting changed): the layer must reopen it.
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
