---
title: Audio settings
sidebar_position: 1
---

# Audio settings

![Audio settings](/assets/ui/settings/audio.png) 

The audio settings within C-Play let you configure audio output devices, volume behavior, and track preferences.

### General

* **Enable audio on master** — Enable audio playback on the master application (default on).
* **Enable audio on nodes** — Enable audio playback on cluster nodes (default off).
* **Volume at startup** — Initial volume level when C-Play starts (0–100, default 100).
* **Volume step** — How much the volume changes per increment in the UI (0–100, default 5).

### Audio output

If *"Use custom audio output"* is not selected, C-Play uses the system default audio device.

* **Use custom audio output** — Enable manual selection of audio output (default off).
* **Use audio device** — Target a specific audio device from the detected list.
* **Use audio driver** — Target an audio driver instead (jack, openal, oss, pcm, pulse, wasapi).
* **Preferred audio output device** — The selected device name.
* **Preferred audio output driver** — The selected driver name.

For multi-channel audio setups such as JACK, see the [Audio configuration guide](/setup/audio).

### Audio track preferences

* **Load audio files in same folder** — Automatically load adjacent audio files as additional tracks when opening a video (default on).
* **Preferred language** — IETF language tag for preferred audio track language (e.g. `eng`, `ger`).
* **Preferred track** — Preferred audio track number. Leave at 0 for automatic selection.

### Audio settings for live video layers

The live layers — **NDI**, **OMT**, **Capture**, **DirectShow** and **WebRTC** — play their audio through PortAudio, separately from the mpv-based media playback above. These settings apply to all of them. (The section is shown in builds with NDI, OMT or capture support.)

* **Enable audio on: Master / Nodes / Clients** — also decides where the live layers play their audio. Changing it requires a restart to take effect for live layers.
* **Use custom audio output** — Use a specific PortAudio output device for the live layers instead of the system default (default off).
* **Use audio device** — The PortAudio output device (with its host API, e.g. WASAPI or ASIO). The line below shows the device's channel count and default sample rate; the refresh button rescans the devices.
* **Perform audio mixing (up/down) to output device** — Mix every live layer to the output layout chosen below (default off). When off, each live layer plays its source channels unmixed.
* **Output channels** — The output layout: 2.0, 2.1, 5.0, 5.1, 7.0 or 7.1 (default 6 / 5.1).
* **Audio mapping...** — Opens the [Audio mapping](/settings/audio_mapping) dialog for the optional spatial mapping: surround panning that makes the sound follow where each layer is shown, or a mono speaker array by distance, including speaker placement in the master view. The text next to the button shows whether a spatial mapping is on.

Each live layer can opt out of the up/down mix and of the spatial mapping in its Layer View; both are on by default. See [Audio mapping](/settings/audio_mapping) for the complete structure.
