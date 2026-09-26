---
type: Concept
title: Hormiga on a phone
description: "Opened 2026-09-22; scoped by the author 2026-09-23: no Builder, no Map, no visible console, no windows. Since 2026-09-25: eight screens (Data, Calendar, Notes, Migos, Migas, Profile, Settings, Antfarm), any four on a bar the person arranges around a locked centre Hormiga button that opens a gallery of all of them; touch scrolling; photos through the system picker; pictures that sync with a progress bar; .miga files on and off the phone; a phone that may host. BUILT in src/phone/, runnable on any desktop with --phone. The phone is a member doing field work first. Also: why a surface's identity is the concept and not the form, how Allomone should read the form (a Void Maiz predicate, like device \"pen\"), and what still stands between this and an APK (libsodium first)."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-09-23T00:00:00Z
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

# The first phone, and what it found (2026-09-25)

The author ran 0.1.7 on their own phone, a modern all-screen Android with
gesture navigation: *"the mobile app is very bad."* Every item had a cause, and
each is fixed where it lives:

| the author saw | the cause | the fix |
|---|---|---|
| everything very small, no icons | the APK stored its fonts as `assets/fonts\Lato-Regular.ttf` (aapt2 on Windows writes the host's separator), Android found nothing and fell back to ImGui's 13-pixel bitmap face | fonts added by name like the library; `build_apk.ps1` now refuses an APK whose entries Android cannot find; a missing font falls back at the screen's size |
| the top and bottom unreachable, the navigation bar "barely" tappable | the surface runs under the status bar and the gesture strip, and nothing reserved them | Void Maiz's **safe area** (`voidmaiz/safearea.hpp`, `reserve_safe_area`): `MaizActivity.maizSafeInsets()` reports bars, cutout and the mandatory gesture strip, and the shell reserves them before any bar |
| the add button does nothing | Void Maiz's `speed_dial` right-aligned its entries against its own auto-sized width, so they settled at a sliver | one width for the column (fixed in Void Maiz) |
| "did not join. cannot write test/test.miga.part" | the joined-database folder and the profile were guessed from `HOME`/`APPDATA`, which a phone does not have | one owner of this device's folders, `platform/device_paths.hpp`, set by the shell before anything reads it; a phone keeps everything in its app folder |
| list vs card, small rows | the Data screen was thin rows | **cards**, one layout, ~76 dp, tap opens, swipe deletes |

**A phone is now tested on the desktop before it ships.** `voidhormiga --phone
--phone-screen 412x915@2.625 --safe 24,24 --script s.txt <db>` gives the phone
front-end a phone's screen, density and safe area, drives it with scripted taps
through the same ImGui input queue a finger feeds, and writes screenshots
(`main/phone_harness.hpp`). Every fix above was reproduced there first, and a
real join (a host sharing from the CLI, the phone joining by taps) was run
through it end to end. **Still owed:** the author's phone running 0.1.8.

Also for the phone: a fresh install starts EMPTY (not the demo), the Together
screen no longer offers a folder chooser, Me no longer shows desktop hardware,
and a card's photo has no file-path box.

# The second pass (2026-09-25, later)

The author, after 0.1.8: *"the mobile app is ALMOST really good!"*, and six
points. Each is built and was measured in the harness unless it says otherwise:

| the author asked | what it is now |
|---|---|
| "scrolling doesn't work ... this is huge" | Void Maiz's `touch_scroll`: drag to scroll, let go and it glides; a scroll never taps the card it started on. Swipe-to-delete, which had never worked in a list (a shared offset every row reset), works too |
| "images can't be shared or uploaded", with loading bars, and the "allow access to photos" prompt | a photo field has **Choose a photo**: the system's photo picker (Void Maiz's documents holiday). **No permission prompt**, deliberately: the picker needs none, and the pick is the consent ([Q90](/developer_questions.md)). Pictures now sync (four sync bugs, [LAN sharing](/concepts/platform/lan-sharing.md) §3b), and each large transfer has a bar on Migos |
| "networking should be its own tab", with ping and connection strength | **Migos**: who is here, with signal (beacons arriving) and ping (the last handshake); transfers; the sync; sharing from this phone; joining |
| notes on the phone, shareable | **Notes**: the desktop's `note` rune, shared or private by the Antfarm's private tag, first line as title |
| a customizable bar, set in the phone's Settings; one locked centre "Hormiga button" opening a gallery of everything | any four of the eight screens, in the order chosen, saved on this device (`app_settings.phone_nav`); the centre is an ant drawn from shapes (the author may replace it with an icon); it opens **Everything**, a gallery with a pin on what is on the bar |
| "network" instead of "Together"; a database manager on phone and desktop; `.miga` files on mobile | **Migos** is the network ("friends"); **Migas** manages databases: new, open, import, save a copy out, default, remove. Switching saves first. A `.miga` arrives by Import or by "Open with Void Hormiga". The desktop has **File > Databases** |
| a default database of the phone's contacts, later | recorded below under Horizons; not built |

