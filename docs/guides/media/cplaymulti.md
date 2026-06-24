---
title: Multi-video composition (*.cplaymulti)
layout: home
nav_order: 6
parent: Media file structure
---

# Multi-video composition files (*.cplaymulti)

C-Play supports playing **multiple videos simultaneously** on cluster nodes, with each video mapped differently (e.g., left/right eye for 3D, different dome positions). All videos are time-synced to a single **master reference video** played on the master node.

This feature is defined in a JSON configuration file with the `.cplaymulti` extension. The full composition is sent from the master to all nodes via the sync system, and each node resolves its own per-node file paths using a separate node-identity configuration.

## Overview

A multi-video composition defines:

1. **Master reference video** — A single video played on the master for preview/timeline control
2. **Video entries** — Multiple videos to play on each node, with distinct rendering mappings (eye mode, grid mode, stereo mode)
3. **Per-node file paths** — Different files per node, resolved via node ID mapping

### Use Cases

- **Stereoscopic 3D dome** — Play left-eye and right-eye videos simultaneously on a dome projection
- **Multi-screen installations** — Display different content on different parts of the environment
- **Redundant playback** — Same video from multiple angles/sources, mapped to different positions

## JSON Schema

A `.cplaymulti` file is a JSON document with the following structure:

```json
{
  "master": {
    "file": "path/to/reference.mp4",
    "gridMode": "Dome",
    "stereoMode": "No_2D",
    "audio": true
  },
  "videos": [
    {
      "name": "leftEye",
      "eyeMode": "left",
      "gridMode": "Dome",
      "stereoMode": "No_2D",
      "audio": true,
      "paths": {
        "Node1": "path/to/left_Node1.mp4"
      }
    },
    {
      "name": "rightEye",
      "eyeMode": "right",
      "gridMode": "Dome",
      "stereoMode": "No_2D",
      "audio": false,
      "paths": {
        "Node1": "path/to/right_Node1.mp4"
      }
    }
  ]
}
```

### Master Section

The `master` object defines the reference video played on the master node:

