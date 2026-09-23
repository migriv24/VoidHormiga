---
type: Concept
title: Void Reyna — the dataset generator
description: "The sibling application that manufactures datasets from the outside world: harvest, archive, extract, emit. Why it is separate (ground rule 5 forbids vendoring an ML stack), why it shares the VERB VOCABULARY rather than a library, what a holiday IS mathematically (an effect boundary plus a pure Lens) and why that makes the pivot rule a theorem rather than a preference, why the archive — not the crawler — is the hard part and why Void Core already built it, where the models go and why that answer is forced."
tags: [status:draft, audience:all, confidence:proposed]
timestamp: 2026-08-12T00:00:00Z
---

**Status: a design, not a build. Not this repo.** Named by the author
2026-08-12. Its first client is [the civic record](/concepts/projects/civic-record.md);
its decisions are [Q30, Q33–Q38](/developer_questions.md).

**Reyna** — the queen. A queen ant does not forage or build; she **founds the
colony**. Void Reyna produces the dataset that Hormiga then keeps, maps, dates
and publishes. The name is right in a way "Forager" was not: this is not an
errand-runner, it is where a colony comes from.

# What it is

Four steps, in order, and the middle one is the one people skip:

    HARVEST   fetch from the public web — portals, PDFs, video, GIS
      ↓
    ARCHIVE   store raw bytes + URL + timestamp + content hash, forever
      ↓
    EXTRACT   parse, segment, transcribe, cluster, attribute
      ↓
    EMIT      a transcript of Void Core commands

It is a **Python** application with whatever ML stack it needs, and it is not in
this repo.

# Why it is separate (and why nothing else is)

The line, from [the civic record](/concepts/projects/civic-record.md): *a view over the
model stays in Hormiga; a producer of the model from outside stays out.* Void
Reyna is the only piece on the author's list that is a producer. Data Manager,
Data Maps, Time Organizer and Document Builder are one model seen four ways and
stay one application.

The decisive argument is **ground rule 5 — vendor, don't depend.** Hormiga
vendors everything: no package managers, no CDNs. That rule is what makes it
auditable and offline-forever, and it is not negotiable. **You cannot vendor a
speech-recognition stack.** So the choice is not "one app or two" but "break the
rule or move the ML out," and moving it out is the rule working exactly as
designed: the thing that needs `pip` gets its own home, and the thing that must
stay auditable stays that way.

Everything else follows the same split naturally: batch versus interactive,
partial results versus toasts, hours versus 16ms, "re-run it" versus "undo."

# The seam: it emits VERBS, not data structures

**Void Reyna links nothing, includes nothing, and shares no header with
Hormiga.** It writes dispatcher commands:

    mantle new meeting-2026-08-04
    rune new statement st-0142
    set st-0142 text 'We should not adopt this without the fiscal note.'
    set st-0142 offset "01:14:22"
    set st-0142 method "diarized"
    set st-0142 confidence "0.61"
    link st-0142 "Councilor Stevens" --relation said-by
    link st-0142 meeting-2026-08-04 --relation at

**Corrected 2026-08-16 by building it.** An earlier draft of this block wrote
`link … --relation said-by --confidence 0.61 --method diarized`, which **is not a
real command**: `link` accepts only `--relation`, `--weight` and `--undirected`,
and the projection drops even the weight (`maiz::SceneWire` has no weight
field). So a projected edge is `(relation, directed)` and nothing else, and
anything richer must live on a rune. Speaker evidence therefore rides the
*statement* — which is the better answer anyway, since a statement has exactly
one speaker, so attribution is a property of it rather than a separate object.
This is the kind of thing that stays wrong until something is built.

Three things this buys, and each of them is load-bearing:

1. **Conformance is automatic.** An import lands as one replayable batch —
   logged, undoable, attributable — because that is what commands are (ground
   rule 3). There is no second write path to audit.
2. **The contract is already written.** It is [verbs](/verbs.md). Not a schema
   to keep in sync across two languages, in two repos, at two release cadences.
3. **Void Reyna could be rewritten in anything** — or replaced by a different
   tool entirely, or by a person typing — and Hormiga would not notice. A tool
   that emits a text transcript is a tool you are never locked into.

**In the Antfarm it is a `records` source**, exactly like the CSV and Sheets
importers ([the Antfarm](/concepts/platform/antfarm/index.md)). The payload-typed canvas
enforces the connection for free: a `records` plug fits `core.records` and
nothing else.

**Transport is a file.** A command transcript, or a `.miga` bundle when assets
travel with it ([.miga v3](/concepts/platform/miga-format.md)). No daemon, no socket, no
protocol to version — the two applications never have to be running at the same
time, which is the loosest coupling available and the correct one for a batch
producer.

