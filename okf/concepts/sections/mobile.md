---
type: Concept
title: Hormiga on a phone
description: "Opened 2026-09-22. The author: the mobile application must absolutely prioritize UX and will be a significantly diminished Hormiga. So the phone is a MEMBER doing field work (look someone up, add who you met, photograph a flier, check the calendar, see the map, publish what is ready), never the host and never the composer. Section by section, what survives and in what form; the Builder answered by the query-backed block (on a phone you edit the data, and the documents follow); the phone as a third front-end over the same core rather than a responsive desktop; and a gap analysis of what Void Maiz carries today, what it has staged, what it lacks, and what is Hormiga's own to fix (libsodium on Android first)."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

# The brief

The author, 2026-09-22:

> we should also begin considering the development of a mobile application for
> hormiga, and what is still lacking from maiz for mobile development.
> especially in the case where hormiga has multiple tabs, menus, settings, and
> especially the document builder. These are substantial components of hormiga
> that would likely need to be completely reimagined on a mobile device. does
> maiz have enough ability to carry that forward?

> While i always am looking to improve the user experience of hormiga, the mobile
> application will have to absolutely prioritize the UX of hormiga. Such that i
> imagine the mobile version of hormiga as being a significantly diminished
> version of the desktop hormiga application.

Two instructions sit in that: **UX before parity**, and **less on purpose**.
Every decision below applies them in that order. When a desktop feature cannot
be made good on a phone, it is left out, not made small.

# What a phone is for

**A phone is a member doing field work.** The desktop is where an organization
composes (newsletters, websites) and configures (the Antfarm, themes, rules).
The phone goes where the organization's people are: a street, an event, a
meeting, a bus. Its jobs, in rough order of how often an outreach worker would
reach for them:

1. **Look someone up.** Name, role, phone, email, notes, and who they are
   connected to. Tap to call or email.
2. **Add someone you just met**, or fix a detail. A name, a way to reach them,
   a tag or two, and suggestions for the rest.
3. **Photograph a flier or an event** and tag it. Because newsletter and
   website blocks are query-backed, a flier tagged `@summer` appears in the
   next issue with no one touching the Builder.
4. **What is on.** Today, this week, and a quick-add for an event.
5. **Where things are.** The Territory map, and a pin dropped where you stand.
6. **Is the publication ready?** Preview the next issue or the site as it would
   go out, and publish it if the desktop already set up where it goes.
7. **Who else is working.** Presence, and the occasional conflict to settle
   with one tap.

**What a phone is not for:** laying out a document, configuring a backend,
writing rules, importing a spreadsheet, theming, backups, hosting a share. All
of those stay on the desktop, and the phone says so instead of offering a
cramped version of them.

# The phone is a member, never the host

This one rule removes most of the hard problems:

- **The data arrives by joining.** A phone joins a database a desktop shares
  on the LAN ([LAN sharing](/concepts/platform/lan-sharing.md)) and keeps a
  Palabra replica, exactly as a second laptop does. It never opens a `.miga`
  from a file, so it needs no file picker to start.
- **Credentials mostly do not need to reach it.** A phone does not publish
  with its own keys unless the organization chooses that. The Antfarm's
  proposed "one device drives" rule ([Antfarm across devices](/concepts/platform/antfarm/collaboration.md)
  §5) is what lets a phone press *Publish now* and have the desktop that holds
  the key perform it. Until that exists, the phone's publish button is absent,
  not broken.
- **Files are cautious by default.** A phone on mobile data should not pull
  every image. [Q77](/developer_questions.md)'s cautious transfer (a coloured
  placeholder until asked) is the phone's default, where it is the desktop's
  option.
- **Private stays home, both ways.** The same `ShareFilter` sync and presence
  use means a desktop's private notes never reach the phone.

# Section by section

