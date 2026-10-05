---
title: TCP control client
sidebar_position: 3
---

# TCP control client

C-Play's master can keep independent outgoing connections to multiple TCP servers. Configure them in **Settings → Configure → TCP control**, define reusable commands, and trigger those commands from **TCP** presentation layers. Connections start once the player's media and slide models are available. Cluster nodes do not open control connections.

## Servers and reusable commands

1. Click **New server**, enter its name, host and port, configure framing and reconnection, then click **Save server**. Repeat for every device or control system.
2. Click **New command**, give it a name, select its server, and enter a UTF-8 text or hexadecimal payload. Click **Save command**.
3. Use **Trigger saved command** to send the saved command immediately.
4. Add a layer with type **TCP** and select a saved command. It fires when the layer starts. Double-click the layer to trigger it manually, or use **Trigger** in its layer view. The layer view also lets you change the referenced command.

For example, define a **Projector** server using CRLF termination and a **Lighting** server using binary messages. Create **Projector on** (`PWR ON`, UTF-8 text) and **Lighting scene** (`020100ff03`, hexadecimal bytes, exact wire bytes). Place them in separate TCP layers on the appropriate slides. They reuse the same persistent connections throughout the presentation.

Every server has its own enabled flag, connection state, error, framing, buffers, timeout, and retry backoff. A reconnect on one server does not interrupt the others. **Disconnect** stops retries for that server until **Connect**, saving that server, or application reactivation. Turn its **Enabled** flag off and save to keep it disconnected across restarts.

Commands refer to servers by stable IDs; layers refer to commands by stable IDs. Renaming either keeps these references intact. Editing a saved command updates all layers that reference it. A server cannot be deleted while commands still reference it. Deleting a command leaves referencing layers with an explicit missing-command rejection; it never silently selects another command.

## Configuration and persistence

Profiles and commands are saved atomically in `C-Play/tcp-control.json` beneath Qt's generic configuration directory. The settings page displays the exact path. Copy this file with a presentation to another master to preserve its server and command references. Presentations store each TCP layer's `tcpCommandId`; they do not embed device endpoints or payloads.

The original single-server `[TcpControl]` configuration is imported once as **Legacy TCP control** when it is enabled and the multi-server configuration file does not exist. An existing file is never replaced by migration.

Server fields in the JSON file are:

| Field | Default | Meaning |
| --- | --- | --- |
| `id`, `name` | generated ID, required name | Stable identity and display name. |
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

## Commands and framing

When **Receive C-Play control commands** is enabled for a server, each incoming frame contains one UTF-8 JSON object with an `operation` string and an optional `parameter` string. Numeric and boolean parameters are strings. Unknown operations, invalid parameter types, extra object fields, invalid UTF-8, and malformed JSON are rejected and reported in the settings page.

With the default LF framing, send these lines from the server to C-Play, ending each line with an actual LF byte:

```json
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

The TCP adapter uses `PlayerController::DispatchControlOperation()`. It does not provide the HTTP API's full query/response endpoint set or automatically acknowledge commands.

## Multiple connections from C++

`TcpControlManager` owns the transports and exposes `servers`, `commands`, and per-server `messageReceived(serverId, payload)`, `connectionStateChanged(serverId, state)`, and `serverError(serverId, description)` signals. `controlMessageReceived` is emitted only for profiles that opt into incoming C-Play commands. `saveServer()`, `saveCommand()`, and `triggerCommand(commandId)` use stable IDs. `setActive()` starts or stops enabled connections; the C-Play integration activates it only on the ready master.

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

The transport and command adapter tests can be built independently of C-Play's rendering dependencies:

```sh
cmake -S tests -B build/tcp-tests -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build/tcp-tests --config Release
ctest --test-dir build/tcp-tests -C Release --output-on-failure
```

Alternatively, enable `CPLAY_BUILD_TCP_CONTROL_TESTS` when configuring the main project. Qt Test and the Qt runtime libraries must be available to the test executable.