| Field | Type | Description |
|-------|------|-------------|
| `file` | string | **Required.** Path to the master reference video file. |
| `gridMode` | string | Optional. Grid mode for the master preview (e.g., `"Dome"`, `"Sphere_EQR"`). Defaults to `None`. |
| `stereoMode` | string | Optional. Stereo mode for the master preview (e.g., `"SBS_3D"`, `"No_2D"`). Defaults to `No_2D`. |
| `audio` | boolean | Optional. Whether to enable audio on the master reference video. Defaults to `false`. |
| `audioFile` | string | Optional. Path to a separate audio file for the master reference video (mirrors cplayfile's "Separate Audio File"). If specified, the file is loaded via mpv's `audio-file` command option when loading the master file. Defaults to empty (no separate audio). |

### Videos Array

The `videos` array contains one entry per video stream to play on each node:

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | **Required.** Human-readable name for this video entry (used in logs). |
| `eyeMode` | string | Optional. Eye mode mapping: `"left"`, `"right"`, or `"Both"` (default). Used for split-eye 3D rendering. |
| `gridMode` | string | Optional. Grid mode for this sub-player: `"None"`, `"Plane"`, `"Dome"`, `"Sphere_EQR"`, or `"Sphere_EAC"`. Defaults to `None`. |
| `stereoMode` | string | Optional. Stereo mode: `"No_2D"`, `"SBS_3D"`, `"TB_3D"`, or `"TBF_3D"`. Defaults to `No_2D`. |
| `audio` | boolean | Optional. Whether to enable audio for this sub-player. Only one entry should have `audio: true` to avoid overlapping audio. Defaults to `false`. |
| `audioFile` | string | Optional. Path to a separate audio file to load for this sub-player (mirrors cplayfile's "Separate Audio File"). If specified, the file is loaded via mpv's `audio-file` property. Defaults to empty (no separate audio). |
| `paths` | object | **Required.** Map of node IDs to file paths (see below). |
| `plane` | object | Optional. Plane grid parameters (used when `gridMode` is `"Plane"`). See [Plane Parameters](#plane-parameters) below. |
| `rotate` | array | Optional. Rotation as `[x, y, z]` floats. Used for non-plane grid modes. Defaults to `[0, 0, 0]`. |
| `translate` | array | Optional. Translation as `[x, y, z]` floats. Defaults to `[0, 0, 0]`. |
| `roi` | object | Optional. Region of interest settings. See [ROI Parameters](#roi-parameters) below. |
| `pathTemplate` | string | Optional. Fallback path template with `{nodeId}` placeholder, used when a direct lookup in `paths` fails. |

#### Grid Mode Values

| Value | Description |
|-------|-------------|
| `"None"` | No mapping — pre-split/prepared content, no transformation |
| `"Plane"` | Flat plane projection with configurable size and position |
| `"Dome"` | 180° fulldome/fisheye projection |
| `"Sphere_EQR"` | 360° equirectangular projection |
| `"Sphere_EAC"` | 360° equi-angular cubemap projection |

#### Stereo Mode Values

| Value | Description |
|-------|-------------|
| `"No_2D"` | Standard 2D/mono video |
| `"SBS_3D"` | Side-by-side 3D |
| `"TB_3D"` | Top-bottom 3D |
| `"TBF_3D"` | Top-bottom with flip 3D |

#### Eye Mode Values

| Value | Description |
|-------|-------------|
| `"Both"` | Render for both eyes (default) |
| `"left"` | Render only for left eye viewport |
| `"right"` | Render only for right eye viewport |

### Per-Node File Paths

Each video entry must specify which file to play on each node. There are two ways:

#### Direct Path Mapping (`paths`)

The `paths` object maps node identifiers (from the node identity config) to file paths:

```json
"paths": {
  "Node1": "/path/to/video_left_Node1.mp4",
  "Node2": "/path/to/video_left_Node2.mp4",
  "Node3": "/path/to/video_left_Node3.mp4"
}
```

#### Path Template (`pathTemplate`)

As a fallback, you can use a path template with the `{nodeId}` placeholder:

```json
"pathTemplate": "/videos/{nodeId}/left.mp4"
```

When resolved for `Node1`, this becomes `/videos/Node1/left.mp4`.

The `paths` lookup takes precedence over `pathTemplate`. If neither is available or resolves to an empty string, that sub-player is skipped with a warning.

### Plane Parameters

Plane parameters are used when `gridMode` is `"Plane"`:

```json
"plane": {
  "azimuth": 0.0,
  "elevation": 0.0,
  "roll": 0.0,
  "distance": 100.0,
  "width": 200.0,
  "height": 100.0,
  "aspect": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `azimuth` | number | Azimuth angle in degrees (default: 0) |
| `elevation` | number | Elevation angle in degrees (default: 0) |
| `roll` | number | Roll angle in degrees (default: 0) |
| `distance` | number | Distance from viewer in cm (default: 0) |
| `width` | number | Plane width in cm (default: 0) |
| `height` | number | Plane height in cm (default: 0) |
| `aspect` | integer | Aspect ratio consideration: `1`=calculate from video, `2`=use specified size (default: 1) |

### ROI Parameters

Region of interest settings allow displaying only a portion of the video:

```json
"roi": {
  "enabled": true,
  "x": 0.0,
  "y": 0.0,
  "w": 0.5,
  "h": 1.0
}
```

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | boolean | Whether ROI is active (default: false) |
| `x` | number | X offset in normalized coordinates [0, 1] (default: 0) |
| `y` | number | Y offset in normalized coordinates [0, 1] (default: 0) |
| `w` | number | Width in normalized coordinates [0, 1] (default: 1) |
| `h` | number | Height in normalized coordinates [0, 1] (default: 1) |

## Node Identity Configuration

For per-node path resolution to work, C-Play needs a mapping from node IP addresses to node identifiers. This is stored in a separate JSON file (typically `./data/multivideo/nodes.json`):

```json
{
  "nodes": [
    { "id": "Node1", "ip": "10.0.0.11" },
    { "id": "Node2", "ip": "10.0.0.12" }
  ]
}
```

Each node uses this file to determine its own identifier, which is then used to look up the correct file path in the composition JSON's `paths` object.

## Example: Single-Node Stereoscopic Dome

This example plays left-eye and right-eye videos on a single node for stereoscopic dome viewing. The left eye entry includes an optional separate audio file:

```json
{
  "master": {
    "file": "/data/Media/Domtest_8K_30_H265/Domtest_Sliced/GUI/DomeTest.mp4",
    "gridMode": "Dome",
    "stereoMode": "No_2D",
    "audio": true
  },
  "videos": [
    {
      "name": "leftEye",
      "eyeMode": "left",
      "gridMode": "Dome",
      "stereoMode": "No_2D",
      "audio": true,
      "audioFile": "/data/Media/Domtest_8K_30_H265/Domtest_Sliced/Node1/audio_left.mp3",
      "paths": {
        "Node1": "/data/Media/Domtest_8K_30_H265/Domtest_Sliced/Node1/Domtest_LEFT.mp4"
      }
    },
    {
      "name": "rightEye",
      "eyeMode": "right",
      "gridMode": "Dome",
      "stereoMode": "No_2D",
      "audio": false,
      "paths": {
        "Node1": "/data/Media/Domtest_8K_30_H265/Domtest_Sliced/Node1/Domtest_RIGHT.mp4"
      }
    }
  ]
}
```

## Example: Multi-Node Cluster with Path Template

This example uses a path template for a 6-node cluster:

```json
{
  "master": {
    "file": "/data/Media/reference.mp4",
    "gridMode": "Dome",
    "stereoMode": "No_2D"
  },
  "videos": [
    {
      "name": "leftEye",
      "eyeMode": "left",
      "gridMode": "Dome",
      "audio": true,
      "pathTemplate": "/data/Media/sliced/{nodeId}/left.mp4"
    },
    {
      "name": "rightEye",
      "eyeMode": "right",
      "gridMode": "Dome",
      "audio": false,
      "pathTemplate": "/data/Media/sliced/{nodeId}/right.mp4"
    }
  ]
}
```

For `Node3`, the resolved paths would be:
- Left eye: `/data/Media/sliced/Node3/left.mp4`
- Right eye: `/data/Media/sliced/Node3/right.mp4`

## Workflow for Creating cplaymulti Files

1. **Prepare your video files** — Ensure all videos are available at the paths specified in the configuration, or use path templates with `{nodeId}` placeholders. If using separate audio files, prepare those as well.

2. **Create node identity config** — Create `./data/multivideo/nodes.json` mapping each node's IP address to a unique identifier:
   ```json
   { "nodes": [ { "id": "Node1", "ip": "10.0.0.11" } ] }
   ```

3. **Choose master reference video** — Select the video that will be played on the master for preview/timeline control. This should typically match one of the node videos or a combined version.

4. **Define master parameters** — Configure the reference video played on the master:
   - Set `gridMode` and `stereoMode` for correct preview rendering
   - Optionally specify an `audioFile` for a separate audio track (similar to cplayfile's "Separate Audio File")

5. **Define video entries** — For each video to play on nodes:
   - Give it a descriptive `name`
   - Set `eyeMode` if using split-eye 3D (`"left"` or `"right"`)
   - Set `gridMode` and `stereoMode` for the desired rendering
   - Enable `audio` for only one entry (to avoid overlapping audio)
   - Optionally specify an `audioFile` for a separate audio track (similar to cplayfile's "Separate Audio File")
   - Specify per-node paths in `paths` or use `pathTemplate`

6. **Test on master** — Load the `.cplaymulti` file through C-Play's multi-video loader. The master should show the reference video, and nodes should display their respective sub-videos correctly mapped.

7. **Verify node resolution** — Check logs for any "no path resolved" warnings indicating missing or incorrect node ID mappings.

## Integration with cplayfile (C-Play File)

A `.cplaymulti` multi-video composition can be incorporated **inside a `.cplayfile`** (C-Play file). This allows you to use multi-video playback as part of a structured media file that also includes metadata such as title, sections, EOF mode, and other settings.

### How It Works

When creating or editing a `.cplayfile`, there is an optional **"Multi-video Composition"** field where you can specify the path to a `.cplaymulti` JSON file. When this field is set:

1. Instead of loading the media file directly, C-Play loads the multi-video composition specified in the cplaymulti file
2. The master reference video from the cplaymulti is played on the master node for preview/timeline control
3. All nodes receive the composition JSON and create sub-players according to their configured entries
4. Other settings from the cplayfile (title, sections, EOF mode) still apply

### Example

In a `.cplayfile`, you can specify:

```json
{
  "mediaFile": "/data/Media/Domtest_8K_30_H265/Domtest_Sliced/GUI/DomeTest.mp4",
  "multivideoConfig": "./data/multivideo/single_node.cplaymulti",
  "useMultivideoConfig": true,
  "title": "Stereoscopic Dome Show",
  "eofMode": 2
}
```

When `useMultivideoConfig` is `true` and `multivideoConfig` points to a valid `.cplaymulti` file, C-Play will:
- Load the multi-video composition instead of the single media file
- Use the master reference video from the cplaymulti for preview
- Sync the full composition to all cluster nodes

### Benefits

Using cplaymulti inside cplayfile gives you:
- **Metadata support** — Title, sections, and other cplayfile features work alongside multi-video
- **Section-based playback** — Define time ranges within the multi-video composition
- **Playlist integration** — Include multi-video entries in playlists with custom EOF behavior
- **SaveAsCPlayFile dialog** — When saving a multi-video session as a cplayfile, the cplaymulti path is automatically populated

## Loading Multi-Video Compositions

Multi-video compositions can be loaded in C-Play by:
- Opening a `.cplaymulti` file directly (C-Play recognizes the extension)
- **Loading a `.cplayfile` that references a cplaymulti composition** (see [Integration with cplayfile](#integration-with-cplayfile-c-play-file) above)

When loading directly, the master plays the reference video for preview, and the composition JSON is synced to all nodes. Each node then creates sub-players according to its configured entries.