---
title: Layers and sources
sidebar_position: 9
---

# Layers and sources

A layer adds media or an automation action to a slide. Use several layers to combine a background, video, live camera feed, labels, and show-control commands. See [Build presentation](/media/cplaypres) for creating slides, ordering layers, saving the presentation, and using visibility and timelines.

## Add and configure a layer

1. Select a slide, or select the **Master** slide for permanent Back layers behind the main player or Front layers above the slide content.
2. Click **Add new layer** in the layers toolbar.
3. Choose a type, give it a descriptive title, and select its file, source, text, or command.
4. For visual layers, choose the mapping and stereo mode that match the content.
5. Open **Layer View** to preview the source and adjust its settings. Save the presentation after editing.

The types available depend on the platform and the features enabled in your C-Play build. Audio and command layers have no image to map. **MultiVideo** appears with that name in the type selector; its settings come from a composition file.

## Choose a layer type

![Layer types in the Add new layer dialog](/assets/ui/layer_types.png)

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

A MultiVideo layer groups several video files into one synchronized composition. Use it for installations where different nodes or eyes need different video files while sharing one playback timeline.

Choose **MultiVideo** and open a `.cplaymulti` composition file. It defines a master reference video and the video entries each node should load, including per-entry mapping, stereo, eye, and audio settings. Define node IDs in `data/multivideo/nodes.json` and make the referenced files available on the corresponding machines. See [Multi-video composition](/media/cplaymulti) for the complete format and examples.

### Audio

Choose **Audio** and browse to an audio file, such as WAV, MP3, FLAC, OGG, or M4A. Use Layer View to adjust playback and volume. Add it alongside visual layers for a soundtrack or cue; mapping and ROI do not apply. Audio routing follows the application's master/node audio settings. See [Audio files](/media/audio).

### PDF

Choose **PDF** and browse to a PDF document. Open Layer View to select the page to display, then position and map it like a still image. Use separate layers or slides for pages that need independent placement or timing. PDF support must be included in the build.

### Text

Choose **Text**, enter the label or caption, and add the layer. In Layer View, edit the text and adjust font, font size, alignment, color, and texture dimensions. Text can contain multiple lines. Use plane mapping to place a label at a specific position, or choose another mapping when the text should follow that surface. Make the chosen font available on machines that render the layer.

