---
title: System integration
layout: home
has_children: true
has_toc: false
nav_order: 8
---

# System integration

C-Play can integrate with external applications and services for launching, routing live media, and controlling local Windows workflows.

Below are guides for the current integration workflows:

 - [OBS Studio + NDI -> C-Play](guides/integration/obs-ndi) - Use OBS as a live-input bridge and send browser, VDO.Ninja, camera, or mixed program output into C-Play through NDI.
 - [C-Play -> NDI output](guides/integration/ndi-output) - Publish the main video or individual presentation layers as NDI sources so other applications (e.g. OBS Studio) can receive C-Play content over the network. New in 2.4.0.
 - [Launch apps and control things with C-Play](guides/integration/rest-troll-obs) - Trigger HTTP and WebSocket commands, launch OBS through C-Troll, and control OBS through obs-websocket.
 - [C-Play "on-top" of other apps](guides/integration/on-top) - Keep C-Play above local applications, reveal apps behind it with opacity, or bring local GPU output in through Spout.