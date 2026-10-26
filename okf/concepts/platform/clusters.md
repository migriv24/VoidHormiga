---
type: Concept
title: Clusters — smaller and smaller, as the unit a weak device holds
description: "The author's proposal (2026-10-25): Hormiga should cut its graph into smaller and smaller clusters from what the mantle already says, so a weak device works on a cluster and never the whole. What is built (Void Maiz's Leiden hierarchy, measured on a 5×10^5-rune database), what the research says about showing and loading by cluster, and what it would take — most of it a change feed from Void Core."
tags: [status:proposed, audience:dev, confidence:measured]
timestamp: 2026-10-25T00:00:00Z
---

# The proposal

Most of the devices Hormiga runs on are not the laptop it is built on: an
organizer's phone, a donated desktop. The author's direction, 2026-10-25: a
database should not cost a device in proportion to its whole size. Hormiga
should "make smaller and smaller clusters based on information we have in the
mantle/graph", and work in one at a time.

Measured, the reason is plain ([scale](/concepts/platform/scale.md)). At 5×10^5
runes, every edit pays three costs linear in the database:
- Void Core's undo memento, about 2.4 s;
- the export, 1.6 s;
- the projection of the data mantle, about 7 s.

Linear is the problem, not the constant: Nielsen's 1 s limit at 5×10^5 runes
is 2 µs a rune for everything. A cluster of 2×10^3 runes is the size
[data model](/concepts/foundation/data-model.md) was designed for. An edit that
costs O(cluster) is fast on any device.

# What is built

**Leiden** (Traag, Waltman & van Eck, 2019), in Void Maiz (`voidmaiz/graph.hpp`),
in C++, deterministic for a seed. It has the three phases:
- fast local moving, from a queue;
- refinement, which merges a node only into a well-connected subset, so every
  community it returns is connected;
- aggregation.

Louvain has no refinement step, and the paper shows it can return communities
that are internally disconnected; Leiden cannot. On Zachary's karate club it
finds Q = 0.4188, against a best known 0.4198.

**The hierarchy.** It runs Leiden recursively inside each community until every
community is below a target size. That is "smaller and smaller" in the literal
sense. Each rune gets a path, written as tags: `c1:12`, `c2:12.4`, and so on.

**Measured on the stress database** (`tools/datasets/`: 2.7×10^5 real
organizations from the IRS exempt-organization file, 1.5×10^5 synthetic
contacts, 7.7×10^4 events, 6.6×10^5 structural links):

| | |
|---|---|
| level-1 clusters | 23,609 (largest 8,640) |
| modularity | 0.977 |
| depth to ≤ 2,000 | 2 |
| NMI with city | 0.77 |
| NMI with field | 0.14 |
| time | 4 s |

The two NMI values carry the finding. The graph's own structure recovers
geography well and the organization's field barely. People meet where they live
before they meet by subject, and their links say so. A cluster is therefore
not a stand-in for any one tag. It is what the links say, which is the point of
computing it rather than filtering by city.

# What the research says about using them

**To show** a graph too large for the screen, the literature's answer is
hierarchical aggregation (Elmqvist & Fekete, 2010). The overview draws a cluster
as one node, sized by what it holds, with the wires between clusters bundled.
Opening a cluster replaces it with its children. That is Shneiderman's "overview
first, zoom and filter, then details on demand" (1996), with the cluster tree
as the zoom. Every level shows a bounded number of things, whatever the size of
the database.

**To load**, the same tree is the unit of residence. A device holds:
- the overview, which is one node per top-level cluster;
- the clusters it has open;
- nothing else.

Memory and the cost of an edit then scale with what is open, not with the
database. This is how map tiles and level-of-detail rendering already work, and
it is how Hormiga's own map tiles work.

**To keep current**, the clustering must not be recomputed from scratch on every
edit. Leiden's local-moving phase is local. Dynamic-frontier variants of Louvain
and Leiden (2024) re-run it only around the nodes whose links changed, and keep
the rest. A new contact joins its organization's cluster in time proportional to
its neighbourhood.

# What it would take

1. **A change feed from Void Core:** which runes a command touched. Without one,
   no host can know that an edit stayed inside a cluster; it can only re-export
   everything and compare. Asked for in the scale message, section 4.
2. **Residence below the whole document.** A cluster could be its own mantle,
   since Void Core has mantle composition (0.2.12). Or Core could export and
   load by cluster. Which is the right seam is Core's to say, and the answer
   decides everything after it.
3. **The overview view in Void Maiz:** a scene of clusters, with aggregate wires.
   It is the same projection, one level up.
4. **Clustering as a command**, so it is replayable (CLAUDE.md rule 3). Its
   output is tags, and a tag pass is already a batch. The `c1:`/`c2:` tags the
   stress tool writes are what that command would produce.

The open decisions are in [developer questions](/developer_questions.md),
Q129.
