---
title: Cluster configuration
layout: home
nav_order: 3
parent: Setup C-Play
---

# Cluster configuration

The backend that enables management and cluster rendering is handled by a library called SGCT (Simple Graphics Cluster Toolkit). It handles communication between master and nodes in a cluster setup, as well as the setup of windows and viewports based on configuration files.
The documentation can be found here:

 - [SGCT Docs](https://sgct.github.io)

If you are an installer or user of C-Play, you primarily need to be familiar with the configuration file syntax, which can be found here:

- [SGCT Configuration Files](https://sgct.readthedocs.io/en/latest/users/configuration/index.html)

## Auto 2D vs 3D switch

This is based on the assumption that you do not mix 3D viewports with 2D viewports, meaning a window could cover a 3D projector or screen that would show left and right eye, or a 2D projector or screen, but a window would not cover both at the same time.

As such, C-Play will check what kinds of viewports are available, and in the special case where it finds both 3D and 2D viewports, it will enable or disable them based on the content and application settings currently loaded.

Basically, in the configuration below, there are two viewports for 3D content, where the eye is either "left" or "right", and there are two viewports for 2D content only, which have the eye set to "center".

C-Play would make sure that only the FIRST two viewports below, or the LAST two viewports, are enabled, but not all four at once.

This makes it possible to apply different calibrations for 3D and 2D content, which in some scenarios can make a noticeable difference and may be necessary to show both 3D and 2D content correctly.

* JSON : *Version 2.2 and above*
```json
"viewports": [
    {
        "eye": "left",
        "pos": { "x": 0, "y": 0 },
        "size": { "x": 0.5, "y": 1 },
        "projection": { "type": "PlanarProjection", }
    },
    {
        "eye": "right",
        "pos": { "x": 0.5, "y": 0 },
        "size": { "x": 0.5, "y": 1 },
        "projection": { "type": "PlanarProjection", }
    },
    {
        "eye": "center",
        "pos": { "x": 0, "y": 0 },
        "size": { "x": 0.5, "y": 1 },
        "projection": { "type": "PlanarProjection", }
    },
    {
        "eye": "center",
        "pos": { "x": 0.5, "y": 0 },
        "size": { "x": 0.5, "y": 1 },
        "projection": { "type": "PlanarProjection", }
    }
]
```

* XML : *Version 2.1 and below*

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