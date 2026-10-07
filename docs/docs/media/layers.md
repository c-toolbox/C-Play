---
title: Layers and sources
sidebar_position: 9
---

# Layers and sources

A layer adds media or an automation action to a slide. Use several layers to combine a background, video, live camera feed, labels, and show-control commands. See [Build presentation](/media/cplaypres) for creating slides, ordering layers, saving the presentation, and using visibility and timelines.

## Add and configure a layer

1. Select a slide, or select the **Master** slide for background layers that sit behind the main player.
2. Click **Add new layer** in the layers toolbar.
3. Choose a type, give it a descriptive title, and select its file, source, text, or command.
4. For visual layers, choose the mapping and stereo mode that match the content.
5. Open **Layer View** to preview the source and adjust its settings. Save the presentation after editing.

The types available depend on the platform and the features enabled in your C-Play build. Audio and command layers have no image to map. **MultiVideo** appears with that name in the type selector; its settings come from a composition file.

## Choose a layer type

| Layer | Use it for | Source to select |
| --- | --- | --- |
| [Image](#image) | Logos, still backgrounds, overlays, and photographs | An image file |
| [Video](#video) | Recorded clips with playback and seeking controls | A video file |
| [MultiVideo](#multivideo) | Different video files or eye views on different nodes | A `.cplaymulti` composition |
| [Audio](#audio) | Soundtracks or audio cues without an image | An audio file |
| [PDF](#pdf) | Pages from a document or slide deck | A PDF file |
| [Text](#text) | Titles, captions, and labels | Text entered in the dialog |
| [NDI](#ndi) | Live video and audio from a network sender | A discovered NDI source |
| [OMT](#omt) | Live video and audio from an OMT network sender | A discovered OMT source |
| [Spout](#spout) | GPU video shared by another local Windows application | A sender or predefined Spout setup |
| [DirectShow](#directshow) | Windows cameras, capture devices, or media through system decoders | Devices, a predefined setup, or a media file |
| [Capture](#capture) | Capture card inputs through a supported vendor SDK | An input or predefined capture setup |
| [WebRTC](#webrtc) | A live network feed served over WHEP | A WHEP URL |
| [Stream](#stream) | Live camera or encoder feeds over SRT, RTSP, or other MPV inputs | A URL or predefined stream |
| [YouTube](#youtube) | A single online video with recorded-video controls | A YouTube video URL |
| [Control](#control) | Playback, fades, loading, or orientation commands inside C-Play | An operation and parameter |
| [REST](#rest) | HTTP or WebSocket commands to external systems | A saved command or custom request |
| [TCP](#tcp) | Commands sent through persistent device connections | A saved TCP command |

## Files and text

### Image

Choose **Image** and browse to an image file, such as PNG, JPEG, TIFF, or WebP. Set its mapping in the add dialog, then use Layer View to adjust position, size, opacity, and cropping. Transparent images are useful for logos and overlays. See [Image files](/media/images) for format and background-image guidance.

### Video

Choose **Video** and browse to a recorded clip, such as MP4, MKV, MOV, or WebM. In Layer View, use the playback controls to play, pause, seek, or rewind. Choose the end-of-file mode and, when needed, an A–B section to loop only part of the clip. Set stereo mode to match the file before positioning it. See [Video files](/media/video) for media preparation.

### MultiVideo

Choose **MultiVideo** and open a `.cplaymulti` composition file. It defines a master reference video and the video entries each node should load, including per-entry mapping, stereo, eye, and audio settings. Use it when the cluster needs separate files for different surfaces or eyes. Define node IDs in `data/multivideo/nodes.json` and make the referenced files available on the corresponding machines. See [Multi-video composition](/media/cplaymulti) for the complete format and examples.

### Audio

Choose **Audio** and browse to an audio file, such as WAV, MP3, FLAC, OGG, or M4A. Use Layer View to adjust playback and volume. Add it alongside visual layers for a soundtrack or cue; mapping and ROI do not apply. Audio routing follows the application's master/node audio settings. See [Audio files](/media/audio).

### PDF

Choose **PDF** and browse to a PDF document. Open Layer View to select the page to display, then position and map it like a still image. Use separate layers or slides for pages that need independent placement or timing. PDF support must be included in the build.

### Text

Choose **Text**, enter the label or caption, and add the layer. In Layer View, edit the text and adjust font, font size, alignment, color, and texture dimensions. Text can contain multiple lines. Use plane mapping to place a label at a specific position, or choose another mapping when the text should follow that surface. Make the chosen font available on machines that render the layer.

The default text settings come from **Settings → Configure → Text & subtitles**. A Text layer is useful for authored content; [master placeholders](#master-placeholders-for-node-sources) generate a label automatically for a node source.

## Live sources

### NDI

When choosing *"NDI (Network Device Interface)"*, a combobox becomes visible instead of the file dialog field. The combobox contains the discovered NDI sources on the network that can be received. Both image and audio are supported through NDI.

See [NDI integration](/system-integration/obs-ndi) for sender setup.

### OMT

When choosing *"OMT (Open Media Transport)"*, a combobox becomes visible instead of the file dialog field. The combobox contains the discovered OMT sources on the network that can be received.

### Spout

When choosing *"Spout"*, a combobox becomes visible instead of the file dialog field. The combobox lists the currently available Spout senders from other Windows applications on the same machine, and the refresh button can be used to rescan the sender list. This makes it practical to bring live rendered content such as graphics, real-time engines, or capture tools directly into a slide as a video layer. Spout layers are available by default in C-Play v2.3 Windows builds.

Predefined Spout setups are loaded from `data/predefined-spouts.json` and appear alongside discovered senders. Each entry has a `title`, default `sender`, optional `enabled`, and a `senders` mapping for per-machine overrides. Select the preset title so nodes resolve their own sender names. A sender must run locally on every machine that receives it. See [Spout for local app sharing](/system-integration/on-top#spout-for-local-app-sharing).

### DirectShow

When choosing *"DirectShow"* (Windows only), **Video device** and **Audio device** comboboxes become visible in addition to the file dialog field. They list the capture devices registered on this machine — cameras and capture cards for video, microphones and line inputs for audio — and a refresh button rescans them when new hardware has been plugged in. If you pick a video device, the layer captures live from that device; if no video device is chosen, the file dialog field is used instead and the selected image or video file is rendered through a DirectShow filter graph using the system's built-in decoders, which makes it useful for media that the regular Image or Video layers cannot decode. Still images show their single frame; video files keep playing through the same pipeline. The **Audio device** combobox also offers a *No audio capture* entry: selecting it creates a video-only layer that captures no audio at all - not from a microphone and not embedded in the capture card either. A picked audio device is stored with the layer and its audio is routed to the machine's audio output; for media files without an explicit audio device, the file's own audio track is used when available.

#### Predefined DirectShow setups

C-Play can load a list of predefined capture setups from the editable file *"data/predefined-directshows.json"*. When the file exists and contains at least one enabled setup, a **Setup** combobox is shown in the layer dialog instead of the video/audio device rows; the button next to it switches between the predefined list and the custom device selection (and keeps both options in sync with each other). If the file does not exist or cannot be parsed, no predefined setups are shown and you simply select the capture devices manually.

The JSON format looks like this:

```json
{
    "directshows": [
        {
            "title": "HDMI Capture 1",
            "videoDevice": "DELTA-hmi Video Source (card0 RX0)",
            "audioDevice": "",
            "enabled": true
        },
        {
            "title": "Displayport Capture 1",
            "videoDevice": "Datapath VisionSC-DP2 Video 01",
            "audioDevice": ""
        }
    ]
}
```

- `title` is the name shown in the layer dialog and used as the default layer title.
- `videoDevice` must match a video capture device name reported by DirectShow on this machine (the same names that appear in the *Video device* dropdown). It may be empty for audio-only setups.
- `audioDevice` must match an audio capture device name reported by DirectShow (the same names as in the *Audio device* dropdown). It may be empty when no audio should be captured.
- `enabled` is optional and defaults to `true`. Set it to `false` to keep a setup in the file without showing it in the UI - useful for documenting example setups that do not apply on every machine.
- `devices` is an optional object with per-machine device overrides, keyed by role (`"master"` or node ids from `data/multivideo/nodes.json`) - see [Per-node resolution in clusters](#per-node-resolution-in-clusters). An entry whose plain devices are both empty but that has a non-empty `devices` object is still shown in the UI.
- Entries where both device names are empty and there is no `devices` object are ignored.
- The file is re-read every time the predefined list is opened, so you can edit it while C-Play is running and pick up changes without restarting.

When a DirectShow layer is created from a predefined setup, its parameter string is set to `videoDevice|audioDevice` (the audio part may be empty) and the entry's *title* is stored as a stable key on the layer, so each machine in a cluster can resolve its own local capture devices.

### Capture

Choose **Capture** for a supported capture card SDK backend, such as Datapath. Select a predefined setup from `data/predefined-captures.json`, or switch to a custom input. Input numbers are **1-based**, matching the Datapath Vision utility; the selected input can exist only on the nodes.

For custom inputs, configure **ganging** when several inputs form one image, choose whether to use **direct GPU** transfer, and enable source audio when needed. Direct GPU transfer depends on suitable hardware and backend support. Layer View shows the capture status, so check it for missing sources, missing signal, or backend errors.

A capture preset contains `title`, `backend`, `input`, and optional `ganging`, `directGpu`, `audio`, and `enabled` fields. Its `sources` object overrides fields for each machine. A role entry inherits the default fields it does not specify. Set a role to `null` or an input of zero to leave capture disabled there. See [per-node resolution](#per-node-resolution-in-clusters) and the [master placeholder example](#master-placeholders-for-node-sources).

### WebRTC

When choosing *"WebRTC"*, a **WHEP URL** field becomes visible instead of the file dialog field. Enter the WHEP endpoint of the stream you want to pull, for example `http://mediamtx:8889/live/mystream/whep` when using [MediaMTX](https://github.com/bluenviron/mediamtx) as the WebRTC server; credentials can be embedded in the URL (`user:pass@host`). If you leave the title empty, it defaults to the stream name taken from the last part of the URL. The layer receives both video and audio when the stream carries them (H.264/H.265 video, Opus audio), decodes with FFmpeg — NVDEC hardware decoding when available — and reconnects automatically if the session fails while the layer should be running. The WHEP URL can be changed later in the *Layer View*; changing it restarts the connection. Like other live layers it can be marked master-only with the **Sync** button: then only the master pulls the stream; otherwise every node in the cluster pulls its own copy of the WHEP endpoint, which keeps each machine's network load local.

The **Master relay** option controls whether the master pulls and relays the feed, or nodes pull it themselves. Select the mode that matches your network and server setup. See [MediaMTX integration](/system-integration/mediamtx).

### Stream

When choosing *"Stream"*, you can choose between pre-defined streams for your system in a combobox, loaded from the editable file *"data/predefined-streams.json"*, or add a custom entry in a text field. The mode button next to the field cycles through *predefined list -> custom path -> MediaMTX server* (the last option only appears when [MediaMTX](/system-integration/mediamtx) is available).

Stream layers are for **live streaming**: camera feeds, capture cards, hardware or software encoders, and media servers. The stream is played by MPV with a low-latency profile and untimed demuxing applied automatically, so the layer follows the live feed with minimal delay instead of buffering it like a video file.

The most common URL types are:

* **SRT** — `srt://host:port/path`. A low-latency protocol that runs over UDP, well suited to lossy or long-distance networks (for example between buildings or across the internet). The bundled ffmpeg build includes libSRT, so SRT URLs work out of the box; a MediaMTX server serves SRT on port `8890` by default.
* **RTSP** — `rtsp://host:port/path`. A solid default that also works over TCP transport on restrictive networks. The included *Rtsp-tcp-lowlatency* options profile (selectable in the layer view) forces RTSP-over-TCP, disables caching, and shortens probing so the stream starts quickly.

Any other MPV-compatible input can be used as well — for example Windows capture cards and cameras through `av://dshow:video=...` paths, or raw UDP multicast feeds from encoders. See the [MPV documentation](https://mpv.io/manual/stable/) for the full list of supported protocols.

In a cluster setup every node opens the same stream URL; layers created from the predefined list can instead resolve a different local path per machine — see [Per-node resolution in clusters](#per-node-resolution-in-clusters).

> **YouTube:** pasting a YouTube URL into a Stream layer also works — C-Play detects it and plays it as video-on-demand content through yt-dlp. For YouTube videos, prefer the dedicated **YouTube** layer type below, which is purpose-built for that use case with full player controls.

### YouTube

When choosing *"YouTube"*, a single **URL** field becomes visible instead of the file dialog field. Paste a single YouTube video URL (`watch`, `shorts`, or `youtu.be`) and the layer plays it through mpv's embedded `ytdl_hook`, which uses the external [yt-dlp](https://github.com/yt-dlp/yt-dlp) tool to resolve the actual media streams. A YouTube layer is treated as video-on-demand content, so — unlike a live Stream layer — it gets the full set of player controls in the *Layer View*: play/pause, a seek slider, stop/rewind, end-of-file mode (pause or loop) and A-B section looping, exactly like a Video layer. It is never forced into the live-stream low-latency mode (which would cause audio drift).

#### YouTube playback and yt-dlp

YouTube URLs are resolved by mpv's `ytdl_hook.lua`, which spawns the external **yt-dlp** executable. C-Play locates yt-dlp on each machine in this order:

1. The **yt-dlp path** set in *Settings → Playback* (*YouTube playback*).
2. `yt-dlp.exe` (or `yt-dlp`) placed next to `C-Play.exe`.
3. `plugins/yt-dlp(.exe)` in the working directory C-Play was started from (also checked relative to the folder containing `C-Play.exe`).
4. Anywhere on the system `PATH`.

If none is found, the layer reports a load error and shows an error overlay in the *Layer View*. The layer-properties dialog and the settings page both show whether yt-dlp was found on the current machine.

> **Every cluster node needs yt-dlp too.** A YouTube layer stores its URL verbatim and syncs it to all machines; each machine resolves the URL locally with its own yt-dlp. A node without yt-dlp shows the error overlay while the master keeps playing — this is expected.

An optional **yt-dlp format** string in *Settings → Playback* is passed to `ytdl_hook` (`ytdl-format`) to cap resolution or pick specific codecs (for example `bv*[height<=1080]+ba/b`). Leave it empty to use the mpv/yt-dlp default.

A per-layer options profile named **Youtube** (from `data/mpv-conf/youtube_youtube.json`) is available in the *Layer View* options dropdown for cache/demuxer tuning.

Cookies, authentication and age-restricted content are out of scope, as are channel and playlist URLs (single video URLs only).

## Automation layers

Command layers have no visual output. Use their trigger controls or the slide timeline to dispatch actions. Configure and test the command before using it in a show.

### Control

When choosing *"Control"*, the layer does not display any visual content. Instead, it dispatches a player control operation when activated through the slide timeline. Control layers exist only on the master node and are useful for automating playback actions within a presentation.

In the layer view for a control layer, two fields are shown instead of the usual grid/stereo parameters:

* **Operation** — The command to execute. Available operations include:
  * **Playback**: Play, Pause, Stop, Rewind, Seek, SetPosition, SetSpeed
  * **Volume & Fading**: SetVolume, FadeVolumeDown, FadeVolumeUp, FadeImageDown, FadeImageUp, SetSyncVolumeVisibilityFading
  * **Loading**: LoadFromPlaylist, LoadFromSlides, LoadFromSections, LoadFromAudioTracks
  * **Orientation**: SpinPitchUp, SpinPitchDown, SpinYawLeft, SpinYawRight, SpinRollCW, SpinRollCCW, OrientationAndSpinReset, RunSurfaceTransition
  * **Surface & Visibility**: SetBackgroundVisibility, SetForegroundVisibility, SetNodeWindowsOpacity

* **Parameter** — An operation-specific value. For example, a volume level for SetVolume, a millisecond position for Seek, a slide name or index for LoadFromSlides, or `true`/`false` for boolean operations. Parameters that reference playlist items, slides, or audio tracks can be specified by name or numeric index.

### REST

When choosing *"REST"*, the layer sends an HTTP or WebSocket request when its slide is triggered. Like Control layers, REST layers are non-visual and exist only on the master node. They are useful for integrating external systems — such as lighting controllers, projectors, OBS Studio, or other networked applications — into your presentation workflow.

In the layer view for a REST layer, three fields are shown:

* **URL** — The target HTTP or WebSocket endpoint. You can either select a predefined command from the dropdown (populated by the REST Commands Editor) or toggle to custom mode and enter a URL manually.
* **Method** — The request method: `GET`, `POST`, `PUT`, `DELETE`, `WS`, or `WSS` (visible in custom mode).
* **Parameters** — A list of name/value pairs, each with individual UI fields that can be added or removed as needed (visible in custom mode). This format supports values with spaces and special characters. For POST/PUT these are sent as the request body; for GET/DELETE they are appended as query parameters; for WS/WSS they are sent as a WebSocket message or OBS WebSocket request data.

When the slide is loaded, the REST layer fires the configured request on a background thread and reports success or failure.

For more details on the REST layer and the REST Commands Editor, see the [HTTP Web API documentation](/remote-control/api#rest-layer).

### TCP

Choose **TCP** to trigger a reusable command through one of C-Play's persistent TCP server connections. Define servers and text or binary commands in **Settings → Configure → TCP control**, then select a command when adding the layer. Each server keeps its own connection, so multiple layers can send different commands to different devices.

The layer fires when started. Double-click it or press **Trigger** in its layer view to send manually; the layer view also lets you choose another command. Its status indicates whether the transport accepted the command, not whether the device executed it. Disconnected commands are rejected and are never replayed after reconnect. See [TCP control client](/remote-control/tcp) for framing, persistence, and examples.

## Position, crop, and preview

After you have added a new layer, you can specify its parameters in more detail through the *"Layer View"*. Here you control grid and stereo parameters, the volume level if applicable, and inspect how the output looks.

![Layer View Video](/assets/ui/layers_view_video.png) &nbsp;&nbsp;&nbsp; ![Layer View Grid Parameters](/assets/ui/layer_view_grid_parameters.png)

### Region of interest

In the top right, there is a *"Region of interest (ROI)"* ![](/assets/icons/trim-to-selection-lime.svg) / ![](/assets/icons/trim-to-selection-crimson.svg) button that enables the feature below, where you can specify a certain region of your layer that should be visible. This is useful when you want to show only part of the source.

![Layer Region of Interest](/assets/ui/layers_view_roi.png)

For mapping choices, see [Video & image mapping modes](/media/mapping). For fades, visibility across slides, and timeline keyframes, return to [Build presentation](/media/cplaypres).

## Master and node sources

By default, nodes load their own copy of a synchronized layer. The **Sync** control in Layer View can make a layer exist only on the master. This is separate from the **Master slide**, which determines the layer's position in the rendering order.

For supported live layer types, **Nodes read from original source** lets each machine load its source, while **Master sends content to nodes** renders on the master and streams the texture. The **Nodes** control in Layer View also enables texture streaming. See [Streaming layers from master to nodes](/playback/node-streaming) for transport, audio, and synchronization details.

Use per-machine presets when the master and nodes need different local inputs. Use a master placeholder when the source only exists on the nodes and you still need a visible layer to position in the master view.

## Per-node resolution in clusters

In a cluster setup (master + nodes), each machine can resolve a different local media path for the same predefined stream. When you add a Stream layer from the predefined list, C-Play stores the entry's *title* as a stable key on the layer and syncs that key to all machines. Each machine then looks up the entry in its own local `data/predefined-streams.json` and resolves which path it should open:

1. **`paths[role]`** — An optional object mapping roles to paths. The role is `"master"` on the master, or the node id from `data/multivideo/nodes.json` on nodes (the machine's IP address when no id can be resolved). When a key exists for that machine its value wins over everything else - even an empty value (`""` or `null`). An empty value means *this machine intentionally does not open this stream*.
2. **`pathTemplate`** — An optional template string where `{nodeId}` is replaced with the machine's role/node id (e.g. `"av://dshow:video=Capture Card ({nodeId})"`). Used when there is no explicit `paths` entry for that machine.
3. **`path`** — The plain default path, used as a fallback when neither of the above applies. It may be empty, in which case only machines listed under `paths` open the stream.

If an entry's title does not exist at all in a machine's local file (for instance if the files differ between machines), that machine falls back to the path synced from the master. Custom (non-predefined) stream paths are always used verbatim on every machine, and per-node resolution is not applied.

Example:

```json
{
    "title": "HDMI Capture 1",
    "path": "",
    "paths": {
        "master": "av://dshow:video=Capture Card (RX0)",
        "node-A": ""
    }
}
```

In this example the master opens its local capture card, while `node-A` intentionally does not open anything. All other nodes fall back to the plain `path`, which is also empty here - so only the master displays this stream.

Note that per-node resolution on a node requires the machine to be identifiable through `data/multivideo/nodes.json` (see [Multi-video composition](/media/cplaymulti)). The local JSON files are re-read every few seconds, so edits take effect without restarting C-Play.

DirectShow capture setups work the same way with `data/predefined-directshows.json`: when you add a DirectShow layer from the predefined list, C-Play stores the entry's *title* as a stable key on the layer and syncs it to all machines. Each machine then looks up the entry in its own local file and resolves which capture devices it should use:

1. **`devices[role]`** — An optional object mapping roles to device pairs (`{"videoDevice": ..., "audioDevice": ...}`). The role is `"master"` on the master, or the node id from `data/multivideo/nodes.json` on nodes (the machine's IP address when no id can be resolved). When a key exists for that machine its value wins over everything else - even when both values are empty (`""` or `null`), which means *this machine intentionally does not open any capture at all* and the layer stays idle there.
2. **`videoDevice` / `audioDevice`** — The plain default device names, used as a fallback when there is no explicit `devices` entry for that machine.

Unlike streams, DirectShow setups do not support `{nodeId}` templates - capture device names are machine-specific strings, so per-role entries are the natural way to describe them. If an entry's title does not exist at all in a machine's local file (for instance if the files differ between machines), that machine falls back to the device pair synced from the master. Custom (non-predefined) DirectShow layers always use their chosen devices verbatim on every machine, and per-node resolution is not applied.

Example:

```json
{
    "title": "HDMI Capture 1",
    "videoDevice": "",
    "audioDevice": "",
    "devices": {
        "master": { "videoDevice": "DELTA-hmi Video Source (card0 RX0)", "audioDevice": "" },
        "node-A": { "videoDevice": "Datapath VisionSC-DP2 Video 01", "audioDevice": "" }
    }
}
```

In this example the master captures from its DELTA card while `node-A` captures from its Datapath card. All other nodes fall back to the plain devices (both empty here) - so they stay idle for this setup.

DirectShow preset keys are stored in *.cplaypres* files (`"directShowPreset"`), so per-machine resolution survives saving and reloading a presentation.

Capture uses `sources[role]` with the inheritance described above. Spout uses `senders[role]`, then the default `sender`. Both use the same role and node identity rules. Keep the preset title consistent across machines; source values can differ in their local JSON files.

## Master placeholders for node sources

When a live source only exists on the nodes, configure the predefined source's
master entry as `{ "placeholder": true }`. The master renders a TextLayer label
such as `C-Play Layer 12 - Camera`, using the same name and ID format as layer NDI
output. Drag it in the master view or change its plane position as usual: these
controls still belong to the original source layer and are synchronized to the
nodes. Each node opens its own source and uses its own source dimensions.

Use the appropriate mapping in the local predefined source file:

| Layer | File | Master entry |
| --- | --- | --- |
| DirectShow | `data/predefined-directshows.json` | `"devices": { "master": { "placeholder": true } }` |
| Capture | `data/predefined-captures.json` | `"sources": { "master": { "placeholder": true } }` |
| Stream | `data/predefined-streams.json` | `"paths": { "master": { "placeholder": true } }` |
| Spout | `data/predefined-spouts.json` | `"senders": { "master": { "placeholder": true } }` |

The placeholder replaces both video and audio on the master. Node overrides and
the preset's default source continue to work as before. A null or empty master
entry still means no source and no placeholder. Only the master recognizes the
placeholder flag. The label uses the configured subtitle font, with an available
font as fallback; its texture is 1280 × 256 pixels and renders as mono text.

For example, a Capture preset with a placeholder on the master and different
inputs on two nodes:

```json
{
    "captures": [
        {
            "title": "Stage camera",
            "backend": "datapath",
            "input": 1,
            "enabled": true,
            "sources": {
                "master": { "placeholder": true },
                "node-A": { "input": 1 },
                "node-B": { "input": 2 }
            }
        }
    ]
}
```

Spout presets appear alongside discovered senders in the Spout selector, even
when the master has no sender. Select the preset's title; its `sender` is the
default sender name, and `senders` maps master/node roles to local sender names.
Preset titles should differ from discovered sender names.

The placeholder is resolved from each machine's local preset JSON. Keep preset
titles consistent across machines. Source configuration edits are picked up within a few seconds while the layer updates. Streaming the master
texture to nodes is suspended while a placeholder is active, so the nodes keep
receiving the original layer configuration and opening their own sources.
