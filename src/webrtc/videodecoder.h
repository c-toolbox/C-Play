/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtctypes.h"

#include <QString>

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

/// Decodes Annex-B H.264/H.265 access units with FFmpeg and hands out RGBA8 frames.
///
/// Tries a CUDA (NVDEC) hardware device first and falls back to software decoding
/// when no suitable GPU/decoder is available or the hardware path fails to open.
/// Not thread-safe; one instance belongs to one decode worker thread.
class VideoDecoder {
public:
    /// rgba points at w*h*4 bytes of RGBA8 data, valid only for the duration of
    /// the call. Frames are delivered in decode order (low delay mode).
    using FrameCallback = std::function<void(const std::uint8_t *rgba, int width, int height)>;

    VideoDecoder();
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder &) = delete;
    VideoDecoder &operator=(const VideoDecoder &) = delete;

    /// Allocates and opens the decoder for one codec. Safe to call while open;
    /// it closes any previous context first.
    bool open(WebRtcVideoCodec codec, QString *error);
    void close();

    /// Feeds one Annex-B access unit. Decoded frames are delivered through the
    /// frame callback. Returns false on a fatal decoder error (the caller should
    /// treat the stream as broken and reconnect).
    bool decode(const std::uint8_t *data, std::size_t size, std::int64_t pts, QString *error);

    void setOnFrame(FrameCallback cb) { m_onFrame = std::move(cb); }

    bool isOpen() const { return m_context != nullptr; }
    bool isHardwareDecoder() const { return m_hardware; }

    /// Pinned by open() so the get_format callback can pick the hardware surface format.
    AVPixelFormat hwPixelFormat() const { return m_hwPixelFormat; }

private:
    bool drain(QString *error);
    bool convertAndEmit(AVFrame *frame, QString *error);

    struct AVCodecContextDeleter { void operator()(AVCodecContext *c) const; };
    struct AVFrameDeleter { void operator()(AVFrame *f) const; };
    struct AVPacketDeleter { void operator()(AVPacket *p) const; };

    using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
    using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
    using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

    CodecContextPtr m_context;
    FramePtr m_frame;        // decoder output (hardware or system memory)
    FramePtr m_sysFrame;     // system-memory copy of a hardware frame
    PacketPtr m_packet;

    SwsContext *m_sws = nullptr;
    int m_swsSrcFormat = -1; // format/size the sws context was built for
    int m_swsWidth = 0;
    int m_swsHeight = 0;

    AVPixelFormat m_hwPixelFormat = AV_PIX_FMT_NONE;
    std::vector<std::uint8_t> m_rgbaBuffer; // RGBA8 output handed to the callback

    bool m_hardware = false;
    FrameCallback m_onFrame;
};
