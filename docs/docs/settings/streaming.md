---
title: Streaming settings
sidebar_position: 8
---

# Streaming settings (C-Play v2.4 and newer)

![Streaming settings](/assets/ui/settings/streaming.png)

The streaming settings within C-Play control YouTube URL resolution, NDI color conversion and output resolution, and the defaults for [streaming layers from the master to the nodes](/playback/node-streaming).

### YouTube playback

These settings apply to [YouTube layers](/media/layers#youtube-playback-and-yt-dlp) and Stream layers that use a YouTube URL — mpv resolves the URLs through its embedded `ytdl_hook`, which spawns an external yt-dlp executable:

* **yt-dlp path** — Path to the yt-dlp executable used on this machine (with browse and clear buttons). Empty = auto-detect: next to C-Play.exe, then `<working directory>/plugins/`, and finally anywhere on PATH. The page shows whether yt-dlp was found on this machine.
* **yt-dlp format** — Optional yt-dlp format selector passed to mpv's `ytdl_hook` (`ytdl-format`) to cap resolution or pick specific codecs (for example `bv*[height<=1080]+ba/b`). Empty = mpv/yt-dlp default.

> **Every cluster node needs yt-dlp too.** A YouTube layer stores its URL verbatim and syncs it to all machines; each machine resolves the URL locally with its own yt-dlp.

### NDI

* **Convert NDI input on the GPU** — Receive NDI in the decoder's fastest native format and convert it to RGBA on the GPU, preserving alpha. Recreate NDI layers after changing it.
* **Convert NDI output on the GPU** — Convert C-Play's NDI output on the GPU, halving the readback size. Discards alpha. Stop and start outputs after changing it.
* **NDI output resolution for 3D view** — Resolution used when the [NDI output](/system-integration/ndi-output) publishes the 3D view instead of the main video: *2K*, *4K*, *6K*, or *8K* (default 2K). The perspective camera and the sphere view keep a 16:9 aspect ratio (from 1920×1080 up to 7680×4320), while the fisheye (fulldome) camera is square (from 2048×2048 up to 8192×8192). The selected value applies when the 3D view output starts, for example after toggling NDI output off/on.

Configure each receiving computer separately, including nodes. Supported formats, deinterlacing and fallbacks are described in [NDI output → Optional GPU color conversion](/system-integration/ndi-output#optional-gpu-color-conversion).

### Layer streaming to nodes

These settings configure the defaults for [streaming layer textures from the master to the nodes](/playback/node-streaming) (builds with `BUILD_CPLAY_WITH_NODE_STREAM`). Each layer can override the group and port individually in its node stream options.

* **Prefer NDI for streaming layers to the nodes** — Stream the layers to the nodes as NDI sources received by automatically created NDI layers on the nodes, instead of UDP multicast (default off). Can be overridden per layer. Only shown when both node streaming and NDI support are built in; requires NDI on the master and the nodes.
* **Enable NodeStream by default for new layers** — Layer types (*DirectShow*, *Spout*, *Stream*, *YouTube*) for which the *Add new layer* dialog preselects **Master sends content to nodes** instead of **Nodes read from original source**.
* **Base multicast group** — Multicast group of the layer streams to the nodes (default `239.192.77.1`). Each layer uses the base group plus its layer id, unless a group is set on the layer.
* **Base port** — UDP port of the layer streams to the nodes (1024–65535, default 50100), unless overridden on the layer.
* **Master interface address** — IPv4 address of the network interface to send from. Empty uses the master address of the cluster configuration.
* **Max datagram size (bytes)** — Largest UDP datagram in bytes (576–65507, default 1472). 1472 fits a standard 1500 MTU, 8972 fits 9000 byte jumbo frames.
* **Send rate limit (Mbit/s)** — Combined send rate of all layer streams (default 0 = 85% of the link speed of the send interface).
* **Compress streams with LZ4** — Losslessly compress each packet with LZ4 (default OFF). Saves bandwidth on flat graphics and text, at a small CPU cost on the master and the nodes. Packets that do not shrink are sent uncompressed.
* **Send only changed regions (delta frames)** — Between keyframes, send only the parts of the layer that changed since the previous frame (default on). A node that misses a delta frame holds the image until the next keyframe, unless *Show frames with lost packets* is enabled.
* **Keyframe interval (msec)** — How often a full frame is sent for nodes that joined late or lost packets (50–10000, default 500).
* **Frame-locked wait (msec)** — How long a node waits for a late frame in frame-locked mode (0–50, default 2).
* **Frame-locked latency guard (msec)** — In frame-locked mode, the nodes are told to show a frame only once it has been on the wire this many milliseconds (0–100, default 1).
* **Multicast TTL** — Multicast time-to-live in router hops (1–32, default 1).
* **Deliver streams to nodes running on the master computer** — Loopback delivery of the streams to node instances on the master itself (default on).
* **Show frames with lost packets on the nodes** — Let the nodes show frames with lost packets on top of the previous frame instead of holding the previous complete frame until a complete one arrives (may briefly mix two frames, default off).
