---
type: Concept
title: Identity — who made this change
description: "The three different things a conventional design collapses into one users table: the admin profile (a key on a device), the signed-in identity (a provider's claim), and the contact (a rune, a person). What attribution means when there is no server, why an admin profile is a credential rather than a person, and why permissions stay tags."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-27T00:00:00Z
---

Opened 2026-08-27, out of the user-profiles proposal and the author's
constraint that follows from it:

> the main thing is i want to be able to work on an hormiga database from
> multiple devices

Multiple devices is [collaboration](/concepts/platform/collaboration.md)'s
subject. *Multiple people* is this one's, and the two are genuinely separate:
one person with a laptop and a desktop needs sync and no identity system at
all, and the design gets worse if the two problems are solved with one object.

# 1. Three things, and a conventional design has one

| | **admin profile** | **identity** | **contact** |
|---|---|---|---|
| what it is | a keypair with a name, held in a database | a claim a sign-in provider vouches for | a rune — a person, an org |
| where it lives | the `.miga`, per database | the provider (Cognito / Google), never here | the `.miga`, in a mantle |
| what it can do | dispatch commands | **propose** a transcript, nothing more | nothing; it is data |
| how many per person | one **per database, per device** | one per provider | exactly one, ideally |
| if it is lost | that device stops signing | sign in again | the person is gone from the database |
| is it a person? | **no** | **no** | **yes** |

The two `no`s are the load-bearing part. `src/domain/seed.hpp` already says it
about the second, and it was written before this page existed:

> `actor` is the identity the sign-in provider vouched for (`google:sub`),
> **NOT a person**. Which contact that identity is, is a link in this database
> and nowhere else — so the provider can be swapped and the relationships
> survive it.

The same reasoning applies one column to the left, and it is the correction the
proposal needed. A profile that *is* a person forces one row per human and then
cannot answer "Ada's laptop and Ada's phone" without either sharing a private
key between devices or inventing a device table under the person table. A
profile that is a **key on a device** answers it for free: two profiles, one
contact, and the link between them is an edge like every other relation in this
database.

**So the arrow points one way: profile → contact, many to one.** Never the
reverse, and never a `contacts.is_admin` column.

# 2. Attribution is a journal property, not a config one

Void Core already carries an actor: `config set actor human:hormiga`, set in
`src/app/app.cpp`, namespaced the way `provider:subject` is. A profile changes
the *value* of that string and nothing structural — `barriga:ada`-shaped, or
whatever [Q46](/developer_questions.md) settles the prefix to be.

Two consequences worth writing down before anyone builds on it:

**The attribution lands in the journal, not in `config`.** `config` is the
peer-local resolution tier — Void Palabra's canonical form deliberately
excludes it from the versioned slice, because a domain carries real deploy
commands and syncing one would run one device's deploy on another. Since
`config` is excluded, *changing the actor does not change the version name*,
which is correct: who you are is not part of what the database says. The record
of who did what is the command journal, which is where `vc_export_journal_output`
reads it from and where [collaboration](/concepts/platform/collaboration.md)'s
history graph gets it.

**Attribution is therefore a claim, not a proof, until signatures exist.** A
device that sets `config set actor barriga:ada` and dispatches is attributed to
Ada whether or not it is Ada's. That is fine and it should be said plainly
rather than dressed up: the `.miga` is the authority, everyone holding it is an
admin ([web platform](/concepts/platform/web-platform.md) §4), and attribution
today is *bookkeeping among people who already trust each other* — which is
what a two-volunteer outreach org actually is. Signed utterances are a real
upgrade and they belong to the same phase as the encrypted transport, not
before it.

# 3. What a profile stores, and how it is encrypted

Per database, in the state (a mantle), so it travels in the bundle and versions
through Palabra like everything else — **not** as a new top-level section of
the `.miga` envelope. That is the difference between a decision that costs
nothing and a format change:

```jsonc
{ "glyph": "profile",
  "name":        "ada-laptop",        // the device's name for itself
  "display":     "Ada",
  "public_key":  "<base64 X25519>",   // identity + key agreement, published
  "sealed_key":  "<base64>",          // the private key, sealed to a passphrase
  "settings":    "<base64>",          // this profile's own preferences, sealed
  "contact":     "rune_abc123",       // the edge to the person, may be empty
  "created":     "2026-08-27" }
```

**The sealing is the existing vault primitive, not a new mechanism.**
`src/platform/vault.cpp` already does argon2id → XChaCha20-Poly1305 with a
per-vault salt, verified against wrong-passphrase and byte-flip. A profile's
private key and settings are that primitive with a per-profile salt. The
proposal this page came from said "encrypted with the Barriga's private key",
which is not a thing a private key does — it signs and it agrees; a passphrase
KDF or a `crypto_box_seal` to the public key is what encrypts. Since the vault
is built and audited, the answer is to use it.

**A profile is `internal`-class and never publishes.** A rune carrying a public
key and two sealed blobs has no business in a directory, and the additive
clearance rule of [security](/concepts/platform/security.md) §3 already covers
it — `publish-index` only emits what carries `clearance:public`, and nothing
grants that to a profile. The thing to *not* do is add a glyph and rely on that
default: the seam should name the glyph, so the refusal is a decision rather
than an accident of tagging.

# 4. Permissions stay tags, and this page does not change that

[web platform](/concepts/platform/web-platform.md) §4 refused a role hierarchy
and it stands:

> an "admin" is not a row in a table with a boolean. It is a contact whose runes
> carry a tag, and the rules that grant power are readable, mergeable, and
> conflict-surfacing like everything else.

A profile is an *authentication* fact — which key signed this. Clearance is an
*authorization* fact and lives on the contact as `role:admin`,
`clearance:public`, `clearance:contact`, derived by Allomone and merged by a
lattice. Keeping them apart is what lets two admins disagree about someone's
clearance and get a **conflict** rather than whichever of them saved last.

**And today the honest answer is still the one already recorded:** anyone
holding the `.miga` is an admin. Profiles buy attribution and per-device keys.
They do not buy a permission system, and building one before a second person
needs it would be inventing the hierarchy the OKF has twice refused.

# 5. What this means for a visitor

Nothing changes. The [`submission`](/concepts/platform/data-planes.md) §4 flow
is built and this page does not touch it: a visitor signs in with a provider,
**asserts** who they are, and that becomes a proposed transcript which an admin
reviews in Hormiga against a local, ranked search. The one thing worth
restating, because the proposal reintroduced it: **no `kind` field.** What a
submission does is read off the commands it carries, never stored beside them
as a summary that could disagree with them.

The other thing the proposal added and this page declines: **no visitor IP is
stored.** In a database whose entire privacy argument is that the member list
must not be enumerable by strangers, for an organization serving people who may
be undocumented, "for audit purposes" is not sufficient reason to start keeping
network locations of people who filled in a form. If an abuse problem appears,
it gets solved with rate limiting at the edge, where the data can stay.

# Boundaries

- **A profile is a credential, not a person.** Many profiles, one contact.
- **Never a `contacts.is_admin` column**, and never a profile that owns a
  contact rather than pointing at one.
- **Crypto is the vault's**: argon2id + XChaCha20-Poly1305 from libsodium, with
  no second mechanism and no fallback secret.
- **Attribution is a claim until utterances are signed**, and the docs say so
  rather than implying more.
- **Profiles never publish.** The seam names the glyph.
