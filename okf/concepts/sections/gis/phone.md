---
type: Concept
title: The map on a phone
description: "Opened and built 2026-10-04, on the author's direction that the clients need a map on the phone. What a field worker does with a map, and therefore what the phone's map is: one finger always moves the map, a long press picks up or adds, a tap selects and a bottom sheet answers; two speeds of adding (long press, or a pin fixed in the middle with the map moved under it); the phone's location, asked for on a tap and drawn with its accuracy; notes that sit on a place; and markers redrawn as real pins from their own geometry. The research it rests on, every desktop map feature and what it became, and what Void Maiz gained."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-04T00:00:00Z
---

# The brief

The author, 2026-10-04:

> lets begin work on the map, specifically on mobile ... right now the clients
> need a map on mobile! now, the map we have on desktop works pretty well, but
> im sure you can tell that a lot of the functionality would have to be
> different on mobile. likely a lot more gestures are going to need to be
> introduced to maiz.

> essentially, i want to focus on being able to actually add things onto the
> map, giving them icons and colors, and attaching them to a real database.
> also on mobile, i would like for there to be a functionality for gps ... it
> could be a thing the app asks for permission to know the location of the
> device.

> i also want to be able to link locations to notes as well, not just contacts
> and organizations.

> we need a slight overhaul on symbol design for markers on the map ... having
> real looking "location markers" would be cool ... without needing to rely on
> some asset or external library.

> we want smoothness, and we want good ui/ux for mobile here. More or less,
> we're gonna do minimal changes to the desktop version ... this is basically a
> drawing application with an infinite canvas (sort of). there's a lot of
> technical things we need to consider.

This reverses one line of the 2026-09-23 scope ([mobile](/concepts/sections/mobile.md):
"No Map"). The rest of that scope stands: the phone is a member doing field
work, and a desktop feature that cannot be made good on a phone is left out,
not made small.

# What the research decided

The author asked for research on mobile map and GIS UX. It is kept, with its
sources and what each finding decided here, in [GIS research](/concepts/sections/gis/research.md),
because it will outlive this screen. In one line each: field GIS fails by
carrying desktop menus onto glass; a finger needs ~1 cm and dense targets need
disambiguation; one-handed zoom beats pinch for one hand; direct touch beats a
crosshair for vertices but a fixed pin is more precise for one point; cluster
point data, list it beside the map; never rotate a map that is not for
navigation; ask for location at the moment it is needed and show its accuracy.

# The four rules the screen is built on

1. **One finger moves the map. Always.** On the desktop a press on a marker
   drags it. Under a 1 cm finger that rule moves a marker every time somebody
   pans across a busy street. So a drag pans, and a marker moves only after a
   **long press picks it up**.
2. **A tap selects, and a sheet answers.** No side panel, no right-click menu,
   no inspector: one bottom sheet, which grows when dragged.
3. **Adding has two speeds**, which end at the same "what is it?" sheet and the
   desktop's own `place` action.
4. **The phone knows where it is only if asked**, and says how sure it is.

# Gestures

| gesture | on the map | where it comes from |
|---|---|---|
| one-finger drag | pan; a fast release coasts | the gate's press-then-drag, and its new **fling** |
| pinch | zoom about the fingers, continuously | the gate's pinch (2026-09-27) |
| double tap | zoom in one level about the tap, animated | the gate's new **double tap** |
| two-finger tap | zoom out one level | the gate's new **two-finger tap** |
| tap, then drag up or down | one-handed zoom (down zooms in) | the gate's new **quick zoom**; never on a list |
| tap a marker | select it: the sheet shows it | |
| tap two markers at once | the sheet asks which | |
| tap a counted disc | zoom until it opens up (or list them, if it cannot) | |
| tap empty map | deselect | |
| long press a marker, then drag | pick it up and move it; it rides above the finger with a crosshair at its tip | the gate's new **long press** |
| long press empty map | "Add here", with a pin dropped there | |
| long press a reference point, then drag | move it and everything gathered at it | |

