---
type: Concept
title: The Civic Record — monitoring a city government
description: "The Springfield, Oregon project: council members as contacts, meetings as mantles of attributed statements, and `policy` / `revision` / `provision` — a versioned DAG over a tree, because a policy is defined by its delta. Votes as edges onto a revision. Why Void Reyna is a separate application and the four view sections are not. Attribution as a first-class uncertainty, never a guess. Bitemporality: the log says when we learned it, not when it was true. What Springfield actually publishes, checked."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-12T00:00:00Z
---

**Status: the data model is BUILT and tested** (`src/domain/civic.hpp`,
`tests/civic_smoke.cpp`, 2026-08-16); the website and the harvester are not.
Proposed by the author 2026-08-12. Questions [Q29–Q38](/developer_questions.md);
Q29 answered, step 1 (go look at the real record) and step 2 (hand-build one
meeting) both done.

# What it is

A public website that makes one city's government legible: who holds each seat,
what they said in council meetings, what they voted on, and where those
decisions landed. Starting with **Springfield, Oregon** — one city, so the shape
is forced by a real record rather than by a guess about all cities.

A citizen arrives and can ask three questions:

1. **Who represents me?** Seats, faces, wards, a short description.
2. **What has been said about X?** Search a topic, get the statements, attributed
   to people, dated, linked back to the meeting they came from.
3. **What was decided, and where does it land?** Policies and their revisions,
   votes per person, and the geography they affect.

And a fourth that only falls out of doing the first three properly: **what did
this look like in 2023?** The record is also an archive.

# The load-bearing question: is this even Hormiga?

Yes, and the fit is closer than it looks. Hormiga's founding sentence is *"keeps
a community-outreach organization's people, events, images, and resources as
tagged runes — and composes newsletters and static websites out of that data."*
Strike the word *outreach* and the civic record is the same sentence: **people**
(council members), **events** (meetings), **resources** (transcripts, videos,
agendas), and **a website composed from them**.

What is genuinely new is two things — a **normative text that versions**, and a
**record of who said what**. Everything else is vocabulary.

That makes this a good forcing case rather than a detour. If the data model
generalizes from an outreach org to a city council with only two additions, that
is evidence the model is right. If it needs ten, the model was overfit to the
predecessor and we want to know.

# The split: four views, one model, one external producer

The author's question — *should Hormiga become five applications?* — has a
principled answer, and it is **no for four of them and yes for the fifth.** The
line is not size or ambition. It is:

> **A view over the model stays in. A producer of the model from outside stays
> out.**

## Data Manager, Data Maps, Time Organizer, Document Builder: keep them together

These four are the same data seen four ways. Their entire value is that they
share one model: a contact edited in the Data section is *immediately* the
marker on the map, the presenter on the calendar entry, and the face in the
newsletter — with no sync, no export, no staleness, because there is one `Core`
and one dispatcher and they are all reading the same projection.

Splitting them into processes would mean **inventing a protocol to recreate what
a shared pointer already gives for free**, and it buys the two hardest problems
in the system at once:

- **Concurrent writes to one command log.** Today "the transcript is the
  session" is true because one process owns the log. Four processes writing it
  is a distributed-consensus problem, and it is the *same* problem the
  collaboration phase (F) already has to solve — solving it twice, once
  accidentally, is how a project acquires a protocol nobody designed.
- **Cross-process cache invalidation.** Every projection, every derivation,
  every render seam is a cache rebuilt from the model. Four processes means four
  caches and a story about how they hear that the model changed. That story is a
  bug generator.

In exchange for that, we would get… separation of *concern*, which the tabs
already provide. Tabs give separate concerns over shared state. Separate
applications give separate state, and shared state is the thing we want.

**The real answer to "these tabs are getting big" is not process boundaries — it
is that `app.cpp` is one file of 12,000 lines.** That is a genuine problem and it
has a genuine fix: split it into `data_section.cpp`, `map_section.cpp`,
`calendar_section.cpp`, `builder_section.cpp` against the existing `HormigaApp`.
Same binary, same model, four files a person can hold in their head. **If the
motivation for splitting is "each section deserves focus," file boundaries buy
that at none of the cost.**