# What a holiday actually is, mathematically

The author's instruction: *use the holiday structure as much as possible when
transforming data from one structure to another,* and *Void Reyna should max out
on the concept — review holidays and what they are mathematically.* Doing that
turned up the good news that **most of it is already built and named**, in Void
Core, and that the thing Void Reyna needs is one small missing operation rather
than a new theory.

## Void Core's own definition, which already says "compute"

> A **holiday** is how Void Core reaches a system it does *not* own — it is "away"
> from the pure core. All real I/O is a holiday: the core does no file/network/
> stdout work itself. […] the pattern is general (data, asset, **compute**,
> output, LLM, and *knowledge* backends).
> — [Void Core: holiday](../../../VoidCore/okf/concepts/holiday.md)

So *"generators are a compute holiday"* is not an extension of the concept; it is
already on Void Core's own list, and the missing piece there is a **tagged holiday
registry with capability selection and fallback chains**, which is marked
`planned`. Void Reyna is a forcing client for exactly that.

And the line that settles the Python question: **"a holiday is reached over a
protocol, so the backend's language is irrelevant."**

## The pure interior and the impure edge

Void Core has three **pure** transformation layers, all built
([transform-layers](../../../VoidCore/okf/design/transform-layers.md)):

| verb | what it does | law |
|---|---|---|
| **reduce** | interaction-net rewrite to normal form; `reduce(net) -> net` | strong confluence on the restricted fragment |
| **temper** | normalize owned state to canonical form | `temper(temper(x)) == temper(x)` |
| **scry** | project state (+ snapshot) + context → a view | same inputs ⇒ byte-identical output |

And banked as a consensus invariant across four apps:

> **Effects live at the holiday boundary; none of reduce/temper/scry ever fires
> one.**

That is the whole architecture in one sentence, and it hands Void Reyna its shape:

    the world  ──[ holiday ]──▶  snapshot  ──▶ reduce/temper/scry ──▶ commands
               impure, quarantined         pure, replayable, testable

**A holiday is the only impure arrow in the system.** Everything Void Reyna does
that is *interesting* — parsing, structuring, clustering, attributing — should
sit on the pure side of that line, and everything that touches the network, the
disk or a model sits on the other, in as thin a layer as possible.

## The mathematics that pays: a holiday is an effect boundary + a Lens

[foundations §7](../../../VoidAllomone/okf/concepts/foundations.md) sets the
standing rule for this whole family of projects — *adopt the behaviour, not the
vocabulary; the burden is to show it makes an implementation simpler.* A sheaf
framing was rejected on that ground. So the bar here is not "does this sound
rigorous" but "does it already pay."

It does, and it is already written:

> A **Lens** is the two-way case: a `forward` projection and its `backward`
> inverse, bundled with the **round-trip law**, so a lossy mapping is structurally
> caught. This is the shape a *persistence* mapping wants — e.g. **a holiday's
> record⇄rune mapping**. […] Pure (no I/O) — **the holiday still does the
> file/network; the Lens is just the mapping + law.**
> — `VoidCore/scry/lens.py`

So the account of a holiday, in one line:

> **holiday = an effectful boundary crossing + a pure `Lens` (forward, backward,
> round-trip law).**

It pays three ways, none of them decorative:

1. **One mapping instead of three.** Portfolio Manager wrote a record⇄rune
   mapping separately for read, write and persistence and shipped a lossy-tag bug
   between the copies. A `Lens` owns it once; `lens.check(samples)` is the
   regression guard. This is a bug class that has *already happened in this
   family of projects*.
2. **`unscry(scry(x)) == x` is testable, and it is the honest definition of "this
   adapter doesn't lose anything."** For a civic archive that is not a nicety —
   a lossy import is an altered public record.
3. **Lenses compose** — and that is the whole future-proofing argument, below.

## The pivot rule, which is what the author is actually asking for

The author's polygon/voxel example is the sharp form of the question: *even if we
could map polygons straight onto our map, we consciously choose a holiday, so
that a future voxel city costs one adapter and not a rewrite.*

That instinct is right, and the reason is composition. Write direct adapters and
you need one per *pair* — polygon→map, voxel→map, geojson→map, shapefile→map, and
again for every consumer. Route through a **pivot** and you need one per
*format*:

    polygon ⇄ geometry ⇄ map
    voxel   ⇄ geometry ⇄ map
    voxel   ⇄ geometry ⇄ heatmap        ← free, wrote nothing new

N adapters instead of N×M, and the composed round-trip law is the proof: **if
both legs are lossless, the composite is lossless.** That is a theorem, not a
hope, and it is why the pivot is worth the extra hop even when the direct path
is easier — which is precisely the case the author flagged.

