---
title: Image files
nav_order: 2
parent: Media file structure
---

# Image files in C-Play

Most image file can be loaded and viewed by "*Open File*", and mapped to different grids and stereographic modes, just like a [video file](video.md).

However, there is two specific type of other images, that C-Play can support, which are:

* Background
* Overlay

A background can be set in the settings, and will be shown when a video fades out. Se [here](../playback/background.md) for more details on backgrounds.

Overlays, are images that are "side-loaded" with video files, and are actually mapped ontop of the video. Such that you can add a static map ontop a ocean flow field, or a logo ontop of the video, for instance. The grid and stereoscopic mappings need to be the same as the video. Read the [cplayfile](cplayfile.md) docs for more info.