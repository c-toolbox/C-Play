---
title: TCP control
sidebar_position: 3
---

# TCP control

C-Play provides both an outgoing TCP Control Client and an incoming TCP Control Server on the ready master. Configure both in **Settings → Configure → TCP control**. Cluster nodes do not open control connections or listen for commands.

| Component | Connection direction | Purpose |
| --- | --- | --- |
| [TCP Control Client](#tcp-control-client) | C-Play connects to an external TCP server. | Send saved commands from presentation layers and send state updates to devices or control systems such as Medialon. |
| [TCP Control Server](#tcp-control-server) | An external TCP client connects to C-Play, normally on port 7008. | Control C-Play using the existing REST action names and parameters over a persistent TCP connection. |

The two components can run together. For Medialon, **C-Play Control Events** is the server receiving C-Play updates, while **C-Play TCP Control Client** connects to C-Play's server to send actions. HTTP control normally uses port 7007 and is configured separately.

## TCP Control Client

C-Play's master can keep independent outgoing connections to multiple TCP servers. Define reusable commands and trigger them from **TCP** presentation layers. Connections start once the player's media and slide models are available.

### Servers and reusable commands

1. Click **New server**, enter its name, host and port, configure framing and reconnection, then click **Save server**. Repeat for every device or control system.
2. Click **New command**, give it a name, select its server, and enter a UTF-8 text or hexadecimal payload. Click **Save command**.
3. Use **Trigger saved command** to send the saved command immediately.
4. Add a layer with type **TCP** and select a saved command. It fires when the layer starts. Double-click the layer to trigger it manually, or use **Trigger** in its layer view. The layer view also lets you change the referenced command.

For example, define a **Projector** server using CRLF termination and a **Lighting** server using binary messages. Create **Projector on** (`PWR ON`, UTF-8 text) and **Lighting scene** (`020100ff03`, hexadecimal bytes, exact wire bytes). Place them in separate TCP layers on the appropriate slides. They reuse the same persistent connections throughout the presentation.

Every server has its own enabled flag, connection state, error, framing, buffers, timeout, and retry backoff. A reconnect on one server does not interrupt the others. **Disconnect** stops retries for that server until **Connect**, saving that server, or application reactivation. Turn its **Enabled** flag off and save to keep it disconnected across restarts.

Server IDs are always the configured endpoint: `host:port`, such as `127.0.0.1:9000` or `showmaster.example:9000`. IPv6 uses `[address]:port`, such as `[2001:db8::1]:9000`. Hostnames are trimmed and lowercased, and numeric addresses use their canonical spelling. Duplicate endpoints are rejected. Changing a host or port changes its server ID and remaps all saved command references atomically. Layers refer to commands by stable IDs; renaming or changing a server endpoint preserves those command IDs and layer references. Editing a saved command updates all layers that reference it. A server cannot be deleted while commands still reference it. Deleting a command leaves referencing layers with an explicit missing-command rejection; it never silently selects another command.

### Configuration and persistence

Profiles and commands are saved atomically in `C-Play/tcp-control.json` beneath Qt's generic configuration directory. The settings page displays the exact path. Copy this file with a presentation to another master to preserve its server and command references. Presentations store each TCP layer's `tcpCommandId`; they do not embed device endpoints or payloads.

Existing multi-server configurations with arbitrary or UUID server IDs migrate on load: IDs derive from host/port and commands are remapped while their IDs remain unchanged. A saved user configuration is rewritten atomically with the migrated references; predefined source files load into memory. Duplicate endpoints or ambiguous old IDs reject the complete configuration without applying partial changes.

The original single-server `[TcpControl]` configuration is imported once as **Legacy TCP control** when it is enabled and the multi-server configuration file does not exist. An existing file is never replaced by migration.

#### Predefined startup configuration

When no saved `C-Play/tcp-control.json` exists and legacy TCP control is disabled, C-Play loads `./data/predefined-tcp-control.json` at startup, relative to the working directory, like the other `predefined-*` files. It uses the same `version`, `servers`, and `commands` structure as the saved configuration. The packaged example defines a disabled Medialon server at 127.0.0.1 on port 9000 with CR framing (`sendTerminatorHex: "0d"`), `task_start MyTask` / `task_stop MyTask` commands, and all 26 ControlEventFeed monitoring commands.

Set the host to the Showmaster Editor computer during programming or the Showmaster address during standalone operation. Replace `MyTask` with your task name, enable the server, and save it in **TCP control**. Medialon's LLC device must listen on port 9000 and dispatch received commands to project tasks.

Predefined entries load into memory; saving a server or command writes the complete configuration to the user configuration path. That saved file takes precedence on later starts, even when empty or invalid. To use edited predefined entries again, back up and move the saved file out of that path before restarting. Keep IDs stable when editing examples so presentation layers retain their command references. Startup loads definitions and connects enabled servers; it does not automatically trigger commands.

Server fields in the JSON file are:

| Field | Default | Meaning |
| --- | --- | --- |
| `id`, `name` | derived `host:port`, required name | Endpoint identity and display name; IPv6 IDs use `[address]:port`. |
| `host`, `port` | required | Server hostname/IP and TCP port, 1–65535. |
| `enabled` | `true` | Maintain this connection when the master is ready. |
| `acceptControlCommands` | `false` | Interpret incoming frames as C-Play JSON commands. Leave off for device replies. |
| `autoReconnect` | `true` | Retry failed connections and remote disconnects. |
| `connectTimeoutMs` | `5000` | Timeout for hostname lookup/connection, and graceful disconnect. |
| `retryInitialMs`, `retryMaximumMs` | `1000`, `30000` | Exponential backoff, capped at the maximum. |
| `stableConnectionMs` | `10000` | Connected time before retry backoff resets. |
| `framing` | `0` | `0`: byte delimiter; `1`: 32-bit big-endian payload length. |
| `receiveTerminatorHex` | `0a` | `0a` for LF, `0d0a` for CRLF, `00` for NUL, or custom byte pairs. |
| `sendTerminatorHex` | `0a` | Terminator appended to outgoing framed messages; may be empty. |
| `maximumMessageBytes` | `1048576` | Maximum payload size; oversized incoming messages close the connection. |
| `maximumPendingWriteBytes` | `1048576` | Limit for outgoing bytes queued in Qt. |

Terminators accept complete hexadecimal byte pairs without spaces, up to 256 bytes. Receive delimiters must be nonempty. Length-prefix mode ignores terminators. Both buffer limits must be between 1 byte and 64 MiB. The maximum retry interval must be at least the initial interval.

Each command has `id`, `name`, `serverId`, `encoding` (`text` or `hex`), `payload`, and `raw`. With `raw: false`, the server's framing is applied. **Exact wire bytes** (`raw: true`) sends the payload without adding a delimiter or length prefix. Hexadecimal payloads contain complete byte pairs without spaces; use them for control bytes, binary commands, or a packet that already includes its framing.

A TCP layer's status is **not triggered**, **dispatching**, **accepted by transport**, or **rejected**. Acceptance means Qt accepted the bytes for sending; it does not acknowledge execution by the device. Commands attempted while disconnected are rejected and never queued for replay.

### Commands and framing

When **Receive C-Play control commands** is enabled for a server, each incoming frame contains one UTF-8 JSON object with an `operation` string and an optional `parameter` string. Numeric and boolean parameters are strings. Unknown operations, invalid parameter types, extra object fields, invalid UTF-8, and malformed JSON are rejected and reported in the settings page.

With the default LF framing, send these lines from the server to C-Play, ending each line with an actual LF byte:

```text
{"operation":"Play"}
{"operation":"SetVolume","parameter":"75"}
{"operation":"Seek","parameter":"-10"}
{"operation":"LoadFromSlides","parameter":"Opening"}
```

TCP reads may split or combine messages; the client preserves incomplete frames and processes all complete frames in order. Delimiters are removed without trimming payload whitespace. In length-prefix mode, prepend four bytes containing the unsigned payload byte length in network byte order; the prefix excludes its own four bytes. This mode also supports binary payloads containing arbitrary delimiter bytes.

The C-Play command adapter supports these existing control operations:

| Operations | Parameter |
| --- | --- |
| `Play`, `Pause`, `Stop`, `Rewind`, `FadeVolumeDown`, `FadeVolumeUp`, `FadeImageDown`, `FadeImageUp`, `OrientationAndSpinReset`, `RunSurfaceTransition` | Omitted or empty. |
| `Seek` | Signed integer seconds relative to current position. |
| `SetPosition` | Nonnegative position in seconds. |
| `SetSpeed` | Positive finite playback factor. |
| `SetVolume` | Integer 0–100. |
| `SetBackgroundVisibility`, `SetForegroundVisibility`, `SetNodeWindowsOpacity` | Finite value 0–1. |
| `SetSyncVolumeVisibilityFading`, `SpinPitchUp`, `SpinPitchDown`, `SpinYawLeft`, `SpinYawRight`, `SpinRollCW`, `SpinRollCCW` | `true`, `false`, `1`, or `0`. |
| `LoadFromAudioTracks`, `LoadFromPlaylist`, `LoadFromSections`, `LoadFromSlides` | Zero-based index or exact existing name; playlist selection also accepts its list title or filename. |

The TCP adapter uses `PlayerController::DispatchControlOperation()`. Control operations do not automatically acknowledge execution. The event protocol below provides queries and change subscriptions for the supported topics.

### Change notifications and automatic commands

In **TCP control**, a server's **Send change notifications** field selects comma-separated topics, for example `triggered_slide, layers, playlist`. C-Play sends a framed UTF-8 JSON notification to that server when a selected value changes. Leave the field empty to disable notifications, or use `*` for all supported topics. This works independently of **Receive C-Play control commands**.

A reusable command's **Trigger on changes** field selects topics that automatically send that command's payload (with its current value when `eventPayload` is `value`) to its configured server. For example, set it to `triggered_slide` to send `task_start RefreshSlides` whenever a slide is triggered. Leave it empty for manual/layer triggering only. Each matching topic change triggers the command once; selecting multiple topics can send the command multiple times during one player update. Changes to `layers` include volume, visibility and plane settings, so fades can trigger repeated notifications/commands.

Both server and command definitions persist these selections in an optional `eventTopics` array; existing version-1 configurations remain valid. Add this field inside the relevant server or command object:

```json
{
  "eventTopics": [
    "triggered_slide",
    "layers",
    "playlist"
  ]
}
```

Notifications use the server's delimiter or length-prefix framing:

```json
{"type":"change","topic":"playing_in_slides","value":2,"previous":1,"sequence":"42","timestamp":"2026-10-08T12:00:00.000Z"}
```

For a complete topic list, call HTTP `GET /event_topics` or use the list in TCP settings. Topic names match API endpoints without the leading `/`, with additional `pause`, `mute` and `triggered_slide` topics. `playing_in_slides` follows the selected slide, as the existing HTTP endpoint does; `triggered_slide` follows the slide actually triggered. Slide index `-1` denotes the master slide.

Changes are sampled on the ready master's Qt thread every 100 ms, regardless of whether they come from the UI, playback, a timeline, or remote control. Rapid changes are coalesced to the latest sampled state. Startup establishes a baseline without triggering commands. Notifications and automatic commands require an enabled, connected server; they are never queued or replayed after reconnect.

#### Medialon event variables and EventModel

`data/EventModel.json` is the shared definition for all 26 event commands and their Medialon monitoring variables. New installations receive these commands through `data/predefined-tcp-control.json`. For an existing project, select a saved server in **TCP control**, enter the EventModel JSON path, and click **Import EventModel**. Import merges commands into that server, keeps unrelated commands, and uses stable IDs; importing again updates the same entries. Configure the server for delimiter framing with `sendTerminatorHex: "0d"` (CR), then enable it. Frames end at CR so that the LF row separators in formatted lists stay inside the value; a server still using `0d0a` makes the driver drop list frames. Server JSON notifications can remain disabled because the imported commands send the monitoring frames themselves.

Generate the matching driver from the same model:

```sh
python help/http_server/generate_control_events.py
# Optional: --model path/to/EventModel.json --port 9000 --output path/to/receiver.mllc
```

The ready-to-use driver is `help/http_server/CPlay_Control_Events.mllc`. Install it as a Medialon LLC TCP/IP Server device. It receives unsolicited CR-terminated frames, updates variables immediately, and makes no polling requests. The existing `Showmaster_TCP_Server.mllc` remains the separate task-start/task-stop receiver; it still expects CRLF, so send to it through its own C-Play server profile with `sendTerminatorHex: "0d0a"`.

| Topics | Medialon variable type and contents |
| --- | --- |
| `audiotracks`, `playlist`, `sections`, `slides`, `layers` | String `CPlay_<topic>`: actual list values, formatted using the command's `charsPerItem`. |
| `pause`, `auto_play`, `mute`, `sync_image_volume_fade` | Integer `CPlay_<topic>`: current boolean as 0/1. |
| `volume`, `visibility`, stereo/grid/EOF/loop modes, playing indices, `triggered_slide` | Integer `CPlay_<topic>`: current value, including negative indices. |
| `position`, `remaining`, `duration`, `speed` | String `CPlay_<topic>`: decimal value preserving fractional precision. LLC has no floating-point variable type. |
| `media_title`, `slide_name` | String `CPlay_<topic>`: current UI text; line breaks become spaces. |

For example, volume changes send `cplay volume 75`, followed by CR. Playlist changes send the list itself to `CPlay_playlist`, rather than a change counter. All 26 event commands use `eventPayload: "value"`.

Reusable commands persist `eventPayload`, defaulting to `fixed` for manual device/task commands. `value` appends the current value to the saved payload prefix and requires text encoding and exactly one specific `eventTopics` entry. Automatic dispatch uses the event's value; manually triggering the command reads the current cached value, including lists. Older saved `change` commands migrate to `value` on load, with `charsPerItem: 40` unless explicitly set. Reimport the updated EventModel and install the regenerated LLC driver to use the new collection variable names and types.

`charsPerItem` is a saved per-command integer from 0 to 4096:

- `0` sends arrays and objects as full compact JSON, retaining their fields. Scalars are unaffected: booleans send 0/1, numbers preserve fractional precision, and strings send their text with CR/LF replaced by spaces.
- A positive width sends `playlist`, `sections`, `audiotracks` and `slides` exactly as the REST `/playlist`, `/sections`, `/audiotracks` and `/slides` endpoints return them with the same `charsPerItem`: numbered rows separated by LF, playlist and section rows ending with their duration, and the slide list excluding the master slide. `layers` is sent as the REST `/layers` endpoint returns it for the currently triggered slide, or for the master slide when no slide is triggered; it is also resent whenever `triggered_slide` changes. Any other list sends numbered labels separated by LF, each exactly that many Unicode characters, padded with spaces or truncated with `... ` when the width allows. Their labels prefer `listTitle`, `shortText`, `title`, `name`, `text`, `language`, `video`, then `path`; rows without these fields use their JSON representation. Use a send terminator without LF (the Medialon driver expects CR, `0d`) when lists contain LF row separators.
- The packaged list commands use `40`; other event commands use `0`. Fixed-width labels intentionally limit list details; use width `0` or the existing REST list/JSON endpoints when full detail is needed. The setting is available in the command editor and through REST, and persists across restarts.

Startup and reconnect do not replay events. For initialization, connect and request a full update through [TCP REST control](api.md#tcp-connection-and-update-control). Full updates read one cached snapshot (sampled every 100 ms) and dispatch each event command on the selected server once. Manual-only task and device commands are excluded.

Each EventModel command also supplies `monitoringVariable` and `monitoringType` for the generator. Keep the model, generated driver, and predefined commands aligned when customizing the protocol. Existing saved TCP configurations take precedence over predefined files and require an explicit import.

#### Subscribe and query over TCP

When **Receive C-Play control commands** is enabled, the connected server can send these additional JSON frames:

```text
{"operation":"Get","topics":["volume","playing_in_slides","playlist"]}
{"operation":"Subscribe","topics":["triggered_slide","layers"]}
{"operation":"Unsubscribe","topics":["*"]}
```

`Get` returns a snapshot without subscribing. `Subscribe` replaces that connection's runtime topic selection, immediately returns a snapshot, and then sends matching change notifications. `Unsubscribe` removes listed runtime topics; `*` clears the runtime subscription. Runtime subscriptions are cleared on disconnect; subscribe again after reconnect. Saved server notification topics remain active independently of runtime subscriptions.

Snapshots contain `type: "snapshot"`, a string `sequence`, a `values` object keyed by topic, an empty `events` array, and `reset: false`. Unsupported topics or malformed event requests return `type: "error"`. Numeric/boolean values in event responses are native JSON values, unlike the string parameters of control operations.

`slides` contains slide indices, names and paths. `layers` contains all layers across slides (including master), with slide/index/id, title/path/type, volume, visibility (0–1), stereo/grid modes and plane settings. `playlist` uses the existing playlist JSON representation; `audiotracks` and `sections` are arrays of model rows. These structured values are intended to refresh a controller's state rather than reproduce the API's formatted text lists.

HTTP long polling and optional WebSocket subscriptions use the same feed; see [API change subscriptions](api.md#change-subscriptions).

## TCP Control Server

The incoming listener accepts REST-style actions from Medialon or another TCP client. It dispatches directly to the same action handlers as HTTP; enabling the HTTP listener is optional. The listener does not use the outgoing client's `operation` / `parameter` protocol.

### Enable the listener

Enable **Incoming TCP control → Enable TCP server on master**, choose a numeric IPv4 or IPv6 bind address and port (default `0.0.0.0:7008`), then click **Apply listener**. These settings persist in `cplay.conf`. Only a ready master listens; disabling the server, changing role, or quitting closes its clients. HTTP may remain disabled. Use a bind address reachable from Medialon; the listener shares the REST API's trusted-network access model.

### Medialon client driver

Generate the Medialon client driver:

```powershell
python help/http_server/generate_control_client.py --address 192.168.1.10 --port 7008
```

Import `help/http_server/CPlay_Control_Client.mllc` into Medialon's Low Level Communicator drivers and create a device using **C-Play TCP Control Client**. Set the address to the C-Play master. The checked-in driver defaults to `127.0.0.1:7008`. It uses TCP/IP Client mode with auto reconnection, and exposes one command for each REST action. The existing **C-Play Control Events** driver remains the separate Medialon server for outgoing state updates.

The shared `data/TcpControlActions.json` catalog defines all 43 actions and is embedded in C-Play. The generator uses this same catalog. Commands include playback, seeking, auto-play, speed, volume, fades, background/foreground visibility, view mode, list selections, spin, orientation reset, surface transition, slide navigation/reset/selection, layer volume/visibility/fade up/fade down/plane properties, quitting, and outgoing TCP connect/disconnect/trigger/command-width changes. Information-only endpoints are unavailable, including REST POST endpoints that only retrieve values.

### Command format and replies

The wire format is one UTF-8 command per LF or CRLF terminated line. Send an actual newline after each request; there is no HTTP version, header block or request body on this TCP connection. Medialon sends request lines:

```text
POST /play
POST /volume?level=65
POST /load_from_playlist?index=2
POST /layer_visibility?slide_idx=0&layer_idx=1&value=50
```

Use the REST parameter names, units and zero-based indices. String parameters in Medialon must be URL percent encoded when they contain special characters: space `%20`, ampersand `%26`, hash `%23`, percent `%25`, and UTF-8 bytes for non-ASCII text. A plus sign is literal. The generated layer commands use slide/layer indices; custom commands can use `slide_name`, `layer_title`, `sub_layer_idx`, or `sub_layer_title` as the REST API does. Omitting the slide selector targets the Master slide. Layer plane commands can set any subset of the plane properties listed in the [HTTP layer control reference](api.md#layer-control).

`/layer_plane` also accepts `circular`, `centreDegrees` and `widthDegrees` for [circular placement](api.md#layer-control), for example `POST /layer_plane?slide_idx=0&layer_idx=0&circular=1&centreDegrees=355&widthDegrees=20`. The shared circular surface must be enabled in Grid Settings. Regenerate the Medialon client driver to expose these additional optional fields; empty degree fields leave their saved values unchanged.

Other TCP clients can send JSON instead, with string parameter values:

```text
{"path":"/volume","parameters":{"level":"65"}}
{"path":"/layer_visibility","parameters":{"layer_title":"Logo","value":"50"}}
```

Each command receives a newline-delimited JSON reply, for example `{"accepted":true,"status":200,"message":"Play"}`. `accepted` reports dispatch to the REST handler, not completion of playback or a transition. Legacy REST validation messages are preserved; read the message for an invalid value or missing target. Malformed requests, unsupported paths, and missing mutation parameters return `accepted:false` without dispatch. The Medialon driver sends commands without waiting for replies and does not replay commands after reconnect.

JSON parameters must be strings, including numbers and booleans. Repeated URL query parameter names are rejected. Setters require their mutation parameter (for example, `/volume` requires `level`); `/volume` alone is rejected as an information query. State queries and subscriptions remain available through the [event API](api.md#change-subscriptions) or the outgoing client's [subscription protocol](#subscribe-and-query-over-tcp).

The listener supports up to 16 clients, a 64 KiB limit per command, and bounded reply buffers. Oversized commands and clients that do not consume replies are disconnected. Partial messages and multiple commands in one TCP packet are supported.

## Multiple connections from C++

`TcpControlManager` owns the transports and exposes `servers`, `commands`, and per-server `messageReceived(serverId, payload)`, `connectionStateChanged(serverId, state)`, and `serverError(serverId, description)` signals. `controlMessageReceived` is emitted only for profiles that opt into incoming C-Play commands. `saveServer()` returns the endpoint-derived server ID; `saveCommand()` and `triggerCommand(commandId)` use stable command IDs. `setActive()` starts or stops enabled connections; the C-Play integration activates it only on the ready master.

`TcpLayer` dispatches through this manager on the application's Qt thread. Stopping or removing a layer does not tear down a shared server connection. The presentation and cluster serialization retain its command ID.

## Reusing the transport from C++

`TcpControlClient` is independent of the JSON command adapter. Run it on a Qt event-loop thread and use its Qt signals to integrate another protocol:

```cpp
auto *client = new TcpControlClient(owner);
TcpControlClient::Options options;
options.host = QStringLiteral("127.0.0.1");
options.port = 7008;
options.receiveTerminator = QByteArray::fromHex("0d0a");
options.sendTerminator = options.receiveTerminator;
if (client->configure(options)) {
    QObject::connect(client, &TcpControlClient::messageReceived,
                     receiver, &Receiver::handleMessage);
    client->start();
}
```

`sendText()` encodes UTF-8 and applies the configured framing. `sendMessage()` frames a `QByteArray`. `sendBinary()` sends exact wire bytes with no added terminator or prefix. All return whether Qt accepted the outgoing bytes. The `bytesWritten` signal reports transport progress, not acknowledgement by the receiving application.

`messageReceived` exposes every complete payload as bytes. `textCommandReceived` additionally emits payloads that are valid UTF-8. `connectionStateChanged`, `connectedChanged`, `reconnectScheduled`, and `errorOccurred` expose lifecycle events; `stateText` and `lastError` provide displayable status.

Incomplete frames and pending writes are discarded when a connection fails. Commands are never replayed automatically after reconnection because the peer may already have executed them. Use an application-level acknowledgement protocol if delivery confirmation is required. The client uses plain TCP; authentication and encryption require a separate protocol or secure network transport.

## Tests

The event feed, TCP notification/command, runtime subscription/reconnection and WebSocket tests can be built independently of C-Play's rendering dependencies:

```sh
cmake -S tests -B build/tcp-tests -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build/tcp-tests --config Release
ctest --test-dir build/tcp-tests -C Release --output-on-failure
```

Qt Core, Network, WebSockets and Test, and their runtime libraries, must be available to the test executable.
