# Build JACK with portaudio support on Windows

## Downloads
https://github.com/jackaudio/jack2/archive/refs/tags/v1.9.17.tar.gz

https://github.com/msys2/MINGW-packages/archive/refs/heads/master.zip

https://www.steinberg.net/asiosdk

https://github.com/notepad-plus-plus/notepad-plus-plus/releases/download/v7.9.5/npp.7.9.5.Installer.exe (for editing PKGBUILD and maintaing unix style end of lines)

## Pre-setups

This guide assumes you first run [Media Build Suite](https://github.com/m-ab-s/media-autobuild_suite) to get a free setup of MSYS64 and the dependencies to build ffmpeg.

## Onto how to do it
msys2 prompts preceded with ?
replace <UserName> for all paths with your login username

Run msys2 installed during "Media Build Suite" setup
```
pacman -S mingw-w64-x86_64-toolchain
?Enter a selection (default=all): - pick 3 mingw-w64-x86_64-gcc
?Proceed with installation? [Y/n] - enter Y
pacman -S patch autoconf make gettext-devel automake libtool pkg-config unzip
?Proceed with installation? [Y/n] - enter Y
pacman -S mingw-w64-x86_64-libsystre
?Proceed with installation? [Y/n] - enter Y
pacman -S mingw-w64-x86_64-db
?Proceed with installation? [Y/n] - enter Y
pacman -S mingw-w64-x86_64-libsndfile
?Proceed with installation? [Y/n] - enter Y
pacman -S mingw-w64-x86_64-libsamplerate
?Proceed with installation? [Y/n] - enter Y
```

extract MINGW-packages-master.zip to C:\msys64\home\<UserName>

extract asio sdk to C:\msys64\opt

rename asiosdk_2.3.3_2019-06-14 to asiosdkstatic clone that folder and rename clone to asiosdkshared

cd /home/<UserName>/MINGW-packages-master/mingw-w64-portaudio/

copy portaudio PKGBUILD (listed below) to C:\msys64\home\<UserName>\MINGW-packages-master\mingw-w64-portaudio

you can use notepad++ to chagne end of line style to unix : go to Edit menu -> EOL Conversion -> Unix (LF) and then save

```
MINGW_INSTALLS=mingw64 makepkg-mingw -sLf
?Proceed with installation? [Y/n] - enter Y
enter : pacman -U mingw-w64-x86_64-portaudio - press tab to auto complete
?Proceed with installation? [Y/n] - enter Y
```

```
cd ..
mkdir mingw-w64-jack2
cd mingw-w64-jack2
```

copy jack2-1.9.17.tar.gz to C:\msys64\home\<UserName>\MINGW-packages-master\mingw-w64-jack2\

copy jack PKGBUILD (listed below) to C:\msys64\home\<UserName>\MINGW-packages-master\mingw-w64-jack2

you can use notepad++ to chagne end of line style to unix : go to Edit menu -> EOL Conversion -> Unix (LF) and then save

will have to edit the source name in PKGBUILD for different versions of jack

copy required includes to correct include directory

```
cp /home/<UserName>/MINGW-packages-master/mingw-w64-portaudio/src/portaudio/include/pa_asio.h /mingw64/include/
cp /home/<UserName>/MINGW-packages-master/mingw-w64-portaudio/src/portaudio/include/portaudio.h /mingw64/include/
```

```
MINGW_INSTALLS=mingw64 makepkg-mingw -sLf
?Proceed with installation? [Y/n] - enter Y
?Proceed with installation? [Y/n] - enter Y
enter : pacman -U mingw-w64-x86_64-jack2 - press tab to auto complete
?Proceed with installation? [Y/n] - enter Y
```

copy over portaudio and jack results to bin/lib/include/pkg folders in media-autobuild_suite\msys64
So we can find these results when we run "Media Build Suite" again.