---
title: C-Lux lights
sidebar_position: 7
---

# C-Lux light server in C-Play

C-Lux is a light server that drives a ring of RGB lights, for example around a fulldome screen. It composites its output from a stack of patterns (static color, sine wave, gradient, rainbow, video), groups them into scenes, and exposes everything through a REST API plus a Server-Sent Events frame stream at the engine's tick rate (~30 fps).

C-Play can connect to a running C-Lux server and:

* browse its **scenes** and **patterns**, apply or unapply scenes, toggle patterns,
* control the output: **blackout**, **half light**, and the solid-color **work light**,
* watch the current frame in a **dome preview** ring, and optionally overlay the live light colors on C-Play's own 3D view,
* feed **C-Play's NDI output** into C-Lux so it can drive a Video pattern from what is on screen,
* attach scene actions to slides, so a slide start applies, unapplies, or replaces a C-Lux scene.

The integration talks to the server over its REST API and SSE frame stream using Qt Network only — no extra dependencies. It is enabled by default in the build (`BUILD_CPLAY_WITH_CLUX`); without it the **C-Lux Editor...** menu entry does not exist.

## Connect a server

Open **Settings -> C-Lux Editor...**. The toolbar has a connection indicator, a server URL field (default `http://localhost:8787`), and a **Connect** / **Disconnect** button. The URL you connect with is stored in `data/clux-server.json` (a single entry) and reloaded at startup.

If the C-Lux server has an edit password configured, a login row appears while connected but not logged in. Only pattern toggles are guarded by the password — scene apply/unapply/replace, clearing patterns, and the NDI endpoints stay open. The session token is kept for the current C-Play session only and never written to disk; restarting the server drops its in-memory sessions, so a reconnect may ask you to log in again.

## Live mode vs preview mode

The editor works in two modes, switched with the **Live mode** toggle (shown as `LIVE` / `PREVIEW`):

| | Preview mode (default) | Live mode |
| --- | --- | --- |
| Scene/pattern commands | Applied to local state and queued as pending changes; nothing that alters the server is sent. | Sent to the server at once, driving the physical lights. |
| Connection required | No — edits work offline against the cached state (see below). | Yes — the switch stays disabled while disconnected. |
| Real output | Blacked out on the server while connected, so it stays dark. | Follows your commands and whatever blackout state the server has. |
| Frames shown | Rendered locally at the server's tick rate from the same scene/pattern state (a C++ port of the pattern engine). | The live SSE frame feed from the server. |
| State sync | Manual **Update scenes** button. | Polled once per second, so changes made from another control surface (the C-Lux web editor, a show application, ...) appear within a second. |

Preview mode is meant for building up a look without touching the real lights: every scene or pattern change you make updates the local preview and is counted in the `N unsent changes` label next to the switch. When you enter live mode while connected, C-Play replays those queued changes against the server — one request at a time, in the order they were made — and then restores the blackout, so the lights fade back in with exactly the look that was built up. If any queued change needs the edit password and you are not logged in yet, entering live mode is refused until you log in.

Losing the connection (server restart, network drop) falls back to preview mode locally; the queue of unsent changes is kept for the next time live mode is entered.

:::note Preview rendering
The local renderer reproduces the static, sine wave, gradient, and rainbow patterns plus the solid work-light layer with the server's blending. A Video pattern has no local implementation yet, so it contributes nothing to a preview frame, and scene cross-dissolves switch at once instead of easing over the server's transition time.
:::

## Editor controls

**Scenes.** One row per scene with its pattern count in parentheses. The switch applies or unapplies the scene (scenes can be combined), **Replace** switches to only that scene, and the list icon offers *Attach to slide* — see below. In preview mode use **Update scenes** to re-fetch the list from the server; live mode keeps it current automatically.

**Patterns.** One row per pattern with its type in parentheses and an enable switch, plus a **Clear all patterns** button. Toggling a pattern is the one action that needs the edit password when the server has one configured.

**Work light.** The solid-color layer C-Lux composites under everything: R/G/B values, an enabled checkbox, and **Apply work light**. Like blackout and half light it only works in live mode — preview mode already keeps the real output dark.

**Blackout / Half light.** Checkable buttons in the bottom bar, live mode only. They mirror the server's state, which is why they are disabled while disconnected or in preview mode.

**Dome preview.** The ring visualizer at the bottom shows one wedge per light, colored with the current frame — the live feed in live mode, the locally rendered frame in preview mode.

### Show in 3D view

The **Show in 3D view** switch overlays the live light colors on C-Play's own 3D render as a dome-grid ring with a radial alpha fade from the rim toward the center — useful for checking how the lights sit over the content while you work. It is runtime-only: always off at startup and not persisted, so it never surprises anyone who opens the show later.

## NDI capture into C-Lux

C-Lux can receive one NDI source of its own and feed a Video pattern from it, sampling the image along the rim in fisheye mode. The **NDI** group controls that receiver:

* **Use NDI source** — turns the receiver on or off. Turning it on defaults to C-Play's own NDI output (`<machine> (C-Play)`) when this build has NDI support, so the lights can follow what is on screen; otherwise it keeps whatever source the server already receives.
* **Source list** — the senders discovery has seen on the network; picking one assigns it to the receiver.
* **Ring width** — thickness of the rim band each light samples, as a fraction of the ring's radius (0 reads a single circle of pixels). The server keeps this setting whether or not a source is running, so you can pre-aim the next one.
* **Status line** — what the receiver is doing right now: receiving which source, starting up, no signal, or an error.

The NDI endpoints are open like the frame stream, so this works in both modes; in preview mode the blackout keeps the real output dark while you aim it. In live mode the state refreshes every second with the rest of the poll; in preview mode use **Update sources**.

## Attach scene actions to slides

In the Scenes list, press the list icon next to a scene and choose:

* **Apply on slide start**
* **Unapply on slide start**
* **Replace with scene on slide start**

This adds a REST layer titled `C-Lux: <scene> (<action>)` to the currently selected slide (or master when none is selected) that fires `POST <server>/api/scenes/<name>/<action>` when the slide starts. No new pattern or scene is created on the server — the existing C-Lux state is simply driven from the show file, so it stays editable in the normal C-Lux tools as well.

## Files

| File | Content |
| --- | --- |
| `data/clux-server.json` | The single configured server URL (`{"serverUrl": "http://localhost:8787"}`). Written whenever you connect with a new URL. |
| `data/clux-state.json` | Cached preview state (light count, scenes, patterns, applied scenes). Updated automatically while connected; lets preview mode build up a look before the server is reachable. |

## Cluster notes

REST layers exist on the master only, so scene actions attached to slides are fired from the master machine — make sure the C-Lux URL in `data/clux-server.json` is reachable there (and from any other machine where you open the editor). The lights themselves follow whatever that one server does; if different machines should drive different light rigs, run a separate C-Lux server per rig and point each machine's `data/clux-server.json` at its own.
