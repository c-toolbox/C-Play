---
title: Grid/mapping settings
sidebar_position: 2
---

# Grid/mapping settings

![Grid/mapping settings](/assets/ui/settings/grid.png) 

The grid/mapping settings within C-Play control the geometry used for texture mapping of all media. Objects such as planes (for flat media), domes (fulldome/fisheye content), and spheres (for equirectangular and equi-angular cubemap content) are used depending on the projection type.

### Plane settings

These define the default plane geometry for flat media:

* **Calculate size based on video** — When enabled, the plane dimensions are derived from the video resolution. When disabled, manual width/height values are used (default on).
* **Plane width** — Width of the plane in centimeters (default 1480).
* **Plane height** — Height of the plane in centimeters (default 740).
* **Plane elevation** — Vertical tilt angle in degrees (default 39).
* **Plane distance** — Distance from the camera in centimeters (default 740).
* **Horizontal slider range** — Maximum horizontal range in centimeters for plane positioning (default 5000).
* **Vertical slider range** — Maximum vertical range in centimeters for plane positioning (default 5000).

### Dome and sphere settings

* **Radius** — Radius of the dome or sphere in centimeters (0–20000, default 740).
* **Field of view** — Angular field of view in degrees (0–360, default 165).
* **Dome angle** — Tilt angle of the dome in degrees (0–360, default 27).
* **Rotation X / Y / Z** — Default rotation offsets around each axis in degrees (default 0 for all).
* **Translate X / Y / Z** — Default translation offsets along each axis (default 0 for all).

### Alternative transition scenario

An alternative set of dome/sphere parameters can be defined for transitioning between two states. For example, you can start inside a sphere and transition to an outside view.

* **Radius (2nd state)** — Target radius (default 400).
* **Field of view (2nd state)** — Target field of view (default 165).
* **Dome angle (2nd state)** — Target dome angle (default 27).
* **Translate X / Y / Z (2nd state)** — Target translation offsets (defaults: 0, 400, -800).
* **Transition time** — Duration of the transition in seconds (0–20, default 10).

Whenever new media with a different grid mode is loaded, these values reset to the standard startup values.

### Surface rotation speed

* **Rotation speed** — Speed multiplier for runtime rotation controls (default 0.01).

### Circular surface mapping

Enable **Circular horizontal mapping** to join the horizontal edges of a logical plane canvas. This is an unwrapped 360° surface, shared by all output viewports; it does not bend the mesh into a cylinder.

* **Full canvas width / height (pixels)** — Resolution of the complete logical canvas, for example 12000 × 1200. Width 12000 gives 33.33 pixels per degree. These dimensions describe the output canvas, not an individual layer's source resolution, and do not change the configured projector resolutions.
* **Full surface width (cm)** — Width of the logical projection plane in the scene's existing centimetre coordinates. Match this to the horizontal extent defined by your projection setup. Its left and right edges are at minus/plus half this width, before the global dome tilt. This calibrates degrees to scene coordinates; it is not the circumference of a new 3D cylinder.
* **Left/right seam angle (degrees)** — Angle assigned to the joined edges, default 0°. With left-to-right angles, the centre is 180° and the right edge is 360°.
* **Increasing angles** — Choose left-to-right or right-to-left movement. This reverses positioning, without mirroring the media texture.

Use **Save current grid settings to load on startup** to persist the mapping. The master synchronizes the active mapping to rendering nodes.

With circular mapping enabled, new plane layers default to **Circular degrees**, including layers switched to Plane for the first time. In **Grid Parameters**, enter the **Centre angle** and **Angular width** (greater than 0°, up to 360°). An explicitly saved movement mode takes precedence over the global default. The existing centimetre position and width remain stored separately. **Height below and video aspect ratio** keeps the specified height in centimetres and calculates angular width as `height × media aspect ratio × 360 / surface width`. **Width below and video aspect ratio** keeps the specified angular width and calculates height. **Values below** uses both specified dimensions. Media aspect ratio includes ROI and stereo adjustments. Distance and vertical movement remain in centimetres.

For a 12000-pixel canvas, a 600-pixel span corresponds to 18°. A layer centred at 355° with width 20° appears at 345°–360° on the right and 0°–5° on the left. The two portions share the same texture, crop, mask, opacity and animation state. Scaling or rotation that extends coverage beyond one circumference is clipped to one revolution to prevent overlapping copies from doubling opacity.

Horizontal timeline pose values use degrees for circular layers. Use an unwrapped target such as 370° when moving forward from 350° across the seam; stored angles are not reduced to 0°–360° during interpolation. Presentation layers without a saved movement mode inherit the global default when loaded; saved modes remain unchanged.
