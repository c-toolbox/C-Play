---
title: Build from code
layout: home
has_children: true
has_toc: false
nav_order: 8
---

# Build C-Play from source code

C-Play is currently tested only on Windows, so this build guide primarily describes that use case.

*Note: All libraries used are cross-platform*

These are the steps:

- Clone the source on GitHub.

- Use the [Craft Guide](guides/build/craft) to install all dependencies for the UI, including Qt6 and KF6 libraries.

- Use the [Build MPV and FFMPEG Guide](guides/build/mpv_ffmpeg) guide to build FFmpeg and MPV with JACK+portaudio support.

- Launch CMake (the latest build was done with CMake 3.31.8). Add paths to qmake.exe (*Craft bin/*) and mpv.exe (*MINGW bin/*) to your path in CMake if they are not in your environment path already. Configure C-Play with CMake.

- It is good practice to follow the [Deploy Guide](guides/build/deploy) as well, to copy the build with its dependencies to a single binary folder.

- Build in Visual Studio 2022 or in Qt-Creator for good QML overview.