---
type: Concept
title: Antfarm v2 — the canvas
description: "How v2 looks and behaves for a person. The canvas is an ant farm's cross-section: chambers underground where everything is pure and live, the ground where this device reads and writes, and the surface where data leaves the device, with a gate on every wire that crosses it. Faces as live widgets, each with a verb twin; socket shapes and colours; strands; placement rings; the Connections dashboard in front of the graph; a palette by stratum; planned nodes; the phone; and the Void Maiz pieces this needs."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Design, not built.** See the [v2 index](/concepts/platform/antfarm/v2/index.md).

# 1. The strata: the effect boundary, drawn as ground

The first conversation asked whether the canvas should show where pure work
ends and effects begin. The author: *"be creative here, i'll trust your lean."*

**Lean: draw it the way an ant farm is built.** An ant farm is a thin slice of
earth between two panes of glass. The tunnels and chambers are underground,
the surface runs across the top, and the world is above it. That is exactly the
Antfarm's structure, and the name was chosen for structural reasons from the
first day ([the front door](/index.md)).

```
┌──────────────────────────────────────────────────────────────────────────┐
│  SURFACE · leaves this device               ☁ R2        ☁ GitHub Pages     │
│                                               ▲              ▲             │
│ ════════════════════════════ gate ═══════════╪══ gate ═══════╪════════════ │
│  GROUND · this device       home river ──────┘   local domain│             │
│                               ▲                     ▲        │             │
│ ─────────────────────────────┼─────────────────────┼────────┼──────────── │
│  CHAMBERS · pure, live        │                     │        │             │
│     Miga ─▶ Separate ─▶ Filter ─▶ Website ──preview─┘        │             │
│                  │  ◆ Tag                  └──publish────────┘             │
│                  └─▶ Data→Assets tunnel                                    │
└──────────────────────────────────────────────────────────────────────────┘
```

| stratum | what lives there | evaluation | crossing into it |
|---|---|---|---|
| **Chambers** | the Miga node, separate/filter/join, queries, tunnels, documents, counts | **live and pure**, like Geometry Nodes: recomputed when inputs change, shown on faces | — |
| **Ground** | the home river and other reservoirs on this device, local domains, local imports | on command; logged | a write to this device's disk: logged, no confirmation |
| **Surface** | cloud reservoirs, peer reservoirs, web and mail domains, vendor checks, gauges | on command or by a placed rule; logged | **a gate**: the effect is named, and the first time in a session it confirms with its consequence sentence |

**A node's stratum is not chosen by dragging it. It follows from what the node
touches.** A filter is in the Chambers because it cannot reach anything. A
river whose reservoirs are all on this device is on the Ground. A river with an
R2 reservoir straddles the surface line, and its strands show which reservoir
is which. Arrange lays nodes out by stratum top to bottom, and by flow left to
right as it does today. A person may still drag a node anywhere. The band
behind a node takes its stratum's tint, so a Surface node dragged underground
still looks like a Surface node.

## Gates

**Every wire that crosses the surface line passes through a gate**, drawn on
the line as a small opening. A gate shows:

- the effect it performs (`deploy-site`, `push-store`, `check-host`);
- whether this session has granted it (open) or will ask (closed);
- on hover, the consequence sentence from the one table (A12, answered): the
  Publish tab's confirmation and a face button now say the same words.

Clicking a gate offers **Rehearse**, which is `--dry-run-effects` for that one
effect. The effect gate has existed since July. The gates on the surface line
are where a person finally sees it.

# 2. Faces are live, and every widget has a verb

The author:

> i don't want these nodes to be very static. […] some visual widgets on the
> nodes themselves. like a simple "test api key" button, with an "online"
> symbol […] or maybe a mini preview graph of the mantle […] this data should be
> readable to the agent as well. Like the "test api key" button, should be a
> function an agent could execute from the cli.

**The rule: no widget without a verb.** Every indicator on a face is a
projection of state an agent can read, and every button compiles to a command
an agent can run. A widget with no line in [the CLI](/concepts/platform/antfarm/v2/cli.md)
cannot be replayed, so by ground rule 3 it is designed wrong.

| face | widget | the agent's twin |
|---|---|---|
| Miga | chamber table: runes, bytes, growth sparkline per chamber | `farm mantles` |
| Filter | "kept 312 / 1,904", the compiled query | `farm eval <filter>.kept` |
| Data→Assets tunnel | referenced · missing · unheld · orphans | `farm check <tunnel>` |
| River | one row per reservoir: reach, used, last checked; *Check* | `farm river <river>`, `farm check <river>` |
| Gauge | a bar of used / free; *Measure* | `farm check <gauge>` |
| Key | provider, last four characters, added by, expiry countdown, *Test* | `farm key <key>`, `farm check <key>` |
| Usage | calls this month by profile, as a small bar per profile colour | `farm eval <usage>.by-profile` |
| Document | thumbnail of the last rendition, runes seen / used, last published | `farm show <document>` |
| Domain | the address, whether the name resolves, last deployment; *Publish* | `farm show <domain>`, `farm publish <document>` |
| Profile | presence dot, device, what it holds, linked profiles | `farm profiles` |

