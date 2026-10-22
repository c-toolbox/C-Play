/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PORTAUDIOLOCK_H
#define PORTAUDIOLOCK_H

#include <mutex>
#include <portaudio.h>

namespace cplay::audio {

// PortAudio's ASIO host API keeps a single process-wide driver (theAsioDriver), and
// Pa_Initialize/Pa_Terminate/Pa_OpenStream/Pa_CloseStream are not thread-safe. All
// layers that open/close streams must serialize through this lock. It is recursive so
// nested helpers (e.g. StartAudioStream -> start -> Pa_StartStream) can re-acquire it.
// Do NOT take it inside a PortAudio stream callback.
inline std::recursive_mutex& portAudioMutex() {
    static std::recursive_mutex m;
    return m;
}

// Locked wrappers for code that does not otherwise hold portAudioMutex().
inline PaError paInitialize() {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_Initialize();
}

inline PaError paTerminate() {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_Terminate();
}

inline PaError paOpenStream(PaStream** stream, const PaStreamParameters* inputParameters,
    const PaStreamParameters* outputParameters, double sampleRate, unsigned long framesPerBuffer,
    PaStreamFlags streamFlags, PaStreamCallback* streamCallback, void* userData) {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_OpenStream(stream, inputParameters, outputParameters, sampleRate, framesPerBuffer,
        streamFlags, streamCallback, userData);
}

inline PaError paCloseStream(PaStream* stream) {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_CloseStream(stream);
}

inline PaError paStartStream(PaStream* stream) {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_StartStream(stream);
}

inline PaError paStopStream(PaStream* stream) {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_StopStream(stream);
}

inline PaError paAbortStream(PaStream* stream) {
    std::lock_guard<std::recursive_mutex> lock(portAudioMutex());
    return Pa_AbortStream(stream);
}

} // namespace cplay::audio

#endif // PORTAUDIOLOCK_H
