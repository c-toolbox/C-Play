---
title: Audio files
sidebar_position: 4
---

# Audio files in C-Play

By default, audio plays on the master PC and nodes render video and images. From C-Play v2.2, audio can also play on nodes. Enable the required outputs in [Audio settings](/settings/audio); make external audio files available on each machine that plays them.

C-Play supports embedded audio in the video files, or "side-loading" of audio files next to the video file.

The "side-loading" is enabled in "Settings -> Configure -> Audio":

[x] *Load audio files in same folder as video file.*

## Select audio track

In the header toolbar, the combo box "*Audio File*" contains a list of the tracks that are embedded in the video file, or have been side-loaded next to the video.

A benefit of side-loaded tracks is that you can choose which one is selected when you load the video, if it is loaded as a [cplayfile](/media/cplayfile).

When pressing "*Save As C-Play file*", you will notice that the current audio track (if side-loaded) is present in the audio field automatically.

If you save this as a cplayfile and then load that file, that audio track becomes selected by default.