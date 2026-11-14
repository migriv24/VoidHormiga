---
type: Discussion
title: The close that woke nothing
description: The LAN beacon's stop() relied on closing a socket to wake a thread blocked in recvfrom, which Windows does and Linux does not. It shipped from 0.1 and hung CI's Linux leg on 0.2.6 with no test named. Hypotheses.
tags: [status:current, audience:all, confidence:speculative]
timestamp: 2026-11-13T00:00:00Z
---

# The close that woke nothing (2026-11-13)

## What happened

- **From 0.1** the beacon's receiving thread blocked in `recvfrom`, and
  `Beacon::stop()` closed the socket from another thread, commented *"unblocks
  recvfrom"*. On Windows it does. On Linux, and so on Android, a close does not
  wake a thread already blocked on the descriptor.
- **It passed CI's Linux leg until 0.2.6**, because something always sent one
  more datagram: a beacon hears its own multicast, and the other beacon was
  still announcing.
- **On 0.2.6's runs** `hormiga_sync_smoke` sat for over 30 minutes, twice. ctest
  has no default limit, and the test's buffered output died with it, so the
  log named nothing. A 600 s limit on every test named it. The fix is in the
  [log](/log.md) ("0.2.6").

## Hypotheses

**1. The comment was the test.** *"unblocks recvfrom"* states a platform
behavior as a fact, and it was true where it was written. Nothing ran it where
it was false. *Wrong if:* the Linux leg had a check of stop() before 0.2.6 that
we missed.

**2. A thread that waits forever needs a reason to stop that it checks
itself.** Here it depended on another thread's action to be woken. The
session's waits in the same file have their own limits (`select` in
`accept_one`, `SO_RCVTIMEO` through `set_timeout_ms`). *Wrong if:* a wait
elsewhere, written the same way, has never misbehaved; then the lesson is
narrower than this.

**3. Green on a network test is partly the runner's.** Whether a datagram
arrives depends on what the machine delivers to itself. The code did not change
between 0.2.5's green run and 0.2.6's hung one; we believe the runner did, and
cannot show it.

**4. A hang costs more than a failure.** A failure names itself in seconds. This
hang cost an hour of CI and named nothing. Bounding every test was cheaper than
any single fix.

## What changed

- Every test has a 600 s limit (`CMakeLists.txt`, `hormiga_test_timeout`).
- The beacon's receiving thread waits at most 200 ms, then checks `stop`.
- `sync_smoke` writes unbuffered.

## Still unknown

- Whether a phone ever stalled turning discovery off. Nobody reported it, and
  nothing would have shown it but a frozen screen.
- What on the runner changed.
