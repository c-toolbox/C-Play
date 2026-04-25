---
title: HTTP Web API
layout: home
nav_order: 1
parent: Remote Control
---

# HTTP Web API

The HTTP Web API allows external systems to control C-Play remotely. Enable or disable it in `data/http-server-conf.json`, where you can also configure the port (default 7007).

All endpoints accept **HTTP POST** requests. Unless noted otherwise, both request and response bodies are plain text.

A sample Medialon Manager 7 project demonstrating these commands is available [here](https://github.com/c-toolbox/C-Play/tree/master/help/http_server).

---

## General

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/status` | — | Check connection | `"OK"` |
| `/quit` | — | Quit C-Play | `"Quitting C-Play"` |
| `/media_title` | — | Get current media title | Title string |
| `/play` | — | Play current media | `"Play"` |
| `/pause` | — | Pause current media | `"Pause"` |
| `/stop` | — | Stop/rewind to start | `"Stop/rewind"` |
| `/rewind` | — | Same as `/stop` | `"Stop/rewind"` |

## Time & position

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/position` | `format=` *(optional)* — e.g. `hh:mm:ss/zz` | Get current position | Seconds or formatted string |
|  | `set=` *(optional)* — seconds | Set position | (same) |
| `/seek` | `time=` *(required)* — seconds (negative = backward) | Seek relative | New position |
|  | `format=` *(optional)* | | Formatted string |
| `/remaining` | `format=` *(optional)* | Remaining time | Seconds or formatted string |
| `/duration` | `format=` *(optional)* | Total duration | Seconds or formatted string |
| `/speed` | `factor=` *(optional)* — `0.01`–`100` | Get or set playback speed | Speed factor |
| `/auto_play` | `on=` *(optional)* — `1` or `0` | Get or set auto-play | `0`/`1`, or confirmation message |

## Volume & visibility

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/volume` | `level=` *(optional)* — `0`–`100` | Get or set audio volume | Volume level (0–100) |
| `/visibility` | — | Get media visibility | Value (0–100) |
| `/fade_duration` | `format=` *(optional)* | Get fade duration | Seconds or formatted string |
| `/fade_volume_down` | — | Fade volume to zero | `"Fading volume down"` |
| `/fade_volume_up` | — | Fade volume back up | `"Fading volume up"` |
| `/fade_image_down` | — | Fade media visibility to 0% | `"Fading image down"` |
| `/fade_image_up` | — | Fade media visibility to 100% | `"Fading image up"` |
| `/sync_image_volume_fade` | `value=` *(optional)* — `1` or `0` | Get or set sync between volume and visibility fading | `0`/`1`, or confirmation message |

## Media modes

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/stereo_mode` | — | Get stereoscopic mode | `0`–`3` (see below) |
| `/grid_mode` | — | Get grid/mapping mode | `0`–`4` (see below) |
| `/eof_mode` | — | Get end-of-file mode (alias: `/loop_mode`) | `0`–`2` (see below) |
| `/loop_mode` | — | Get end-of-file mode | `0`–`2` (see below) |
| `/view_mode` | `mode=` *(optional)* — `0` or `1` | Get or set 2D/3D view mode | `0`–`1` (see below) |

**Stereo mode values:**

| Value | Mode |
|-------|------|
| `0` | 2D (mono) |
| `1` | 3D side-by-side |
| `2` | 3D top-bottom |
| `3` | 3D top-bottom-flip |

**Grid mode values:**

| Value | Mode |
|-------|------|
| `0` | Pre-split |
| `1` | Plane |
| `2` | Dome |
| `3` | Sphere EQR |
| `4` | Sphere EAC |

**EOF mode values:**

| Value | Mode |
|-------|------|
| `0` | Pause |
| `1` | Continue to next |
| `2` | Loop |

**View mode values:**

| Value | Mode |
|-------|------|
| `0` | Auto 2D/3D switch |
| `1` | Force 2D for all |

## Background & foreground images

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/background_image` | `on=` *(optional)* — `1` or `0` | Get or set background visibility | `0`/`1`, or confirmation message |
| `/background_image_stereo_mode` | — | Get background stereo mode | `0`–`3` |
| `/background_image_grid_mode` | — | Get background grid mode | `0`–`4` |
| `/foreground_image` | `on=` *(optional)* — `1` or `0` | Get or set foreground visibility | `0`/`1`, or confirmation message |
| `/foreground_image_stereo_mode` | — | Get foreground stereo mode | `0`–`3` |
| `/foreground_image_grid_mode` | — | Get foreground grid mode | `0`–`4` |

## Playlist

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/playlist` | `charsPerItem=` *(optional)* | Get formatted playlist | Text list |
| `/playing_in_playlist` | — | Current playlist index | Index, or `-1` |
| `/load_from_playlist` | `index=` *(required)* | Load playlist item | Confirmation or error |

## Audio tracks

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/audiotracks` | `charsPerItem=` *(optional)*, `removeLoadedFilePrefix=1` *(optional)* | Get formatted audio track list | Text list |
| `/playing_in_audiotracks` | — | Current audio track index | Index, or `-1` |
| `/load_from_audiotracks` | `index=` *(required)* | Load audio track | Confirmation or error |

## Sections

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/sections` | `charsPerItem=` *(optional)* | Get formatted section list | Text list |
| `/playing_in_sections` | — | Current section index | Index, or `-1` |
| `/load_from_sections` | `index=` *(required)* | Load section | Confirmation or error |
| `/section_start_time` | `format=` *(optional)* | Current section start time | Seconds or formatted string |
| `/section_end_time` | `format=` *(optional)* | Current section end time | Seconds or formatted string |
| `/section_end_mode` | — | Current section end mode | `0`–`4` (see below) |

**Section end mode values:**

| Value | Mode |
|-------|------|
| `0` | Pause |
| `1` | Fade out (then pause) |
| `2` | Continue |
| `3` | Next |
| `4` | Loop |

## Slides & layers

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/slide_name` | — | Get current slide name | Slide name, or empty string |
| `/slide_previous` | — | Go to previous slide | `"Previous slide"` |
| `/slide_next` | — | Go to next slide | `"Next slide"` |
| `/slides` | `charsPerItem=` *(optional)* | Get formatted slide list | Text list |
| `/playing_in_slides` | — | Current slide index (`-1` = master) | Index |
| `/select_from_slides` | `index=` *(required)* | Select slide (pre-loads layers) | Confirmation or error |
| `/load_from_slides` | `index=` *(required)* | Load/trigger slide | Confirmation or error |
| `/layers` | `slide_name=` or `slide_idx=` *(required)*, `charsPerItem=` *(optional)* | Get formatted layer list for a slide | Text list |

### Layer control

All layer endpoints require a layer identifier and an optional slide identifier:

| Param | Description |
|-------|-------------|
| `layer_title=` | Layer title (name) |
| `layer_idx=` | Layer index (alternative to title) |
| `slide_name=` | Slide name *(optional — defaults to master slide)* |
| `slide_idx=` | Slide index *(optional — alternative to name, `-1` for master)* |

| Endpoint | Additional params | Description | Returns |
|----------|-------------------|-------------|---------|
| `/layer_volume` | `level=` *(optional)* — `0`–`100` | Get or set layer volume | Volume level (0–100) |
| `/layer_visibility` | `value=` *(optional)* — `0`–`100` | Get or set layer visibility | Visibility (0–100) |
| `/layer_plane` | `azimuth=`, `elevation=`, `roll=`, `distance=`, `horizontal=`, `vertical=` *(any combination)* | Get or set layer plane parameters. Send a param with no value to read. | One value per param, newline-separated |

## Spin & orientation

| Endpoint | Params | Description | Returns |
|----------|--------|-------------|---------|
| `/orientation_reset` | — | Reset grid orientation to defaults | `"Origin Reset"` |
| `/surface_transition` | `format=` *(optional)* | Run transition to alternative values | Transition time |
| `/spin_pitch_up` | `on=` — `1` or `0` | Enable/disable pitch up | Confirmation or error |
| `/spin_pitch_down` | `on=` — `1` or `0` | Enable/disable pitch down | Confirmation or error |
| `/spin_yaw_left` | `on=` — `1` or `0` | Enable/disable yaw left | Confirmation or error |
| `/spin_yaw_right` | `on=` — `1` or `0` | Enable/disable yaw right | Confirmation or error |
| `/spin_roll_ccw` | `on=` — `1` or `0` | Enable/disable roll counter-clockwise | Confirmation or error |
| `/spin_roll_cw` | `on=` — `1` or `0` | Enable/disable roll clockwise | Confirmation or error |

## JSON

| Endpoint | Description | Returns |
|----------|-------------|---------|
| `/playfile_json` | Current media state as cplayfile | JSON |
| `/playlist_json` | Full playlist | JSON |