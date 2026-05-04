---
title: Image files
layout: home
nav_order: 2
parent: Media file structure
---

# Image files in C-Play

Most image files can be loaded and viewed by "*Open File*", and mapped to different grids and stereographic modes, just like a [video file](video).

However, there are three specific types of other images that C-Play supports:

* Background
* Foreground
* Overlay

A background image can be set in the settings, and will be shown when a video fades out. 
A foreground image is shown on top of everything, including video and overlay below.
See [here](../playback/images) for more details on backgrounds and foregrounds.

Overlays are images that are "side-loaded" with video files, and are mapped on top of the video. This means that you can add a static map on top of an ocean flow field, or a logo on top of the video, for instance. The grid and stereoscopic mappings need to be the same as the video. Read the [cplayfile](cplayfile) docs for more info.