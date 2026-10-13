---
title: Presentation (Slides & Layers)
sidebar_position: 3
---

# Presentation (Slides & Layers)

A presentation can be built according to the guide seen [here](/media/cplaypres).

To play a presentation, you can use the left and right arrow keys on the keyboard, if you have not changed the default shortcuts, to move forwards and backwards through the slide deck. At startup, when a presentation is loaded, no slides other than master layers should ideally be visible. This is good practice before saving the presentation, as seen in the left image below. Notice that the *"pre-load icon"* ![](/assets/icons/address-book-new-crimson.svg) to the right of *"Slides:"* is always red at startup. This prevents pre-loading from occurring if the presentation is not supposed to be used, even if it is loaded at startup.

As seen in the middle image, when pressing the *"Pre-Load"* button ![](/assets/icons/task-complete-lime.svg), it turns green and all slides shift from *Red* to *Orange*, which means the slides and layers are loaded, but not yet visible. When a slide is triggered and the layers become visible, they turn *Green*. If a slide contains both visible and non-visible layers, two colors might show in the slide, as indicated in the image below to the right.

![Master Layer](/assets/ui/slides_layers_master.png) &nbsp;&nbsp;&nbsp; ![PreLoad Layers](/assets/ui/slides_layers_preload.png) &nbsp;&nbsp;&nbsp; ![Triggering Slides](/assets/ui/slides_layers.png)

In the *"Visibility"* ![](/assets/icons/table.svg) window, you can set layers to be visible during multiple slides, which may come in handy if you want background media to stay visible during a set of slides while fading in layers one by one.

![Slide Visibility Matrix](/assets/ui/slide_visibility_matrix.png)

For changing the presentation timings, this is made in the "Settings -> Configure ![](/assets/icons/configure.svg) -> Presentation", as seen [here](/settings/presentation).

## Layer operation modes (C-Play v2.3 and newer)

Layers that receive a live video feed (NDI, OMT, Stream, DirectShow, Capture, Spout and WebRTC) support a **Layer Operation** mode that controls how the layer's texture is subdivided into sublayers. The mode is selected via a combo box in the layer view:

| Mode | Description | Supported by |
|------|-------------|--------------|
| **None** | No subdivision — the layer displays normally. | All layers |
| **ImPres Mode (QR)** | QR code-driven sublayer control (see below). | NDI, OMT, Stream, DirectShow, Capture, Spout, WebRTC |
| **Division Mode** | Static grid-based texture division (see below). | NDI, OMT, Stream, DirectShow, Capture, Spout, WebRTC |

When a sublayer mode is active, the layer view shows a **Sub-Layer** selector and highlights the selected sublayer's region in yellow on the preview.

---

### Division Mode

Division Mode splits the layer's live video texture into a configurable grid of sublayers. This is useful for displaying multiple views or regions from a single video source across different positions in the dome or sphere.

When Division Mode is selected, a **Division** combo box appears with the following grid options:

| Grid | Layout |
|:-:|--------|
| 1×1 | Single (no split) |
| 1×2 | 1 column, 2 rows |
| 2×1 | 2 columns, 1 row |
| 2×2 | 2 columns, 2 rows |
| 2×3 | 2 columns, 3 rows |
| 3×2 | 3 columns, 2 rows |
| 3×3 | 3 columns, 3 rows |

Each sublayer points to a region of the parent texture (no copy) and has its own alpha.

#### Merge layout

The **Merge** combo box sets how the sublayers are placed when the grid mode is *Plane*. They are put side by side, in reading order (left to right, top row first), on the chosen grid. That grid is centred on the *Original* layer's plane, at the same scale. The options are every grid that holds all the cells without leaving a row or column empty. For example:

| Division | Merge options |
|:-:|--------|
| 1×2 | 1×2 (as source), 2×1 |
| 2×2 | 1×4, 2×2 (as source), 3×2, 4×1 |
| 3×3 | 1×9, 2×5, 3×3 (as source), 4×3, 5×2, 6×2, 7×2, 8×2, 9×1 |

With the source layout the merged result looks like the undivided layer. When the last row of a layout has fewer cells, those cells are left-aligned.

The *Original* layer sets the position, orientation, distance and size of the whole layout, including circular plane mapping. Sizes are per cell: a cell is the *Original* plane's width divided by the division columns and its height divided by the division rows. Moving or resizing any sublayer, through its grid parameters or the HTTP API, moves or resizes the whole layout, so the cells always stay aligned. Changing the grid mode of a sublayer changes it for all of them. A new division starts with its source layout. Changing the division resets a merge layout that no longer fits.

On a circular plane, the cells are placed by centre angle. With a plane roll they are not rotated around the layout centre.

Choose **Custom** to give each sublayer **independent** grid mode, plane parameters, rotation and translation, all editable in the grid parameters panel. Presentations saved before merge layouts existed open as *Custom*.

---

### QR code presentation mode (ImPres)

ImPres is a QR code-driven presentation mode. When enabled, C-Play scans incoming video frames for QR codes and uses them to control frozen video sublayers — useful for interactive multi-angle presentations where a camera feed carries embedded QR commands.

![ImPres QR Concept](/assets/impres/impres-icons-concept.png)

### Enabling ImPres

In the layer view for a live-feed layer (NDI, OMT, Stream, DirectShow, Capture, Spout or WebRTC), select *"ImPres Mode (QR)"* from the **Layer Operation** combo box. This setting is saved with the presentation. NDI, OMT, DirectShow and WebRTC layers scan each frame before it is uploaded; Stream, Capture and Spout layers read the frame back from the GPU, which costs more on high resolutions.

### Example
You can open the example PowerPoint at `data/impres/ImPres_PPT_Template.ppt`, which shows one practical way to display the QR codes so they are visible immediately when a new slide appears and then disappear quite quickly.

### How it works

The system uses a two-phase processing scheme:

1. **Phase 1 (control frame)**: A QR code is detected in the video frame. The command is queued and the frame is skipped — the previous clean frame stays on screen so the QR code is never visible to the audience.
2. **Phase 2 (clean frame)**: No QR code is detected. All queued commands are executed and the frame is displayed normally.

This means the QR code only needs to appear for a single frame to be picked up.

### QR command format

Commands are encoded as text in the QR code using semicolons as delimiters:

```
<target>;<action>
```

**Targets** are plane names defined in the configuration file (see below), or `All` for bulk operations.

**Actions:**

| Action | Description |
|--------|-------------|
| `SetActive` | Switch to this plane — freeze all other sublayers and start displaying live video in the target plane. If the plane does not exist yet, it is created. |
| `Clear` | Fade all sublayers to transparent (use with target `All`). |

Multiple actions can be chained: `FrontCapture;SetActive;Freeze`.

### Predefined planes

Planes define where frozen sublayers appear in the dome or sphere. They are configured in `data/impres/config.json`:

```json
{
  "planes": [
    { "name": "FrontCapture" },
    { "name": "LeftCapture", "azimuth": -45 },
    { "name": "RightCapture", "azimuth": 45 },
    { "name": "TopCapture", "elevation": 65 }
  ]
}
```

Each plane can optionally specify `height`, `azimuth`, `elevation`, `roll`, and `distance`. Properties that are not specified are inherited from the parent layer.

---

## Streaming layers from master to nodes (C-Play v2.4 and newer)

Any layer type except Audio can be streamed from the master to all nodes, so each node does not have to decode its own copy of the media — see [Streaming layers from master to nodes](/playback/node-streaming).