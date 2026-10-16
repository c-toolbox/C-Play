---
title: Save video description (*.cplayfile)
sidebar_position: 5
---

# Build and save video description files (*.cplayfile)

A `.cplayfile` stores a media path and the settings needed to reopen it correctly. Use one for videos whose mapping, audio track, overlay, orientation, or sections need to be restored. It references the media files; it does not embed or copy them.

## Saved settings

* **Media title** — The name shown in the player and playlist.
* **Stereoscopic mode** — 2D (mono), 3D side-by-side, 3D top-bottom, or 3D top-bottom+flip.
* **Grid mode** — None (pre-split), Plane, Dome, Sphere EQR, or Sphere EAC. See [mapping modes](/media/mapping).
* **Separate audio file** — An external audio track selected when the media loads. Make it available on each machine configured to play audio.
* **Separate overlay file** — An image, such as PNG or JPEG, mapped on top of the video. Use the same grid and stereoscopic layout as the video.
* **Multi-video composition** — An optional `.cplaymulti` configuration for per-node video entries. See [multi-video composition](/media/cplaymulti#integration-with-cplayfile-c-play-file).
* **Orientation** (v2.3 and newer) — Enable **Save orientation** in the save dialog to restore the current viewing direction. Dome mapping saves yaw; sphere mappings (EQR and EAC) save yaw, pitch, and roll.

## Sections

Sections define named ranges within a video. Each has a title, start time, end time, and end-of-section (EOS) behavior:

* Pause
* Fade out video, then pause
* Continue video
* Next section
* Loop section

Open the sections panel beside the playlist. Its position follows the [playlist layout setting](/settings/playlist). Enter start and end times manually, or capture them from the current playback position.

## Create a .cplayfile

1. Open the media file in C-Play.

   If **Load audio files in same folder** is enabled in [Audio settings](/settings/audio), adjacent audio files appear as additional tracks.

2. In the header, choose the audio track, stereoscopic mode, and grid mode. Check the result in your display environment.
3. Optionally define sections and set the viewing orientation.
4. Click **Save As C-Play File** in the header.

   ![Save As C-Play File](/assets/ui/saveAsCplayfile.png)

   The dialog starts with the current settings. Review the title, paths, mapping, and optional overlay before saving.

5. Click **Save C-Play File**. Use the **Common C-play file location** configured in [Location settings](/settings/location) to keep descriptions together and simplify relative paths.

Reopen the saved file to check that the intended media and settings are restored before adding it to a [playlist](/media/cplaylist).
