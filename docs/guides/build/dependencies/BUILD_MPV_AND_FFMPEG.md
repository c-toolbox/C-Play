# Build MPV and FFmpeg with JACK+portaudio support on Windows

## Pre-setups

This guide assumes you use [Media Build Suite](https://github.com/m-ab-s/media-autobuild_suite) to get a free setup of MSYS64 and the dependencies to build ffmpeg and mpv.

Run the batch script after unzipping it in a short path, but quit after MSYS has been setup (i.e. do not run the configure yet)

## Install JACK package
Run msys2 installed during "Media Build Suite" setup. Then install Jack package (see below).

```
pacman -S mingw-w64-x86_64-jack2
```

## Build FFmpeg and MPV with JACK support

- Change the MPV git path from master to release 0.36 (latest one with bootstrap.py), by opening the file build\media-suite_deps.sh and replace in line 73 "https://github.com/mpv-player/mpv.git" with "https://github.com/mpv-player/mpv.git#release/0.36".

- My setup: Just copy over these [FFmpeg Options](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/ffmpeg_options.txt), [MPV Options](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/mpv_options.txt) and [Media Build Suite Configuration](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/media-autobuild_suite.ini) to the build folder. The latest FFmpeg 5.1 will be used in this configuration.
    - Alternatively, do not copy over these files, but during configuration, disable as many encoder as possible, as we want focus on decoding for the player. Also, choose to build ffmpeg with shared libs only, do not strip build files. When asked to edit the FFmpeg options file, add "--enable-libjack" to the configuration file.

- Run "Media Build Suite" batch script again, and do not remove the JACK package when asked. Continue and compile everything.

## Linking libmpv with MSVC programs

As according to the [Native compilation with MSYS2](https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md#native-compilation-with-msys2) you need to create a import library for the mpv DLL. Launch a Visual Studio command prompt (latest 2019 or later), and cd to the bin-video directory "(Media Build Suite location)\local64\bin-video". Copy the new "mpv.def" from the build "(Media Build Suite location)\build\mpv-git\build" to the bin-video directory. Then create the library with this syntax:

```
lib /def:mpv.def /name:mpv-2.dll /out:mpv.lib /MACHINE:X64
```

## Environmental variables for C-Play CMake build

Create an environmental variable called "PKG_CONFIG_PATH" and add the path "(Media Build Suite location)\local64\lib\pkgconfig".

Also add the "(Media Build Suite location)\local64\bin-video" to your environmental "Path".

## Extra: Alternative MPV compiliation for newer versions without bootstrap.py (should not be necassary)

Download/clone latest release source from [MPV repo](https://github.com/mpv-player/mpv), into your users home folder within Media Build Suites MSYS installation (msys64\home\...)

Launch MINGW64 MSYS (mingw64.exe) and move to the mpv source directory.

Build MPV with last steps of the official guide [Native compilation with MSYS2](https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md#native-compilation-with-msys2)
No need to install any more dependencies from the instructions above, as you use the MSYS from above again, where ffmpeg has already been built.
So build MPV and LibMPV as seen below.
```
meson setup build -Dlibmpv=true --prefix=$MSYSTEM_PREFIX
meson compile -C build
meson install -C build
```
