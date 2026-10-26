---
title: Presenter and movie views (web)
sidebar_position: 1.5
---

# Presenter view and movie view

*New in C-Play v2.4.*

C-Play includes two web interfaces that its [HTTP server](/remote-control/api) serves from the master:

* **Presenter view** (presentation mode) — for presenting a presentation (`*.cplaypres`) with slide thumbnails and speaker notes.
* **Movie view** (movie mode) — for running a playlist (`*.cplaylist`) and the sections of its files, with a cue that can be prepared before it goes live.

Both are open from **Views** in the menu bar. They can be shown fullscreen on a screen of the master, in the default browser, or on a tablet or phone on the same network.

## Requirements

* The HTTP server has to run: set `"run": "yes"` in `data/http-server-conf.json` (default). The port is set there too (default 7007). When the server is off, the view menus only say *The HTTP server is off (data/http-server-conf.json)*.
* The C-Play build has to include the web views (CMake option `BUILD_CPLAY_WEB_VIEWS`, on by default). See [Build from code → Web views](/build/web-views).
* The movie view shows video pictures only in builds with WebRTC (`BUILD_CPLAY_WITH_WEBRTC`). See [Pictures](#pictures) below.

| View | Address |
| --- | --- |
| Presenter view | `http://<master>:<port>/presenter/` |
| Movie view | `http://<master>:<port>/movie/` |

## Opening a view

The **Views** menu in the menu bar has one submenu per web view: *Presenter view (slides, notes)* and *Movie view (playlist, sections, cues)*. Each submenu has:

* **Open on &lt;screen&gt; (&lt;width&gt;x&lt;height&gt;)** — one entry per connected screen. Opens the view fullscreen on that screen. In builds with Qt WebView (`BUILD_CPLAY_WITH_WEBVIEW`) it opens in a C-Play window; otherwise in a browser app window without tabs or address bar (Edge or Chrome), or the default browser when no Chromium-based browser is found.
* **Open in default browser** — opens the view in a normal browser tab on the master.
* **Copy address for other devices** — lists the network addresses of the view on the master; select one to copy it to the clipboard and open it on a tablet or phone.

In the C-Play window, **F11** toggles fullscreen and **Ctrl+W** / **Ctrl+Q** closes it. In a browser, **F** toggles fullscreen.

## Presenter view

The presenter view shows the current and the next slide as thumbnails, the notes of the current slide, a clock and a timer. It moves forward and backward through the slides on the master.

* **Slide notes** are editable in the view and saved in the presentation file, together with the slide thumbnails. Notes and thumbnails are kept in presentations also by builds without the web views.
* **Thumbnails** — the thumbnail button of a slide uses what C-Play shows now as that slide's thumbnail, rendered with the [master view](/playback/views#-master-view) camera (perspective, fisheye or sphere view). Choose the master view mode before updating thumbnails.
* An *Unsaved* mark shows when the presentation has unsaved changes in C-Play; **Save** writes the notes and thumbnails to the `*.cplaypres` file the presentation was loaded from or last saved to.

| Key | Action |
| --- | --- |
| → ↓ Page Down Space Enter | Next slide |
| ← ↑ Page Up Backspace | Previous slide |
| Esc | Follow the current slide again |
| F | Fullscreen (browser) |

## Movie view

The movie view shows the playlist, what is on air (item, section, position and time left in the section) with a timeline of the sections, and the transport: play/pause, ±10 s, previous/next section and stop.

It has two modes:

* **Live** — the view follows C-Play, and every action goes to C-Play at once: clicking an item loads it, clicking a section or the timeline jumps there.
* **Prepare** — C-Play no longer follows the view. Picking an item, a section or a time (timeline or *Start at*) builds a **cue** in the view only, while the on-air panel keeps showing what C-Play plays (play/pause stays available). **Take → Live** (or the *Live* button) sends the cue to C-Play in one command: it loads the item if needed, goes to the section and time once the file is loaded, and then plays, pauses or leaves it to C-Play (*Then*). Only what was changed is sent, so going back to Live without changes does nothing. **Discard** starts the cue over from what is on air.

The mode and the prepared cue are remembered on the device across reloads.

When a new file loads, C-Play holds playback until all cluster nodes have it loaded (see [Starting playback on all nodes](/remote-control/api#starting-playback-on-all-nodes)). Meanwhile the on-air panel shows *Loading on nodes*, and **Play** and **Play & fade in** are disabled; hover them to see which nodes are still loading. **Transition** waits for the nodes after loading too, so the new file fades in only once every node has it.

### Pictures

In builds with WebRTC (`BUILD_CPLAY_WITH_WEBRTC`), **Pictures** shows video next to the controls, received over WebRTC (H.264, no sound). Without WebRTC in C-Play the button is not shown.

* **On air** — what the master plays: the video frame as mpv renders it (not the 3D view), at up to 1280x720 and 25 frames per second.
* **Cue** (prepare mode) — the cued item at the cued time, or where the cue timeline is dragged. The master never loads it in its player: an ffmpeg process opens the file, seeks and sends that one frame. **Run picture** plays on from the cue in real time instead, until the cue changes.

C-Play encodes with `ffmpeg.exe` next to `C-Play.exe` (otherwise the one on the `PATH`), which needs libx264, in processes below its own priority. A picture is only encoded while a device shows it. The media goes over UDP ports 8891–8990 of the master (the WebRTC relay range), so these must be open in its firewall for the devices that show pictures.

| Key | Action |
| --- | --- |
| Space | Play/pause |
| ← → (Page Up/Down) | Previous/next section (on air when live, of the cue when preparing) |
| ↑ ↓ | Previous/next item of the cue |
| L | Switch Live/Prepare (going live takes the cue) |
| Enter | Take the cue |
| Esc | Discard the cue |
| P | Pictures on/off |
| R | Run or hold the cue picture |
| F | Fullscreen (browser) |

## HTTP endpoints

The views use the [HTTP API](/remote-control/api) and the [event feed](/remote-control/api#change-subscriptions) (topics `presentation` and `movie`). The endpoints below are only available in builds with the web views and can also be used by other clients.

| Method | Path | Description |
| --- | --- | --- |
| GET | `/presentation_state` | The presentation state: slides, notes, current and next slide |
| GET | `/slide_thumbnail?index=N[&rev=R]` | Thumbnail JPEG of slide N |
| POST | `/slide_notes?index=N` | Sets the notes of slide N (UTF-8 text body) |
| POST | `/slide_thumbnail_update[?index=N]` | Renders the shown slide with the master view camera as its thumbnail |
| POST | `/presentation_save` | Saves the presentation to its file |
| GET | `/movie_state` | Playlist, sections of each item and of the loaded file, playing item and section |
| POST | `/movie_cue?index=N&section=S&position=T&play=0\|1` | Loads item N unless loaded, then section S (-1 for none) and time T (seconds); parameters left out keep what C-Play has |
| GET | `/movie_preview` | Whether pictures are available (built with WebRTC), and the last problem of each picture |
| POST | `/movie_preview/offer?feed=live\|prepare` | Body: SDP offer with ICE gathering complete. Answers `{id, sdp}` |
| POST | `/movie_preview/close?id=` | Ends a picture session |
| POST | `/movie_preview/prepare?index=N&position=T&play=0\|1` | The cue picture: item N (-1 for the loaded file) at T seconds, playing on with `play=1` |
