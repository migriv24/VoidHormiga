---
type: Concept
title: Allomone — paradigm
description: "What Allomone is, academically: a declarative production / graph-rewriting system over the rune graph, coordinated stigmergically — the precise opposite of Scratch's imperative, object/actor-oriented model."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). This document fixes *what kind
of thing* Allomone is, so every later decision has a foundation and prior art.

# The one distinction to remember

**Scratch programs objects. Allomone programs the relations among objects.**

Scratch is **imperative, object/actor-oriented**: a *sprite* is an autonomous
object with its own scripts; sprites coordinate by **broadcast events**
(message-passing — the actor model). It operates **object-at-a-time** ("*this*
sprite, move 10 steps").

Allomone is the opposite on two independent axes:

- **Declarative, not imperative** — a rule states *what should be true* ("cards
  tagged `democrat` are blue"), not a step-by-step procedure. The engine, not
  the author, decides evaluation order.
- **Set-at-a-time, not object-at-a-time** — a rule's condition is a **pattern
  matched against the whole dataset at once**; the action applies to *every*
  rune that matches. You never name a single rune. This is the **relational /
  collection-oriented** stance (SQL/Datalog), the opposite of naming one sprite.

Structurally it is a **production / graph-rewriting system**, and it coordinates
**stigmergically**. The four lineages below each own a piece of the design.

# 1. Production-rule systems (forward chaining)

A **rule** is `condition → action` over a **working memory** (here: the mantle's
runes + tags + links). The runtime is the classic **match → conflict-resolution
→ act** cycle; the efficient pattern matcher is the **Rete algorithm** (Forgy,
1979) — it caches partial matches so a change re-tests only what's affected
(this is also the hook for our delta reactivity,
[reactivity](/concepts/allomone/reactivity.md)). This is the primary framing for
"rules applied to the dataset *as a whole*". Prior art we inherit: the
**conflict set** (rules that fire on the same fact) and **conflict-resolution
strategies** ([conflicts](/concepts/allomone/conflicts.md)). Lineage: OPS5,
CLIPS, Drools, Jess.

# 2. Datalog — the declarative semantics

**Datalog** (logic programming over a database) supplies the *meaning* of a rule
set: rules over a graph of facts, evaluated to a **least fixpoint** (keep
deriving until nothing new appears), with **stratified negation** (negation only
over already-settled facts, so "not tagged X" is well-defined). It also gives the
vocabulary for **stored vs derived** facts — **EDB** (extensional: the tags/links
actually in the database) vs **IDB** (intensional: facts a rule computes) — which
is exactly the **derive vs materialize** distinction
([domains](/concepts/allomone/domains.md), and the explainer in
[developer_explanations](/developer_explanations.md)).

# 3. Graph transformation / interaction nets — the substrate

When a rule *changes structure* (adds a tag, adds a link, fills an empty logo),
it is a **graph rewrite**: match a subgraph pattern, produce a new subgraph
(algebraic graph transformation — the double-/single-pushout constructions; tools
GROOVE, AGG). The specific substrate underneath is **Lafont's interaction nets**,
which *are* Void Core's engine — nodes with **ports** rewritten by local rules.
This is why the author's instinct — "**port mapping, graph rewriting**" — is
exactly right, and why a materializing Allomone rule is not a foreign bolt-on but
the same kind of operation Void Core already performs.

# 4. Stigmergy — the coordination model

**Stigmergy** (Grassé, 1959) is coordination by **modifying a shared
environment**: ants leave **pheromone** trails; other ants read the environment
and act; global order emerges with **no direct messaging**. Allomone's runes
coordinate the same way — rules **read and write the shared graph** (tags ≈
pheromones), and system-wide behavior emerges without Scratch's broadcast
events. In systems terms this is a **blackboard / tuple-space** architecture
(Linda), not message passing. It is the reason we deliberately ship **no event
blocks** in the actor sense: *the graph is the message*. (Change **deltas** —
"on tag change" — are still allowed; they are reactions to the environment
changing, not messages between agents. See
[reactivity](/concepts/allomone/reactivity.md).)

# Consequences that follow from the paradigm

- **No execution order to author.** Rules are a *set*, not a *sequence*; order is
  the engine's concern, mediated by conflict resolution.
- **Idempotence & confluence matter.** A well-formed rule set should reach the
  same fixpoint regardless of firing order (confluence); we prefer rules that are
  idempotent (re-firing changes nothing). Non-confluent sets are surfaced as
  conflicts, not silently order-dependent.
- **The whole dataset is the unit.** UI, storage, and performance are sized for
  "match across 10²–10³ runes", not "one object at a time".
- **It is still all dispatcher commands.** A materializing rule **macro-expands
  into Void Script** ([language](/concepts/allomone/language.md)); an
  observational one is a **standing query** evaluated at projection. The founding
  pillar (everything is a replayable command) holds.