*(Done 2026-08-17 as [Q30a](/developer_questions.md); the section units now live in `src/ui/` - see [workspace & sections](/concepts/sections/workspace-and-sections.md).)*

## Dataset generation: this one is genuinely a separate application

The author's tenth point is the sharpest thing in the brief, and it is right for
a reason worth stating precisely. The dataset generator is **not a view over the
model. It manufactures the model out of the unstructured world.** Concretely it
is different in every dimension that matters:

| | the four sections | the dataset generator |
|---|---|---|
| input | the model | video, audio, PDFs, scraped HTML |
| runtime | interactive, 60fps | batch, minutes to hours |
| failure | a toast | a partial result you re-run |
| language | C++20, vendored | Python, and it has to be |
| dependencies | vendored, no package manager (ground rule 5) | ML models, ASR, an NLP stack, possibly an LLM |

That last row is decisive on its own. **Ground rule 5 says vendor, don't depend —
no package managers, no CDNs.** You cannot vendor a speech-recognition stack.
Trying to would either break the rule or cripple the tool. Putting it in a
separate process with a separate dependency universe is not a compromise, it is
the rule working as intended: the thing that needs pip gets its own home, and the
thing that must stay auditable and offline stays that way.

## And the seam already exists — it is a holiday

The author asked, with a question mark: *"this new application would need to
share the data structures and schema of hormiga… it would also need to kinda
export a node for the antfarm?"*

**Yes — exactly that, and the schema it shares is the verb vocabulary, not a
library.** The [Antfarm](/concepts/platform/antfarm/index.md) already types its ports by payload,
and `records` is the payload of "the org's structured data." A dataset generator
is a **`records` source**, precisely like the CSV importer and the Sheets
importer already are. It plugs into `core.records` and the canvas enforces the
type for free.

The important consequence: **the generator does not need to link anything, know
C++, or share a header.** It emits Void Core commands.

    rune new contact "Sean VanGordon"
    tag "Sean VanGordon" +role:mayor +ward:at-large
    rune new policy ord-6472
    rune new revision ord-6472-r3
    link ord-6472-r3 ord-6472-r2 --relation supersedes
    tag ord-6472-r3 +topic:zoning +status:adopted
    link "Sean VanGordon" ord-6472-r3 --relation voted-yes

A tool that emits that transcript is **automatically conformant**: every import
lands as one replayable batch, logged and undoable, exactly like the CSV importer
does today (ground rule 3). The schema is `okf/verbs.md`, and it is already
written down. That is a much smaller and more durable contract than a shared
data structure, and it means the generator could be rewritten in any language
later without Hormiga noticing.

**Named by the author: Void Reyna** — the queen, who does not forage or build but
*founds the colony*. It is a Void-family sibling, not a Hormiga subdirectory; its
own concept page is [Void Reyna](/concepts/projects/void-reyna.md).

# What the real document actually looks like

Before designing anything, the author supplied one: **Springfield City Council
Operating Policies and Procedures, updated October 2025.** Reading it changed the
model, which is the whole reason to read one.

- **It is a TREE, not a text blob.** Ten sections, roughly a hundred numbered
  provisions, nested three deep — `SECTION 3` → `3.3 Mayor and Councilor
  Attendance` → `3.3.1 Notification`. Anything that stores a policy as a document
  with a body field throws away the structure that every citation, amendment and
  violation actually points at.
- **Provisions cite other documents.** §1: *"established and adopted under the
  authority granted in the Springfield Charter, Chapter IV, Section 12."* A
  policy references a *provision of another policy*. That is an edge.
- **Supersession is a named concept inside the document itself** — §9.5.1 is
  literally titled *"Supersede Previous Policies."* We are not inventing a
  `supersedes` relation; the domain already has one.
- **There are TWO kinds of change, and they are not the same shape.** §10 gives
  the amendment procedure: a provision may be **permanently amended** (two-thirds
  vote, prior notice required) *or* **temporarily suspended** (two-thirds vote of
  those present). **A suspension changes nothing about the text.** A version
  model that only knows "revision N → revision N+1" cannot say "3.3.1 was
  suspended for the September 14th meeting," and that is a real event with real
  consequences that someone will want to search for.
- **It carries two dates**: "UPDATED: October 2025" and "Adopted by the Common
  Council on ______". Valid time and adoption time, in the document, on the
  signature page.

