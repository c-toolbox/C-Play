# C-Play movie view

Web app for running a C-Play playlist (`*.cplaylist`) and the sections of its files. It shows the
playlist, what is on air (item, section, position, time left in the section) with a timeline of
the sections, and the transport (play/pause, ±10 s, previous/next section, stop).

It has two modes:

- **Live**: the view follows C-Play, and every action goes to C-Play at once: clicking an item
  loads it, clicking a section or the timeline jumps there.
- **Prepare**: C-Play no longer follows the view. Picking an item, a section or a time (timeline
  or *Start at*) builds a cue in the view only, while the on air panel keeps showing what C-Play
  plays (play/pause stays available). **Take → Live** (or the Live button) sends the cue to C-Play
  in one command, which loads the item if needed and goes to the section and time once the file
  is loaded, then plays, pauses or leaves it to C-Play (*Then*). Only what was changed is sent, so
  going back to Live without changes does nothing. *Discard* starts the cue over from what is on air.

The mode and the prepared cue are remembered on the device across reloads.

## Transition

The buttons at the top go from what plays to the next thing smoothly:

1. **Fade out** the on air picture (the bar left of it shows how visible it is).
2. **Load cue** in prepare mode, **Load next** (the next playlist item) when live, paused. In prepare
   mode the cue is on air then, so the next one can be prepared from it.
3. **Play & fade in**.

**Transition** runs the three in a row: it waits until the picture is faded out (C-Play's fade time)
and the item is loaded, then plays and fades in. Clicking it again stops it. The sound fades with
the picture when C-Play syncs the image and volume fades, as it does with its own fade buttons.
While it runs, the master slide is skipped and the background is black (color mode) instead of the
background image; both are put back as they were when it ends or is stopped.

## Pictures

When C-Play is built with WebRTC (`BUILD_CPLAY_WITH_WEBRTC`), **Pictures** shows video next to the
controls, received over WebRTC (H.264, never any sound). Without WebRTC in C-Play the button is not
there; a browser without WebRTC says so in place of the pictures. There is no other way to get them.

- **On air**: what the master plays, the video frame as mpv renders it (not the 3D view), at up to
  1280x720 and 25 frames per second.
- **Cue** (prepare mode): the cued item at the cued time, or where the cue timeline is dragged over.
  The master never loads it in its player: an ffmpeg process opens the file, seeks and sends that
  one frame. **Run picture** plays on from the cue in real time instead, until the cue changes.

C-Play encodes with `ffmpeg.exe` next to `C-Play.exe` (otherwise the one on the PATH), which needs
libx264, in processes below its own priority. A picture is only encoded while a device shows it.
The media goes over UDP ports 8891-8990 of the master (the WebRTC relay range), so those have to be
open in its firewall for the devices that show pictures.

C-Play serves it from `data/web/movie_mode` at `http://<master>:<port>/movie/`, where the port is
set in `data/http-server-conf.json` (default 7007, `"run": "yes"` needed). In C-Play, Views > Movie view
(also under the master view button in the header) opens it fullscreen on a chosen screen (a C-Play window with Qt WebView,
otherwise a browser app window), in the default browser, or copies the address for tablets or phones
on the network. It is built with the presenter view (CMake option `BUILD_CPLAY_WEB_VIEWS`).

## Build

```sh
pnpm install
pnpm build        # -> dist/, installed by CMake to bin/data/web/movie_mode
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
| GET | `/events?topics=movie,position,duration,pause[&since=&wait=]` | Snapshot, then long polling |
| GET | `/movie_state` | Playlist, sections of each item and of the loaded file, playing item and section |
| GET | `/position`, `/duration` | Seconds (polled when the event feed does not run) |
| POST | `/movie_cue?index=N&section=S&position=T&play=0\|1` | Loads item N unless loaded, then section S (-1 for none) and time T (seconds); left out parameters keep what C-Play has |
| POST | `/play`, `/pause`, `/stop`, `/seek?time=±s` | Transport |
| POST | `/fade_image_down`, `/fade_image_up` | Fades (the sound follows when C-Play syncs them); GET `/fade_duration` in seconds |
| GET | `/movie_preview` | `available` (built with WebRTC), and the last problem of each picture |
| POST | `/movie_preview/offer?feed=live\|prepare` | Body: SDP offer, ICE gathering complete. Answers `{id, sdp}` |
| POST | `/movie_preview/close?id=` | Ends a picture session |
| POST | `/movie_preview/prepare?index=N&position=T&play=0\|1` | The cue picture: item N (-1 the loaded file) at T seconds, playing on with play=1 |

Keyboard: Space = play/pause, ← → (Page Up/Down) = previous/next section (on air when live, of the
cue when preparing), ↑ ↓ = previous/next item of the cue, L = switch Live/Prepare (going live takes
the cue), Enter = take the cue, Esc = discard the cue, P = pictures on/off, R = run or hold the
cue picture, 1 2 3 = the transition steps, T = transition (or stop it), F = fullscreen (browser).
