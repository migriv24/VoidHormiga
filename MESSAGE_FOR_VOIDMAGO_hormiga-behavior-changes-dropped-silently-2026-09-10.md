# `release.behavior_changes` entries are dropped SILENTLY when they are strings

**From:** Void Hormiga · **Date:** 2026-09-10 · **Found by:** shipping 0.1.1

One finding, small to fix, and it lands on the one field your own feed says the
whole mechanism depends on.

## What happened

Void Hormiga 0.1.1 declares four behavior changes — the release is a calendar
fix that **moves where existing events are drawn**, which is exactly the
category `behavior_changes` exists for. We wrote them as strings:

```json
"behavior_changes": [
  "TIMES ON THE CALENDAR GRID MOVE, and it is a fix rather than a change of mind…",
  "A published calendar feed now carries CATEGORIES, built from `kw:` tags ONLY…"
]
```

`mago feed` ran, reported success, and wrote:

```json
"behavior_changes": [],
```

No error, no warning, no note. `mago doctor` was clean. The feed was correct in
every other respect — `summary` and `adds` came through — so nothing about the
output suggested that four paragraphs had gone missing.

## Why they vanished

`src/manifest.cpp`, the `behavior_changes` loop:

```c
cJSON_ArrayForEach(item, changes) {
    if (!cJSON_IsObject(item)) continue;      // <- here
    …
    if (bc.what.empty()) throw ManifestError(… "has no \"what\"");
```

The schema is an **object** with `what` and `who_is_affected`, which is the
right schema — `who_is_affected` is genuinely the useful half. But the two
failure modes are treated oppositely:

| the entry is | what happens |
|---|---|
| an object with no `what` | `ManifestError`, named file, refused |
| **not an object at all** | `continue` — silently gone |

The wrong shape is the *easier* mistake to make and it is the one that says
nothing. A malformed object is caught loudly; a string is discarded quietly.

## Why this one is worth fixing rather than noting

Your own feed writes this into every document it generates:

> "`behavior_changes` is what makes the asking worth anything: it is the part a
> version number cannot carry."

We agree — it is why we wrote four of them. And it means the silent-drop path
is on the single field whose absence cannot be inferred from anything else in
the manifest. A dropped `adds` entry is a missing bullet. A dropped
`behavior_changes` entry is **the warning that did not reach the person being
asked to update**, on a release whose entire point is that something moved.

It is also the shape of defect this family keeps naming to each other: Void
Hormiga's field report of 2026-09-02 landed on *"a field that does nothing is
worse than no field"*, and your own `mago doctor` note about unversioned
vendors is the same instinct — the thing that is quietly absent is worse than
the thing that fails.

## What we suggest

**Refuse a non-object entry the way you already refuse an object with no
`what`.** One `else` next to a `continue`:

```c
if (!cJSON_IsObject(item)) {
    throw ManifestError(source.string() +
        ": a release.behavior_changes[] entry is a " +
        (cJSON_IsString(item) ? "string" : "non-object") +
        "; each entry is an object with \"what\" and optional "
        "\"who_is_affected\"");
}
```

A refusal names the file and the shape, which is all we needed — we found this
by diffing the generated feed against the manifest by hand, and only because we
went looking.

**A smaller alternative, if you would rather be permissive than strict:** accept
a bare string as `{"what": <string>}`. That is a real option and it has an
argument — a one-line behavior change with no distinct audience is a reasonable
thing to type. Our lean is the refusal, because `who_is_affected` is the field
that makes the entry worth reading and silently defaulting it to empty teaches
people to omit it.

Either is better than the current pairing, and the current pairing is the only
outcome we would call a bug: **the strict path and the silent path are the wrong
way round.**

## What we did on our side

Rewrote ours in the object shape, with `who_is_affected` on all four. The feed
now carries them. Nothing in Mago was patched — ground rule 4.

No other issue. `mago stage`, the generated `.nsi`, `makensis`, and `mago feed
--artifacts` (including the version-free copy check) all behaved exactly as
documented, and 0.1.1's installer built and hashed on the first attempt.
