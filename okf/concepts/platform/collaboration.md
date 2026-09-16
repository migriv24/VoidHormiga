---
type: Concept
title: Collaboration — one database, several devices
description: "How two devices holding the same .miga converge: the merge (Void Palabra's, built and correct), the transport (ours for now, LAN first, Palabra's later), and discovery. Why the merge is built before the network, why the LAN node is the default rather than the fallback, and the honest statement of which half exists."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-27T00:00:00Z
---

Opened 2026-08-27, on the author's direction:

> the main thing is i want to be able to work on an hormiga database from
> multiple devices … otherwise, we should have backup LAN based nodes in place
> for user sync and such … remember to utilize palabra

This page is the *how*. Who is making the change is
[identity](/concepts/platform/identity.md); which body of data may go where is
[data planes](/concepts/platform/data-planes.md), and nothing here is allowed
to move that line.

# 1. The problem splits in three, and only one of them is hard

The instinct is to build "sync" as one feature. It is three, they have
different owners and very different risk, and conflating them is how a project
spends a month on sockets and discovers its merge duplicates data.

| rung | the question | who owns it | status |
|---|---|---|---|
| **merge** | given two states, what is the correct single state? | **Void Palabra** | **built, and correct** |
| **transport** | how do the bytes get from A to B, privately? | Palabra eventually; **ours in the meantime** | ours, LAN |
| **discovery** | who is out there to sync with? | ours | ours, LAN |

**The merge is the one that can be silently wrong**, and it is the one already
solved. The other two fail loudly: a socket that does not connect connects to
nobody, and you find out immediately. So the build order is merge first, and it
needs no network at all — which is exactly the phase F that
[data planes](/concepts/platform/data-planes.md) §7 argued for and the proposal
that opened this page had left out.

# 2. The merge is Palabra's, and this is what we actually call

Measured 2026-08-27 against `../VoidPalabra`: `canonical-form`, `join`,
`persistence`, `archive`, `utterance` and `history-graph` are all
`status:current`. `reconciliation` and `peer-and-tier` — the network — are
still `status:planned`. That split is the whole of §1's table, stated by the
library itself.

The API, and the one structural fact a caller must understand:

```cpp
Doc a = enrich(state_a, mint);        // Core state -> CRDT-enriched
Doc b = enrich(state_b, mint);
Doc m = join(a, b);                   // commutative, associative, idempotent
for (auto& c : conflicts(m)) { … }    // a conflict is a VALUE, not an error
cJSON* merged = flatten(m).root;      // back to a Core state document
```

**Why `enrich` exists, and why it is not overhead.** A join over bare Void Core
state is impossible: two peers holding `{a}` and `{}` cannot distinguish "I
never had a" from "I removed a", so any join of plain state is union and
**removes never propagate**. Deleting a contact on the laptop and syncing would
silently bring it back. The per-element tags Palabra attaches are what buy the
difference, and they are the reason the merge is a library rather than fifty
lines in this repo.

**Three things do not merge, deliberately.** Palabra's canonical slice is
`mantles` and nothing else: `domains`, `bindings` and `config` are *peer-local
resolution*. A domain carries real deploy commands, so syncing one would run
one device's deploy on another — which is the correct paranoia and it means
each device keeps its own Antfarm wiring while sharing all of the org's data.
The consequence for us is concrete: **syncing does not sync your AWS
credentials, and it cannot.**

**A conflict surfaces; it never resolves itself.** `flatten` with the default
policy picks the lowest-ordered value, so calling it without checking
`conflicts()` is the one way to lose data here — the header says so, and our
seam therefore reports the count and refuses to write silently over a
divergence.

# 3. The transport is ours, and the seam is drawn so Palabra can take it

Palabra's second pillar is *Speaking* — "device-to-device communication, LAN
first" — and it is not built. We need multi-device now. The repo has been here
before and the answer is written down in
[miga-format](/concepts/platform/miga-format.md): `src/platform/miga.*` is a
**swappable implementation, not an interface other code binds to**, so when
Palabra lands, four method bodies change and nothing else moves.

The same discipline applies here, and it is the whole design constraint on
`src/sync/`:

- **`sync/merge.*` is a thin adapter over Palabra.** It converts, calls, and
  reports. It contains no merge logic, because merge logic here would be a
  second implementation of the thing we adopted a library for.
- **`sync/peer.*` is the part we own for now**, and it is deliberately dumb:
  find a peer, prove it is the peer, move an opaque blob. It knows nothing
  about runes, and that ignorance is what makes it replaceable.

**Ground rule 4 applies to the gap, not to the code.** We are not patching
Palabra; we are building the LAN transport in our own tree and reporting what
we learn so their Phase 4 arrives informed. That is the same play as the
`archive` round-trip bug and the Core §6.1 injection: build, measure, send
evidence.

# 4. The LAN node, and why it is the default rather than a backup

The author called LAN the backup for AWS. **The OKF's own commitments invert
that**, and it is worth stating because it changes what gets built first:
local-first is the resting state, the `.miga` is the authority, and
[data planes](/concepts/platform/data-planes.md) §1 requires every plane to
have a local origin or destination. Two laptops on one Wi-Fi is the
*primary* collaboration story; S3 is what you fall back to when the two people
are not in the same room.

