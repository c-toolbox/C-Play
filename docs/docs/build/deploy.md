---
title: Deploy C-Play with all dependencies
sidebar_position: 3
---

# Deploy C-Play and build an installer

After building C-Play, install it into a separate staging directory with the Qt/KF, mpv, plugin, QML, and optional-library dependencies it needs. Use a staging directory separate from your source, dependency installations, and existing show installation.

## Configure installation

Set `CMAKE_INSTALL_PREFIX` to the staging directory and enable `CPLAY_INSTALL_DEPENDENCIES`. Reconfigure CMake to expose the dependency-copy options:

| Option | What it copies |
| --- | --- |
| `CPLAY_INSTALL_ITEMS_TO_DATA` | Selected application data, including configuration files, fonts, and media-related resources. |
| `CPLAY_INSTALL_PLUGINS_FROM_CRAFT` | Qt/KDE plugin DLLs from the Craft installation. |
| `CPLAY_INSTALL_QML_FROM_CRAFT` | QML modules and their runtime files from Craft. |
| `CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES` | DLLs found through CMake's runtime dependency scanner. |
| `CPLAY_INSTALL_DLLS_FROM_PATHS` | DLLs from the directories in `CPLAY_INSTALL_DLLS_FROM_PATH_LIST`, processed in list order. |

The current build does not provide `CPLAY_INSTALL_ITEMS_FROM_DATA` or `CPLAY_INSTALL_PREDEFINED_DLL_LIST`; use the options above.

The source recommends copying DLLs from configured paths on Windows 11, where runtime dependency scanning has caused conflicts. Review `CPLAY_INSTALL_DLLS_FROM_PATH_LIST` for your dependency locations, especially when optional backends are enabled. Avoid mixing DLLs from incompatible versions or build configurations.

When available beside the detected mpv library, `mpv.exe` and `ffmpeg.exe` are copied automatically. A detected yt-dlp executable and its optional `_internal` directory are installed under `bin/plugins/yt-dlp`.

## Install and verify the staged application

Run the **INSTALL** target in Visual Studio, or use the matching configuration from the command line:

```powershell
cmake --build build --config Release
cmake --install build --config Release
```

Replace `build` with your CMake build directory. The staged executable and runtime files are placed under `CMAKE_INSTALL_PREFIX/bin`.

Launch from that `bin` directory so relative `data/` paths resolve correctly. Test startup, file playback, audio, QML dialogs, and each optional input used in your show. Also test on a machine without the development environment's dependency directories on `PATH`.

Windows keeping a DLL locked while C-Play runs is not a complete dependency check: delayed plugins and codecs may only load when their features are used. If reducing package size, work on a copy of the staging directory and retest every required feature after removing files.

## Build an installer

1. Install NSIS on the packaging machine.
2. Run **INSTALL** to populate the staging directory. It also copies the generated `CPlay_Pack_Installer.nsi` and `CPlay_Pack_Zip.bat` scripts to its root.
3. Compile `CPlay_Pack_Installer.nsi` with NSIS from the staging directory, where `bin/` and `LICENSE.txt` are available. The script packages the staged `bin/` tree.
4. Test the installer on a separate machine before distributing it to the cluster.

The current NSIS script does not launch a Visual C++ Redistributable installer. Ensure the target machines have the runtime required by your MSVC build; placing `vc_redist.x64.exe` in `bin/` only includes that file in the package. If you change the installer to install the runtime, test that step as part of packaging.

For a ZIP distribution, use the generated `CPlay_Pack_Zip.bat` script and verify the extracted application in the same way.
