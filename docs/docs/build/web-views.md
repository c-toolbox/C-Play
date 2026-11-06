---
title: Web views (presenter, movie and layer view)
sidebar_position: 4
---

# Build the web views

The [presenter view, movie view and layer view](/remote-control/web-views) are web apps (Svelte + Vite) in `web/presentation_mode`, `web/movie_mode` and `web/layer_mode`, with the start page that links them in `web/views_mode`, served by the C-Play HTTP server. They are controlled by these CMake options:

| Option | Default | Description |
| --- | --- | --- |
| `BUILD_CPLAY_WEB_VIEWS` | ON | Builds the web apps and the parts of C-Play that serve them, and adds the *Presenter view*, *Movie view* and *Layer view* entries to the **Views** menu. With it OFF, neither is built; slide notes and thumbnails in presentations are kept in either build. |
| `BUILD_CPLAY_WITH_WEBVIEW` | ON | Opens the views in a C-Play window through Qt WebView. Turned off automatically when `BUILD_CPLAY_WEB_VIEWS` is OFF or Qt WebView is not found; the views then open in a browser app window (Edge or Chrome) instead. |
| `BUILD_CPLAY_WITH_WEBRTC` | OFF | Also enables the video pictures (on air and cue) in the movie view. |

## Building the web apps

Install [Node.js](https://nodejs.org) and [pnpm](https://pnpm.io) (for example `npm install -g pnpm`) and make sure `pnpm` is on the `PATH` when configuring CMake. When pnpm is found, the build runs `pnpm install --frozen-lockfile` and `pnpm run build` in each web app folder (targets `presentation_mode_web`, `movie_mode_web`, `layer_mode_web` and `views_mode_web`), and rebuilds them when their sources change. When pnpm is not found, CMake prints a message and you build them by hand:

```sh
cd web/presentation_mode
pnpm install
pnpm build
cd ../movie_mode
pnpm install
pnpm build
cd ../layer_mode
pnpm install
pnpm build
cd ../views_mode
pnpm install
pnpm build
```

The **INSTALL** target copies each `dist/` folder to `bin/data/web/presentation_mode`, `bin/data/web/movie_mode`, `bin/data/web/layer_mode` and `bin/data/web/views_mode`, where the HTTP server looks for them.

The menu that switches between the views is `src/components/ViewSwitcher.svelte`, copied into each web app; keep the copies the same.

To work on a web app, run `pnpm dev` in its folder: the Vite dev server proxies the API to a running C-Play (`CPLAY_URL`, default `http://127.0.0.1:7007`). `pnpm check` runs svelte-check and TypeScript.

## Qt WebView

Install Qt WebView through Craft (`libs/qt6/qtwebview`, see the [Craft Guide](/build/craft)). On Windows it runs on Qt WebEngine, so with `CPLAY_INSTALL_DEPENDENCIES` the INSTALL target also copies `QtWebEngineProcess.exe`, the WebEngine resources (to `bin/resources`) and `qtwebengine_locales` (to `bin/translations`). Without these files the C-Play window stays black.

The Windows deployment also installs `bin/qt.conf` so Qt finds the packaged plugins, QML modules, helper process, resources and locales after the folder is moved. WebView/WebEngine DLLs loaded by plugins are copied explicitly: scanning `C-Play.exe` alone does not find every dependency. In particular, newer Qt builds need `Qt6WebViewQuick.dll` and `Qt6WebEngineQuickDelegatesQml.dll` alongside the other Qt DLLs. Keep `CPLAY_INSTALL_DEPENDENCIES` enabled when refreshing a distribution, and test with Craft and other development directories removed from `PATH`; otherwise missing DLLs can be supplied by the development environment.

## Movie view pictures

The pictures in the movie view need a build with `BUILD_CPLAY_WITH_WEBRTC`, and at runtime `ffmpeg.exe` with libx264 next to `C-Play.exe` (or on the `PATH`). The deploy step copies the `ffmpeg.exe` found beside the mpv library. UDP ports 8891–8990 of the master must be open for the devices that show pictures.
