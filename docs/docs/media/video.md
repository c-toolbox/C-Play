---
title: Video files
sidebar_position: 1
---

# Video files in C-Play

As soon as you load a video file from the master UI, the path to that video file is transferred to the nodes/clients, and they open the same file located at the same path on their system.

This might be the exact same file, which means all clients decode the complete video, but may show different parts, depending on the SGCT configuration files. This is referred to as *"online splitting"*.

However, normally, for a high-resolution movie, *"pre-splitting"* is applied so the clients only decode the video content needed for that specific client.

## Prepare videos with C-Slice

[C-Slice](https://c-toolbox.github.io/C-Slice/) complements C-Play by preparing video files or image sequences for immersive playback. It renders and encodes separate outputs for the selected windows in your installation's SGCT configuration, making it useful for pre-splitting high-resolution content.

Follow the [C-Slice basic workflow](https://c-toolbox.github.io/C-Slice/guides/workflow/basic_slice.html) to select the source, target SGCT configuration, output windows, mapping, and stereo layout. Its [encoding guide](https://c-toolbox.github.io/C-Slice/guides/media/encoding.html) explains codec, frame-rate, and quality options. Render a short test before processing the full video, then check it on the C-Play playback machines.

Use **None (pre-split)** for outputs already mapped to the target windows. Keep the source's Dome, Sphere, or Plane mapping when playing an unsplit source that still needs mapping in C-Play. For separate per-node or per-eye files, see [multi-video compositions](/media/cplaymulti).

## Make the prepared files available

C-Play does not copy media files from master to nodes. Make local files available before playback, using the same paths on each machine or the per-node paths supported by [multi-video compositions](/media/cplaymulti) and [predefined sources](/media/layers#per-node-resolution-in-clusters).

For presentation layers, [streaming textures from the master](/playback/node-streaming) is an alternative to local decoding. This sends rendered frames rather than copying the source file; its audio and synchronization limits are described in that guide.

For details on video formats etc, please see the [video setup](/setup/video) guide.
