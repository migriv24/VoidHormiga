---
type: Concept
title: The frontier — a database that never finishes loading
description: "The author's premise (2026-11-03): assume every database is infinitely large and densely interwoven, and still traverse it all. What failed when the stress database ate a machine's memory (Hormiga's whole-document open, not the graph), why an interwoven graph can still be loaded in pieces (the cut is a constant factor; only the radius explodes), and the mathematics a working set needs to act on partial knowledge honestly: the knowledge order, completeness certificates, Gaifman locality and four classes of question, summaries as monoid homomorphisms over the cluster tree, loading as Z-set deltas. The vocabulary proposed to Void Rancho (el lindero), exports as complete embarques cut into partidas, opening never refused, and the tests that would prove it."
tags: [status:proposed, audience:dev, confidence:exploratory]
timestamp: 2026-11-03T00:00:00Z
---

# What failed, and what did not

The author, 2026-11-03: Hormiga *"can't open the stress test. It CAN open other
smaller databases just fine ... it begins to load, then ends up eating all my
ram until it crashes. I dont want this software to just completely consume a
host device's ram and resources."*

**The memory goes to Hormiga's own open, not to the graph.** The app still
gives Void Core the whole database as one state document, then holds the
projected scene, the export and the undo frames on top of it. The
5×10^5-rune stress database measured about 18 GB that way on 2026-10-25
([scale](/concepts/platform/scale.md)). The app uses Void Rancho only to read a
bundle. Its working set, which pages by cluster, is not in the app yet: that
is stage 5 of the [store](/concepts/platform/store.md). The same database
under Rancho's working set opened and took edits at a **290 MB peak under a
512 MB cap** (2026-11-02). Nothing checks before an open whether the device
can afford it, so the app tries, and the machine pays.

**The stress database is the easy case.** Its links cluster with modularity
Q = 0.977 ([clusters](/concepts/platform/clusters.md)). It is almost a set of
islands, because people meet where they live. The author's worry is the
harder one, and the right one to design for:

> *"the entire mantle/graph is likely very interconnected. making it almost
> imporssible to truly chunk load things ... this is also the exact idea behind
> hormiga, and i in fact, HOPE that databases are built this way. this
> interconnectedness should be the application's strength!"*

# The premise (the author, 2026-11-03)

> *"from now on, les really assume we have infinately large datasets, and we
> still need to traverse it all."*

Read as requirements on every feature from now on:

1. **Bounded residence.** What a device holds is a function of the device,
   never of the database.
2. **Productivity.** Every finite question finishes with work bounded by the
   question, not by the database: drawing a view, opening a record, an edit, a
   search page. The word is from coinduction. An infinite structure is
   *productive* if every finite observation of it finishes (Hughes 1989; the
   coalgebra tutorials). The database form is **scale independence** (Fan,
   Geerts & Libkin 2014): a query is scale-independent if it can be answered
   by reading a bounded number of tuples, whatever the size of the database.
   They show that bound comes from access constraints, meaning indexes with a
   cardinality bound.
3. **Honest partiality.** Every answer shown knows whether loading more could
   change it.
4. **Traversal is a pass, not a residence.** "Traverse it all" means streaming
   passes over the store with bounded memory, whose results are stored as
   derived data. It does not mean holding it all.

# Why an interwoven graph can still be loaded in pieces

A piece does not need to be isolated. It needs its **boundary** to be
affordable. For a set of runes S, let ∂S be the edges with exactly one end in
S. The graph's edge expansion is h(G) = min |∂S| / |S| over the sets holding at
most half of it.

- **A modular graph** (the stress database) has |∂S| ≪ |S|. A cluster is
  nearly closed.
- **A maximally interwoven graph**, an expander, has |∂S| ≥ h·|S| for every
  such S, so no piece is ever nearly closed. But that is a constant factor. A
  piece of k runes has at most d·k boundary edges, where d is the average
  degree. Loading the piece together with one stub per boundary edge costs
  O((1 + d)·k). That is a lot, but it is never "everything".
- **What explodes is radius.** The r-hop neighbourhood of a piece grows like
  (d − 1)^r in an expander. A small-world graph reaches everything in about
  log n hops (Watts & Strogatz 1998).

