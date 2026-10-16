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

### Persistent TCP servers

Each enabled server keeps its own outgoing connection on the master. Select a server in the list, or click **New server**, then edit:

* **Name** / **Host** / **Port** — Label and endpoint of the server. The endpoint (`host:port`) is the server's ID.
* **Enabled** — Connect to this server (keeps it disconnected across restarts when off).
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
* **Command text** — The payload to send.

Use **Save command**, **Delete command**, and **Trigger saved command** to manage and test the selected command. The bottom of the page shows the last command result, connection errors, and the path of the saved configuration (`tcp-control.json`). See [Configuration and persistence](/remote-control/tcp#configuration-and-persistence).
