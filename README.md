# C-Play : Cluster Video Player

![Render Dome Image 1](docs/assets/CPlay-in-dome-2.jpg)

C-Play is an open source cluster video player, based on these projects:

- [SGCT](https://github.com/sgct) - Simple graphics cluster toolkit, using GLFW/OpenGL etc
- [LibMPV](https://github.com/mpv-player/mpv) - command line video player, using FFmpeg
- [FFmpeg](https://github.com/FFmpeg/FFmpeg) - The one and only video decoder/encoder
- [Haruna](https://github.com/g-fb/haruna) - Qt/QML UI for MPV

# Documentation

https://c-toolbox.github.io/C-Play/

# Features

These are just some features that set C-Play apart from others players

- Runs on master/node setup and use QT/QML UI for master, and GLFW/SGCT client for nodes

- Playing audio only on master (support for audio settings, primarily using JACK for multi-channel output to ASIO devices)

- Sync playback, loading and other properties between master and nodes

- Support for GPU and CPU decoding and more through configuration files.

- Editing and saving playlist and playfiles including all necassary parameters.

- "Sections" and associated editor to create bookmarks to jump between clips inside a larger movie.

- Sync video+audio fade in/out

- HTTP Server API

- Tested and used on primarily Windows 10, in domes and other big arenas

# Build on Windows

- Use [Craft Guide](./help/CRAFT_INSTALLS.md) to install all dependencies for the UI, from Qt5, KF5 libs.

- Use the [Build FFMPEG and MPV Guide](./help/BUILD_FFMPEG_AND_MPV.md) guide to build FFmpeg and MPV with JACK+portaudio support.

- Configure C-Play with CMake.

- Build in Qt-Creator for good QML overview, or use Visual Studio.