**The screens now:**

| screen | what it does |
|---|---|
| **Data** | as before: search, kind chips, cards, swipe to delete, + |
| **Calendar** | as before |
| **Notes** | cards (title, first lines, a lock or a people mark); tap to write; Shared/Private; tags; + |
| **Migos** | the network (above) |
| **Migas** | the databases (above) |
| **Profile** | the desktop's own profile body |
| **Settings** | the bar (pin, order, defaults), light/dark, "ask before downloading pictures", version and folders, Save now |
| **Antfarm** | its nodes as cards (a lock on those that stay on this device), a node's fields and wires, or the graph on a canvas a finger pans, with zoom buttons |

**The phone may host (explored, and it works in code).** "Share from this phone"
is on Migos. The share code had nothing desktop-only in it except a temp folder
Android does not have; the shell now points `TMPDIR` inside the app. What it
costs is said on the button: the room key lives on the phone. **Not yet run as
a host on a real phone.**

**Still owed:** the author's phone running this. The harness has played every
flow above; a real device has not.

# The author's scope (2026-09-23, decided), and what is built

The author, the next day, after Void Maiz's networking worked across Linux,
Windows and Android:

1. **No Builder.**
2. **No Map.**
3. **No visible console.** *"there's no real reason to have a console (at least
   not visible to the user)"*.
4. **No windows at all**: *"we have a navigation bar instead, where we navigate
   to different 'screens' ... for an actual mobile application, we need a
   navigation bar and real mobile features (not just 'technically correct'
   things)"*. Interaction Combinators' File/Edit/View menu is fine for a demo,
   not for this.
5. **Allomone must be considered across both.** Some interactions should be
   consistent across devices, but windows, ribbons, navigation bars and screens
   are UI concepts that differ. So UI elements will need to be *tagged* as
   mobile or explicitly different, and **that tagging and identity belong to
   Void Maiz, as does the navigation bar**, or at least its core concept.
6. **The Data screen is what gets tested across networked devices**, and the
   Calendar deserves a decent phone overhaul.
7. **No Antfarm** until its redesign.

That narrows the 2026-09-22 plan below. Where the two disagree, this section
wins, and the tables below have been corrected to match. [Q84](/developer_questions.md)
is answered.

**Built the same day** (`src/phone/`, run with `voidhormiga --phone <database>`,
a phone-sized window on any desktop, with `--touch` adding the drawn keyboard):

| screen | what it does |
|---|---|
| **Data** | search; kinds as a sideways-scrolling chip row; one list with avatars and a one-line subtitle; **swipe a row to delete, with UNDO in a snackbar**; tap for the detail; **+** adds a contact, organization or event |
| detail | every declared field through the same widget editors as the desktop (so photos, dates, role pickers and bilingual pairs all work), labels above the fields, tags with suggestions, Delete |
| **Calendar** | a week strip with a dot on every day that has something, arrows by the week, a **Today** button; the chosen day's entries; **one-line quick add** ("Food drive 3pm-5pm"); *Coming up* over the next fortnight |
| **Together** | who is here and on what (with *Go there*), the sync state, and **the desktop's own join flow**, shared rather than copied |
| **Me** | the desktop's own profile editor, light/dark, which database, *Save now* |