**So a piece with a one-hop fringe of stubs is always affordable, and a piece
with its full r-hop neighbourhood is not, for r beyond 1 or 2.** Whatever needs
a larger radius must not be answered by loading. The store answers it, from an
index or a summary, or a pass computes it. The author's intuition is right
about where the difficulty is. The difficulty is in the questions, not in the
loading.

# The knowledge order: acting on partial information

A working set W is an under-approximation of the database D: W ⊑ D. Loading
moves W up the **knowledge order**. That is Belnap's (1977) second ordering, in
which a value can be unknown, true, false or both. Allomone's merge lattice
already has "both" as ⊤, the conflicted cell. "Not known yet" is its
counterpart at the bottom. Two results do all the work:

- **A monotone question is sound on any W.** If Q is monotone (adding facts
  never removes an answer), then Q(W) ⊆ Q(D). Every positive answer found in a
  partial load is already true, and stays true. This is the certain-answers
  result of incomplete databases (Imieliński & Lipski 1984).
- **Any other question needs completeness of its footprint.** A question that
  uses negation over other runes, a count, "none", "every", a maximum or
  uniqueness is answered correctly only if W agrees with D on everything the
  question reads. A **completeness statement** declares that agreement for a
  part of the database. Whether a set of such statements makes a question's
  answer complete is a decided question in the database literature (Motro
  1989; Levy 1996; Razniewski & Nutt 2011). Razniewski and Nutt show that the
  weakest precondition for a query's completeness can be written as table
  completeness statements. Those statements identify exactly the part of the
  database a query's completeness depends on.

So **every answer Hormiga shows carries one of three statuses**:

| status | meaning | loading more can |
|---|---|---|
| **settled** | its footprint lies inside the part W is certified complete for | change nothing |
| **growing** | it is monotone: true so far, and more may join | only add to it |
| **open** | it is not monotone, and its footprint is not complete | revise it; show it as a bound ("at least 12") |

