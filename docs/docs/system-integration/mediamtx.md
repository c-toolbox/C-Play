---
title: MediaMTX streams
sidebar_position: 5
---

# MediaMTX streams in C-Play

[MediaMTX](https://github.com/bluenviron/mediamtx) is a small real-time media server that can receive video from cameras, encoders, OBS, or WebRTC senders and republish it over RTSP, RTMP, HLS, SRT, and WebRTC. C-Play can ask a MediaMTX server which streams it currently serves and turn any of them into a **Stream layer** with a correct RTSP or SRT URL. If the server has WebRTC enabled (`webrtc: yes`), streams can additionally be added as a **WebRTC layer** that pulls via WHEP.

C-Play uses **RTSP** as the primary protocol, because it gives the lowest latency with mpv-based playback and works with TCP transport on restrictive networks. **SRT** is offered as an alternative for UDP-friendly networks; it is played by the same Stream layer (the bundled mpv/ffmpeg build includes libSRT).

## Enable the MediaMTX control API

The stream list is read from the MediaMTX *Control API*, which is **disabled by default**. In `mediamtx.yml`, set:

```yaml
api: yes
apiAddress: :9997
```

If the server is on another machine, make sure `apiAddress` is not bound to localhost only, and that port `9997` is reachable.

If the API is protected by internal authentication, C-Play sends the user name and password as HTTP Basic auth.

## Add a server in C-Play

Open **Settings -> MediaMTX Streams...** and fill in the server form:

| Field | Meaning |
| --- | --- |
| Name | Label shown in C-Play. Also used as prefix when saving streams to the predefined list. |
| Host | Host name or IP of the MediaMTX server. |
| API | Scheme and port of the control API (`http`, port `9997` by default). |
| RTSP | Scheme and port used to build playback URLs (`rtsp`, port `8554` by default). |
| WebRTC | Scheme and port of the WHEP endpoint used for WebRTC layers (`http`, port `8889` by default). Requires `webrtc: yes` in `mediamtx.yml`. |
| SRT | Port used to build SRT playback URLs (port `8890` by default). Requires that SRT is not disabled on the server (`srt: no`). |
| Username / Password | Optional API credentials. On Windows the password can also be stored in the Windows Credential Manager (see below). |
| Auto-detect RTSP/WebRTC/SRT ports | Reads `rtspAddress`, `rtspEncryption`, `webrtcAddress`, `webrtcEncryption`, and `srtAddress` from the server configuration and updates the schemes and ports automatically. |

Press **Add New** to store the server. Servers are saved in `data/mediamtx-servers.json`.

:::warning Passwords are not saved
The password is kept in memory for the current C-Play session only and is never written to `data/mediamtx-servers.json`. Re-enter it after restarting C-Play — or store it once in the **Windows Credential Manager** (see below).
:::

### Windows Credential Manager

On Windows, C-Play also looks up the password in the Windows Credential Manager. The entry is a *generic* credential whose target name combines the server's friendly name and API user:

```text
MediaMTX/<server name>/<username>
```

For example, for a server named `studio` with API user `reader`:

```powershell
cmdkey /generic:"MediaMTX/studio/reader" /user:reader /pass:secret
```

When such an entry exists, C-Play uses it automatically for **Test Connection**, **Fetch Streams**, and *Include credentials in URL*. A password typed into the dialog (with **Enter manually** checked) always takes precedence over the stored one. The editor shows whether a credential was found for the current name/user pair.

Press **Test Connection** to verify the API is reachable, then **Fetch Streams** to list the paths.

## Use a stream

The stream list shows every active path on the server, plus configured paths that are currently idle. For each path you get the publishing source type, the track codecs, the number of readers, and the resulting playback URL for the selected protocol.

Select a stream, pick the **Protocol** (RTSP, SRT, or WebRTC), and choose:

* **Add As Layer** - creates a Stream layer on the currently selected slide using the RTSP URL or an SRT URL (`srt://host:8890/path`), or a WebRTC layer using the WHEP URL (`http://host:8889/path/whep`) when WebRTC is selected.
* **Save To Predefined Streams** - appends the stream to `data/predefined-streams.json` so it shows up in the normal predefined stream list on every machine that has the same file. Available for RTSP and SRT, because the predefined stream list is consumed by mpv-based Stream layers; WebRTC is not offered here, since WHEP URLs are ephemeral.

WebRTC is only offered as a protocol when both of these hold:

1. The server has WebRTC enabled (`webrtc: yes` in `mediamtx.yml`). C-Play reads this from the global configuration on every fetch; if it is missing or disabled, only RTSP is shown.
2. This C-Play build was compiled with WebRTC support (the **WebRTC** layer type exists in the Add Layer dialog).

SRT is only offered as a protocol when both of these hold:

1. The server has SRT enabled (`srt` is not `no` in `mediamtx.yml`). C-Play reads this from the global configuration on every fetch; if it is disabled, SRT is not shown.
2. This C-Play build has a **Stream** layer type (SRT is played by mpv-based Stream layers).

You can also pick MediaMTX streams directly while adding a layer. In the **Add layer** dialog, choose type **Stream** and press the mode button next to the path field until the MediaMTX server and stream selectors appear. The mode button cycles through *predefined list -> custom path -> MediaMTX server*.

### Credentials in the URL

If the MediaMTX paths require authentication for reading, enable **Include credentials in URL** before creating the layer. The RTSP URL then becomes `rtsp://user:password@host:8554/path`, and the WHEP URL becomes `http://user:password@host:8889/path/whep`.

SRT URLs never carry credentials — **Include credentials in URL** is disabled for SRT. MediaMTX matches SRT readers against the path name and authenticates them by IP allowlist (`ipWhitelist`) or a path-level SRT passphrase (`srtPassphrase`), not by `user:password@`.

:::warning
Saving a stream with credentials to the predefined stream list writes the password to `data/predefined-streams.json` in clear text. Prefer read-open paths, or IP-based authentication in MediaMTX, for cluster setups.
:::

## Playback options

Both RTSP and SRT streams are played by mpv inside the Stream layer (the bundled ffmpeg build includes libSRT, so `srt://` URLs work out of the box). Note that SRT uses UDP port `8890` by default. A preset named **Rtsp-tcp-lowlatency** is included and can be selected per layer in the layer view. It forces RTSP over TCP, disables caching, and shortens probing so the stream starts quickly:

```json
{
    "rtsp-transport": "tcp",
    "cache": "no",
    "demuxer-lavf-probesize": "32",
    "demuxer-lavf-analyzeduration": "0.1",
    "profile": "low-latency"
}
```

Additional presets can be added as `data/mpv-conf/<name>_stream.json`.

## Cluster notes

A MediaMTX stream layer stores its playback URL (RTSP or SRT) directly, so every node in the cluster connects to the same server address. If nodes should pull from different servers, save the stream to `data/predefined-streams.json` and then edit that entry to use the per-node `paths` map or a `pathTemplate`, as described in the predefined stream documentation.
