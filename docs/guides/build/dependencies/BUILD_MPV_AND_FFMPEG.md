# Build MPV and FFmpeg with JACK+portaudio+ASIO support on Windows

C-Play depends on the great *mpv* media player library, which indeed depends on the great *ffmpeg* library. It is usually straight-forward to build in many ways, but normally we want to modify the standard build by including JACK, to go from maximum 8-channel audio support, to support ASIO and other low-latency audio frameworks. Choose one of the two below options (after the pre-setup). After building, check further below for creating a linkable library.

## Pre-setup: Install JACK + portaudio (needed NDI audio) with vcpkg

In the options below, we will be compiling FFMPEG and MPV with MINGW64, which is a standard (and easiest) practise to build a customized library on Windows. But MINGW64 headers are extensive and the include folder is making compiliation fail (often at least). As we are also using the great 'vcpkg' functionality for many packages, we can utilize this as well to install packages that we can include when building C-Play.

Install the [ASIO SDK](https://www.steinberg.net/asiosdk) through 'vcpkg', and then install portaudio with the asio feature enabled. In the C-Play repository, there is some [portaudio vcpkg files](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/portaudio) to make the 'portaudio' compile with 'asio', which is not in the standard package included in 'vcpkg'. At last, install 'jack2' as well.

```
vcpkg install asiosdk
vcpkg install portaudio[asio]:x64-windows
vcpkg install jack2
```

## Option 1: Build mpv and ffmpeg from MINGW-packages

As of writing (2025-09-01) this is mpv 0.40 and ffmpeg 7.1.1.

This is easiest if you want the latest libraries. However, Option 2 has more control, and a such can maximize performance in C-Play for specific configurations.

Here are the steps:

Download MSYS from [official website](https://www.msys2.org/).
Install and the run *mingw64.exe*.

Then clone "https://github.com/msys2/MINGW-packages" into your mingw64 home folder.

```
git clone "https://github.com/msys2/MINGW-packages"
```

### 1.1 (Optional) Build JACK PortAudio with ASIO support for the MINGW64 environment

Install the [ASIO SDK](https://www.steinberg.net/asiosdk) through 'vcpkg' or download it (se pre-setup).

- Create an environmental variable called "ASIOSDK_ROOT_DIR" and add the path "C:\vcpkg\installed\x64-windows\include\asiosdk" if located in standard vcpk paths.

* Notice: If you running the "Media Build Suite", all the packages in that MSYS64 environment can cause this option to fail the build. Recommended to close the MSYS2/MINGW64 environment at first launch after pressing 'media-autobuild_suite.bat' so you can run this option with fresh environment. The continue on the "Media Build Suite" setup after this option is completed, but do not remove the installed packages that you built/installed in this setup.

Then clone "https://github.com/msys2/MINGW-packages" into your mingw64 home folder.
Launch 'MINGW64' environment and go the the portaudio package.

```
cd MINGW-packages/mingw-w64-portaudio
```

edit the PKGBUILD file, to make sure the cmake configuration part has this:

```
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DASIOSDK_ROOT_DIR=${ASIOSDK_ROOT_DIR} \
  -DASIOSDK_INCLUDE_DIR=${ASIOSDK_ROOT_DIR} \
```
The run these commands to build and install jack2 + portaudio with the ASIO support configured.

```
pacman -S binutils base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-ccache mingw-w64-x86_64-meson mingw-w64-x86_64-toolchain
updpkgsums
makepkg-mingw -sCLf
pacman -U mingw-w64-*-portaudio-*-any.pkg.tar.zst
```

Then let's update the jack2 package to use our custom portaudio package.

```
cd MINGW-packages/mingw-w64-jack2
updpkgsums
makepkg-mingw -sCLf
pacman -U mingw-w64-*-jack2-*-any.pkg.tar.zst
```

### 1.2 Build FFmpeg with custom options (supporting jack2, nvdec etc)

FFmpeg is highly customizable, but here are the preferred settings for using it with C-Play.

```
cd MINGW-packages/mingw-w64-ffmpeg
```

Edit the PKGBUILD file to include *${MINGW_PACKAGE_PREFIX}-jack2* and *--enable-libjack*. Also preferred to include missing *--enable-nvdec* after *--enable-nvenc*.
Here are my [mingw-w64-ffmpeg-pkgbuild](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/mingw-w64-ffmpeg/PKGBUILD) configuration file.

Then install dependencies for packing and perform the new packing:

```
pacman -S binutils base-devel mingw-w64-x86_64-gcc
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

### 1.3 Build MPV with custom options.

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

Create an environmental variable called "PKG_CONFIG_PATH" and add the path "MSYS64_INSTALL_ROOT\ucrt64\lib\pkgconfig". Also add the "MSYS64_INSTALL_ROOT\ucrt64\bin to your environmental "Path".

Go to point 3 (last section) below to create library for compiling.

## Option 2: Build custom mpv and ffmpeg with *m-ab-s*

As of writing (2025-09-01) this build setup includes mpv 0.38 and then option to specify and ffmpeg version under that.

This compilation takes much more time(as you are checking out all sources from git of ffmpeg dependencies), but has easier and more flexible configuration options.

This guide assumes you use [Media Build Suite](https://github.com/m-ab-s/media-autobuild_suite) to get a free setup of dependencies to build ffmpeg and mpv.

Run the batch script after unzipping it in a short path, but quit after MSYS has been setup (i.e. do not run the configure yet).

* Option: Perform step 1.1 above to include ASIO into portaudio, and build Jack2*

If not doing option 1.1, simply install jack2 (see below), after MINGW64 has installed other packages during "Media Build Suite" setup.

```
pacman -S mingw-w64-x86_64-jack2
```

- Change the MPV git path from master to release 0.38 (latest one with bootstrap.py), by opening the file build\media-suite_deps.sh and replace in line 73 "https://github.com/mpv-player/mpv.git" with "https://github.com/mpv-player/mpv.git#release/0.38".

- My setup: Just copy over these [FFmpeg Options](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/ffmpeg_options.txt), [MPV Options](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/mpv_options_new.txt) and [Media Build Suite Configuration](https://raw.githubusercontent.com/c-toolbox/C-Play/master/help/configurations/gplv3/media-autobuild_suite.ini) to the build folder. The latest FFmpeg 5.1 will be used in this configuration.
    - Alternatively, do not copy over these files, but during configuration, disable as many encoder as possible, as we want focus on decoding for the player. Also, choose to build ffmpeg with shared libs only, do not strip build files. When asked to edit the FFmpeg options file, add 'jack' to the configuration file.
```
# Enable JACK
--enable-libjack
```
    - For the 'mpv_options.txt' the important part to add is this:
```
# Build libmpv
-Ddefault_library=shared
-Dlibmpv=true
```

- Run "Media Build Suite" batch script again, and do not remove the JACK package when asked. Continue and compile everything.

- Notice: If ffmpeg build fails it most likely because there are new packages of dependicies that has changed and made the build fail. If you are lucky, you can download an old one and try again. Below is an example: 

```
wget https://repo.msys2.org/mingw/mingw64/mingw-w64-x86_64-texinfo-7.1.1-2-any.pkg.tar.zst
pacman -U mingw-w64-x86_64-texinfo-7.1.1-2-any.pkg.tar.zst
```

- Create an environmental variable called "PKG_CONFIG_PATH" and add the path "(Media Build Suite location)\local64\lib\pkgconfig". Also add the "(Media Build Suite location)\local64\bin-video" to your environmental "Path".

## 3. After MPV compiliation, linking mpv with MSVC programs

As according to the [Native compilation with MSYS2](https://github.com/mpv-player/mpv/blob/master/DOCS/compile-windows.md#native-compilation-with-msys2) you need to create a import library for the mpv DLL. Launch a Visual Studio command prompt (latest 2022 or later), and cd to the bin directory where *mpv-2.dll* is located. Then create the library with you need a definition file (*.def*). 

If you have a mpv.def file(*Option 2 normally gives that*), you can already do this:

```
lib /def:mpv.def /name:mpv-2.dll /out:mpv.lib /MACHINE:X64

or

lib /def:libmpv.def /name:libmpv-2.dll /out:mpv.lib /MACHINE:X64
```

If the build hasn't generated one (*normally it does not*), we need to generate one. Install *gendef*, as part of *-tools* and then generate the definition file (From the VS prompt).

```
pacman -S mingw-w64-x86_64-tools

cd M-AB-S/local64/bin-video
gendef - mpv-2.dll > mpv.def

or

cd MSYS64_INSTALL_ROOT/mingw64/bin/
gendef - libmpv-2.dll > libmpv.def
```
With the new *libmpv.def* file, generate the lib by running the command below in a Visual Studio 2022 Developer Command Prompt:

```
lib /def:mpv.def /name:mpv-2.dll /out:mpv.lib /MACHINE:X64

or

lib /def:libmpv.def /name:libmpv-2.dll /out:mpv.lib /MACHINE:X64
```
### 3.1 Library creation for JACK2
If you compiled and included 'jack2', you might need to go through the same process with 'libjack64.dll' as stated in point above to create a library.

As we have compiled PortAudio with asio support using the vcpkg in the 'pre-setup', we do not need to create a library with MINGW64. But the JACK2 within 'vcpkg' does not seem to have 'asio' support in there, so we want to stick with the MINGW64 version of JACK2.