**Why zoom is continuous on the phone and not on the desktop.** The desktop
zooms a wheel notch at a time and draws tiles at exactly 256 px. A pinch is a
continuous scale, so the phone keeps `zoom` as a real number, draws the nearest
whole tile level, and scales those tiles by 2^(zoom − level)
(`src/gis/view.hpp`, tested in `gis_smoke`). Tiles are drawn at 256 dp × ½
density, so a phone at 2.6× reads the map's own labels at about 1.3×.

# Every desktop map feature, and what it became

The author: *"there's probably a LOT of things, we honestly may have to even
list everything out individually."* So here they are. **Built** means on the
phone today; **adapted** means built in a different form; **left out** is a
decision, with its reason; **later** is wanted and not built.

| desktop feature | phone | how |
|---|---|---|
| slippy map, OSM/CARTO tiles, ancestor/child stand-ins while loading | **built** | the same tile cache and fetcher; on a phone the fetcher uses Android's HTTP (a phone has no `curl`), named as this app for OSM's tile policy |
| wheel zoom about the cursor | **adapted** | pinch, double tap, two-finger tap, quick zoom; the wheel still works on the desktop's `--phone` |
| drag empty map to pan | **built** | plus a coast after a fling |
| camera remembered | **adapted** | its own key, `view.phone.map.camera`, so a phone does not move a desktop's map; first open falls back to the desktop's camera, then to fitting everything placed |
| base map choice (labelled / no labels / dark) | **built** | in *Views and base map* |
| base map brightness / fade | **built (read)** | applied as on the desktop; set on the desktop |
| attribution | **built** | bottom right, always |
| loading indicator | **adapted** | a small spinner, not a sentence |
| markers by glyph colour | **built** | one resolver for both front-ends now (`marker_look`) |
| rule-styled markers (view rules) | **built (read)** | rules apply; writing rules stays on the desktop (configuration) |
| `color:` / `icon:` / `shape:` tags | **built** | swatches, an icon grid and the five forms in the sheet, each one tap, each one `tag` command |
| Allomone's derived `map` style | **built** | same precedence as the desktop |
| labels, no-overlap | **adapted** | shown from zoom 14.5 when the view shows labels, dodging each other; the selected marker is always labelled |
| click to select → inspector | **adapted** | tap → the sheet; *Open* goes to the phone's own detail screen (every field, labels above) |
| drag marker to move (`move` action) | **adapted** | long press, then drag; or *Move* in the sheet, which uses the fixed centre pin |
| right-click empty → place new contact / org / event / incident | **adapted** | long press → *Add here*, six kinds as tiles |
| right-click → place an existing unplaced rune | **adapted** | *Someone or something already saved* in the Add sheet, a search with the platform keyboard |
| search bar → jump to a located rune / arm click-to-place | **built** | *Search the map*; a placed result flies there and selects it, an unplaced one starts the centre pin for it |
| "On this map" list | **adapted** | the sheet's resting state, nearest first |
| marker menu: centre, icon, colour, shape, remove from map, delete | **built** | in the sheet; *Take off the map* and *Delete* both offer UNDO |
| shift-click / box select, batch panel | **left out** | a multi-selection on a 6-inch screen is a desktop job; the phone does one thing at a time. Retagging many is the Data screen's or the desktop's |
| layers (2026-10-05: views became layers) | **built** | *Layers*: the desktop panel at a finger's size. The same stack top first, an eye each, tap a row to edit that layer, arrows to reorder (renumbered 0..n-1, the desktop's rule), *New layer on top* at the phone's own camera; the edited layer's name, opacity and brightness under others, own positions, labels, delete (UNDO); the base map as the bottom row, with its source, brightness and fade. The other visible layers are drawn ghosted underneath, in their own positions, colours, opacity and brightness; a hidden layer or a layer's `filter` hides its markers, as on the desktop |
| Actions (export, home view, map block) | **left out** | the author: not on the phone yet |
| per-view position channels | **built** | the phone reads and writes the active view's channel, as the desktop does |
| rule editor, rule audit, match counts, conflicts | **left out** | configuration; desktop |
| map config (labels on/off, label size, colour) | **adapted** | labels on/off from the phone; size and colour read |
| PNG export | **left out (later)** | a phone would hand the picture to the system's share sheet; that intent is not built (see mobile.md's platform-intents gap) |
| draw rectangle / ellipse (`mapshape`) | **built** | *Draw a region instead* in the Add sheet: drag across the map; pinch still zooms |
| shape colour, delete | **built** | the region's sheet |
| shape label, `bestows` | **adapted** | *Name, and the tag it gives* opens the region in the detail screen |
| apply a shape's tag to what is inside | **built** | one button, the same containment test as the desktop (`domain/bestow.hpp`) |
| polygons | **later** | not on the desktop either; when built, vertices by direct tap, never by crosshair (research above) |
| reference points: create, draw gizmo and fan lines | **built** | *Reference point* is one of the Add tiles |
| reference points: drag to move with children | **adapted** | long press, then drag |
| attach / detach a marker to a reference point | **adapted** | *Grouped at* in the marker's sheet |
| drag a fanned child (offset, not position) | **built** | the same `ref_off`, in the desktop's pixels |
| fan-out layout | **built** | the desktop's offsets × the phone's density |
| hidden connections (proximity lines) | **left out** | an analysis view; on a small screen lines between markers are clutter |
| presence on markers | **built** | the same `presence_rect` on the `map` surface; a desktop member sees what the phone has selected |
| incidents | **built** | a kind on the Add sheet |
| geocoding by address | **later** | it is an Antfarm holiday on the desktop's roadmap, not built anywhere yet |
| image markers (photos as markers) | **later** | not built on the desktop either |
| the website's map widget | n/a | it now draws the same pins (below) |

