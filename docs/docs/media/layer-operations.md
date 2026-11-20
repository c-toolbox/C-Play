---
title: Layer operations
sidebar_position: 10
---

# Layer operations

Once a layer is added to a slide, you can position and crop it, apply post process effects to its image, and decide which machines in a cluster open its source. For the layer types and how to add them, see [Layers and sources](/media/layers). For slides, visibility, timelines, and entrance and exit effects, see [Build presentation](/media/cplaypres).

## Position, crop, and preview

After you have added a new layer, you can specify its parameters in more detail through the *"Inspect"* view. Here you control grid and stereo parameters, the volume level if applicable, and inspect how the output looks.

![Inspect view Video](/assets/ui/layers_view_video.png) &nbsp;&nbsp;&nbsp; ![Inspect view Grid Parameters](/assets/ui/layer_view_grid_parameters.png)

### Source file

For layers that load a file from disk (Image, Video, Audio, PDF and Multi-video), the top left corner of the Inspect view shows the file's path. The folder button next to it opens the file explorer with the file selected. Right-click the path to copy it.

If the file does not exist on this computer, the path is shown in red as *File not found* with a warning icon. Check that the drive or network share is connected and that the file has not been moved or renamed. The folder button then opens the closest folder that exists. The check is repeated every few seconds, so the warning goes away once the file is available.

When a presentation is loaded and a layer's file cannot be found, the layer keeps the path from the presentation file, and saving the presentation keeps it as well. An image layer loads on its own once the file is available; for other layer types, load the presentation again. In a cluster, each node also needs the file at the same path, unless the layer is [streamed from the master](#master-and-node-sources).

### Region of interest

In the top right, there is a *"Region of interest (ROI)"* ![](/assets/icons/trim-to-selection-lime.svg) / ![](/assets/icons/trim-to-selection-crimson.svg) button that enables the feature below, where you can specify a certain region of your layer that should be visible. This is useful when you want to show only part of the source.

![Layer Region of Interest](/assets/ui/layers_view_roi.png)

For mapping choices, see [Video & image mapping modes](/media/mapping). For fades, visibility across slides, and timeline keyframes, return to [Build presentation](/media/cplaypres).

### Maximum visibility

In the **Inspect** view, set the **Layer visibility** slider to the brightest level you want, then click the lock beside it. The current percentage becomes that layer's maximum. The slider shows **max …%** and shades the part of the range above the limit. You can still lower the visibility or fade the layer out; fading it in stops at the locked maximum, including during slide transitions and entrance effects.

Click the lock again to restore the full 0–100% range. The maximum is saved with the layer. If you select a division sublayer, the lock applies to that sublayer.

### Maximum volume

The **Max volume** slider in the **Inspect** view sets the loudest level for a layer with audio. Visibility scales the volume, so fading the layer in stops at this level and fading it out takes the audio down to silence. At 50% visibility, a layer with **Max volume** at 80% plays at 40%. A locked maximum visibility limits the volume in the same way. While the layer plays below its maximum, the slider shows **(now …%)**, with the solid bar marking the current level and the lighter bar reaching up to the maximum. If the master volume also controls layer volumes and is below 100%, or the slide volume is below 100%, a line below the slider shows the actual volume after these are applied.

### Slide volume

Every slide, including the master, has a **Slide volume** slider just above its layers list. It scales the volume of all layers in that slide, on top of each layer's own volume and visibility, and together with the master volume when **Master volume control all layers volume** is enabled. With the slide volume at 50% and the master volume at 50%, a layer at full volume and visibility plays at 25%.

The buttons on either side of the slider fade the slide volume over the fade duration. Fading down goes to 0, and fading up returns to the level the last fade down started from, or else to 100. Moving the slider stops a running fade.

The slide volume is saved with the presentation, so saving after a fade down stores 0. It can also be set through the [`/slide_volume` API endpoint](/remote-control/api), or from a [Control layer](/media/layers) with the **SetSlideVolume** operation, so that one slide can change the volume of another. The **FadeSlideVolumeDown** and **FadeSlideVolumeUp** operations, and the `/slide_volume_fade_down` and `/slide_volume_fade_up` endpoints, run the same fades. A referenced layer follows the slide that owns it.

