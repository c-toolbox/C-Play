/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AUDIODECODER_H
#define AUDIODECODER_H

#include <QString>
#include <cstddef>
#include <cstdint>
#include <functional>

struct AVCodecContext;

// Decodes depacketized Opus payloads (as delivered by the WHEP/WebRTC source) into
// interleaved float32 PCM at 48 kHz. The callback receives one decoded frame at a
// time: `pcm` holds `frames * channels` samples in [-1, 1].
class AudioDecoder {
public:
    using FrameCallback = std::function<void(const float* pcm, int sampleRate, int channels, int frames)>;

    AudioDecoder();
    ~AudioDecoder();

    // Opens the Opus decoder. Returns false and fills `error` on failure.
    bool open(QString* error);
    void close();
    bool isOpen() const { return m_context != nullptr; }

    // Feeds one depacketized Opus payload. Decoded frames are delivered through the
    // frame callback (which may fire zero or more times per call). Returns false and
    // fills `error` on a fatal decode error, after which the context should be closed.
    bool decode(const std::uint8_t* data, size_t size, QString* error);

    void setOnFrame(FrameCallback cb) { m_onFrame = std::move(cb); }

private:
    // Drains all pending decoded frames through the frame callback.
    bool drainFrames(QString* error);

    AVCodecContext* m_context = nullptr;
    FrameCallback m_onFrame;
};

#endif // AUDIODECODER_H