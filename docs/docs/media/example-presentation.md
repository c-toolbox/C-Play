---
title: Example dome presentation
sidebar_position: 8.5
---

# Example dome presentation

The **Dome Showcase** is a ready-made presentation that shows what you can do with slides and layers in a dome. Download it, open it in C-Play, and step through it. Then open each slide to see how it is built.

**[Download C-Play-Dome-Showcase.zip](/examples/C-Play-Dome-Showcase.zip)** (1.3 MB, for C-Play v2.4 and newer)

![Media in the Dome Showcase](/assets/examples/dome-showcase-media.jpg)

All images in the example were generated for it, so you can reuse them in your own shows.

## Open and play it

1. Unzip the file. Keep the `C-Play-Dome-Showcase` folder together: the presentation finds its images in the `media` folder next to it.
2. In C-Play, show the slides list and click ![](/assets/icons/document-open.svg) **Open a saved presentation**. Pick `Dome-Showcase.cplaypres`.
3. Press the right arrow key (or *Next* in the [presenter view](/remote-control/web-views)) to step forwards, and the left arrow key to step back.

Only the master slide is visible after loading. Turn on ![](/assets/icons/task-complete-lime.svg) **Pre-load** to load all slides before you start. See [Presentation playback](/playback/presentation).

Every slide has presenter notes that say what it shows and when to click. They are shown in the presenter view.

## What the slides show

| # | Slide | What it shows |
|---|-------|---------------|
| – | **Master** | A starfield as a *Back* layer behind everything (dome mapping), and the C-Play logo as a *Front* layer above everything. See [master layers](/media/cplaypres). |
| 1 | **Welcome** | Two Text layers on planes. The title has a *Zoom* entrance with *OutBack* easing; the subtitle *floats* in after it. |
| 2 | **Look around** | Four cards at azimuth 0°, 90°, 180° and 270°. The first flies in with the slide; the other three are *On click* steps, so each *Next* reveals one card. |
| 3 | **Fisheye dome** | A full-dome aurora (180° fisheye dome master) revealed with an *Iris* wipe. Click to sweep in an orientation grid with a *Clock* wipe. |
| 4 | **360° landscape** | An equirectangular (2:1) panorama on the *Sphere (EQR)* grid, revealed with a *Dissolve*. Panorama and caption leave together with a 2 s *Fade* exit. |
| 5 | **Timeline orbit** | A slide timeline (14 s) with five keyframes that move a moon around the dome, changing azimuth, elevation, scale and opacity with *InOutSine* easing. |
| 6 | **Particles and move** | An image assembles from *Particles*. Click to lift it towards the zenith with a *Move* effect. It scatters into particles when you leave the slide. |
| 7 | **Post process** | The same image three times, each with a *luma key* that removes its black background so it floats over the stars: luma key only, plus *tint* and *vignette*, and plus *colour correction* and *pixelate*. The last click adds a white-on-black wordmark keyed the same way. |
| 8 | **What else** | A text slide listing the layer types that need your own files, devices or network sources: video, audio, PDF, NDI, OMT, Spout, WebRTC, streams, capture cards, REST, TCP and Control. |
| 9 | **Thank you** | The aurora again, turned 90° with the dome *yaw*. The text spins in and bursts into particles on exit. |

## Look at how it is built

* Select a slide and a layer, then click **Inspect** to see its mapping, plane position and post process effects. See [Layer operations](/media/layer-operations).
* Click the star button left of **Inspect** to open the effects pane with the layer's entrance, exit and post process effects. See [Animations](/media/cplaypres#animations-c-play-v24-and-newer).
* On *Timeline orbit*, open the timeline editor to see the keyframes. See [Timeline](/media/cplaypres#-timeline-c-play-v23-and-newer).

## Fit it to your dome

The plane layers are placed for the default surface radius of 740 cm, with text and cards between 20° and 50° elevation. If your dome is tilted or a different size, adjust the plane *elevation* and *distance* in **Inspect**, or change the surface settings in [Grid settings](/settings/grid). Save the presentation under a new name to keep your changes.

The example uses only images and text, so it runs the same on a single computer and on a cluster: copy the folder to the same location on every node, or use [node media folders](/media/node-media-folders).