Both get built. The ordering is the only thing that changes, and LAN is cheaper
anyway — it needs no account, no credential, and no vendor.

## Discovery: a UDP beacon, hand-rolled, and the reasoning

mDNS is the obvious choice and we are not using it. The job is "find another
Hormiga on this subnet"; both ends are ours, there is no Bonjour browser to
interoperate with, and vendoring an mDNS implementation buys a protocol we do
not need. Ground rule 5 is *vendor, don't depend* — the cheapest way to obey it
is to not need the dependency.

So: a multicast beacon on a fixed port carrying a small JSON payload — the
application, the database's name, its **version name** (Palabra's `v:…`, so a
peer can tell at a glance whether there is anything to do), the TCP port, and
the profile's public-key fingerprint. Announce on an interval, listen always.

**Hand-rolling the beacon is fine; hand-rolling the crypto is not** — that
distinction is ground rule 6 and it is not a matter of taste. The beacon is a
datagram format. Everything below is libsodium.

If interop ever matters — a phone, a third-party tool — `mjansson/mdns` is a
single-header public-domain implementation and this is a fifty-line swap. It is
recorded here so the decision is revisitable rather than forgotten.

## The handshake, and what the short code is actually for

[security](/concepts/platform/security.md) §4 already specified this: "pairing
via QR / short authentication string (Noise-style handshake)". Concretely,
X25519 key agreement (`crypto_kx`), then a **short authentication string**
derived from *both* public keys and read aloud by the two humans.

The proposal that opened this page had the short code as something device A
generates and device B types — which is a **pairing code**, and it is a
different thing that provides no protection against the attack it names. A
value that travels over the channel being attacked cannot authenticate that
channel. The SAS works precisely because it is computed independently on both
ends from material that was exchanged, and compared **out of band** — two people
in the same room, reading six characters at each other. That is what makes a
machine-in-the-middle visible: an attacker who substituted keys produces two
different strings and cannot make them match.

Since LAN sync means the two people are on the same network, "read this aloud"
is a realistic instruction rather than a security theatre. Trust, once
established, is remembered: the peer's public key is stored, and a changed key
on a known peer is a **loud** refusal rather than a re-prompt.

## Transport: TCP, sealed with the session key

`crypto_secretstream` over a TCP socket — the same primitive
`src/platform/backup.cpp` already uses, chosen there because it detects
truncation, which is exactly the property a network transfer wants for the same
reason a file does. Winsock is already linked for the preview server.

**Private keys are never transmitted.** Public keys and sealed payloads only.

# 5. What actually moves between two peers

Not the `.miga`. Sending a whole bundle is "full sync" and it is the wrong
default for the same reason it was the wrong default for publishing: it costs a
render and a redeploy to change a phone number.

The unit is the **state document** — `mantles` and nothing else, which is
Palabra's versioned slice — plus, later, the assets the receiving side does not
have, addressed by content hash so they dedupe. The exchange:

1. Both sides say their `version_name`. **Equal → done**, one round trip, no
   data. Two devices that are already in sync discover it in a message.
2. Different → exchange the enriched documents, `join`, report conflicts.
3. Each side writes the merged state through **its own dispatcher**, so the
   change is logged, replayable and undoable like every other change. Ground
   rule 3 has no exception for the network.

Step 3 is the one that is easy to get wrong and expensive to fix later. A sync
that writes into the store directly is a second door, and every property this
project rests on is a property of going through the first one.

# 6. The S3 rendezvous, for people who are not in the same room

[data planes](/concepts/platform/data-planes.md) §3 already settled the shape
and it needs nothing new: an **encrypted backup** — the whole `.miga` sealed
client-side, in a bucket that holds bytes it cannot read. That is end-to-end
encrypted by construction, works today (`effect backup-database` +
`effect push-store backup`), and is not a compromise so much as the same
guarantee with a slower transport.

What it is **not** is a queryable cloud database of the organization's data.
The distinction is §3 of that page and it is the breach this whole folder
exists to prevent.

The field agent owns standing this up — real credentials, a real bucket, the
first call — per the pattern that has now paid for itself four times: the
developer builds and proves the local half, the operator drives the first
remote call and reports the vendor's own words verbatim.

# 7. What is built, and what is not

Written this way deliberately, because "sync works" is a claim that can be true
of a demo and false of a database. Everything marked **verified** below was run
against two real databases on 2026-08-27, not asserted.

| | state |
|---|---|
| Palabra linked; `version_name` over our real state | **verified** — two copies of a database name the same cut; one edited field moves it; rune ORDER does not |
| two-database merge, `effect sync-merge` | **verified** — see §7.1 |
| conflicts surfaced with both sides and an address | **verified** |
| LAN discovery beacon | **verified** — two instances found each other on the real interface and each read the other's cut name |
| SAS handshake + sealed stream | **verified** — both ends independently derived the same six characters over a real socket |
| trust on first use, then pinned | **verified** — a second sync with the same device says "known device" and does not re-prompt |
| `hol_lan_peer` + the `peer` rune | built |
| two separate MACHINES on one Wi-Fi | **not done** — needs a second machine. Everything up to the socket is proven; the socket has only been proven on loopback and on one host's own interface |
| signed utterances (attribution as proof) | not started; wants Palabra Phase 4 |
| asset sync by content hash | not started — only the state document moves today |
| conflict resolution UI | not started — conflicts are reported, not yet edited |
| S3 rendezvous | the pieces exist; awaiting the field agent's credential |

