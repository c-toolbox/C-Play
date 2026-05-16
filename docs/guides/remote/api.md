---
title: HTTP API + WebSockets
layout: home
nav_order: 1
parent: Remote control
---

# HTTP API and HTTP/WebSockets Commands

This page covers three topics:

1. [HTTP API for C-Play](#http-api-for-c-play) — endpoints for controlling C-Play remotely
2. [REST (HTTP and WebSockets) Commands Editor](#rest-commands-editor) — built-in client for managing and testing HTTP and WebSocket commands
3. [REST Layer](#rest-layer) — non-visual layer type that fires HTTP or WebSocket requests from slides

---

## HTTP API for C-Play

The HTTP Web API allows external systems to control C-Play remotely. Enable or disable it in `data/http-server-conf.json`, where you can also configure the port (default 7007).

Endpoints support **HTTP POST** and/or **HTTP GET** requests. Unless noted otherwise, both request and response bodies are plain text.

A sample Medialon Manager 7 project demonstrating these commands is available [here](https://github.com/c-toolbox/C-Play/tree/master/help/http_server).

---

## HTTP methods

Endpoints are divided into two categories:

- **GET + POST** — retrieval/get-set endpoints that support both methods. The **GET** handler returns only the current value (no parameters required). The **POST** handler can both set and return values.
- **POST only** — action endpoints that trigger a state change and only accept POST.

### GET + POST endpoints

`/status`, `/position`, `/remaining`, `/duration`, `/auto_play`, `/speed`, `/volume`,
`/fade_duration`, `/sync_image_volume_fade`, `/visibility`, `/stereo_mode`, `/grid_mode`,
`/background_image`, `/background_image_stereo_mode`, `/background_image_grid_mode`,
`/foreground_image`, `/foreground_image_stereo_mode`, `/foreground_image_grid_mode`,
`/eof_mode`, `/loop_mode`, `/view_mode`, `/media_title`,
`/audiotracks`, `/playing_in_audiotracks`, `/playlist`, `/playing_in_playlist`,
`/sections`, `/playing_in_sections`,
`/section_start_time`, `/section_end_time`, `/section_end_mode`,
`/playfile_json`, `/playlist_json`,
`/slide_name`, `/slides`, `/playing_in_slides`,
`/layers`, `/layer_volume`, `/layer_visibility`, `/layer_plane`

### POST only endpoints

`/quit`, `/play`, `/pause`, `/stop`, `/rewind`, `/seek`,
`/fade_volume_down`, `/fade_volume_up`, `/fade_image_down`, `/fade_image_up`,
`/spin_pitch_up`, `/spin_pitch_down`, `/spin_yaw_left`, `/spin_yaw_right`,
`/spin_roll_ccw`, `/spin_roll_cw`, `/orientation_reset`, `/surface_transition`,
`/slide_previous`, `/slide_next`,
`/load_from_audiotracks`, `/load_from_playlist`, `/load_from_sections`,
`/load_from_slides`, `/select_from_slides`

---

## General

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/status` | GET, POST | — | Check connection | `"OK"` |
| `/quit` | POST | — | Quit C-Play | `"Quitting C-Play"` |
| `/media_title` | GET, POST | — | Get current media title | Title string |
| `/play` | POST | — | Play current media | `"Play"` |
| `/pause` | POST | — | Pause current media | `"Pause"` |
| `/stop` | POST | — | Stop/rewind to start | `"Stop/rewind"` |
| `/rewind` | POST | — | Same as `/stop` | `"Stop/rewind"` |

## Time & position

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/position` | GET, POST | `format=` *(optional)* — e.g. `hh:mm:ss/zz` | Get current position | Seconds or formatted string |
|  |  | `set=` *(optional)* — seconds | Set position | (same) |
| `/seek` | POST | `time=` *(required)* — seconds (negative = backward) | Seek relative | New position |
|  |  | `format=` *(optional)* | | Formatted string |
| `/remaining` | GET, POST | `format=` *(optional)* | Remaining time | Seconds or formatted string |
| `/duration` | GET, POST | `format=` *(optional)* | Total duration | Seconds or formatted string |
| `/speed` | GET, POST | `factor=` *(optional)* — `0.01`–`100` | Get or set playback speed | Speed factor |
| `/auto_play` | GET, POST | `on=` *(optional)* — `1` or `0` | Get or set auto-play | `0`/`1`, or confirmation message |

## Volume & visibility

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/volume` | GET, POST | `level=` *(optional)* — `0`–`100` | Get or set audio volume | Volume level (0–100) |
| `/visibility` | GET, POST | — | Get media visibility | Value (0–100) |
| `/fade_duration` | GET, POST | `format=` *(optional)* | Get fade duration | Seconds or formatted string |
| `/fade_volume_down` | POST | — | Fade volume to zero | `"Fading volume down"` |
| `/fade_volume_up` | POST | — | Fade volume back up | `"Fading volume up"` |
| `/fade_image_down` | POST | — | Fade media visibility to 0% | `"Fading image down"` |
| `/fade_image_up` | POST | — | Fade media visibility to 100% | `"Fading image up"` |
| `/sync_image_volume_fade` | GET, POST | `value=` *(optional)* — `1` or `0` | Get or set sync between volume and visibility fading | `0`/`1`, or confirmation message |

## Media modes

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/stereo_mode` | GET, POST | — | Get stereoscopic mode | `0`–`3` (see below) |
| `/grid_mode` | GET, POST | — | Get grid/mapping mode | `0`–`4` (see below) |
| `/eof_mode` | GET, POST | — | Get end-of-file mode (alias: `/loop_mode`) | `0`–`2` (see below) |
| `/loop_mode` | GET, POST | — | Get end-of-file mode | `0`–`2` (see below) |
| `/view_mode` | GET, POST | `mode=` *(optional)* — `0` or `1` | Get or set 2D/3D view mode | `0`–`1` (see below) |

**Stereo mode values:**

| Value | Mode |
|:-:|------|
| `0` | 2D (mono) |
| `1` | 3D side-by-side |
| `2` | 3D top-bottom |
| `3` | 3D top-bottom-flip |

**Grid mode values:**

| Value | Mode |
|:-:|------|
| `0` | Pre-split |
| `1` | Plane |
| `2` | Dome |
| `3` | Sphere EQR |
| `4` | Sphere EAC |

**EOF mode values:**

| Value | Mode |
|:-:|------|
| `0` | Pause |
| `1` | Continue to next |
| `2` | Loop |

**View mode values:**

| Value | Mode |
|:-:|------|
| `0` | Auto 2D/3D switch |
| `1` | Force 2D for all |

## Background & foreground images

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/background_image` | GET, POST | `on=` *(optional)* — `1` or `0` | Get or set background visibility | `0`/`1`, or confirmation message |
| `/background_image_stereo_mode` | GET, POST | — | Get background stereo mode | `0`–`3` |
| `/background_image_grid_mode` | GET, POST | — | Get background grid mode | `0`–`4` |
| `/foreground_image` | GET, POST | `on=` *(optional)* — `1` or `0` | Get or set foreground visibility | `0`/`1`, or confirmation message |
| `/foreground_image_stereo_mode` | GET, POST | — | Get foreground stereo mode | `0`–`3` |
| `/foreground_image_grid_mode` | GET, POST | — | Get foreground grid mode | `0`–`4` |

## Playlist

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/playlist` | GET, POST | `charsPerItem=` *(optional)* | Get formatted playlist | Text list |
| `/playing_in_playlist` | GET, POST | — | Current playlist index | Index, or `-1` |
| `/load_from_playlist` | POST | `index=` *(required)* | Load playlist item | Confirmation or error |

## Audio tracks

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/audiotracks` | GET, POST | `charsPerItem=` *(optional)*, `removeLoadedFilePrefix=1` *(optional)* | Get formatted audio track list | Text list |
| `/playing_in_audiotracks` | GET, POST | — | Current audio track index | Index, or `-1` |
| `/load_from_audiotracks` | POST | `index=` *(required)* | Load audio track | Confirmation or error |

## Sections

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/sections` | GET, POST | `charsPerItem=` *(optional)* | Get formatted section list | Text list |
| `/playing_in_sections` | GET, POST | — | Current section index | Index, or `-1` |
| `/load_from_sections` | POST | `index=` *(required)* | Load section | Confirmation or error |
| `/section_start_time` | GET, POST | `format=` *(optional)* | Current section start time | Seconds or formatted string |
| `/section_end_time` | GET, POST | `format=` *(optional)* | Current section end time | Seconds or formatted string |
| `/section_end_mode` | GET, POST | — | Current section end mode | `0`–`4` (see below) |

**Section end mode values:**

| Value | Mode |
|:-:|------|
| `0` | Pause |
| `1` | Fade out (then pause) |
| `2` | Continue |
| `3` | Next |
| `4` | Loop |

## Slides & layers

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/slide_name` | GET, POST | — | Get current slide name | Slide name, or empty string |
| `/slide_previous` | POST | — | Go to previous slide | `"Previous slide"` |
| `/slide_next` | POST | — | Go to next slide | `"Next slide"` |
| `/slides` | GET, POST | `charsPerItem=` *(optional)* | Get formatted slide list | Text list |
| `/playing_in_slides` | GET, POST | — | Current slide index (`-1` = master) | Index |
| `/select_from_slides` | POST | `index=` *(required)* | Select slide (pre-loads layers) | Confirmation or error |
| `/load_from_slides` | POST | `index=` *(required)* | Load/trigger slide | Confirmation or error |
| `/layers` | GET, POST | `slide_name=` or `slide_idx=` *(required)*, `charsPerItem=` *(optional)* | Get formatted layer list for a slide | Text list |

### Layer control

All layer endpoints require a layer identifier and an optional slide identifier:

| Param | Description |
|-------|-------------|
| `layer_title=` | Layer title (name) |
| `layer_idx=` | Layer index (alternative to title) |
| `slide_name=` | Slide name *(optional — defaults to master slide)* |
| `slide_idx=` | Slide index *(optional — alternative to name, `-1` for master)* |

To target a **sublayer** (e.g. a Division Mode cell), add one of:

| Param | Description |
|-------|-------------|
| `sub_layer_title=` | Sublayer title (e.g. `Div_0_0`) |
| `sub_layer_idx=` | Sublayer index (0-based) |

If neither sublayer param is provided, the parent layer is used. If the parent layer has no sublayers and a sublayer param is given, an error is returned.

| Endpoint | Method | Additional params | Description | Returns |
|----------|--------|-------------------|-------------|---------|
| `/layer_volume` | GET, POST | `level=` *(optional)* — `0`–`100` | Get or set layer volume | Volume level (0–100) |
| `/layer_visibility` | GET, POST | `value=` *(optional)* — `0`–`100` | Get or set layer visibility | Visibility (0–100) |
| `/layer_plane` | GET, POST | `azimuth=`, `elevation=`, `roll=`, `distance=`, `horizontal=`, `vertical=` *(any combination)* | Get or set layer plane parameters. Send a param with no value to read. | One value per param, newline-separated |

## Spin & orientation

| Endpoint | Method | Params | Description | Returns |
|----------|--------|--------|-------------|---------|
| `/orientation_reset` | POST | — | Reset grid orientation to defaults | `"Origin Reset"` |
| `/surface_transition` | POST | `format=` *(optional)* | Run transition to alternative values | Transition time |
| `/spin_pitch_up` | POST | `on=` — `1` or `0` | Enable/disable pitch up | Confirmation or error |
| `/spin_pitch_down` | POST | `on=` — `1` or `0` | Enable/disable pitch down | Confirmation or error |
| `/spin_yaw_left` | POST | `on=` — `1` or `0` | Enable/disable yaw left | Confirmation or error |
| `/spin_yaw_right` | POST | `on=` — `1` or `0` | Enable/disable yaw right | Confirmation or error |
| `/spin_roll_ccw` | POST | `on=` — `1` or `0` | Enable/disable roll counter-clockwise | Confirmation or error |
| `/spin_roll_cw` | POST | `on=` — `1` or `0` | Enable/disable roll clockwise | Confirmation or error |

## JSON

| Endpoint | Method | Description | Returns |
|----------|--------|-------------|---------|
| `/playfile_json` | GET, POST | Current media state as cplayfile | JSON |
| `/playlist_json` | GET, POST | Full playlist | JSON |

---

## REST Layer

The **REST Layer** is a non-visual layer type that fires an HTTP or WebSocket request when triggered as part of the slide/layer system. It exists on the master only and can be used to integrate external systems (lighting, projectors, other applications) into your slide workflow.

### Properties

| Property | Description |
|----------|-------------|
| URL | Target endpoint |
| Method | `GET`, `POST`, `PUT`, `DELETE`, `WS`, or `WSS` |
| Parameters | A list of name/value pairs, each with individual UI fields that can be added or removed as needed |
| Ignore Status | Optional fire-and-forget behavior; C-Play assumes success after the request is sent or the connection opens |

When a slide containing a REST layer is loaded, the layer sends the configured HTTP or WebSocket request on a background thread. Status is reported as: in-progress, success, or failure.

### Configuring a REST layer

In the layer editor, a REST layer can be configured in two ways:

1. **From predefined commands** — Select a command from the dropdown populated by the REST Commands Editor (see below). This auto-fills URL, method, and parameters.
2. **Custom** — Toggle to custom mode to manually enter URL, method, parameters, and ignore-status behavior.

---

## REST Commands Editor

The **REST Commands Editor** is a standalone window accessible from **Settings → REST Commands Editor**. It manages a list of predefined HTTP and WebSocket commands that can be used both for manual testing and as presets when configuring REST layers.

### Features

- Browse all predefined commands (title, method, URL)
- Add, update, or remove commands
- Trigger a command immediately and inspect the response (status code + body)
- Use `WS`/`WSS` for WebSocket targets, including OBS WebSocket v5 commands

### Command fields

| Field | Description |
|-------|-------------|
| Title | Display name for the command |
| URL | Full endpoint URL |
| Method | `GET`, `POST`, `PUT`, `DELETE`, `WS`, or `WSS` |
| Parameters | A list of name/value pairs with individual fields for each entry (supports values with spaces and special characters) |
| Ignore Status | Do not wait for the full response; useful for fire-and-forget launch/control commands |

### WebSocket and OBS commands

For `WS` and `WSS`, C-Play opens the WebSocket URL and sends the configured parameters as a JSON text message. If the command is an OBS WebSocket v5 command, C-Play negotiates the `obswebsocket.json` subprotocol, sends the required Identify message, and then sends the OBS request.

OBS commands use these parameter names:

| Parameter style | Description |
|-----------------|-------------|
| `requestType` | OBS request name, for example `SetCurrentProgramScene` |
| `requestData.name` | Adds `name` to the OBS `requestData` object, for example `requestData.sceneName = Browser` |
| `password` | OBS WebSocket password, if authentication is enabled |
| `eventSubscriptions` | Optional OBS event subscription bitmask used during Identify |

The editor includes an OBS helper for switching profile, current program scene, and scene collection. Those helpers fill `requestType` and the correct `requestData.*` field for you.

### Storage

Commands are stored in `data/predefined-rest-commands.json` relative to the application working directory. The file format:

```json
{
    "commands": [
        {
            "enabled": true,
            "ignoreStatus": false,
            "method": "GET",
            "title": "Get C-Play playlist",
            "url": "http://localhost:7007/playlist",
            "parameters": []
        },
        {
            "enabled": true,
            "ignoreStatus": false,
            "method": "POST",
            "title": "Force 2D in C-Play",
            "url": "http://localhost:7007/view_mode",
            "parameters": [{"name": "mode", "value": "1"}]
        },
        {
            "enabled": true,
            "ignoreStatus": false,
            "method": "WS",
            "title": "OBS - Set scene Browser",
            "url": "ws://localhost:4455",
            "parameters": [
                {"name": "requestType", "value": "SetCurrentProgramScene"},
                {"name": "requestData.sceneName", "value": "Browser"}
            ]
        }
    ]
}
```

Commands with `"enabled": false` are skipped when loading. Parameters are stored as a JSON array of `{"name": ..., "value": ...}` objects, which allows values containing spaces and special characters. For `POST`/`PUT` requests parameters are sent as the request body; for `GET`/`DELETE` they are appended as query parameters; for `WS`/`WSS` they are converted to a JSON WebSocket message or an OBS WebSocket request.