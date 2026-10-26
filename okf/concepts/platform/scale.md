---
type: Concept
title: Scale — what a database of 10^5 runes costs, measured, and the models that fixed what was ours
description: "Opened 2026-10-24 by issues #1 and #2 (a client two orders of magnitude past the documented scale). Every cost was measured before anything was changed: the growth order of each, the per-edit budget against Nielsen's limits, the bundle rewritten as a scanner with byte-weighted progress, the Data tab made O(visible), the projection's O(E·N) and its section ping-pong removed, and what is left (Void Core's undo memento) sent upstream. Then 5×10^5 runes from public data, opened in the app and sampled until the window was usable."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-10-25T00:00:00Z
---

# The question, and how it was answered

`data-model.md` plans for 10^2 to 10^3 runes, with 2×10^3 as the comfort point.
Issue #2 came from a client at tens of thousands of runes in one mantle, and
issue #1 from the same client: opening froze the window. The report proposed
fixes. The rule of this project is to measure before adopting anyone's fix, our
own included, so we did:
- synthetic state documents of 5×10^3 to 10^5 runes, built without any client's
  data;
- a 208 MB bundle (10^5 runes and 400 pictures);
- `tests/scale_bench.cpp`, which times one edit phase by phase;
- `tests/miga_bench.cpp`, which times open and pack;
- the phone harness's new `perf N`, which reads frame times in the running app.

Fitting log t against log N tells a cost's ORDER. The ratio between the
largest and the smallest size is enough when the sizes double: a ratio of 2 per
doubling is linear, 4 is quadratic.

# What one edit cost at 10^5, before

| phase | 10^4 | 10^5 | order | whose |
|---|---|---|---|---|
| Void Core `set` (the undo memento) | 45 ms | 476 ms | linear, 4.8 µs/rune | Void Core |
| export the whole state | 20 ms | 195 ms | linear | Void Core (no single-mantle export) |
| `project_scene` of the active mantle | 101 ms | 1,150 ms | linear | Void Maiz |
| `kinds::apply`, inside every reprojection | | a second full projection | linear | ours |
| two visible sections, every frame | | two reprojections a frame, GBs a minute | linear per frame | ours |
| edges, per projection | | O(E·N): 10^10 with a value edge per rune | quadratic | Void Maiz |

Nielsen's limits (0.1 s feels instant, 1 s keeps the flow, 10 s loses the
person) put the budget for an edit at 10^5 runes under 1 µs per rune. No O(N)
term at these constants fits, so the end state is incremental: a change feed
from the core, and a projection that re-projects what changed. Until then, the
work was to remove everything that was worse than linear, and everything that
ran when nothing had changed.

# What changed

**The bundle** (`platform/miga.cpp`): the old code built a JSON tree of the
whole file, dumped the state back to text, and rewrote every picture. Now:
- the file is read once and walked by a tokenizer that records byte ranges;
- the state goes to Void Core as the text it already is;
- pictures are decoded in parallel and written only when they differ;
- pack streams.

| on a 208 MB bundle | before | after |
|---|---|---|
| open into an empty folder | 3,067 ms | 603 ms |
| reopen, pictures already there | 3,492 ms | 734 ms, 0 written |
| pack | 6,183 ms | 1,711 ms |

**The window stays alive** (`app/busy.*`): open and save run on a worker, and
the core is built on the worker too. Measured in the running app during a 10^5
open: median frame 6 ms, one 290–430 ms frame for the main thread's half.

**Progress that means something.** Myers (1985) found people prefer a
percent-done indicator. Harrison et al. (2007) found they judge a bar by its
steadiness more than by its end. So the bar's fraction is
F = Σ w_phase × bytes_done / Σ w_phase × bytes_total, with each phase's weight
being its measured cost per byte, which makes F track elapsed time. It never
moves backwards. The phase is written in words. The time estimate is
(1 − F) / r, where r is the rate smoothed exponentially in time
(τ = 1.5 s, so the estimate does not depend on the frame rate). It is shown only
after 2 s and 10% of the work, and rounded to 5 s steps, because a wrong
estimate is worse than none.

