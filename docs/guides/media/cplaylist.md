---
title: Build playlists (*.cplaylists)
layout: home
nav_order: 6
parent: Media file structure
---

# Build and save a playlist (*.cplaylist)

Playlist in C-Play are essentially lists of [cplayfiles](cplayfile.md), which are descriptions (mapping, sections etc) of specific videos.

C-Play requires you to save *cplayfile* of your video to make sure that the mappings and other parameters are correct between loading of different files.

While each item in a playlist corresponds to a *cplayfile*, there is also a eof(*end of file*) mode that can be changed fo each item in the playlist. The eof modes are:

* Pause
* Next
* Loop

The playlist view, where sections are created and managed, is opened in the bottom right (or bottom left, depending on settings). 

It should be straight-forward to build a playlist from adding and removing *cplayfiles*, moving them around, and changing the eof mode for the selected one.

Save the playlist by pressing the disk icon, in the middle of top bar in the playlist view. 
