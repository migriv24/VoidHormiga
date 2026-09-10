---
type: Concept
title: Data model
description: "The five kinds of things an outreach org runs on — people, events, assets, documents, the org itself — as glyphs; tag axes with temper hygiene; relations as edges."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

An outreach organization runs on five kinds of things, and all of them are
runes with glyphs, tagged on axes, living in mantles. The dispatcher is the
only way any of it changes.

# The five kinds

- **People** — `contact`, `organization` glyphs (and `job` for the
  jobs/opportunities board). A contact's **public bio and internal notes are
  born as separate fields** — the privacy line between them is enforced at the
  render seam, not by discipline (see [security](/concepts/platform/security.md) §3).
  Each carries an **`avatar`** field (UI/UX phase, 2026-08-03): a per-entity
  photo, editable via the image picker, with a **procedural placeholder** when
  unset — a disc colored by a hash of the name, with the initials — so the Data
  tab's **list and card views** always show a face. (The predecessor showed
  contacts this way; card view is the modern take, with a list ⇄ cards toggle.)
- **Events** — `event` glyph: meetings and presentations, each with 1–N
  presenters (edges to contacts, native — not a comma-separated field), with
  flyers and images attached in context (edges to assets).
- **Assets** — `image` and `resource` (PDF/document) glyphs: metadata runes in
  the images/resources mantles, tagged at ingestion; the bytes live on the
  Asset holiday (local FS, content-hashed filenames — see
  [Antfarm](/concepts/platform/antfarm.md)).
- **Documents** — the things composed *from* the above: newsletter issues and
  website pages, each a mantle of block runes. Their whole story is
  [blocks & domains](/concepts/sections/blocks-and-domains.md).
- **The organization itself** — its backends, credentials, collaborators, and
  trust topology: the [Antfarm](/concepts/platform/antfarm.md) mantle.

Facets carry the six-facet story where natural (who = the contact's name,
when = the event's date, …); a `note` glyph exists for the freeform rest.

# The rune KIND — entity, act, measure

**Since Void Core 0.2.14** ([Q64](/developer_questions.md), answered
2026-09-04) a glyph descriptor carries a `"kind"`: `entity`, `act` or `measure`,
defaulting to `entity`. It answers a question the five families above do not: an
**entity** is an explicit thing that has representations; an **act** is a
happening whose subject is some other rune; a **measure** is a dimension an
entity has an amount of.

The argument that carries it is **arity**. An edge label can express only a
*binary* relation, and a sentence like *"this council member said this, at this
point, in that meeting"* is not binary. The standard move is to reify the verb
as a node with typed ports for its roles — RDF reification, neo-Davidsonian
event semantics — which is also exactly an interaction net agent, the model Void
Maiz already implements. So it is not a new mechanism; it is the existing one
applied to verbs instead of only to nouns.

**Hormiga is almost entirely entities, and the default is left alone.** Five
glyphs are marked `act`, and each was already reified for that reason:

| glyph | the happening it records |
|---|---|
| `statement` | a person said this, at this offset, in that meeting |
| `revision` | a policy changed — the delta *is* the rune |
| `submission` | a stranger sent something in |
| `deployment` | a site was published, to this host, at this time |
| `incident` | something occurred, here, then |

**`event` is deliberately not one.** An event in Hormiga is the thing a person
*attends* — a venue, times, a flier, a page on the website — and its fields are
read by renderers, not filled as the roles of a verb. Marking it `act` would be
reading the English word rather than the model.

**`measure` is unused here, and that is [Q65](/developer_questions.md)'s answer
standing.** A weight is a *magnitude*, and Hormiga's numbers are mostly
**points**: a date has no magnitude, it has a position. Dates, coordinates and
grid columns are points in an affine space — subtract two for a duration or a
displacement, add one of those to a point, but never add or scale two points —
which is exactly why *"half of September 3rd"* is meaningless while *"a quarter
past twelve"* is fine. Points stay in fields.

The transferable rule, for anybody choosing: **if a number is read by rules that
produce new structure it belongs on an edge where the rules can see it; if it is
read only by renderers it belongs in a field.**

# An organization may declare its own type

Also since 0.2.14: glyph declarations live **in the state document**
(`state.glyphs`, written by `glyph declare`), so a type an organization declared
travels inside its `.miga` and a rune can never arrive somewhere without the
descriptor explaining it. Before that, descriptors lived on the manager and a
bundle carried runes without their meaning.

Two rules that matter to any caller:

- **A declaration shadows a registration of the same name**, because the
  declaration is the one that travelled with the data — and `glyphs` stamps
  every descriptor `"source": "document"` or `"host"`, so the shadowing is never
  silent.
- **The reverse must not happen.** A merely *registered* glyph does not export;
  host config that travelled would be one machine's registration becoming
  another machine's data. `tests/spine_smoke.cpp` pins both halves.

Read the schema from the core (`glyphs <name>`) and never keep a second copy —
`hormiga::glyph_fields()` used to be that second copy and was already wrong when
it was deleted. Declare through `hormiga::declare_glyph()`, which quotes the
descriptor as the one SPEC §6.1 argument it is. The remaining half of
[Q59](/developer_questions.md) — the generic renderer and the declaration UI —
is Hormiga's and is no longer blocked on anybody.

# Tags: axis-typed from day one

The namespace→axis map is declared at founding, not accreted:
`month:` → when, `type:` → what, `status:` → state, `lang:` → language, and so
on. Two consequences:

- **One grammar everywhere.** `@month:june AND type:event` is the same
  expression in the CLI, the table view's filter box, and a query-backed
  block's source field. There is no second query language.
- **Hygiene is a `temper` pass, not a management UI.** Drift
  (`may` / `May` / `may2026`) is fixed by a registered idempotent
  normalization rule that runs as a logged command. The "tag management
  panel" is the axis registry plus temper — a property of the model, not a
  screen.
- **Adding a tag searches the tags you already have.** Tag entry is a
  type-ahead over the existing vocabulary (`tag_picker`, 2026-07-22 — a
  sibling of the reusable `search_picker` the author flagged as crucial),
  with a "+ create" row for genuinely new tags. Prevention over cleanup:
  offering the existing tag first is how drift is avoided *before* temper has
  to fix it. Namespaced tags (`icon:`/`color:`/`month:`) are hidden from this
  picker — they are set by their own dedicated UI (marker menus, the date
  temper), never hand-typed. Built into the rule editor and the map's
  multi-select "tag all"; more surfaces to come.

# Relations are edges

Contact↔organization, image↔event, resource pairs, multi-presenter events:
all **link relations between runes**, first-class in the core. Void Maiz
renders them as wires when you want to see them — the connections view is an
actual canvas, inherited rather than built. Graph-shaped queries ("every
image attached to a June event") compile against the same join tables the
tags use.

# Scale, honestly

The working target is 10²–10³ runes per mantle (a real org's contacts and a
year of events), with the upstream calibration that full-rebuild projection
stays comfortable at 2×10³ small runes on desktop. **We measure and report**:
our numbers are the first real data-heavy datapoint for Void Maiz's
diff-seam question, and they size both the canvas and the coming table view,
which share the one-sync path.