**New on the phone, with no desktop ancestor:** where the phone is, the
accuracy circle, *Where I am instead* when adding, *Here* when placing,
clustering, the pick-which sheet, a scale bar, distance from you on every
marker and in the list.

# Where the phone is

**Void Maiz's location holiday** (`voidmaiz/location.hpp`, added the same day):
a platform object installed once per process, like the touch gate. On Android,
`MaizActivity` asks for `ACCESS_FINE_LOCATION` (with `ACCESS_COARSE_LOCATION`, so
"approximate" is a possible answer) and runs `LocationManager` (GPS and network
providers, keeping the better of a fresher-but-worse fix and an older-but-better
one), stopping while the app is paused. Off Android, `location()` is null and
the locate button is not drawn. The phone harness installs `FixedLocation`, a
stand-in whose answer can be set to grant, deny or be switched off.

| the person sees | because |
|---|---|
| nothing about location until they press the locate button | `request()` is the only call that can prompt, and only that button calls it |
| the system's own prompt | a dangerous permission: Android draws it, never us |
| a blue dot and a circle | the circle is the fix's 68% radius at this zoom |
| a grey dot | the fix is more than 30 s old (last known) |
| "Location is off for Hormiga …" | they refused, and Android will not ask again |
| the GPS stopping when they leave the map | `phone_frame` stops it on any other screen: a battery nobody asked to spend |

**What a "here" place records:** the coordinates, through the same `place` or
`move` action as a tap. The accuracy is shown, not stored ([Q100](/developer_questions.md)).

# Notes on the map

The `note` glyph carries the location facet now (`geo`, `ref`, `ref_off`, and
the per-view channels), so a note can sit on a place:

- **Add here → Note** places a note and opens it to be written.
- A marker's sheet has **Note**: a note *about this place*, placed at it,
  starting "About <name>".
- A marker's sheet lists **Notes here**: every note within 40 m.
- The note editor (phone) has **Pin to a place** / **Show on the map**, and the
  Data detail has **Put on the map** / **Show on the map** for every placeable
  kind. A thing and where it is are one tap apart in both directions.
- A note on the map is an amber **balloon** with a note in it, captioned with
  its first line.
- The desktop's place menu has **New note here**, and notes can be placed from
  its search.

**Privacy.** A note is the internal-notes class the render seam guards
([security](/concepts/platform/security.md)). The website's map widget already
skipped notes; the **PNG export now skips them too**, so a note's position never
leaves the app through a picture. A private note stays private by the Antfarm's
private tag exactly as before: it is never sent to members.

# The markers, redrawn

