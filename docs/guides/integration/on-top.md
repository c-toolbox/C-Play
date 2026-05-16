---
title: C-Play "On-Top" + Spout
layout: home
nav_order: 3
parent: System integration
---

# C-Play "on-top" of other apps

C-Play nodes can run above other local applications. This is useful when another program should remain open and interactive behind the C-Play output, while C-Play can still reveal, hide, mask, or layer content in front of it.

Common examples are:

* A PowerPoint, browser, game engine, simulation, or control UI running behind the C-Play node window.
* C-Play fading its node window opacity down to reveal the application behind it.
* C-Play staying on top for mapped media, subtitles, masks, or overlays while another application continues to render locally.
* A local Windows application sending frames to C-Play through Spout instead of going through the network.

---

## Always on top

The **window on-top** action keeps the node display window above other operating-system windows. Toggle it from the C-Play header using the window on-top action described in [Views](../playback/views#always-on-top).

You can also make this active at startup with **Node windows always on top at startup** in [Window & UI settings](../settings/window_and_ui).

When this is enabled, you can keep another application running behind C-Play while C-Play remains the visible projection surface. For example, a browser or PowerPoint can sit behind the node window, and C-Play can show mapped media, foreground images, or presentation layers above it.

---

## Reveal the application behind C-Play

C-Play can fade node window opacity between fully visible and hidden. This makes it possible to reveal the local application behind C-Play without changing focus or closing the C-Play node window.

Use the **window opacity** action in the header, or trigger it from a presentation **Control** layer using `SetNodeWindowsOpacity`. The fade duration is configured in [Window & UI settings](../settings/window_and_ui).

Typical workflow:

1. Start the background application on the node machine.
2. Place the application window on the same display area as the C-Play node window.
3. Start C-Play and enable **window on-top**.
4. Fade C-Play opacity down when the background application should be visible.
5. Fade C-Play opacity back up when C-Play content should take over again.

This is a local-compositing technique. The application behind C-Play is not captured as a C-Play texture; it is simply visible because the C-Play node window is transparent or hidden.

---

## Use C-Play layers above another app

Presentation layers can be used as overlays while the background application keeps running. For example, a master-slide layer can act as a persistent background or mask, while a normal slide layer can show titles, logos, timers, or mapped media above the live application.

Layer ordering still follows the normal presentation rules described in [Presentation (Slides & Layers)](../media/cplaypres): master layers are behind normal media, slide layers are in front of media, and foreground images are on top.

---

## Spout for local app sharing

Spout is a Windows-only way for one application to share live GPU frames with another application on the same machine. When another application can send Spout, C-Play can receive it as a **Spout** layer.

This is different from the on-top/opacity workflow:

| Workflow | Best for | What C-Play receives |
|----------|----------|----------------------|
| On-top + opacity | Revealing a full local app behind the C-Play node window | No texture; the app is visible through the OS window stack |
| Spout layer | Bringing a local app's rendered output into C-Play as content | A live video texture that can be mapped, faded, masked, and layered |

To use Spout:

1. Start the sending application and enable its Spout output.
2. In C-Play, add a **Spout** layer.
3. Select the sender from the Spout sender list.
4. Configure the layer like other live video layers.

Spout is often the cleaner option when the external application should become part of the C-Play composition. On-top/opacity is often simpler when the external application should stay as a separate local program that is only revealed at certain moments.

