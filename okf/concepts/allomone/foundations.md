---
type: Concept
title: Allomone — foundations & prior art (DSL + interaction nets)
description: "The research grounding for Allomone: it is an EXTERNAL DSL (own syntax/parser/AST → our own IntelliSense) over Void Core's INTERACTION-NET substrate. What to take from the DSL literature (LSP-style tooling) and the interaction-net literature (Lafont's combinators; Mackie's programming-language-for-inets; Jiresch / Fernández–Mackie on real-world I/O and external programs — directly the Antfarm question)."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-04T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). Research requested by the author
(2026-08-04): read the DSL and interaction-net literature and decide **what to
bring into Hormiga**. Two clean conclusions, then the specifics.

# Two positioning conclusions

1. **Allomone is an EXTERNAL DSL, not an internal one.** An *internal* DSL is an
   API embedded in a host language (limited to the host's syntax, hard to
   analyze); an *external* DSL has its **own syntax, its own parser, and its own
   AST**, and every tool — editor, highlighter, completion, checker — is built
   for it. We already chose the external path (Allomone Script + `allo_parse` +
   the rune-tree AST). The **cost** of an external DSL is that we build the
   tooling ourselves; the **payoff** is total freedom of notation **and** that
   the IntelliSense is *ours to invent*, tuned to the domain
   ([intellisense](/concepts/allomone/intellisense.md)). The literature's headline
   best practice for a serious external DSL: **treat tooling as first-class**
   (the LSP/language-server model — highlighting, diagnostics, completion). Our
   version of "the language server" is the **language service** in the shared
   editor engine ([language](/concepts/allomone/language.md#the-shared-text-editor-engine-with-notes--author-2026-08-03)).

2. **Allomone is a DSL *over an interaction-net substrate*.** Void Core is built
   on **Lafont's interaction combinators** (three agent types, universal), and
   the interaction-net world has a direct precedent for what we're doing:
   **Mackie (2005), "Towards a programming language for interaction nets."** A
   language whose runtime is interaction-net rewriting is not exotic — it is a
   studied thing, and modern systems (HVM/Bend/Vine from HigherOrderCO) show it
   scales. This is the deep reason the [paradigm](/concepts/allomone/paradigm.md)
   framing (graph rewriting, not Scratch's actor model) is right: the substrate
   *is* a rewriting system.

# The interaction-net vocabulary, and where Hormiga already uses it

The core notions (Lafont), and their Hormiga reading:

- **Agents & ports** — nodes with a **principal** port and **auxiliary** ports.
  Void Maiz's block/node model literally exposes principal + aux ports; a rune's
  wiring is port-to-port. Allomone reads this graph.
- **Active pair / interaction rule** — two agents connected principal-to-principal
  reduce by a **local** rule. This is the mathematical shape of a *materializing*
  Allomone rule (when mutation is enabled): match a local pattern, rewrite it.
- **Reduction strategy / confluence** — the order of firing; a well-formed system
  is confluent (same result regardless of order). This is exactly our
  [conflicts](/concepts/allomone/conflicts.md) concern (specificity/layering) and
  the reason we prefer idempotent, order-independent rules.
- **Interaction combinators** — the minimal universal set. We don't expose
  combinators to script authors, but it's why the substrate can express anything
  we lower to.

# What to TAKE into Hormiga (the actionable finds)

- **Mackie (2005), a programming language for interaction nets** → the design
  license: our language may lower to net rewriting and stay principled. Keep
  Allomone's evaluation **local and rule-shaped** so it maps cleanly onto the
  substrate (and, later, onto materialization as real graph rewrites).
- **Jiresch (2012), interaction nets + real-world I/O**, and **Fernández & Mackie
  (2001), combining nets with externally defined programs** → **this is the
  Antfarm question, already solved in the literature.** Pure interaction nets are
  closed; real systems need **I/O agents / external calls** at the boundary. Read
  the **Antfarm holidays as exactly those boundary agents**: a source holiday is
  an *input agent* feeding data into the net; an output holiday (newsletter,
  website) is an *output agent* consuming a projection. Allomone's
  [inputs](/concepts/allomone/inputs.md) and [domains](/concepts/allomone/domains.md)
  are the net's boundary; the interior (tags, rules) stays pure and confluent.
  **Takeaway: keep the pure rewriting core separate from the I/O boundary** — the
  same separation the literature settled on.
- **Term-rewriting equivalence (Fernández & Mackie)** → sanity check that our
  Datalog/production-rule reading ([paradigm](/concepts/allomone/paradigm.md)) and
  the graph-rewriting reading are two views of one thing; we can reason in
  whichever is convenient.
- **Optimal reduction / parallelism (HVM, Lévy-optimality)** → *not now*, but a
  note: because the substrate is an interaction net, a future Allomone could
  evaluate **in parallel** over large datasets. Don't design anything that
  precludes it (keep effects commutative where possible).
- **DSL tooling = LSP** → build the **language service** early and share it with
  Notes; completion/diagnostics are where the domain value is
  ([intellisense](/concepts/allomone/intellisense.md)).

# What NOT to take

- **Combinator-level authoring** — script authors write Allomone Script, never
  raw agents/ports. The net is the *implementation*, not the surface.
- **Optimal-reduction machinery** — the BOHM/optimal-lambda apparatus is about
  efficient λ-evaluation; our workloads are set-at-a-time tag queries, not
  higher-order reduction. Borrow the *vocabulary*, not the machinery.
- **Turing-completeness for its own sake** — Allomone will get variables, lists,
  and (gated) loops, but the default engine stays a terminating fixpoint
  ([reactivity](/concepts/allomone/reactivity.md)); we add power deliberately.

# One-line synthesis

**Allomone = an external DSL (own syntax/AST/tooling) whose runtime is graph
rewriting over Void Core's interaction-net substrate, with the Antfarm as the
I/O boundary the interaction-net literature already tells us to keep separate
from the pure core.**
