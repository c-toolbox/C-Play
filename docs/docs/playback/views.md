---
title: Views
sidebar_position: 6
---

# Views

C-Play provides several view-related features for controlling how content is displayed, both on the master application and on networked display nodes.

### ![](/assets/icons/map-globe.svg) Master view

The master view is what C-Play's main window displays — either just the main video, all layers rendered in an interactive 3D scene, or all layers through a 180° fisheye lens. The layer-rendering modes are useful for previewing how content will look when projected onto domes or spheres.

The view-mode button in the header taskbar opens a menu with the three master view modes. Its icon reflects the current mode:

* **Show only the main video** — the standard flat playback view (crimson flat-map icon).
* **Show 3D view with perspective camera** — the interactive 3D scene described below (lime globe icon).
* **Show 3D view with fisheye camera** — a flat circular fulldome image, see [Fisheye 180° projection](#fisheye-180-projection-fulldome) below (light blue half-circle icon).

The same menu is available under **Settings → States**, where the *Master view* entry also shows an icon of the currently selected mode — there the items are worded as *Render only the main video*, *Render 3D view with perspective camera*, and *Render 3D view with a fisheye camera*. Choosing a mode only changes the current session; which mode C-Play starts in is decided by the [Window & UI settings](/settings/window_and_ui). The [NDI output](/system-integration/ndi-output) follows this mode as well — while a 3D view is active it publishes the rendered scene instead of the main video.

The 3D view supports an interactive camera with orbit controls, letting you rotate around the scene to inspect your content from different angles.

#### Navigating the 3D view

The camera starts at the centre of the rendered sphere/dome, looking straight forward. All navigation is done with the mouse directly in the 3D view:

| Input | Action |
|:---|:---|
| **Left- or right-click + drag** | *Orbit* the camera. Dragging right/left turns the view horizontally, dragging up/down tilts it vertically. |
| **Mouse wheel / touchpad scroll** | *Zoom* (dolly) along the view axis. Scrolling up moves the camera forward into the scene, scrolling down moves it back. The camera is kept inside the rendered sphere, so it cannot pass through the content surface. |
| **Ctrl + left-click + drag** | *Manipulate the selected layer* with the operation configured for Ctrl in the [Presentation settings](/settings/presentation) (default: aim at pointer). Spheres rotate with the pointer (horizontal → yaw, vertical → pitch), domes horizontally only. With no layer selected it orbits like plain dragging. |
| **Alt + left-click + drag** | Same as Ctrl+drag, but using the operation configured for Alt in the [Presentation settings](/settings/presentation). |
| **Shift + left-click + drag** | Same as Ctrl+drag, but using the operation configured for Shift in the [Presentation settings](/settings/presentation) (default: resize plane size). |
| **Double-click** | *Reset the camera* back to its original position and orientation (centre of the scene, no rotation). Zoom level and orbit rotation are both restored. |

A press only becomes a drag after the pointer has moved a few pixels, so a double-click never accidentally rotates the view or moves a layer.

#### Moving layers in the 3D view

Ctrl, Alt and Shift + left-drag all manipulate the **selected layer** — flat (plane), dome or sphere. The [Layers](/playback/presentation) panel is the single source of truth for which layer is moved: select the layer row there first, then drag it in the 3D view with the modifier held down. If a plain 2D layer — or no layer at all — is selected, the modifier+drag falls back to orbiting the camera instead.

Each modifier combo is independently assigned one of these operations in the [Presentation settings](/settings/presentation):

* **Elevation & Azimuth** (default for Ctrl and Alt) — aims a flat layer at the pointer wherever it is, updating *plane azimuth* and *plane elevation*.
* **Elevation** / **Azimuth** — same as above but changes only that one angle.
* **Horizontal & Vertical Move**, **Horizontal Move**, **Vertical Move** — slide a flat layer sideways/up-down (updating its horizontal/vertical offsets) instead of re-aiming it; the content follows the pointer.
* **Resize plane size** (default for Shift) — scales a flat layer's *plane width* and *plane height* proportionally: dragging down grows the plane, dragging up shrinks it.
* **Move plane distance** — moves a flat layer away from / toward the camera.

Sphere layers always rotate with the pointer movement since press regardless of which operation is configured: horizontal dragging controls *yaw*, vertical dragging controls *pitch*. Dome layers rotate in *yaw* only (horizontal dragging); their pitch is left unchanged.

While dragging, the values are updated live and mirrored back to the grid parameters dialog and the Layers list. When you release the mouse, the change is marked for saving with the current slide.

The mouse cursor indicates the current mode: an open hand while orbiting, a closed hand while manipulating a layer.

#### Field of view

The camera's field of view is set by *"3D View FOV"* in the [Window & UI settings](/settings/window_and_ui) (30–150 degrees, default is a wide-angle view). It is not changed by mouse input — use the wheel to zoom and the setting to change the lens angle.

#### Fisheye 180° projection (fulldome)

Select **"Show 3D view with fisheye camera"** from the view-mode menu in the header taskbar to replace the perspective camera with a virtual **180° equidistant fisheye lens** centered on the zenith (straight up). To start C-Play directly in this mode, enable *"Render 3D view as 180-degree fisheye (fulldome) at startup"* in the [Window & UI settings](/settings/window_and_ui). The whole scene is then projected onto a flat circular image — exactly what an omnidirectional 180° fulldome camera would capture from inside your dome or sphere.

* Dome-mapped, sphere-mapped (EQR and EAC), and flat/plane layers are all rendered through the lens; plain 2D layers stay flat as usual.
* The image is *equidistant*: distance from the centre equals angle from the zenith. For full-hemisphere content (180° FOV) the disk matches exactly what normal mode shows on the dome, and areas below the horizon are rendered black. Content with a narrower FOV sits in the inner region of the circle.
* The output is locked to the zenith: orbiting or zooming the camera does not change it, and the dome tilt angle is ignored while this mode is active.
* The *"3D View FOV"* setting does not apply in this mode — the lens angle is fixed at 180° — and dome overflow masking is also not applied.

Switching between the modes in the view-mode menu takes effect immediately; no restart is needed. This is useful for previewing how your content looks as a flat fulldome image, for example before stitching or when checking coverage of the full hemisphere.

#### Dome overflow masking

When working with dome-mapped content, you can hide the area that falls outside the dome projection. Enable *"Hide dome overflow in 3D view"* in the Window & UI settings. An opacity slider (0–100%) controls how strongly the overflow area is masked, allowing you to see a faint outline of the full content or hide it completely.

In [Window & UI settings](/settings/window_and_ui), you can choose which master view mode C-Play starts in.

### ![](/assets/icons/view-task.svg) Floating window layer

The *floating window layer* feature provides a frameless floating window within your display for showing layer or video content in a secondary view. This is useful for picture-in-picture style monitoring while working with the main interface.

Toggle the floating window on or off using the ![](/assets/icons/view-task-lime.svg) (on) / ![](/assets/icons/view-close-crimson.svg) (off) action in the header taskbar.

#### Configuration

The floating window position, size, and startup visibility can all be configured in the [Window & UI settings](/settings/window_and_ui):

* **Position** — X and Y coordinates for where the floating window appears.
* **Size** — Width and height of the floating window.
* **Visible at startup** — Whether the floating window is shown automatically when C-Play launches.

The floating window can toggle between displaying the main video layer and layer content, making it flexible for different monitoring workflows.

### View states

![States](/assets/ui/header_taskbar/states.png)
Besides the master view modes explained above, there are a few other view states.

#### Always on top

Toggle whether the node window stays above all other operating system windows using the *window on-top* action ![](/assets/icons/window-restore-pip-lime.svg) (on) / ![](/assets/icons/window-minimize-pip-crimson.svg) (off) in the header. The icon changes between a raised and lowered pip to indicate the current state.

This can also be set to activate at startup via *"Node windows always on top at startup"* in the [Window & UI settings](/settings/window_and_ui).

#### Opacity and fading

The node window opacity can be faded between fully visible (1.0) and fully hidden (0.0) using the *window opacity* action ![](/assets/icons/view-visible-lime.svg) (visible) / ![](/assets/icons/view-hidden-crimson.svg) (hidden). The fade animation duration is configurable in the Window & UI settings (default 2 seconds). While the window is in a partially transparent state, the action indicates *"TRANSPARENT"* with an ![](/assets/icons/view-visible-orange.svg) orange highlight.

How the opacity is applied can be chosen as **Node window opacity mode** in the [Window & UI settings](/settings/window_and_ui#node-window-behavior): with *Content-based (alpha channel)* unchecked, the entire window fades uniformly, content included; checked, only the unrendered/background areas fade between opaque black and fully transparent, keeping rendered content fully visible. The choice is synced from the master to the nodes and applies immediately.
