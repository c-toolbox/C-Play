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
#include <libavutil/mem.h>
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

// Channel count requested on the decoder context. Some FFmpeg builds leave a decoded Opus
// frame's own channel layout unset (it reads back as 0), so this is used as a fallback: this
// class always configures the context for stereo output, which tells us how to interpret the
// per-channel sample count even when the frame carries no channel metadata of its own.
int contextChannels(const AVCodecContext* ctx) {
    if (!ctx) {
        return 0;
    }
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    return ctx->ch_layout.nb_channels;
#else
    return ctx->channels;
#endif
}

// Requests stereo output from the decoder so that mono streams are upmixed by Opus and
// true stereo keeps both channels.
void requestStereoOutput(AVCodecContext* ctx) {
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    av_channel_layout_default(&ctx->ch_layout, 2);
#else
    ctx->channels = 2;
#endif
}

// Builds a standard OpusHead (RFC 7845) with mapping family 0, so the built-in decoder never
// interprets packet data as a multistream configuration header. Layout per RFC 7845 section 4,
// little-endian fields; pre-skip and output gain stay zero.
std::vector<std::uint8_t> makeOpusHead(int channels, int sampleRate) {
    std::vector<std::uint8_t> head(21, 0);
    std::memcpy(head.data(), "OpusHead", 8); // magic string
    head[8] = 1;                             // version
    head[9] = static_cast<std::uint8_t>(channels); // channel count (mapping family 0: 1 or 2)
    const std::uint32_t rate = sampleRate > 0 ? static_cast<std::uint32_t>(sampleRate) : 48000;
    head[14] = static_cast<std::uint8_t>(rate & 0xFF); // input sample rate, little-endian
    head[15] = static_cast<std::uint8_t>((rate >> 8) & 0xFF);
    head[16] = static_cast<std::uint8_t>((rate >> 16) & 0xFF);
    head[17] = static_cast<std::uint8_t>((rate >> 24) & 0xFF);
    head[20] = 0; // mapping family: 0 selects the standard channel mapping
    return head;
}

// A little slack past the header so any over-reading parser stays inside allocated memory.
constexpr std::size_t kExtradataPadding = 16;

// Converts one decoded frame (planar or packed, float or s16) into interleaved float32 in [-1, 1].
// `channels` is supplied by the caller because some FFmpeg builds leave the per-frame channel
// layout unset; the caller resolves it (frame value with a context fallback).
bool convertToInterleavedFloat(const AVFrame& frame, int channels, std::vector<float>& out) {
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
    } else if (frame.format == AV_SAMPLE_FMT_FLT) {
        // Packed float: plane 0 already holds the interleaved samples.
        std::memcpy(out.data(), planes[0], out.size() * sizeof(float));
    } else if (frame.format == AV_SAMPLE_FMT_S16) {
        // Packed s16: the libopus wrapper emits this layout in some FFmpeg builds, so it must
        // be handled here or every frame would be silently dropped.
        const int16_t* samples = reinterpret_cast<const int16_t*>(planes[0]);
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = static_cast<float>(samples[i]) / 32768.f;
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

    // Prefer FFmpeg's external libopus wrapper when the build includes it. Otherwise use
    // the built-in native decoder, which is told via an explicit OpusHead (mapping family
    // 0) that the stream consists of raw RFC 6716 packets - not a multistream container.
    const AVCodec* codec = avcodec_find_decoder_by_name("libopus");
    const bool usingLibOpusWrapper = codec != nullptr;
    if (!codec) {
        codec = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    }
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
    requestStereoOutput(m_context);
    m_context->sample_rate = 48000;
    // Ask FFmpeg for planar float output; it inserts an internal resampler when the decoder's
    // native format differs (the libopus wrapper emits packed s16 in some builds). The manual
    // conversion below still copes with any other layout, so this is belt and braces.
    m_context->request_sample_fmt = AV_SAMPLE_FMT_FLTP;

    if (!usingLibOpusWrapper) {
        // Built-in decoder: without extradata it hunts for an in-band OpusHead and misreads the
        // first raw packet's TOC byte as a multichannel configuration header, then aborts. A
        // standard header with mapping family 0 tells it to treat the stream as plain RFC 6716.
        const std::vector<std::uint8_t> head = makeOpusHead(2, 48000);
        m_context->extradata = static_cast<uint8_t*>(av_mallocz(head.size() + kExtradataPadding));
        if (!m_context->extradata) {
            *error = u"Failed to allocate Opus header"_s;
            close();
            return false;
        }
        std::memcpy(m_context->extradata, head.data(), head.size());
        m_context->extradata_size = static_cast<int>(head.size());
    }

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
        // Resolve the channel count. Some FFmpeg builds leave a decoded Opus frame's own layout
        // unset (reads back as 0), which would make conversion drop every frame; fall back to the
        // layout requested on the context, and finally to stereo - this class always decodes to
        // two channels, so that is how the per-channel sample count must be interpreted.
        int channels = frameChannels(*frame);
        if (channels <= 0) {
            channels = contextChannels(m_context);
        }
        if (channels <= 0) {
            channels = 2;
        }
        // Same class of bug as the channel layout: some FFmpeg builds leave the decoded frame's
        // sample rate unset (reads back as 0). A zero/varying rate makes the PortAudio side reopen
        // the stream on every single frame, so resolve a stable rate from the context (48 kHz for Opus).
        int sampleRate = frame->sample_rate;
        if (sampleRate <= 0) {
            sampleRate = m_context ? m_context->sample_rate : 48000;
        }
        if (sampleRate <= 0) {
            sampleRate = 48000;
        }
        if (!convertToInterleavedFloat(*frame, channels, pcm)) {
            // Unsupported sample format or malformed frame: skip it but keep going.
            av_frame_unref(frame);
            continue;
        }

        if (m_onFrame) {
            m_onFrame(pcm.data(), sampleRate, channels, frame->nb_samples);
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

    // Some upstream encoders (observed on the WHEP/RTSP feeds we consume) append one undeclared
    // extra byte to their Opus RTP payloads, which makes two-CBR-frame packets structurally
    // invalid: opus_packet_parse_impl() rejects any odd length after the TOC byte. Try the full
    // packet first and fall back to dropping the trailing byte before giving up on a frame.
    if (decodePacket(data, size, error)) {
        return true;
    }
    if (size > 1) {
        QString retryError;
        if (decodePacket(data, size - 1, &retryError)) {
            ++m_trailingByteRetries;
            return true;
        }
    }
    // *error was already set by the first attempt.
    return false;
}

bool AudioDecoder::decodePacket(const uint8_t* data, size_t size, QString* error) {
    if (!m_context || !data || size == 0) {
        return false;
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