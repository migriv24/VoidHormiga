---
type: Concept
title: Void Maiz, September 2026 — what we take up
description: "Void Maiz grew a great deal between 2026-09-19 and 09-22, much of it carried over from Hormiga (networking, updates, tag suggestions) and demonstrated on Interaction Combinators (a collaborative canvas across PC and phone, a mobile kit, a LAN transport). Piece by piece: what it is, what Hormiga has today, what we do about it, and the state on 2026-09-22. Measured: Hormiga builds against it unchanged (48/49, the one failure upstream's known reducer gap), and Void Maiz's update client already reads Hormiga's live feed correctly."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-09-22T00:00:00Z
---

# The shape of what changed

The author, 2026-09-22: *"When in development, void maiz took a lot of
inspiration from hormiga. A lot of the features have been carried over into
maiz, and I would like for hormiga to update itself accordingly."*

Three movements, in Void Maiz's own words:

- **Networking** (2026-09-18/19, their Q30): *Void Maiz owns what networking
  looks like, Void Palabra owns what networking is, the application answers
  what may be shared.* Hormiga asked for this and adopted stages A to C on
  2026-09-19/20 ([log](/log.md)).
- **Updates** (2026-09-21): *"all applications should be able to update
  themselves."* Void Mago owns what a release is, Void Maiz owns the in-app
  client, the application owns who it is. **Adapted from Hormiga's client**
  (`src/update/`).
- **The collaborative canvas and the phone** (2026-09-20 to 22), proven on
  Interaction Combinators 0.2 to 0.5: in-flight gestures in presence, claims,
  wires as runes, device-scoped names, mantle rules, a LAN transport with a
  person's Allow, a mobile kit, a drawn keyboard, layout classes, and
  **tag suggestions ported from Hormiga**.

**The first measurement is the reassuring one:** Hormiga builds against
today's Void Maiz with no changes, and the suite is 48/49. The one failure is
`reduce_conformance` at 17/25, Void Maiz's own known gap (Void Core's corpus
grew boxes and `patch` on 2026-09-01), not ours. Nothing here is forced. Every
row below is a choice.

# Piece by piece

| piece | Void Maiz | Hormiga before today | what we do | state (2026-09-22) |
|---|---|---|---|---|
| presence, marks, roster, member list, networking settings | `presence.hpp`, `netview.hpp` | adopted 09-19 | nothing | **adopted** |
| the sync seam | `net.hpp` (`voidmaiz_net`) | adopted 09-19/20 | nothing | **adopted** |
| tag suggestions | `tags.hpp` (`suggest_tags`) | our own `compute_tag_suggestions` | delete ours, call theirs, keep our cache | **adopted today**. Connective mode now takes the best link, not the sum (their fix to our bug) |
| device-scoped names on the canvas | `CanvasStyle::device_tag` | our `mint_name` scoped our palettes; **the canvas's own palette minted untagged names on the Builder, which syncs** | set it every frame from `device_tag()` | **adopted today**. See [Antfarm across devices](/concepts/platform/antfarm/collaboration.md) §1 |
| the update client and its views | `update.hpp`, `updateview.hpp` | our `src/update/` (≈1,080 lines), the original | migrate: see §"Updates" | **proven compatible today, not migrated** |
| the LAN transport | `lan.hpp` (sockets, interfaces, join code, Android multicast lock), `lanlink.hpp` (`LanSession`: beacon, Allow, **unencrypted**) | our own **sealed** transport (room key, SAS, libsodium) | keep ours; take `lan.hpp`'s platform facts for the phone; compare three ideas | see §"The LAN" |
| profiles | `load_profile`, `save_profile`, `default_device_name`, `suggested_colour` | our richer profile (key, avatar, the 12-colour registry) | keep ours | **declined, reported 09-19** |
| wires as runes | `wires.hpp`, `CanvasStyle::wires` | Antfarm edges are plain relations | adopt for the Antfarm **if** wiring becomes authoritative | **waiting on** [redesign A2](/concepts/platform/antfarm/redesign.md) |
| claims | `claims.hpp` | the human-floor claim via `voidmaiz_headless` | canvas claims arrive with upstream's N1 drawing, since we already pass `CanvasNet` | **waiting on upstream N1** |
| gestures in flight (cursors, ghosts, pending wires) | `CanvasPresence` | none | same as claims: likely free through `CanvasNet` | **waiting on upstream N1** |
| view state converges silently | `presentational_joins()` (the `NetOptions` default) | already the default, so Antfarm and canvas `content.pos` converge | **decide the Builder's grid fields**: see §"Builder layout under sync" | open question |
| rules of a mantle | `rules.hpp` (`{"rule":"physics","driver":…}`) | none | the pattern for "which device performs a synced node's effects" | proposed in [across devices](/concepts/platform/antfarm/collaboration.md) §5 |
| the mobile kit, touch, layout classes, keyboard | `mobile.hpp`, `touch.hpp` | none | the phone front-end | see [mobile](/concepts/sections/mobile.md) |
| `Network::resync` | `net.hpp` | none | a **Sync again** button in the Share window: every link restarts and the whole document is exchanged | small, not done |
| the console | (not taken) | our `ui/console.cpp`, offered upstream 09-20 | keep ours ([Q81](/developer_questions.md)) | no upstream answer yet |