**So the design rule for every holiday in this family:**

> **Never write a direct A→B holiday when A→pivot→B exists. If no pivot exists,
> the first job is to name one.**

For the ward-boundary case that means Territory needs a **geometry payload** that
is neither polygon nor voxel — a Void-Core-native notion of *region*. Without it,
"use a holiday" is just a direct adapter wearing a costume. **This is the real
work item hiding inside Q32**, and it is a Territory question, not a civic one.

**The one thing genuinely missing upstream:** `Lens` has `check`,
`check_inverse` and `inverse()`, but **no `compose`**. Composition is exactly the
operation the pivot rule runs on, its law is one line
(`backward∘backward ∘ forward∘forward`, losslessness composing), and without it
every host re-implements chaining by hand. Drafted as a message to Void Core —
small ask, concrete forcing case, law attached.

## The Antfarm graph, read back

Hormiga's [Antfarm](/concepts/platform/antfarm/index.md) already types ports by **payload**
(records / assets / site) and already refuses a mismatched plug. Read against the
above, that canvas is doing something precise: **payload types are the objects,
holidays are the arrows, and wiring is composition.** The type-check the canvas
performs for free *is* composability. (**2026-09-22:** only the canvas performs
it. The dispatcher accepts an ill-typed `link`, so composition is not yet
checked at the door. See [the Antfarm CLI examples](/concepts/platform/antfarm/cli-examples.md)
§8 and [Q83](/developer_questions.md).)

Two things already in that document turn out to be the same idea and are worth
naming as such:

- **`out-html` is already a transform**, not a sink: it consumes `records` +
  `assets` and emits `site`. So "compute holiday" generalizes something the
  Antfarm has been doing since the Q25 redesign.
- **"Fallback, Cache and Logger are holiday *wrappers* — resilience as a graph
  property."** Those have type `holiday → holiday`. That is an algebra of
  holidays that already exists in the design and has never been called one.

# What this makes Void Reyna

Not "a scraper with an importer." A **domain-transfer engine** whose whole job is
crossing boundaries safely:

    HARVEST   a holiday. Impure, thin: fetch bytes, record what happened.
      ↓
    ARCHIVE   `materialize` with a provenance stamp (below). Owned state.
      ↓
    EXTRACT   pure. Lenses, temper, scry — parse, structure, normalize.
      ↓
    EMIT      dispatcher commands. Already conformant, already replayable.

And **the archive step is already built too**, which was the biggest surprise of
this session:

> `materialize(..., stamp=<field>)` records **provenance** — `provenance(data)`
> is a stable, order-independent snapshot id — so an archive carries proof of
> *what snapshot it captured* (and a reader can tell if the live data still
> matches). — [Scry](../../../VoidCore/okf/concepts/scry.md)

That is *exactly* the tamper-detection requirement: a content-derived id, pure,
no clock, so "has this page changed since we cited it" is a comparison rather
than an investigation. Q34 does not need a new harvest-record design; it needs
**a holiday that snapshots, and `materialize` with a stamp.**

And one more inherited rule that matters more than it looks:

> **Holiday-backed data is resolved from a *snapshot*, never folded into
> authoritative state at edit time** (bake-into-state is a bug magnet;
> derive-from-snapshot is the default).

For a civic record that is a *provenance* guarantee, not just hygiene: harvested
claims stay visibly harvested until someone explicitly bakes them.

# Where the models go, and why that answer is forced

Void Core's invariant 4: **"any nondeterminism is injected by the action, never
generated inside."** A diarization model is nondeterministic and impure. So it
cannot live in the pure interior — and the resolution is not "give up on
reproducibility," it is:

> **Run the model at the holiday boundary, record its output as data, and make
> everything downstream a pure function of that record.**

The model's output becomes an **input**, not a computation. Consequences, all
good, and all forced rather than chosen:

- **A run is re-runnable** without the model — the segments are archived.
- **The model version is part of the record**, so "re-transcribed with a better
  model in 2028" is a new assertion, not a silent overwrite. (Which is the same
  shape as a council amending a policy — one mechanism, two uses.)
- **A model can be swapped** without invalidating anything downstream.
- **Everything after the boundary is testable** with no GPU and no network.

This is also why the LLM ban is not squeamishness: an LLM's output is
irreproducible *and* unciteable, which fails both the invariant and the archive's
whole reason for existing. A small bounded local model, recorded with its
version, passes.

# Harvesting is the main thing, and the ARCHIVE is the hard part

The author is right that this cannot be a small nervous feature. But the
sharpest framing is not "we need a crawler" — anyone can fetch a URL. It is:

> **Fetching is easy. Being able to prove, in two years, what a page said on a
> given day is the hard part and the valuable one.**

