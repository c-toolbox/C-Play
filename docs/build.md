---
title: Build from code
layout: home
has_children: true
has_toc: false
nav_order: 8
---

# Build C-Play from source code

The only platform C-Play is tested on is on Windows, thus this building guide describes primarily that use-case.

*Note: All libraries used are cross-platform*

The are the steps:

- Clone the source on Github.

- Use [Craft Guide](guides/build/craft) to install all dependencies for the UI, from Qt5, KF5 libs.

- Use the [Build MPV and FFMPEG Guide](guides/build/mpv_ffmpeg) guide to build FFmpeg and MPV with JACK+portaudio support.

- Configure C-Play with CMake.

- Good practice to follow [Deploy Guide](guides/build/deploy) as well, to copy the build with it's dependencies to single binary folder.

- Build in Visual Studio 2022 or in Qt-Creator for good QML overview.