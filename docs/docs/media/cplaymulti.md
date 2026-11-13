---
title: Multi-video composition (*.cplaymulti)
sidebar_position: 6
---

# Multi-video composition files (*.cplaymulti)

C-Play can play **several videos at the same time** on the cluster nodes, each with its own mapping: for example left and right eye for 3D, or a different file on each node. All of them stay in time with one **master reference video** that plays on the master.

Such a setup is saved as a multi-video composition, a `.cplaymulti` file. **Build and edit compositions in the [Multi-Video Composer](#multi-video-composer).** It writes a valid file, shows where every video and audio source plays, and warns about mistakes before you play. You do not need to write the JSON by hand; the [file format reference](#file-format-reference) at the end of this page is there for reading files, scripting and troubleshooting.

A composition consists of:

1. **Master reference** — one video played on the master, for preview and timeline control.
2. **Entries** — the videos (or audio streams) each node plays, each with its own eye, grid and stereo mode.
3. **Per-node files and audio** — which file each node plays for an entry, and which nodes play audio. Nodes are named by the node ids in `data/nodes.json`.

Typical uses:

- **Stereoscopic 3D dome** — left-eye and right-eye videos played together on a dome.
- **Multi-screen installations** — different content on different nodes or parts of the environment.
- **Audio on one node** — the soundtrack played only on the node wired to the sound system.

## Quick start

1. **Prepare the files.** Use [C-Slice](https://c-toolbox.github.io/C-Slice/) to render and encode per-window outputs for your SGCT setup (see [video preparation](/media/video#prepare-videos-with-c-slice)). Put the node files where each node can reach them, and prepare separate audio files if you use them.
2. **Name the nodes.** Open **Settings → Node Configuration...**, click **Add from cluster** (or add the rows by hand), give each node an id such as `Node1`, and save. Copy `data/nodes.json` to every node. See [Node Configuration](#node-configuration).
3. **Open the composer.** Choose **File → Multi-Video Composer...** and click **New**.
4. **Set the master reference.** Select *Master reference* in the entry list, choose the reference video, and set its grid and stereoscopic mode so the preview looks right.
5. **Add entries.** Click **Add video** for each video stream (for example one per eye) and set its eye, grid and stereoscopic mode. Click **Add audio** for a stream that only plays sound.
6. **Choose the files per node.** For each entry, fill in the file on each node's row under *Per node*, or type one **Path template** such as `/media/sliced/{nodeId}/left.mp4` that works for all nodes.
7. **Choose where audio plays.** Use **Play audio on** (**All nodes**, **No nodes**, or **Only on** a node, then **Apply**), or set audio per node. The **Audio routing** tab shows the result; aim for one audio source per machine.
8. **Check and play.** Look at the **Checks** tab, then click **Save and Play**. The master plays the reference video and each node creates its own players.

To use the composition later, open the `.cplaymulti` file, [add it to a playlist](#in-a-playlist), or reference it from a [C-Play file](#integration-with-cplayfile-c-play-file).

## Multi-Video Composer

Open the composer with **File → Multi-Video Composer...**. You can also open it from the **Save As C-Play File** window, using the edit button next to the multi-video composition field. When that field has a file, the composer opens it.

![Multi-Video Composer with a video entry selected](/assets/ui/multivideo_composer.png)

- **Toolbar**: **New**, **Open...**, **Save**, **Save As...** and **Save and Play**. The window title shows the file name, with `*` when there are unsaved changes; New and Open ask before they discard them.
- **Nodes**: the node ids come from `data/nodes.json`, plus the ids used in the open composition. To add an id by hand, type it in the *Node id* field and click **Add node** (or press Enter). **Node Configuration...** opens the [Node Configuration window](#node-configuration-window).
- **Entry list**: the master reference first, then the entries, each with a summary of where it plays and where its audio plays. **Add video**, **Add audio**, **Duplicate**, **Up**, **Down** and **Remove** edit the list.
- **Master reference**: the reference video, its grid and stereoscopic modes, **Play audio on the master**, and an optional separate audio file.
- **Video entries**: name, eye, grid and stereoscopic mode, and an optional path template. Under *Audio*, **Play audio on** sets audio for **All nodes**, **No nodes**, or **Only on** one node (choose it and click **Apply**). *Default* sets the entry's own audio and separate audio file. Under *Per node*, each node has a row with its video file, an audio setting (**Default (on/off)**, **On** or **Off**) and an optional separate audio file. A node without a file uses the path template.
- **Audio entries**: give an audio file to each node that should play the stream. Nodes left empty play nothing for it.
- **Audio routing** lists which audio sources play on the master and on each node. **Checks** warns about issues, for example two audio sources on one node, entries that play nowhere, or node ids missing from `nodes.json`. **JSON** shows the file that will be saved.

File paths are shown in a warning color when they do not exist on this computer. That is fine for files that only exist on a node.

**Save and Play** saves the composition and loads it on the master and all nodes. Fields the composer does not edit (`plane`, `rotate`, `translate`, `roi`) are kept when you save the file, and are listed below the entry.

### Common setups

- **Stereoscopic dome on one node** — add two video entries, set *Eye* to **Left eye only** and **Right eye only**, grid mode **Dome** on both, and choose the left and right files on the node's row. Set **Play audio on** to the node on one of the two entries only.
- **One file per node on a large cluster** — add one video entry and type a **Path template** with `{nodeId}`, for example `/media/sliced/{nodeId}/show.mp4`, instead of filling in every row. A file in a node's row overrides the template for that node.
- **Video on one node only** — leave **Path template** empty and choose a file only on that node's row. Other nodes skip the entry.
- **Audio on one node only** — click **Add audio**, choose the audio file only on that node's row, and turn off audio on the video entries (**Play audio on → No nodes**). Turn off **Play audio on the master** if the master should be silent too.

The master's volume and mute controls apply to all node audio in a composition, also when audio on nodes is turned off in the audio settings.

## Node Configuration

Nodes find their node id by looking up their own IP address in `data/nodes.json`. Multi-video compositions use the id to choose each node's files and audio, and other per-node features use the same file, such as per-node stream, DirectShow and capture settings (see [Per-node resolution in clusters](/media/layer-operations#per-node-resolution-in-clusters)).

### Node Configuration window

Edit the node ids under **Settings → Node Configuration...**. The window can also be opened with the **Node Configuration...** buttons in the Multi-Video Composer and in the Logging window (next to *Node telemetry*).

![Node Configuration window](/assets/ui/node_configuration.png)

* **File** — the `nodes.json` that is edited, in the `data` folder of this C-Play. When it does not exist yet, it is created when you save. When the file was found in the old `data/multivideo` folder, the window says so and saving writes `data/nodes.json`.
* **Node id** / **IP address** — one row per node. Reorder rows with the arrows, or remove a row with the delete button.
* **Add node** — adds an empty row.
* **Add from cluster** — adds a row for each node address in the running cluster configuration that is not listed yet. It is disabled when no cluster configuration is running.
* **Reload** — discards your changes and reads `nodes.json` again.
* **Save** — writes the file. The status next to it shows what was loaded, added or saved.

The window warns about rows without a node id or IP address, node ids used more than once (those nodes get the same files), and IP addresses listed more than once (only the last row counts). When a cluster is running, it also lists cluster nodes without a node id and listed addresses that are not in the cluster configuration.

Every computer reads `nodes.json` from its own C-Play folder. After saving, copy the file to every node (unless they share the folder) and restart C-Play on the nodes. An open Multi-Video Composer picks up added or renamed node ids at once.

Older versions kept the file in `./data/multivideo/nodes.json`. That location is still read when `./data/nodes.json` does not exist, and C-Play logs a warning. Open it in the Node Configuration window and save it to move it to `./data/nodes.json`, then copy that file to the nodes.

## Playing compositions

A composition plays when you:

- open the `.cplaymulti` file, or use **Save and Play** in the composer;
- play a playlist entry that is a `.cplaymulti` file (see [In a playlist](#in-a-playlist));
- open or play a `.cplayfile` that references one (see [Integration with cplayfile](#integration-with-cplayfile-c-play-file)).

The master plays the reference video for preview and timeline control, and the composition is sent to all nodes. Each node then creates players for the entries that have a file for it. Time, pause and loop follow the master.

After you change a composition, test it on the cluster and check the log for "no path resolved" warnings, which mean a node found no file for an entry: either the node id is missing from `nodes.json`, or the entry has no file or template for that node.

### In a playlist

A `.cplaymulti` file can be a playlist entry by itself; no `.cplayfile` is needed. Add it with **Add file to playlist** (the dialog lists *Multi-video composition (\*.cplaymulti)*) or by dropping it on the playlist.

* **Playing the entry** starts the composition: the master plays the reference video and the nodes create their per-node players. Nothing starts when the playlist is loaded or the entry is added.
* **Playing a normal entry** after a composition ends the composition; the nodes go back to playing the main video.
* **Playlist entry options** apply as for other entries: end-of-file mode, start and end time, and the grid and stereo mode overrides (for the master preview). The separate audio file override does not apply; set audio in the composition instead.
* **Shown in the playlist**: the file name, the master's grid and stereo modes, and the duration once the entry has been played. The entry's media file is the master reference file.

A `.cplayfile` that references a composition works the same way in a playlist. Use one when the entry also needs a title or sections.

### Integration with cplayfile (C-Play File)

A composition can be used **inside a `.cplayfile`** (C-Play file), which adds a title, sections and other C-Play file settings. In the **Save As C-Play File** window, check **Multi-Video Composition** and choose the `.cplaymulti` file; the edit button next to it opens the file in the composer. When you save a C-Play file while a composition is playing, the field is filled in for you.

When the C-Play file is opened or played:

1. C-Play loads the composition instead of a single media file.
2. The master plays the composition's reference video.
3. All nodes receive the composition and create their players.
4. The C-Play file's title and sections still apply, so you can define sections within the composition. In a playlist, the playlist entry supplies the end-of-file mode.

## File format reference

The Multi-Video Composer writes these files, and its **JSON** tab shows the file before you save. Use this reference to read a file, generate one from a script, or troubleshoot. If you edit a file by hand, open it in the composer afterwards and look at the **Checks** tab.

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
| `audio` | boolean | Optional. Set to `false` to silence the master preview, for example when the audio plays on a node instead. When omitted, the master follows C-Play's audio settings. |
| `audioFile` | string | Optional. Path to a separate audio file for the master reference video (mirrors cplayfile's "Separate Audio File"). If specified, the file is loaded via mpv's `audio-file` command option when loading the master file. Defaults to empty (no separate audio). |

### Videos Array

The `videos` array contains one entry per video stream to play on each node:

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | **Required.** Human-readable name for this video entry (used in logs). |
| `type` | string | Optional. `"video"` (default) or `"audio"`. An audio entry plays its per-node file as audio only, with no video rendering. See [Audio-only entries](#audio-only-entries). |
| `eyeMode` | string | Optional. Eye mode mapping: `"left"`, `"right"`, or `"Both"` (default). Used for split-eye 3D rendering. |
| `gridMode` | string | Optional. Grid mode for this sub-player: `"None"`, `"Plane"`, `"Dome"`, `"Sphere_EQR"`, or `"Sphere_EAC"`. Defaults to `None`. |
| `stereoMode` | string | Optional. Stereo mode: `"No_2D"`, `"SBS_3D"`, `"TB_3D"`, or `"TBF_3D"`. Defaults to `No_2D`. |
| `audio` | boolean | Optional. Whether to enable audio for this sub-player on every node. A node can override it in `paths`; see [Per-node audio](#per-node-audio). Each machine should play only one audio source to avoid overlapping audio. Defaults to `false`. |
| `audioFile` | string | Optional. Path to a separate audio file to load for this sub-player (mirrors cplayfile's "Separate Audio File"). If specified, the file is loaded via mpv's `audio-file` property. Defaults to empty (no separate audio). |
| `paths` | object | Map of node IDs to file paths. A value is either a path string or an object with `file` and optional per-node `audio` and `audioFile`. Supply `paths`, `pathTemplate`, or both so each intended node can resolve a file. |
| `plane` | object | Optional. Plane grid parameters (used when `gridMode` is `"Plane"`). See [Plane Parameters](#plane-parameters) below. |
| `rotate` | array | Optional. Rotation as `[x, y, z]` floats. Used for non-plane grid modes. Defaults to `[0, 0, 0]`. |
| `translate` | array | Optional. Translation as `[x, y, z]` floats. Defaults to `[0, 0, 0]`. |
| `roi` | object | Optional. Region of interest settings. See [ROI Parameters](#roi-parameters) below. |
| `pathTemplate` | string | Optional. Fallback path template with `{nodeId}` placeholder, used when a direct lookup in `paths` is missing or empty. |

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

Each video entry must specify which file to play on each node. There are two ways. The examples below show fields to place inside an entry in the `videos` array, rather than complete composition files:

#### Direct Path Mapping (`paths`)

The `paths` object maps node identifiers (from `data/nodes.json`) to file paths:

```json
{
  "paths": {
    "Node1": "/path/to/video_left_Node1.mp4",
    "Node2": "/path/to/video_left_Node2.mp4",
    "Node3": "/path/to/video_left_Node3.mp4"
  }
}
```

#### Path Template (`pathTemplate`)

As a fallback, you can use a path template with the `{nodeId}` placeholder:

```json
{
  "pathTemplate": "/videos/{nodeId}/left.mp4"
}
```

When resolved for `Node1`, this becomes `/videos/Node1/left.mp4`.

#### Per-node audio

To choose audio per node, write the node's value as an object instead of a string:

```json
{
  "audio": false,
  "paths": {
    "Node1": { "file": "/videos/Node1/left.mp4", "audio": true, "audioFile": "/audio/stereo_mix.wav" },
    "Node2": "/videos/Node2/left.mp4",
    "Node3": "/videos/Node3/left.mp4"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `file` | string | Video file for this node. Leave it out to use `pathTemplate`. |
| `audio` | boolean | Optional. Overrides the entry's `audio` on this node. |
| `audioFile` | string | Optional. Overrides the entry's `audioFile` on this node. |

In this example, only Node1 plays audio, and it plays a separate stereo mix. Node2 and Node3 play the video without audio.

A nonempty `paths` lookup takes precedence over `pathTemplate`. An empty path falls back to the template; it does not disable that node when a template is set. If neither is available or resolves to an empty string, that sub-player is skipped with a warning.

### Audio-only entries

An entry with `"type": "audio"` plays audio with no video. Each node listed in `paths` (or resolved through `pathTemplate`) plays its file. Nodes without a file play nothing for this entry:

```json
{
  "name": "soundtrack",
  "type": "audio",
  "paths": {
    "Node1": "/data/Media/show/soundtrack_5_1.wav"
  }
}
```

The audio sub-player follows the master's time, pause and loop state like the video entries. The `eyeMode`, `gridMode`, `stereoMode`, `audio` and `audioFile` fields do not apply to audio entries.

### Plane Parameters

Plane parameters are used when `gridMode` is `"Plane"`:

```json
{
  "plane": {
    "azimuth": 0.0,
    "elevation": 0.0,
    "roll": 0.0,
    "distance": 100.0,
    "width": 200.0,
    "height": 100.0,
    "aspect": 1
  }
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
{
  "roi": {
    "enabled": true,
    "x": 0.0,
    "y": 0.0,
    "w": 0.5,
    "h": 1.0
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | boolean | Whether ROI is active (default: false) |
| `x` | number | X offset in normalized coordinates [0, 1] (default: 0) |
| `y` | number | Y offset in normalized coordinates [0, 1] (default: 0) |
| `w` | number | Width in normalized coordinates [0, 1] (default: 1) |
| `h` | number | Height in normalized coordinates [0, 1] (default: 1) |

### nodes.json

The [Node Configuration window](#node-configuration-window) writes `./data/nodes.json` in this format:

```json
{
  "nodes": [
    { "id": "Node1", "ip": "10.0.0.11" },
    { "id": "Node2", "ip": "10.0.0.12" }
  ]
}
```

### In a cplayfile

A `.cplayfile` refers to a composition with the `multivideo` key:

```json
{
  "video": "/data/Media/Domtest_8K_30_H265/Domtest_Sliced/GUI/DomeTest.mp4",
  "multivideo": "./data/multivideo/single_node.cplaymulti",
  "title": "Stereoscopic Dome Show"
}
```

When `multivideo` resolves to a valid `.cplaymulti` file, the composition is played instead of the `video` file. `useMultivideoConfig` is an application property, not a JSON field.

### In a cplaylist

A `.cplaylist` lists a composition like any other file:

```json
{
  "playlist": [
    { "file": "intro.mp4", "on_file_end": "continue" },
    { "file": "./data/multivideo/single_node.cplaymulti", "on_file_end": "continue" },
    { "file": "outro.mp4", "on_file_end": "pause" }
  ]
}
```

### Example: Single-Node Stereoscopic Dome

Left-eye and right-eye videos on one node for a stereoscopic dome. The left eye entry plays audio from a separate file:

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

### Example: Multi-Node Cluster with Path Template

A path template for a 6-node cluster:

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
