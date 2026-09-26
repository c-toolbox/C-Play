/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/videodecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

#include <cstring>
#include <vector>

using namespace Qt::Literals::StringLiterals;

namespace {

QString avErrorString(int code)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

/// Process-wide CUDA device shared by every decoder: one context per process
/// rather than per stream keeps GPU memory and context-switch overhead down.
AVBufferRef *getCudaDevice()
{
    static AVBufferRef *device = nullptr;
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        const int err = av_hwdevice_ctx_create(&device, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
        if (err < 0) {
            device = nullptr;
        }
    }
    return device;
}

AVCodecID toAvCodecId(WebRtcVideoCodec codec)
{
    return codec == WebRtcVideoCodec::H265 ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
}

/// get_format callback: pick the pinned hardware surface format when offered,
/// otherwise fall back to the first software format so decoding stays alive.
AVPixelFormat selectHwFormat(AVCodecContext *ctx, const AVPixelFormat *formats)
{
    auto *self = static_cast<VideoDecoder *>(ctx->opaque);
    for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; ++format) {
        if (*format == self->hwPixelFormat()) {
            return *format;
        }
    }
    return formats[0];
}

} // namespace

void VideoDecoder::AVCodecContextDeleter::operator()(AVCodecContext *context) const
{
    avcodec_free_context(&context);
}

void VideoDecoder::AVFrameDeleter::operator()(AVFrame *frame) const
{
    av_frame_free(&frame);
}

void VideoDecoder::AVPacketDeleter::operator()(AVPacket *packet) const
{
    av_packet_free(&packet);
}

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder()
{
    close();
}

bool VideoDecoder::open(WebRtcVideoCodec codec, QString *error)
{
    close();

    if (codec == WebRtcVideoCodec::Unknown) {
        if (error) {
            *error = u"Cannot open a decoder for an unknown codec"_s;
        }
        return false;
    }

    const AVCodecID codecId = toAvCodecId(codec);
    const AVCodec *decoder = avcodec_find_decoder(codecId);
    if (!decoder) {
        if (error) {
            *error = u"No FFmpeg decoder available for %1"_s.arg(
                codec == WebRtcVideoCodec::H265 ? u"H.265"_s : u"H.264"_s);
        }
        return false;
    }

    m_context.reset(avcodec_alloc_context3(decoder));
    if (!m_context) {
        if (error) {
            *error = u"Out of memory allocating the decoder context"_s;
        }
        return false;
    }

    // Live streaming: never trade latency for throughput.
    m_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_context->flags2 |= AV_CODEC_FLAG2_FAST;

    // Attempt a CUDA (NVDEC) hardware device first.
    m_hardware = false;
    if (getCudaDevice()) {
        for (int i = 0;; ++i) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
            if (!config) {
                break;
            }
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                && config->device_type == AV_HWDEVICE_TYPE_CUDA) {
                m_hwPixelFormat = config->pix_fmt;
                m_context->get_format = selectHwFormat;
                m_context->opaque = this;
                m_context->hw_device_ctx = av_buffer_ref(getCudaDevice());
                m_hardware = m_context->hw_device_ctx != nullptr;
                break;
            }
        }
    }

    m_context->thread_count = m_hardware ? 1 : 2;
    m_context->thread_type = FF_THREAD_SLICE;

    AVCodecContext *raw = m_context.get();
    int ret = avcodec_open2(raw, decoder, nullptr);
    if (ret < 0 && m_hardware) {
        // The hardware path failed to open; retry once in software so the layer
        // still works on machines without a usable NVDEC.
        qWarning("NVDEC open failed (%s), falling back to software decode",
                 qUtf8Printable(avErrorString(ret)));
        close();

        m_context.reset(avcodec_alloc_context3(decoder));
        if (!m_context) {
            if (error) {
                *error = u"Out of memory allocating the decoder context"_s;
            }
            return false;
        }
        raw = m_context.get();
        m_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
        m_context->flags2 |= AV_CODEC_FLAG2_FAST;
        m_context->thread_count = 2;
        m_context->thread_type = FF_THREAD_SLICE;

        ret = avcodec_open2(raw, decoder, nullptr);
    }
    if (ret < 0) {
        if (error) {
            *error = u"Could not open the %1 decoder: %2"_s
                         .arg(codec == WebRtcVideoCodec::H265 ? u"H.265"_s : u"H.264"_s,
                              avErrorString(ret));
        }
        m_context.reset();
        return false;
    }

    m_frame = FramePtr(av_frame_alloc());
    m_sysFrame = FramePtr(av_frame_alloc());
    m_packet = PacketPtr(av_packet_alloc());
    if (!m_frame || !m_sysFrame || !m_packet) {
        if (error) {
            *error = u"Out of memory allocating decoder buffers"_s;
        }
        close();
        return false;
    }

    qInfo("Opened %s video decoder (%s)",
          codec == WebRtcVideoCodec::H265 ? "H.265" : "H.264",
          m_hardware ? "NVDEC" : "software");
    return true;
}

