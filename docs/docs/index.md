---
title: Home
sidebar_position: 1
---

import DemoVideo from '@site/src/components/DemoVideo';

# C-Play : Cluster Media Player

C-Play is a video/media player developed for cluster environments where you need multiple computers and/or displays to run your content on. The displays could be flat or curved in any setup that is supported by our underlying toolkit [SGCT](https://sgct.github.io/) and any media format supported by [MPV](https://mpv.io/).

## Latest Version: 2.4 (Beta)

<DemoVideo />

### Content features
These are just some features that set C-Play apart from other media and video players:

C-Play supports media that is:

- Stereoscopic (Side-by-side or Top-Bottom) and Monoscopic

- 180 fulldome / fisheye

- 360 equirectangular or equiangular cubemap (common on YouTube)

- Any "flat" media with arbitrary aspect ratio

Primary media, such as video and images, is easily opened and configured through playfiles and added to playlists for a standard media player setup.

For video preparation and pre-splitting to match your display setup, use [C-Slice](https://c-toolbox.github.io/C-Slice/). See [video preparation](/media/video#prepare-videos-with-c-slice) for how its outputs are used in C-Play.

An additional powerful feature in C-Play is the presentation tool, where you can add an arbitrary number of layers within slides using numerous inputs, such as:

- [*Images*](/media/layers#image) (PNG, JPG, TIFF, WEBP etc)

- [*PDF*](/media/layers#pdf) (Common export format from PPT)

- [*Videos*](/media/layers#video) (H264, HEVC/H265, AV1, H.266/VVC, VP9 etc)

- [*MultiVideo*](/media/layers#multivideo) (Multiple synchronized videos with per-node files and separate mappings, including left/right eye views)

- [*Audio*](/media/layers#audio) (WAV, AAC, MP3 etc)

- [*NDI*](/media/layers#ndi) or [*OMT*](/media/layers#omt) (Live video/audio over the network)

- [*Spout*](/media/layers#spout) (GPU video shared by another local Windows application)

- [*DirectShow*](/media/layers#directshow) (Windows cameras, capture devices, or media played through system decoders)

- [*Capture*](/media/layers#capture) (Capture card inputs through supported vendor SDKs, such as Datapath)

- [*Streams*](/media/layers#stream) (Live streaming over SRT (UDP) or RTSP — camera feeds, capture cards, encoders, and other MPV-compatible live inputs)

- [*YouTube*](/media/layers#youtube) (Single video URLs played as VOD layers with full player controls, resolved by yt-dlp *(Added in 2.4)*)

- [*WebRTC*](/media/layers#webrtc) (Low-latency streaming from anywhere. *Pair with VDO.Ninja for remote guests to share their devices.*)

- [*Text*](/media/layers#text) (With custom fonts, also used for subtitles)

- [*Control*](/media/layers#control) (Automate C-Play playback, loading, fades, and orientation)

- [*REST*](/media/layers#rest) (Send HTTP or WebSocket commands to external applications and systems)

- [*TCP*](/media/layers#tcp) (Trigger saved commands through persistent device connections)

With these [layer types](/media/layers), you can build immersive presentations combining media, live sources, and show-control actions. Available types depend on the platform and features enabled in your build.

Each slide can have a [timeline](/media/cplaypres#-timeline-c-play-v23-and-newer) with keyframes to animate layer opacity, position, rotation, and scale. The [animation pane](/media/cplaypres#animations-c-play-v24-and-newer) adds entrance and exit effects such as fades, wipes, fly-ins, and zooms, with easing and click-triggered steps; these effects also work on slides without a timeline.

### Technical features

- Runs a Qt/QML UI application on the master computer and a small non-UI GLFW/SGCT application on the nodes/clients.

- Sync playback, loading and other properties between master and clients.

- Playing audio is usually performed on master *(Support for node audio is added in 2.2)*. Change of audio output is supported, and C-Play is pre-built with "JACK", which opens for multi-channel low-latency output to for instance ASIO devices.

- Loading external audio files as multiple tracks.

- Editing and saving playlists and playfiles including all necessary parameters.

- Configure "sections" in an editor to create bookmarks for jumping between clips inside a larger movie.

- C-Play nodes can run on top of other applications. On the master, viewing your video or a layer on a secondary monitor is also simple and requires no extra decoding resources.

- HTTP Web API, so you can integrate control of C-Play into a custom system.

- REST Commands (HTTP and WebSockets) to control other applications, such as OBS Studio which C-Play then can receive content from.

- NDI output of the main video and individual presentation layers, so other applications (e.g. OBS Studio) can receive C-Play content over the network *(Added in 2.4)*.

- Stream presentation layers from the master to all nodes over UDP multicast or NDI, so each layer is rendered once on the master and displayed on every node without decoding it again (frame-locked synchronization is available with UDP multicast) *(Added in 2.4)*.

- Optional [MediaMTX](https://github.com/bluenviron/mediamtx) integration to fetch available live streams from a server and add them as Stream or WebRTC layers *(Added in 2.4)*.

- Tested and used on Windows 10/11, in domes and other big arenas.

## Guides
1. [Install C-Play](/install)
1. [Setup C-Play](/setup)
1. [Media structure](/media)
1. [Settings](/settings)
1. [Playback features](/playback)
1. [Remote control](/remote-control)
1. [Build from code](/build)

## Launcher
To launch the application on master+nodes, we use our own application called [C-Troll](https://github.com/c-toolbox/C-Troll).

## Backend
C-Play is an open source cluster video player, based on these open source projects:

- [SGCT](https://sgct.github.io/) - Our own simple graphics cluster toolkit
- [LibMPV](https://github.com/mpv-player/mpv) - command line video player, using FFmpeg
- [FFmpeg](https://github.com/FFmpeg/FFmpeg) - The one and only video decoder/encoder
- [Haruna](https://github.com/g-fb/haruna) - Qt/QML UI for MPV

Optional libraries in current C-Play builds include:

- [NDI](https://ndi.video/for-developers/ndi-sdk/) - Support frame-synced NDI streams, video and audio
- [OMT](https://openmediatransport.org/) (Open Media Transport) - Support frame-synced OMT video and audio streams
- [libdatachannel](https://github.com/paullouisageneau/libdatachannel) - WebRTC data channels for pulling WHEP live streams *(Added in 2.4)*
- [Poppler](https://poppler.freedesktop.org/) - For rendering PDF pages
- [SAIL](https://sail.software/) - For more extensive image decoding

# License
C-Play is licensed under the [GNU General Public License v3.0](https://choosealicense.com/licenses/gpl-3.0/)

# Contact
For any questions or further information about the C-Play project, [erik.sunden@liu.se](mailto:erik.sunden@liu.se).
