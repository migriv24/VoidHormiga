---
type: Reference
title: Calendar — the feature roadmap (a living checklist)
description: "The Calendar's do-list, reframed around the author's 2026-09-10 direction: Hormiga is the COMPATIBLE calendar, a hub that speaks every other calendar's language rather than a calendar that wants to replace them. The X-track (exchange: the iCalendar lens, conformance, time, import, subscriptions, two-way) and the continuing C-track (the room: navigation, quick-add, spans, recurrence). The design lives in calendar.md; this is the do-list."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-09-10T00:00:00Z
---

The design and rationale live in [calendar.md](/concepts/sections/calendar.md);
**this file is the do-list** — every planned Calendar feature with a status, so
the next thing to build is always visible. Statuses: ✅ built · 🔨 partial · ⬜
planned. Same shape and same purpose as the
[Builder roadmap](/concepts/sections/builder-roadmap.md).

Opened 2026-09-10 on the author's direction, which is an **identity statement**
and not a feature request:

> "We are not trying to make the calendar on Hormiga *the super calendar with
> everything*, but rather **the compatible calendar, that can use any other
> existing calendar system and integrate it**. […] This calendar is a hub of
> all other possible calendars. We are NOT exclusive. We are the opposite of
> Apple or Microsoft for this."

# The correction that reorders this list (author, 2026-09-11)

> *"We shouldn't NEED other calendars. Just like the database, where we have a
> local version of our own data, the calendar data doesn't NEED to live
> somewhere outside of us. We can also create it."*

**Compatible is not dependent, and the X-track had started to read as though it
were.** Founding commitment 2 is that local-first is the *resting state*: the
default install works forever with no network, and the network is something an
admin adds. The calendar is no different from the database. Import and export
are doors; they are not the floor.

**The proof that this needed saying is one measurement.** Give Hormiga a
community organization's most ordinary recurring thing —

```
rune new event standing
set standing title_en "Riverton Community Meeting"
set standing days "Last Friday of the Month"
```

— and it appears **nowhere**. Not on the month grid, not in week view, not in
the agenda, not in the `.ics` export, not to a subscriber. `days` is free text
that the newsletter prints and the calendar cannot read, and `cal_entries_on`
needs a parseable `date`, which a recurring event does not have.

