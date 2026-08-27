---
title: Video configuration
layout: home
nav_order: 1
parent: Setup C-Play
---

# Video decoding configuration

This guide is intended for installers who want to get maximum performance out of C-Play. See [video](../media/video) in the media guide if you want to know how C-Play loads and handles video files.

C-Play has embedded [mpv](https://mpv.io/), which is highly capable free and open-source media player. MPV can be configured in many ways to maximize performance based on your system.

For reference, at the moment, the latest C-Play comes shipped with compilation of the mpv 0.36 library, and a backend of the latest ffmpeg 5.1 version.

## MPV configuration files

C-Play ships with a bunch of different pre-configured mpv configuration files, located in the *data/mpv-conf* directory.

If nothing is specified, C-Play will load the configuration in the *default* folder.

But, by supplying a command-line parameter "*--mpvconf decoding_gpu_nvdec*" you would be loading the specific configuration files in that folder when C-Play starts.

Each configuration folder can contain up to three different JSON files:

* all.json
* master-only.json
* nodes-only.json

The *all.json* file should include options you want C-Play to use on both the master and the clients/nodes. Respectively, you can set options only for the master or only for the clients/nodes in the other two files.

The various configurations that C-Play ships with can be found [here](https://github.com/c-toolbox/C-Play/tree/master/data/mpv-conf), for reference.

All the different mpv options can be found [here](https://mpv.io/manual/stable/).

### CPU vs GPU

As seen in the included configurations, some are named "*_gpu*" and some are named "*_cpu*". MPV is a diverse and powerful media player that can take advantage of a high-end CPU system, by defining thread usage in the configuration file, or a high-end GPU by selecting a decoding library of your choice.

It should be noted that GPU decoding libraries are often more limited in terms of supported formats and resolutions.

However, should you load a video file that is not supported by the GPU to be decoded, MPV automatically tries to utilize the CPU to decode it if the GPU says it can't.

### Time sync vs Display sync

By default, the MPV playback engine times the video frames to the audio, and then everything is synced against the system clock. If no audio is present, as is usually the case for the nodes, the system clock is also the default method for syncing. Normally you would have a domain NTP (Network Time Protocol) setup so the computers in the cluster do not differ in time by many milliseconds, and the default mode should work fine.

However, as a cluster setup usually requires frame-syncing to avoid tearing, we could ideally use a timer based on the display rate, instead of the system clock for our playback timing.

The MPV configuration named *"decoding_gpu_nvdec_video_sync"* showcases the settings needed to enable this. As seen in the config, there are two interesting options:

1) *"video-sync": "display-resample"*

This line tells MPV to resample the audio to match the display rate, opposite of the default behaviour.

2) *"display-fps-override": "59.94"*

Usually the nodes are frame-synced, but the master is not. Because of that, we want to tell all instances to use the refresh rate of the synchronized displays, including the master, so the timing between the master and nodes is the same. You could override only on the master and let the nodes use a detected value if desired, but in the included example we override all.
 
## H264 vs H265

C-Play has been mostly tested with video files encoded with either H264 or H265, as they are very common formats around and well supported by various decoder libraries integrated in MPV.

### Pros/cons of H264

H264 has been around for quite some time, but it is still a widely used format. It is fast to encode, takes reasonably little space, and is fast to decode depending on bitrate.
However, the specs limit H264 to 4K videos. You are allowed to go beyond that, to for instance encode a 4K 3D side-by-side video with H264, ending up with a 8Kx2K video.

C-Play will load and play this video. However, even if you specified the use of a GPU decoding library such as nvdec, the GPU will not allow that video to be decoded because it is outside the specification. So C-Play will play it using the CPU decoder, which is not restricted in the same way.

### Pros/cons of H265

H265 is a newer format and a premier choice for streaming 4K content and beyond. While it takes longer to encode, it produces higher quality at smaller bitrates and results in smaller files than H264 at the same quality.

However, H265 is much more complex to decode, and you might notice that if you try to play H265 files on the CPU with C-Play, it struggles a bit. However, the structure of H265 makes it much more suitable to be better decoded on the GPU. And the resolution specification of H265 goes up to 8K, meaning that a GPU decoding library (such as nvdec) will allow a 4K 3D side-by-side video (8K x 2K) in H265 to be decoded on the GPU.

## Per-file MPV options

In addition to the global MPV configuration folders described above, C-Play can apply extra MPV options to individual layers. This makes it possible to use special decoding settings for a single file (for instance a transparent video) without affecting any other layer.

### Option files

Per-file options are defined in JSON files placed directly in the *data/mpv-conf* directory (next to the global configuration folders, not inside them). The file name determines which layer type the options apply to:

* *\<name\>_video.json* — video layers
* *\<name\>_audio.json* — audio layers
* *\<name\>_stream.json* — stream layers

The *\<name\>* part is what identifies the option set in the user interface, so a file named *transparent_video.json* shows up as *transparent* for video layers. You can create as many option sets as you need, and the same name can be used with different suffixes to cover several layer types.

C-Play ships with one such file, *transparent_video.json*, described below. The available files are scanned at runtime, so new files added to *data/mpv-conf* appear in the UI without restarting.

### Selecting options for a layer

Open the layer's view from the master UI. Video and audio layers have a **"Video Options"** dropdown next to the audio controls at the top of the window, and stream layers have the same dropdown in their stream controls. It lists *"Use global settings"* (the default) plus every option file found for that layer type.

When an option set is selected, C-Play first applies the global configuration (*all.json* and *master-only.json* / *nodes-only.json*) and then loads the per-file options, so the per-file values take precedence over the global ones. Changing the selection re-applies the options to the layer's already running MPV instance.

The selection is stored as part of the layer, so it is saved in *.cplayfile* files and synced to the nodes in a cluster setup (master and nodes must run the same C-Play version, and the option file must exist in *data/mpv-conf* on all computers).

### Example: transparent videos

C-Play supports playing back transparent videos, which have been tested with both VP9 and HAP formats. This is implemented as the shipped *data/mpv-conf/transparent_video.json* option file, which contains:

```json
	"hwdec": "no",
	"alpha": "yes",
	"vd": "libvpx-vp9,libvpx"
```

To use it, open the layer view of the video layer and select *transparent* in the **"Video Options"** dropdown.

1) *"hwdec": "no"*

This disables hardware decoding for the layer, since GPU decoders do not support alpha channel output. The video is decoded on the CPU instead.

2) *"alpha": "yes"*

This option tells MPV to enable alpha channel rendering, so the transparent parts of the video are displayed as transparent. HAP encoded files (and other formats) with alpha support should now playback with transparency.

3) *"vd": "libvpx-vp9,libvpx"*

This forces MPV to use the libvpx VP8 & VP9 decoders for these video formats, which support alpha channel decoding compared to the default VP8 & VP9 decoders.
