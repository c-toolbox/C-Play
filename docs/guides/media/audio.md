---
title: Audio files
layout: home
nav_order: 4
parent: Media file structure
---

# Audio files in C-Play

C-Play is designed to handle audio playback and files, only on the master PC, which run the master UI. The nodes should only handle video/image rendering.

The master supports embedded audio in the video files, or "side-loading" of audio files next to the video file.

The "side-loading" is enabled in "Settings -> Configure -> Audio":

[x] *Load audio files in same folder as video file.*

## Select audio track

In the header toolbar, the combo box "*Audio File*" contains a list of the tracks that are embedded in the video file, or have been side-loaded next to the video.

A benefit of side-loaded tracks are that you can choose which one is selected when you load the video, if it is loaded as a [cplayfile](cplayfile.md). 

When pressing "*Save As C-Play file*", you will notice that this the current audio track (if side-loaded) is present in the audio field automatically.

If you save this as a cplayfile and then load that file, that audio track becomes selected by default.