# The new data types

The author's naming instinct was right and my earlier `measure` recommendation
was answering a narrower question. **Take `policy`** — and the reason is the
author's own: *a policy is defined by its delta.* The thing that persists across
proposal, amendment, deferral, split and merge is the **identity**; what carries
the meaning is the **set of dated assertions** about it and what they compose to.

Once versions are first-class, a separate `measure` glyph stops being needed:
**the thing voted on is a revision.** "Adopt the October 2025 revision" is a vote
edge onto a revision, and a policy that was never voted on (a platform's terms of
service) simply has no vote edges. One fewer glyph, and the generalization gets
better rather than worse.

Three glyphs, and no more:

## 1. `policy` — the identity that persists

One rune in the data mantle. Title, citation, adopting body. It is deliberately
thin: **almost nothing true about a policy is true of the policy rather than of
one of its versions**, including, as the author noted, its own name.

## 2. `revision` — a dated assertion about a policy

A rune per adopted change: what it asserts, the provisions it touches, its
adoption date, and the interval it is valid for. **Immutable once recorded** —
correcting a mis-entry means asserting a correction, not editing history.

The word "revision" survives from the earlier draft but the meaning narrowed, and
the narrowing is the point: it is **not "a version of the document."** It is one
statement, by one body, on one date, about some provisions. The document at any
date is *derived* from the set of them — see "Change, without a taxonomy of
changes" below, which is where the real mechanism is.

`revision --supersedes--> revision` stays as a **link**, recording lineage
because lineage is worth knowing (and because §9.5.1 of the real document names
supersession explicitly). But it is **documentation of ancestry, not the
mechanism by which current text is computed** — that distinction is what my first
draft got wrong. Splits, merges and "it came back different" need no special
handling; they are just assertions whose validity intervals overlap or do not.

Sponsorship rides the revision, not the policy — `contact --sponsored--> revision`
— which is what makes "who dropped this and who picked it up" answerable at all.

## 3. `provision` — a numbered piece of a policy

The real document is *drawn* as a tree. **It is not stored as one**, and the
author's objection is right: a tree is not a structure Void Core agrees with, and
it is not what interaction nets are made of.

Interaction nets are **agents with ports, joined by wires**
([Void Core: interaction nets](../../../VoidCore/okf/concepts/interaction-nets.md)).
Everything is local — an agent knows its ports and nothing else. A tree smuggles
in three things a net does not have: a **distinguished root**, a **global depth**,
and **exactly one parent per node**. The third is the one that actually breaks:
provisions are *shared*. Model codes get incorporated by reference; one section
is cited by four others; a merged policy has provisions with two ancestries. A
tree cannot hold that and a graph can.

So: **containment is a link**, which is Void Core's passive substrate —
`provision --part-of--> provision`, with an `order`. Links are non-reactive by
default, may dangle, and are exactly "a connection with no consequence"
([links](../../../VoidCore/okf/concepts/links.md)). Nothing enforces tree-ness,
sharing is free, and a cycle would be a *data* error to detect rather than a
type error to forbid.

**The tree is a `scry`** — a projection with a context, in Void Core's own
vocabulary, exactly as `Scene` is a projection of a mantle. Drawn as a tree,
stored as a graph. That is not a compromise; it is the same split the whole stack
already runs on.

**The number `3.3.1` is stored, not computed.** Tempting to derive it from
position — and wrong twice over. It is a **fact from the source document**: the
council wrote it, every citation in the world points at it, and it must survive
its neighbours being renumbered. Deriving it would make one amendment a mutation
cascade across a hundred runes and would silently break every inbound citation.
In Datalog terms it is **EDB, not IDB** — something asserted, not something
concluded.

# Change, without a taxonomy of changes

The author's second objection is the one that improved this design most: *do not
classify types of change; implement a general method and let the kinds fall out.*
My earlier draft had `amendment`, `suspension`, `repeal`, split and merge as
distinct mechanisms. That was a taxonomy I imposed because I was thinking in
edits — and an edit is a linear idea.

**The general method is the one Allomone already uses**, and reading its
foundations is what made this obvious:

> **A policy's history is a SET of dated assertions. "What the policy says on
> date D" is a projection with D in the context — derived, never stored.**