## Post process effects

Post process effects are always applied to the layer's image, for example to key out a green screen. Unlike [entrance and exit effects](/media/cplaypres#effects-pane-entrance-exit-and-post-process-effects), they belong to the layer, not to the slide: they apply in every slide that shows the layer, on the master and on every node, and they are saved with the layer.

Click **Effects** in the Inspect view toolbar and open the **Post Process** tab. The **Effects** button is highlighted when the layer has effects. Turn an effect on with its check box; its settings then appear below it. **Reset** restores an effect's default settings, and **Remove all** turns every effect off.

![Post process effects](/assets/ui/effects_post_process.png)

| Effect | Settings | Description |
|--------|----------|-------------|
| **Chroma key** | Key color, Similarity, Smoothness, Spill reduction | Makes a background color, such as a green or blue screen, transparent. *Similarity* sets how close to the key color a pixel must be to be removed, *Smoothness* softens the edge, and *Spill reduction* removes the key color's tint from the edges that remain. |
| **Luma key** | Threshold, Smoothness, Remove bright areas instead | Makes dark areas transparent, for example graphics on a black background, or bright areas when inverted. |
| **Color correction** | Brightness, Contrast, Saturation, Gamma, Hue | Adjusts the colors. A saturation of 0 % gives black and white. |
| **Invert colors** | — | Shows the image as a negative. |
| **Tint** | Color, Amount | Colors the image with one color, for a sepia or monochrome look. |
| **Blur** | Radius | Softens the image, for example to put a background out of focus. |
| **Sharpen** | Amount | Enhances edges and fine details. |
| **Pixelate** | Block size | Shows the image as large blocks. |
| **Vignette / edge fade** | Radius, Softness, Amount, Edges | Darkens the edges of the layer, or, with *Edges* set to *Fade to transparent*, fades them out to blend the layer into what is behind it. |

