---
title: Video configuration
nav_order: 1
parent: Setup C-Play
---

# Video decoding configuration

This is the configuration guide for installers, how to get maximum performance out of C-Play. See the [video](../media/video.md) in the media guide if you want to now how C-Play loads and handle video files.

C-Play has embedded [mpv](https://mpv.io/), which is highly capable free and open-source media player. MPV can be configured in many ways to maximize performance based on your system.

For reference, at the moment, the latest C-Play comes shipped with compiliation of the mpv 0.36 library, and a backend of the latest ffmpeg 5.1 version.

## MPV configuration files

C-Play ships with a bunch of different pre-configured mpv configuration files, located in the *data/mpv-conf* directory.

If nothing is specified, C-Play will load the configuration in the *default* folder.

But, by supplying a command-line parameter "*--mpvconf decoding_gpu_nvdec*" you would be loading the specifc configuration files in that folder when C-Play starts.

Each configuration folder can contain up to three different JSON files:

* all.json
* master-only.json
* nodes-only.json

The *all.json* should include option you would like C-Play to utilize on both master and clients/nodes. An respectivly, you can set settings for only the master or only the clients/nodes in the other two files.

The various configuration that C-Play ship with can be found [here](https://github.com/c-toolbox/C-Play/tree/master/data/mpv-conf), for reference.

All the different mpv options can be found [here](https://mpv.io/manual/stable/).

### CPU vs GPU

As noticed by the included bunch of configurations, there is some that are named "*_gpu*" and some(one) named "*_cpu*". MPV is a diverse and powerful media player that can take advantage of a high-end cpu system (by defining thread usage in the configuration file) or high-end gpu:s by selecting a decoding library of your choice.

It should be noted, that when you select that you prefer to use a GPU decoding library, does libraries are often more limited in terms of supported formats and resolutions.

However, should you load a video file that is not supported by the GPU to be decoded, MPV automatically tries to utilize the CPU to decode it if the GPU says it can't.

### Time sync vs Display sync

By default, the MPV playback engine time the video frames to the audio, and then everything is synced against the system clock. If no audio is present (as is the usual case for the nodes), system clock is also default method for syncing. As you normally, you would have a domain NTP (Network Time Protocol), which hopefully i setup in such away that the computers in the cluster do not differ in time buy many milliseconds, and the default mode should work fine.

However, as a cluster setup usually requires frame-syncing to avoid tearing, we could ideally use a timer based on the display rate, instead of the system clock for our playback timing.

The MPV configuration named *"decoding_gpu_nvdec_video_sync"* does showcase the settings needed to enabled this. As seen in the config, there is two interesting options: 

1) *"video-sync": "display-resample"*

This line tells MPV to resample the audio to match the display rate, opposite of the default behaviour.

2) *"display-fps-override": "59.94"*

As usually the nodes are frame-synced, but not the master. Does, we want to tell all instances to use the refresh-rate of the syncronized displays, even the master, such that the timing between master and nodes is the same. You could override only on the master, and let the nodes use a detected value, if desired, but in the included example we override all.
 
## H264 vs H265

C-Play has been mostly tested with video files encoded with either H264 or H265, as they are very common formats around and well supported by various decoder libraries integrated in MPV.

### Pros/cons of H264

H264 has been around for quiet some time, but is still a widely used format. It is fast to encode, takes reasonably small space, and is fast to decode (depending on bitrate).
However, the specs limit H264 to 4K videos. You are allowed to go beyond that, to for instance encode a 4K 3D side-by-side video with H264, ending up with a 8Kx2K video.

C-Play will load and play this video. However, even if you specifed the use of a GPU decoding library (such as nvdec), the GPU will not allow that video to be decoded, as it is outside the specifications. So C-Play will play it utilizing the cpu decoder, which is not a restricted.

### Pros/cons of H265

H265 is a bit newer format, and the premier format for streaming 4K content and beyond. While it takes longer time to encode, it produces higher quality for smaller bitrate, and does smaller files then H264, with the same quality.

However, H265 is much more complex to decode, and you might notice that if you try to play H265 files on the CPU with C-Play, it struggles a bit. However, the structure of H265 makes it much more suitable to be better decoded on the GPU. And the resolution specification of H265 goes up to 8K, meaning that a GPU decoding library (such as nvdec) will allow a 4K 3D side-by-side video (8K x 2K) in H265 to be decoded on the GPU.