---
type: Concept
title: Security
description: "E2EE as a pillar: libsodium as the one crypto dependency, passphrase-sealed registry and at-rest encryption, privacy enforced at the render seam, three E2EE collaboration modes over the command log, signed releases."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

Author's directive at founding: **end-to-end encryption is a MAJOR aspect of
development, especially for collaboration.** An outreach org's database is
people — names, contacts, internal notes about them. The design consequences,
ordered by when they bite:

# 1. One crypto dependency: libsodium

Vendored, audited, misuse-resistant. No home-rolled primitives, no OpenSSL
sprawl. The suite: **argon2id** (passphrase KDF), **XChaCha20-Poly1305
secretstream** (files/blobs), **Ed25519** (signatures), **X25519** (key
agreement).

# 2. At rest (v1)

- The `.miga` v2 registry ([Antfarm](/concepts/platform/antfarm.md)) lives under a
  **passphrase**; credentials are encrypted. No hardcoded fallback secret
  exists, ever.
- The data store is encryptable at the app level — content columns sealed
  with a master key derived at unlock — with **"encrypt everything" as an
  org-level switch** (loudly offered at org creation; the perf cost is honest
  and measured). Internal-notes fields are encrypted regardless of the switch.
- OS keychain integration for convenience unlock is an open question, not an
  assumption ([developer questions](/developer_questions.md)).

**Built (2026-07-20): the credential vault** — `src/platform/vault.{hpp,cpp}` +
`tests/vault_smoke.cpp`, libsodium vendored from source. The `.miga` v2 file
is a small JSON envelope: `{magic:"MIGA", version:2, kdf:"argon2id", ops,
mem, salt, nonce, ct}` — everything to re-derive the key EXCEPT the
passphrase. **argon2id** (`crypto_pwhash`, interactive limits) derives the
key from passphrase + per-vault salt; **XChaCha20-Poly1305**
(`crypto_aead_xchacha20poly1305_ietf`) seals the secrets JSON with a fresh
nonce — authenticated, so a wrong passphrase or a tampered file fails cleanly
with no oracle (verified: wrong-pass and byte-flip both rejected). Plaintext
and derived key live only in memory while unlocked, `sodium_memzero`'d on
lock; save is write-temp-then-rename (a crash never corrupts the vault). The
secret never touches the command log or exported state.

**Opt-in, off by default** (this section's own rule): with no `org.miga`, the
plaintext `imgbb.key` (dev) is used; **File → Encrypt credentials** creates
the vault under a new passphrase and retires the plaintext; a vault's presence
prompts **Unlock** on boot (skippable → offline, ImgBB dormant). This is the
first slice — the vault currently holds the ImgBB key; collaboration keys
(§4–5) and the whole-store switch build on the same `crypto_pwhash` +
AEAD spine. Container-format growth (topology, templates, notes, media refs —
the "org bundle") is [Q17](/developer_questions.md).

# 3. Privacy is a property of the seam, not the app

The public-bio / internal-notes split is enforced where data leaves:
**internal notes are a field that no Output-interface holiday ever receives**
— checked at the render/export seam, testable, not a template convention. The
same mechanism later expresses "this data may not leave the device" for
location-grade PII. Corollary at the repo level: **the repo root is a public
artifact** — no member data, exports, or credentials beside the code, ever.

## The seam has two halves, and the second one is opt-IN (2026-08-19)

Everything above is **subtractive**: publishing is the default and `notes` is
carved out of it. That is the right shape for an event, a job posting or a
flier, all of which exist in order to be seen.

It is the wrong shape for a person. The `directory` block — the first block that
can publish a contact — is therefore **additive**: a rune reaches an output only
if it carries `clearance:public`, and its `email`/`phone` only with a separate
`clearance:contact`. No query, field or flag overrides either.

**Why the rule lives in the block and not in the query.** A query is authored to
select, and the natural query for a directory is `type:contact`. If selection
were sufficient, the first person to write the obvious thing would publish
84 people's phone numbers and be told it worked. A privacy rule that a correct-
looking query can step around is not a seam; it is a convention with a good
reputation.

**Why two tags rather than a level.** [web-platform](/concepts/platform/web-platform.md) §4:
clearance is an annotation, not a rank. "You may list me" and "you may print my
phone number" are independent grants, and a hierarchy would force one to imply
the other. Two tags also compose the way everything else in this system does —
mergeable, conflict-surfacing, readable off the rune.

**And the withheld count is part of the seam.** The render reports how many
runes it declined to publish. Silence would make a working privacy rule look
like a broken block, and the pressure to "fix" it by tagging everybody is
exactly the pressure the rule exists to resist.

# 4. Collaboration (later, designed-for now)

Different protocols for different work, all behind the Auth/Peer interface,
all E2EE. **The sync unit is the command log** — the thing the core already
gives us; log-shipping between peers (with materialized snapshots for
bootstrap) is the CRDT-adjacent starting point.

- **LAN mode** — mDNS discovery on shared Wi-Fi, pairing via QR/short
  authentication string (Noise-style handshake), encrypted sync between
  trusted devices. No server at all.
- **Login mode** — a shared relay for distributed teams that stores and
  forwards **ciphertext only**. The server is dumb by design: running one is
  not a position of power over the org's data.
- **Mesh mode** — the MeshDB path, when multi-editor demand is real.

Conflict UX is a declared hard problem — localized behind the seam, not
solved on paper here.

# 5. Trust topology

Lives in `.miga` v2: the admin builds the Antfarm and invites peers; members
hold **keys, not passwords-to-a-cloud**. Key loss = data loss is a real
trade; recovery codes at org creation are the mitigation lean.

# 6. Releases are signed

Ed25519 signatures on release artifacts; the self-updater verifies before it
swaps anything, keeps N-1 as rollback, and the update is a visible, logged
event like everything else. No silent updates.
