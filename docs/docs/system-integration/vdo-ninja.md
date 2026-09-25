---
title: VDO.Ninja
sidebar_position: 5
---

# VDO.Ninja -> C-Play

[VDO.Ninja](https://vdo.ninja/) is a browser-based WebRTC streaming tool that lets anyone with a link share their camera, microphone, or screen in real time — no software to install on the guest side. It is well suited for bringing remote guests into a C-Play presentation as live layers: interview subjects, remote presenters, phone cameras, or screen shares from anywhere.

There are three ways to bring a VDO.Ninja stream into C-Play:

1. **Direct VDO.Ninja WHEP** (new in 2.4, requires a build with WebRTC support) — the guest publishes to VDO.Ninja's hosted WHIP service and C-Play pulls the matching WHEP endpoint straight into a **WebRTC** layer. No MediaMTX, no OBS, and no relay infrastructure of your own.
2. **OBS bridge** (all versions) — render the VDO.Ninja view link in an OBS browser source and send it into C-Play as NDI.
3. **Self-hosted MediaMTX** (new in 2.4, requires a build with WebRTC support) — route the guest's stream through your own MediaMTX server using VDO.Ninja's `&mediamtx` parameter, then pull it via WHEP. Use this when you want to host the relay yourself instead of relying on VDO.Ninja's servers.

![VDO.Ninja to C-Play — Option A (direct WHEP), B (OBS bridge) and C (self-hosted MediaMTX)](/assets/integration/vdo-ninja-options.svg)

## Option A: Direct VDO.Ninja WHEP (new in 2.4)

VDO.Ninja operates a hosted **WHIP/WHEP** service, so no relay server of your own is needed at all: the guest publishes to `https://whip.vdo.ninja/<TOKEN>` and the same stream becomes available as a standard WHEP endpoint at `https://whep.vdo.ninja/<TOKEN>`, which C-Play's WebRTC layer pulls directly. VDO.Ninja's servers — including its TURN service, advertised to clients through the WHEP handshake — handle delivery, so this works across ordinary home/office networks without opening any ports on your side.

### Prerequisites

* A C-Play build compiled with WebRTC support (`BUILD_CPLAY_WITH_WEBRTC`, off by default). The **WebRTC** type appears in the *Add new layer* dialog only when it is enabled.
* A guest device with a modern browser (Chrome, Edge, Safari, or Firefox) — or OBS Studio 30+ if the guest prefers to publish from OBS.

### 1. Pick a stream token

Make up a long, random token (e.g. `guest-7f3a9c2b`). The token is the access control for this setup: anyone who knows `https://whep.vdo.ninja/<TOKEN>` can view the stream, so treat it like a password and use a different one per guest or show.

### 2. Have the guest publish to VDO.Ninja's WHIP service

**From a browser:** send the guest this link:

```text
https://vdo.ninja/?whipout=<TOKEN>
```

When they open it and allow camera/microphone access, their feed (or screen share) is published over WHIP to `https://whip.vdo.ninja/<TOKEN>`. The [VDO.Ninja WHIP page](https://vdo.ninja/whip) can also generate these links for you.

**From OBS Studio:** in *Settings > Stream*, choose service **WHIP**, set the server to `https://whip.vdo.ninja` and the stream key to `<TOKEN>`, then start streaming (see VDO.Ninja's [OBS-to-VDO.Ninja WHIP guide](https://docs.vdo.ninja/guides/from-obs-to-vdo.ninja-using-whip) for encoder settings and its notes on OBS's WHIP limitations over the Internet).

### 3. Add the WHEP URL to a WebRTC layer

1. In the presentation tool, click **+** (*Add new layer*) on the slide.
2. Choose type **WebRTC**.
3. Enter `https://whep.vdo.ninja/<TOKEN>` in the **WHEP URL:** field. If you leave the title empty, it defaults to the stream name taken from the last part of the URL.
4. Leave **Auth user/password** empty — this endpoint needs no credentials; the token in the URL is the access control.
5. Leave **Master relay** checked (see below) and add the layer.

The stream must be active for playback to start: if you add the layer before the guest goes live, C-Play keeps retrying every couple of seconds and picks up the feed automatically as soon as the guest publishes. To sanity-check that a guest's feed is actually live without C-Play, open `https://vdo.ninja/?whep=<TOKEN>` in a browser — it plays the same WHEP stream with VDO.Ninja's player.

Use Option A when:

* your C-Play build has WebRTC support and you do not want to run any relay server of your own,
* the guest is on an ordinary internet connection (VDO.Ninja's TURN service handles NAT traversal),
* a single guest feed is enough — for mixing several guests or adding graphics before the feed reaches C-Play, use Option B.

## Option B: OBS bridge (all versions)

This setup works with every C-Play version and is the most flexible when you want to composite the guest with lower thirds, masks, or other graphics in OBS first.

1. Create a VDO.Ninja room and copy the view link for the guest you want to show.
2. In OBS, add a **Browser** source with that view URL as its scene content.
3. Enable OBS NDI output (e.g. named `OBS VDO Ninja`).
4. In C-Play, add an **NDI** layer and select the sender.

The full walkthrough, including audio routing and switching the OBS scene from C-Play with a WebSocket command, is in [OBS Studio + NDI -> C-Play](/system-integration/obs-ndi#obs-vdoninja-example).

Use Option B when:

* your C-Play build has no WebRTC support (see below),
* you want to mix several guests or graphics in OBS before the feed reaches C-Play,
* the guest stream only needs to be visible on one machine.

## Option C: Self-hosted MediaMTX (new in 2.4)

Use this instead of Option A when you want the relay on your own infrastructure — for example to keep all traffic inside your network, to control firewalls and TURN yourself, or because your environment requires a WHEP endpoint with HTTP Basic authentication. VDO.Ninja publishes the guest's stream over **WHIP** to your [MediaMTX](https://github.com/bluenviron/mediamtx) server instead of relying only on peer-to-peer delivery; the same stream is then available as a standard **WHEP** endpoint, which C-Play's WebRTC layer pulls directly — no browser rendering and no NDI hop in between.

### Prerequisites

* A C-Play build compiled with WebRTC support (`BUILD_CPLAY_WITH_WEBRTC`, off by default). The **WebRTC** type appears in the *Add new layer* dialog only when it is enabled.
* A MediaMTX server with WebRTC enabled — see [MediaMTX streams](/system-integration/mediamtx) for the general setup.
* A guest device with a modern browser (Chrome, Edge, Safari, or Firefox).

### 1. Enable WebRTC on the MediaMTX server

In `mediamtx.yml`, make sure WebRTC is enabled:

```yaml
webrtc: yes
# webrtcAddress: :8889   # default WHIP/WHEP address
```

Open TCP port `8889` (WHIP/WHEP signaling) plus the UDP range MediaMTX uses for WebRTC media on the server host, both for the guests' networks and for the C-Play machines. Guests and C-Play must be able to reach this port directly.

### 2. Create the guest link with `&mediamtx`

Give the guest a push link that combines the room name, a fixed stream ID, and your MediaMTX server address:

```text
https://vdo.ninja/?room=MyRoom&push=guest1&mediamtx=mediamtx.example.com
```

| Parameter | Meaning |
| --- | --- |
| `&room` | The VDO.Ninja room. The director sees all guests in the room as usual — no extra parameters needed on the view side. |
| `&push=guest1` | A fixed stream ID for this guest. Without it, VDO.Ninja generates a random one per session (see step 3). |
| `&mediamtx` | Your MediaMTX server address. A bare domain name is treated as HTTPS on port `8889`; an IP address or explicit scheme/port can also be used. |

When the guest opens the link and allows camera/microphone access, their browser publishes to your MediaMTX server via WHIP. The director keeps seeing the guest in the VDO.Ninja room exactly as before — VDO.Ninja handles playback from the server automatically.

### 3. Find the WHEP URL

The stream is now available on the MediaMTX server at:

```text
https://mediamtx.example.com:8889/MyRoom/guest1/whep
```

The path is `<room>/<stream ID>`. The stream ID appears in the guest's browser address bar (or in the VDO.Ninja interface). With a fixed `&push=guest1` it stays stable across reconnects, so the URL can be saved with the presentation. With a random stream ID you must update the layer whenever the guest rejoins — WHEP URLs are ephemeral for that reason.

### 4. Add the WebRTC layer in C-Play

1. In the presentation tool, click **+** (*Add new layer*) on the slide.
2. Choose type **WebRTC**.
3. Paste the WHEP URL into the **WHEP URL:** field. If you leave the title empty, it defaults to the stream name taken from the last part of the URL.
4. Leave **Master relay** checked (see below) and add the layer.
5. Trigger the slide — video appears as soon as the guest is publishing, and audio plays through C-Play's normal audio path when the stream carries Opus audio.

The layer decodes H.264/H.265 with FFmpeg (NVDEC hardware decoding when available) and re-establishes the session automatically if it drops while the slide should be running — a guest who briefly loses connection comes back without touching C-Play.

In the *Layer View* you can change the **WHEP URL** at any time (changing it restarts the connection), set **Auth user/password** for HTTP Basic authentication, and toggle **Master only** / **Master relay**. See [Build presentation](/media/cplaypres) for the full layer reference.

## Master Relay option

The WebRTC layer has a **Master relay** checkbox — in the *Add new layer* dialog and on the WebRTC section of the *Layer View*. It is checked by default.

* **On (default):** the master pulls the WHEP stream once and relays it to all nodes through its built-in hub, so only one upstream connection is used. Nodes do not need direct network access to the WHEP server — they receive the feed from the master over the cluster network.
* **Off:** every machine that has this layer opens its own WHEP session directly against the endpoint. That means N concurrent readers on the server for an N-node cluster, and each node must be able to reach the WHEP URL itself.

For VDO.Ninja feeds, keep it on: the guest's WHIP publish is a single upstream stream, and fanning it out from the master keeps the load on the WHEP endpoint (VDO.Ninja's hosted service or your MediaMTX server) — and on the guest's upload link — at one copy regardless of cluster size. Turn it off only when each node has a better or more direct path to the WHEP endpoint than through the master.

The *Layer View* also offers **Master only** for WebRTC layers (the same Sync control as other live layers). When checked, the layer exists on the master only and is not synced to the nodes at all — useful when the VDO.Ninja feed is needed on the master alone, for example for [NDI output](/system-integration/ndi-output) or monitoring.

## Audio notes

* The guest's microphone arrives as Opus audio in the WHEP stream; C-Play decodes it and plays it through PortAudio like NDI/OMT layers. Volume and mute are available per layer in the *Layer View*.
* Have remote guests use headphones. VDO.Ninja applies echo cancellation by default, but speakerphone feedback is still a risk — especially when C-Play is also playing local media audio on the same machine.

## Limitations and troubleshooting

* **No WebRTC type in the Add Layer dialog** — this C-Play build was compiled without `BUILD_CPLAY_WITH_WEBRTC`. Rebuild with it enabled, or use Option B (OBS bridge).
* **Authentication** — C-Play's WebRTC layer supports HTTP Basic authentication only (**Auth user/password**, or `user:pass@host` embedded in the URL). The hosted VDO.Ninja WHEP endpoint needs no credentials at all, and MediaMTX uses Basic auth, so both work out of the box. WHEP endpoints that require a bearer token cannot be pulled directly — for those, front them with MediaMTX (Option C) or use the OBS bridge.
* **Stream ID changes** (Option C) — without a fixed `&push=ID`, each guest session gets a new stream ID and therefore a new WHEP URL. Update the layer's WHEP URL when the guest rejoins (this is also why MediaMTX streams are not offered in the predefined stream list). Option A has no such issue: the token you choose is the stable part of the URL.
* **Firewall** — with Option C, if the guest publishes but C-Play shows nothing, check that TCP `8889` and the WebRTC media UDP range are open on the MediaMTX host for both directions. With Option A no ports need to be opened on your side; verify instead that the token in the WHEP URL matches the one the guest is publishing with exactly (case-sensitive).
* **Black screen while the guest is live** — for Option C, verify that the WHEP URL path matches `<room>/<stream ID>` exactly (case-sensitive) and that the stream shows up as active on the MediaMTX server, e.g. through its logs or the [MediaMTX streams](/system-integration/mediamtx) dialog in C-Play. For Option A, open `https://vdo.ninja/?whep=<TOKEN>` in a browser to confirm the stream is actually being published — if it plays there but not in C-Play, check that the WHEP URL and token match exactly.

## Related

* [OBS Studio + NDI -> C-Play](/system-integration/obs-ndi) — Option B details, including the VDO.Ninja browser-source example
* [MediaMTX streams](/system-integration/mediamtx) — server setup, control API, and RTSP/SRT/WebRTC layers
* [C-Play -> NDI output](/system-integration/ndi-output) — send C-Play content back out, e.g. to the VDO.Ninja room through OBS
* [Build presentation](/media/cplaypres) — adding and configuring WebRTC layers