void VideoDecoder::close()
{
    if (m_sws) {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    m_swsSrcFormat = -1;
    m_swsWidth = 0;
    m_swsHeight = 0;

    m_frame.reset();
    m_sysFrame.reset();
    m_packet.reset();
    m_context.reset();
    m_hwPixelFormat = AV_PIX_FMT_NONE;
    m_hardware = false;
}

bool VideoDecoder::decode(const std::uint8_t *data, std::size_t size, std::int64_t pts,
                          QString *error)
{
    if (!m_context || !data || size == 0) {
        return false;
    }

    av_packet_unref(m_packet.get());
    if (av_new_packet(m_packet.get(), int(size)) < 0) {
        if (error) {
            *error = u"Out of memory allocating a decode packet"_s;
        }
        return false;
    }
    std::memcpy(m_packet->data, data, size);
    m_packet->pts = pts;
    m_packet->dts = pts;

    const int ret = avcodec_send_packet(m_context.get(), m_packet.get());
    av_packet_unref(m_packet.get());

    if (ret == AVERROR_INVALIDDATA) {
        // Joined mid-GOP or lost packets: skip the access unit but keep the decoder
        // alive, otherwise the keyframe that would resync it is thrown away too.
        return true;
    }
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        if (error) {
            *error = u"Decoder rejected a packet: %1"_s.arg(avErrorString(ret));
        }
        return false;
    }

    return drain(error);
}

void VideoDecoder::flush()
{
    if (m_context) {
        avcodec_flush_buffers(m_context.get());
    }
}

bool VideoDecoder::drain(QString *error)
{
    while (true) {
        const int ret = avcodec_receive_frame(m_context.get(), m_frame.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF || ret == AVERROR_INVALIDDATA) {
            return true;
        }
        if (ret < 0) {
            if (error) {
                *error = u"Decode failed: %1"_s.arg(avErrorString(ret));
            }
            return false;
        }

        const bool ok = convertAndEmit(m_frame.get(), error);
        av_frame_unref(m_frame.get());
        if (!ok) {
            return false;
        }
    }
}