That is it. There is no change taxonomy because there are no changes as objects,
only assertions with validity intervals:

| what people call it | what it is |
|---|---|
| amendment | an assertion of new text for a provision, valid from D |
| suspension | an assertion of *non-application*, valid D₁–D₂ |
| repeal | an assertion of non-application, valid from D onward |
| a split | new provisions asserted under two policies |
| a merge | assertions from two lineages valid at the same date |
| "it came back different" | a later assertion; nothing was undone |

**Every one of these already fits Void Core's built machinery, with nothing
invented:**

- **`scry(state, context)` where `Context = {locale, audience, date, role}`** —
  **`date` is already a first-class context field**
  ([Scry](../../../VoidCore/okf/concepts/scry.md)), carried explicitly so output
  is reproducible. "The policy as of 2019-06-01" is a *view*, and the guarantee
  that live-preview == final render == archived send comes with it.
- **EDB / IDB** ([foundations §5](../../../VoidAllomone/okf/concepts/foundations.md)) —
  assertions are stored and undoable; the resolved text at date D is derived and
  stored nowhere. So correcting a mis-recorded amendment has **nothing to
  retract**, which is the same property that makes disabling an Allomone script
  free.
- **The join semilattice** — two assertions about the same provision at the same
  date, from different sources, compose exactly like two scripts' opinions:
  agreement is agreement, and genuine disagreement is **⊤, surfaced for a human**.
  That is not a hypothetical: legislatures pass conflicting amendments, and
  clerks reconcile them. The engine should say "these two disagree" rather than
  pick the one entered last.
- **Constraint stores as closure operators** ([foundations §3]) — adding an
  assertion only ever *refines*. You cannot make the record know less by telling
  it more.

## Does it pass the non-linearity test?

[non-linearity](../../../VoidAllomone/okf/concepts/non-linearity.md) states
the test every proposed feature must pass: *does this make the result depend on
something nobody authored?* Checked, clause by clause:

- **Order of entry** — irrelevant; the assertions are a set. Import 2019 after
  2025 and the answer is the same.
- **Duplicate entry** — irrelevant; the join is idempotent.
- **Position in a file** — no files.
- **Wall-clock** — **nothing consults a clock.** `date` is an argument in the
  context, exactly as `today` is a frozen frame input to our Allomone predicates.
- **A "run" step** — none; it is a projection.

