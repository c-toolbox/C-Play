---
title: TCP control settings
sidebar_position: 9
---

# TCP control settings (C-Play v2.4 and newer)

![TCP control settings](/assets/ui/settings/tcp_control.png)

The TCP control settings configure C-Play's incoming TCP Control Server and its outgoing TCP Control Client connections. Only the master opens connections or listens; cluster nodes ignore these settings. For protocols, framing, event topics and Medialon examples, see [TCP control](/remote-control/tcp).

### Incoming TCP control

* **Enable TCP server on master** — Accept REST-style actions from a TCP client such as Medialon, see [TCP Control Server](/remote-control/tcp#tcp-control-server).
* **Bind address** / **Port** — Numeric IPv4 or IPv6 address and port to listen on (default `0.0.0.0:7008`). Click **Apply listener** to apply them.

The line below shows the listener state and the number of connected clients. These settings are saved in `cplay.conf`.

### Incoming client subscriptions

Clients of the TCP server can subscribe to state updates with `/subscribe`, `/unsubscribe` and `/full_update`. The list shows each connected client with its address, profile and subscribed topics. A profile selected by the client's address defines what the client receives. See [Subscriptions and incoming client profiles](/remote-control/tcp#subscriptions-and-incoming-client-profiles).

* **Name** / **Client address** — Label and numeric IP address of the client. The address is the profile's ID. The **default profile** has address `*` and is used for every client without an enabled profile of its own; its address cannot be changed.
* **Enabled** — Use this profile for matching clients; clients of a disabled profile use the default profile. Always on for the default profile.
* **Subscribe on connect** — Subscribe the client to the checked topics when it connects; when off, the client waits for `/subscribe`. On by default with all topics checked. Uncheck topics to leave them out, or use **Check all** / **Uncheck all**. Topics added in later versions are included. Saving updates the subscription of connected clients.
* **Update on connect** — Send the current values of the subscribed topics when the client connects (default on). Turn off for clients that disconnect and reconnect often; they then only receive changes and request the current values with `/full_update`.
* **Update format** — *JSON notifications*, or *Event commands of a TCP server*: the client receives that server's event commands (for example `cplay volume 75`) exactly as the server would.
* **Command structure from** — The server whose event commands are used. It may be a disabled server that only defines the structure.
* **Send terminator (hex)** — Terminator of every reply and update sent to the client: `0a` (LF) by default, `0d` (CR) for the Medialon full-duplex driver.
* **Replies** — Reply to every action with its JSON result (default on). Turn off for the Medialon full-duplex driver: replies arriving between updates can briefly set its monitoring variables, such as the volume, to 0.

Use **Save client profile** and **Delete client profile** to manage the selected profile. The default profile cannot be deleted, only edited. Changes apply immediately to connected clients. Profiles are saved in `tcp-control.json`.

### Persistent TCP servers

![Persistent TCP servers](/assets/ui/settings/tcp_control_servers.png)

Each enabled server keeps its own outgoing connection on the master. Select a server in the list, or click **New server**, then edit:

* **Name** / **Host** / **Port** — Label and endpoint of the server. The endpoint (`host:port`) is the server's ID.
* **Enabled** — Connect to this server (keeps it disconnected across restarts when off).
* **Connect on startup** — Connect when C-Play starts (default on). When off, the server stays disconnected until you click **Connect**, or connect it via REST or TCP control. Only available when the server is enabled.
* **Receive C-Play control commands** — Parse incoming JSON `operation` / `parameter` commands from this server.
* **Send change notifications** — Comma-separated event topics sent to this server when they change; `*` for all, empty disables notifications.
* **Automatic reconnect** — Reconnect after a lost connection, using the retry delays below.
* **Framing** — *Delimiter* (with the receive and send terminators below) or a 4-byte length prefix.
* **Receive terminator (hex)** / **Send terminator (hex)** — Delimiter bytes in hexadecimal, for example `0a` (LF) or `0d0a` (CRLF).
* **Connection timeout**, **Initial retry delay**, **Maximum retry delay**, **Reset backoff after** (ms) — Connection timeout and reconnect backoff.
* **Maximum message** / **Maximum pending writes** (bytes) — Buffer limits for received frames and queued outgoing data.

Use **Save server**, **Delete server**, **Connect**, and **Disconnect** to manage the selected server. See [Servers and reusable commands](/remote-control/tcp#servers-and-reusable-commands).

### Reusable TCP commands

![Reusable TCP commands](/assets/ui/settings/tcp_control_commands.png)

Commands defined here are triggered by [TCP layers](/media/layers#tcp) in presentations, manually, or automatically on changes.

* **EventModel path** / **Import EventModel** — Import Medialon event commands into the selected saved server.
* **Name** / **Server** — Command label and the server it is sent to.
* **Payload** — *UTF-8 text* or hexadecimal bytes.
* **Exact wire bytes** — Send the payload without adding framing or termination.
* **Trigger on changes** — Event topics that send this command automatically; empty = manual or layer triggering only.
* **Event payload** — *Fixed payload*, or include the changed value.
* **Characters per list item** — Pads or shortens each list label in value payloads to this width; 0 sends full lists.
* **Time format** — Format of the time value appended to the payload, for example `hh:mm:ss` or `hh:mm:ss/zz` (at most 64 characters). Empty sends seconds. Only for commands that append the current value and trigger on *position*, *remaining* or *duration*. With a coarse format, *position* commands are only sent when the formatted text changes.
* **Command text** — The payload to send.

Use **Save command**, **Delete command**, and **Trigger saved command** to manage and test the selected command. The bottom of the page shows the last command result, connection errors, and the path of the saved configuration (`tcp-control.json`). See [Configuration and persistence](/remote-control/tcp#configuration-and-persistence).
