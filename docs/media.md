---
title: Media file structure

has_children: true
has_toc: false
nav_order: 5
---

# How C-Play opens and interpret media files

You can open various formats for video, images and audio directly by using "Open file".

However, while C-Play is this flexible, it has been designed to use combinations of video, images and audio during a show.

For insight into how video is handled, se:
 - [Video file guide](guides/media/video)

And images, either as primary source, backgrounds or overlays, se:
 - [Image file guide](guides/media/images)

 Both video and images can be mapped onto physical objects in multiple ways, se:
 - [Video & Image Mapping Modes](guides/media/mapping)

 Audio, can be loaded without video, but usually are combined inside the video or as adjacent files, se:
  - [Audio file guide](guides/media/audio)


## C-Play file structures

You can save a media bundle, based on numerous content specified above, to make C-Play load everything correctly from one file. Se this guide how to make these:
 - [CPlayfile](guides/media/cplayfile)

 Also, multiple cplayfiles can be ordered in a playlist, through this guide:
 - [CPlaylist](guides/media/cplaylist)