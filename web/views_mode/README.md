# C-Play web views start page

The page that links the web views of C-Play, so one address is enough: the
[presenter view](../presentation_mode) (`/presenter/`), the [movie view](../movie_mode) (`/movie/`)
and the [layer view](../layer_mode) (`/layerview/`). Each card shows what C-Play shows now (the
slide on air, the file playing, the slide selected in C-Play); click it or press 1, 2 or 3.

Every view has the same menu under the C-Play logo, top left (`src/components/ViewSwitcher.svelte`,
copied into each web app; keep the copies the same), to go to another view or back here. Opened in
the C-Play window (`?host=cplay`), the links keep that, so the close button still closes the window.

C-Play serves it from `data/web/views_mode` at `http://<master>:<port>/views/`, and `/` leads there.
In C-Play, Views > Web views start page opens it like the other views.

## Build

```sh
pnpm install
pnpm build        # -> dist/, installed by CMake to bin/data/web/views_mode
```

## Develop

```sh
pnpm dev          # proxies the API to a running C-Play (CPLAY_URL, default http://127.0.0.1:7007)
pnpm check        # svelte-check / TypeScript
```

The links go to `/<view>/`, which the dev server does not serve; try them against C-Play.

## API used (C-Play HTTP server)

| Method | Path | |
| --- | --- | --- |
| GET | `/events?topics=slides,triggered_slide,playing_in_slides,media_title,pause[&since=&wait=]` | What the cards show, long polled |
| GET | `/status` | Whether C-Play answers, when its event feed does not run |
