# C-Play : Cluster Video Player

C-Play is an open source cluster video player, based on these projects:
- SGCT (github.com/sgct)
- LibMPV (github.com/mpv-player/mpv)
- FFmpeg (https://github.com/FFmpeg/FFmpeg)
- Haruna (github.com/g-fb/haruna) - Qt/QML UI for MPV

# Features

these are just some features that set C-Play apart from others players

- run on master/node setup and use QT/QML UI for master, and GLFW/SGCT client for nodes

- sync between master and nodes

- playing audio only on master (support for audio settings, primarily using JACK for multi-channel output to ASIO devices)

- loading external audio files as multiple tracks 

- testes and used on primarily Windows 10

# Build

-- Use Craft on Windows to install all dependencies for Haruna, from Qt to KF5 libs.
-- Use github.com/m-ab-s/media-autobuild_suite to compile FFmpeg, MPV libs with PortAudio an JACK support, if needed.
-- Build in Qt-Creator for good QML overview.
