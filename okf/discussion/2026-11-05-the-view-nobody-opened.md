---
type: Discussion
title: The view nobody opened
description: 0.2.5 shipped saying the stress database opens, and the first view the author opened on it froze. Why the check stopped at the open, and a mistake of our own while measuring the fix. Hypotheses.
tags: [status:current, audience:all, confidence:speculative]
timestamp: 2026-11-05T00:00:00Z
---

# The view nobody opened (2026-11-05)

Answers in part [the database that would not open](/discussion/2026-11-03-the-database-that-would-not-open.md).

## What happened

- **2026-11-04.** 0.2.5 shipped: *"any database opens, however large"*. Its
  check (`store-check`) opened, edited, saved, restarted and passed. The window
  was looked at once, on the Data tab's list.
- **2026-11-05.** The author opened the connections view on the stress
  database, and it froze. Its layout compared every loaded rune with every
  other, and reset itself every frame on a partial load
  ([connections](/concepts/sections/connections.md)). The *Other parts...*
  button in the same release froze the window for 12 s.

## Hypotheses

**1. "Opens" was tested; "is usable" was not.** The last entry's hypothesis 1
was that we tested the library's door, not the person's, and its fix was to
run the app's own open. That fix was right, and one level too low: a person
does not open a database, they open views on it. Every view that reads all of
what is loaded is the same bug waiting: the map, the calendar, Allomone, the
Antfarm.
*Untested idea:* a check that opens every section and view on the stress
database and measures a frame in each.

**2. The old view had a hidden assumption about completeness.** It reset
whenever its positions did not match the scene's runes. On a whole database
that never happens. On a partial one, every link to an unloaded rune made a
position for a rune it did not hold. It did not crash, so it slid past. A
partial load breaks any code that quietly assumes the scene is the whole
graph.
*Hypothesis:* the frontier's mojones need to be a type, not an accident of
which names happen to be missing.

**3. Our own mistake while fixing it: misreading the instrument.** I spent a
long stretch chasing a "two-minute boot" and a "three-minute quit". They were
the test harness: its `wait` counts frames, not milliseconds, and the
connections view's frames are 6 ms. The real costs were a 10–14 s boot and a
1.4 s quit. I assumed the unit from the number's size.
*Lesson, maybe:* time a harness against a wall clock once before trusting it.

## Still unknown

- **Whether the grouping reads well to a person.** The tree is bounded and
  tested, and the screenshots look right. Nobody but the author can say
  whether the groups mean anything to someone browsing.
- **Whether hypothesis 1 is in our habits or in our checks.** A check per
  view is cheap. Remembering that a view is a door is the part that keeps
  failing.
