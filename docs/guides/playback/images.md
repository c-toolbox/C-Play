---
title: Background and foreground images
layout: home
nav_order: 2
parent: Playback features
---

# Background and foreground images

Alongside the primary media (being video, audio or images) and optional overlay image (specific for each media), C-Play has functionality for one additional background image and one additional foreground image. 

These images are usually designed to be defined at startup, but can be changed during runtime without overriding the default value for next launch of C-Play.

For changing the images runtime, this is made in the "Settings -> Configure -> Images", as seen [here](../settings/image.md).

At runtime, you can choose to display or hide both the background and foreground in the UI header, as seen below.

![Image control](../../assets/ui/header_taskbar/image.png){:width="50%"}

The bottom of this user interface has a *"On master"* setting, which indicate what media you want to show in the master compared to the nodes. This setting is useful when you for instance want to seek in the media file for a certain position, or loading a new background, without showing the changes to the audience, until you are ready to fade between background and media.

### Fading between background and media

While both background and foreground image is either completely visible or hidden, the background is normally fade into view by decreasing the visibility (i.e. increasing the transparency) of the primary media. This is fading is controlled in the header UI, as seen below.

![Visibility fading](../../assets/ui/header_taskbar/visibility.png){:width="50%"}

*Note: The media visibility is always automatically fading up when you press "Play" on your media.*