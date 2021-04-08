# C-Play : Cluster Video Player

C-Play is an open source cluster video player, based on these projects:

- [SGCT](https://github.com/sgct) - Simple graphics cluster toolkit, using GLFW/OpenGL etc
- [LibMPV](https://github.com/mpv-player/mpv) - command line video player, using FFmpeg
- [FFmpeg](https://github.com/FFmpeg/FFmpeg) - The one and only video decoder/encoder
- [Haruna](https://github.com/g-fb/haruna) - Qt/QML UI for MPV

# Features

these are just some features that set C-Play apart from others players

- run on master/node setup and use QT/QML UI for master, and GLFW/SGCT client for nodes

- sync playback, loading and other properties between master and nodes

- playing audio only on master (support for audio settings, primarily using JACK for multi-channel output to ASIO devices)

- loading external audio files as multiple tracks 

- testes and used on primarily Windows 10, in domes and other big arenas

# Build on Windows

-- Use [Craft](./help/CRAFT_INSTALLS.md) on Windows to install all dependencies for Haruna, from Qt5, KF5 libs.
-- Use [Media Build Suite](https://github.com/m-ab-s/media-autobuild_suite) to compile FFmpeg, MPV libs
-- Use the [Build Jack](./help/BUILD_JACK.md) guide to build libjack with portaudio if you want that functionality.
-- Move result (dll, exe, dll.a etc) from "Build Jack" compiliation (JACK, Portaudio) to MINGW64 bin/lib/pkg folders in "Media Build Suite", so they can be found when running "Media Build Suite" again.
-- Run "Media Build Suite" again, now with PortAudio an JACK support and these [FFmpeg Options](./help/ffmpeg_options.txt)
-- Copy over compiliations for FFmpeg and MPV to Craft bin/libs so whe can have new versions when compiling C-Play.
-- Build in Qt-Creator (the one installed with Craft) for good QML overview.
