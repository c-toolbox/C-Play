---
title: Build from code
sidebar_position: 9
---

# Build C-Play from source code

C-Play is currently tested only on Windows, so this build guide primarily describes that use case.

The instructions below target the Windows build and its optional Windows-specific backends.

These are the steps:

- Clone the source and its submodules:

  ```sh
  git clone --recurse-submodules https://github.com/c-toolbox/C-Play.git
  cd C-Play
  ```

  For an existing checkout, run `git submodule update --init --recursive`.

- Use the [Craft Guide](/build/craft) to install all dependencies for the UI, including Qt6 and KF6 libraries.

- Use the [Build MPV and FFMPEG Guide](/build/mpv_ffmpeg) guide to build FFmpeg and MPV with JACK+portaudio support.

- Install the dependencies of the SGCT submodule with vcpkg. `stbimage` is not in the vcpkg registry, so it comes from the overlay port shipped in the SGCT repository:

  ```sh
  vcpkg install freetype glfw3 glm json-schema-validator libpng minizip nlohmann-json openxr-loader scnlib stbimage tinyxml2 zlib glad[extensions,loader,wgl] spout2 vulkan --triplet x64-windows --overlay-ports=src/sgct/support/vcpkg/ports
  ```

  Run this from the C-Play root rather than from `src/sgct`, as the `vcpkg.json` there switches vcpkg to manifest mode.

- Configure with CMake 4.0 or newer and a compiler with C++23 support. Make your Craft Qt6/KF6 installation and mpv development files discoverable through `CMAKE_PREFIX_PATH` and `PATH`. The build enables vcpkg by default: set `VCPKG_ROOT` to its installation, or disable `BUILD_WITH_VCPKG_SUPPORT` when supplying dependencies yourself.

- Optionally, install Node.js and pnpm to build the web views (see the [Web views Guide](/build/web-views)).

- Build in Visual Studio 2022 or in Qt-Creator for good QML overview.

- Follow the [Deploy Guide](/build/deploy) to copy the built application and its dependencies to a standalone folder and optionally create an installer.
