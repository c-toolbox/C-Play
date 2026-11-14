/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Include after the FFmpeg headers of a translation unit. Fails the build when they belong to
// another FFmpeg than the tree the target links (see cmake/FFmpegHeaderVersions.cmake):
//  - C-Play.exe (WebRTC decoders): the FFmpeg tree libmpv was built with;
//  - cplay_vkengine.dll: the Vulkan/libplacebo FFmpeg tree.
// Libraries whose headers the unit did not include are not checked.

#ifndef CPLAY_FFMPEGVERSIONCHECK_H
#define CPLAY_FFMPEGVERSIONCHECK_H

#if defined(CPLAY_FFMPEG_EXPECT_AVCODEC_MAJOR) && defined(LIBAVCODEC_VERSION_MAJOR)
#if LIBAVCODEC_VERSION_MAJOR != CPLAY_FFMPEG_EXPECT_AVCODEC_MAJOR
#error "libavcodec headers of another FFmpeg than the linked one: check the include directory order"
#endif
#endif

#if defined(CPLAY_FFMPEG_EXPECT_AVUTIL_MAJOR) && defined(LIBAVUTIL_VERSION_MAJOR)
#if LIBAVUTIL_VERSION_MAJOR != CPLAY_FFMPEG_EXPECT_AVUTIL_MAJOR
#error "libavutil headers of another FFmpeg than the linked one: check the include directory order"
#endif
#endif

#if defined(CPLAY_FFMPEG_EXPECT_AVFORMAT_MAJOR) && defined(LIBAVFORMAT_VERSION_MAJOR)
#if LIBAVFORMAT_VERSION_MAJOR != CPLAY_FFMPEG_EXPECT_AVFORMAT_MAJOR
#error "libavformat headers of another FFmpeg than the linked one: check the include directory order"
#endif
#endif

#if defined(CPLAY_FFMPEG_EXPECT_AVFILTER_MAJOR) && defined(LIBAVFILTER_VERSION_MAJOR)
#if LIBAVFILTER_VERSION_MAJOR != CPLAY_FFMPEG_EXPECT_AVFILTER_MAJOR
#error "libavfilter headers of another FFmpeg than the linked one: check the include directory order"
#endif
#endif

#if defined(CPLAY_FFMPEG_EXPECT_SWSCALE_MAJOR) && defined(LIBSWSCALE_VERSION_MAJOR)
#if LIBSWSCALE_VERSION_MAJOR != CPLAY_FFMPEG_EXPECT_SWSCALE_MAJOR
#error "libswscale headers of another FFmpeg than the linked one: check the include directory order"
#endif
#endif

#endif // CPLAY_FFMPEGVERSIONCHECK_H
