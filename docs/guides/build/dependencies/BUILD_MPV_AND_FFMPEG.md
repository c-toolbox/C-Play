# Build MPV and FFmpeg with JACK+portaudio support on Windows

C-Play depends on the great *mpv* media player library, which indeed depends on the great *ffmpeg* library. It is usually straight-forward to build in many ways, but normally we want to modify the standard build by including JACK, to go from maximum 8-channel audio support, to support ASIO and other low-latency audio frameworks. Choose one of the two below options. After building, check further below for creating a linkable library.

## Option 1: Build mpv and ffmpeg from MINGW-packages

As of writing (2024-06-08) this is mpv 0.38 and ffmpeg 6.1.1.
This is the recommend option, as it gives you latest libraries.

Here are the steps:

Download MSYS from [official website](https://www.msys2.org/).
Install and the run *mingw64.exe*.

Then clone "https://github.com/msys2/MINGW-packages" into your mingw64 home folder.

```
git clone "https://github.com/msys2/MINGW-packages"
cd MINGW-packages/mingw-w64-ffmpeg
```

Edit the PKGBUILD file to include *${MINGW_PACKAGE_PREFIX}-jack2* and *--enable-libjack*. Also preferred to include missing *--enable-nvdec* after *--enable-nvenc*.
Here are my [mingw-w64-ffmpeg-pkgbuild](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/mingw-w64-ffmpeg/PKGBUILD) configuration file.

Then install dependencies for packing and perform the new packing:

```
pacman -S binutils base-devel
updpkgsums
makepkg-mingw -sCLf
```

If you get a problem with the public key during the packing, run this:

```
gpg --recv-key <KEYID>
gpg --lsign <KEYID>
```

After packing, run:

```
pacman -U mingw-w64-*-ffmpeg-*-any.pkg.tar.zst
```

When your happy with the ffmpeg build, let's do the mpv build.
We want *shared* library so you need to change the PKGBUILD here as well.
CD to *mingw-w64-mpv*, then add the line below (before *-Dlibmpv=true*).

```
-Ddefault_library=shared \
```
Here are my [mingw-w64-mpv-pkgbuild](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/mingw-w64-mpv/PKGBUILD) configuration file.

After packing, according to the same steps as for *ffmpeg* above, run:

```
pacman -U mingw-w64-*-mpv-*-any.pkg.tar.zst
```

Create an environmental variable called "PKG_CONFIG_PATH" and add the path "MSYS64_INSTALL_ROOT\mingw64\lib\pkgconfig". Also add the "MSYS64_INSTALL_ROOT\mingw64\bin to your environmental "Path".

## Option 2: Build mpv 0.36 and ffmpeg 5.1 with *m-ab-s*

This compilation takes much more time(as you are checking out all sources from git of ffmpeg dependencies), but has easier and more flexible configuration options.

This guide assumes you use [Media Build Suite](https://github.com/m-ab-s/media-autobuild_suite) to get a free setup of MSYS64 and the dependencies to build ffmpeg and mpv.

Run the batch script after unzipping it in a short path, but quit after MSYS has been setup (i.e. do not run the configure yet)

Run msys2 installed during "Media Build Suite" setup. Then install Jack package (see below).

```
pacman -S mingw-w64-x86_64-jack2
```

- Change the MPV git path from master to release 0.36 (latest one with bootstrap.py), by opening the file build\media-suite_deps.sh and replace in line 73 "https://github.com/mpv-player/mpv.git" with "https://github.com/mpv-player/mpv.git#release/0.36".

- My setup: Just copy over these [FFmpeg Options](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/ffmpeg_options.txt), [MPV Options](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/mpv_options.txt) and [Media Build Suite Configuration](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/media-autobuild_suite.ini) to the build folder. The latest FFmpeg 5.1 will be used in this configuration.
    - Alternatively, do not copy over these files, but during configuration, disable as many encoder as possible, as we want focus on decoding for the player. Also, choose to build ffmpeg with shared libs only, do not strip build files. When asked to edit the FFmpeg options file, add "--enable-libjack" to the configuration file.

- Run "Media Build Suite" batch script again, and do not remove the JACK package when asked. Continue and compile everything.

- Create an environmental variable called "PKG_CONFIG_PATH" and add the path "(Media Build Suite location)\local64\lib\pkgconfig". Also add the "(Media Build Suite location)\local64\bin-video" to your environmental "Path".

## Linking libmpv with MSVC programs

As according to the [Native compilation with MSYS2](https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md#native-compilation-with-msys2) you need to create a import library for the mpv DLL. Launch a Visual Studio command prompt (latest 2022 or later), and cd to the bin directory where *mpv-2.dll* is located. Then create the library with you need a definition file (*.def*). 

If you have a mpv.def file(*Option 2 normally gives that*), you can already do this:

```
lib /def:mpv.def /name:mpv-2.dll /out:mpv.lib /MACHINE:X64
```

If the build hasn't generated one (*Option 1 does not*), we need to generate one. Install *gendef*, as part of *-tools* and then generate the definition file.

```
pacman -S mingw-w64-x86_64-tools

gendef - MSYS64_INSTALL_ROOT/mingw64/bin/libmpv-2.dll > MSYS64_INSTALL_ROOT/mingw64/bin/libmpv.def
```
With the new *libmpv.def* file, generate the lib by running the command below in a Visual Studio 2022 Developer Command Prompt:

```
lib /def:libmpv.def /name:libmpv-2.dll /out:mpv.lib /MACHINE:X64
```