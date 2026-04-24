---
title: Build playlists (*.cplaylists)
layout: home
nav_order: 6
parent: Media file structure
---

# Build and save a playlist (*.cplaylist)

Playlists in C-Play are essentially lists of [cplayfiles](cplayfile.md), which are descriptions (mapping, sections etc) of specific videos.

C-Play requires you to save a *cplayfile* of your video to make sure that the mappings and other parameters are correct between loading of different files.

The playlist view, where items are created and managed, is opened in the bottom right (or bottom left, depending on settings).

It should be straight-forward to build a playlist from adding and removing *cplayfiles*, moving them around, and changing the settings for the selected one.

Save the playlist by pressing the disk icon, in the middle of the top bar in the playlist view.

### End-of-file mode

Each item in the playlist has an end-of-file (EOF) mode that controls what happens when the media finishes playing:

* **Pause** — Playback stops at the end of the file.
* **Continue** — Automatically advances to the next item in the playlist.
* **Loop** — Repeats the current item indefinitely.

### Per-item overrides

Starting from C-Play v2.3, the playlist supports per-item overrides that take priority over the values stored in the referenced *.cplayfile*. These let you customize how individual items behave without modifying the original media description.

#### Title override

Set a custom display title for a playlist entry. When enabled, this title is shown in the playlist view instead of the one from the media file or *.cplayfile*.

#### Start and end time

Override the playback range for a specific item. This allows you to play only a portion of the media without editing the *.cplayfile* itself.

* **Start time** — Time in seconds where playback begins.
* **End time** — Time in seconds where playback ends.

#### Stereo mode override

Override the stereoscopic format for a specific item:

* 2D (mono)
* 3D side-by-side
* 3D top-bottom
* 3D top-bottom-flip

#### Grid mode override

Override the grid/mapping mode for a specific item:

* Pre-split
* Plane (flat)
* Dome
* Sphere EQR (equirectangular)
* Sphere EAC (equi-angular cubemap)

#### Audio file override

Specify an alternative audio file to use with a playlist item instead of the embedded or linked audio from the *.cplayfile*.

### File format

The *.cplaylist* file uses JSON. Each item in the `"playlist"` array has a required `"file"` path and `"on_file_end"` mode. Override fields are only written when explicitly enabled:

```json
{
  "playlist": [
    {
      "file": "path/to/media.cplayfile",
      "on_file_end": "continue",
      "list_title": "Custom Title",
      "list_file_start": 10.5,
      "list_file_end": 120.0,
      "list_stereo_mode": 1,
      "list_grid_mode": 3,
      "list_audio_file": "path/to/audio.wav"
    },
    {
      "file": "another_video.mp4",
      "on_file_end": "pause"
    }
  ]
}
```

File paths in the playlist are stored as relative paths when possible. When loading, C-Play resolves them against the playlist file directory and the configured location settings.