That is what separates the Internet Archive from a scraper, and it is what the
author is actually asking for when they say *"if they change previous meeting
transcripts, we would have to have locally stored data to prove it."*

So: **every fetch is recorded as evidence.** URL, UTC timestamp, HTTP status and
headers, the **raw bytes**, and a **snapshot id**. Nothing is parsed before it is
archived, and the archive is never edited.

**The snapshot id is not ours to invent** — see above: `provenance(data)` is
already exactly that, and `materialize(..., stamp=)` already writes it. What
Void Reyna adds is only the transport facts a content id cannot carry (the URL,
the time, the HTTP response), which is a much smaller job than the one this
section originally described. Hormiga's content-hashed asset ingestion is the
same move a layer down, so the discipline is not new anywhere — only newly
pointed at the network.

Change detection is then a comparison rather than an investigation, and **a
snapshot id that moved on a document supposed to be final is itself a finding** —
arguably the most interesting thing this project could ever surface about a
government.

## On the fear of crawling

Web crawling has a bad name because of what bad crawlers do, not because
fetching public documents is fraught. What makes the Internet Archive acceptable
is not scale or special permission — it is manners, and manners are a short
list:

- **Identify yourself** in the User-Agent, with a contact URL.
- **Rate-limit per host** and back off on 429/503. One request every few seconds
  is invisible to a city web server and fast enough for a weekly harvest.
- **Respect `robots.txt`**, and note that government portals generally permit
  document directories precisely because the documents are meant to be read.
- **Fetch conditionally** — `If-Modified-Since` / `ETag`. Polite *and* it makes
  change detection nearly free.
- **Take the structured path when one exists.** Prefer an API, then a sitemap or
  RSS, then HTML. Never re-download what the archive already holds.

**Scope is what keeps it honest:** public records published by public bodies.
Not YouTube at large, not social media, not anything behind a login. That is a
narrow, defensible target — and the author drew that line already.

## Two external services worth using rather than reinventing

- **The Wayback Machine's CDX API** lists every capture of a URL with its
  timestamp *and a content digest*, which means **you can see that a page
  changed without downloading a single capture.** Its Save Page Now endpoint
  also lets you push a URL into a third-party archive — so a claim rests on
  someone else's copy as well as ours, which is worth much more than our copy
  alone. **[Q35](/developer_questions.md).**
- **Per-platform adapters beat a generic crawler.** Most US municipal records
  live on one of a handful of systems (Granicus/Legistar, CivicClerk, PrimeGov,
  Laserfiche, NovusAGENDA). **Springfield is on Laserfiche WebLink**, and
  Laserfiche has a documented folder/search API. An adapter per platform makes
  every city on that platform nearly free — which is exactly the Antfarm's
  port-mapping idea one layer out: **map each source to a typed interface, then
  the generic path is the fallback rather than the plan.**

# Models: assistive, never authoritative (the rule for attribution)

The author's framing is the correct one and it should be the project's rule:
**models distinguish; humans identify.**

A speaker-diarization model is very good at *"these forty segments are the same
voice"* and has no idea *whose* voice it is. That is precisely the useful split:
clustering is a low-stakes, verifiable, enormously labour-saving operation, and
naming is a claim about a real person that gets published. So:

> **A model may propose a grouping. Only a human may attach a name.**

Concretely: cluster the voices, let a person label each cluster **once** — and
one click then attributes hundreds of segments. That is the difference between
an afternoon and a month, with the accountability intact.

Everything published carries **method** (`labeled` / `diarized` / `inferred` /
`human`) and **confidence**, and **only `labeled` and `human` may reach the
website** — the `web-hide` export seam that already exists and is tested. This
is the Allomone discipline, unchanged: **surface, never guess.**

**LLMs: avoided by default.** The author's instinct is right, and it has a
technical reason, not only a taste one: an LLM's output is not reproducible and
cannot be pointed at a source, which is the opposite of everything the archive
is for. Small local models with a bounded job — diarization, embeddings for
topic clustering, layout-aware PDF parsing — are reproducible enough to record a
model version and re-run. **Nothing generative writes text that gets published.**
A small local model (the author's example: `needle`) is acceptable for *summary
suggestions a human accepts*, on the same assistive-not-authoritative terms.

# Open questions

[Q30](/developer_questions.md) (does it exist at all — lean yes),
[Q33](/developer_questions.md) (attribution), [Q34](/developer_questions.md)
(what a harvest record is, exactly), [Q35](/developer_questions.md) (Wayback as
a second witness), [Q36](/developer_questions.md) (does Hormiga get a
harvest/provenance glyph, or does Reyna keep its own archive and pass only
citations).
