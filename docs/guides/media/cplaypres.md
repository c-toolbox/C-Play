---
title: Build presentation (*.cplaypres)
layout: home
nav_order: 7
parent: Media file structure
---

# Build and save a presentation (*.cplaypres)

Presentations in C-Play essentially a list of *slides* consisting, where each slide is a list of *layers*. These list can be saved/loaded as "*.cplaypres" files.

These list are shown/hidden through the buttons on either the left or right side in the *footer* taskbar (depending on your preference for the playlist).

![Slide and Layers show and hide](../../assets/ui/slides_layers_show_hide.png){:width="30%"}

When showing the slides and layers lists, the top of these list including the primary controls for building slides with layers.

![Slide and Layers building](../../assets/ui/slides_layers_top.png){:width="85%"}

Even if you haven't created any slides, there is always a special slide called *Master* accessible to add layers too. The special thing with the master slide is that layers in this slide are always rendered in front of the background images, but behind and media you play from *"Open file"* or through the playlist. Hence the master layers essentially work a flexible background layers.

To add new layers, click the "+" button to the left in the layers list, and the *"Add new layer"* will become visible. 

![Layer Add New](../../assets/ui/layer_add_new.png){:width="50%"}

Here you can choose to add layers of the following types:

* Image
* Video
* Audio
* PDF
* NDI
* Stream (capture cards etc)
* Control (C-Play v2.3 and newer)

The four first ones all are local or network files that are chosen through file dialogs. The default dialog locations of each of these types can be changed in "Settings -> Configure -> Location", as seen [here](../settings/location).

When choosing *"NDI (Network Device Interface)*" a combobox becomes visible instead of the file dialog field. The combobox contains all of the discovered NDI sources on the network which can be received. Both image and audio is supported through NDI.

When choosing *"Stream*" you can choose between pre-defined streams for your system in a combobox (loaded from editable file *"data/predefined-streams.json*") or to add a custom entry in a text field. The stream is handled as video/audio with the MPV library, so explore possibilities through the MPV docs.

When choosing *"Control"*, the layer does not display any visual content. Instead, it dispatches a player control operation when activated through the slide timeline. Control layers exist only on the master node and are useful for automating playback actions within a presentation.

In the layer view for a control layer, two fields are shown instead of the usual grid/stereo parameters:

* **Operation** — The command to execute. Available operations include:
  * **Playback**: Play, Pause, Stop, Rewind, Seek, SetPosition, SetSpeed, SetVolume
  * **Fading**: FadeVolumeDown, FadeVolumeUp, FadeImageDown, FadeImageUp, SetSyncVolumeVisibilityFading
  * **Loading**: LoadFromPlaylist, LoadFromSlides, LoadFromSections, LoadFromAudioTracks
  * **Orientation**: SpinPitchUp, SpinPitchDown, SpinYawLeft, SpinYawRight, SpinRollCW, SpinRollCCW, OrientationAndSpinReset, RunSurfaceTransition

* **Parameter** — An operation-specific value. For example, a volume level for SetVolume, a millisecond position for Seek, a slide name or index for LoadFromSlides, or `true`/`false` for boolean operations. Parameters that reference playlist items, slides, or audio tracks can be specified by name or numeric index.

After you've added a new layer, you can specify the parameters of the layer in more detail through the *"Layer View*". Here you control grid/stereo parameters, the volume level (if applicable), as well as check how the output looks like.

![Layer View Video](../../assets/ui/layers_view_video.png){:width="40%"} &nbsp;&nbsp;&nbsp; ![Layer View Grid Parameters](../../assets/ui/layer_view_grid_parameters.png){:width="48%"}

### Region of Interest

In the top right, there is *"Region of interest (ROI)"* button, that enables a feature below where you can specify a certain region of you layer that should be visible. This is useful for when you want to show only a part of the source.

![Layer Region of Interest](../../assets/ui/layers_view_roi.png){:width="80%"}

### Layer visibility across slides.

In the slides menu there is a button named *"Visibility"*, which when clicked opens the window name *"Slide Visibility Table View"*. The window let's you control and overview how layer *live* across slides. This means that you can create layers that are visible across multiple slides, instead of only being shown during one specific slide. The meanings of the different cell colors are:

* A grey cell means layer not yet available.

* A red cell (100 -> 0%) means the layer fades out.

* A green cell titled (0 -> 100%) means this layer fades in when the slide is triggered. Click this cell to make the next cell red.

* A white cell (100%) means they layer stays visible. Click this cell to make the next cell red.

* A black cell can be altered to a white cell, if pressed.

Remember, master layers visibility is fade in/out usually depend on media visibility (See [presentation settings](../settings/presentation)).

![Slide Visibility Matrix](../../assets/ui/slide_visibility_matrix.png){:width="80%"}

### Timeline (C-Play v2.3 and newer)

The timeline is a per-slide animation system that lets you animate layer properties over time using keyframes. Each slide can have its own timeline with a configurable duration (default 5 seconds).

![Slide Timeline](../../assets/ui/slide_timeline.png){:width="80%"}

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

