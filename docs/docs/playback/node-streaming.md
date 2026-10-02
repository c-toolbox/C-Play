---
title: Streaming layers from master to nodes
sidebar_position: 7
---

# Streaming layers from master to nodes (C-Play v2.4 and newer)

By default, every layer in a presentation is loaded and rendered on **each** machine of the cluster — the master only syncs the layer's settings, and each node decodes its own copy of the video or image. From C-Play version 2.4, you can instead stream the *rendered texture* of a layer from the master to all nodes: the layer is rendered once on the master and sent over the network as compressed frames, so the nodes only have to receive and display it.

This is useful when:

- A video layer is expensive to decode (e.g. 8K or high-bitrate HEVC) — decoding then happens once on the master instead of once per node.
- You want all nodes to show exactly the same frame at the same time, without depending on each machine's decoder timing.
- The nodes are low-spec machines that should not spend GPU/CPU resources on decoding.

The feature is available in builds with `BUILD_CPLAY_WITH_NODE_STREAM` (ON by default). When NDI support (`BUILD_CPLAY_WITH_NDI`) is also present, a layer can be streamed over **NDI** instead of UDP multicast — see [Streaming over NDI](#streaming-over-ndi) below.

## Enabling for a layer

1. Open the *Layer View* for the slide and select the layer you want to stream (any layer type except Audio).
2. In the layer's control row, click the **Nodes** button to enable streaming of that layer's texture from the master to all nodes.
3. The button icon shows the state: crimson when OFF, orange when ON but not actively sending yet, and lime while frames are being sent. The tooltip shows where the layer is streamed — the multicast address or the NDI source name.

While a layer is streamed to the nodes it always exists on them as a stream receiver, so the **Sync** (master-only) button is disabled for that layer. Node streaming of a layer and plain [NDI output](/system-integration/ndi-output#layer-ndi-output) of the same layer are mutually exclusive — enabling one turns the other off.

### Per-layer options

The configure (![](/assets/icons/configure.svg)) button next to the **Nodes** button opens the per-layer stream options:

| Option | Description |
|--------|-------------|
| **Prefer NDI (instead of UDP multicast)** | Stream this layer as an NDI source received by automatically created NDI layers on the nodes. Overrides the global preference for this layer; unchecking it makes the layer follow the global setting again. Only shown when both node streaming and NDI support are built in. |
| **NDI source** | Read-only name of the published NDI source: `C-Play Layer <layer id> - <layer title>` (shown when NDI is used). |
| **Compression** | GPU block-compression format for the UDP stream: *Auto* (BC1, or BC3 when the layer has alpha), *BC1* (RGB, smallest), *BC3* (RGBA) or *BC7* (RGBA, best quality). |
| **Sync** | *Frame-locked to the master* — nodes wait for a complete frame and show it at the same time as the master (tightest cluster sync); *Immediate (lowest latency)* — nodes show each frame as soon as it arrives. |
| **Max frame rate** | Cap on frames per second sent for this layer; 0 sends every rendered frame (0–240). |
| **Multicast group** | Override the multicast group for this layer. Empty uses the automatic group: base group + layer id from the [presentation settings](/settings/presentation#layer-streaming-to-nodes). |
| **Port** | Override the UDP port for this layer; 0 uses the base port from the presentation settings. |
| **Address** | Read-only effective multicast address (group:port) of the layer stream. |

The compression, sync, frame rate, group and port options only apply to the UDP multicast path — they are disabled while NDI is used for the layer. All per-layer values are saved with the presentation.

## Global settings

The defaults for all layer streams are set in *Settings → Presentation*, section **Layer streaming to nodes (UDP multicast)** — see [Presentation settings](/settings/presentation#layer-streaming-to-nodes). In short: base multicast group and port, master send interface, TTL, max datagram size, combined send rate limit, frame-locked wait time and latency guard, partial-frame handling, loopback delivery to nodes running on the master computer, and the global **Prefer NDI** checkbox.

## How it works (UDP multicast)

The master block-compresses each rendered layer texture on the GPU (BC1/BC3/BC7) and sends it as self-describing UDP datagrams to a per-layer multicast group; every node joins that group and reassembles the frames into a texture. Because packets are independent, a frame with lost packets can still be uploaded region by region:

- With **Frame-locked** sync (default), nodes hold the previous complete frame until the new one is fully received — with a small configurable wait for late packets and a latency guard so all nodes switch to the new frame at the same instant.
- With **Immediate** sync, each node uploads every packet as it arrives for lowest possible latency; frames may be shown slightly earlier on faster machines.
- When *Show frames with lost packets* is enabled in the settings, partially received frames are displayed on top of the previous one instead of holding it (two frames can briefly mix).

The stream carries **video only**: the layer's audio continues to play from the master, where the media is actually decoded — the node-side receiver does not decode any media. All other layer parameters (grid/mapping mode, visibility, position, etc.) keep applying on each node as usual; only the texture source changes from "load locally" to "receive from the master".

## Streaming over NDI

When C-Play is built with both node streaming and NDI support, layers can be streamed to the nodes as **NDI sources** instead of UDP multicast:

- Enable it globally with *Prefer NDI for streaming layers to the nodes* in [Presentation settings](/settings/presentation#layer-streaming-to-nodes), or per layer with the checkbox in the node stream options.
- The master then publishes the layer texture as an NDI source named `C-Play Layer <layer id> - <layer title>` and does not open any UDP socket for it; each node automatically creates a regular [NDI layer](/system-integration/obs-ndi) that receives that source, exactly like a manually added NDI layer.
- Switching the preference (globally or per layer) makes the nodes swap between the NDI receiver and the UDP stream receiver on the next sync.

Limitations of the NDI path:

- **Video only** — no audio is sent with the NDI source yet; the node's receiver layer stays silent.
- **No frame-locked mode** — NDI delivery is asynchronous, so the frame-locked sync mode and the latency-guard settings are UDP-only (the options are hidden in the UI when NDI is used).
- Requires NDI to be available on **both** master and nodes at runtime; if it is not, the layer falls back to UDP multicast.