**Measured on 2026-09-23, on one machine with two profiles:** the phone
discovered a database the CLI was sharing, asked to join, and received it sealed
("4 files sent, sealed"). The pictures came across. A name edited on the phone
screen (`display_name: "Bandit the Brave"`) was in the host's database after the
host's `lan-stay` session. Presence showed the host "in Data" on the phone. A
quick-added event was minted `food-drive-081f-1`, device-scoped, so it is safe
to sync.

**Seen once and not reproduced:** the first edit typed on the phone was lost
when "a member's changes arrived" during the same seconds. The second attempt
committed. If a merge landing mid-typing drops the field being edited, that is
a real bug for every front-end, and it wants a test that types while a merge
lands.

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

# The phone is a member first

*(Written as "never the host" on 2026-09-22. On 2026-09-25 the author asked to
"figure out how to possibly make the mobile device host a network", and it can
now; see "The second pass". What follows still describes the default.)*

This one rule removes most of the hard problems:

- **The data arrives by joining.** A phone joins a database a desktop shares
  on the LAN ([LAN sharing](/concepts/platform/lan-sharing.md)) and keeps a
  Palabra replica, exactly as a second laptop does. It needs no file picker
  to start. (Since 2026-09-25 it can also take a `.miga` from a file, on
  Migas or by "Open with", for the person who was sent one.)
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
| **Calendar** (3-day / week / month, quick-add, `.ics`) | **yes, overhauled** | built: a week strip, the day's agenda, one-line quick add, *Coming up*. Date and time *pickers* are still Void Maiz's staged items; today the date field is the desktop's three boxes |
| **Territory** (the map) | **no** (author, 2026-09-23) | |
| **Builder** (grid, palette, inspector, preview) | **no** (author, 2026-09-23) | the reimagining below is kept as a design, not a plan |
| **Publish** | **no** | it follows the Builder |
| **Antfarm** | **no, until its redesign** (author, 2026-09-23) | the redesign's A11 (a read-only dashboard) is what it would become |
| **Allomone** | **no editor** | rules still *run*: a card coloured by a rule on the desktop is coloured on the phone. See "Allomone and UI identity" below |
| **Console / command bar** | **not visible** (author, 2026-09-23) | errors become toasts and snackbars, and "Deleted. UNDO" is one widget. The log still records everything |
| **Settings** | **a short list** | profile (name, colour, picture), networking (who I am shown as, cautious files), updates, language |
| **Niche Tools, Style, Data Tools** | **no** | |
| **Share / members** | **join and leave** | join by code on a digit keypad (the code is `maiz::lan::encode_join_code`, and the keypad is Interaction Combinators', an easy lift); the member list (`draw_member_list`); never host |

**Navigation (built 2026-09-23; superseded by the arranged bar above on
2026-09-25, which keeps everything below except the fixed four).** A **bottom navigation bar**, not a segmented
control and not a menu: **Data**, **Calendar**, **Together** (with a badge
counting who is here) and **Me**. Each destination keeps its own stack of
screens (list → detail), so switching away and back keeps your place. A second
tap on the current destination returns to its top. **Back** (Escape, the mouse's
back button, or Android's Back key) pops the stack, and at a root returns to
Data. Both bars *reserve* their edge of the viewport the way ImGui's menu bar
does, so Void Maiz's add button and snackbar sit above the navigation bar
without knowing it exists. The bar lives in `src/phone/nav.hpp`, **written to
be lifted into Void Maiz**, which the author says owns the concept. Asked in
`MESSAGE_FOR_VOIDMAIZ_hormiga-navigation-and-ui-identity-2026-09-23.md`.

# The Builder on a phone

> **Out of scope by the author's decision of 2026-09-23.** This section is kept
> as the design for when it returns. Nothing below is planned.

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

**How it actually landed (2026-09-23), and why it differs from the first
bullet.** The phone is not a separate app object. It is `phone_frame()` on
`HormigaApp`, in `src/phone/`, and `frame()` hands over to it after a shared
`frame_prelude()` (merges landing, jobs reporting, deferred commands). The
reason is that the LAN runtime, the replica, presence and the dispatcher's host
seams all hang off `HormigaApp`. A separate object would have meant untangling
them first, for no gain the author asked for. **What makes it a front-end and
not a skin still holds:** it draws only its own screens, the layering rule is
in (`phone/` may not include `ui/`), and the join flow and profile it shares
with the desktop were split into bodies that both call
(`draw_discover_body`, `draw_profile_body`), not copied. The cost is honest:
`app.hpp` is at 1242 of its 1245-line budget.

# Allomone and UI identity across devices

The author's fifth point is the deepest one: *"i would love for certain
interactions to be consistent across devices, but core UI concepts like
windows, ribbons, navigation bars, screens, makes me think that a lot of UI
elements in the mobile version will need to be tagged with 'mobile' ... this
tagging and identity belong to maiz."*

There are two different things to keep consistent, and they need different
answers:

**1. What a thing IS stays the same on every device.** The Data screen on a
phone and the Data window on a desktop are *the same place*, shown two ways.
Presence already depends on that. So the phone declares **the same surface ids
the desktop does** (`table:data`, `calendar`), and a desktop member sees "in
Data" whichever device someone is on. That is built. The rule it implies:
**a surface's identity is the concept, never the form.** A phone screen and a
desktop window that show the same thing share one id.

**2. How it is SHOWN differs, and Allomone needs to be able to say so.** A
rule like "highlight overdue events in red" should fire on both. A rule like
"show the tag chips expanded" may only make sense on one. That needs the
*form* to be something a rule can read. Void Maiz already has the pattern:
`UserGraph::touch` records the input channel (finger, stylus, mouse) as an
observation, and Allomone's `device "pen"` reads it. **The same shape
answers this.** A surface carries its *form* (window, screen, sheet, panel)
beside its identity, and the application carries its *form factor* (phone,
tablet, desktop, from `classify_layout`). A predicate reads them:

    when form "phone" -> chips 0        # this device is a phone
    when surface "table:data" -> ...    # wherever the Data place is shown

**Why this belongs to Void Maiz, as the author said:** surfaces, the layout
classifier, the UserGraph and the Allomone bridge are all Void Maiz's, and
every Void application with a phone build will need the same predicate.
Hormiga's part is only *which* surfaces it has. **Asked, not built.** Until it
exists, nothing here tags anything "mobile". A tag Hormiga invents now would
be the vocabulary Void Maiz later has to break.

**What stays out of the tagging:** the model. A rune is never "a phone rune".
Data is the same everywhere, and the form belongs to the view.

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
| **the phone's own keyboard** (2026-09-23), as a holiday: the platform keyboard's editing state becomes keystrokes every field accepts, and registration picks the layout (a `phone` field gets a dial pad) | `textinput.hpp`, `textinputview.hpp`, `MaizActivity.java` | every text field. The drawn keyboard (`mobile.hpp`) is now only the fallback |
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
| ~~**safe-area insets**~~ | **built 2026-09-25** (`reserve_safe_area`) |
| action sheet | "Call · Email · Copy" on a contact |
| pull-to-refresh | "sync now", which in a log-first system is a real gesture |
| the stepper as a registry field editor | so a glyph's `hints.editors` can ask for one |
| fling / momentum consumed by the canvas | the map should coast. Lists glide since 2026-09-25 (`touch_scroll`) |

## Missing from Void Maiz (to ask for)

| gap | why it blocks or hurts | severity |
|---|---|---|
| ~~accented letters on the drawn keyboard~~ | **resolved 2026-09-23 by not drawing one.** The author: *"Custom keyboard is too much of a hassle ... we should focus on the integration of the keyboard."* The phone's own keyboard has every accent, and emoji now survive typing too (`IMGUI_USE_WCHAR32`). Not yet typed on a device | resolved, unwitnessed |
| **a navigation stack**, and ~~the system Back button~~ | Back is **done in Void Maiz** (2026-09-23): ImGui's android backend mapped no `AKEYCODE_BACK`, and `maiz::android_system_key` now turns it into `ImGuiKey_AppBack`, which Hormiga's phone already reads. The stack itself is still Hormiga's (`phone/nav.hpp`), offered upstream | Back resolved, unwitnessed; the stack is still asked |
| **platform intents**: open a URL, share, `tel:` and `mailto:`, the photo picker, the camera, location | "tap to call", "photograph a flier", "drop a pin where I am" and the Builder preview all need one. Each is a JNI call into the Java runtime, the same shape as the update client's `PackageInstaller` path | **blocking for jobs 1, 3, 5, 6** |
| **app lifecycle**: pause, resume, low memory | a backgrounded phone must save its replica and drop links cleanly, then come back. `idle_ms` already makes a silent link honest, and saving on pause is the other half | high |
| **a general HTTP seam** | `update.hpp` has `android_http` (`HttpURLConnection` through JNI) *inside* the update client. Hormiga's HTTP shells out to `curl`, which a phone does not have. Publishing, tiles and `.ics` feeds need the seam, not the updater | high, for publish and the map |
| text selection and copy | copying a phone number out of a note | medium; the touch page says "not planned without a client", and Hormiga is that client |
| ~~the Storage Access Framework (open or save a file)~~ | **built 2026-09-25**: `voidmaiz/documents.hpp` (the photo picker, open a document, save a copy to, "Open with"), which is how `.miga` files and photos get on and off a phone | resolved |

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
| **D** (done 2026-09-23) | the phone front-end on the desktop (`--phone`): navigation bar, Data, Calendar, Together, Me; join and sync measured between two profiles | the phone's UX and networking, before any APK |
| **M0** (built 2026-09-24, not run on a device) | Hormiga's non-UI layers built for Android: domain, render, sync, platform, libsodium, Palabra. **All of Hormiga compiled for arm64 with one fix** (a format string the NDK rejects), so M0 and the shell arrived together: `android/` builds a signed APK of the phone front-end. Still owed: a run on a device with `adb` | the core is portable, before any screen exists |
| **M1** | the phone shell: join by code, **People** list and detail (read-only), presence, the four-room switcher | "I can look someone up on my phone" |
| **M2** | edits: a contact's fields, quick-add an event, tags with suggestions; errors and undo as snackbars | "I added the person I just met" |
| **M3** | photos: camera and picker (an intent), ingest by content hash, tag, cautious transfer | "the flier I photographed is in the next issue" |
| **M4** | publications: document list, outline, bilingual text edits, image swap, preview (an intent), *Publish now* driven by the desktop | "I fixed a typo and sent the issue from the bus" |
| **M5** | the map: markers, detail sheets, drop a pin (location) | "I can see where things are" |

**The keyboard (2026-09-23).** Hormiga's phone takes the platform keyboard
when its shell has one: `enable_phone(touch, maiz::android_text_input(activity))`.
Its own fields declare their kind (search → Search, quick add → Go), the
registry's fields declare theirs from their keys, and the screen ends where the
keyboard begins, so a field being typed into scrolls into view above it. On a
desktop there is no platform keyboard and nothing changes. **Since 2026-09-23, M4 and M5 are out of scope** (no Builder, no Map), and stage D is done. M0 has no dependency on any upstream answer. M1 needs the navigation stack
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
  device that gets lost). **Explored and built as an option on 2026-09-25**
  at the author's request ("Share from this phone" on Migos); what is still
  open is running it on a real phone, and whether a hotspot the phone itself
  makes carries the beacon.
- **The phone's contacts as a database** (the author, 2026-09-25: *"a default
  database be one of your contacts on your phone"*, a future plan). What it
  would take: Android's `READ_CONTACTS`, a real runtime permission prompt
  (unlike the photo picker there is no permission-free contact picker for a
  whole address book), read through the documents holiday's Java side. The
  shape it should have: a **read-only projection** of the address book as
  `contact` runes, in a database of its own that is **never shared** unless
  the person copies a contact into a shared one. Otherwise a phone joining an
  organization's database would carry its owner's whole address book to every
  member. The "database of databases" the author mentioned in the same breath
  (a database that lists and links others) is the natural home for it, and is
  also later.
- **Wi-Fi Direct** between two phones with no router. Void Maiz's call, later,
  and only with two Android devices in hand to test.
