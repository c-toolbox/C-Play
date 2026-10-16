---
title: Build presentation (*.cplaypres)
sidebar_position: 8
---

# Build and save a presentation (*.cplaypres)

Presentations in C-Play are essentially a list of *slides*, where each slide contains a list of *layers*. These lists can be saved and loaded as "*.cplaypres" files.

These lists are shown or hidden through the buttons on either the left or right side of the *footer* taskbar, depending on your playlist preference.

![Slide and Layers show and hide](/assets/ui/slides_layers_show_hide.png)

When showing the slides and layers lists, the top of these lists includes the primary controls for building slides with layers.

![Slide and Layers building](/assets/ui/slides_layers_top.png)

#### Slides toolbar

The slides panel toolbar (first and second row):

| Icon | Action |
|------|--------|
| ![](/assets/icons/document-open.svg) | Open a saved presentation |
| ![](/assets/icons/document-replace.svg) | New presentation (clear all slides) |
| ![](/assets/icons/system-save-session-lime.svg) / ![](/assets/icons/system-save-session-orange.svg) | Save presentation (lime = saved, orange = unsaved changes) |
| ![](/assets/icons/task-complete-lime.svg) / ![](/assets/icons/address-book-new-crimson.svg) | Pre-load layers toggle (lime = on, crimson = off) |
| ![](/assets/icons/table.svg) | Slide visibility table view |
| ![](/assets/icons/list-add.svg) | Add slide to bottom of list |
| ![](/assets/icons/list-remove.svg) | Remove selected slide |
| ![](/assets/icons/pan-up-symbolic.svg) | Move selected slide up |
| ![](/assets/icons/pan-down-symbolic.svg) | Move selected slide down |
| ![](/assets/icons/backgroundtool.svg) | Master slide (permanent layers, behind or above all slides) |

#### Layers toolbar

The layers panel toolbar (first and second row):

| Icon | Action |
|------|--------|
| ![](/assets/icons/document-open.svg) | Open presentation |
| ![](/assets/icons/system-save-session-lime.svg) / ![](/assets/icons/system-save-session-orange.svg) | Save presentation |
| ![](/assets/icons/layer-top.svg) | Move selected layer to top |
| ![](/assets/icons/layer-bottom.svg) | Move selected layer to bottom |
| ![](/assets/icons/trash-empty-crimson.svg) | Clear all layers |
| ![](/assets/icons/configure.svg) | Configure grid parameters |
| ![](/assets/icons/layer-new.svg) | Add a new layer |
| ![](/assets/icons/layer-delete.svg) | Remove selected layer |
| ![](/assets/icons/layer-raise.svg) | Move selected layer up |
| ![](/assets/icons/layer-lower.svg) | Move selected layer down |
| ![](/assets/icons/document-edit-decrypt-verify.svg) | Layer view (inspect selected layer) |

Even if you have not created any slides, there is always a special slide called *Master* that you can add layers to. Master layers are permanent: they are not tied to a triggered slide. Each master layer is either a **Back** or a **Front** layer:

* **Back** layers (default) are rendered in front of the background image, but behind any media you play from *"Open file"* or through the playlist. They work as flexible background layers.
* **Front** layers are rendered above the layers of all other slides, but below the foreground image. Use them for permanent overlays such as logos, frames or masks.

Layers in the other slides are rendered in front of the media in the main player, and the foreground image is always on top.

I.e., from top to bottom in render hierarchy:

* ![](/assets/icons/layer-top.svg) Foreground image
* ![](/assets/icons/backgroundtool.svg) Master slide, Front layers
* ![](/assets/icons/layer-raise.svg) Slides in list
* ![](/assets/icons/media-playback-start.svg) Main media
* ![](/assets/icons/backgroundtool.svg) Master slide, Back layers
* ![](/assets/icons/layer-bottom.svg) Background image

Within each group, the order of the master layers list applies (top of the list is rendered on top). The same order is used in the master 3D view and on all nodes.

#### Marking master layers as Front or Back

