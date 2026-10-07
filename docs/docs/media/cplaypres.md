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
| ![](/assets/icons/backgroundtool.svg) | Master slide (permanent background layers) |

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

Even if you have not created any slides, there is always a special slide called *Master* that you can add layers to. The special thing about the *master* slide is that layers in this slide are always rendered in front of the background image, but behind any media you play from *"Open file"* or through the playlist. Hence, the master layers essentially work as flexible background layers. Layers in the other slides are rendered in front of the media in the main player, and the foreground image is on top.

I.e., from top to bottom in render hierarchy:

* ![](/assets/icons/layer-top.svg) Foreground image
* ![](/assets/icons/layer-raise.svg) Slides in list
* ![](/assets/icons/media-playback-start.svg) Main media
* ![](/assets/icons/backgroundtool.svg) Master slide
* ![](/assets/icons/layer-bottom.svg) Background image

To add new layers, click the ![](/assets/icons/list-add.svg) "+" button to the left in the layers list, and the *"Add new layer"* will become visible. 

![Layer Add New](/assets/ui/layer_add_new.png)

Choose a layer type and source, then open **Layer View** to preview and position it. The [Layers and sources guide](/media/layers) explains every layer type, source setup, per-node inputs, and [master placeholders](/media/layers#master-placeholders-for-node-sources).

### ![](/assets/icons/table.svg) Layer visibility across slides.

In the slides menu there is a button named *"Visibility"* ![](/assets/icons/table.svg), which opens the window named *"Slide Visibility Table View"* when clicked. The window lets you control and review how layers behave across slides. This means that you can create layers that are visible across multiple slides, instead of only being shown during one specific slide. The meanings of the different cell colors are:

* A grey cell means layer not yet available.

* A red cell (100 -> 0%) means the layer fades out.

* A green cell labeled (0 -> 100%) means this layer fades in when the slide is triggered. Click this cell to make the next cell red.

* A white cell (100%) means the layer stays visible. Click this cell to make the next cell red.

* A black cell can be changed to a white cell by clicking it.

Remember, master layer visibility usually fades in and out depending on media visibility. See [presentation settings](/settings/presentation).

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