bool VideoDecoder::convertAndEmit(AVFrame *frame, QString *error)
{
    AVFrame *src = frame;

    // Hardware frames must be copied to system memory before they can be uploaded. The
    // transfer keeps NVDEC's NV12 layout; no conversion happens on the CPU.
    if (frame->hw_frames_ctx != nullptr) {
        av_frame_unref(m_sysFrame.get());
        const int ret = av_hwframe_transfer_data(m_sysFrame.get(), frame, 0);
        if (ret < 0) {
            if (error) {
                *error = u"Could not transfer a hardware frame to system memory: %1"_s
                             .arg(avErrorString(ret));
            }
            return false;
        }
        src = m_sysFrame.get();
    }

    const int width = src->width > 0 ? src->width : frame->width;
    const int height = src->height > 0 ? src->height : frame->height;
    if (width <= 0 || height <= 0) {
        return true; // skip empty frames
    }

    // Timestamps and colour properties live on the decoder's frame; the transferred copy
    // only carries the pixels.
    DecodedVideoFrame out;
    out.width = width;
    out.height = height;
    out.pts = frame->best_effort_timestamp != AV_NOPTS_VALUE ? frame->best_effort_timestamp : frame->pts;
    switch (frame->colorspace) {
    case AVCOL_SPC_BT709:
        out.matrix = VideoColorMatrix::Bt709;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        out.matrix = VideoColorMatrix::Bt2020;
        break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_FCC:
        out.matrix = VideoColorMatrix::Bt601;
        break;
    default:
        // Unspecified: the usual convention is BT.709 for HD, BT.601 for SD.
        out.matrix = height >= 720 ? VideoColorMatrix::Bt709 : VideoColorMatrix::Bt601;
        break;
    }
    out.fullRange = frame->color_range == AVCOL_RANGE_JPEG || src->format == AV_PIX_FMT_YUVJ420P;

    if (m_forceRgba) {
        if (!convertToRgba(src, out, error)) {
            return false;
        }
    } else if (src->format == AV_PIX_FMT_NV12) {
        out.layout = VideoPixelLayout::Nv12;
        for (int i = 0; i < 2; ++i) {
            out.planes[i] = src->data[i];
            out.strides[i] = src->linesize[i];
        }
    } else if (src->format == AV_PIX_FMT_YUV420P || src->format == AV_PIX_FMT_YUVJ420P) {
        out.layout = VideoPixelLayout::Yuv420p;
        for (int i = 0; i < 3; ++i) {
            out.planes[i] = src->data[i];
            out.strides[i] = src->linesize[i];
        }
    } else {
        // Anything else (10-bit, 4:2:2, 4:4:4, ...) is rare here: convert on the CPU.
        if (!convertToRgba(src, out, error)) {
            return false;
        }
    }

    if (m_onFrame) {
        m_onFrame(out);
    }
    return true;
}

bool VideoDecoder::convertToRgba(AVFrame *src, DecodedVideoFrame &out, QString *error)
{
    const int width = out.width;
    const int height = out.height;
    if (!m_sws || m_swsSrcFormat != src->format || m_swsWidth != width || m_swsHeight != height) {
        if (m_sws) {
            sws_freeContext(m_sws);
        }
        m_sws = sws_getContext(width, height, static_cast<AVPixelFormat>(src->format),
                               width, height, AV_PIX_FMT_RGBA, SWS_BILINEAR,
                               nullptr, nullptr, nullptr);
        if (!m_sws) {
            if (error) {
                *error = u"Could not create the RGBA conversion context"_s;
            }
            return false;
        }
        m_swsSrcFormat = src->format;
        m_swsWidth = width;
        m_swsHeight = height;
    }

    // Honour the stream's matrix and range instead of swscale's BT.601 limited default.
    const int colorspace = out.matrix == VideoColorMatrix::Bt709    ? SWS_CS_ITU709
                           : out.matrix == VideoColorMatrix::Bt2020 ? SWS_CS_BT2020
                                                                    : SWS_CS_ITU601;
    const int *coefficients = sws_getCoefficients(colorspace);
    sws_setColorspaceDetails(m_sws, coefficients, out.fullRange ? 1 : 0, coefficients, 1, 0, 1 << 16, 1 << 16);

    const std::size_t bufferSize = static_cast<std::size_t>(width) * height * 4u;
    if (m_rgbaBuffer.size() < bufferSize) {
        m_rgbaBuffer.resize(bufferSize);
    }

    uint8_t *dstData[4] = { m_rgbaBuffer.data(), nullptr, nullptr, nullptr };
    int dstStride[4] = { static_cast<int>(width) * 4, 0, 0, 0 };
    sws_scale(m_sws, src->data, src->linesize, 0, height, dstData, dstStride);

    out.layout = VideoPixelLayout::Rgba;
    out.planes[0] = m_rgbaBuffer.data();
    out.strides[0] = width * 4;
    return true;
}