Meanwhile [X3](#x3) will happily import a recurring event from Google and store
its `RRULE`. **So today Hormiga can read somebody else's standing meeting and
cannot express its own.** That is exactly backwards, and it is a hole in our
calendar rather than an argument for anybody else's.

So the order changes: **C3a is no longer item 4 on the recommended list, it is
next.** The rule this establishes for everything after it — *a capability we
can only get by importing it is a missing feature, not an integration* — is
worth more than the item.

# What the hub reframe actually changes

[calendar.md](/concepts/sections/calendar.md) already grounded the model in RFC
5545 and called `.ics` "the calendar's cloud-interop holiday". That was right,
but it was filed under **C5 — exports**, one item among five tracks, as though
interoperability were an output format. Under the author's framing it is the
**thesis**, and the do-list has to be organized around it. So:

- the **X-track** (new, below) is the exchange — the hub identity, built out;
- the **C-track** (continuing, from `calendar.md`'s inventory) is the room —
  the in-app surface, which the author separately notes has "a lot to be
  desired for the user experience."

Both are real work and neither waits on the other.

## The pivot rule is why this is cheap

A hub that speaks *N* calendar systems is **N² adapters** if the adapters are
written pairwise, and **N** if there is a pivot format in the middle. Void
Hormiga already has both halves of that argument written down:

- [Void Reyna](/concepts/projects/void-reyna.md) is where "holidays taken
  seriously as the transformation primitive" lives, and it states the **pivot
  rule** outright — *never write a direct A→B adapter when A→pivot→B exists* —
  along with the shape that makes it work: a holiday is *an effect boundary
  plus a pure `Lens`*, and lenses compose.
- [The Antfarm](/concepts/platform/antfarm.md) already renders one rune per
  backend with typed ports and live status. `hol_sheets` is a cloud **record
  source**; a calendar feed is a cloud record source. The mechanism exists.

**The pivot is the dated rune, and RFC 5545's `VEVENT` is its interchange
serialization.** Write the `VEVENT ⟷ rune` lens once, and Google Calendar,
Outlook, iCloud, Nextcloud, Radicale, Fastmail, a school district's published
feed, a `.ics` a partner emailed, and a Meetup page all become *transports*.
None of them ever needs to know what a Hormiga `event` is. That is what makes
"we are not exclusive" a **property of the mechanism** rather than a promise —
the same move the [download page](/concepts/platform/download-page.md) made when
"never hide a platform" became platform sets instead of a rule someone has to
remember.

It also sets the boundary. `calendar.md` says **not a scheduler** — no
invitations, no attendees, no free/busy. That stands, and the hub framing
sharpens it rather than loosening it: *we translate and aggregate; we do not
negotiate.* A hub that started sending iTIP invitations would be competing with
Google Calendar, which is exactly what the author said not to do.

# The field: what everyone else does, and what we take

Researched 2026-09-10. Listed because the author asked, and because each row
answers "do we need to build this?" differently.

| system | what it is | what we take from it |
|---|---|---|
| **RFC 5545 (iCalendar)** | the interchange format everything speaks | **the pivot.** Already our model grounding |
| **RFC 4791 (CalDAV)** | read/write sync over WebDAV; iCloud, Fastmail, Nextcloud, Radicale, Baïkal, SOGo | **the two-way write path** (X6). XML-heavy, discovery-by-convention, per-server quirks — but it is the only *open* write standard |
| **ICS subscription (`webcal:`)** | poll a URL, read-only | **the highest-leverage feature we can build** (X4). Zero credentials, universal |
| **Google Calendar API v3** | the proprietary one | read via its **secret ICS address** (needs nothing); write needs OAuth — see X6 and [Q69](/developer_questions.md) |
| **Microsoft Graph / Outlook** | the other proprietary one | published-calendar ICS for read; Graph is the same OAuth problem as Google |
| **libical** (MPL-2.0 / LGPL-2.1) | the reference C implementation; Evolution, KDE Kontact, Cyrus, Fantastical | a **correctness oracle**, not a dependency — see the vendoring note below |
| **Nextcloud Calendar** (AGPL) | self-hosted groupware calendar | the model of ICS import/export as a first-class button, not a menu afterthought |
| **Radicale / Baïkal** | tiny self-hosted CalDAV servers | the realistic X6 test targets — an org can stand one up in an afternoon |
| **DAVx5 / Etar / Fossify Calendar** | Android; sync-adapter vs direct | the demonstration that *the sync layer is separable from the view layer*, which is our section/holiday split |
| **Morgen, Cal.com, Rye** | commercial "calendar hub" products | confirmation the category exists; their answer is a cloud account, ours is local-first, which is the whole difference |
| **FullCalendar** (MIT) | the view vocabulary | already our grounding; the [C5d decision](/concepts/sections/calendar.md) not to vendor it stands |

**On vendoring libical.** Ground rule 5 says *vendor, don't depend*, so it is
allowed. The lean is **no, write ours, and use libical as the oracle.**
Reasons: it is a CMake project with generated sources and an optional ICU
dependency, which is a different kind of vendoring than SQLite's one
amalgamation `.c`; [Q67](/developer_questions.md) is currently open *because* a
vendored artifact turned out to be less portable than it looked, and this repo
now builds on three platforms it did not build on a week ago. Against that, the
subset we need — unfold, parse properties and parameters, `VEVENT`, `RRULE`,
`VTIMEZONE` for one zone — is small and testable, and a hand-rolled parser can
be *forgiving* in the way a hub must be (see X3). The honest use of libical is
as a **conformance check in tests**: generate our `.ics`, and if a maintainer
has libical installed, assert it parses. Revisit if `RRULE` expansion or
`VTIMEZONE` turns out to be the tar pit the research suggests it can be.

---

# X-track — the exchange

## ✅ X0 — the lens: `VEVENT ⟷ dated rune`

**The keystone; everything else in this track is a transport onto it.**

- ✅ **the writer half** (2026-09-10) — [domain/ical.hpp](src/domain/ical.hpp).
  It was inline in `site.cpp`, inside the loop that builds the web embed's JSON,
  which is exactly why it could only ever serve one caller. Now: an `ical::Event`
  type, `fold`, `escape_text`, `public_categories`, `to_vevent`, `to_vcalendar` —
  pure, no ImGui, no HTML, no I/O, in `domain/` and therefore reachable from
  anywhere. `render/site.cpp` calls it instead of containing it.

  **`Event` is the privacy seam, by shape.** It carries exactly the fields that
  may leave the machine, for the reason
  [published.hpp](src/render/published.hpp) gives about its own `Person` having
  no `notes` member: *"a field that does not exist on the type cannot be leaked
  by a future caller who forgets, and cannot be added by accident."* A caller
  fills each field deliberately.
- ✅ **the reader half** (2026-09-11) — `ical::parse()` → `Incoming`, and
  [ical_import.hpp](src/domain/ical_import.hpp)'s `plan_import()` →
  **dispatcher commands**, because founding commitment 1 says every change is a
  logged, replayable command and an importer that wrote rows directly would be
  the first thing in the application that isn't. See X3.

## ✅ X1 — conformance repairs on the writer (2026-09-10)

Four defects confirmed by reading the code, then fixed and pinned. They came
first because a subscriber whose client chokes on our feed never reaches the
interesting features.

**Measured rather than asserted.** A conformance checker run over the real
rendered output says it plainly — the old writer emitted a **142-octet**
`SUMMARY` line (the limit is 75) and `UID:potluck@voidhormiga` built from the
editable rune name; the new one passes every check with the same fixture.

- ✅ **X1a — fold lines at 75 octets** (RFC 5545 §3.1). There is no folding
  anywhere in the writer. A long `SUMMARY` emits an over-length line that
  strict parsers reject, and the fold must count **octets, not code points**,
  without splitting a UTF-8 sequence — which this application hits immediately
  rather than theoretically, because it is bilingual by decision (log,
  2026-09-02) and every Spanish title carries multi-byte characters.
- ✅ **X1b — `UID` from the frozen `spirit.id`, not the editable `name`.**
  [site.cpp:1849](src/render/site.cpp#L1849) builds `UID:` from `dn.name`. Void
  Core's [rune spec](../VoidCore/okf/concepts/rune.md) is explicit that `id` is
  "minted once, never reused" and `name` is "editable". So **renaming an event
  today tells every subscriber the old event was deleted and an unrelated new
  one created.** `maiz::SceneNode` already carries `id` — the fix is one field.
  Existing feeds change UID once, unavoidably; do it now, while the number of
  subscribers in the world is approximately zero.
- ✅ **X1c — the `VCALENDAR` header.** It currently carries `VERSION` and
  `PRODID` and nothing else. Add `CALSCALE:GREGORIAN`, `METHOD:PUBLISH`,
  **`X-WR-CALNAME`** (the name a subscriber's client displays — for a hub this
  is not cosmetic, it is how a person tells four subscribed calendars apart),
  `X-WR-TIMEZONE`, and both `REFRESH-INTERVAL;VALUE=DURATION:PT1H` and
  `X-PUBLISHED-TTL` (Outlook reads the `X-`, everyone else reads the standard
  one; emitting both is the compatible answer and costs one line).
- ✅ **X1d — the `VEVENT` body.** Present: `UID`, `DTSTAMP`, `SUMMARY`,
  `LOCATION`, `DTSTART`, `DTEND`. Missing and each one already backed by a
  field we have: `DESCRIPTION` (from `summary_en`/`summary_es`, language chosen
  at the same seam the site uses), `URL` (deep link to the published page),
  `GEO` (the `geo` field, which Territory already parses), `CATEGORIES` (our
  **tags** — the natural mapping, and the one that round-trips), and
  `LAST-MODIFIED` + `SEQUENCE` so an edit reads as an *update* rather than a
  duplicate. Plus **`STATUS:CANCELLED`** — a cancelled event that simply
  vanishes from the feed stays on every subscriber's calendar forever; a
  tombstone is how you actually cancel something.
- ✅ **X1e — privacy at this seam, and it is an ALLOWLIST.** `CATEGORIES` from
  tags is exactly the shape of thing that leaks an internal axis onto a feed
  strangers poll. The decision that fell out of building it, and the one worth
  remembering: **a denylist here is unsafe by construction** — it can only
  exclude the namespaces that existed when it was written, so the first internal
  axis an organization invents is published to the world by default. So only
  `kw:` — the keyword axis, the one that exists to describe subject matter and
  the one `effect read-flier` proposes into — reaches a feed, prefix stripped.
  `clearance:`, `status:`, `type:` and everything an organization coins stays
  home. Widening it is an edit to one function under a comment saying so, which
  is `published.hpp`'s enforcement-by-shape and the reason rule 6 says the check
  lives at the seam rather than in a convention. Tested.

## ⬜ X2 — time, honestly (the org timezone)

The writer emits **floating** times — no `TZID`, no trailing `Z`, no
`VTIMEZONE`. The research is unanimous that this is where every "off by N
hours" bug lives: a floating time means "whatever o'clock it is where the
reader is," which is invisible until the first subscriber is in another state.

The v1 that is honest without being a tzdb project:

- all-day stays `VALUE=DATE` (already correct, including the exclusive `DTEND`,
  which the code already gets right and comments).
- timed events get an explicit `TZID` plus a **minimal inline `VTIMEZONE`**
  from **one org-level timezone** — `config set org.timezone
  "America/Los_Angeles"`, alongside the `deploy_cmd` and `tools.image_text`
  settings that already live there.
- we do **not** vendor the IANA tzdb. A `VTIMEZONE` for one zone with its two
  DST transition rules is roughly a dozen lines of static text. Ship a table of
  the ~40 zones a US / Latin-American community organization actually uses; for
  anything outside it, fall through to floating **with a visible warning rather
  than silently**. Shipping the compatible 95% and *saying* where it stops is
  the same posture as the download page saying what SmartScreen will say.
- `RRULE`'s `UNTIL` must be UTC when `DTSTART` carries a `TZID` — a
  specification rule that the research shows real projects (Nextcloud among
  them) have shipped bugs against. Worth a test the day X2 and C3a meet.

Opens **[Q68](/developer_questions.md)** — is an outreach org's calendar ever
genuinely multi-timezone? Lean: no; one org timezone, stated once, with a
per-event override field reserved and unused.

## ✅ X3 — the importer: read any calendar (2026-09-11)

`effect import-ics <path|url> [apply]`. **Measured against real feeds rather
than synthetic ones:** Google's public US-holidays calendar (317 entries, all
all-day) and FOSDEM 2025's schedule (580 KB, 1,105 timed talks across parallel
tracks) both parse with **zero failures, zero unparseable dates and zero missing
UIDs**, and both re-import to `0 create / 0 update / N unchanged` — a fixed
point, which is the property that makes a subscription safe to re-run.

Two bugs found by running it rather than by reading it, both worth keeping:

- **The importer never reached a fixed point**, reporting 174 updates every time
  for a file that had not changed. The cause was one function doing two things
  wrong at once: a hand-rolled quoter flattened newlines to spaces on the way
  in, while the comparison used the *unflattened* parsed value — so the value
  written could never equal the value compared. Google's `DESCRIPTION` carries a
  newline ("Observance" then "To hide observances…"), which is why 174 and not
  all 317. Fixed by using `maiz::arg` — Void Core's own `vc_arg_quote`, SPEC
  §6.1 — which round-trips a newline intact (checked: a `set` carrying one
  stores one). One change, two bugs: a silent data loss and a plan that could
  not converge.
- **`ext_uid` and `rrule` were caught by `lint_glyph_fields`** the moment they
  were declared, which is that linter doing exactly its job. Both are
  deliberately unrendered and are now exempt *with reasons*: publishing a
  foreign system's opaque identifier tells a reader nothing and tells a scraper
  which feed the organization subscribes to, and a renderer must not print
  `FREQ=MONTHLY;BYDAY=3TU` at a person.

*Original plan, for the record:*

`effect import-ics <path>` → a **preview** → one replayable batch.

- **Parse:** unfolding, escaping, `VEVENT`, `DTSTART`/`DTEND`/`DURATION`,
  `VALUE=DATE`, `TZID` / `Z` / floating, `SUMMARY`, `DESCRIPTION`, `LOCATION`,
  `GEO`, `URL`, `CATEGORIES`, `UID`, `RRULE`, `EXDATE`, `RECURRENCE-ID`,
  `STATUS`.
- **Ignore gracefully:** `VTODO`, `VJOURNAL`, `VFREEBUSY`, `VALARM`, unknown
  `X-` properties, unknown parameters. **A hub must never reject a file for
  containing something it does not model** — that is the Apple/Microsoft
  behavior the author is defining us against, and it is a one-line policy
  decision that has to be made deliberately in the parser's default branch.
- **Identity is where a hub is won or lost.** The foreign `UID` is stored on the
  rune (a new `ext_uid` field on `event` / `incident`) plus a `cal:<source>`
  tag. Re-importing the same feed **updates and does not duplicate**. This is
  precisely the discipline [data planes](/concepts/platform/data-planes.md)
  already states for contacts — *claiming a contact must not search the
  database* — applied to events: match on the foreign key you were given, never
  on a fuzzy title-and-date guess.
- **Import proposes; `apply` writes.** The same posture `effect read-flier`
  takes ("proposes only: it dispatches nothing") and the sync effects take
  ("every one of these REPORTS by default and writes only on `apply`"). A
  calendar import that silently writes four hundred events into an
  organization's database is the failure mode worth designing against, and this
  codebase has already decided how.

## ⬜ X4 — subscriptions: `hol_ics_feed`

**The highest-leverage item in the entire track, and it needs no credential
from anyone.** Google, Outlook, Apple, Nextcloud, Radicale, Meetup, Eventbrite,
city councils, school districts and library systems all publish an ICS URL. One
holiday reads all of them.

A new Antfarm node, registered in the existing table in
[glyphs_antfarm.hpp](src/domain/glyphs_antfarm.hpp) with no new mechanism —
`CLOUD` colored, a `records`-typed port, exactly like `hol_sheets`:

- fields: `url`, `refresh` (cadence), `tag` (the `cal:<source>` namespace this
  feed owns), `read_only` (default on).
- subscribed runes are **not owned**: read-only in the inspector, badged with
  their source in the grid, and a re-fetch overwrites them. What makes that
  safe is a single invariant — **the holiday only ever touches runes carrying
  its own `cal:` tag.** Hand-made events are structurally out of its reach.
- refresh is a **request the operator made**, not a background poll — the same
  rule [distribution](/concepts/platform/distribution.md) turns on (*a check is
  a network request a person did not make*), which means the cadence preference
  starts unasked and a manual "Refresh" is always the honest default.

**This is also the entire Google Calendar read integration.** Every Google
calendar has a "secret address in iCal format"; every published Outlook
calendar has one too. Paste it into `hol_ics_feed` and you have Google Calendar
in Hormiga with zero OAuth, zero client secret, and nothing to revoke. It
should be documented as the *recommended* path, with the API as a power-user
upgrade rather than the headline.

## 🔨 X5 — be subscribable: publish the other direction

We already write `site/calendar.ics` beside the page (built 2026-07-22), and
phase E can deploy that site to Cloudflare Pages or GitHub Pages natively. So
the feed URL that makes Hormiga subscribable *from* Google, Apple and Outlook
is one deploy away. What is missing to make it real:

- ⬜ **one feed per `calview`** (`site/calendar-<view>.ics`). C4d already built
  saved calendar views carrying their kind + tag filter, and already wires that
  filter into the PNG export so the privacy choice is baked into the artifact.
  Do the same for `.ics`: "Public Events" becomes the thing the world
  subscribes to, and the filter is a property of the file rather than of the
  request. **This is the privacy prerequisite for any published calendar** —
  `calendar.md` already says a published calendar blocks on it.
- ⬜ **`webcal://` links** on the page beside the existing per-entry "add to
  Google Calendar" template links, plus a copy-the-URL affordance. A
  one-click-subscribe is the difference between a feed that exists and a feed
  anyone uses.
- ⬜ the refresh properties from X1c, so clients do not poll daily and call us
  stale.
- ⬜ **`calendar_embed` referencing a calview by name**, the way `map_embed`
  references a map view — carried over from `calendar.md`'s open C4d item, and
  the same job as the per-view feed.

## ⬜ X6 — two-way, and the credential problem

**CalDAV first, Google's API second**, and the ordering follows from a founding
rule rather than a preference.

- **CalDAV (RFC 4791)** is the open write standard — Nextcloud, Radicale,
  Baïkal, SOGo, Fastmail and iCloud all speak it, and most accept an **app
  password**, which is a string in the passphrase-locked vault: exactly the
  `token_key` shape `hol_github` already established, with the same
  "…or a file beside the database" second door. A `hol_caldav` node with
  `url` / `user` / `token_key` / `collection`. Realistic first test targets are
  Radicale or Baïkal, because an org can stand one up in an afternoon.
- **Google Calendar API v3 needs an OAuth client id and secret.** The repo root
  is a public artifact (ground rule 2) and a desktop binary is not a secret
  store — the research is blunt that mobile and desktop apps are *public
  clients* and must not rely on an embedded secret. There is no version of
  "ship a Google integration" that does not confront this. The two honest
  options are (i) **the operator brings their own** Google Cloud client, the
  same posture `config set tools.image_text` already takes for the OCR
  recognizer the binary deliberately does not contain, or (ii) we do not ship
  it and X4's secret ICS address is the Google story. Lean: **(i)**, stated
  plainly in the UI, with (ii) as the path that needs none of it.
  Opens **[Q69](/developer_questions.md)**.
- **iTIP / iMIP invitations (RFC 5546 / 6047) are out of scope**, and stay out.
  `calendar.md`'s *not a scheduler* boundary is the reason, and the hub framing
  reinforces it: we translate and aggregate, we do not negotiate meetings.

---

# C-track — the room (continuing)

The author's other note: *"there's still a lot to be desired for the user
experience."* These are `calendar.md`'s existing C-items, ranked by what an
operator hits first rather than by track number. The already-built ones are
listed in [calendar.md](/concepts/sections/calendar.md) and not repeated here.

- ✅ **C0 — the grid and the `.ics` export agree about what time it is**
  (2026-09-10). **Not on any list, because nobody knew.** The calendar grid
  parsed times with `sscanf("%d:%d")` while the renderers have used
  `parse_clock` since 2026-08-19, and `render/text.hpp` states the case that
  makes the divergence matter: *"Every time in a real community database is
  12-hour with a meridiem, because that is what a flier prints."* Against that
  input the grid read **`"3:00 PM"` as three in the morning** — it took the 3,
  discarded the meridiem, and drew the block twelve hours early — and read
  **`"9 AM"` as no time at all**, dropping a timed event off the grid into the
  all-day chip lane. The export got all three right and the screen got all three
  wrong, which is the worst arrangement available: what you verify is correct
  and what you look at is not.

  The fix is one parser, and making it *shareable* was the actual work.
  `parse_clock` was already pure, and still unreachable: it lives in
  `render/text.hpp`, whose own docstring promises "no `HormigaApp`, no ImGui, no
  I/O" — true of every function in it and false of the header, which includes
  `app/app_internal.hpp`. So the calendar could not use the one time parser
  without dragging a window in behind it, **which is why it grew a second,
  worse one.** `parse_clock` now lives in
  [domain/clock.hpp](src/domain/clock.hpp), which includes `<cctype>` and
  `<string>` and nothing else. Same discipline the OKF already applies to
  styling, where the value is "the *absence* of a second styling system" — and
  the header this one will be reached from when [X0](#x-track--the-exchange)
  builds the iCalendar lens.
- ✅ **C2c — jump-to-date and keyboard strides** (2026-09-10). The month/year
  label is now a button that opens a date field (`2026-12-01`, `12/1/2026`,
  `12/1`, or a bare `14` for this month). Keys, when the section is focused and
  nothing is capturing text: arrows a day, shift+←/→ and ↑/↓ a week, PgUp/PgDn a
  month, Home or `T` today, `G` jump, `N` quick-add, `M`/`W`/`D`/`A` the four
  granularities, Escape deselect.
- ✅ **C1f — quick-add** (2026-09-10). A toolbar box: one line, Enter, a named
  dated rune — `Food drive 3pm-5pm`, `Standup 9am`, `Volunteer training`,
  `!Road closure 2pm` for an incident. It targets the **focused day**, which
  clicking a cell or the arrow keys move and which the month grid now marks, so
  where an entry will land is never a guess. The grammar is
  [quick_add.hpp](src/domain/quick_add.hpp) — pure, linking nothing, tested in
  [calendar_smoke.cpp](tests/calendar_smoke.cpp).

  **The interesting half is what it refuses.** `parse_clock` exists to read
  fliers and is far too permissive to drive a creation gesture: it finds a time
  in "Ward 5 meeting". So a trailing token counts as a time only if it carries a
  colon or a meridiem, or is `noon`/`midnight`, and a line that is *only* a time
  is rejected outright rather than minting an all-day event called "3pm". A
  quick-add that guesses wrong is slower than one that does nothing — you have
  to notice, undo, and retype — and those negative cases are most of the test.
- ✅ **Better creation gestures** (2026-09-10) — the author's ask for "better
  ways to do things, or better gestures that actually create an event":
  **double-click an empty day cell** in month view for an all-day event, and
  **double-click empty time** in week/3-day for a one-hour one. Both existed
  only as right-click → menu → rename-in-inspector. Drag-to-size is still the
  better gesture when you know the length; double-click is the one people try
  first, and a short decisive click used to do nothing at all. Single click on
  a day cell now moves the focus (what quick-add and the keys act on).
- ⬜ **C3b — multi-day spans.** An event spanning days currently appears only on
  its first day, so a two-day festival is invisible on its second. Also the
  thing that makes imported feeds render correctly, since multi-day `VEVENT`s
  are common in the wild.
- ✅ **C3a — recurrence, AUTHORED HERE** (2026-09-11) —
  [domain/rrule.hpp](src/domain/rrule.hpp). A recurring event is `date` +
  `rrule`, which is what RFC 5545 says and what our two fields already were, so
  this is not a second recurrence model beside the interop one — **it is the
  interop one, authored from our side.** The grid expands occurrences as a
  *projection* (never stored runes: `calendar.md`'s rule is *recurrence is
  modeled, not simulated*), and the export emits **one VEVENT carrying the
  rule** rather than one per occurrence. Round trip verified exact: a monthly
  meeting exports as one event and re-imports as one event, not twelve.

  The authored vocabulary is deliberately the subset every client honours —
  every N days, weekly on chosen weekdays, monthly on a date, monthly on the
  Nth (or last) weekday, yearly — because this roadmap's own research says
  *"Outlook desktop is the strictest… stick to weekly/monthly with simple BYDAY
  clauses."* A richer rule arriving from a feed is **kept verbatim, expanded if
  it is one of these shapes, and otherwise shown on its start date with a plain
  sentence saying so.** A wrong date is worse than an honest absence, and
  silence is worse than both.

  Two things the tests caught before anything was wired to them: the 31st is
  **skipped** in a 30-day month rather than clamped (RFC 5545 §3.3.10 — "the
  31st" in November is nothing, and clamping silently invents a meeting), and
  `add_days` silently ignored negative deltas, which anchored every weekly rule
  to the wrong weekday by a consistent offset.
- ⬜ **`days` → `rrule` for data that already exists.** The legacy `days` field
  is free text a newsletter prints ("Last Friday of the Month"); it is **not**
  auto-converted, so an organization with recurring events written that way
  still sees nothing on the grid until an `rrule` is set. Parsing the common
  phrasings and *offering* the rule — proposing, not rewriting — is the next
  step and is small now that `rrule::parse`/`describe` exist.
- ⬜ **C3a (original entry) — recurrence.** `days` renders as concrete occurrences, per
  `calendar.md`'s standing rule: *recurrence is modeled, not simulated* — no
  phantom entries. **Sequence this together with X3's `RRULE` parsing**: they
  are the same code, and building them apart means writing an expander twice
  and having the imported one disagree with ours. This is also the point at
  which imported feeds stop being lossy, because real feeds are full of
  `RRULE`s and today we would have to drop or explode them.
- ✅ **the three confirmed time-grid defects, fixed** (2026-09-10). All three
  were entries that were *neither shown nor accounted for*, which is the worst
  way for a view to run out of room — the operator has no way to learn the entry
  exists. All three also get worse the moment X4 starts pouring feeds in.
  - **overlap lanes.** Every timed block used to span the full column width, so
    two events at 3pm drew one exactly on top of the other and the earlier was
    not hidden but *unreachable* — it could not be clicked. Now: entries group
    into clusters of transitively-overlapping blocks, each takes the first lane
    free at its start, and the column divides by the lane count **of its own
    cluster** — per-cluster rather than per-day, so one 9am collision does not
    shrink an empty afternoon to half width. Labels clip to their lane.
  - **the all-day lane counts its overflow.** `if (++shown >= 2) break;` became
    a per-column "+N more" / "less" toggle.
  - **the hour range fits the day.** 06:00–22:00 is still the resting range —
    it is right for almost every community organization and a full 24 rows waste
    half the screen — but it now *expands* to contain whatever the visible days
    hold, so a 05:30 setup call is drawn at 05:30 instead of clamped onto the
    edge of the grid at 6. A **24h** checkbox forces the whole day for anyone
    who would rather have a stable grid than a fitted one.
- ✅ **the month grid caps at four with "+N more"** (2026-09-10). It drew every
  entry, so a day with eleven things on it silently grew the whole week's row
  and pushed the rest of the month off screen.
- ⬜ **source badges and per-source visibility** — the hub's own UI need,
  falling out of X4: a checkbox list of subscribed calendars in the toolbar,
  each with its color, each toggleable. This is what every calendar client
  calls "my calendars," and it is the one piece of hub UX that has no analogue
  in the current single-source design.
- ⬜ **C2b — year view as a density heat map**, GitHub-contributions style,
  clicking into month. The author's "idk how we can do that"; the grounding is
  already recorded in `calendar.md` and it is genuinely cheap.
- ✅ **C5b — `.ics` export from the Calendar** (2026-09-10) — a toolbar button
  and `effect export-calendar-ics`, so handing a partner a calendar no longer
  means deploying a website first. It cost about thirty lines, which is the
  return on [X0](#x-track--the-exchange) collected: the folding, the UID, the
  all-day `DTEND` and the tag allowlist are written down once. It exports what
  the **filter** shows rather than what the viewport shows, so a saved "Public
  Events" calview bakes the privacy choice into the file.

  It also found an older defect: **`effect export-calendar` (the PNG) is in the
  desktop effect table and has never been in the headless one**, so from the CLI
  it has answered `done` and written nothing for as long as it has existed —
  founding commitment 1's "three callers of the same verbs" being false. The
  `.ics` goes through `render_from_state` instead. The PNG stays desktop-only
  because it blits a baked ImGui font atlas, which is a real difference and is
  now said out loud in both files.
- ⬜ **C5a — week / 3-day PNG export** (month is built).
- ⬜ **C4b / C4c — temporal-linkage badges and relation jumps** (an event
  `responds-to` an incident).
- ⬜ **per-`calview` rules editor** — the `rules` field exists on the `calview`
  glyph and is reserved; styling still shares the map's rules.

---

# Recommended order

> **Reordered 2026-09-11.** Authoring first, interop second — see the correction
> at the top. What was item 4 is now item 1, and X4 waits until Hormiga's own
> calendar can express what it is being asked to subscribe to.

0. **C3a + C3b — recurrence and multi-day spans, authored HERE.** The calendar
   cannot currently state a standing monthly meeting. Nothing else in this list
   matters as much as that.

1. **X0** — extract the lens. Nothing else in the track is reachable through a
   renderer-internal function, and it changes no behavior.
2. **X1** — the four confirmed conformance defects. Cheap, verifiable, and they
   gate everything downstream; X1b in particular gets cheaper the earlier it
   happens.
3. **C2c + C1f** — the two navigation/creation gaps an operator hits daily.
   Small, independent of the X-track, and they answer the "UX is missing" half
   of the direction immediately.
4. **X3 + C3a/C3b together** — the importer and recurrence/spans as one job,
   because they share the expander.
5. **X4** — `hol_ics_feed`. The moment this lands, Hormiga *is* the hub in the
   sense the author means, and Google Calendar arrives with it for free.
6. **X2** — the org timezone, needed before any feed we publish crosses a state
   line, and before X6 writes anything anywhere.
7. **X5** — per-calview feeds and `webcal://`; the outbound half.
8. **X6** — CalDAV, then the Q69 answer about Google.

# Where this sits

Phase-wise this straddles **C** (the data spine — the importer is an import
holiday like Sheets and the rescue dump) and **E** (publish — the feeds ride
the deploy holidays that are already built). It needs nothing from phase F or
G. Nothing here is an upstream ask: `calendar.md`'s "no ask needed for v1"
still holds, and the two ◇ upstream *candidates* it lists (a calendar widget in
the widget protocol; cross-window drag-and-drop) are untouched by the hub
reframe — a hub is transport and lens work, and both of those are ours.