When the *Master* slide is selected, two buttons, **Front** and **Back**, are shown next to the *Master Layers* title above the layers list. They set the hierarchy of the selected layer, and show which one it currently has. Each master layer also shows a small grey *Front*/*Back* mark in the top right corner of its row; click the mark to toggle it. Locked layers and references cannot be changed.

New master layers start as Back layers. Presentations saved before this feature load with all master layers as Back, so they render exactly as before. A `"hierarchy": "front"` entry is only saved for Front layers; older C-Play versions ignore it and show those layers as Back layers.

To add new layers, click the ![](/assets/icons/list-add.svg) "+" button to the left in the layers list, and the *"Add new layer"* will become visible. 

![Layer Add New](/assets/ui/layer_add_new.png)

Choose a layer type and source, then open **Layer View** to preview and position it. The [Layers and sources guide](/media/layers) explains every layer type and source setup. [Layer operations](/media/layer-operations) covers positioning, post process effects, per-node inputs, and [master placeholders](/media/layer-operations#master-placeholders-for-node-sources).

Plane layers can also use [circular horizontal mapping](/settings/grid#circular-surface-mapping). Define and enable the shared 360° canvas in Grid Settings; new plane layers then default to **Circular degrees**. Presentation files store `circular`, `centreDegrees` and `widthDegrees` in the layer's `plane` object, including texture-division sublayers. The shared surface calibration stays in GridSettings. Missing circular mode fields inherit the global default; explicitly saved modes are preserved.

### ![](/assets/icons/table.svg) Layer visibility across slides.

In the slides menu there is a button named *"Visibility"* ![](/assets/icons/table.svg), which opens the window named *"Slide Visibility Table View"* when clicked. The window lets you control and review how layers behave across slides. This means that you can create layers that are visible across multiple slides, instead of only being shown during one specific slide. The meanings of the different cell colors are:

* A grey cell means layer not yet available.

* A red cell (100 -> 0%) means the layer fades out.

* A green cell labeled (0 -> 100%) means this layer fades in when the slide is triggered. Click this cell to make the next cell red.

* A white cell (100%) means the layer stays visible. Click this cell to make the next cell red.

* A black cell can be changed to a white cell by clicking it.

Remember, the visibility of master Back layers usually fades in and out depending on media visibility. Master Front layers are not affected by media visibility. See [presentation settings](/settings/presentation).

![Slide Visibility Matrix](/assets/ui/slide_visibility_matrix.png)

### ![](/assets/icons/chronometer.svg) Timeline (C-Play v2.3 and newer)

The timeline is a per-slide animation system that lets you animate layer properties over time using keyframes. Each slide can have its own timeline with a configurable duration (default 5 seconds).

![Slide Timeline](/assets/ui/slide_timeline.png)

#### Animatable properties

For each keyframe you can set:

* **Alpha (Opacity)** — from 0.0 (transparent) to 1.0 (fully visible)
* **Rotation** — X, Y, Z angles in degrees
* **Translation** — X, Y, Z positional offset

The system interpolates smoothly between keyframes at 60 FPS during playback.

#### Using the timeline editor

Enable the timeline for a slide via the toolbar toggle. The editor displays one track per layer, with a time ruler across the top.

* **Add a keyframe** — Double-click on a layer track at the desired time.
* **Select and edit** — Click a keyframe to select it, then modify its properties (time, alpha, rotation, translation) in the inspector panel.
* **Delete a keyframe** — Right-click on an existing keyframe.
* **Scrub** — Drag the red playhead line to preview the animation at any point.
* **Zoom** — Use the zoom slider to adjust the time scale.

#### Intro and outro sections

The timeline can be split into an *intro* and *outro* section by dragging the amber divider line. This lets you define separate fade-in and fade-out animations within the same timeline. When the outro finishes, layers fade to zero opacity and the timeline stops.

#### Playback modes

| Mode | Description |
|------|-------------|
| **Forward** | Plays from the start to the outro point (or end). |
| **Reverse** | Plays backward from the end to the start. |
| **Intro only** | Plays only the intro section. |
| **Outro only** | Plays only the outro/fade-out section. |
| **From position** | Resumes playback from a custom scrub position. |

#### Saving

Timeline data, including all keyframes and duration settings, is stored in the *.cplaypres* file alongside the rest of the slide and layer definitions.

### Animations (C-Play v2.4 and newer)

C-Play v2.4 extends the timeline with easing curves, more animatable properties, and a PowerPoint-style *animation pane* for entrance and exit effects. Keyframes need the slide timeline. Entrance and exit effects do not: every layer can have them, also on a slide without a timeline. On a timeline slide, effects and keyframes share the same timeline, so you can use either one or both.

#### Keyframe easing

Each keyframe has an **Easing to next** setting in the timeline inspector. It shapes the movement from that keyframe to the next one. A small curve preview is shown next to the selector.

| Easing | Description |
|--------|-------------|
| **Linear** | Constant speed (the default, and the v2.3 behavior). |
| **In / Out / InOut** — Quad, Cubic, Quart, Quint, Sine, Expo, Circ | Standard acceleration (*In*), deceleration (*Out*) or both (*InOut*), from gentle (Sine, Quad) to strong (Quint, Expo). |
| **In / Out / InOut** — Back, Elastic, Bounce | Overshoot, spring and bounce curves. |
| **Hold** | Keeps the value until the next keyframe, then jumps. |
| **Bezier** | A CSS-style cubic Bézier curve. |

The keyframe marker shows the easing type: a **diamond** for linear, a **circle** for eased, and a **square** for hold. Press **F9** with a keyframe selected to apply *Easy ease* (InOut Cubic), as in After Effects. Press **Delete** to remove the selected keyframe.

#### Extra keyframe properties

In addition to alpha, rotation and translation, a keyframe can include these optional groups. Turn them on with the checkboxes in the inspector:

* **Scale** — Layer size in percent (100 % is the original size).
* **Plane** — Azimuth, elevation and roll (degrees), and distance, horizontal and vertical offset (cm). Used for layers in plane grid mode.

An optional group only interpolates between keyframes that have it turned on. Keyframe values are absolute layer values.

#### Effects pane (entrance, exit and post process effects)

Open the effects pane in one of these ways:

* Click **Effects** in the layer view toolbar. The pane then shows only the effects of that layer, and follows the layer selected in the layer list. The button is highlighted when the layer has effects.
* Click **Effects** in the timeline toolbar (the timeline does not have to be enabled).
* Double-click an effect bar in the timeline.

The pane has three tabs: **Entrance**, **Exit** and **Post Process** (see [Post process effects](/media/layer-operations#post-process-effects)). To add an effect, pick a layer in the *Layer* list, click **Add effect**, and choose a preset from the gallery. Only layers with visual content can be animated, and the master slide has no entrance or exit effects.

On a slide without a timeline, layers without effects keep the regular slide fade, and the effects play on top of it when the slide is triggered and left. On a timeline slide, the timeline duration grows automatically to fit the effects. Turning the timeline off keeps the effects you added, and removes the default fades that turning it on gave the layers.

| Category | Presets | Description |
|----------|---------|-------------|
| Basic | **Appear**, **Fade** | Shows or hides the layer at once, or fades its opacity. |
| Wipe & mask | **Wipe**, **Iris**, **Clock**, **Dissolve** | Reveals or hides the layer with an animated mask: a directional wipe, a circle from the center, a clock sweep, or a noise dissolve. |
| Motion | **Fly**, **Float** | Slides the layer in from, or out to, a side. *Float* moves a shorter distance and fades at the same time. |
| Zoom & spin | **Zoom**, **Spin** | Scales the layer from, or to, nothing, or spins it around the viewing axis while fading. |
| Move | **Move** | Moves the layer's plane from one pose to another. See [Move effects](#move-effects). |

Select an effect in the list to edit its options:

| Option | Description |
|--------|-------------|
| **Effect** | Change the preset. |
| **Start** | *On click*, *With previous* or *After previous*. See [Triggers and click steps](#triggers-and-click-steps). |
| **Duration** | Length of the effect (default 0.5 s). |
| **Delay** | Wait time before the effect starts, counted from its trigger. |
| **Easing** | Any of the [easing curves](#keyframe-easing) (default OutCubic). |
| **Direction** | Fly, Float, Wipe and Spin: Left, Right, Top, Bottom, or one of the four corners. |
| **Softness** | Wipe & mask presets: how soft the mask edge is. |
| **Amount** | Fly, Float, Zoom and Spin: strength of the effect (0.1×–3×). For example, the number of turns for Spin. |

Each row shows a timing bar, so you can see when every effect plays within its phase. Use the buttons below the list to:

* **Play earlier / Play later** — Change the playback order.
* **Remove** — Delete the effect (or press **Delete**).
* **Convert to keyframes** — Turn a *Fade* or *Zoom* effect into normal timeline keyframes that you can edit further.
* **Play** — Preview the selected effect (or double-click it in the list).
* **Play intro / Play outro** — Preview the whole phase.

In the timeline, effects are shown as green (entrance) and red (exit) bars below each layer track. Drag a bar to change its delay, or drag its right edge to change its duration.

#### Post process effects

The **Post Process** tab holds effects that are always applied to the layer's image, such as chroma key, luma key, color correction, blur and vignette. They belong to the layer rather than the slide, so they apply in every slide that shows it. See [Layer operations → Post process effects](/media/layer-operations#post-process-effects) for every effect, its settings, and how to pick a key color.

#### Triggers and click steps

Entrance effects play in list order, and each one starts according to its trigger:

| Trigger | Starts |
|---------|--------|
| **On click** | At the next *Next slide* command (keyboard, remote, or the `/slide_next` API). Numbered as a step in the list, and marked in the timeline ruler. |
| **With previous** | Together with the previous effect. |
| **After previous** | When the previous effect has finished. |

While a slide has pending click steps, *Next* plays the next step instead of changing slide, and *Previous* steps back. When all steps have played, *Next* moves on to the next slide as usual. If the first effect is *On click*, the regular slide fade runs first.

Exit effects always play automatically when the slide is left, so *On click* acts as *After previous* in the exit phase.

#### Move effects

A **Move** effect animates the layer's plane position (azimuth, elevation, roll, distance, horizontal and vertical) from a start pose to an end pose. Unlike other effects, it does not hide or show the layer.

* By default, **Automatic start** is on, and the move starts from where earlier moves left the layer.
* To set a pose visually, select the layer in the Layers panel and drag it in the 3D view (Ctrl, Alt or Shift drag). Then click **Set from layer** under *Start* or *End*. Dragging a layer that is shown at an animated position only changes that position, not the layer's own saved settings.
* The end pose carries over to later slides where the layer stays visible, so a sequence of slides can move a layer step by step.

#### Layers kept from earlier slides

A layer kept visible across several slides (see *Layer visibility across slides* above) now appears in the layer list of every slide that shows it, as a *reference*. References are shown in italics with a link marker, and the tooltip names the slide that owns the layer.

* Each slide can order and animate its references independently. Keyframes and effects on a reference only apply while that slide is shown.
* Edits to the layer's own settings change the original layer.
* Removing a reference shortens the layer's visibility, so the layer is no longer shown from that slide on.
* The last slide showing a reference automatically fades it out during its outro. Editing that fade replaces it.

The Slide Visibility Table shows animated cells in cyan, labelled *Timeline Intro*, *Ref. Timeline* or *Timeline Outro*, so you can see which slides animate which layers.

#### Saving animations

Easing, extra keyframe properties, entrance and exit effects, reference animations and the layer order of each slide are saved in the *.cplaypres* file together with the rest of the timeline. Animation values are applied on top of the layer's saved settings during playback, so the layer's settings are not changed.

