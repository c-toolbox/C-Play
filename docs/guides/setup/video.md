---
title: Video configuration
nav_order: 2
parent: Setup C-Play
---

# Video files in C-Play

As soon as you load a video file from the master UI, the path to that video file is transferred to the nodes/clients, and they open the same file located at the same path on their system.

This might be the exact same file, which means all clients decode the complete video, but may show different parts, depending on the SGCT configuration files. This is referred to as "online splitting".

However, normally, for a high resolution movie, "pre-splittning" is applied, such that the clients only decode the necessary video content for that specific client.

## MPV

## H264 vs H265