---
type: Concept
title: Antfarm v2 — types and connections
description: "The socket types of Antfarm v2 and the three socket shapes that say what kind of thing a wire carries: payloads that flow (Mantle, Rendition), fields that are evaluated where they land (Query), and references that name a thing (River, Key, Domain, Profile), plus small values for management nodes. One wire verb for all of them, placement as a separate relation, named ports, input cardinality, the rule that wires never write by themselves, and strands as an honest picture of how much flows."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Design, not built.** See the [v2 index](/concepts/platform/antfarm/v2/index.md).

# 1. The socket types

| type | shape | colour | what it is | typical source | typical consumer |
|---|---|---|---|---|---|
| **Mantle** | ● circle | blue | a set of runes, all of a chamber or a selection of it | a chamber node, a filter | a filter, a document, a tunnel, a store |
| **Query** | ◆ diamond | teal | a selection *rule* (a tag expression, a glyph, a date window), evaluated against whatever mantle it is applied to | a query node | a filter's `where` input |
| **Rendition** | ● circle | green | a document rendered for one output, after the privacy seam: a site manifest, an email, an `.ics` feed, a GeoJSON layer | a document's `preview` / `publish` | a domain |
| **River** | ■ square | purple | a reference to a place data rests | a river node | a store, a chamber's `rests in`, a gauge |
| **Key** | ■ square | yellow | a reference to a shared credential | a key node | anything that crosses the surface with an account: a domain, a cloud reservoir, a monitor |
| **Domain** | ■ square | orange | a reference to a place renditions are served or sent from | a domain node | a document's outputs plug *into* it; a DNS node configures it |
| **Profile** | ■ square | white | a reference to a profile in the Network chamber | a profile node | a peer reservoir, a usage monitor |
| **Value** | · small circle | grey | a number, text or yes/no | a gauge, a monitor | a threshold, a warning |

## Why three shapes

The workbook asked whether keys and devices belong on the same wires as data.
The first answer in the 2026-09-28 conversation was three wire grammars (data,
dependency, placement). **Geometry Nodes showed a simpler way**, and this page
takes it: Blender puts geometry, fields and data-blocks (an object, a material,
an image) on the same kind of wire, and lets the **socket shape** say which one
it is. A circle carries a value that flows. A diamond carries a field, which is
evaluated where it lands. Data-block sockets name a thing that exists
elsewhere.

That maps exactly:

- **● Payload.** Something flows and is evaluated: a mantle, a rendition.
- **◆ Field.** A rule that is not data yet. `tag:volunteer` means nothing until
  it is applied to a mantle, and then it means "these runes". This is the
  author's "index feature" ([chambers](/concepts/platform/antfarm/v2/mantles.md) §2).
- **■ Reference.** Something that *exists* rather than flows: a river, a key, a
  domain, a profile. Nothing streams down a key wire. The wire says "this node
  uses that key".

So the author's instinct (*"as long as ports are distinct, then anything can
travel across a wire"*) is kept, and the distinction the critique asked for is
made visible where the eye already looks: at the socket.

**Placement is the one thing that is not a wire.** "This node runs on the
office PC" is not data and not a dependency. It is where an effect fires. It is
drawn as a ring in the profile's colour around the node, and set with
`farm place` ([network](/concepts/platform/antfarm/v2/network.md) §3).

# 2. What may connect

**Same type to same type, and nothing else.** There are no implicit
conversions. Every conversion is a node, so it is visible and nameable:

| from | to | the node that converts |
|---|---|---|
| a file on disk | Mantle | `Import CSV`, `Import calendar`, `Import rescue` |
| Mantle | Rendition | a document node |
| Mantle | Value | `Count`, `Measure` (runes, bytes) |
| River | Value | `Gauge` (used, free, reach, cost) |
| Key | Value | `Usage`, `Expiry` |
| Query + Mantle | Mantle | `Filter` |

A wire from a Mantle socket to a Rendition socket is refused by `farm plug` with
the types named: *"refused: publish (rendition) cannot take contacts (mantle).
A mantle becomes a rendition through a document."* The GUI refuses the same
drop with the same sentence, because it calls the same check.

# 3. Wires never write by themselves

