---
title: Per-node media folders
sidebar_position: 6.5
---

# Per-node media folders (`<name>_NODES`)

By default, every node opens the same file as the master. A per-node media folder lets you give **each node its own files** for a video, without writing a [multi-video composition](/media/cplaymulti). A node can play:

* **video and audio**,
* **video only**, or
* **audio only**,

and a node can have **several audio files**, for example one per language.

Nodes are named by the node ids in `data/nodes.json` (see [Node Configuration](/media/cplaymulti#node-configuration)).

## Folder layout

Put a folder named `<file name without extension>_NODES` next to the media file. In it, make one folder for each node, named by its node id:

```text
Media/
  Show.mp4                 <- played by the master
  Show_EN.wav              <- side-loaded audio tracks of the master (optional)
  Show_SV.wav
  Show_NODES/
    Node1/
      Show.mp4             <- video + audio: video, plus the audio files below
      Show_EN.wav
      Show_SV.wav
    Node2/
      Show.mp4             <- video only
    Node3/
      Show_EN.wav          <- audio only
      Show_SV.wav
    default/
      Show.mp4             <- optional: nodes without their own folder
```

What a node folder holds decides what the node plays:

| Node folder holds | The node plays |
| --- | --- |
| A video file **and** audio files | The video, with the audio files as its audio tracks. The video's own audio is not used. |
| Only a video file | The video, without audio. |
| Only audio files | Audio only. The node shows no main video. |

* **Video file:** the file with the same name as the master's file (without extension), for example `Show.mp4` or `Show.mov` for `Show.mp4`. Other video files in the folder are ignored.
* **Audio files:** every file with an audio extension (`wav`, `w64`, `mp3`, `flac`, `aac`, `ac3`, `eac3`, `dts`, `thd`, `ogg`, `oga`, `opus`, `m4a`, `wma`, `aif`, `aiff`, `caf`, `mka`, `mp2`, `ape`, `wv`), in alphabetical order. Only these files are loaded. The *Load audio files in video folder* setting does not add other files.
* **Folder names** are node ids from `data/nodes.json`. Case does not matter. A node that is not in `nodes.json` matches a folder named after its IP address.
* **`default/`** (optional) is used by every node that has no folder of its own.
* **No folder for a node** (and no `default/`): the node plays what it would play without a node folder, which is the [`_NODE` file](/setup/audio#file-based-media-the-master-needs-the-file-too) if there is one, otherwise the master's file.

## Audio

Node audio is played only when **Enable audio on nodes** is turned on in the [audio settings](/settings/audio). Volume and mute follow the master, as usual.

When the master changes audio track, each node selects its audio file **by file name**:

* If the master's current track comes from a separate file, for example `Show_SV.wav`, a node that has a file with the same name plays that file.
* Otherwise, for example when the master plays the audio embedded in its video, or the node has no file with that name, the node plays its **first** audio file in alphabetical order.

So name the audio files the same on the master and on the nodes. Nodes can have fewer audio files than the master.

## Requirements

As with `_NODE` files, **the master decides which files the nodes play**. It looks in the `_NODES` folder on its own disk when the file is opened, and sends the result to the nodes. Therefore:

* The `_NODES` folder must exist **on the master**, next to the media file. It must contain every node's folder and files. Large files can be replaced by small placeholder files with the same names on the master, because the master never plays the node files.
* Each node must have **its own** files at the same path, or at a path that C-Play's [file locations](/settings/location) resolve on that node. A node needs only its own folder.
* Changes to the folder take effect the next time the file is opened.

The master still plays its own file and drives playback for the whole cluster, as for all file-based media. The playlist, `.cplayfile` files, sections, grid and stereoscopic mode, fades and the node load check work the same way as without node folders.

## Checking the setup

Look in the master's log when you open the file. It lists the folder that was found and what each node folder plays (`video + audio`, `video only` or `audio only`). It also warns about:

* folders that are not node ids in `data/nodes.json`,
* node folders that contain neither a matching video file nor audio files,
* folders that contain several video files with the master's name.

Each node logs which file it opens, or that it has no folder and plays the master's file.

## When to use a multi-video composition instead

Use a [`.cplaymulti` composition](/media/cplaymulti) when a node must play **several videos at the same time** (for example left and right eye), or when entries need their own grid, stereo or eye mode. A per-node media folder plays one file per node, and it uses the master's playback settings.