**And the one that looks like a violation and is not:** the record *does* retain
time, which the attention graph is forbidden to do. The difference is the one
non-linearity.md itself draws — *"the objection was never to order per se, but to
order nobody chose."* Adoption dates are the most authored order in existence: a
body voted, on a date, in public. That is case 3 on that page ("resolutions are
authored, so their order is meaningful"), not a smuggled sequence.

## What this deletes

The `suspension` glyph I proposed yesterday, and the whole amendment/repeal
vocabulary. `policy`, `revision`, `provision` remain; **`revision` becomes
"an assertion, dated"** rather than "a version of a document," which is both
smaller and more honest about what a council actually does. `supersedes` stays as
a *link* — lineage worth recording — but it is documentation of ancestry, not the
mechanism by which the current text is computed.

# The rest of the model

## `statement` — one attributed thing said on the record

Text, a speaker, a meeting, an offset into the recording, topic tags.
**Statements live in a mantle per meeting.** A year of council meetings is
10⁴–10⁵ statements against a data mantle of hundreds; together, every projection
in the application would pay for a corpus almost nothing needs. Documents are
already mantles of block runes — a meeting is a mantle of statement runes, the
same move, loaded only when opened.

## Votes and speech are EDGES, not fields

`contact --voted-yes--> revision`. The single most valuable relation in the
dataset, and it must be a relation for the reason
[data-model](/concepts/foundation/data-model.md) already gives about presenters: queryable
from both ends, survives the data changing, and the graph tools work on it free.
"Which two councilors vote together most often" becomes a graph question over
existing machinery rather than a new report.

Likewise `statement --said-by--> contact` and `statement --at--> event`.

## Does it generalize?

The author's test cases, checked against the model rather than asserted:

| | policy | revisions | provisions | votes |
|---|---|---|---|---|
| Springfield ordinance | ✓ | as amended | numbered sections | ✓ |
| a platform's terms of service | ✓ | dated "effective" versions | numbered clauses | none — and that is fine |
| a tabletop rules system | ✓ | editions and errata | chapters and rules | none |

All three are *a normative text that changes over time, where the changes
matter*. That is the abstraction, and `policy` names it well.

# Attribution is the hard part, and it must never be a guess

Everything else here is engineering. **This is the part with a real failure mode
that hurts someone.**

If a transcript does not label speakers — and many do not — then attribution is
inferred, from voice, from turn-taking, from "Councilor Smith:" appearing in the
text. Every one of those can be wrong. And the output is a *public website that
says a named, real person said a specific thing.* A wrong attribution on a
controversial rezoning is not a data-quality issue; it is a false statement about
a person, published.

The discipline is the one we just built for Allomone, and it transfers exactly:

> **An uncertain attribution is surfaced, not resolved. A contested attribution
> renders as nothing.**

Concretely:

- Every `--said-by-->` edge carries a **method** (`labeled` from the source,
  `diarized` by audio, `inferred` from text, `human`) and a **confidence**.
- **Only `labeled` and `human` are publishable.** Everything else is visible
  in-app, flagged, and **excluded from the website** — which is
  `web-hide` at the export seam, the mechanism already built and tested.
- Confirming an inference is **a dispatcher command**, so it is logged,
  attributed and reversible, exactly like settling an Allomone conflict.
- **Every statement links to its source** — the video at a timestamp, or the
  transcript. The claim is checkable by the reader, which is the only real
  defence against being wrong.

**A transcript that labels its speakers is worth an enormous amount more than one
that does not.** If Springfield publishes labeled transcripts, take them and skip
diarization entirely. Establishing that, for this one city, is the first thing
worth doing — before any code.

# The archive: two kinds of time

The author wants the site to work like the Wayback Machine — *what did this look
like in 2023?* Void Core gives half of that for free and the half it does not
give is the half that matters.

- **Transaction time** — when we recorded something. The command log *is* this,
  totally and already: replay to a point and you have the model as we then knew
  it.
- **Valid time** — when something was *true in the world*. When someone actually
  held a seat.

These come apart constantly in a records project: importing 2019's minutes today
records, in 2026 transaction time, a fact whose valid time is 2019. **"Replay the
log to 2023" answers the wrong question** — it tells you what we had typed in by
then, not who was mayor.

- **Decision time** — when the body *voted*. Added 2026-08-17, because it is a
  third axis and not a rewording of the second.

So valid time is modelled explicitly. The cleanest fit is **terms as runes with
date ranges** — a reified `term` carrying `from`/`until`, since an edge cannot
hold dates — which keeps a person one rune across many terms and makes "who sat
on this in 2023" a query rather than a replay.

## Why decision time is separate, and what it fixes

`from` was doing two jobs: *when this takes effect* and *which assertion wins*.
They are the same number whenever a council adopts something effective
immediately, which is every assertion in the corpus we have, so the conflation
survived a long time without showing.

It breaks on a **retroactive amendment** — adopted in March, effective back to
January — which is ordinary in civic data. Two retroactive assertions sharing an
effective date get equal strength under the merge's `Unique` law and resolve to
**⊤**: the reader is shown a conflict, and told the law is unclear, when in fact
the council settled it three weeks after the first vote.

So the resolution splits the two roles cleanly:

| date | job | field |
|---|---|---|
| valid time | **filters** — does this assertion apply on date D? | `from` / `until` |
| decision time | **ranks** — among those that apply, which governs? | `adopted` |
| transaction time | neither; it is not on the rune at all | the command log |

`adopted` is a **field**, not a second interval, because it is a fact the source
states *about the act*; where an assertion sits in time is `from`/`until`. Absent,
it falls back to `from`, so every assertion stored before this change keeps
exactly the meaning it had. **Answered under
[Q31](/developer_questions.md); `civic_smoke` §4b is the test, and it fails under
the old ranking.**

# Geography, and the heatmap

Measures affect places. [Territory](/concepts/sections/territory.md) already has
location-faceted runes over a swappable map source, so a revision with a
`geo`/`mapshape` gets a marker or an area for free. "Which areas get the most
tax-exemption policies" is then: filter revisions by tag, project their areas,
count. The Allomone `map-` surface already colours by derived annotation, so a
heatmap is a derived `map-weight` — **no new engine.**

The honest caveat: most policies affect a *district or a parcel set*, not a
point, and Hormiga's geography today is points plus rectangles/ellipses. Real
boundaries mean polygons, which is a genuine Territory gap rather than a civic
one. **[Q32](/developer_questions.md).**

# What Springfield actually publishes (checked 2026-08-12)

Step 1 of the plan below was *go look*, so it was done first. The findings
change the ordering, mostly for the better.

| | |
|---|---|
| **Agendas & minutes, pre-Nov 2024** | **Laserfiche WebLink** at `laserfiche.springfield-or.gov/WebLink/` — a document-management portal with folders and full-text search |
| **Agendas & minutes, Nov 2024 on** | a request **form** on the city site, not a browsable archive |
| **Video** | **YouTube**, `@cityspfldoregon`, everything from **2020-09-21** |
| **Transcripts** | **none published** |
| **Civic API** | **none** — no Granicus, no Legistar, no CivicClerk |
| **GIS** | an **ArcGIS Hub** at `springfield-development-center-sporgis.hub.arcgis.com` |

Four consequences:

**1. There is no clean civic API, and that is the normal case.** Cities on
Granicus/Legistar have one; Springfield does not. So Void Reyna cannot be built
around a single tidy integration, which is an argument *for* the harvester
design rather than against it.

**2. The best news: minutes are labeled and votes are structured.** Council
minutes record who moved, who seconded, and how each member voted, by name, as a
matter of law. **The highest-value, legally-meaningful part of the dataset needs
no machine learning at all** — it needs a PDF parser and a careful eye. Votes,
attendance, and adopted revisions all come out of minutes, attributed, reliably.

**3. The worst news: speech is unlabeled.** No transcripts are published, so the
statement corpus has to come from YouTube's auto-captions — which are timed but
carry **no speaker labels**. So [Q33](/developer_questions.md) is live: getting
from captions to "Councilor X said this" is inference, on video of real named
people, published. It is exactly the case that has to be assistive rather than
authoritative.

**The two split cleanly, and that is the design's luck:** votes are *labeled and
authoritative*, speech is *unlabeled and inferred*. They should never be built by
the same pipeline or trusted by the same rule. Doing votes first gets a real,
publishable, fully-attributed dataset with zero ML risk.

**4. Boundaries are probably available.** An ArcGIS Hub means ward and district
polygons are likely downloadable as GeoJSON, which turns
[Q32](/developer_questions.md) from "should Territory grow polygons" into "there
is real data waiting for it."

# The model, built (2026-08-16)

Step 2 — *hand-build one meeting and see whether the model survives* — is done,
as `src/domain/civic.hpp` + `tests/civic_smoke.cpp`. **The council is fictional; the
document structure is not**, being modelled on the real Springfield PDF. The
people are invented deliberately: committing fabricated votes attributed to real
named officials would be exactly the harm this page argues against, in a public
repo. Verified structure, fictional claims — the same split the Cat Dataset uses.

Seven claims went in as design and came out as passing assertions:

| claim | how it is tested |
|---|---|
| containment is a graph | §9.5.3 sits under **two** parents; nothing objects |
| the tree is a view | `children_of` derives it from links, in stored `number` order |
| no taxonomy of change | amendment and suspension are the same glyph, different fields |
| "what did it say on date D" is a projection | `resolve_at` returns different text for 2024 and 2026 |
| a same-day disagreement is ⊤ | two amendments dated 2025-10-06 → conflict, value `""` |
| valid time ≠ transaction time | the mayor in 2021 is not the mayor in 2026, though both were typed today |
| an inferred attribution cannot publish | `diarized` is unpublishable; confirming is a command; `undo` reverts it |

## What building it changed

**1. Edges carry almost nothing, and that forces reification.** `link` accepts
`--relation`, `--weight` and `--undirected` — and the projection **drops even
the weight** (`maiz::SceneWire` has no weight field). So a projected edge is
`(relation, directed)`, full stop.

Consequences, both good: a **term** is a rune (an edge could not hold
`from`/`until`), and **speaker evidence lives on the statement** rather than on
the `said-by` edge — which is the better answer anyway, since a statement has
exactly one speaker, so attribution is a property of it and not a separate
object. It also means a command written in an earlier draft of
[Void Reyna](/concepts/projects/void-reyna.md) was **not a real command**; corrected there.

**2. The resolution is `maiz::merge`, and that was not a stretch.** Set an
assertion's **strength to its adoption date in days** and the Unique law reads
as the actual legal rule: *the later adoption governs; two adoptions on the same
day that disagree are a question for a human.* Nothing was invented — the
lattice already had the shape, and order-independence, idempotence and conflict
surfacing come with it instead of being re-implemented.

**3. An unplanned property fell out, and it is the good kind.** A *later*
amendment reconciling two clashing ones **clears the conflict with no human
involvement** — because a higher-strength opinion replaces the tied set rather
than joining it ([foundations
§1](../../../VoidAllomone/okf/concepts/foundations.md)). Nobody designed
that; it is the merge being correct. It is also exactly what a council does.

**4. `scry` is Python-only.** Void Core's `scry(state, context={date})` is the
right shape and the C core does not expose it, so the date projection is
host-side here. No harm — it turned out to *be* the merge — but worth knowing
before someone plans around calling `scry` from C++.

# The path, in order of what de-risks the most

Deliberately front-loaded with the things that can kill the project. Revised
against the findings above.

1. ~~Go look at what Springfield publishes.~~ **Done 2026-08-12** — see above.
2. ~~Hand-build one meeting.~~ **Done 2026-08-16** — `src/domain/civic.hpp` +
   `tests/civic_smoke.cpp`, see "The model, built" above. Seven design claims
   went in and came out as passing assertions, and four things changed on
   contact.
3. ~~Add the glyphs and the vote edges.~~ **Done** — `policy`, `provision`,
   `revision`, plus `term` and `statement`, which the exercise argued for
   (edges cannot carry dates, so a tenure is a rune).
3b. ~~Do it in the UI.~~ **Done 2026-08-16** — the glyphs are registered, a
   synthetic record is seeded into a `civic` mantle, and there is a **Civic
   Record window** to read it in. A *reading* surface deliberately: everything
   is editable in the Data tab, and what Data cannot show is the part that
   matters, because none of the interesting facts live on one rune. **The
   as-of-date control is the projection made visible** — move it and the
   provision text, the officeholders and the conflicts all change while nothing
   is stored per date.
4. **Void Reyna: the evidence locker, then real minutes.** **This is the next
   thing, and the ordering was wrong until 2026-08-16.**

   > **Correction.** Step 4 used to be "generate the website," on the argument
   > that a one-meeting site proves the spine end to end. That argument does not
   > survive being checked. **The Builder already generates sites** — it ships
   > the cat colony's newsletter and website today, so this would mostly
   > re-prove something proven, and it is a *software* de-risk when every risk
   > this project actually has is in the **data**: whether Springfield's minutes
   > parse, whether votes extract reliably, whether attribution is feasible at
   > all. A website built on the synthetic council de-risks none of those.
   >
   > And there is a sharper reason: **a rendered site built from fabricated
   > council data is a fake civic record.** Fine as a local test, bad as an
   > artifact that could be screenshotted or deployed by accident. The seeded
   > data exists to test the model, not to be published.

   4a. **The evidence locker** — fetch politely, archive the bytes, stamp a
   content-derived id, detect change. No parsing at all. It has to work before
   anything depends on it.
   4b. **Minutes → votes.** Laserfiche → PDF → who moved, seconded and voted →
   a command transcript. **No video, no captions, no ML**, because minutes
   record all of that by name as a matter of law.

5. **Then the website, from real data** — at which point `web-hide` and the
   attribution line stop being tested and start being load-bearing, which is the
   only version of this step that proves anything new.
6. **Void Reyna v2: the statement corpus.** YouTube captions, segmentation,
   assistive speaker attribution, and the human confirmation surface. Everything
   with real risk is here, behind a product that already works without it.
7. Topic clustering, geography, and the rest of the search surface.

**Steps 2–3 needed no new application, no Python and no network** — and they are
done. **Step 4 is where real data enters**, which is where the real risk always
was; the website waits for it rather than preceding it.
