---
type: Discussion
title: The database that would not open
description: Why the 5×10^5-rune stress database still exhausted memory in 0.2.4, nine days after it was first measured at 18 GB and one day after the store opened it at 290 MB. Hypotheses, some of which may be wrong.
tags: [status:current, audience:all, confidence:speculative]
timestamp: 2026-11-04T00:00:00Z
---

# The database that would not open (2026-11-03)

## What happened

- **2026-10-25.** The stress database opened in the window at about 18 GB,
  after a dozen rounds of fixes ([log](/log.md), "5×10^5 runes from public
  data").
- **2026-10-30.** The first step of the app's move to the store was committed
  half-done, and did not compile. It was reverted on 11-02, and stage 5 started
  again on Void Rancho.
- **2026-11-02.** Rancho's working set opened the same database at 290 MB under
  a 512 MB cap, in a harness. 0.2.4 shipped as *"the crash fix"*. Its log entry
  said: *"Not done: ... the crash fix seen in a window on the database that
  crashed."*
- **2026-11-03.** The author opened the stress database in 0.2.4, and it used
  memory until the machine gave out. The app's open had never touched the store.
  Stage 5 was built that day: 531 MB under a 1 GB cap
  ([store](/concepts/platform/store.md)).

## Hypotheses

**1. We tested the library's door, not the person's.** The 290 MB was true,
but it answered *can Rancho open this?* The author's question was *does
Hormiga?*. The harness called Rancho directly, and the window's File > Open
path stayed the whole-document one.
*What changed:* `store-check` runs the app's own open path, with no window,
under a memory cap (`hormiga_store_check`).
*Untested idea:* a release step that opens the largest fixture through the
shipping binary.

**2. A "Not done" line reads as bookkeeping, not as a stop.** The one check
that would have caught this was written down, and we released anyway. Our "Not
done" lines are scattered through log entries, and no list gathers them, so
each session meets them one entry at a time.
*Possible improvement:* known failures kept in one place, perhaps the index's
Status, and read before a release.
*Could be wrong:* a list like that may just become a second place nobody reads.

**3. One word covered two failures.** 0.2.4 fixed a real crash, a
use-after-free in the hosting panel. The author's "crash" was running out of
memory. The release's name invited the author, and us, to believe both were
fixed.
*Hypothesis:* name a fix after its cause, not after its symptom.

**4. Building bottom-up hid the top for nine days.** The store's stages went
library first (Rancho, then the working set), and the app came at stage 5.
Every stage passed its exit test, and the path a person uses stayed the old one
across two releases.
*The alternative:* a crude end-to-end path first, opening into the store and
holding everything even if slowly, so the old path dies early.
*Against it:* the store's API moved to Rancho mid-way (10-30), and an early
slice might have been written twice. Which costs more is not known.

**5. The founding scale was never written down as a limit.** The `.miga` v3
format is *"the whole database as one document"*, and the first outreach
organizations had thousands of runes. Every layer was sized to one document:
Core's state text, its undo copies, `_baseline`, the projections. That was
right at the time. It became wrong when the data outgrew it, and nothing said
where the assumption lived.
*Hypothesis:* a design written without its limit reads as unlimited. Writing
"valid to about 10^4 runes" would have made the edge searchable. Since 11-03
the premise is the opposite: assume infinite ([frontier](/concepts/platform/frontier.md)).

**6. The guard we proposed.** The agent proposed refusing an open the device
could not afford (Q132, until stage 5), and the author refused: *"a flag or
error would diminish the issue and make it seem like something that can be
worked around."* The guard was tempting because it turned an engineering gap
into a policy that could be closed the same day. It would also have made the
failure look intended.
*Hypothesis:* when a guard, a flag or a warning is proposed, first ask which
missing mechanism it stands in for, and build that instead.

**7. The same mistake, smaller, inside the fix.** The first stage-5 build spent
3.3 s per edit. Core's `save` moved the revision, so every write-back fell to
the full comparison. Core and Rancho were each correct, and their combination
was not. Timing showed it was slow. Only Rancho's counters, which record
targeted against full syncs, showed why.
*Hypothesis:* integrations fail at the seam between two correct parts, and the
cheapest defence is a counter that says which path ran, not a timer.

## Still unknown

- **The fix may repeat hypothesis 1, one level up.** The stress database
  clusters very well (modularity 0.977), and the plan profits from that. A
  densely interwoven database has not been opened through the app. If it
  behaves badly, today's measurement was taken on a friendly fixture.
- **Whether hypotheses 2 and 4 are causes or coincidences.** Two releases in
  A crash, a library adoption, two CI failures and a release all landed on
  2026-11-02. That crowding may explain more than either hypothesis.
