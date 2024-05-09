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

If you are an installer and user of C-Play, you primarily need be familiar with the configuration file syntax, which can be found here: 

- [SGCT Configuration Files](https://sgct.github.io/configuration-files.html)

## Auto 2D vs 3D switch

Based on the assumption, that you do not mix 3D viewports with 2D viewports, meaing a window could cover a 3D projector/screen, that would show (Left + Right Eye), or for 2D projector/screen, but a window would not cover both at the same time.

As such, C-Play will check what kind of viewports that are available, and in the special case that it finds both 3D and 2D viewports, it will enable and/or disable them based on the content (and application settings) that C-Play has loaded.

Basically, in the below configuration, there is two viewports for a 3D content (where eye is either "left" och "right"), and there is viewports for 2D content only (which have eye "center").

C-Play would make sure that only the FIRST two viewports below, or the LAST two viewports, are enabled, but not all four at once.

This makes it possible to apply different calibrations for 3D vs 2D content, which in some scenarios can make a noticeable difference, and are then needed to show both 3D or 2D content correctly.

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

Bare in mind, that you can also run a "active 3D" configuraton or any configuration when left and right eye has the same viewport parameters. In most cases this works correctly. Then C-Play will show 3D and 2D correctly without additional viewports, as this is built in in C-Play to adapt the rendering based on mono vs stereographic material.