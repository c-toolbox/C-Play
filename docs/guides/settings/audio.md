---
title: Audio settings
layout: home
nav_order: 1
parent: Settings
---

# Audio settings

![Audio settings](../../assets/ui/settings/audio.png) 

The audio settings within C-Play let's you choose audio output, through either a found device or a using a specific audio driver.

If the *"Use custom audio output*" is not selected, it utilizes the system default. When selected, the first choice is to try and detect a JACK device, but the drop-down list should also include devices located on the system. In some cases trying to find an audio driver instead of an device is preferred, but targeting a device solves most use cases.

For more knowledge into use multi-channel audio, such as JACK on your system, see the
[Audio configuration guide](../setup/audio).

In this settings dialog you can also alter the startup volume, and steps size for volume in the UI.

Also, you could choose to indicate the preferred track language or ID, if the video files include tracks. However, this is a general setting that mostly should be ignored.

From C-Play v2.1 and onwards, NDI is supported as a layer within a slide. There is a specific audio configuration for the NDI audio playback on master. By default it already used the default device of your PC, so you would only need to set this if you want to use another one.

