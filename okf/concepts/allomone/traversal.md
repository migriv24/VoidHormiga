---
type: Concept
title: Allomone — graph traversal (refer by STRUCTURE, not by name)
description: "Allomone traverses the rune graph structurally — neighbours, clusters, degree, reach — so logic refers to runes it doesn't know exist yet and survives growth and link change. Built 2026-08-05: neighbours/linked/cluster/degree, plus host-computed centrality (eigenvector) + community (label propagation) and BFS within/distance. Exact betweenness/closeness are the heavier next rung."
tags: [status:built, audience:dev, confidence:asserted]
timestamp: 2026-08-05T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). Extends the links input
([inputs](/concepts/allomone/inputs.md) §3).

# The principle: generalize, don't name

The author's steer (2026-08-05), and it is a *core* design principle:

> "Steer away from referring to specific runes or specific linked runes. Look
> broadly at how we refer to runes we don't know exist yet… Allomone should
> gracefully expand to 1000+ contacts. If the links change, that shouldn't change
> how the logic works. Today garfield is closely connected to whiskers; maybe in
> the future he's most connected to charles — but the logic that groups them
> should still matter."

So Allomone refers to runes **by their place in the graph**, not by identity:
"the cluster garfield is in," "his neighbours," "the most central," "everyone
within his group." This is the same **stigmergic, set-at-a-time** stance as the
rest of the language ([paradigm](/concepts/allomone/paradigm.md)) — you describe
a *shape*, and whatever runes fit it are swept in, now and forever.

**Naming a specific rune is the exception, not the rule** — runes can be deleted
or renamed, and a hard reference is brittle. When it IS needed, it goes through a
name↔ID indirection so it survives renames (see
[editor](/concepts/allomone/editor.md)). But the language is *built for the
general case.*

# Built (2026-08-05): the structural verbs

The host attaches every mantle wire to its two runes (both directions), so the
interpreter sees each rune's edges `(relation, neighbour)`. Four builtins:

| verb | returns | meaning |
|---|---|---|
| `neighbours(rune)` | a set of runes | everything directly linked to it (any relation) |
| `linked(rune, "rel")` | a set of runes | neighbours via a relation (`"rel:"` prefix ok) |
| `cluster(rune)` | a set of runes | the **connected component** it belongs to (BFS over links, fuel-bounded) |
| `degree(rune)` | a number | how many distinct neighbours — a cheap **degree centrality** |

They return ordinary rune sets, so they compose with everything else —
`runes where …`, `count`, `union`/`intersect`/`minus`/`overlaps`, `any`/`all`.
The author's example — *"a variable that is the cluster which x rune is part
of"* — is literally `local mygroup = cluster(rune)`.

Verified: on the cat colony (friend/rival/sibling + member-of + attends +
celebrates edges) the shipped `10-traversal-and-clusters` example colors hubs
(`degree >= 6`), cats with a rival (`count(linked(rune, "rival-of")) > 0`), and
members of big clusters (`count(cluster(rune)) >= 20`) — **naming no rune.** And
the generalization test "color everyone in the same cluster as a hub" is
`cluster(rune) where degree(rune) >= 3` — pure structure.

# Global measures — BUILT 2026-08-05

`cluster`/`degree` were the cheap, in-interpreter start; the richer measures the
author named now ship too. The split is exactly as designed
([inputs](/concepts/allomone/inputs.md) §4): the **global, costly** ones are
**host-computed once per run** (budgeted) and stamped on each rune, so Allomone
reads them O(1); the **pairwise/reach** ones are in-interpreter BFS.

| verb | how | meaning |
|---|---|---|
| `centrality(rune)` | host: **eigenvector** (power iteration), normalised [0,1] | how important — connected to important runes |
| `community(rune)` | host: **label propagation**, returns the set | the detected sub-group (a richer `cluster` than the raw connected component) |
| `within(rune, k)` | interpreter: **BFS to depth k** | every rune within k hops (incl. itself) — the neighbourhood |
| `distance(a, b)` | interpreter: **BFS** | hop distance between two runes (−1 if unreachable) |

`allo_compute_measures` (app.cpp) runs the two global passes each refresh — cheap
at the org scale (10²–10³ runes) and never per-keystroke, matching the
reactivity doc's "expensive path." Verified: the shipped `12-graph-measures`
example colors central figures (`centrality >= 0.6`), members of big communities,
and busy neighbourhoods on the cat colony (76 runes), naming nobody.

**Still future** (heavier, or weighted): exact **betweenness** / **closeness**
centrality, `closest(rune, n)` (nearest-n), and edge-weighted distance. Same
shape — a host pass or a bounded BFS behind one more builtin; the interpreter
surface is already right, and the **engine is untouched**.

# Why this doesn't touch the engine

Traversal is **more builtins over the same rune-value model** — the tokenizer,
parser, evaluator, derive-only rule, and fuel budget are untouched. Adding a
graph measure later is the same shape: a builtin that reads a value the host
supplies. The **fundamental engine stays intact** (the author's check) — the
language just gains reach.
