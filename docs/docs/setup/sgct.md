---
title: Cluster configuration
sidebar_position: 3
---

# Cluster configuration

The backend that enables management and cluster rendering is handled by a library called SGCT (Simple Graphics Cluster Toolkit). It handles communication between master and nodes in a cluster setup, as well as the setup of windows and viewports based on configuration files.
The documentation can be found here:

 - [SGCT Docs](https://sgct.github.io)

If you are an installer or user of C-Play, you primarily need to be familiar with the configuration file syntax, which can be found here:

- [SGCT Configuration Files](https://sgct.readthedocs.io/en/latest/users/configuration/index.html)

## Example configurations in C-Play

C-Play ships with example cluster configurations, located in the [help/sgct](https://github.com/c-toolbox/C-Play/tree/master/help/sgct) folder of the repository. When C-Play is installed, they are copied to the same folder as *C-Play.exe*. Start from the example closest to your setup rather than from an empty file, as the examples already contain the settings C-Play needs, including the `datatransferport` of each node (see [Communication between master and nodes](#communication-between-master-and-nodes)).

| Configuration | Node window |
|:---|:---|
| `master_and_node_simple.json` | A 16:9 projection plane. |
| `master_and_node_wide.json` | A wide projection plane. |
| `master_and_node_fisheye.json` | A fisheye (fulldome) projection. |
| `master_and_auto_2D3D_node.json` | Separate viewports for 3D and 2D content, see [Auto 2D vs 3D switch](#auto-2d-vs-3d-switch). |

In every example, node 0 is the master, with its window at `127.0.0.1`, and node 1 is a node at `127.0.0.2`. Both run on the same computer, so you can try a cluster setup without a second computer. Each example has a matching batch file, such as `C-Play-master-and-node-simple.bat`, that starts both:

```
start C-Play.exe --config master_and_node_simple.json --local 0
start C-Play.exe --config master_and_node_simple.json --local 1 --client
```

In a real installation, set `masteraddress` and the `address` of every node to the IP addresses of the computers. Each computer then starts C-Play with only `--config <file>`, and SGCT finds out which node it is from its IP address. All computers must use the same configuration file.

## Communication between master and nodes

Every node in the configuration has two ports, `port` and `datatransferport`, and the master and the nodes need both of them to work together:

* **`port`** — The sync connection. The master sends the synchronized state to the node every frame: playback position, layers, visibility and so on. This is what keeps the nodes showing the same content as the master.
* **`datatransferport`** — The data transfer connection, which goes the other way. The nodes report back to the master on what is going on in each of them, so the pilot at the master sees the state of the complete system, not only of the master:
  * **Loading on the nodes** — When a file is loaded, each node reports when it has the file open, or that it could not open it. The load notice in the master view shows the progress (*"Loading on nodes: 1 of 2 ready"*, *"Loaded on all 2 nodes"* or *"Failed to load on 1 of 2 nodes"*), and the master holds playback until all nodes are ready, so that playback starts on all of them at once. See [Starting playback on all nodes](/remote-control/api#starting-playback-on-all-nodes).
  * **Node load failures** — Files that a node could not open or decode are listed in **Settings → Logging...** on the master.
  * **Node telemetry** — When enabled in **Settings → Logging...**, every node reports its FPS, dropped frames, sync offset, GPU load (render and hardware video decode shown separately, with a per-engine breakdown in the tooltip), decoder state and latency once per second.
    * **Measurement over time** — Below the telemetry table, **Start** and **Stop** record the reports into one summary row per node: average and peak GPU render and decode load, average and lowest FPS while a video plays, total dropped frames and the largest sync offset. With **Start/stop with playback**, play starts a new measurement and pause, stop/rewind or the end of the file stops it, so after playing a video to its end the table shows how hard each node's GPU worked. The **Timeline** tab graphs the render and video decode load of every node over the measurement, with a shared crosshair that reads out all nodes at the time under the pointer. **Export CSV** writes every recorded report to `data/log/node_measurement_<date>_<time>.csv` on the master.
  * **Capture inputs** — The nodes report the capture inputs they have, so a capture layer can be assigned to an input on a specific node.
  * **Shutting down** — When C-Play is closed on the master, it asks the nodes to close over the data transfer connection and waits for each of them to report that it has closed. A notice in the master view shows the progress (*"2 of 3 closed, waiting for: ..."*), and the master closes shortly after the last node. A node that does not answer within 15 seconds does not block the master from closing. A node that closes on its own, or whose connection is lost, is reported in the master view.

Without the data transfer connection, the nodes still show the synchronized content, but the master knows nothing about them: playback is not held while nodes load, and failures and problems on the nodes are not shown to the pilot.

### Example: a master and two nodes

An installation has three computers: the master, where the pilot works, at `192.168.1.10`, and two computers that drive projectors at `192.168.1.11` and `192.168.1.12`. The configuration for it looks like this. The `users` and `windows` are left out, since they do not affect the ports:

```json
{
  "version": 1,
  "masteraddress": "192.168.1.10",
  "nodes": [
    {
      "address": "192.168.1.10",
      "port": 20401,
      "datatransferport": 21401,
      "windows": [ ... ]
    },
    {
      "address": "192.168.1.11",
      "port": 20402,
      "datatransferport": 21402,
      "windows": [ ... ]
    },
    {
      "address": "192.168.1.12",
      "port": 20403,
      "datatransferport": 21403,
      "windows": [ ... ]
    }
  ]
}
```

Every node opens two TCP connections to `masteraddress`, one on its own `port` and one on its own `datatransferport`. The master listens on all four ports:

| Computer | Connects to | Connection | Direction of the data |
|:---|:---|:---|:---|
| Node `192.168.1.11` | `192.168.1.10:20402` | Sync | Master → node |
| Node `192.168.1.11` | `192.168.1.10:21402` | Data transfer | Node → master |
| Node `192.168.1.12` | `192.168.1.10:20403` | Sync | Master → node |
| Node `192.168.1.12` | `192.168.1.10:21403` | Data transfer | Node → master |

The master does not connect to itself, so its own `port` and `datatransferport` (`20401` and `21401`) are not used. Set them anyway: `port` is required for every node, and the master entry then looks like the others.

In this example, the firewall on the master has to allow incoming TCP connections on ports `20402`, `20403`, `21402` and `21403`. On Windows, you can add the rule in a PowerShell started as administrator on the master:

```powershell
New-NetFirewallRule -DisplayName "C-Play cluster" -Direction Inbound -Protocol TCP -LocalPort 20402,20403,21402,21403 -Action Allow
```

The nodes only make outgoing connections, so their firewalls normally need no new rule.

To add a third node, give it the next free ports, such as `"port": 20404` and `"datatransferport": 21404`, and open both of them in the firewall on the master.

### Rules for the ports

* Every `port` and `datatransferport` must be unique within the configuration. In particular, the `datatransferport` of one node must not be the `port` of another. In the example above, `"datatransferport": 20403` for node `192.168.1.11` would clash with the sync port of node `192.168.1.12`.
* When a node has no `datatransferport`, C-Play uses its `port` + 1000. That is why the example uses the `20400` range for `port` and the `21400` range for `datatransferport`. Still, set it explicitly for every node so that all ports to open in the firewall can be read from the configuration.
* A node with `"datatransferport": 0` has no data transfer connection.
* If the firewall allows `port` but blocks `datatransferport`, the nodes still show the synchronized content, but the master gets no reports from them, as described above. If loading on nodes or node telemetry never shows anything, check the `datatransferport` first.

## Auto 2D vs 3D switch

This is based on the assumption that you do not mix 3D viewports with 2D viewports, meaning a window could cover a 3D projector or screen that would show left and right eye, or a 2D projector or screen, but a window would not cover both at the same time.

As such, C-Play will check what kinds of viewports are available, and in the special case where it finds both 3D and 2D viewports, it will enable or disable them based on the content and application settings currently loaded.

Basically, in the configuration below, there are two viewports for 3D content, where the eye is either "left" or "right", and there are two viewports for 2D content only, which have the eye set to "center".

C-Play would make sure that only the FIRST two viewports below, or the LAST two viewports, are enabled, but not all four at once.

This makes it possible to apply different calibrations for 3D and 2D content, which in some scenarios can make a noticeable difference and may be necessary to show both 3D and 2D content correctly.

### JSON: Version 2.2 and newer

The following is a window's `viewports` fragment; include it in a complete SGCT cluster configuration. The example configuration `master_and_auto_2D3D_node.json` contains a complete configuration with these viewports.

```json
{
  "viewports": [
    {
      "eye": "left",
      "pos": {
        "x": 0,
        "y": 0
      },
      "size": {
        "x": 0.5,
        "y": 1
      },
      "projection": {
        "type": "PlanarProjection"
      }
    },
    {
      "eye": "right",
      "pos": {
        "x": 0.5,
        "y": 0
      },
      "size": {
        "x": 0.5,
        "y": 1
      },
      "projection": {
        "type": "PlanarProjection"
      }
    },
    {
      "eye": "center",
      "pos": {
        "x": 0,
        "y": 0
      },
      "size": {
        "x": 0.5,
        "y": 1
      },
      "projection": {
        "type": "PlanarProjection"
      }
    },
    {
      "eye": "center",
      "pos": {
        "x": 0.5,
        "y": 0
      },
      "size": {
        "x": 0.5,
        "y": 1
      },
      "projection": {
        "type": "PlanarProjection"
      }
    }
  ]
}
```

### XML: Version 2.1 and older

This legacy format is not supported in C-Play v2.2 and newer.

```xml
<Viewport eye="left">
    <Pos x="0.0" y="0.0" />
    <Size x="0.5" y="1.0" />
    <!--  Left VP (when 3D) -->
    <PlanarProjection />
</Viewport>
<Viewport eye="right">
    <Pos x="0.5" y="0.0" />
    <Size x="0.5" y="1.0" />
    <!-- Right VP(when 3D) -->
    <PlanarProjection />
</Viewport>
<Viewport eye="center">
    <Pos x="0.0" y="0.0" />
    <Size x="0.5" y="1.0" />
    <!-- Left VP (when 2D) -->
    <PlanarProjection />
</Viewport>
<Viewport eye="center">
    <Pos x="0.5" y="0.0" />
    <Size x="0.5" y="1.0" />
    <!-- Right VP (when 2D) -->
    <PlanarProjection />
</Viewport>
```

Bear in mind that you can also run an "active 3D" configuration, or any configuration where left and right eye have the same viewport parameters. In most cases this works correctly. Then C-Play will show both 3D and 2D correctly without additional viewports, as this is built into C-Play to adapt the rendering based on mono versus stereoscopic material.