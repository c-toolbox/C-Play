---
title: System integration
sidebar_position: 8
---

# System integration

C-Play can integrate with external applications and services for launching, routing live media, and controlling local Windows workflows.

Below are guides for the current integration workflows:

 - [OBS Studio + NDI -> C-Play](/system-integration/obs-ndi) - Use OBS as a live-input bridge and send browser, VDO.Ninja, camera, or mixed program output into C-Play through NDI.
 - [C-Play -> NDI output](/system-integration/ndi-output) - Publish the player output — main video, or the 3D view when a 3D master view mode is active — or individual presentation layers as NDI sources so other applications (e.g. OBS Studio) can receive C-Play content over the network. New in 2.4.0.
 - [Launch apps and control things with C-Play](/system-integration/rest-troll-obs) - Trigger HTTP and WebSocket commands, launch OBS through C-Troll, and control OBS through obs-websocket.
 - [C-Play "on-top" of other apps](/system-integration/on-top) - Keep C-Play above local applications, reveal apps behind it with opacity, or bring local GPU output in through Spout.