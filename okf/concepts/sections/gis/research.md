---
type: Research
title: GIS research — what the map's design rests on
description: "Opened 2026-10-05 from the research done for the phone's map (2026-10-04). Findings from field-GIS usability, touch HCI, clustering, map orientation and location-permission studies, each with its source and what it decided; then what to keep in mind before adding a map feature, and what to read next. Add to it whenever research decides something."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-10-05T00:00:00Z
---

**Why this page exists.** The author asked for research before the phone's map
was designed, and noted that most map literature is about navigation, which
Hormiga is not. The useful literature turned out to be **field data collection
(mobile GIS)**, **touch on dense targets**, **clutter**, and **location
privacy**. Research outlives the screen it was done for, so it lives here, and
any new finding that decides something should be added with its source.

# The findings, and what each decided (2026-10-04)

The author asked for research on map UI/UX, noting that most of it is about
navigation, which Hormiga is not. So the reading leaned on mobile **GIS and
field data collection** and on the HCI of **touch on maps**, and each finding
below is here because it decided something (in [the map on a phone](/concepts/sections/gis/phone.md)).

| finding | source | what it decided |
|---|---|---|
| Field GIS interfaces fail from *interface complexity and information overload*, and from carrying over desktop (WIMP) metaphors | Delikostidis & van Elzakker, field-based usability of mobile geo-applications; the Wits field-collection thesis | no menus, no panels, no right-click on the phone. One sheet that grows when dragged, and configuration (rules, views) stays on the desktop |
| Usage-centred design starts from the field task and its context, not the schema; schemas dictating forms is the common failure | the Wits thesis; QGIS/QField UX talk (2026) | adding asks *what is it* first (six big tiles), then goes straight to what that kind needs (a name, or the note's words), never to a field list |
| A touch target needs about 1 cm (Parhi, Karlson & Bederson, MobileHCI 2006); a fingertip is 1.6–2 cm, a thumb 2.5 cm; dense small targets suffer the *fat-finger problem*: inaccuracy and occlusion | Parhi et al.; NN/g touch target size; MIT Touch Lab | markers are drawn small and **hit large** (24 dp around the head); when a tap lands on two markers too close to tell apart, the sheet **asks which** instead of guessing; a lifted marker rides **above** the finger so the finger never hides where it lands |
| Pinch needs two hands; one-handed *tap-and-drag* zoom was ~18% faster and preferred one-handed (Farhad & MacKenzie, HCII 2018); a two-finger tap saved 14% over pinch for zooming out (Tap&Tap, INTERACT 2015) | yorku.ca/mack; IFIP LNCS 9298 | the gate gained **quick zoom** (tap, then drag) and the **two-finger tap**, beside pinch and double tap. Field work is often one-handed |
| Placing vertices by direct touch is about twice as fast as moving the map under a crosshair; crosshair-only drawing is described as "extremely frustrating" | Esri Field Maps / Collector user reports; MobileMap docs | **two speeds** of adding a point: direct (long press where it is) and precise (a fixed centre pin, the map moved under it, where nothing covers the spot). Regions are drawn by direct drag, never by crosshair |
| Hundreds of points on a small map need clutter reduction; clustering suits point data (heat maps suit areal data), and practitioners overuse it for performance rather than meaning | Meier, *The Marker Cluster* (2016); PeerJ preprint 27858 | markers that would overlap become a counted disc in the colour most of them share; a tap opens it up (zooms to fit), and past zoom 17 the map stops grouping. Markers somebody arranged around a reference point never cluster |
| A list view beside the map is a real alternative for people who cannot use the map well | NSW design system; accessibility literature | the sheet's resting state is **"N on this map"**, and a drag up lists them nearest first, tappable |
| North-up keeps a cognitive map stable; heading-up helps forward-view navigation only | NASA/Human Factors north-up vs track-up studies | **no rotation**. Two fingers that twist do nothing. Hormiga is not navigation |
| Run-time disclosure works; a stated reason before the prompt made **no measurable difference** to the answer (2,579 Android users); the app and the *moment* of asking did. Users given "approximate" choose sensibly | Google/Rutgers location-permission studies; Aalto "general area" study | the location prompt comes **only** from a tap on the locate button, never at start-up; no pre-prompt essay. "Approximate" is a real answer, drawn as a wide circle |
| Phone location can be tens of metres out, and field apps collect points wrongly when that is hidden | Fulcrum/Field Maps reviews; VGI positional-quality studies | a fix is drawn **with its accuracy circle**, a place set from it says "give or take N m", and an old fix greys out |

Sources: [Delikostidis & van Elzakker](https://research.utwente.nl/en/publications/field-based-usability-evaluation-methodology-for-mobile-geo-appli/),
[Wits thesis](https://wiredspace.wits.ac.za/items/0d4e2c36-7959-4098-b6af-6ea84a916715),
[QField UX talk](https://talks.osgeo.org/qgis-uc2026/talk/LCHAWA/),
[NN/g touch targets](https://www.nngroup.com/articles/touch-target-size/),
[Farhad & MacKenzie](https://www.yorku.ca/mack/hcii2018c.html),
[Tap&Tap](https://dl.ifip.org/IFIP-LNCS/hal-01609390v1),
[crosshair vs touch vertices](https://community.esri.com/t5/arcgis-field-maps-ideas/enable-touch-to-enter-vertices/m-p/1122589),
[clustering markers](https://peerj.com/preprints/27858v2),
[The Marker Cluster](https://www.igi-global.com/article/the-marker-cluster/153624/),
[north-up vs track-up](https://stars.library.ucf.edu/scopus2000/389),
[location permission studies](https://research.aalto.fi/en/publications/general-area-or-approximate-location-how-people-understand-locati/).

# Keep in mind before adding a map feature

1. **Which world is it for?** Classify it in [worlds](/concepts/sections/gis/worlds.md)
   first: universal, anchored or Earth-only. An Earth-only feature asks the
   source and is absent elsewhere.
2. **Is it configuration or field work?** Configuration (rules, views, channels,
   export settings) is the desktop's; the phone reads it. Field work (look,
   place, mark, note) is the phone's first job.
3. **What does a finger do with it?** One finger already pans. A new gesture is
   a long press, a sheet action, or a mode with a visible banner and a way out
   (Back, Cancel). Never a second meaning for a plain drag.
4. **What does it do to clutter?** Every new marker kind adds to the densest
   place on the map. It needs a form that reads at 20 dp, a cluster colour, and
   a caption that is not a slug.
5. **Does it leave the app?** Exports and the website pass the render seam: a
   note never does, a contact's position does not on the public site, a photo
   on a map would need the same guard ([security](/concepts/platform/security.md)).
6. **Is it a command?** Every change is a dispatcher command the CLI could send.
   If a gesture cannot be written as one, the design is wrong.
7. **Location is personal data.** A person's position (a member's GPS, a
   contact's home) is Neighborhood-grade PII. Ask at the moment of need, show
   accuracy, never collect in the background, never publish precise personal
   positions.

# To read next

- **Label placement**: the cartographic literature on point-feature label
  placement (Christensen, Marks & Shieber's comparison of algorithms; Imhof's
  rules) before labels get any smarter than "try four positions".
- **Clutter beyond clustering**: Meier's marker-cluster critique proposes
  alternatives (aggregation by area, heat maps for areal data); relevant when
  incidents or conditions get dense.
- **Accessible maps**: the list view is one answer; screen-reader access to a
  canvas map is not solved here at all.
- **Offline tiles**: OpenStreetMap's tile usage policy forbids bulk
  prefetching from its servers, so offline areas for field work need a
  different source (vector tiles, or an organization's own extract).
- **Georeferencing**: affine and similarity transforms from control points, and
  how QGIS's georeferencer presents them, for anchored worlds.
- **Polygon digitizing on touch**: direct-tap vertices, snapping, undoing the
  last vertex (Q102).