| desktop | phone | form on the phone (Void Maiz piece) |
|---|---|---|
| **Data** (tables, cards, detail, Connections canvas, tag filter) | **yes, the centre of the app** | a searchable list of people and things with swipe actions (`begin_swipe_row`); the detail in a bottom sheet that drags to full (`begin_bottom_sheet`); tag chips with suggestions (`maiz::suggest_tags`); edits compile to the same commands |
| **Calendar** (3-day / week / month, quick-add, `.ics`) | **yes** | an agenda list and a 3-day strip; quick-add through the FAB (`fab`); date and time need pickers Void Maiz has *staged*, not built |
| **Territory** (the map) | **yes, as a viewer with one gesture** | pan and pinch (`TouchRecognizer`, the same camera commands), tap a marker for its detail sheet, long-press to drop a pin; no layers panel |
| **Builder** (grid, palette, inspector, preview) | **reimagined, see below** | a document list, a block outline, text and image edits in a sheet, preview and publish |
| **Publish** | **one button, when ready** | on a connection that is ready, driven by the desktop that holds the key |
| **Antfarm** | **status only** | the connections dashboard from the [redesign](/concepts/platform/antfarm/redesign.md) A3 and A11: read-only, *Check*, and a ready connection's primary action |
| **Allomone** | **no** | rules still *run*: a card coloured by a rule on the desktop is coloured on the phone |
| **Console / command bar** | **no, with one exception** | errors become snackbars (`show_snackbar`), and "Deleted. UNDO" is one widget. A command sheet for power users is a later, optional thing |
| **Settings** | **a short list** | profile (name, colour, picture), networking (who I am shown as, cautious files), updates, language |
| **Niche Tools, Style, Data Tools** | **no** | |
| **Share / members** | **join and leave** | join by code on a digit keypad (the code is `maiz::lan::encode_join_code`, and the keypad is Interaction Combinators', an easy lift); the member list (`draw_member_list`); never host |

**Navigation.** Four rooms in a segmented control at the bottom, where a thumb
reaches (`segmented`): **People**, **Calendar**, **Map**, **Publications**.
Everything else goes behind the app bar's ⋮ (`begin_overflow_menu`), which can
hold a desktop menu's content without rewriting it. Void Maiz's touch page
already names Hormiga's four workflows as the segmented control's client.

# The Builder on a phone

The desktop Builder is a data-bound Figma: a grid, a palette, bands,
drag-to-place, an inspector, and a live preview in the real browser beside the
app. **None of that survives a 6-inch screen, and it should not try.** Void
Maiz's `apply_touch_canvas` makes a node graph *grabbable* on glass. It cannot
make a two-dimensional layout editor *usable* at 360 dp, and nothing in Void
Maiz claims to.

**The answer is already in the architecture: the query-backed block.** An event
grid in the newsletter is a tag query, not a list of events. So on the desktop,
putting next month's events in the issue means editing a document. On the phone
it means **adding the events**, and the document follows. Most of what an
outreach worker would want to "put in the newsletter" from the field is data:
an event, a flier, a person, a job. The phone is already the best tool for
that, and it is not the Builder.

What remains is the part of a document that is not data, and it is small:

| need | on the phone |
|---|---|
| see what documents exist | a list: newsletters and websites, each with its state ("draft", "published 2 days ago") |
| see what is in one | **an outline**: one row per block, in order, with its kind and a one-line summary ("Event grid: `@summer` → 6 events") |
| fix a sentence | tap a text block's row: its fields in a sheet, **both languages side by side**, since every text field is bilingual |
| swap a picture | tap an image block: pick from the organization's images, or take a new one |
| move a block | up / down in the outline (no drag on a grid) |
| see it as it goes out | preview the rendered issue or page (see the gap below) |
| send it | *Publish now*, when the Antfarm says it is ready |

**Not on the phone:** adding or removing blocks, the grid, bands, columns,
theme, navigation, multi-page structure. A person who wants those is told,
once and plainly, that the document is laid out on the desktop.

**The preview is the one hard part.** The desktop previews in the real browser,
served by the preview server. A phone has a browser too, but reaching it from a
NativeActivity is an `Intent`, which means Java through JNI. So the preview
depends on the same gap as "tap to call" (below).

# The architecture: a third front-end, not a responsive desktop

Hormiga already has **two front-ends over one core**: the desktop GUI and the
headless CLI, which share `src/domain`, `src/render`, `src/sync` and
`src/platform` and differ in what they draw. **The phone should be the third**,
not the desktop with `if (touch)` through every file in `src/ui/`.

The reason is size. Interaction Combinators branches its one screen three ways
(desktop, phone upright, phone sideways) inside one `app.cpp`, and that is
right for one screen. Hormiga has five sections, each holding several windows,
and `HormigaApp`'s header is at its line budget. Branching every one of those
for a phone would double the desktop's complexity to build something the author
wants *smaller*.

So:

- **`src/phone/`**: its own app object and screens, built on Void Maiz's
  mobile kit, and reusing everything below `src/ui/`.
- **`tools/check_layering.py`** gains a rule: `phone/` may not include
  `ui/`. If a phone screen needs something a desktop panel has, that thing
  moves down into `domain/` where both can reach it. That is how the headless
  front-end was kept honest.
- **The same commands.** A contact edited on the phone is `set maria phone
  …`, logged and replayable like everything else. Ground rule 3 does not
  bend for a smaller screen.
- **`classify_layout`** still matters *within* the phone front-end: a tablet,
  or a phone turned sideways, gets the detail beside the list instead of in a
  sheet, the way Interaction Combinators does it.

# Does Void Maiz carry it?

**For the chrome and the network: mostly yes.** Much of it was built with
Hormiga named as the client (touch.md: *"Void Hormiga is the second host, and
it is the harder one"*). **For the screens: it gives primitives, deliberately
not screens.** Q28 upstream declined a layout engine, and that is right, since
the phone front-end is ours to design. **For the Builder: nothing would carry
the grid editor, and nothing should.** Above, the Builder is redesigned rather
than carried.

## Built in Void Maiz (usable today)

| piece | header | the Hormiga use |
|---|---|---|
| touch recognizer: the deferred press, long-press as right-click, pinch and pan, edge-swipe back, millimetres not pixels | `touch.hpp` | every gesture; the map's camera |
| `apply_touch_canvas` | `mobile.hpp` | the Connections canvas and the Antfarm wiring, if ever shown |
| bottom sheet with detents | `mobile.hpp` | every detail view |
| snackbar with an action | `mobile.hpp` | errors, and "Deleted. UNDO" |
| FAB and speed dial | `mobile.hpp` | add a person, an event, a photo |
| segmented control | `mobile.hpp` | the four rooms |
| stepper | `mobile.hpp` | small numbers (a raw control; the registry field editor is staged) |
| swipe rows | `mobile.hpp` | People, the block outline |
| action bar that never clips, the ⋮ overflow menu | `mobile.hpp` | app bars |
| a drawn keyboard (appears when a field wants text; works over modals) | `mobile.hpp` | every text field |
| layout classes in dp (phone / tablet, portrait / landscape, compact, narrow) | `touch.hpp` | phone vs tablet arrangements |
| presence, the roster, marks, member list, networking settings | `presence.hpp`, `netview.hpp` | already adopted on the desktop |
| `voidmaiz_net` over Palabra | `net.hpp` | already adopted |
| LAN sockets, interfaces, join code, **Android's multicast lock** | `lan.hpp`, `lanlink.hpp` | discovery on a phone (Android drops broadcast without the lock) |
| the update client, including applying an `.apk` through `PackageInstaller` | `update.hpp`, `updateview.hpp` | the phone updating itself |
| an Android host (NativeActivity, GLES3), proven by Interaction Combinators' APK | the IC `android/` build | the template |

## Staged in Void Maiz (named, not built)

| piece | why the phone needs it |
|---|---|
| wheel / drum **date and time pickers** | Calendar quick-add; every date field |
| **safe-area insets** | notches and gesture bars cover content on current phones |
| action sheet | "Call · Email · Copy" on a contact |
| pull-to-refresh | "sync now", which in a log-first system is a real gesture |
| the stepper as a registry field editor | so a glyph's `hints.editors` can ask for one |
| fling / momentum consumed by the canvas | the map should coast |

## Missing from Void Maiz (to ask for)

| gap | why it blocks or hurts | severity |
|---|---|---|
| **accented letters on the drawn keyboard** (ñ á é í ó ú ü ¿ ¡) | Hormiga is bilingual EN/ES by founding decision (Q5). The keyboard is `qwertyuiop / asdfghjkl / zxcvbnm` with a symbols page. A Spanish-speaking organization cannot type its own name | **blocking** |
| **a navigation stack and the system Back button** | list → detail → edit → back is the phone's basic motion. Android's Back key reaches a NativeActivity as a key event, and nothing routes it. The touch page notes that *"the mantle stack is exactly a navigation stack"*, but no API exposes one | **blocking** (host-side possible, upstream better) |
| **platform intents**: open a URL, share, `tel:` and `mailto:`, the photo picker, the camera, location | "tap to call", "photograph a flier", "drop a pin where I am" and the Builder preview all need one. Each is a JNI call into the Java runtime, the same shape as the update client's `PackageInstaller` path | **blocking for jobs 1, 3, 5, 6** |
| **app lifecycle**: pause, resume, low memory | a backgrounded phone must save its replica and drop links cleanly, then come back. `idle_ms` already makes a silent link honest, and saving on pause is the other half | high |
| **a general HTTP seam** | `update.hpp` has `android_http` (`HttpURLConnection` through JNI) *inside* the update client. Hormiga's HTTP shells out to `curl`, which a phone does not have. Publishing, tiles and `.ics` feeds need the seam, not the updater | high, for publish and the map |
| text selection and copy | copying a phone number out of a note | medium; the touch page says "not planned without a client", and Hormiga is that client |
| the Storage Access Framework (open or save a file) | not needed while the phone only joins; needed the day it exports | low |

## Hormiga's own blockers (not Void Maiz's)

| blocker | why | fix |
|---|---|---|
| **libsodium is vendored as a Windows binary** ([Q67](/developer_questions.md)) | the LAN transport is sealed with it, and the room key and presence beacon need it. **A phone that cannot build libsodium cannot join.** | re-vendor the C sources (the question's own lean); they build with the NDK |
| HTTP through `curl` | as above | move behind the HTTP seam once Void Maiz exposes it; keep `curl` as the desktop's implementation |
| packaging | Void Mago's wizard emits NSIS only | Interaction Combinators' `build_apk.ps1` is the working pattern; the update feed already understands `.apk` |
| the signing key | Android requires the same key for every update, and losing it means every phone uninstalls | minted once, kept out of the repository, and **backed up**, as IC's `updates.md` warns |
| `HormigaApp` is desktop-shaped | see the architecture above | `src/phone/`, and the layering rule |

## What is fine as it is

SQLite (the vendored amalgamation), `stb` image decoding, the vendored fonts,
nlohmann/json, Void Core (Void Maiz compiles it from source on Android), and
Void Palabra (Interaction Combinators links `voidmaiz_net` on Android, so
Palabra already builds there).

# A staged path

Each stage is an exit test in the roadmap's style, and each is useful alone.

| stage | what | proves |
|---|---|---|
| **M0** | Hormiga's non-UI layers built for Android: domain, render, sync, platform, libsodium from source, Palabra. A headless smoke test run on a device with `adb` | the core is portable, before any screen exists |
| **M1** | the phone shell: join by code, **People** list and detail (read-only), presence, the four-room switcher | "I can look someone up on my phone" |
| **M2** | edits: a contact's fields, quick-add an event, tags with suggestions; errors and undo as snackbars | "I added the person I just met" |
| **M3** | photos: camera and picker (an intent), ingest by content hash, tag, cautious transfer | "the flier I photographed is in the next issue" |
| **M4** | publications: document list, outline, bilingual text edits, image swap, preview (an intent), *Publish now* driven by the desktop | "I fixed a typo and sent the issue from the bus" |
| **M5** | the map: markers, detail sheets, drop a pin (location) | "I can see where things are" |

M0 has no dependency on any upstream answer. M1 needs the navigation stack
(host-side if necessary). M2 needs the accented keyboard. M3 to M5 need
intents.

# Horizons, recorded and not planned

- **The phone as a control surface for a desktop session.** Void Maiz's touch
  page recorded this: two front-ends over one state document is already
  proven (headless). A phone driving a projector-connected desktop at a
  meeting is the DS dual-screen idea, and Hormiga's LAN membership already
  makes it nearly free.
- **A phone that hosts.** Joining through a hotspot needs no code
  ([Void Maiz's LAN transport](../../../../VoidMaiz/okf/concepts/lan-transport.md)).
  A phone *hosting* a database is a different trust decision (keys on a
  device that gets lost), and it is not the author's "diminished" brief.
- **Wi-Fi Direct** between two phones with no router. Void Maiz's call, later,
  and only with two Android devices in hand to test.
