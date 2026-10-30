---
title: Layer operations
sidebar_position: 10
---

# Layer operations

Once a layer is added to a slide, you can position and crop it, apply post process effects to its image, and decide which machines in a cluster open its source. For the layer types and how to add them, see [Layers and sources](/media/layers). For slides, visibility, timelines, and entrance and exit effects, see [Build presentation](/media/cplaypres).

## Position, crop, and preview

After you have added a new layer, you can specify its parameters in more detail through the *"Layer View"*. Here you control grid and stereo parameters, the volume level if applicable, and inspect how the output looks.

![Layer View Video](/assets/ui/layers_view_video.png) &nbsp;&nbsp;&nbsp; ![Layer View Grid Parameters](/assets/ui/layer_view_grid_parameters.png)

### Region of interest

In the top right, there is a *"Region of interest (ROI)"* ![](/assets/icons/trim-to-selection-lime.svg) / ![](/assets/icons/trim-to-selection-crimson.svg) button that enables the feature below, where you can specify a certain region of your layer that should be visible. This is useful when you want to show only part of the source.

![Layer Region of Interest](/assets/ui/layers_view_roi.png)

For mapping choices, see [Video & image mapping modes](/media/mapping). For fades, visibility across slides, and timeline keyframes, return to [Build presentation](/media/cplaypres).

### Maximum visibility

In **Layer View**, set the **Layer visibility** slider to the brightest level you want, then click the lock beside it. The current percentage becomes that layer's maximum. The slider shows **max …%** and shades the part of the range above the limit. You can still lower the visibility or fade the layer out; fading it in stops at the locked maximum, including during slide transitions and entrance effects.

Click the lock again to restore the full 0–100% range. The maximum is saved with the layer. If you select a division sublayer, the lock applies to that sublayer.

## Post process effects

Post process effects are always applied to the layer's image, for example to key out a green screen. Unlike [entrance and exit effects](/media/cplaypres#effects-pane-entrance-exit-and-post-process-effects), they belong to the layer, not to the slide: they apply in every slide that shows the layer, on the master and on every node, and they are saved with the layer.

Click **Effects** in the layer view toolbar and open the **Post Process** tab. The **Effects** button is highlighted when the layer has effects. Turn an effect on with its check box; its settings then appear below it. **Reset** restores an effect's default settings, and **Remove all** turns every effect off.

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
* Click **Pick from image**, then click in the layer view on the background to remove. You can also drag across the image; the color follows the mouse until you release the button. Press **Esc** to cancel. The color is averaged over 5 × 5 pixels of the original image (before effects), so camera noise does not decide it. Picking is available when the pane is opened from the layer view.

While the pane is open from the layer view, **Preview in layer view** shows the layer view with the effects applied, over a checkerboard where they make the image transparent.

For a typical green screen: pick the key color from the background, raise *Similarity* until the background is gone, then raise *Smoothness* a little for softer edges and *Spill reduction* until no green tint is left on the edges.

## Master and node sources

By default, nodes load their own copy of a synchronized layer. The **Sync** control in Layer View can make a layer exist only on the master. This is separate from the **Master slide** and its Front/Back marking, which determine the layer's position in the [rendering order](/media/cplaypres).

![Layer View toolbar with Effects, ROI, Nodes, Sync and NDI](/assets/ui/layer_view_toolbar.png)

For supported live layer types, **Nodes read from original source** lets each machine load its source, while **Master sends content to nodes** renders on the master and streams the texture. The **Nodes** control in Layer View also enables texture streaming. See [Streaming layers from master to nodes](/playback/node-streaming) for transport, audio, and synchronization details.

Use per-machine presets when the master and nodes need different local inputs. Use a master placeholder when the source only exists on the nodes and you still need a visible layer to position in the master view.

## Per-node resolution in clusters

In a cluster setup (master + nodes), each machine can resolve a different local media path for the same predefined stream. When you add a Stream layer from the predefined list, C-Play stores the entry's *title* as a stable key on the layer and syncs that key to all machines. Each machine then looks up the entry in its own local `data/predefined-streams.json` and resolves which path it should open:

1. **`paths[role]`** — An optional object mapping roles to paths. The role is `"master"` on the master, or the node id from `data/multivideo/nodes.json` on nodes (the machine's IP address when no id can be resolved). When a key exists for that machine its value wins over everything else - even an empty value (`""` or `null`). An empty value means *this machine intentionally does not open this stream*.
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

Note that per-node resolution on a node requires the machine to be identifiable through `data/multivideo/nodes.json` (see [Multi-video composition](/media/cplaymulti)). The local JSON files are re-read every few seconds, so edits take effect without restarting C-Play.

DirectShow capture setups work the same way with `data/predefined-directshows.json`: when you add a DirectShow layer from the predefined list, C-Play stores the entry's *title* as a stable key on the layer and syncs it to all machines. Each machine then looks up the entry in its own local file and resolves which capture devices it should use:

1. **`devices[role]`** — An optional object mapping roles to device pairs (`{"videoDevice": ..., "audioDevice": ...}`). The role is `"master"` on the master, or the node id from `data/multivideo/nodes.json` on nodes (the machine's IP address when no id can be resolved). When a key exists for that machine its value wins over everything else - even when both values are empty (`""` or `null`), which means *this machine intentionally does not open any capture at all* and the layer stays idle there.
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
