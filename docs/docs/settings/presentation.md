---
title: Presentation settings
sidebar_position: 7
---

# Presentation settings (C-Play v2.1 and newer)

![Presentation settings](/assets/ui/settings/presentation.png) 

The presentation settings within C-Play control defaults for layers, slide behavior, and timing for the presentation system.

### Startup

* **Presentation to load on startup** — Path to a *.cplaypres* file loaded automatically when C-Play starts. Leave empty for none.

### Layer defaults

These defaults are applied when adding new layers. Each layer can be changed individually after creation.

* **Default stereoscopic mode** — Stereo format for new layers: 2D (mono), 3D side-by-side, 3D top-bottom, or 3D top-bottom+flip (default 2D).
* **Default grid mode** — Mapping mode for new layers: None (pre-split), Plane, Dome, Sphere EQR, or Sphere EAC (default Plane).
* **Default visibility** — Initial visibility percentage for new layers (0–100, default 0).

### 3D view layer dragging

* **Ctrl/Alt/Shift+drag on layer** — Each of Ctrl, Alt and Shift + left-drag in the [3D view](/playback/views) manipulates the selected layer with an independently chosen operation: *Elevation & Azimuth*, *Elevation*, *Azimuth*, *Horizontal & Vertical Move*, *Horizontal Move*, *Vertical Move*, *Resize plane size* or *Move plane distance*. Defaults: Ctrl and Alt aim at the pointer, Shift resizes the plane. Sphere and dome layers rotate with the pointer regardless of these settings.

### Master control

* **Media visibility controls master layer visibility** — When enabled, the primary media visibility (from "Open file" or the playlist) controls the fade in/out and start/stop of master layers (default on).
* **Master volume controls all layers volume** — When enabled, changing the master volume in the header also affects all layer volume levels, applied as a percentage of each layer's individual maximum (default on).

### Lockable layers

* **Master slide can have lockable layers** — Allow layers in the master slide to be locked (default off).
* **Custom slides can have lockable layers** — Allow layers in custom slides to be locked (default off).

### Pre-loading

* **Pre-load all layers** — Pre-load all layers at startup (default off). In most cases it is recommended to use the *Preload Layers* button in the Slides toolbar instead.
* **Number of upcoming slides to preload** — How many upcoming slides to load ahead when triggering a slide, for smoother transitions (0–10, default 2).

### PDF rendering (requires Poppler support)

* **DPI (dots per inch) for rendering PDF pages** — Resolution for PDF page rendering (0–1000, default 300).

### Timing

All timings are in milliseconds:

* **Fade duration to previous slide** — Fade time when moving backwards in the slide deck (20–20000, default 20).
* **Fade duration to next slide** — Fade time when moving forwards in the slide deck (20–20000, default 2000).
* **Clear and load delay** — Delay between clearing the old presentation and loading a new one, to allow nodes to clear first (0–20000, default 1000).
* **Sync after load delay** — Delay after loading before syncing, to wait for layers to be ready (0–20000, default 1000).
* **Start after load delay** — Delay after loading before starting playback, if layers are visible at startup (0–20000, default 5000).
* **Presentation change sync iterations** — Number of network sync iterations when changing presentations (0–20000, default 30).

### Layer streaming to nodes

These settings configure the defaults for [streaming layer textures from the master to the nodes](/playback/node-streaming) (C-Play v2.4 and newer, builds with `BUILD_CPLAY_WITH_NODE_STREAM`). Each layer can override the group and port individually in its node stream options.

* **Prefer NDI for streaming layers to the nodes** — Stream the layers to the nodes as NDI sources received by automatically created NDI layers on the nodes, instead of UDP multicast (default off). Can be overridden per layer. Only shown when both node streaming and NDI support are built in; requires NDI on the master and the nodes.
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
