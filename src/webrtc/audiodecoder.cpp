/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/audiodecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libavutil/version.h>
}

#include <cstring>
#include <vector>

using namespace Qt::Literals::StringLiterals;

namespace {

QString avErrorString(int ret) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(ret, errbuf, sizeof(errbuf));
    return QString::fromUtf8(errbuf);
}

// Number of channels in a decoded frame. Newer FFmpeg (>= 5.1) moved this to the
// AVChannelLayout struct; older builds still expose the plain `channels` field.
int frameChannels(const AVFrame& frame) {
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    return frame.ch_layout.nb_channels;
#else
    return frame.channels;
#endif
}

// Converts one planar decoded frame (S16P or FLTP) into interleaved float32 in [-1, 1].
bool convertToInterleavedFloat(const AVFrame& frame, std::vector<float>& out) {
    const int channels = frameChannels(frame);
    const int frames = frame.nb_samples;
    if (channels <= 0 || frames <= 0) {
        return false;
    }

    // AVFrame::data is a fixed array of pointers, which cannot be reinterpreted as a
    // pointer-to-pointer on MSVC; copy it out instead.
    const uint8_t *planes[AV_NUM_DATA_POINTERS];
    std::memcpy(planes, frame.data, sizeof(planes));

    out.resize(static_cast<size_t>(frames) * static_cast<size_t>(channels));

    if (frame.format == AV_SAMPLE_FMT_FLTP) {
        for (int c = 0; c < channels; ++c) {
            const float* plane = reinterpret_cast<const float*>(planes[c]);
            float* dst = out.data() + c;
            for (int i = 0; i < frames; ++i, dst += channels) {
                *dst = plane[i];
            }
        }
    } else if (frame.format == AV_SAMPLE_FMT_S16P) {
        for (int c = 0; c < channels; ++c) {
            const int16_t* plane = reinterpret_cast<const int16_t*>(planes[c]);
            float* dst = out.data() + c;
            for (int i = 0; i < frames; ++i, dst += channels) {
                *dst = static_cast<float>(plane[i]) / 32768.f;
            }
        }
    } else {
        return false; // unexpected sample format from the Opus decoder
    }

    return true;
}

} // namespace

AudioDecoder::AudioDecoder() = default;

AudioDecoder::~AudioDecoder() {
    close();
}

bool AudioDecoder::open(QString* error) {
    close();

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec) {
        *error = u"Opus decoder not found in FFmpeg build"_s;
        return false;
    }

    m_context = avcodec_alloc_context3(codec);
    if (!m_context) {
        *error = u"Failed to allocate Opus decoder context"_s;
        return false;
    }

    // Opus RTP timestamps are expressed in 48 kHz units.
    m_context->pkt_timebase = AVRational{1, 48000};

    const int ret = avcodec_open2(m_context, codec, nullptr);
    if (ret < 0) {
        *error = u"Failed to open Opus decoder: "_s + avErrorString(ret);
        close();
        return false;
    }

    return true;
}

void AudioDecoder::close() {
    if (m_context) {
        avcodec_free_context(&m_context);
    }
}

// Drains all pending decoded frames, delivering each through the frame callback.
// Returns false and fills `error` on a fatal decode error.
bool AudioDecoder::drainFrames(QString* error) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        *error = u"Failed to allocate output frame"_s;
        return false;
    }

    bool ok = true;
    while (true) {
        const int ret = avcodec_receive_frame(m_context, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // no more frames right now
        }
        if (ret < 0) {
            *error = u"Opus decode error: "_s + avErrorString(ret);
            ok = false;
            break;
        }

        std::vector<float> pcm;
        if (!convertToInterleavedFloat(*frame, pcm)) {
            // Unsupported sample format or malformed frame: skip it but keep going.
            av_frame_unref(frame);
            continue;
        }

        if (m_onFrame) {
            m_onFrame(pcm.data(), frame->sample_rate, frameChannels(*frame), frame->nb_samples);
        }
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    return ok;
}

bool AudioDecoder::decode(const uint8_t* data, size_t size, QString* error) {
    if (!m_context || !data || size == 0) {
        return true; // nothing to feed (not an error)
    }

    AVPacket pkt{};
    pkt.data = const_cast<uint8_t*>(data);
    pkt.size = static_cast<int>(size);

    int ret = avcodec_send_packet(m_context, &pkt);
    if (ret == AVERROR(EAGAIN)) {
        // Input buffer still busy: drain pending output first, then retry once.
        if (!drainFrames(error)) {
            return false;
        }
        ret = avcodec_send_packet(m_context, &pkt);
    }
    if (ret < 0) {
        *error = u"Failed to feed Opus decoder: "_s + avErrorString(ret);
        return false;
    }

    return drainFrames(error);
}