**"Online" is never faked.** A status that needs the network says when it was
checked (*"ok · checked 3 min ago"*). A status that does not need the network
(presence, our own usage log, a count) is live. See
[rivers](/concepts/platform/antfarm/v2/rivers.md) §5.

**Faces cost what is on screen.** Sparklines and counts are cached projections
recomputed on dispatch, and only for faces that are visible. Previews of
renditions render when their face is visible and the document has changed,
debounced, never per frame.

# 3. The visual vocabulary

| element | how it looks | means |
|---|---|---|
| ● socket | circle, in the type's colour | a payload flows: Mantle (blue), Rendition (green) |
| ◆ socket | diamond, teal | a query: a rule applied where it lands |
| ■ socket | square | a reference: River (purple), Key (yellow), Domain (orange), Profile (white) |
| pill socket | a stretched socket | an input that accepts many wires |
| strands | 1 to 5 parallel lines | how much flows, on a log scale; animated while writing ([types](/concepts/platform/antfarm/v2/types.md) §5) |
| dashed wire | a query wire | lazy, like Geometry Nodes fields |
| placement ring | a ring in a profile's presence colour | this node acts on that device |
| greyed node with "planned" | the node kind exists, no code answers it | A7, answered |
| gate | an opening on the surface line | an effect; open when granted |

# 4. What a person sees first: Connections

The workbook's A3, answered **yes**: the tab opens on **Connections**, a list of
what the organization is connected to, grouped by the question each answers,
with readiness and one primary action per row. The graph is the **Wiring** view,
one click away. The author asked for this on 2026-07-16 (*"everyone else gets
the dashboard over it"*), and v2's types make the grouping fall out:

| group | built from |
|---|---|
| **Where the database rests** | the Miga node's rivers |
| **Where files are kept** | rivers and their reservoirs |
| **Putting images online** | rivers with a `public` reservoir |
| **Documents and where they go** | each document, its domains, last published |
| **Keys** | each key, what uses it, expiry, spend |
| **The network** | profiles here, what they hold |
| **Planned** | kinds with no code yet |

Selecting a row opens its node in the Inspector, the same panel the canvas
uses. **Add a connection** starts from a question ("I want to publish a
map"), lists the node kinds and keys that answer it, asks only for what they
need, runs Check, and compiles to `farm add` / `farm plug` / `farm key add`. That
wizard comes last in the [phases](/concepts/platform/antfarm/v2/phases.md),
because it is the sum of the others.

# 5. The palette

Grouped by stratum, then by shape, so the palette teaches the model:

- **Chambers**: Miga, Separate chambers, Filter, Join, Count, Measure; the
  query nodes; the tunnels; the four documents.
- **Ground**: River (local), Local domain, Import CSV, Import calendar.
- **Surface**: cloud and peer reservoirs, Web domain, Mail domain (planned),
  Key, Gauge, Vendor usage.
- **Network**: Profile.
- **Tidying**: Reroute (v1's polygon router, untyped, kept).

v1's test nodes (`math_*`, `str_*`) leave the palette. They were there to
stress sync and have done it.

# 6. On a phone

The workbook's A11, kept: **Connections, read-only, plus Check and the primary
action of a ready row.** No graph editing and no key values. A phone is a
member doing field work. It does see its own readiness, which is the useful
part: *"Publish the events calendar: nowhere to go from this device."*

# 7. What this needs from Void Maiz

The author allows direct Void Maiz edits since 2026-09-23, kept as separate
work in that repository:

| need | what exists | the change |
|---|---|---|
| named-port wires | `i:j` numeric; any other label is a loose link | resolve `out:in` names against `hints.ports` |
| socket shapes | round sockets | a `shape` hint per port: circle, diamond, square |
| input cardinality | fan-out is allowed everywhere | a `max` hint; refuse a second wire into a `max:1` input; draw `many` as a pill |
| strands | `render: adjacency` is the only port render style | `render: strands`, and a per-wire count set by the host after projection |
| strata | none | background bands and a horizontal "surface" line the host places |
| gates | `SceneWire::active` highlights a wire | a per-wire marker at a host-given point, with a hover text |
| placement rings | presence marks on nodes | a ring in a given colour |

Each is generic. None is Hormiga vocabulary, so each belongs upstream.
