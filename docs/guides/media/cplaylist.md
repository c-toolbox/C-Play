---
title: Build playlists (*.cplaylists)
layout: home
nav_order: 6
parent: Media file structure
---

# Build and save a playlist (*.cplaylist)

Playlists in C-Play are essentially lists of [cplayfiles](cplayfile), which are descriptions (mapping, sections etc) of specific videos.

C-Play requires you to save a *cplayfile* of your video to make sure that the mappings and other parameters are correct between loading of different files.

The playlist view, where items are created and managed, is opened in the bottom right (or bottom left, depending on settings).

The top toolbar provides the main controls for managing the playlist:

| Icon | Action |
|------|--------|
| ![](../../assets/icons/document-open.svg) | Open a saved playlist |
| ![](../../assets/icons/document-replace.svg) | New playlist (clear all items) |
| ![](../../assets/icons/list-add.svg) | Add item to playlist |
| ![](../../assets/icons/list-remove.svg) | Remove selected item |
| ![](../../assets/icons/view-form.svg) | View / edit selected playlist entry |
| ![](../../assets/icons/pan-up-symbolic.svg) | Move selected item up |
| ![](../../assets/icons/pan-down-symbolic.svg) | Move selected item down |

The end-of-file mode button and the second row are described below.

### ![](../../assets/icons/media-playlist-play.svg) Auto-play

The ![](../../assets/icons/media-playlist-play-lime.svg) / ![](../../assets/icons/media-playlist-play-crimson.svg) toggle controls whether the playlist starts playing automatically when a file is loaded. Lime means auto-play is on, crimson means off.

### ![](../../assets/icons/system-save-session.svg) Save playlist

Save the playlist by pressing the ![](../../assets/icons/system-save-session-lime.svg) / ![](../../assets/icons/system-save-session-orange.svg) disk icon. It is lime when the playlist is saved, and turns orange when unsaved changes exist.

### End-of-file mode

Each item in the playlist has an end-of-file (EOF) mode that controls what happens when the media finishes playing. The EOF button icon changes to reflect the current mode:

* ![](../../assets/icons/media-playback-pause.svg) **Pause** — Playback stops at the end of the file.
* ![](../../assets/icons/go-next.svg) **Continue** — Automatically advances to the next item in the playlist.
* ![](../../assets/icons/media-playlist-repeat.svg) **Loop** — Repeats the current item indefinitely.

### Per-item overrides

Starting from C-Play v2.3, the playlist supports per-item overrides that take priority over the values stored in the referenced *.cplayfile*. These let you customize how individual items behave without modifying the original media description.

Per-item overrides are edited in the ![](../../assets/icons/view-form.svg) *"View / edit selected playlist entry"* window, opened from the playlist toolbar.

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
