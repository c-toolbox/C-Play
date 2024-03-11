---
title: Video description files
parent: Media file structure
---

# Build video description files (*.cplayfile)

 C-Play can open any format that MPV support, which is basically any that the included FFmpeg libraries support.

 However, there is good practice to build a ".cplayfile" of any your video files, to store all of the descriptive settings, which C-Play get from MPV or which can be user-defined.
 
 These user defined settings are:

 * Media title
 * Stereoscopic mode
     * 2D (mono)
     * 3D (side-by-side)
     * 3D (top-bottom)
     * 3D (top-bottom+flip)
* Grid mode
     * 2D (mono)
     * 3D (side-by-side)
     * 3D (top-bottom)
     * 3D (top-bottom+flip)
* Separate Audio File
     * C-Play supports incorporated audio tracks in the video, however, through this option a seperate file can be loaded (on the master) as the default selected audio track.
* Separate Overlay File
     * C-Play support the loading of an overlay image file (such as *.png or *.jpg) to be overlayed ontop of the video. This might be useful when mapping a static world map ontop a video showing ocean flow, for instance. This overlay file needs to be loaded when the video is, thus can only be define in a *"*.cplayfile*".

## Workflow for *.cplayfile creation

1. Just load the video (mp4 etc) in C-Play. 


If the C-Play Audio Setting *"Load audio files in same folder as video file"* is checked, any adjacent audio file will be loaded as an audio track as well.

2. In the header menu, select correct *"Audio File"*, *"Stereo (2D or 3D) mode"* and *"Grid Mode"*, such that your video is combined with desired audio track and looks correct in your environment (arena, dome etc).

3. Click the button *"Save As C-Play File"* in the header menu, which will open the dialog below.
 ![Save As CPlayfile](/docs/assets/ui/saveAsCplayfile.png)
 The dialog will already be pre-defined with the current user defined values currently chosen in C-Play. You can change any value here before you save as well, and optionally add an overlay image as well.

 4. Click save...