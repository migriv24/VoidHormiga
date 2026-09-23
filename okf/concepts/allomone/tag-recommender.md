---
type: Concept
title: Allomone — the tag recommender
description: "When adding tags, suggest tags over the tag co-occurrence graph — the same graph substrate Allomone reads, run in reverse (structure → suggested tags). Three settings-selectable modes: similarity (reinforce clusters), dissimilarity (make distinct), comprehensive (connect stranded nodes)."
tags: [status:built, audience:dev, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). **Built 2026-08-05** (author's
ask). This is a small feature with a big conceptual tie: **Allomone reads the
tag graph to derive styling; the recommender reads the same graph to derive
*suggested tags*** — the substrate run backwards. It also foreshadows the
graph-analytic inputs Allomone will grow ([inputs](/concepts/allomone/inputs.md)
§4: centrality, community, connectivity).

# What it does

When you open the shared [tag editor](/concepts/allomone/language.md) (contacts,
organizations, scripts, calendar days — anything with `draw_tag_editor`), below
the `+ tag…` picker it shows a row of **suggested tags** as one-click chips. The
suggestions are computed over the **tag co-occurrence graph of the target's
same-glyph peers** — the author's framing: *"if you're adding tags to a script
or a contact or an event, what other tags do events/scripts have?"* — and ranked
by a **mode** set in **Settings → Tag recommendations** (config
`ui.tags.recommend_mode`, 0/1/2).

# The three modes (a spectrum over how the graph should grow)

Similarity between two things is **Jaccard overlap of their *meaningful* tags**
(`type:` is the glyph itself; `icon:`/`color:` are render directives — all three
are excluded from the similarity signal and from suggestions).

- **0 — Similarity** *(reinforce clusters).* Suggest tags held by things **similar**
  to the target: `score(t) = Σ_{c carries t} sim(target, c)`. Things that are
  alike get tagged alike. Cold start (target has no tags): fall back to
  **frequency** among peers.
- **1 — Dissimilarity** *(make distinct).* Suggest tags that pull the target
  **away** from its neighbors, pivoted at the mean similarity:
  `score(t) = Σ_{c carries t} (sim_mean − sim(target, c))` — tags common among
  *similar* things score negative; tags from the far side of the graph score
  high. Cold start: **rarest-first**.
- **2 — Comprehensive** *(no stranded nodes).* The author's connectivity goal:
  "make sure everything is connected… don't leave stranded nodes/branches."
  Suggest tags that forge a **new** link (to things the target shares nothing
  with yet), preferring the **least-connected** of those:
  `score(t) = max_{c carries t, sim(target,c)=0} 1 / (1 + degree(c))`, where
  `degree(c)` is how many peers `c` co-occurs with. So the **most stranded**
  node's tags surface first. **It was a sum (Σ) until 2026-09-22.** Void Maiz,
  porting it, found that a sum lets *two* well-connected peers holding one tag
  outscore *one* stranded peer holding another, which sinks exactly the node
  the mode exists to reach. The maximum takes the best link a tag would forge,
  and their `tags_smoke` pins the case.

Verified on a synthetic two-cluster-plus-stranded graph: for a target inside
cluster A, similarity → the cluster-A tag it lacks; dissimilarity → the *other*
cluster's tags first (its own cluster last); comprehensive → the **stranded
node's tag first**, then a bridge to the other cluster. The three rankings are
provably distinct and each behaves as designed.

# Where it lives

- **`maiz::suggest_tags(scene, target, opt)` in Void Maiz**
  (`voidmaiz/tags.hpp`), since 2026-09-22. The author: *"tag suggestion ...
  should be a maiz native thing (because multiple other void based applications
  should be able to do it)"*. The port was mechanical, because ours was already
  a pure read of `scene.nodes` filtered to the target's glyph. `skip_prefixes`
  defaults to our `type:`, `icon:` and `color:`. Hormiga's own
  `compute_tag_suggestions` is deleted.
- **The cache stays Hormiga's** (`tag_rec_cache`, keyed on
  `(target · tags · mode)`), because the library function is pure and only the
  host knows when its scene changed.
- `draw_tag_editor` renders the chips (each emits a `tag <name> +<t>` command —
  a normal logged dispatcher command, like every other change).
- **Settings** writes `ui.tags.recommend_mode`; it is read on boot into
  `tag_rec_mode`. Machine/config tier (a per-user preference, not org data).

# Relationship to Allomone

The recommender is **not** an Allomone script — it's a host feature. But it is
the **inverse computation** on the same substrate, and it is a natural candidate
to **become** an Allomone capability once the language grows: a suggestion is a
*derived* proposal over the graph, and materializing it (actually writing the
tag) is exactly the **gated mutation** Allomone defers
([roadmap](/concepts/allomone/roadmap.md) "Materialization"). Today the human
clicks the chip; later an Allomone script could *propose* tags the same way,
still leaving the write to a human until mutation is unlocked. Its graph reads
(similarity, degree/connectivity, community) are the same
[inputs](/concepts/allomone/inputs.md) §4 measures Allomone will consume.
