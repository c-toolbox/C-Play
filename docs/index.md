---
title: Home
nav_order: 1
---

# C-Play : Cluster Video Player

C-Play is a video/media player developed for cluster environments where you need multiple computers and/or displays to run your content on. The displays could be flat or curved in any setup that is supported by our underlying toolkit [SGCT](https://sgct.github.io/) and any media format supported by [MPV](https://mpv.io/).

![Render v1](assets/Cplay-v1.png) 

![Render Dome Image 1](assets/CPlay-in-dome-1.jpg)

## General
C-Play is an open source cluster video player, based on these open source projects:

- [SGCT](https://sgct.github.io/) - Our own simple graphics cluster toolkit
- [LibMPV](https://github.com/mpv-player/mpv) - command line video player, using FFmpeg
- [FFmpeg](https://github.com/FFmpeg/FFmpeg) - The one and only video decoder/encoder
- [Haruna](https://github.com/g-fb/haruna) - Qt/QML UI for MPV

## Version: 2.0

![Render v2](assets/Cplay-v2.png)

### Current Features
These are just some features that set C-Play apart from others media/video players:

- Runs a QT/QML UI application on master computer and small none-UI GLFW/SGCT application on nodes.

- Sync playback, loading and other properties between master and nodes.

- Playing audio only on master (support for audio settings, primarily using JACK for multi-channel output to ASIO devices).

- Loading external audio files as multiple tracks .

- Tested and used on primarily Windows 10, in domes and other big arenas.

- Editing and saving playlist and playfiles including all necassary parameters.

- Introduce "Sections" and associated editor to create bookmarks to jump between clips inside a larger movie.

- Sync video+audio fade in/out

- HTTP Server API

### Launcher
To launch application on master+nodes, we use an own developed application called [C-Troll](https://github.com/c-toolbox/C-Troll).

## Guides
1. [Install C-Play](install.md)
1. [Setup C-Play](setup.md)
1. [Media structure](media.md)
1. [Playback features](playback.md)
1. [Web API](api.md)
1. [Build from code](build.md)

# License
C-Play is licensed under the [GNU General Public License v3.0](https://choosealicense.com/licenses/gpl-3.0/)

# Contact
For any questions or further information about the C-Play project, [erik.sunden@liu.se](mailto:erik.sunden@liu.se).
