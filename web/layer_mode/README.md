# C-Play layer view

Web app for placing the layers of a C-Play presentation (`*.cplaypres`) without running it. It
lists the master and the slides with their layers. A flat layer (grid mode Plane) can be moved and
resized on a 2D canvas; every layer can be faded in or out, and layers with sound have a volume.
Changes go to C-Play at once, as if made in its layer grid parameters, and are saved with the
presentation (**Save**).

What the flat layers are moved on is chosen in C-Play under Views > Layer view > Move layers on:

| Canvas | Moves | Drawn as |
| --- | --- | --- |
| Fisheye (dome) | azimuth and elevation | the fisheye image of the master view (zenith in the centre, front down) |
| Flat | horizontal and vertical (cm) | a rectangle spanning the plane move ranges of the Grid settings |
| Sphere | azimuth and elevation | an equirectangular map of the whole sphere |
| Circular screen | degrees around and vertical (cm) | a ring of the circular surface mapping (Grid settings), its seam in red |

The outlines are computed from the plane values with the same transform as the C-Play renderer.

C-Play serves it from `data/web/layer_mode` at `http://<master>:<port>/layerview/` (not `/layers/`,
which is the layer list of the HTTP API), where the port is set in `data/http-server-conf.json`
(default 7007, `"run": "yes"` needed). In C-Play, Views > Layer view opens it fullscreen on a chosen
screen, in a C-Play window (Qt WebView) or a browser app window, and copies the address for tablets.

## Build

```sh
pnpm install
pnpm build        # -> dist/, installed by CMake to bin/data/web/layer_mode
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
| GET | `/events?topics=layerview[&since=&wait=]` | Snapshot, then long polling of the layer view state |
| GET | `/layerview_state` | Slides and their layers, the canvas and its settings |
| POST | `/layerview_layer?slide=S&row=R&...` | Sets `azimuth`, `elevation`, `roll`, `distance`, `horizontal`, `vertical`, `width`, `height`, `centreDegrees`, `widthDegrees` and/or `volume` (0-100) of a layer; answers the layer |
| POST | `/layerview_fade?slide=S&row=R&on=0\|1` | Fades a layer in or out, as double-clicking it in C-Play |
| POST | `/presentation_save` | Saves the presentation to its file |

Touch: drag a layer with one finger, pinch it with two to resize it; a finger off the layers scrolls
the page.

Keyboard: arrows = move the selected layer (Shift: 10x), + and − = resize, Page Up/Down = previous or
next slide, Esc = deselect, F = fullscreen (browser).