## 7.1 The two-database run, and what it proved

Two folders, one machine, one database copied between them, then diverged: A
edited a contact's phone and **deleted** a second contact; B edited the *same*
phone differently and **added** a third contact. Then merged, both directions.

- The conflict on the shared field was reported with both values and a content
  address, and nothing was chosen silently.
- **Both folders ended at the same cut name**, from both merge directions. That
  is the convergence property, measured rather than assumed, and it is the
  phase F question [data planes](/concepts/platform/data-planes.md) §7 asked.
- A repeat merge reported "already in sync" and wrote nothing.
- The deleted contact came back, was **named** in the report as an arrival, and
  the warning said what it might mean. §2's limitation, visible rather than
  silent.
- `config` did not cross: each side kept its own `site.base_url`.

## 7.2 Three bugs this run found that the unit tests did not

Kept because each is a class of mistake rather than a typo, and two of them are
the kind that report success.

**1. The resurrection warning was on the wrong side.** The report warned about
runes that exist only *here*, which is the harmless direction. The dangerous one
is a rune that exists only on the *peer* — because that is either their addition
or **our deletion coming back**. The unit test asserted the data structure and
passed; the first real run put "2 runes arrive from the file" in a cheerful
info line while quietly undoing a delete. **A test that checks the struct does
not check the sentence**, and the sentence is what a volunteer acts on.

**2. Applying a merged document inside the effect handler is a use-after-free.**
The obvious implementation calls `reload_from_state`, which assigns to `core` —
and the effect handler is a `std::function` **owned by** `core`. So the merge
freed the callable that was executing. Headless it presented perfectly: the
process died mid-effect, before the session could save, leaving a stale lock and
a database that looked untouched behind a merge that had said *applied*. The
merged document is now carried out of the effect and applied where nothing is
mid-callback — the GUI at the top of the next frame, the CLI after the session
has closed and released its lock. The same latent bug was in the GUI path and
was fixed with it.

**3. Trust on first use is worthless if every use is the first.** The peer
record was dispatched onto the pre-merge core, and the merged document then
replaced it — so the pairing code was re-prompted on every sync, which is
precisely how a person is trained to stop reading it. The peer commands are now
replayed onto the *merged* document. (And a `use antfarm` against a database
that has no Antfarm mantle silently leaves the active mantle alone, which put a
device fingerprint in the contact list on a bare fixture. Guarded.)

## 7.3 A merge preview with detections, in the GUI (2026-09-13)

`effect sync-merge <path>` has always reported before it writes. Niche Tools >
Merge puts a structured version of that report in front of a person, for the
case that prompted it: an organization's edits went into a stray copy of its
database in the source folder and needed folding back.

It calls `merge_states` with the SAME prefixes `sync_op` uses, so the version it
predicts is the version Apply produces; Apply goes through `gui_sync_effect`, the
console's own path, so there is still one merge in the application. Beyond the
report it detects:

- **the same database, or a different one** -- the share of runes both copies
  hold. A diverged copy shares most; an unrelated database shares almost none,
  and merging it would combine two organizations' data, which is almost never
  meant and never obvious afterwards.
- **files only in the other folder** -- data-folder paths the other copy
  references that exist beside it but not here, with a button to copy them.
- **Antfarm paths into the other folder** -- absolute `file`, `dir`, `dump_dir`,
  `key_file`, `secret_file` and `token_file` values under the other copy's
  folder, with a button that rewrites them relative to this one as ONE batch of
  logged `set` commands.
- **the result** -- after Apply, "Check the result" compares the open database's
  version name with the one the preview predicted.

Not yet: a detailed preview of a `.miga` bundle (the console's report covers
one). And none of it has run on real data: it compiles, the merge underneath is
the tested one, but the detections have only ever existed in a window nobody has
opened.

# Boundaries

- **We do not implement merge.** If a merge question arises, it is a message to
  Palabra, not a function here.
- **Sync writes through the dispatcher.** No exceptions for the network.
- **`config`, `domains` and `bindings` do not sync** — each device keeps its own
  backends, and credentials therefore cannot travel by accident. **Joining** a
  database is a different act and carries keys on purpose, after a person allows
  it: [LAN sharing](/concepts/platform/lan-sharing.md) §0.
- **Runes carrying a private tag are withheld from the exchange** (2026-09-16,
  [LAN sharing](/concepts/platform/lan-sharing.md) §7).
- **A conflict is reported, never resolved silently.** `flatten` without
  checking `conflicts()` is the one way to lose data here.
- **The peer layer stays ignorant of runes**, so Palabra's Phase 4 can replace
  it without touching anything else.