`src/gis/marker.hpp` computes a marker's **outline as points**, which three
surfaces fill: the canvas (ImGui's polygon fill), the PNG export (a 4×4
supersampled scanline fill) and the website's widget (a JS port in
`render/web/app.js`, to be kept in step). Before this the pin was a triangle
under a circle, drawn three slightly different ways.

**The pin, as a construction.** A head of radius *r*, its centre *h* = 2.2*r*
above the tip *T* (the classic 27×43 proportion). The two points where a line
from *T* just touches the circle are where the sides leave the head: the angle
at the centre between "down" and those points is φ = acos(*r*/*h*), so they sit
at *C* + *r*(±sin φ, cos φ). The outline is the long arc over the top between
them, and two sides from them to *T*. Straight sides read as a cone with a ball
on it; what makes a pin read as a **drop** is that its sides curve in, so each
side is a cubic Bézier whose first control point lies on the tangent line (so
it leaves the head without a kink) and whose second sits just inside it near the
tip (so it arrives narrow and still sharp). The four numbers that shape it are
named constants, the knobs to turn. A pin with no icon gets the white dot in its
head; a pin or balloon gets a soft shadow where its tip touches the ground.

**The forms:** circle, **pin**, square (now rounded), diamond (now equal in area
to the others), and **balloon** (a rounded square on a short pointer, the "place
card", which notes use). Every form's anchor is its location: the centre of a
circle, square or diamond, the **tip** of a pin or balloon. Labels and the
desktop's hit test now aim at the head of a pin, not the tip under it.

`gis_smoke` checks the geometry: the tip is at the location, nothing hangs
below it, the top is exactly the head, the sides curve inside the straight
tangents, the icon sits in the head.

# What Void Maiz gained

| piece | what | tested |
|---|---|---|
| touch gate: **long press** | a still finger past 0.45 s, reported once; ImGui already holds the press, so a drag afterwards is ImGui's | `touch_gate_smoke` |
| touch gate: **double tap** | `taps` = 2 for a tap close in time and place to the last; both still reach ImGui | yes |
| touch gate: **two-finger tap** | two fingers down and up without moving; nothing reaches ImGui | yes, and that a moving pinch is not one |
| touch gate: **quick zoom** | tap, then down and drag: a per-frame scale; only where the window cannot scroll vertically, so lists still scroll | yes, both |
| touch gate: **fling** for a canvas drag | velocity of a press-then-drag released fast; a drag that stopped first does not fling | yes, both |
| **location holiday** | `voidmaiz/location.hpp`, `src/input/location.cpp`, `MaizActivity.maizLocation*` | the stand-in, in `touch_gate_smoke`; the Java compiles (below) |
| `android_http(activity, user_agent)` | a User-Agent for servers that refuse Android's default | compile |

Every other screen behaves exactly as before: the new gestures are reported
beside what ImGui was told, and a screen that does not read them does not
change. The one behavioural difference: a finger that comes back down within
0.3 s of a tap, on something that cannot scroll vertically, waits the long-press
time before becoming a press (it may be a quick zoom).

# Where it lives

`src/phone/phone_map.cpp` (the canvas: tiles, markers, clusters, location,
gestures, the floating controls), `src/phone/phone_map_sheet.cpp` (the bottom
sheet, and the map's own screens: search, put something saved here, layers and
the base map), `src/phone/phone_map.hpp` (their shared state, `MapUi`).
`src/gis/marker.hpp` and `src/gis/view.hpp` are the engine's: the outline and
the continuous viewport, both tested in `gis_smoke`. The desktop's half is
`marker_look` and `draw_marker_shape` (`app/app_internal.hpp`, `app_shared.cpp`).
The phone harness learned `dtap`, `tap2`, `qzoom`, `holddrag` and `location`.

# What is not done

- **A device.** Everything above ran in the phone harness on the desktop
  (screens, taps, holds, pinch, double tap, quick zoom, two-finger tap, a
  stand-in location granted and refused). Nothing has run on a phone, so the
  first real GPS fix, the system prompt, the tile download over Android's HTTP
  and the feel of the gestures are the author's to judge on 0.1.11.
- Polygons; sharing a map picture;
  geocoding; image markers (all in the table above).
- Q100 to Q102 in [developer questions](/developer_questions.md).