**This is the rule that makes a Geometry-Nodes-style graph safe for an
organization's data.** Geometry Nodes re-evaluates everything on every change,
and that is fine because nothing it computes leaves Blender. The Antfarm's
graph ends in a website, a bucket and an inbox.

So v2 separates **evaluation** from **writing**:

- **Evaluation is continuous and pure.** A filter's output, a count, a
  document's preview rendition: these are recomputed when their inputs change,
  shown on faces, and readable with `farm eval`. They write nothing.
- **Writing happens on command.** Importing into a chamber, storing a mantle in
  a river, publishing a rendition to a domain: each is a named action
  (`farm run import-csv`, `farm publish newsletter`), logged, and, if it leaves
  the device, gated with its consequence sentence.
- **Automatic writes are rules with a driver.** "Back up nightly" or "publish
  when the newsletter is marked ready" is a rule that one device drives (Void
  Maiz's `rules.hpp` pattern), never a property of a wire. Otherwise twenty
  members would each publish the site.

A wire into a writing input therefore means *"this is where it goes when it
is run"*, which is what v1's edges meant too. The difference is that v2's code
reads it.

# 4. Ports

**Ports are named.** A wire is stored as a Void Core relation labelled
`<out>:<in>` with port **names** (`publish:in`, `rows:where`), never indices.
v1 stored `3:1`, which meant reordering a glyph's ports rewired every database
silently (B8). A breaking version is the one moment the change is free.

**This needs Void Maiz.** Its projection reads `i:j` as numeric indices and
treats any other label as a loose semantic link. Named-port relations resolved
against `hints.ports` are a Void Maiz change (the author allows direct Void
Maiz edits since 2026-09-23), listed in [phases](/concepts/platform/antfarm/v2/phases.md)
V0.

**Each port declares:**

| hint | meaning | default |
|---|---|---|
| `name` | the handle in wires and printouts | required |
| `type` | one of §1's types | required |
| `dir` | `in` / `out` | required |
| `max` | how many wires an *input* accepts (`1`, or `many`) | `1` |
| `writes` | this input is where a run writes (§3) | `false` |
| `optional` | the node works without it | `false` |

**Outputs always fan out.** One key powers GitHub Pages and the host check; one
Data chamber feeds four documents. **The one-wire rule belongs on the input**:
a domain's `key` input takes exactly one key. That corrects the first
conversation's "an API key port should have only one wire coming from it": the
key can serve many nodes, and each node uses exactly one key. A `many` input is
drawn as Blender's stretched pill (the `Join Geometry` socket).

# 5. Strands

The author asked for several strands on a wire "to illustrate that the data
coming from that port has multiple things from it", with a setting to combine
them into one, and with the number carrying no functional meaning.

**Lean: the number carries no function, but it is never made up.** People
count what they see. A wire drawn with four strands that stands for three runes
teaches the wrong thing. So strands are a *projection of a real quantity*, on a
coarse scale:

| wire | strands |
|---|---|
| Mantle | `1 + floor(log10(runes))`, capped at 5: one strand under 10 runes, two under 100, five at 10,000 or more |
| River | one strand per reservoir (a river backed by this device and R2 has two) |
| Rendition | one |
| Query, Key, Domain, Profile, Value | one, always |

Void Maiz's own rule allows this: in `scene.hpp`, a *strength* may be drawn as
thickness and a *value* must be a label. "How much flows, compared to the other
wires of the same type" is a strength. The exact count is on the face and in
`farm eval`, never read off the strands.

- **Combine to one wire** is view state (`config` tier, undo-exempt, per
  device), like pane fractions: `farm view strands off`.
- **Strands move while something is being written.** A publish in flight
  animates its strands along the wire. That is the "working" readiness state
  (§6 of the [workbook](/concepts/platform/antfarm/redesign.md), A6), drawn.
- **This is a Void Maiz render style**, beside the existing
  `"render":"adjacency"`: a port hint `"render":"strands"` and a per-wire
  strand count the host sets after projection, the way it sets `active`.

# 6. Readiness, for every node

Every node kind answers one function with one of the workbook's states
(A6, answered): **planned**, **unconfigured**, **needs** (the one thing
missing), **ready**, **working**, **failing** (reason and when), and
**idle** (last success). Faces, `farm status`, the dashboard and the phone read
the same answer. Readiness is computed **per device**: a key present on this
device and absent on another is ready here and needs there.
