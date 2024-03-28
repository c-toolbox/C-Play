---
title: Video files
nav_order: 1
parent: Media file structure
---

# Video files in C-Play

As soon as you load a video file from the master UI, the path to that video file is transferred to the nodes/clients, and they open the same file located at the same path on their system.

This might be the exact same file, which means all clients decode the complete video, but may show different parts, depending on the SGCT configuration files. This is referred to as "online splitting".

However, normally, for a high resolution movie, "pre-splittning" is applied, such that the clients only decode the necessary video content for that specific client.

To conclude, C-Play does not sync video, image or audio files from master to nodes by itself. Your are free to use any other solution for file management, so that C-Play can find files on all computers which is running it.

For details on video formats etc, please see the [video setup](../setup/video.md) guide.