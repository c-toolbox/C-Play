---
title: C-Lux lights
sidebar_position: 7
---

# C-Lux light server in C-Play

C-Lux is a light server that drives a ring of RGB lights, for example around a fulldome screen. It composites its output from a stack of patterns (static color, sine wave, gradient, rainbow, video, audio and more), each with its own blend mode, groups them into scenes - optionally with a timeline that switches the scene's patterns on and off over time - and exposes everything through a REST API plus a Server-Sent Events frame stream at the engine's tick rate (~30 fps).

C-Play's integration targets **C-Lux 1.1 or later**. With an older server, scenes and patterns still work, but the NDI controls report that the server has no NDI for Video patterns.

C-Play can connect to a running C-Lux server and:

* browse its **scenes** and **patterns**, apply or unapply scenes, toggle patterns,
* control the output: **blackout**, **half light**, and the solid-color **work light**,
* watch the current frame in a **dome preview** ring, and optionally overlay the live light colors on C-Play's own 3D view,
* feed **C-Play's NDI output** into C-Lux so it can drive a Video pattern from what is on screen,
* attach scene actions to slides, so a slide start applies, unapplies, or replaces a C-Lux scene.

The integration talks to the server over its REST API and SSE frame stream using Qt Network only — no extra dependencies. It is enabled by default in the build (`BUILD_CPLAY_WITH_CLUX`); without it the **C-Lux Editor...** menu entry does not exist.

## Connect a server

Open **Settings -> C-Lux Editor...**. The toolbar has a connection indicator, a server URL field (default `http://localhost:8787`), and a **Connect** / **Disconnect** button. The URL you connect with is stored in `data/clux-server.json` (a single entry) and reloaded at startup.

![C-Lux Editor](/assets/ui/clux_editor.png)

If the C-Lux server has an edit password configured, a login row appears while connected but not logged in. The password guards pattern toggles, pattern edits (which is what the NDI source and ring width are) and the list of NDI sources — scene apply/unapply/replace, clearing patterns, the NDI receiver status and the frame stream stay open. The session token is kept for the current C-Play session only and never written to disk; restarting the server drops its in-memory sessions, so a reconnect may ask you to log in again.

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

:::note[Preview rendering]

The local renderer reproduces the static, sine wave, gradient (any number of colors) and rainbow patterns plus the solid work-light layer, including color transparency, the four blend modes (alpha, additive, multiply, subtract) and the timelines of the scenes switched on. Other pattern types (Video, Audio, Fire, ...) have no local implementation yet, so they contribute nothing to a preview frame. Cross-dissolves — after a scene change or a pattern toggle — switch at once instead of easing over the server's transition time.

:::

## Editor controls

**Scenes.** One row per scene with its pattern count in parentheses. The switch applies or unapplies the scene (scenes can be combined), **Replace** switches to only that scene, and the list icon offers *Attach to slide* — see below. In preview mode use **Update scenes** to re-fetch the list from the server; live mode keeps it current automatically.

**Patterns.** One row per pattern with its type in parentheses and an enable switch, plus a **Clear all patterns** button. Toggling a pattern needs the edit password when the server has one configured. A pattern that a running scene timeline switches on and off is marked *timeline* and its switch is disabled: C-Lux refuses manual toggles of it. Applying a scene starts its timeline from the top (unless it is already playing), unapplying stops it, and **Replace** restarts it — in preview mode C-Play plays it locally the same way.

**Work light.** The solid-color layer C-Lux composites under everything: R/G/B values, an enabled checkbox, and **Apply work light**. Any transparency set for it from the C-Lux web editor is kept. Like blackout and half light it only works in live mode — preview mode already keeps the real output dark.

**Blackout / Half light.** Checkable buttons in the bottom bar, live mode only. They mirror the server's state, which is why they are disabled while disconnected or in preview mode.

**Dome preview.** The ring visualizer at the bottom shows one wedge per light, colored with the current frame — the live feed in live mode, the locally rendered frame in preview mode.

### Show in 3D view

The **Show in 3D view** switch overlays the live light colors on C-Play's own 3D render as a narrow, bright glow within the outer 10% of the fisheye radius, with a faint color tint fading farther upward toward the dome crown — useful for checking how the lights sit over the content while you work. It is runtime-only: always off at startup and not persisted, so it never surprises anyone who opens the show later.

## NDI capture into C-Lux

In C-Lux, NDI belongs to the **Video pattern**: a Video pattern whose source is set to *NDI stream* names a sender, and the server receives that sender for the pattern, sampling the image along the rim in fisheye mode (or a strip, as the pattern is set). Each Video pattern can follow a sender of its own. The **NDI** group edits one running Video pattern — so a scene with a Video pattern has to be applied first:

* **Video pattern** — which running Video pattern the controls edit. With a single one it is picked automatically; otherwise the first one already set to NDI is preselected.
* **Use NDI source** — switches the pattern's source to NDI, or (off) clears its sender so the server closes the receiver. Turning it on defaults to C-Play's own NDI output (`<machine> (C-Play)`) when this build has NDI support, so the lights can follow what is on screen; otherwise it keeps the sender the pattern already names.
* **Source list** — the senders discovery has seen on the network; picking one assigns it to the pattern. With an edit password configured, the list is only shown after logging in.
* **Ring width** — thickness of the rim band each light samples, as a fraction of the ring's radius (0 reads a single circle of pixels). The pattern keeps this setting whether or not a source is running, so you can pre-aim the next one.
* **Status line** — what the server's receiver for that pattern is doing right now: receiving which source, starting up, no signal, or an error (for example a sender that is not on the network).

Since these are pattern parameters, they follow the same rules as the other pattern changes: in live mode they are sent at once (and need the edit password when one is set); in preview mode they change the local state and are queued as unsent changes, which are sent when live mode is entered. They are not saved into the scene on the server — save the scene from the C-Lux web editor to keep them. NDI is queried only after you turn on **Use NDI source**. While NDI is enabled, live mode refreshes its state every second; in preview mode use **Update sources**. Turning it off or disconnecting stops NDI polling.

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
| `data/clux-state.json` | Cached preview state (light count, scenes, patterns, applied scenes, running scene timelines). Updated automatically while connected; lets preview mode build up a look before the server is reachable. |

## Cluster notes

REST layers exist on the master only, so scene actions attached to slides are fired from the master machine — make sure the C-Lux URL in `data/clux-server.json` is reachable there (and from any other machine where you open the editor). The lights themselves follow whatever that one server does; if different machines should drive different light rigs, run a separate C-Lux server per rig and point each machine's `data/clux-server.json` at its own.
