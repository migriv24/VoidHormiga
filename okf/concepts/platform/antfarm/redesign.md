---
type: Workbook
title: The Antfarm redesign workbook
description: "For the author's study before the redesign (2026-09-22). Everything the author has said about the Antfarm, in their words; what is broken, concretely and by cause; twelve design questions the redesign must answer, each with options and a lean (door-checked types, wiring authority, dashboard-first, credentials as their own objects, declared capabilities, a readiness contract, placeholders, sync, a separate application, wrappers, the phone, confirmations); an anatomy to react to; what Void Maiz now provides; and an order of work that starts at the command line."
tags: [status:current, audience:author, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

**Nothing on this page is decided.** It gathers what the redesign has to settle
so the author can settle it. Each question has options and a **lean**, a
default so that silence has a sensible answer, per ground rule 8. When the
author answers, the answer goes into the concept pages and the question clears
from here.

# 1. What the author has said

In order, because the order shows how the brief grew:

- **2026-07-16**, the founding: the Antfarm is where the org's behaviour is
  *"seen and reconfigured, not where it must be built"*. It was also named as
  the section missing a layer: **user-friendliness**. *"The graph alone is a
  diagram, not a tool."* It asked for status at a glance, guided add-a-holiday
  flows, and plain-language explanations on every node: *"Power users get the
  graph; everyone else gets the dashboard over it."*
- **2026-07-22**: *"create new database" creates a `.miga`, and what it saves
  is the Antfarm.* Topology is personal, and shared nodes are the
  collaboration.
- **2026-08-05**, Q25: ports typed by what flows, and publishing as a pipeline.
- **2026-09-15**, running 0.1.2: *"antfarm in general should have like a little
  warning in the GUI that it's not really ready for human users yet. Sure we
  have the node graph, but i'll be honest, it does NOT work ... the bones of the
  antfarm works, and it can currently be driven by agents in a fine enough way.
  it's mostly the UI/UX of the antfarm is horrible to a point where it might be
  unusable."*
- **2026-09-15**: *"a button to automatically do something like 'host it
  online' should have its protocols be called upon via the antfarm ... its kinda
  like a function call, where we expect a link to be given in return."*
- **2026-09-16**: *"we should also be considering how the antfarm determines how
  a database is collaborated with ... what is local private data? ... admins
  and permissions."* And: *"the host's antfarm should be prioritized."*
- **2026-09-19**: *"if i add a node on one antfarm, it should do stuff on the
  other. maybe private nodes can exist?"* And, larger: *"potentially isolate it
  in its own void based thing."*
- **2026-09-20**, on the database profile (Q79): *"it includes the antfarms and
  everything!"*
- **2026-09-22**: *"quite possibly one of the most important structures for
  hormiga, and i'm still leaving it without a proper design. This is actually
  because of the very fact of how important the antfarm is."*

# 2. What is broken, by cause

"The UI is horrible" is the symptom. These are the causes found while writing
this folder. Most of them are not drawing problems:

| # | what a person meets | the cause | page |
|---|---|---|---|
| B1 | wires that can be drawn and mean nothing | **no code reads wiring**; every consumer finds its node by glyph | [model](/concepts/platform/antfarm/model.md) |
| B2 | a graph an agent can make that the GUI refuses | **types are checked by the canvas, not the dispatcher** | [CLI §8](/concepts/platform/antfarm/cli-examples.md) |
| B3 | a palette where 5 of 18 kinds do nothing, and 2 more are decorative | placeholders presented like working nodes | [model](/concepts/platform/antfarm/model.md) |
| B4 | nodes that cannot say whether they work | readiness is face text computed per glyph in `app.cpp`, not a contract | [holidays](/concepts/platform/antfarm/holidays.md) §describe |
| B5 | a graph organized by payload when a person thinks in tasks | there is no view by *question* ("putting images online") | [capabilities](/concepts/platform/antfarm/capabilities.md) |
| B6 | credentials mixed into node fields beside targets and options | no separation of target / credential / behaviour | [mappings](/concepts/platform/antfarm/mappings.md) §5 |
| B7 | buttons on faces that act without saying what they will do | consequence sentences are shown by the Publish tab only | [holidays](/concepts/platform/antfarm/holidays.md) |
| B8 | port numbers, not names, everywhere text is shown | relation labels are indices | [CLI §3](/concepts/platform/antfarm/cli-examples.md) |
| B9 | two devices, two Antfarms | the mantle is excluded from sync, deliberately (§3a) | [across devices](/concepts/platform/antfarm/collaboration.md) |
| B10 | the canvas itself "does NOT work" | **not diagnosed on this page.** The author's report names the graph's editing surface. A two-minute screen recording of the specific failures would turn it into a list | — |

**B10 is the one this session could not investigate.** It needs the author at
the canvas. Everything else above has a cause in the code.

# 3. The design questions

## A1. Where are payload types enforced?

- (a) **Void Core** validates `link --relation i:j` against the glyph's
  declared ports. That fixes it for every host. But `hints.ports` is Void Maiz
  vocabulary that Void Core carries without reading, so this is an upstream
  conversation about who owns port semantics.
- (b) **A Hormiga host guard** refuses an ill-typed `link` before dispatch, in
  the GUI and the CLI alike.
- (c) **A named-port verb** (`plug a.site b.site`, [CLI §9](/concepts/platform/antfarm/cli-examples.md))
  that checks, with raw `link` left as it is.

**Lean: (c) and (b) now, (a) asked upstream.** Raw `link` stays the escape
hatch. It is a Core verb, and Hormiga does not patch Core. Every Hormiga
surface uses the checked path. A message to Void Core and Void Maiz asks
whether ports, and port *names* in relation labels, should become part of the
glyph contract (B8 has the same answer).

## A2. Is wiring authoritative?

- (a) **Yes.** Consumers follow edges. The publisher's `site` goes where it is
  wired, an image host is whichever host `core.assets` reaches, and two
  websites can publish to two hosts because the graph says which is which.
- (b) **No, and say so.** Nodes configure by existing. Edges are
  documentation, and the rule "wiring is configuration" is retired.
- (c) **Only where there is a choice.** With one candidate, wiring is
  optional. With two, the edge decides.

**Lean: (a), with the seed doing the wiring.** "One database, many artifacts"
(2026-07-22) cannot work by existence: two websites and two hosts need the
graph to say which goes where. "Defaults, not assembly" is kept by the seed,
which already wires everything, not by ignoring wires. The cost is real: an
unwired node stops working, so the node must *say* it is unwired ("not
connected to anything, so nothing uses it"). This must be settled before
[sync](/concepts/platform/antfarm/collaboration.md), because it decides
whether a lost edge is a lost drawing or a lost configuration.

## A3. What does a person see first?

- (a) The graph, as today.
- (b) **A connections dashboard**: what the organization is connected to,
  grouped by the question each connection answers, with readiness and one
  primary action per row. The graph is one click away.
- (c) Only a dashboard. The graph is for developers.

**Lean: (b).** It is what the author asked for on 2026-07-16 (*"everyone else
gets the dashboard over it"*). It is how every tool in the
[prior-art survey](/concepts/platform/antfarm/mappings.md) §6 that people trust
is arranged (a connections list separate from the flow). It is also the
phone's entire Antfarm, so it has to exist anyway.

## A4. Are credentials their own objects?

- (a) Fields on each node, as now (`token_key`, `key_file`).
- (b) **A credential is its own visible thing**: a `key` rune naming a vault
  entry or file, showing presence, when it was set, when it last worked, and
  which nodes use it. Nodes reference it.

**Lean: (b).** It is n8n's and Zapier's shape. It makes §3a's "keys go stale,
and whoever keeps showing up gets new ones" something a person can *see*: this
key was handed out 40 days ago. It lets one token serve both `hol_github` and
`check-host`. And it is what lets the Antfarm sync without secrets, because
the `key` rune syncs as a *reference* and its value never does. It is also
the natural home for the room-key rotation [Q80](/developer_questions.md)
asks for.

## A5. Are capabilities declared on node kinds?

**Lean: yes.** See [capabilities](/concepts/platform/antfarm/capabilities.md)
§"What capabilities should become": an `answers` hint per glyph. The header
tables become data read from the declarations. It is the Allomone seam, and
it fills Q79's summary for free.

## A6. What is the readiness contract?

**Lean: one function per holiday, returning one of a small set of states**,
read by the face, `farm check` and the phone alike:

| state | means | example |
|---|---|---|
| **planned** | the kind exists and no code answers it yet | `hol_sheets` today |
| **unconfigured** | a target is missing | no `repo` |
| **needs** | a credential or prerequisite is missing, with the one thing named | "no token on this device" |
| **ready** | could act now; nothing is known to be wrong | |
| **working** | an effect is in flight | "uploading 12 of 40" |
| **failing** | the last attempt failed, with the reason and when | "rate limited, 3 min ago" |
| **idle / last success** | when it last did its job | "published 2 days ago, 14 files" |

`host_problem()` is the first instance of "needs". The rest is the same idea,
completed.

## A7. How do placeholders present?

- (a) Hide kinds nothing implements.
- (b) **Show them as planned**: greyed, labelled "planned", placeable only to
  record intent, and saying so on their face.
- (c) Remove them from the code.

**Lean: (b).** It is the same honesty as the download page's *"no build
yet"* cards. It also keeps what the placeholders are for: showing an
organization where the application is going.

## A8. Does the Antfarm sync? (Q78)

**Lean: yes, in the shape [across devices](/concepts/platform/antfarm/collaboration.md)
proposes**: description syncs, credentials never do, readiness is per
device, one device drives automatic effects, and a `private` tag keeps a node
home. After A2, and with wire runes if A2 is (a).

## A9. Should the Antfarm be its own application?

The author's *"potentially isolate it in its own void based thing"*.

- (a) A separate application.
- (b) **A separable layer inside Hormiga**: the holiday implementations, the
  capability tables and the readiness contract, kept free of Hormiga's UI and
  domain, so another Void application could take it the way Void Maiz took
  networking and updates.
- (c) Offered upstream as Void Core's *planned* holiday registry
  ([holidays](/concepts/platform/antfarm/holidays.md) §1).

**Lean: (b) now, aiming at (c).** [Application boundaries](/concepts/foundation/application-boundaries.md)
asks what a separation costs a *user*. A separate application would mean an
organization installs two programs to publish its newsletter. A separable
*layer* costs the user nothing and keeps (c) open. The layering checker
(`tools/check_layering.py`) is how "separable" stays true rather than hoped.

## A10. Are wrappers (fallback, cache, logger) visible nodes?

**Lean: no, they are properties on a face.** A wrapper that is always on
should not cost a node. The snapshot fallback shows as a line on the SQLite
node ("falls back to snapshot: yes, last snapshot 2 min ago"). Revisit if an
organization ever needs to *choose* a wrapper, for example an encrypting
wrapper around a bucket (rclone's `crypt`).

## A11. What is the Antfarm on a phone?

**Lean: the dashboard (A3), read-only, plus two actions**: *Check* and the
primary action of a ready connection ("Publish now"). No graph, no node
editing, no credentials. A phone is a member, not the host. See
[mobile](/concepts/sections/mobile.md).

## A12. Do all effects confirm with their consequence?

**Lean: yes, from one table.** The consequence sentences already exist for
every CLI effect. The Publish tab already quotes one. Every face button that
fires an effect shows its sentence first, *once per session per effect*, so a
person is told without being nagged.

# 4. An anatomy to react to

A sketch for the desktop tab, not a mock-up. Its only purpose is to give the
author something concrete to disagree with.

```
┌ Antfarm ───────────────────────────────────────────────────────────────────────┐
│ [Connections] [Wiring]                              + Add a connection          │
├──────────────────────────────┬─────────────────────────────────────────────────┤
│ KEEPING THE DATA             │  site-pages · GitHub Pages            cloud     │
│  ● data-sqlite  ready        │  ─────────────────────────────────────────────  │
│ KEEPING FILES                │  NEEDS  a token on this device                  │
│  ● assets-fs    ready        │         [Choose a key…]                         │
│ PUTTING IMAGES ONLINE        │                                                 │
│  ◐ site-pages   needs token  │  Where     repo   example-org/example-org.gi…   │
│ PUBLISHING THE SITE          │            branch gh-pages                      │
│  ● out-localhost ready       │  Key       — none —   (keys are shared, never   │
│  ◐ site-pages   needs token  │             copied into this node)              │
│ SHARING                      │  Options   message (blank = generated)          │
│  ● lan-share  2 here         │                                                 │
│ PLANNED                      │  Connected  out-html.site → site               │
│  ○ Google Sheets             │             org-domain.domain → domain         │
│  ○ Sign-in                   │                                                 │
│                              │  [Check]  [Publish now]    last: never          │
└──────────────────────────────┴─────────────────────────────────────────────────┘
```

- **Left:** connections by the question they answer (A3, A5), with readiness
  (A6), and placeholders at the bottom as planned (A7). One node can appear
  under two questions (`site-pages` both hosts images and publishes).
- **Right:** the selected connection in the three groups from the
  [mappings](/concepts/platform/antfarm/mappings.md) survey (where / key /
  options), what it is wired to by port name (A2, B8), and its actions,
  each confirming with its consequence (A12).
- **Wiring** is today's canvas, kept for the people who want it. The
  canvas's Inspector window and presence marks (`canvas:antfarm`) carry over.
- **+ Add a connection** starts from a question ("I want to put images
  online"), lists the kinds that answer it with what each needs, asks only
  for those fields, runs *Check*, and compiles to `rune new` / `set` / `plug`.
  That is Home Assistant's config-flow shape.

# 5. What Void Maiz now provides for this

| need | Void Maiz piece | since |
|---|---|---|
| the graph with presence marks | `edit_canvas` + `CanvasNet` (Hormiga uses it: `canvas:antfarm`) | 09-18 |
| a toolbar that never clips | `action_bar` | 09-21 |
| the connection detail on a phone | `begin_bottom_sheet` | 09-19 |
| "Deleted. UNDO" after removing a node | `show_snackbar` | 09-19 |
| rows with swipe actions (phone dashboard) | `begin_swipe_row` | 09-19 |
| two admins editing one node | `claims.hpp` | 09-20 |
| wiring that survives concurrent edits | `wires.hpp` (wire runes) | 09-21 |
| "one device performs this" | `rules.hpp` (the physics-driver pattern) | 09-22 |
| node names that cannot collide | `CanvasStyle::device_tag` (set by Hormiga 09-22) | 09-22 |
| per-field editors (`combo:`, `path`, `date`) | the widget registry, `hints.editors` | earlier |

What it does not provide, and should not: capabilities, readiness, credentials.
Those are the Antfarm's own vocabulary. They become upstream candidates only
if A9 goes to (c).

# 6. An order of work

Starting at the command line, because every GUI piece needs its CLI twin first
(rule 8), and the CLI pieces are small:

1. **`plug` and `farm ports`** (A1, B2, B8). A host-side type check, and port
   names in every printout. Send the upstream ask.
2. **The readiness contract and `farm check`** (A6, B4). One function per
   holiday. The faces switch to reading it.
3. **Capability declarations** (A5, B5). The header tables become data from
   glyph hints. `ask` on the CLI.
4. **`farm`, the dashboard** (A3). CLI first, then the Connections view,
   which is the phone's view too.
5. **Credentials as objects** (A4, B6). The `key` rune, and the vault showing
   what it holds and where it is used.
6. **Wiring authority** (A2, B1). Consumers follow edges. Unwired nodes say so.
7. **Sync** (A8, B9). Wire runes, description-syncs, the driver.
8. **The wizard.** Last, because it is the sum of 3 to 6.

B10 (the canvas's own misbehaviour) runs in parallel whenever the author can
show it.
