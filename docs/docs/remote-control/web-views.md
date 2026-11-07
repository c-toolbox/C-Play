---
title: Presenter, movie and layer views (web)
sidebar_position: 1.5
---

# Presenter view, movie view and layer view

*New in C-Play v2.4.*

C-Play includes three web interfaces that its [HTTP server](/remote-control/api) serves from the master:

* **Presenter view** (presentation mode) — for presenting a presentation (`*.cplaypres`) with slide thumbnails and speaker notes.
* **Movie view** (movie mode) — for running a playlist (`*.cplaylist`) and the sections of its files, with a cue that can be prepared before it goes live.
* **Layer view** — for placing the layers of the slides on a fisheye, flat, sphere or circular canvas, and setting their volume and visibility.

All are open from **Views** in the menu bar. They can be shown fullscreen on a screen of the master, in the default browser, or on a tablet or phone on the same network.

A **start page** links the three, so one address is enough: `http://<master>:<port>/` leads to it. Every view has a menu under the C-Play logo, top left, to go to another view or back to the start page. See [Start page and switching views](#start-page-and-switching-views).

## Requirements

* The HTTP server has to run: set `"run": "yes"` in `data/http-server-conf.json` (default). The port is set there too (default 7007). When the server is off, the view menus only say *The HTTP server is off (data/http-server-conf.json)*.
* The C-Play build has to include the web views (CMake option `BUILD_CPLAY_WEB_VIEWS`, on by default). See [Build from code → Web views](/build/web-views).
* The movie view shows video pictures only in builds with WebRTC (`BUILD_CPLAY_WITH_WEBRTC`). See [Pictures](#pictures) below.

| View | Address |
| --- | --- |
| Start page | `http://<master>:<port>/views/` (also `http://<master>:<port>/`) |
| Presenter view | `http://<master>:<port>/presenter/` |
| Movie view | `http://<master>:<port>/movie/` |
| Layer view | `http://<master>:<port>/layerview/` |

## Opening a view

The **Views** menu in the menu bar has one submenu per web view: *Web views start page (links to all views)*, *Presenter view (slides, notes)*, *Movie view (playlist, sections, cues)* and *Layer view (position, size, volume)*. Each submenu has:

![Views menu: Presenter view submenu](/assets/ui/views_presenter.png)


* **Open on &lt;screen&gt; (&lt;width&gt;x&lt;height&gt;)** — one entry per connected screen. Opens the view fullscreen on that screen. In builds with Qt WebView (`BUILD_CPLAY_WITH_WEBVIEW`) it opens in a C-Play window; otherwise in a browser app window without tabs or address bar (Edge or Chrome), or the default browser when no Chromium-based browser is found. The screen the view is open on is checked, and the view menu title ends with **- open**. Opening it on another screen moves it there.
* **Close** — closes the view opened on a screen (the C-Play window or the browser app window).
* **Open in default browser** — opens the view in a normal browser tab on the master.
* **Copy address for other devices** — lists the network addresses of the view on the master; select one to copy it to the clipboard and open it on a tablet or phone.
* **Move layers on** (layer view only, first in its submenu) — what the layer view moves the flat layers on: *Fisheye (dome)*, *Flat*, *Sphere* or *Circular screen*. The current choice is shown in its title, for example *Move layers on: Fisheye (dome)*. See [What layers are moved on](#what-layers-are-moved-on).

In the C-Play window, **F11** toggles fullscreen and **Ctrl+W** / **Ctrl+Q** closes it. In a browser, **F** toggles fullscreen.

## Start page and switching views

The start page shows a card per view: **Presenter**, **Movie** and **Layers**, each with what C-Play shows now (the slide on air, the file playing, the slide selected in C-Play). Click a card, or press **1**, **2** or **3**, to open that view.

![Web views start page](/assets/ui/web_start_page.png)

In every view, the C-Play logo top left is a menu with the name of the view: it lists **All views** (the start page) and the three views, the current one marked. Choosing one opens it in the same window or tab, so a tablet only needs the address of the start page. A view opened from C-Play in its window stays in that window when you switch, and its close button still closes it. The menu of C-Play still tells which view the window was opened with.

![The view menu under the C-Play logo](/assets/ui/web_view_menu.png)

### Several clusters

With more than one cluster running C-Play (for example two domes), name each cluster and list the others in `data/http-server-conf.json` of each master:

```json
{
    "run": "yes",
    "port": 7007,
    "clusterName": "Dome A",
    "clusters": [
        { "name": "Dome A", "url": "http://10.0.1.10:7007/" },
        { "name": "Dome B", "url": "http://10.0.2.10:7007/" },
        { "name": "Mobile dome", "url": "http://10.0.3.10:7007/" }
    ]
}
```

The start page then shows the cluster name in its header and in the tab title (*C-Play – Dome A*), so tabs and bookmarks of several clusters can be told apart, and links to the start pages of the other clusters below the cards. The menu under the logo in every view has the name of the cluster on top and the other clusters at the bottom. The list can be the same on every master: the entry with the name of the cluster itself is left out. A `url` without a path opens the start page (`/views/`) of that cluster. Restart C-Play after changing the file.

## On phones

Every view also works on a phone, upright or turned sideways: the page becomes one column that scrolls, and the header takes less room (the connection state is shown as a dot only; its tooltip still says it).

* **Presenter view** — **Previous** and **Next** stay at the bottom of the screen while you scroll through the slides and notes. The notes start at a smaller text size on a phone; **A−** / **A+** still change it, remembered per device.
* **Movie view** — the playlist and cue list show all their items in the page instead of in a box of their own, and the volume slider gets a row of its own above **Fade out** and **Fade in**.
* **Layer view** — drag a layer with one finger to move it, and pinch it with two fingers (the first one on the layer) to resize it. A finger on the canvas outside the layers scrolls the page.

<div style={{display: 'flex', gap: '0.5rem', alignItems: 'flex-start'}}>
  <figure style={{flex: '1 1 0', margin: 0, textAlign: 'center'}}><img src={require('@site/static/assets/ui/phone_start_page.png').default} alt="Start page on a phone" style={{width: '100%'}} /><figcaption>Start page</figcaption></figure>
  <figure style={{flex: '1 1 0', margin: 0, textAlign: 'center'}}><img src={require('@site/static/assets/ui/phone_presenter.png').default} alt="Presenter view on a phone" style={{width: '100%'}} /><figcaption>Presenter view</figcaption></figure>
  <figure style={{flex: '1 1 0', margin: 0, textAlign: 'center'}}><img src={require('@site/static/assets/ui/phone_movie.png').default} alt="Movie view on a phone" style={{width: '100%'}} /><figcaption>Movie view</figcaption></figure>
  <figure style={{flex: '1 1 0', margin: 0, textAlign: 'center'}}><img src={require('@site/static/assets/ui/phone_layer_view.png').default} alt="Layer view on a phone" style={{width: '100%'}} /><figcaption>Layer view</figcaption></figure>
</div>

![Presenter view on a phone turned sideways](/assets/ui/phone_presenter_landscape.png)

## Presenter view

The presenter view shows the current and the next slide as thumbnails, the notes of the current slide, a clock and a timer. It moves forward and backward through the slides on the master.

![Presenter view](/assets/ui/presenter_view.png)

* **Header** — the presentation name (with an *Unsaved* mark when C-Play has unsaved changes), the clock, the presentation timer with start/pause and reset (it starts at zero, paused, each time C-Play starts, keeps counting when the page is reloaded, and starts by itself with the first **Next**), the connection state (*Live*), **Recent ▾** (see [Opening a recent presentation](#opening-a-recent-presentation)), **Save**, a light/dark theme toggle and fullscreen.
* **Current** — the slide shown in C-Play now, with **Update thumbnail** to replace its thumbnail. Before the first slide is triggered it says *Not started, Next shows the first slide*.
* **Next** — the slide that **Next →** triggers. **Skip slide** marks it as skipped, and Next then shows the slide after it.
* **Notes** — the notes of the current slide (or of the next slide before the presentation has started). **A−** / **A+** change the text size; *Saved to slide* shows that an edit has reached C-Play.
* **Slide strip** — every slide with its thumbnail (or its number when it has none yet). Click one to read or edit its notes; **Go to slide** then triggers it and **Back to current** follows the current slide again. Each slide has a **Skip** / **Include** toggle in its top left corner (shown on hover, always on touch screens).

* **Skipped slides** — choose which slides to present by marking the others as skipped, in the slide strip or with **Skip slide** / **Include slide** above the notes of a slide picked in the strip. Skipped slides are dimmed and marked *Skipped*; **Next** and **Previous** pass over them, as they do in C-Play. This is the same skip setting as in the C-Play slide list, so it is saved with the presentation, and the view shows changes made in C-Play. Go to slide still triggers a skipped slide.

* **Slide notes** are editable in the view and saved in the presentation file. The slide thumbnails are numbered JPEG files in a `cplaypres_thumbnails` folder next to the presentation file, or where the [presentation thumbnail location](/settings/location#presentation-thumbnail-location) says, and the presentation file refers to them. Notes and thumbnails are kept in presentations also by builds without the web views.
* **Thumbnails** — the thumbnail button of a slide uses what C-Play shows now as that slide's thumbnail, rendered with the [master view](/playback/views#-master-view) camera (perspective, fisheye or sphere view). Choose the master view mode before updating thumbnails.
* An *Unsaved* mark shows when the presentation has unsaved changes in C-Play; **Save** writes the notes and the references to the thumbnails to the `*.cplaypres` file the presentation was loaded from or last saved to.

### Opening a recent presentation

**Recent ▾** in the header lists the presentations recently loaded in C-Play, the newest first, the same list as *File → Open Recent Presentations* on the master. Each entry shows the file name with its full path below. Select one and confirm to open it in C-Play; the confirmation warns when the open presentation has unsaved changes. C-Play then loads it as from its own menu, and the view follows the new presentation. Esc or a click outside closes the list.

Only presentations that are in the recent list can be opened from the view, so a device on the network cannot open other files on the master.

| Key | Action |
| --- | --- |
| → ↓ Page Down Space Enter | Next slide |
| ← ↑ Page Up Backspace | Previous slide |
| Esc | Follow the current slide again |
| F | Fullscreen (browser) |

## Movie view

The movie view shows the playlist, what is on air (item, section, position and time left in the section) with a timeline of the sections, and the transport: play/pause, ±10 s, previous/next section and stop.

The on-air panel also has a **Volume** slider (0–100%) with **Fade out** and **Fade in** buttons at its ends (as in C-Play, **Fade in** goes back to the level before the fade out, and pressed again up to 100; the picture fades too when C-Play syncs the image and volume fades), and below it an **Audio file** selector for the audio tracks and external audio files already loaded with the movie, like the desktop **Audio File** button. These controls follow changes in C-Play and affect the on-air audio immediately in both Live and Prepare mode.

![Movie view in Live mode, with the cue list](/assets/ui/movie_view_live.png)

It has two modes:

* **Live** — the view follows C-Play, and every action goes to C-Play at once: clicking an item loads it, clicking a section or the timeline jumps there.
* **Prepare** — C-Play no longer follows the view. Picking an item, a section, a time (timeline or *Start at*) or an **Audio file** builds a **cue** in the view only, while the on-air panel keeps showing what C-Play plays (play/pause stays available). **Take → Live** (or the *Live* button) sends the cue to C-Play in one command: it loads the item if needed, selects the audio, goes to the section and time once the file is loaded, and then plays, pauses or leaves it to C-Play (*Then*). Only what was changed is sent, so going back to Live without changes does nothing. **Discard** starts the cue over from what is on air.

The cue's **Audio file** selector lists the audio tracks and external audio file of the cued item, also before it is loaded (C-Play opens the file silently to read them). *Default audio* leaves the choice to C-Play.

The cue timeline works for files added to the playlist directly too: C-Play reads their duration the same way. When the playlist sets a start or end time for the item, a cue starts at the start time and stays between the two.

![Movie view in Prepare mode, with pictures](/assets/ui/movie_view_prepare.png)

The mode and the prepared cue are remembered on the device across reloads.

When a new file loads, C-Play holds playback until all cluster nodes have it loaded (see [Starting playback on all nodes](/remote-control/api#starting-playback-on-all-nodes)). Meanwhile the on-air panel shows *Loading on nodes*, and **Play** and **Play & fade in** are disabled; hover them to see which nodes are still loading. **Transition** waits for the nodes after loading too, so the new file fades in only once every node has it.

### Loading a recent playlist or media file

**Recent ▾** in the header (next to the connection state) lists the playlists and the media files recently loaded in C-Play, in two groups with the newest first: the same lists as *File → Open Recent Playlists* and *File → Open Recent Media Files* on the master. Each entry shows the file name with its full path (or the URL of a stream) below. Select one and confirm to load it in C-Play; it replaces what is loaded, so this is best done between shows or while the picture is faded out. A playlist replaces the playlist in the view. A prepared cue refers to playlist items by their position, so check it in Prepare mode after loading another playlist (it starts over from what is on air when its item no longer exists). The button is disabled while a [transition](#transition) runs.

Only files that are in the recent lists can be loaded from the view, so a device on the network cannot open other files on the master.

### Transition

The buttons at the top go from what plays to the next thing smoothly:

1. **Fade out** — fades the on-air picture out. The bar left of the button shows how visible it is.
2. **Load cue** (prepare mode) or **Load next** (live mode, the next playlist item) — loads it paused. In prepare mode the cue is on air afterwards, so the next one can be prepared from it.
3. **Play & fade in** — plays and fades the picture in.

**Transition** runs the three steps in a row: it waits until the picture has faded out (C-Play's fade time) and the item is loaded, then plays and fades in. Click it again to stop it. The sound fades with the picture when C-Play syncs the image and volume fades, as it does with its own fade buttons. While a transition runs, the master slide is skipped and the background is black (background color mode) instead of the background image; both are restored when it ends or is stopped.

### Cue list

Below the playlist is a list of cues that are fired one after the other, for example with a presentation clicker. Each cue is a playlist item (found by its file name, so reordering the playlist does not break it), a section and/or a time, an audio track, what to do then (*As C-Play*, *Play* or *Pause*), and how it goes on air: **Cut** (at once) or **Fade** (the [transition](#transition): fade out, load, play & fade in).

* **+ Add cue** inserts the prepared cue after the cue on standby. Without a prepared cue (or in live mode), **+ Add on air** adds the item, section, time and audio that play now. **Update cue N** replaces the standby cue with the prepared cue.
* Click a cue to put it on standby (marked *NEXT*). In prepare mode it also becomes the prepared cue, so the cue picture shows it. The standby cue can be renamed, set to *Cut* or *Fade*, given a *Then* action and an audio file, moved up or down, or deleted.
* **GO ▶** (Page Down or G) fires the standby cue and puts the next one on standby. **◀** (Page Up) puts the previous cue on standby without firing it. With cues in the list, Page Up/Down run the cue list instead of stepping sections; the arrow keys still step sections.
* **Save** writes `<name>.cplaycue` (JSON) to the cue list location in [Location settings](/settings/location), or else to the folder of the open playlist (or the C-Play file location). **Open** lists the cue lists there, and **New** starts an empty list. The list is also kept on the device.

### Pictures

In builds with WebRTC (`BUILD_CPLAY_WITH_WEBRTC`), **Pictures** shows video next to the controls, received over WebRTC (H.264, no sound). Without WebRTC in C-Play the button is not shown.

* **On air** — what the master plays: the video frame as mpv renders it (not the 3D view), at up to 1280x720 and 25 frames per second.
* **Cue** (prepare mode) — the cued item at the cued time, or where the cue timeline is dragged. The master never loads it in its player: an ffmpeg process opens the file, seeks and sends that one frame. **Run picture** plays on from the cue in real time instead, until the cue changes.

C-Play encodes with `ffmpeg.exe` next to `C-Play.exe` (otherwise the one on the `PATH`), which needs libx264, in processes below its own priority. A picture is only encoded while a device shows it. The media goes over UDP ports 8891–8990 of the master (the WebRTC relay range), so these must be open in its firewall for the devices that show pictures.

| Key | Action |
| --- | --- |
| Space | Play/pause |
| ← → (Page Up/Down without cues in the cue list) | Previous/next section (on air when live, of the cue when preparing) |
| ↑ ↓ | Previous/next item of the cue |
| L | Switch Live/Prepare (going live takes the cue) |
| Enter | Take the cue |
| Esc | Discard the cue |
| P | Pictures on/off |
| R | Run or hold the cue picture |
| 1 2 3 | The transition steps |
| T | Transition (or stop it) |
| Page Down / G | GO: fire the standby cue |
| Page Up | Put the previous cue on standby |
| F | Fullscreen (browser) |

## Layer view

The layer view is for preparing a presentation, not for running it: it lists the master and the slides with their layers, and places the layers. Picking a slide does not trigger it in C-Play. Each change goes to C-Play at once, as if made in the layer's grid parameters, and is saved with the presentation (**Save**).

![Layer view with a layer of the slide on air selected](/assets/ui/web_layer_view.png)

* **Slides** — the master and every slide, with the number of layers. *On air* marks the slide shown on the output, *C-Play* the slide selected in the C-Play slide list. The view starts on the slide selected in C-Play and follows it until another slide is picked.
* **Canvas** — the flat layers (grid mode *Plane*) of the slide, drawn where C-Play renders them. Drag a layer to move it, and scroll over the canvas to resize the selected layer (on a touch screen: pinch the layer with two fingers). Layers that are faded out have a dashed outline.
* **Selected layer** — azimuth, elevation, roll, distance, horizontal and vertical move, width and height as numbers, with move and *Smaller* / *Larger* buttons for touch screens. A size that follows the media aspect ratio (*Plane size based on* in the grid parameters) is not editable.
* **Layers** — every layer of the slide, top first, with its grid mode and type. **On** / **Off** fades the layer in or out as double-clicking it in C-Play does (with its entrance or exit effects); a partly visible layer shows its visibility in percent. Layers with sound have a **volume** slider (0–100). Layers kept visible from an earlier slide are marked *From &lt;slide&gt;*; changing them changes them in that slide.

Dome and sphere layers and 2D layers have no position on the canvas; their visibility and volume are set in the list.

### What layers are moved on

**Views → Layer view → Move layers on** in C-Play chooses the canvas, to match the display. The choice is saved in C-Play and the view follows it at once.

![Views menu: Layer view submenu](/assets/ui/views_layer_view.png)

| Canvas | Dragging sets | The canvas shows |
| --- | --- | --- |
| **Fisheye (dome)** | Azimuth and elevation | The fisheye image of the dome as the master view fisheye camera shows it: the zenith in the centre, the front down, the field of view of the Grid settings. |
| **Flat** | Horizontal and vertical move (cm) | The plane move ranges of the Grid settings (*Plane horizontal/vertical range*), with a grid. |
| **Sphere** | Azimuth and elevation | An equirectangular map of the whole sphere, the front in the centre. |
| **Circular screen** | Degrees around the screen and vertical move (cm) | A ring for the circular surface mapping of the Grid settings: the screen height outwards, the seam in red. Layers with *Horizontal movement: Circular degrees* move their centre angle; other layers move their horizontal position, converted to degrees. |

| Key | Action |
| --- | --- |
| ← → ↑ ↓ | Move the selected layer (1° or 10 cm; with Shift 10 times as far) |
| + − | Resize the selected layer |
| Page Up / Page Down | Previous / next slide |
| Esc | Deselect the layer |
| F | Fullscreen (browser) |

## HTTP endpoints

The views use the [HTTP API](/remote-control/api) and the [event feed](/remote-control/api#change-subscriptions) (topics `presentation`, `movie` and `layerview`). The endpoints below are only available in builds with the web views and can also be used by other clients.

| Method | Path | Description |
| --- | --- | --- |
| GET | `/clusters` | `{name, clusters: [{name, url}]}`: the name of this cluster and the other clusters, from `clusterName` and `clusters` in `data/http-server-conf.json` |
| GET | `/presentation_state` | The presentation state: slides, notes, current and next slide |
| GET | `/slide_thumbnail?index=N[&rev=R]` | Thumbnail JPEG of slide N |
| POST | `/slide_notes?index=N` | Sets the notes of slide N (UTF-8 text body) |
| POST | `/slide_skipped?slide_idx=N&on=0\|1` | Skips slide N or includes it again (see the [HTTP API](/remote-control/api)); `/presentation_state` lists `skipped` for each slide |
| POST | `/slide_thumbnail_update[?index=N]` | Renders the shown slide with the master view camera as its thumbnail |
| POST | `/presentation_save` | Saves the presentation to its file |
| GET | `/presentation_recent` | `{files: [{path, name}]}`: the recently loaded presentations, the newest first |
| POST | `/presentation_open_recent?path=` | Opens the presentation at `path` in C-Play, as *File → Open Recent Presentations* does. `path` has to be one of `/presentation_recent` (400 otherwise) |
| GET | `/movie_state` | Playlist, sections of each item and of the loaded file, playing item and section |
| GET | `/movie_media?index=N` | `{tracks: [{index, id, text}], duration}`: the audio and seconds of item N, also when it is not loaded (422 when the file cannot be opened) |
| POST | `/movie_cue?index=N&section=S&position=T&audio=A&play=0\|1` | Loads item N unless loaded, then audio track A (zero-based), section S (-1 for none) and time T (seconds); parameters left out keep what C-Play has |
| GET | `/movie_preview` | Whether pictures are available (built with WebRTC), and the last problem of each picture |
| POST | `/movie_preview/offer?feed=live\|prepare` | Body: SDP offer with ICE gathering complete. Answers `{id, sdp}` |
| POST | `/movie_preview/close?id=` | Ends a picture session |
| POST | `/movie_preview/prepare?index=N&position=T&play=0\|1` | The cue picture: item N (-1 for the loaded file) at T seconds, playing on with `play=1` |
| GET | `/movie_cuelists` | `{folder, playlist, files: [{name, modified}]}`: the `*.cplaycue` files in the cue list folder |
| GET | `/movie_cuelist?name=` | A cue list: `{type: "cplaycue", version, playlist, cues}` |
| POST | `/movie_cuelist?name=` | Saves the cue list in the body (JSON) as `name.cplaycue` |
| GET | `/movie_recent` | `{playlists: [{path, name}], media: [{path, name}]}`: the recently loaded playlists and media files, the newest first |
| POST | `/movie_open_recent?path=` | Loads the playlist or media file at `path` in C-Play, as *File → Open Recent* does. `path` has to be one of `/movie_recent` (400 otherwise) |
| GET | `/layerview_state` | The layer view state: the slides (index -1 is the master) and their layers with grid mode, visibility, volume and plane values; the canvas chosen in C-Play and the dome, flat range and circular mapping settings it is drawn with |
| POST | `/layerview_layer?slide=S&row=R&...` | Sets plane values of the layer in row R of slide S (`azimuth`, `elevation`, `roll`, `distance`, `horizontal`, `vertical`, `width`, `height`, `centreDegrees`, `widthDegrees`; flat layers only) and/or `volume` (0–100). Nothing changes when one value is invalid (400). Answers the layer |
| POST | `/layerview_fade?slide=S&row=R&on=0\|1` | Fades the layer in (1) or out (0), as double-clicking it in C-Play does |