**The Data tab** (`ui/data.cpp`): the filtered rows and the kind counts are
cached on (model generation, kind, search, filter, the scene's identity). The
list and the cards draw through ImGuiListClipper. Rows are one fixed height,
which is what makes clipping exact, and which a mail client and a file browser
use for the same reason: a steady rhythm scans faster.

**The projection** (Void Maiz):
- edges resolve their endpoints through one name map, O(N+E) where it was O(E·N);
- layout runs over the strongly connected components in topological order,
  O(N+E) on any graph (the first version kept a relaxation for cycles, and it
  took 330 s on a 5×10^4 trial: see below);
- one mantle is sliced out of the export by a scanner, so the 10-rune `kinds`
  mantle no longer costs a 43 MB parse;
- `Scene::find` is indexed.

**The ping-pong** (`app/app.cpp`): a scene is kept per mantle until something
changes. A `use` swaps one in, and only a real change projects. The map's
channels are read only from the data mantle, so switching to a document no
longer re-registers glyphs.

# What is left, and whose it is

At 10^5, an edit still costs about 0.5 s in Void Core (the undo memento) and
about 1.1 s to export and project the active mantle. Both are linear, and both
need the core:
- reified commands with inverses, which Void Core's own command-architecture
  page plans;
- a single-mantle export, or a change feed.

The measurements and the asks are in
`MESSAGE_FOR_VOIDCORE_hormiga-what-one-edit-costs-at-ten-to-the-fifth-2026-10-25.md`.
Replaying a script is superquadratic inside a batch; that is in the message too.

The issue's first point, numbers as edges to measure runes, is answered in
[measures](/concepts/foundation/measures.md).

# At 5×10^5: what was left on the main thread (2026-10-25)

The author asked for a database five times larger, built from real open data,
to stand in for a weak device. The tool is `tools/datasets/stressgen`. It uses
2.7×10^5 organizations from the IRS exempt-organization file, plus synthetic
contacts and events: 5×10^5 runes and 8.4×10^5 links in all, a 296 MB bundle.

We opened it in the real app, under the phone harness's script, and sampled the
main thread's stack every few seconds. Each round fixed whatever held the thread
longest, then ran again. At first the window never drew a frame after the bar,
and memory climbed past 18 GB. In order of discovery:

| what held the thread | why | now |
|---|---|---|
| layout of a cyclic graph | a relaxation of N passes per cycle | strongly connected components, O(N+E) (Void Maiz) |
| six whole-state exports per open | every projection exported 300 MB again | one export per change, shared (Void Maiz `Core::revision`) |
| a read moved the revision | `config get`, asked every frame by the map | reads are reads (Core's query family) |
| re-registering the same glyphs | every refresh counted as a change | a no-op (Void Maiz) |
| the save effect | a projection of every mantle and millions of SQLite rows, inside the dispatch | written on a thread of its own, coalesced (`app/save_writer`); statements prepared once |
| a re-projection after every command | including a save, and a change to the Antfarm's mantle | none after a save; kept when the mantle's digest is unchanged (`mantle_digest`) |
| Allomone with no scripts | built a frame of every subject anyway | skipped until a script exists |
| every Antfarm context | projected the data mantle for itself | lazily, once per change, from the scene the window holds (`held_data`) |
| the Antfarm pipeline | every stage copied every rune | a rune is an index into a shared scene |
| one filter over the data | each rune's date read every wire: O(N·E) | `Scene::wires_at`, and no date work for a query without `date:` |
| the calendar grid | 42 days a frame, each a scan of every rune | dated runes indexed once per change |
| the tag picker | the whole vocabulary gathered every frame | gathered when typed into, kept per change |
| the image-host panel | a projection every 3 s to count pictures | from the held scene |

**Where that leaves the stress database**, on the laptop it is built on:
- The open completes, and the window is usable after about 280 s.
- Frames then take a median of 11 ms (p95 12.5 ms).
- Memory is about 18 GB.

Most of the 280 s is a one-time cost of a database that has never been opened in
Hormiga before:
- Its Antfarm is created (about 75 s);
- its chambers are reconciled (about 105 s, the projection and then the
  commands);
- its first evaluation runs.

Each of those is a sequence of commands, and at this size each command costs
Void Core's undo copy (about 2.4 s) plus a projection.

**What is left, and whose:**
- **Void Core:** the undo memento; the `_baseline` copy, which doubles the
  state in memory and on disk (Q130); a `validate` that is quadratic; and no way
  to tell a host that a command changed nothing. All of it is in the message,
  section 5.
- **Ours:**
  - the first open's command sequences should be one batch each;
  - quitting waits for the working copy's write, which takes minutes at this
    size;
  - memory holds the core, the scene and the shared copy.

  The structural answer to all three is not to hold the whole database
  ([clusters](/concepts/platform/clusters.md)).

# How to re-measure

    cmake --build build --target hormiga_scale_bench hormiga_miga_bench
    hormiga_scale_bench a.state.json b.state.json ...
    hormiga_miga_bench big.miga
    voidhormiga --script s.txt db.state.json    # with `perf 120` in s.txt
    hormiga_stressgen --raw <downloads> --out <dir> --validate   # tools/datasets/README.md