The default text settings come from **Settings → Configure → Text & subtitles**. A Text layer is useful for authored content; [master placeholders](/media/layer-operations#master-placeholders-for-node-sources) generate a label automatically for a node source.

## Live sources

### NDI

NDI (Network Device Interface) carries live video and audio between applications and devices over a network. An NDI layer brings a sender's output, such as an OBS scene or camera feed, into your presentation as a surface you can position and map.

When choosing *"NDI (Network Device Interface)"*, a combobox becomes visible instead of the file dialog field. The combobox contains the discovered NDI sources on the network that can be received. Both image and audio are supported through NDI.

See [NDI integration](/system-integration/obs-ndi) for sender setup.

### OMT

OMT (Open Media Transport) is another network transport for live video and audio. An OMT layer receives output from an OMT-compatible application or device; use it when your source publishes an OMT feed.

When choosing *"OMT (Open Media Transport)"*, a combobox becomes visible instead of the file dialog field. The combobox contains the discovered OMT sources on the network that can be received.

### Spout

Spout shares live GPU textures between Windows applications on the same computer. A Spout layer can bring graphics from a rendering engine or another visual application directly into the presentation, with the sender running locally on each receiving machine.

When choosing *"Spout"*, a combobox becomes visible instead of the file dialog field. The combobox lists the currently available Spout senders from other Windows applications on the same machine, and the refresh button can be used to rescan the sender list. Spout layers are available by default in C-Play v2.3 Windows builds.

Predefined Spout setups are loaded from `data/predefined-spouts.json` and appear alongside discovered senders. Each entry has a `title`, default `sender`, optional `enabled`, and a `senders` mapping for per-machine overrides. Select the preset title so nodes resolve their own sender names. A sender must run locally on every machine that receives it. See [Spout for local app sharing](/system-integration/on-top#spout-for-local-app-sharing).

### DirectShow

DirectShow is a Windows framework for accessing media devices and decoders. A DirectShow layer brings in a local webcam, capture card, or media file through the Windows drivers and filters installed on that machine.

When choosing *"DirectShow"* (Windows only), **Video device** and **Audio device** comboboxes become visible in addition to the file dialog field. They list the capture devices registered on this machine — cameras and capture cards for video, microphones and line inputs for audio — and a refresh button rescans them when new hardware has been plugged in. If you pick a video device, the layer captures live from that device; if no video device is chosen, the file dialog field is used instead and the selected image or video file is rendered through a DirectShow filter graph using the system's built-in decoders, which makes it useful for media that the regular Image or Video layers cannot decode. Still images show their single frame; video files keep playing through the same pipeline. The **Audio device** combobox also offers a *No audio capture* entry: selecting it creates a video-only layer that captures no audio at all - not from a microphone and not embedded in the capture card either. A picked audio device is stored with the layer and its audio is routed to the machine's audio output; for media files without an explicit audio device, the file's own audio track is used when available.

![Add DirectShow layer](/assets/ui/layer_add_directshow.png)

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
- `devices` is an optional object with per-machine device overrides, keyed by role (`"master"` or node ids from `data/multivideo/nodes.json`) - see [Per-node resolution in clusters](/media/layer-operations#per-node-resolution-in-clusters). An entry whose plain devices are both empty but that has a non-empty `devices` object is still shown in the UI.
- Entries where both device names are empty and there is no `devices` object are ignored.
- The file is re-read every time the predefined list is opened, so you can edit it while C-Play is running and pick up changes without restarting.

When a DirectShow layer is created from a predefined setup, its parameter string is set to `videoDevice|audioDevice` (the audio part may be empty) and the entry's *title* is stored as a stable key on the layer, so each machine in a cluster can resolve its own local capture devices.

### Capture

A Capture layer receives a capture card's input through a supported manufacturer's SDK, such as Datapath's. Use it to bring an external computer's display or a camera signal into the presentation with hardware-specific options such as combining several inputs into one image.

Choose **Capture** for a supported capture card SDK backend, such as Datapath. Select a predefined setup from `data/predefined-captures.json`, or switch to a custom input. Input numbers are **1-based**, matching the Datapath Vision utility; the selected input can exist only on the nodes.

For custom inputs, configure **ganging** when several inputs form one image, choose whether to use **direct GPU** transfer, and enable source audio when needed. Direct GPU transfer depends on suitable hardware and backend support. Layer View shows the capture status, so check it for missing sources, missing signal, or backend errors.

A capture preset contains `title`, `backend`, `input`, and optional `ganging`, `directGpu`, `audio`, and `enabled` fields. Its `sources` object overrides fields for each machine. A role entry inherits the default fields it does not specify. Set a role to `null` or an input of zero to leave capture disabled there. See [per-node resolution](/media/layer-operations#per-node-resolution-in-clusters) and the [master placeholder example](/media/layer-operations#master-placeholders-for-node-sources).

### WebRTC

WebRTC carries live video and audio with low latency. In C-Play, a WebRTC layer receives a feed from a WHEP endpoint (WebRTC-HTTP Egress Protocol), such as one provided by MediaMTX or VDO.Ninja, making it useful for remote cameras, presenters, and screen sharing.

When choosing *"WebRTC"*, a **WHEP URL** field becomes visible instead of the file dialog field. Enter the WHEP endpoint of the stream you want to pull, for example `http://mediamtx:8889/live/mystream/whep` when using [MediaMTX](https://github.com/bluenviron/mediamtx) as the WebRTC server; credentials can be embedded in the URL (`user:pass@host`). If you leave the title empty, it defaults to the stream name taken from the last part of the URL. The layer receives both video and audio when the stream carries them (H.264/H.265 video, Opus audio), decodes with FFmpeg — NVDEC hardware decoding when available — and reconnects automatically if the session fails while the layer should be running. The WHEP URL can be changed later in the *Layer View*; changing it restarts the connection. Like other live layers it can be marked master-only with the **Sync** button: then only the master pulls the stream; otherwise delivery follows the **Master relay** setting described below.

![Add WebRTC layer](/assets/ui/layer_add_webrtc.png)

The **Master relay** option controls whether the master pulls and relays the feed, or nodes pull it themselves. Select the mode that matches your network and server setup. See [MediaMTX integration](/system-integration/mediamtx).

### Stream

A Stream layer plays an ongoing media feed from a URL, such as a camera or encoder publishing through RTSP or SRT. Use it for live inputs that should follow the incoming feed, with playback handled by C-Play's MPV backend.

When choosing *"Stream"*, you can choose between pre-defined streams for your system in a combobox, loaded from the editable file *"data/predefined-streams.json"*, or add a custom entry in a text field. The mode button next to the field cycles through *predefined list -> custom path -> MediaMTX server* (the last option only appears when [MediaMTX](/system-integration/mediamtx) is available).

Stream layers are for **live streaming**: camera feeds, capture cards, hardware or software encoders, and media servers. The stream is played by MPV with a low-latency profile and untimed demuxing applied automatically, so the layer follows the live feed with minimal delay instead of buffering it like a video file.

The most common URL types are:

* **SRT** — `srt://host:port/path`. A low-latency protocol that runs over UDP, well suited to lossy or long-distance networks (for example between buildings or across the internet). The bundled ffmpeg build includes libSRT, so SRT URLs work out of the box; a MediaMTX server serves SRT on port `8890` by default.
* **RTSP** — `rtsp://host:port/path`. A solid default that also works over TCP transport on restrictive networks. The included *Rtsp-tcp-lowlatency* options profile (selectable in the layer view) forces RTSP-over-TCP, disables caching, and shortens probing so the stream starts quickly.

Any other MPV-compatible input can be used as well — for example Windows capture cards and cameras through `av://dshow:video=...` paths, or raw UDP multicast feeds from encoders. See the [MPV documentation](https://mpv.io/manual/stable/) for the full list of supported protocols.

In a cluster setup every node opens the same stream URL; layers created from the predefined list can instead resolve a different local path per machine — see [Per-node resolution in clusters](/media/layer-operations#per-node-resolution-in-clusters).

> **YouTube:** pasting a YouTube URL into a Stream layer also works — C-Play detects it and plays it as video-on-demand content through yt-dlp. For YouTube videos, prefer the dedicated **YouTube** layer type below, which is purpose-built for that use case with full player controls.

### YouTube

When choosing *"YouTube"*, a single **URL** field becomes visible instead of the file dialog field. Paste a single YouTube video URL (`watch`, `shorts`, or `youtu.be`) and the layer plays it through mpv's embedded `ytdl_hook`, which uses the external [yt-dlp](https://github.com/yt-dlp/yt-dlp) tool to resolve the actual media streams. A YouTube layer is treated as video-on-demand content, so — unlike a live Stream layer — it gets the full set of player controls in the *Layer View*: play/pause, a seek slider, stop/rewind, end-of-file mode (pause or loop) and A-B section looping, exactly like a Video layer. It is never forced into the live-stream low-latency mode (which would cause audio drift).

![Add YouTube layer](/assets/ui/layer_add_youtube.png)

#### YouTube playback and yt-dlp

YouTube URLs are resolved by mpv's `ytdl_hook.lua`, which spawns the external **yt-dlp** executable. C-Play locates yt-dlp on each machine in this order:

1. The **yt-dlp path** set in [*Settings → Streaming*](/settings/streaming#youtube-playback).
2. `yt-dlp.exe` (or `yt-dlp`) placed next to `C-Play.exe`.
3. `plugins/yt-dlp(.exe)` in the working directory C-Play was started from (also checked relative to the folder containing `C-Play.exe`).
4. Anywhere on the system `PATH`.

If none is found, the layer reports a load error and shows an error overlay in the *Layer View*. The layer-properties dialog and the settings page both show whether yt-dlp was found on the current machine.

> **Every cluster node needs yt-dlp too.** A YouTube layer stores its URL verbatim and syncs it to all machines; each machine resolves the URL locally with its own yt-dlp. A node without yt-dlp shows the error overlay while the master keeps playing — this is expected.

An optional **yt-dlp format** string in [*Settings → Streaming*](/settings/streaming#youtube-playback) is passed to `ytdl_hook` (`ytdl-format`) to cap resolution or pick specific codecs (for example `bv*[height<=1080]+ba/b`). Leave it empty to use the mpv/yt-dlp default.

A per-layer options profile named **Youtube** (from `data/mpv-conf/youtube_youtube.json`) is available in the *Layer View* options dropdown for cache/demuxer tuning.

Cookies, authentication and age-restricted content are out of scope, as are channel and playlist URLs (single video URLs only).

## Automation layers

Command layers have no visual output. Use their trigger controls or the slide timeline to dispatch actions. Configure and test the command before using it in a show.

### Control

A Control layer is a cue that operates C-Play itself: for example, starting the main video, changing volume, or loading a playlist item at a planned moment. It has no visual output and runs on the master, dispatching a player operation when activated through the slide timeline.

In the layer view for a control layer, two fields are shown instead of the usual grid/stereo parameters:

* **Operation** — The command to execute. Available operations include:
  * **Playback**: Play, Pause, Stop, Rewind, Seek, SetPosition, SetSpeed
  * **Volume & Fading**: SetVolume, FadeVolumeDown, FadeVolumeUp, FadeImageDown, FadeImageUp, SetSyncVolumeVisibilityFading
  * **Loading**: LoadFromPlaylist, LoadFromSlides, LoadFromSections, LoadFromAudioTracks
  * **Orientation**: SpinPitchUp, SpinPitchDown, SpinYawLeft, SpinYawRight, SpinRollCW, SpinRollCCW, OrientationAndSpinReset, RunSurfaceTransition
  * **Surface & Visibility**: SetBackgroundVisibility, SetForegroundVisibility, SetNodeWindowsOpacity

* **Parameter** — An operation-specific value. For example, a volume level for SetVolume, a signed number of seconds for Seek, a slide name or index for LoadFromSlides, or `true`/`false` for boolean operations. Parameters that reference playlist items, slides, or audio tracks can be specified by name or numeric index.

### REST

A REST layer is a cue that controls an external system through an HTTP or WebSocket request. For example, it can switch an OBS scene or call a lighting controller's API when a slide starts. It has no visual output and sends its request from the master.

In the layer view for a REST layer, three fields are shown:

* **URL** — The target HTTP or WebSocket endpoint. You can either select a predefined command from the dropdown (populated by the REST Commands Editor) or toggle to custom mode and enter a URL manually.
* **Method** — The request method: `GET`, `POST`, `PUT`, `DELETE`, `WS`, or `WSS` (visible in custom mode).
* **Parameters** — A list of name/value pairs, each with individual UI fields that can be added or removed as needed (visible in custom mode). This format supports values with spaces and special characters. For POST/PUT these are sent as the request body; for GET/DELETE they are appended as query parameters; for WS/WSS they are sent as a WebSocket message or OBS WebSocket request data.

When the slide is loaded, the REST layer fires the configured request on a background thread and reports success or failure.

For more details on the REST layer and the REST Commands Editor, see the [HTTP Web API documentation](/remote-control/api#rest-layer).

### TCP

A TCP layer is a cue that sends a saved text or binary command over a persistent network connection. Use it for devices and show-control systems with a TCP command protocol, such as triggering a Medialon task or sending a projector command. It has no visual output and runs on the master.

Choose **TCP** to trigger a reusable command through one of C-Play's persistent TCP server connections. Define servers and text or binary commands in [**Settings → Configure → TCP control**](/settings/tcp_control), then select a command when adding the layer. Each server keeps its own connection, so multiple layers can send different commands to different devices.

The layer fires when started. Double-click it or press **Trigger** in its layer view to send manually; the layer view also lets you choose another command. Its status indicates whether the transport accepted the command, not whether the device executed it. Disconnected commands are rejected and are never replayed after reconnect. See [TCP control client](/remote-control/tcp) for framing, persistence, and examples.

## Next steps

To position and crop a layer, apply post process effects such as chroma key and color correction, or choose which machines in a cluster open its source, see [Layer operations](/media/layer-operations).