# Updates: proven compatible, migration staged

Void Maiz's client is ours, generalized. What changed in the move:
`AppIdentity` instead of our hard-coded name, the network behind a seam (a
phone has no `curl`), `.zip` as well as `.tar.gz`, and an Android apply path.

**Measured today.** Their probe (`maiz_update_smoke --probe`, built as part of
our tree) read **Hormiga's live feed**:

```
probe: latest 0.1.6, offer for 0.1.5 on windows-x64: YES, VoidHormiga-0.1.6-windows-x64-setup.exe
probe: latest 0.1.6, offer for 0.1.5 on linux-x64: YES, VoidHormiga-0.1.6-linux-x64.tar.gz
probe: latest 0.1.6, offer for 0.1.6 on windows-x64: no
probe: latest 0.1.6, offer for 0.1.6 on linux-x64: no
```

and printed our release's own summary and behaviour changes. So the feed
format, the version comparison and the artifact choice already agree.

**What must be checked before migrating**, because both involve a person's
earlier answer:

1. **The consent answer must survive.** Ours lives in
   `<config>/voidhormiga/updates.json` (XDG on Linux). Theirs is per machine
   too, but in its own place and shape. A person who said *Never* to 0.1.x must
   not be asked again by 0.2.0. The migration reads our file once, writes
   theirs, and says so in the log. **The rule is "never check unasked", and a
   lost preference is exactly an unasked check.**
2. **`voidhormiga-cli update`** keeps its verbs and words, now over their
   `describe()`. The GUI and CLI already say the same sentences, and they
   should keep doing so.

**Lean:** migrate in the release after next, not the next one. The feed
proves compatibility, and nothing is broken today. The value is that
Hormiga's phone build gets the Android apply path for free, so the migration
belongs with mobile stage M0 ([mobile](/concepts/sections/mobile.md)), not
before it.

# The LAN: keep the seal, take the platform facts

Void Maiz's `LanSession` is **unencrypted**, by that author's explicit
LAN-only ask for Interaction Combinators, and says so on screen. Hormiga's
transport is sealed to a room key with an out-of-band short code, because an
organization's member list travels on it. **We keep ours.** Void Maiz's own
[LAN transport page](../../../../VoidMaiz/okf/concepts/lan-transport.md)
agrees on ownership: *"an application with different rules replaces that
layer, not the two below it."*

What we take, and when:

- **`lan.hpp`'s platform facts: interface ranking, subnet maths, the join
  code, Android's multicast lock through JNI.** A phone needs the lock or it
  never hears a beacon. Take them at mobile M0, under our sealed session.
- **Three ideas to compare against our own:**
  - **silence as death** (`idle_ms`: a link quiet for 12 s is dead). Our
    0.1.6 "zombie link" fix solved the same problem from the other side. Check
    that a sleeping phone's socket, which neither delivers nor fails, is caught
    by ours.
  - **a known member comes back without a second Allow**
    (`already_allowed`). Ours decided the same for credential refresh
    (lan-sharing §3a), and it is not built.
  - **resync**, as above.

# Builder layout under sync

`presentational_joins()` makes `content.pos`, `placement`, `content.size`,
`content.collapsed` and `content.route.*` resolve by Lamport-latest, which is
the author's *"last one wins"* for view state. Antfarm and canvas positions are
`content.pos`, so they are covered.

**Builder blocks are placed by `row`, `col`, `span` and `page`**, which are
content fields. So today two members moving the same block at once get a
*conflict*, not a silent winner. Which is right is a real question, because a
block's position is part of the *published* document, not only the view:

- **Converge (Latest)** on `row`, `col` and `span`: consistent with the
  author's ruling, and never interrupts. The cost is that one person's layout
  change can vanish under another's with no sign.
- **Conflict** (today): shown and settled with one click. The cost is
  friction on a thing people do often.

**Lean: converge `row`, `col` and `span`, keep `page` as content** (moving a
block to another page is a structural decision). Show the author's "who changed
it" glow when upstream draws it. `NetOptions::joins` is the host's to extend,
so this is one line when decided. Recorded as a question, not done.

# What this page does not cover

The phone as a product is [mobile](/concepts/sections/mobile.md). The
Antfarm's use of wires, claims and rules is
[Antfarm across devices](/concepts/platform/antfarm/collaboration.md). The
networking adoption history is in the [log](/log.md), 2026-09-19 onward.
