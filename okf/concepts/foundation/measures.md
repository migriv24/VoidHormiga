---
type: Concept
title: Measures — which of Hormiga's numbers are values on edges, and which stay fields
description: "Issue #2's first point, investigated rather than adopted: SPEC §3.7.1's rule applied to every number Hormiga keeps. Points stay fields; magnitudes read by rules may become value edges to measure runes. The helper (Void Maiz measures.hpp) and the mathematics it enforces: Stevens' levels of measurement decide which summaries mean anything."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-10-25T00:00:00Z
---

# The proposal, and the rule it rests on

Issue #2's client moved their catalogue's numbers onto shared measure runes:
`product -[40]-> target-margin`. The unit is stated once, on the measure, and the
graph can answer "everything with a target margin" or "the highest in this
category". They asked that Hormiga do the same in its importers and renderers.

Void Core's SPEC §3.7.1 already states when this is right, and it is narrower than
"every number":

> If a number is read by rules that produce new structure, it belongs on an edge
> where the rules can see it. If it is read only by renderers, it belongs in a
> field. A weight is a magnitude, so a point is not one.

# Hormiga's numbers, one by one

| number | what it is | where it belongs |
|---|---|---|
| an event's date and times | a point on a timeline | field |
| a place's coordinates, a pin's position | a point in space | field |
| a block's row, column, span | a grid position | field |
| a card colour, a divider height | presentation | field |
| a contact's phone number | an identifier, not a number | field |
| a product's price (grocery workspace) | a magnitude, ratio scale, read by renderers and by totals | **a measure, when totals or rules read it** |
| a product's stock (grocery workspace) | a magnitude, read by the "low stock" rule | **a measure** |
| a supply's quantity (smart-home workspace) | a magnitude, read by an "expiring" rule | **a measure** |
| a client's catalogue margins | a magnitude, ratio scale, compared across products | a measure (the issue's own case) |

So the outreach organization, Hormiga's main workspace, has no number that
belongs on an edge. The demonstration workspaces do, and so does anyone whose
data is a catalogue. That is the answer to "adopt it in importers and
renderers": **only where the number is a magnitude something computes with**.
A field-to-measure migration of the demos is open as
[Q128](/developer_questions.md).

# The helper, and the mathematics it enforces

Void Maiz now carries the vocabulary the issue asked to have upstream
(`voidmaiz/measures.hpp`, 2026-10-25). It takes a scene and returns commands, so
every write goes through the dispatcher:

- `ensure_measure(scene, name, glyph, quantity)`: the commands that make a measure
  rune state its unit and level, and none if it already does. A rune of that
  name that is not a measure is refused, never repurposed.
- `write_value(subject, measure, value)`: one `link … --weight`, which Void Core
  treats as an upsert, so a re-import does not duplicate.
- `read_value`, `values_of`: read back from the projection.
- `summarise(scene, measure, statistic, subjects)`: a summary, but **only one
  the measure's level admits.**

That last one is where the research is. A measure's `level` is Stevens'
level of measurement ("On the theory of scales of measurement", Science, 1946).
Each level is defined by the transformations that preserve it, and a statistic
is meaningful exactly when it commutes with all of them:

| level | preserved by | admits |
|---|---|---|
| nominal | any one-to-one relabelling | count, mode |
| ordinal | any increasing function | and min, max, median |
| interval | any increasing affine map x ↦ ax + b | and mean |
| ratio | any similarity x ↦ ax | and sum |

The proof sketch for the two that surprise people:
- **A mean of ranks is meaningless.** mean(f(x)) = f(mean(x)) holds for affine f
  and fails for an arbitrary increasing f, and a rank only promises the latter.
- **A sum of temperatures in Celsius is meaningless.**
  Σ(ax_i + b) = aΣx_i + nb: the offset grows with the count, so the sum depends
  on where the scale's zero was put.

`summarise` refuses with that reason instead of returning a number that looks
right. A measure with no stated level is read as nominal, the weakest claim.
Two smaller choices follow the same logic:
- the median of an even number of ordinal values is the lower middle, a value
  the scale actually has;
- sums and means use compensated (Kahan) summation, so a catalogue's worth of
  prices does not drift.

`tests/measures_smoke.cpp` in Void Maiz runs all of this against a live core.

# What it means for scale

A value edge per record is exactly the shape that made Void Maiz's projection
O(E·N): every edge's endpoints were found by scanning every node. That was fixed
on 2026-10-24 (an O(1) name map), and [scale](/concepts/platform/scale.md)
measures it. Adopting measures before that fix would have made a 10^5-record
catalogue unusable, which is why the issue's two points could not be taken
separately.
