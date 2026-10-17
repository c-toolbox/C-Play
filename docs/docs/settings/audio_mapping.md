---
title: Audio mapping
sidebar_position: 1.5
---

# Audio mapping

*New in C-Play v2.4.*

The audio mapping decides which speakers play the sound of the **live layers** — the layers that play their audio through PortAudio: **NDI**, **OMT**, **Capture**, **DirectShow** and **WebRTC**. (Video, audio and other mpv-based layers follow the regular [audio output settings](/settings/audio#audio-output) instead.)

It has three levels, each one building on the previous:

| Level | Where | What it does |
| --- | --- | --- |
| 1. Up/down mix | [Audio settings](/settings/audio#audio-settings-for-live-video-layers) → *Perform audio mixing (up/down) to output device* and *Output channels* | Mixes every live layer to one output layout, for example 2.0 → 5.1 or 7.1 → 2.0. |
| 2. Spatial mapping | *Audio mapping...* dialog (button below *Output channels*) | Optional. Makes the sound follow where each layer is shown: **surround panning** of the output layout, or **mono to a speaker array** by distance. |
| 3. Per layer | Layer View, above the audio meter | Each live layer can opt out of the up/down mix and of the spatial mapping. Both are on by default. |

When a level is off, the levels after it do nothing: without the up/down mix the live layers play their source channels unmixed, and without a spatial mapping they keep the plain up/down mix.

## 1. Up/down mix to the output layout

Turn on **Perform audio mixing (up/down) to output device** and choose **Output channels** in the [audio settings](/settings/audio#audio-settings-for-live-video-layers). The supported layouts and their channel order on the device are:

| Layout | Channels | Order |
| --- | --- | --- |
| 2.0 Stereo | 2 | FL FR |
| 2.1 Stereo + LFE | 3 | FL FR LFE |
| 5.0 Surround | 5 | FL FR C SL SR |
| 5.1 Surround + LFE | 6 | FL FR C LFE SL SR |
| 7.0 Surround | 7 | FL FR C SL SR RL RR |
| 7.1 Surround + LFE | 8 | FL FR C LFE SL SR RL RR |

* **Mono and stereo sources** are spread over a larger layout: the fronts are kept, the centre gets a phantom centre (L + R at -9 dB), the surrounds get some of the same side (-6 dB, rears -12 dB) and the LFE gets the low end of both channels.
* **Multichannel sources** keep every channel that the output layout has. Channels it lacks are folded into the closest ones at -3 dB (for example 7.1 → 5.1 folds the rears into the sides, 5.1 → 2.0 folds centre and surrounds into the fronts), and any output that would get more than full scale is scaled down.
* With the option off, each live layer opens the device with its own channel count and plays it unmixed.

## 2. Spatial mapping

Click **Audio mapping...** below *Output channels* in the audio settings to open the dialog. The dialog is not modal, so you can keep it open while placing speakers in the master view. Select the mode in **Spatial mapping**:

* **Off (standard up/down mix)** — only level 1 applies.
* **Surround panning of the output layout** — the speakers of the output layout are placed around the dome.
* **Mono to a speaker array, by distance** — any number of speakers, each on its own output channel.

The spatial mapping needs the up/down mix (level 1) to be on; the dialog says so when it is not.

### Surround panning

![Audio mapping dialog: surround panning](/assets/ui/settings/audio_mapping_surround.png)

Each channel of a layer is panned toward the speakers of the output layout closest to where the layer is shown, so moving a layer around the dome moves its sound with it. The total loudness stays constant while a layer moves.

* **Focus** — how sharply the sound is panned (1–20, default 4). Low values spread it over many speakers, high values keep it at the speakers nearest the layer.
* **Stereo width** — how far the left and right channels spread across a flat layer (0–100 %, default 100 %). The left channels play from the left part of the layer and the right channels from the right part; at 0 % all channels play from the layer's centre.
* **Speakers** — one row per output channel with **Azimuth** (clockwise from the dome front, 0°) and **Elevation** (upward from the horizon, 90° is the zenith). The defaults are the standard angles of the layout (FL/FR ±30°, SL/SR ±110° for 5.x, ±90° for 7.x, RL/RR ±150°). The LFE channel is not placed: it gets the low end of every layer. **Reset to default positions** restores the standard angles.

The positions belong to one output layout. When you change *Output channels*, the speakers start again from that layout's standard angles.

### Mono to a speaker array

![Audio mapping dialog: mono to a speaker array](/assets/ui/settings/audio_mapping_array.png)

Each layer is summed to mono and shared out over any number of speakers (up to 64), each on its own output channel. The closer a speaker is to the layer, the larger its share. This suits installations where speakers are spread around a dome or along a circular screen rather than in a standard surround layout.

* **Speaker positions** —
  * **On the dome (azimuth / elevation)**: speakers are placed like the surround speakers, and compared with the direction of the layer as seen from the dome centre.
  * **Along the circular screen (degrees / height)**: for the [circular surface mapping](/settings/grid#circular-surface-mapping). A speaker is placed at an angle on the screen (the same degrees as the *Centre angle* of circular plane layers) and a height in cm (like the layer's vertical position). The distance is measured around the real circle, so a layer at 350° is close to a speaker at 0°. The *Full surface width* of the circular mapping is taken as the circumference of the screen, which sets how much the height counts compared with the angle. Only plane layers have a position on the circular screen, and only while the circular mapping is enabled.
* **Level drop** — how much quieter a speaker gets per doubling of its distance to the layer (1–24 dB, default 6 dB, the inverse distance law). Higher values keep the sound at the nearest speakers.
* **Spatial blur** — added to every distance, in percent of the dome/circle radius (default 5 %). It avoids that a layer right on top of a speaker plays from that speaker only.
* **Constant loudness** — on (default): the shares are shares of the power, so the loudness stays the same wherever the layer is. Off: the amplitude shares sum to 100 %.
* **Speakers** — each row has the speaker's **output ch** (1–64) and its position. **Add speaker** adds one next to the last one, **Create evenly spaced** replaces the list with the given number of speakers spaced evenly around the dome or the screen on output channels 1 and up, and the red cross removes a speaker. Several speakers can share an output channel.

The device opens as many channels as the highest output channel used by a speaker (limited to what the device has). Layers without a position — 2D layers, and in the circular layout also dome and sphere layers — play equally on all speakers.

**Share of each speaker:** select a 3D layer in the Layers panel and the dialog shows the share of every speaker for that layer, updated live while the layer moves. With constant loudness on, the share is the part of the power; otherwise the part of the amplitude. For example, with 8 speakers 45° apart, a layer right on a speaker gives it about 99 %, and a layer halfway between two speakers gives each about 41 %.

### Placing the speakers in the master view

![Speakers shown in the master 3D view](/assets/ui/settings/audio_mapping_master_view.png)

Check **Show speakers in the master view** to draw the speakers in the 3D view (the view switches to the 3D view with the perspective camera if the flat video view is shown; the fisheye view works too). Speakers of the surround layout are labelled with their channel name, speakers of the array with their output channel, and array speakers also show their share of the selected layer.

* **Drag a speaker** to move it. On the dome it follows the dome surface; in the circular layout it follows the unwrapped 360° screen, drawn at the distance of the circular layers.
* **Select a speaker** by pressing it in the view or with the round button in its row, then check **Click in the view places the selected speaker**: a click in the view moves the selected speaker there. Dragging still orbits the camera.
* The rows in the dialog show the exact values and accept typed values.

Closing the dialog hides the speakers again; the positions are saved as soon as they change.

## 3. Per layer: opt out of the mapping

![Layer View audio options](/assets/ui/layer_view_audio_mapping.png)

The Layer View of every live layer shows its audio options above the audio meter toggle:

* **Map audio to the output layout (up/down mix)** — shown while *Perform audio mixing (up/down) to output device* is on. Uncheck it to play this layer's source channels unmixed; its audio output reopens with its own channel count.
* **Follow the spatial mapping (surround panning / speaker array by distance)** — shown while a spatial mapping is selected, and available only while the layer uses the up/down mix. Uncheck it to give this layer the plain up/down mix, for example for a presenter microphone that should play from all fronts regardless of where its video is shown. In speaker array mode such a layer gets the standard up/down mix onto the first output channels.

Both options are on by default whenever the mapping is enabled in the audio settings. They are saved with the layer in the presentation only when turned off (`"audioMapping": false`, `"spatialAudio": false`), so existing presentations follow the mapping, and they are synced to the nodes.

## Good to know

* **Audio on the nodes** — the mapping settings and the speaker positions are read from each machine's own audio settings. When nodes play audio (*Enable audio on: Nodes / Clients*), configure the mapping on those nodes as well; the per-layer options come from the master.
* **Coherent stereo in the speaker array** — a stereo source whose left and right channels are identical sounds about 3 dB louder when summed to mono for the array.
* **No position** — 2D layers have no place in the dome: with surround panning they keep the standard up/down mix, in speaker array mode they play equally on all speakers.
* **Audio level meter** — the meter in the Layer View shows the level after the mapping, for all live layers.