Blur, sharpen and pixelate act on the image first; the other effects then follow in the order of the table. All of them are applied after the [region of interest](#region-of-interest) crop and before the layer's opacity and its entrance and exit effects.

### Picking the key color

Set the key color in one of these ways:

* Click the color swatch to open a color dialog, or type a hex value such as `#00b140`.
* Click **Green**, **Blue** or **Magenta** for a standard key color.
* Click **Pick from image**, then click in the Inspect view on the background to remove. You can also drag across the image; the color follows the mouse until you release the button. Press **Esc** to cancel. The color is averaged over 5 × 5 pixels of the original image (before effects), so camera noise does not decide it. Picking is available when the pane is opened from the Inspect view.

While the pane is open from the Inspect view, **Preview in Inspect view** shows the Inspect view with the effects applied, over a checkerboard where they make the image transparent.

For a typical green screen: pick the key color from the background, raise *Similarity* until the background is gone, then raise *Smoothness* a little for softer edges and *Spill reduction* until no green tint is left on the edges.

## Master and node sources

By default, nodes load their own copy of a synchronized layer. The **Sync** control in Inspect view can make a layer exist only on the master. This is separate from the **Master slide** and its Front/Back marking, which determine the layer's position in the [rendering order](/media/cplaypres).

![Inspect view toolbar with Effects, ROI, Nodes, Sync and NDI](/assets/ui/layer_view_toolbar.png)

For supported live layer types, **Nodes read from original source** lets each machine load its source, while **Master sends content to nodes** renders on the master and streams the texture. The **Nodes** control in Inspect view also enables texture streaming. See [Streaming layers from master to nodes](/playback/node-streaming) for transport, audio, and synchronization details.

Use per-machine presets when the master and nodes need different local inputs. Use a master placeholder when the source only exists on the nodes and you still need a visible layer to position in the master view. For NDI, OMT, Stream, YouTube and WebRTC layers, the **Nodes only** control in Inspect view does this without a preset: see [Receive on the nodes only](#receive-on-the-nodes-only).

## Per-node resolution in clusters

In a cluster setup (master + nodes), each machine can resolve a different local media path for the same predefined stream. When you add a Stream layer from the predefined list, C-Play stores the entry's *title* as a stable key on the layer and syncs that key to all machines. Each machine then looks up the entry in its own local `data/predefined-streams.json` and resolves which path it should open:

1. **`paths[role]`** — An optional object mapping roles to paths. The role is `"master"` on the master, or the node id from `data/nodes.json` on nodes (the machine's IP address when no id can be resolved). When a key exists for that machine its value wins over everything else - even an empty value (`""` or `null`). An empty value means *this machine intentionally does not open this stream*.
2. **`pathTemplate`** — An optional template string where `{nodeId}` is replaced with the machine's role/node id (e.g. `"av://dshow:video=Capture Card ({nodeId})"`). Used when there is no explicit `paths` entry for that machine.
3. **`path`** — The plain default path, used as a fallback when neither of the above applies. It may be empty, in which case only machines listed under `paths` open the stream.

If an entry's title does not exist at all in a machine's local file (for instance if the files differ between machines), that machine falls back to the path synced from the master. Custom (non-predefined) stream paths are always used verbatim on every machine, and per-node resolution is not applied.

Example:

```json
{
    "title": "HDMI Capture 1",
    "path": "",
    "paths": {
        "master": "av://dshow:video=Capture Card (RX0)",
        "node-A": ""
    }
}
```

In this example the master opens its local capture card, while `node-A` intentionally does not open anything. All other nodes fall back to the plain `path`, which is also empty here - so only the master displays this stream.

Note that per-node resolution on a node requires the machine to be identifiable through `data/nodes.json` (see [Multi-video composition](/media/cplaymulti)). The local JSON files are re-read every few seconds, so edits take effect without restarting C-Play.

DirectShow capture setups work the same way with `data/predefined-directshows.json`: when you add a DirectShow layer from the predefined list, C-Play stores the entry's *title* as a stable key on the layer and syncs it to all machines. Each machine then looks up the entry in its own local file and resolves which capture devices it should use:

1. **`devices[role]`** — An optional object mapping roles to device pairs (`{"videoDevice": ..., "audioDevice": ...}`). The role is `"master"` on the master, or the node id from `data/nodes.json` on nodes (the machine's IP address when no id can be resolved). When a key exists for that machine its value wins over everything else - even when both values are empty (`""` or `null`), which means *this machine intentionally does not open any capture at all* and the layer stays idle there.
2. **`videoDevice` / `audioDevice`** — The plain default device names, used as a fallback when there is no explicit `devices` entry for that machine.

Unlike streams, DirectShow setups do not support `{nodeId}` templates - capture device names are machine-specific strings, so per-role entries are the natural way to describe them. If an entry's title does not exist at all in a machine's local file (for instance if the files differ between machines), that machine falls back to the device pair synced from the master. Custom (non-predefined) DirectShow layers always use their chosen devices verbatim on every machine, and per-node resolution is not applied.

Example:

```json
{
    "title": "HDMI Capture 1",
    "videoDevice": "",
    "audioDevice": "",
    "devices": {
        "master": { "videoDevice": "DELTA-hmi Video Source (card0 RX0)", "audioDevice": "" },
        "node-A": { "videoDevice": "Datapath VisionSC-DP2 Video 01", "audioDevice": "" }
    }
}
```

In this example the master captures from its DELTA card while `node-A` captures from its Datapath card. All other nodes fall back to the plain devices (both empty here) - so they stay idle for this setup.

DirectShow preset keys are stored in *.cplaypres* files (`"directShowPreset"`), so per-machine resolution survives saving and reloading a presentation.

Capture uses `sources[role]`, where a role entry inherits the default fields it does not specify. Spout uses `senders[role]`, then the default `sender`. Both use the same role and node identity rules. Keep the preset title consistent across machines; source values can differ in their local JSON files.

## Master placeholders for node sources

When a live source only exists on the nodes, configure the predefined source's
master entry as `{ "placeholder": true }`. The master renders a TextLayer label
such as `C-Play Layer 12 - Camera`, using the same name and ID format as layer NDI
output. Drag it in the master view or change its plane position as usual: these
controls still belong to the original source layer and are synchronized to the
nodes. Each node opens its own source and uses its own source dimensions.

Use the appropriate mapping in the local predefined source file:

| Layer | File | Master entry |
| --- | --- | --- |
| DirectShow | `data/predefined-directshows.json` | `"devices": { "master": { "placeholder": true } }` |
| Capture | `data/predefined-captures.json` | `"sources": { "master": { "placeholder": true } }` |
| Stream | `data/predefined-streams.json` | `"paths": { "master": { "placeholder": true } }` |
| Spout | `data/predefined-spouts.json` | `"senders": { "master": { "placeholder": true } }` |

The placeholder replaces both video and audio on the master. Node overrides and
the preset's default source continue to work as before. A null or empty master
entry still means no source and no placeholder. Only the master recognizes the
placeholder flag. The label uses the configured subtitle font, with an available
font as fallback; its texture is 1280 × 256 pixels and renders as mono text.

For example, a Capture preset with a placeholder on the master and different
inputs on two nodes:

```json
{
    "captures": [
        {
            "title": "Stage camera",
            "backend": "datapath",
            "input": 1,
            "enabled": true,
            "sources": {
                "master": { "placeholder": true },
                "node-A": { "input": 1 },
                "node-B": { "input": 2 }
            }
        }
    ]
}
```

Spout presets appear alongside discovered senders in the Spout selector, even
when the master has no sender. Select the preset's title; its `sender` is the
default sender name, and `senders` maps master/node roles to local sender names.
Preset titles should differ from discovered sender names.

The placeholder is resolved from each machine's local preset JSON. Keep preset
titles consistent across machines. Source configuration edits are picked up within a few seconds while the layer updates. Streaming the master
texture to nodes is suspended while a placeholder is active, so the nodes keep
receiving the original layer configuration and opening their own sources.

### Receive on the nodes only

Often the master only needs to show *where* a live layer is, not its content. Receiving and decoding a high-bitrate feed on the master then costs network bandwidth, CPU and GPU time for nothing. For **NDI**, **OMT**, **Stream**, **YouTube** and **WebRTC** layers, turn on **Nodes only** in the Inspect view toolbar (next to **Sync**). The master then stops receiving the source and shows a placeholder with the layer title instead, while every node receives and decodes the source as before. No predefined source file is needed, and the setting is saved with the presentation.

- The placeholder behaves like the preset placeholders above: position, size, visibility and layer operations still apply to the layer and are synchronized to the nodes.
- The placeholder keeps the aspect ratio of the source, so a plane layer has the same size on the master as on the nodes. The size comes from what the nodes report receiving, or else from what the master received before **Nodes only** was turned on. It is saved with the presentation, so the placeholder has the right shape right after loading. A stereo source counts as one eye.
- The master plays no audio for the layer. Audio on the nodes is unaffected.
- **Nodes only** requires the layer to exist on the nodes, so it has no effect while **Sync** marks the layer master-only. Streaming the layer texture to the nodes (**Nodes**) is not available, since the master has no texture to stream.
- **WebRTC:** without the master receiving the stream, there is nothing to relay. Every node pulls the stream directly from the WHEP server, whatever the **Master relay** setting, and frames are not presented in sync across nodes. Make sure the server accepts one connection per node.
- **YouTube:** the master has no playback position, so the nodes stop following it. Play, pause, stop and seeks from the master still reach the nodes, but between those the nodes play on their own, and the master's seek slider does not show the playback position.
- **Stream:** a placeholder from a predefined stream entry and **Nodes only** can be combined; either one makes the master show the placeholder.

Turning **Nodes only** off again makes the master reconnect to the source.