**The conflict the author anticipated** (*"you may in fact be loading
something, start working, only to eventually load something else that
conflicts"*) can only happen to an answer already marked open. Loading never
contradicts a settled answer, and never removes anything from a growing one.
An edit made with a partial working set is never invalidated by a later load.
It is a command on named runes, and the runes beyond the frontier were always
there. **What must never be decided from W alone** is a constraint over the
whole database: a duplicate name, "is this tag new?", "who else has this
email?". Those are always asked of the store, which holds everything.

# Locality: which questions a piece can answer

**Gaifman's theorem** (1982): every first-order property of an element
depends only on that element's radius-r neighbourhood, where r depends on the
formula and not on the graph. That is the mathematics of chunk loading. It
sorts every question Hormiga asks into four classes:

| class | what it is | answered by | examples |
|---|---|---|---|
| **L_r**: local, radius r | determined by a rune's r-ball | the working set, exactly, for any rune whose r-ball is inside W and its fringe | Allomone today (r = 0); "linked to something tagged X" (r = 1, if stubs carry tags); a map bestowal (the shapes covering the rune) |
| **M**: monotone, global | a set that only grows with the data | the store, paged, never by loading | search results; "every event tagged picnic"; a query-backed block |
| **A**: decomposable aggregate | h(S ∪ T) = h(S) ⊕ h(T) | summaries kept per cluster-tree node | counts by kind or tag; date ranges; a map's bounding box; the tag vocabulary |
| **R**: recursive, global | not first-order, not local | passes in the store, maintained incrementally, stored as derived data with an as-of date | reachability; components; PageRank; clustering itself |

Three consequences for things that exist:

- **Allomone is exact on any loaded rune, however little is loaded.** Its
  kernel conditions read only the subject (its tags, kind, name and mantle),
  so its radius is 0. Negation (`no(t)`) is safe too, because it negates a
  fact about the subject, and the subject is loaded. This holds while
  conditions cannot read derived values or other runes. Void Allomone's own
  foundations (`VoidAllomone:/concepts/foundations.md`, §5) already name that as
  the hinge of its design. The day a rule reads a neighbour, its radius becomes 1. The day
  rules feed each other in a cycle, the radius is unbounded and the rule is
  class R.
- **A map bestowal has a spatial radius.** A rune's given tags depend on the
  shapes covering it (`domain/bestow.hpp`). The rule that keeps this exact:
  **loading a rune loads every shape that covers it** (the *covering
  closure*). A spatial index in the store can answer that cheaply, since the
  shapes are few and large.
- **Global measures today are computed over the whole data mantle**
  (`app.cpp`, the graph measures stamped on each rune). Under the premise,
  they are class R, and belong in the store as a pass.

**Hub runes** (a city, a measure, a tag everyone carries) break the fringe's
bound, because one rune has a million links. Rancho already files measure runes
as always loaded. The general rule: **a hub is summarized, never expanded.**
Its links are counted in its summary, not listed in the fringe. Systems that
process power-law graphs split hubs the same way (Gonzalez et al. 2012, the
vertex-cut).

# The cluster tree is the map's quadtree

The Leiden hierarchy already in Rancho defines an **ultrametric** on runes:
d(a, b) is the height of the smallest cluster holding both. Its balls are
exactly the clusters. They are nested and never overlap, which is what makes a
quadtree a quadtree. So the graph gets what the [map](/concepts/sections/gis/index.md)
already has:
- tiles at every zoom;
- an overview at any size, one node per cluster drawn with its summary
  (hierarchical aggregation, Elmqvist & Fekete 2010);
- level of detail: open a cluster and its children replace it.

**Summaries are monoid homomorphisms.** For any question of class A, keep
h(node) at every node of the tree, with h(parent) = ⊕ h(children). Finger
trees call that a *measure* (Hinze & Paterson 2006); column stores call it a
zone map or small materialized aggregate (Moerkotte 1998). A class A question
then reads O(depth) summaries and no rune, and an edit updates one leaf-to-root
path. A question that does not decompose (distinct counts over huge sets, a
median) gets a sketch with an error bound (HyperLogLog, count-min) and is
shown as approximate.

**For an infinite database the tree is infinite, and every window of it is
finite.** It is browsed as codata, unfolded only where it is observed.

# Loading is a change like any other: Z-sets

Write W as a **Z-set**, a multiset with integer weights. Loading a region adds
its runes with weight +1, and evicting adds −1. DBSP (Budiu et al., VLDB
2023) gives every query over Z-sets an incremental form automatically,
including relational algebra, aggregation and recursion. **A load is then just
another delta, handled by the same machinery as an edit.** Allomone's
annotations, query-backed blocks and counts are maintained across loads exactly
as across edits, and the status of each answer is updated with it: a load can
settle an open answer, or grow a growing one.

# Traversing it all: passes

Class R questions are answered the way out-of-core graph systems answer them:
iterate the whole graph in bounded memory by streaming it from disk in an order
that keeps neighbours near each other.
- GraphChi (Kyrola et al. 2012) uses parallel sliding windows.
- X-Stream (Roy et al. 2013) streams the edges.
- Mosaic (Maass et al. 2017) iterates a trillion-edge graph on one machine in
  21 minutes, using Hilbert-ordered tiles.

The store's clusters are already such an order. Between passes, results are
maintained incrementally. Dynamic-frontier Leiden (Sahu 2024) re-runs only
around the runes whose links changed.

**The honest limit.** A pass over an infinite database never finishes. A class
R answer is therefore always "as of the part visited". That is not a defect of
the design. A positive reachability answer ("these two are connected") is found
in finite time, while a negative one never is. Even reachability is
semi-decidable in the same monotone way as everything above: it is growing
until a path is found, and then settled.

# El lindero: the vocabulary, proposed to Void Rancho (2026-11-03)

The author asked for the concept to have names of its own, *"bonus points if
its in spanish related to these concepts"*. These are a rancher's words. Each
one names an idea above, and the image does some of the explaining. The
**corral** and the **herd** were already in Rancho's own naming offer
(`VoidRancho:/design/open-questions.md` §1); the rest extend it. Rancho may take
them, change them or decline them, and the author decides.

| término | literally | what it names |
|---|---|---|
| **el potrero** | the fenced pasture | the store at rest: the whole database, on disk or shared |
| **el corral** | the pen near the house | the working set: what Core holds |
| **la manada** | the herd | a cluster, the unit that moves in and out of the corral together |
| **el lindero** | the boundary line between two properties | the frontier: the edge of the corral, and the name of the whole concept |
| **el mojón** | a boundary stone, a marker that says the land goes on | a stub: the far end of an edge that leaves the corral (name, glyph, tags, herd) |
| **arrear** | to drive cattle in | load a region into the corral (`want`) |
| **soltar** | to turn out to pasture | drop a region from the corral (`drop`) |
| **la cuenta** | the head count | the certificate: what the corral holds complete |
| **cabal** | exact, complete (*la cuenta cabal*) | an answer that is settled |
| **creciente** | rising, as a river does | an answer that is growing |
| **suelto** | loose, at large | an answer that is open and may be revised |
| **la reseña** | a summary, from *señal*, the earmark that identifies a herd | a herd's summary: counts, ranges, a bounding box (a monoid homomorphism) |
| **el rodeo** | the roundup that rides the whole range and gathers every animal | a pass: a traversal of the whole potrero in bounded memory |
| **el embarque** | the shipment of a herd to market | a complete export: a newsletter, a website, a game's build |
| **la partida** | one lot of a shipment | a piece of an embarque too large to be one file |
| **el molino** | the mill, which grinds what is brought to it, as much as is asked | an output made on demand from the database, rather than shipped whole |

So the frontier reads:

| word | asks | the potrero answers with |
|---|---|---|
| `arrear <region>` | a region into the corral: a herd path, a tag expression, a spatial box, or a rune's r-ball | the runes, the **mojones** at the lindero, and **la cuenta** |
| `soltar <region>` | a region out of the corral | a −1 delta |
| `reseña <herd>` | a herd's summary | h(herd) |
| `preguntar <question>` | a class M, A or R question | the answer and its status: cabal, creciente or suelto |
| `rodeo <job>` | a pass over everything | the job's result, stored as derived data with the date it finished |

**Residence is view state, not history.** Moving herds in or out of the corral
is a reload, never an edit, so it does not enter the journal (Rancho's rule). It
is recorded beside the journal, though, so a headless replay can reproduce what
a person saw (CLAUDE.md rule 3).

# Exports: el embarque (the author, 2026-11-03)

> *"any export that is transforming the data from a database into some other
> modality (like into a newsletter or something), then we have to be able to
> completely traverse the database and encapsulate it fully in some way. like
> we also gotta think about void squid or something, where it needs to actually
> compile a video game. we would want to theoretically create like, a final
> singualr thing (even if it would be petabytes in size)."*

That answers Q131, and it is stronger than its lean:

- **An export is a rodeo.** It reads the whole potrero, never the corral. Its
  output is cabal by construction, because nothing it could have read was left
  unread. For Hormiga today: a newsletter, a website, a calendar feed and the
  map's files read the whole data mantle from the store when only a part is
  loaded (`HormigaApp::export_data_scene`).
- **The output is one thing in identity, many in storage.** An embarque is a
  **manifest** naming its **partidas**, each content-named (SHA-256) and
  immutable. That is Rancho's segment design
  ([segments](/concepts/platform/store.md); `VoidRancho:/concepts/segment.md`),
  applied to outputs rather than to the database. The manifest's hash names the
  whole, so a petabyte embarque is still "a final singular thing": one name, one
  signature, one version. It is never one petabyte file, though. It can be
  written, copied, resumed, verified and served a partida at a time, and two
  embarques that share a partida store it once. A Merkle tree is the same idea;
  so are Git's objects and an OCI image's layers.
- **A partida is a bounded piece of the output, not of the database.** The
  output's own structure cuts it: a website's pages, a game's levels and asset
  packs, a newsletter's sections. The rodeo writes each as it completes it, so
  the memory an embarque needs is one partida's, whatever the total.
- **El molino is the later, creative form.** An output that is a function of the
  database, evaluated when someone asks: a website that renders a page when it
  is requested, a game that streams its world from a potrero as the player
  moves. It needs no complete encapsulation, because it never ships the whole.
  Hormiga encapsulates: a newsletter goes to people who cannot query anything.
  Rancho should keep both possible, because Void Squid's game builds may want
  either.

# Opening is never refused (the author, 2026-11-03)

> *"no, we should always be able to open any database. we shouldn't develop
> with this hesitation ... i dont want a temporary bandaid, i want the issue to
> not exist. if the bug still persists (we are unable to load some database),
> then that means we have to keep developing to prevent that."*

That answers Q132: no refusal, no warning, no "open anyway". The budget never
decides *whether* a database opens, only *how much of it the corral holds*.
**Opening any database is a requirement on every version from now on, and a
database that does not open is a bug, whatever its size.**

# Who does what

| who | what |
|---|---|
| **Void Core** | nothing new for stubs, since dangling edges are legal. Its standing asks (`touched`, reified undo) matter more, because a reload loses memento undo |
| **Void Rancho** | mojones and la cuenta in `Store::state(Selection)`, reseñas per herd, the covering closure, questions with a status, rodeos, embarques in partidas, and the two tests below |
| **Void Allomone** | keep the kernel at radius 0. When tier 1 lets rules read derived values, the effect-graph analyzer computes each rule's radius and calls a cycle class R |
| **Void Maiz** | the overview scene (clusters as nodes, with summaries) and the fringe drawn as "more this way" |
| **Hormiga** | stage 5 (the app on the store, begun 2026-11-03); never decide a global constraint from the working set; show each answer's status; exports as rodeos |

# The tests that would prove it

1. **The interwoven database.** A `stressgen` variant whose links are random
   (an expander, Q ≈ 0), at the stress database's size. Rancho's 512 MB
   harness must still pass. This is the case the author hopes real databases
   approach, and the one the current stress database does not test.
2. **The endless database.** A store backed by a generator, whose rune n and
   its links are a function of n, so that it has no last rune. The app must
   open it, browse, search, edit and export a selection. Every view must
   finish with bounded work, and memory must stay flat while a person browses
   forever. That is the operational meaning of "infinite".
3. **Status honesty.** On finite databases, for random questions over random
   working sets:
   - every settled answer equals the answer over the whole database;
   - every growing answer is a subset of it;
   - every open answer is a sound bound.

# Status

`proposed`. Built: Rancho's working set, its clusters and its store-side
queries; Core's tolerance of dangling edges; Allomone's radius-0 kernel.
Proposed: everything else on this page. Asked in
`MESSAGE_FOR_VOIDRANCHO_hormiga-the-frontier-2026-11-03.md` and
`MESSAGE_FOR_VOIDRANCHO_hormiga-el-lindero-2026-11-03.md`. Q131 and Q132 were
answered by the author the same day (above).

# Sources

- Belnap, N. (1977). A useful four-valued logic.
- Budiu, M., Chajed, T., McSherry, F., Ryzhyk, L., Tannen, V. (2023). DBSP:
  automatic incremental view maintenance for rich query languages. PVLDB 16(7).
- Elmqvist, N., Fekete, J.-D. (2010). Hierarchical aggregation for information
  visualization. IEEE TVCG.
- Fan, W., Geerts, F., Libkin, L. (2014). On scale independence for querying
  big data. PODS. And Fan, Geerts, Cao, Deng, Lu (2015), Querying big data by
  accessing small data, PODS; Cao, Fan (2015), Making pattern queries bounded
  in big graphs, ICDE.
- Gaifman, H. (1982). On local and non-local properties. Logic Colloquium '81.
- Gonzalez, J. et al. (2012). PowerGraph: distributed graph-parallel
  computation on natural graphs. OSDI.
- Hinze, R., Paterson, R. (2006). Finger trees: a simple general-purpose data
  structure. JFP.
- Hughes, J. (1989). Why functional programming matters. The Computer Journal.
- Imieliński, T., Lipski, W. (1984). Incomplete information in relational
  databases. JACM.
- Kyrola, A., Blelloch, G., Guestrin, C. (2012). GraphChi: large-scale graph
  computation on just a PC. OSDI.
- Levy, A. (1996). Obtaining complete answers from incomplete databases. VLDB.
- Maass, S. et al. (2017). Mosaic: processing a trillion-edge graph on a single
  machine. EuroSys.
- Moerkotte, G. (1998). Small materialized aggregates. VLDB.
- Motro, A. (1989). Integrity = validity + completeness. ACM TODS.
- Razniewski, S., Nutt, W. (2011). Completeness of queries over incomplete
  databases. PVLDB 4(11).
- Roy, A., Mihailovic, I., Zwaenepoel, W. (2013). X-Stream: edge-centric graph
  processing using streaming partitions. SOSP.
- Sahu, S. (2024). A starting point for dynamic community detection with
  Leiden algorithm. arXiv:2405.11658.
- Traag, V., Waltman, L., van Eck, N. (2019). From Louvain to Leiden.
  Scientific Reports.
- Watts, D., Strogatz, S. (1998). Collective dynamics of small-world networks.
  Nature.
