---
title: REST + C-Troll + OBS
layout: home
nav_order: 2
parent: System integration
---

# Launch apps and control things with C-Play

This page covers three topics:

1. [Control things with REST and WebSockets](#rest) - send commands from C-Play to other systems
2. [Launch OBS through REST + C-Troll](#launch) - start an OBS instance that is ready for WebSocket control
3. [OBS WebSocket examples](#obs-websocket-examples) - switch OBS profile, scene collection, and scene from C-Play

---

## REST in C-play<a name="rest"></a>

C-Play can be controlled by external systems through its [HTTP Web API](../remote/api), but C-Play can also be the controller. The **REST Commands Editor** and **REST layers** can send commands to other programs when a show starts, when a slide is triggered, or when you press **Trigger** in the editor.

Typical uses are:

* Start or stop another program through [C-Troll](https://github.com/c-toolbox/C-Troll).
* Switch scenes, profiles, or scene collections in OBS Studio.
* Fire a command to a projector, lighting controller, media server, or show-control system.
* Run a REST layer from a C-Play presentation so external state changes happen at the same time as a slide.

Open **Settings -> REST Commands Editor** to create reusable commands. Commands are saved in `data/predefined-rest-commands.json` and can be selected later when configuring a REST layer.

### Supported command types

REST commands support the normal HTTP methods `GET`, `POST`, `PUT`, and `DELETE`. They also support `WS` and `WSS` for WebSocket targets.

For HTTP commands, parameters are sent as form-style name/value pairs. For `GET` and `DELETE`, they are appended to the URL as query parameters. For `POST` and `PUT`, they are sent in the request body.

For WebSocket commands, the parameter list is converted to a JSON message. This is useful for simple WebSocket receivers, and C-Play also has special handling for OBS WebSocket v5.

### OBS WebSocket commands

OBS Studio 28 and newer includes obs-websocket by default. The default WebSocket URL is:

```text
ws://localhost:4455
```

When C-Play sees an OBS-style WebSocket command, it negotiates the `obswebsocket.json` subprotocol, identifies with OBS, and then sends an OBS request. In the REST Commands Editor, select method `WS`, enable **OBS: Connecting to OBS**, then choose one of the helper actions or enter a custom OBS request name.

The helper currently fills the fields for these common requests:

| Action | OBS request | Request data field |
|--------|-------------|--------------------|
| Set Profile | `SetCurrentProfile` | `profileName` |
| Set Scene | `SetCurrentProgramScene` | `sceneName` |
| Set Scene Collection | `SetCurrentSceneCollection` | `sceneCollectionName` |

If OBS WebSocket authentication is enabled, add a parameter named `password` with the OBS WebSocket password. C-Play will generate the authentication response during the OBS handshake. The helper can fetch OBS profiles, scenes, and scene collections when authentication is disabled; with authentication enabled, type the target name manually.

For other OBS requests, set **OBS request** to the request name from the [obs-websocket protocol documentation](https://github.com/obsproject/obs-websocket/blob/master/docs/generated/protocol.md), then add request data rows. Values such as `true`, `false`, numbers, JSON objects, and JSON arrays are converted to their JSON value types before C-Play sends the request.

---

## Launch OBS Portable through C-Troll<a name="launch"></a>

C-Troll can expose a small HTTP API that starts and stops configured programs. C-Play can call that API through a REST command, so a presentation can start OBS before switching it to the right profile, scene collection, and scene.

The default C-Troll examples in `data/predefined-rest-commands.json` use:

| Field | Value |
|-------|-------|
| URL | `http://localhost:7001/program/start` |
| Method | `GET` |
| `program` | `C-Play` |
| `cluster` | `LOCAL` |
| `configuration` | `Master` |

For OBS, create a matching C-Troll program entry that points to `obs64.exe` and set the working directory to the OBS `bin/64bit` folder. Then create a C-Play REST command with the OBS program/configuration names used by C-Troll.

Example command:

| Field | Value |
|-------|-------|
| Title | `Start OBS NDI through C-Troll` |
| URL | `http://localhost:7001/program/start` |
| Method | `GET` |
| Ignore Status | enabled |
| `program` | `OBS` |
| `cluster` | `LOCAL` |
| `configuration` | `NDI Browser` |

### Launch OBS with WebSocket support

OBS WebSocket can be configured in the OBS UI under **Tools -> WebSocket Server Settings**, but it can also be overridden from the command line. This is especially useful when C-Troll launches OBS because the port can be part of the launch configuration.

Example Windows launch parameters:

```bat
obs64.exe --portable --multi --collection "C-Play NDI" --profile "C-Play" --scene "Browser" --websocket_port 4455 --websocket_ipv4_only
```

Useful parameters for this workflow:

| Parameter | Purpose |
|-----------|---------|
| `--portable` or `-p` | Use the OBS directory as a portable OBS profile/configuration root. |
| `--multi` or `-m` | Allow more than one OBS instance without showing the multiple-instance warning. |
| `--collection "name"` | Start with a specific scene collection. |
| `--profile "name"` | Start with a specific profile. |
| `--scene "name"` | Start with a specific scene. |
| `--websocket_port 4455` | Override the OBS WebSocket port for this launch. |
| `--websocket_ipv4_only` | Force obs-websocket to bind IPv4 only. |
| `--websocket_password "password"` | Override the WebSocket password. Use carefully, because command-line passwords can be visible to other local tools. |

For the full OBS list, see the [OBS launch parameters](https://obsproject.com/kb/launch-parameters). For WebSocket-specific options, see the [obs-websocket README](https://github.com/obsproject/obs-websocket#using-obs-websocket).

### Multiple OBS instances

If you need multiple independent NDI outputs, the easiest setup is usually multiple OBS directories, each running in portable mode. Give each directory its own scene collection, output name, WebSocket port, and C-Troll configuration.

For example:

| OBS directory | WebSocket URL | NDI output |
|---------------|---------------|------------|
| `D:\OBS-Browser` | `ws://localhost:4455` | `OBS Browser` |
| `D:\OBS-VDO-Ninja` | `ws://localhost:4456` | `OBS VDO Ninja` |

This keeps the settings separate and avoids one OBS instance overwriting another instance's profile, scene collection, plugin, or WebSocket settings.

---

## OBS WebSocket examples<a name="obs-websocket-examples"></a>

### Switch OBS scene

| Field | Value |
|-------|-------|
| URL | `ws://localhost:4455` |
| Method | `WS` |
| OBS | `Connecting to OBS` enabled |
| OBS request | `SetCurrentProgramScene` |
| Request data | `sceneName = Browser` |

Stored in `data/predefined-rest-commands.json`, the same command looks like:

```json
{
	"enabled": true,
	"ignoreStatus": false,
	"method": "WS",
	"parameters": [
		{ "name": "requestType", "value": "SetCurrentProgramScene" },
		{ "name": "requestData.sceneName", "value": "Browser" }
	],
	"title": "OBS - Set scene Browser",
	"url": "ws://localhost:4455"
}
```

### Switch OBS profile

```json
{
	"enabled": true,
	"ignoreStatus": false,
	"method": "WS",
	"parameters": [
		{ "name": "requestType", "value": "SetCurrentProfile" },
		{ "name": "requestData.profileName", "value": "C-Play" }
	],
	"title": "OBS - Set profile C-Play",
	"url": "ws://localhost:4455"
}
```

### Switch OBS scene collection

```json
{
	"enabled": true,
	"ignoreStatus": false,
	"method": "WS",
	"parameters": [
		{ "name": "requestType", "value": "SetCurrentSceneCollection" },
		{ "name": "requestData.sceneCollectionName", "value": "C-Play NDI" }
	],
	"title": "OBS - Set scene collection C-Play NDI",
	"url": "ws://localhost:4455"
}
```

These commands can be triggered manually from the editor, selected as presets in a REST layer, or combined with a C-Troll launch command so C-Play starts OBS and then switches it into the right state.

