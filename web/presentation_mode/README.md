# C-Play presenter view

Web app for presenting a C-Play presentation (`*.cplaypres`). It shows the current and next slide
thumbnails, the slide notes (editable, saved in the presentation), a clock and timer, and moves
forward and backward through the slides on the C-Play master.

C-Play serves it from `data/web/presentation_mode` at `http://<master>:<port>/presenter/`, where the
port is set in `data/http-server-conf.json` (default 7007, `"run": "yes"` needed). In C-Play,
Views > Presenter view (also under the master view button in the header) opens it fullscreen on a
chosen screen. It uses a C-Play
window when C-Play is built with Qt WebView (`craft libs/qt6/qtwebview`), otherwise a browser app
window (Edge or Chrome). The same menu copies the address for tablets or phones on the network.

The presenter view is built when the CMake option `BUILD_CPLAY_WEB_VIEWS` is ON (default).
With it OFF, neither the web app nor the presenter parts of C-Play are built, and C-Play has no
Presenter view entry in the Views menu. Slide notes and thumbnails in presentations are kept in either build.

## Build

```sh
pnpm install
pnpm build        # -> dist/, installed by CMake to bin/data/web/presentation_mode
```

The CMake build runs these steps when pnpm is found (`BUILD_CPLAY_WEB_VIEWS`).

## Develop

```sh
pnpm dev          # proxies the API to a running C-Play (CPLAY_URL, default http://127.0.0.1:7007)
pnpm check        # svelte-check / TypeScript
```

## API used (C-Play HTTP server)

| Method | Path | |
| --- | --- | --- |
| GET | `/events?topics=presentation[&since=&wait=]` | Snapshot, then long polling of the presentation state |
| GET | `/presentation_state` | The presentation state (slides, notes, current/next) |
| GET | `/slide_thumbnail?index=N[&rev=R]` | Thumbnail JPEG of slide N |
| POST | `/slide_notes?index=N` | Sets the notes of slide N (UTF-8 text body) |
| POST | `/slide_thumbnail_update[?index=N]` | Renders the shown slide with the master view camera (perspective, fisheye or sphere) as its thumbnail |
| POST | `/presentation_save` | Saves the presentation to its file |
| POST | `/slide_next`, `/slide_previous`, `/load_from_slides?index=N` | Navigation |

Keyboard: → ↓ Page Down Space Enter = next, ← ↑ Page Up Backspace = previous, Esc = follow the
current slide again, F = fullscreen (browser).
