---
title: C-Play -> NDI output
sidebar_position: 4
---

# C-Play -> NDI output (C-Play v2.4.0 and newer)

From **C-Play version 2.4.0**, C-Play can publish its own video as an NDI source, so that other applications on the network — OBS Studio, a control room monitor, another media player, etc. — can receive it like any other NDI feed. This is the reverse direction of [OBS Studio + NDI -> C-Play](/system-integration/obs-ndi), where C-Play receives an external NDI source as a layer.

NDI output requires a build with NDI support (the official builds include it). When the application is built without NDI, the header button and menu entry are hidden and the per-layer **NDI** buttons are disabled.

Two levels of output are available:

| Output | What is sent | Where you toggle it |
|--------|--------------|---------------------|
| **Main video** | The full player output (main media + composited layers) | Upload button (![](/assets/icons/kt-set-max-upload-speed-white.svg)) in the header taskbar, or the Settings menu |
| **Layers** | A single presentation layer, independently of the main output | Per-layer **NDI** button in the layer view |

Both outputs are **video only** for now and run on the **master** computer. The nodes are not affected by NDI output.

---

## Main video NDI output

The entire player output can be sent as an NDI source named `C-Play` (default).

1. In the header taskbar, click the **upload** button (![](/assets/icons/kt-set-max-upload-speed-white.svg)) to turn NDI output on or off. The same toggle is available from the **Settings** menu ("ON/OFF to send the player as an NDI source.").
2. The button icon turns ![](/assets/icons/kt-set-max-upload-speed-lime.svg) **lime** while the output is ON and ![](/assets/icons/kt-set-max-upload-speed-crimson.svg) **crimson** when it is OFF.
3. While sending, the tooltip shows the live sender name and resolution, for example: `NDI output "C-Play" is sending at 1920x1080.`

The source is published at the native resolution of the player texture and one frame is sent per presented frame, so the receiver sees exactly what C-Play shows.

---

## Layer NDI output

Each presentation layer can additionally be published as its **own** NDI source, independently of the main video output. This is useful when another application should receive a single feed — for example one camera or one OBS scene that C-Play displays as a layer — without the rest of the composition.

1. Open the layer view for the slide and select the layer you want to send.
2. In the layer's control row (next to the Grid, ROI, and visibility controls), click the **NDI** button to enable NDI output for that layer.
3. The layer is then published from the master as an NDI source with a generated name:

   ```text
   C-Play Layer <layer id> - <layer title>
   ```

4. The button icon shows the current state:

* ![](/assets/icons/kt-set-max-upload-speed-crimson.svg) — NDI output for this layer is OFF
* ![](/assets/icons/kt-set-max-upload-speed-orange.svg) — enabled, but no frame is being sent (for example when the slide with the layer is hidden)
* ![](/assets/icons/kt-set-max-upload-speed-lime.svg) — frames are actively being sent over NDI

Notes:

* The button tooltip shows `NDI output on master: <sender name>` while enabled, so you know exactly which source to pick in the receiving application.
* NDI output is only available for layers that render video; it is disabled for **Audio** layers.
* Output happens on the master only — the nodes are not affected by this setting.
* The setting (and the sender name) is saved with the presentation file, so a show can be reloaded and the same NDI sources come back automatically.

---

## Master-only layers ("Sync" button)

The layer view also has a **Sync** button next to the **NDI** button that controls whether a layer is synced from the master to the nodes:

* ![](/assets/icons/network-connect-lime.svg) Layer is synced to the nodes (default)
* ![](/assets/icons/network-disconnect-orange.svg) Layer exists on the master only, it is not synced to the nodes

This is useful for layers that should stay local to the master — for example a layer whose purpose is NDI output, or a control-only layer. The flag is saved with the presentation file like the NDI setting.

Master-only layers are hidden from the 3D view by default; this can be changed in **Settings -> User interface** under *Hide master-only layers in 3D view*. See [Window & UI](/settings/window_and_ui).

---

## Receiving C-Play's NDI output

Any application with an NDI receiver can discover and receive the sources, as long as it is on the same network as the C-Play master. The sender names to look for are `C-Play` (main video) and `C-Play Layer <id> - <title>` (layers).

### Example: OBS Studio

1. In OBS, add an **NDI Input** source (from the NDI plugin).
2. Pick `C-Play`, or a specific `C-Play Layer ...` sender, from the list of discovered sources.
3. The feed can now be used in any OBS scene — for streaming, recording, or compositing with other content.

If the source does not appear on the receiving machine:

* Make sure both machines are on the same network/subnet and that Windows Firewall is not blocking C-Play (or the receiver application).
* Check that the NDI runtime/plugin is installed and loaded in the receiving application.
* Verify that the output is actually ON in C-Play — the header button should be lime, and for a layer the **NDI** button icon should be lime (actively sending), not orange.

For the reverse direction — sending OBS content *into* C-Play as an NDI layer — see [OBS Studio + NDI -> C-Play](/system-integration/obs-ndi).