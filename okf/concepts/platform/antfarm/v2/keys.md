---
type: Concept
title: Antfarm v2 — keys
description: "Credentials as their own nodes, shared between the members of a database (the author, 2026-09-28): the value lives sealed in each member's vault and arrives only over a sealed session, the rune that describes it syncs like any other, and nobody sees it raw without asking. What a key declares (the provider, what it is compatible with, scopes, expiry, cost), the monitor nodes (usage from our own log, vendor usage where an API exists, budget, expiry), the rules that still hold, and the reversal of 'keys stay on their device'."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Design, not built.** See the [v2 index](/concepts/platform/antfarm/v2/index.md).

> **2026-10-01: Void Verguenza.** A sibling application for shared secrets was
> founded so that this node can eventually **name a Verguenza secret instead of
> holding a value** (the author: *"eventually the antfarm will reference
> Verguenza's structures to use instead of our current api key node"*). Sharing
> key values between members (phase V3 below) should wait for Verguenza's client
> seam. See [Void Verguenza](/concepts/projects/void-verguenza.md).

# 1. The author's position

2026-09-28, in full because it reverses two earlier decisions:

> API keys MUST be shared between devices. As long as they're on the same
> network and its a secure connection. […] maybe the node itself doesn't
> actually show the raw text of the api key. but it stores it the same across
> devices. Sure, an agent on the network could easily access the key and read
> it, but this is an issue of security for the networks in general.

> API keys are very useful to share across devices, because it can essentially
> allow for multiple other devices to not need to go through the hassle of
> creating an api key for some service. […] If someone has created a google maps
> key, that takes like a solid amount of time to create.

> credentials are shared! this might be considered a security risk, however,
> again, that's an issue for the security of hormiga in general. Sharing API
> Keys is fine. they should be attributed to device context and profiles in
> general.

## The reversal

| date | decision | status after 2026-09-28 |
|---|---|---|
| 2026-09-16 | the host's Antfarm wins on credentials; keys travel only at join, and are meant to go stale | **reversed** for sharing; "keys go stale" survives as expiry, now visible |
| 2026-09-25 | a node that names a credential file stays on its device (`collab::device_only`) | **reversed**: a key node syncs; its value travels sealed |

The reason the author gives is the one that matters to an organization: a key
is often hours of someone's work (a Google Cloud project, billing, an API
enabled, a key restricted), and making every member repeat it is how a
volunteer gives up.

# 2. What is stored where

| part | where | travels |
|---|---|---|
| **the key rune** (name, provider, what it serves, scopes, expiry, who added it, from which device, when, last success) | the `farm` mantle | syncs like any rune |
| **the value** | each member's own vault (Argon2id + XChaCha20-Poly1305, sealed with the profile's key) | only over a sealed Void Palabra session, directly into the receiving vault |

So a key is stored **the same on every device**, as the author asked, and it is
never written anywhere in plaintext. The rules that do not change:

- **Never a field.** The rune holds no secret, so `cat`, `get --json`, the state
  document, an unencrypted `pack-database` and sync all carry the description
  and never the value.
- **Never on the command line.** `farm key add` reads the value from a prompt or
  standard input, never from an argument, because arguments land in the
  dispatcher log and the shell's history.
- **No fallback secret.** A device whose vault is locked has the key rune and
  not the value. Its readiness is *needs: unlock the vault*.
- **Not shown raw.** The face shows presence, the last four characters and who
  added it. `farm key reveal` is an effect, gated like any other, and logged,
  so a reveal is on the record.
- **v1 key files are imported, then retired.** `token_file`, `key_file` and
  `secret_file` become key runes with vault values in the
  [migration](/concepts/platform/antfarm/v2/migration.md).

**Which members receive values.** **Lean:** every member whose role is `admin`,
and today everyone is an admin
([LAN sharing](/concepts/platform/lan-sharing.md) §4), so in practice every
member. The author placed the risk with the network ("that's for the
networking sector to solve"), and roles are the network's tool for it. When
roles are enforced, "viewers don't receive keys" becomes one line, not a
redesign. This is [Q93](/developer_questions.md).

# 3. What a key declares

The author:

> different API keys define themselves by the documents they are compatible
> with. For example, a cloudflare API key is compatible with a email and
> website document. a google maps key is compatible with a maps document. A
> weather api key might be compatible with a maps document or a calendar
> document.

A **provider** is a small declaration (not code) saying what a key from that
vendor can serve. The key node picks a provider, and inherits:

| provider | serves documents | serves on the canvas | where to create one |
|---|---|---|---|
| Cloudflare | website, newsletter (email sending) | web domains, mail domains, R2 reservoirs, DNS | the provider's link and the scopes to tick |
| GitHub | website | web domains (Pages) | |
| Google Maps | map | map tiles and geocoding sources | |
| a weather service | map, calendar | weather overlays (planned) | |
| ImgBB | any (through images) | an ImgBB reservoir | |
| AWS | website, any (through images) | S3 reservoirs, a static host | |

**What this buys:**

- **Compatibility drives the canvas.** Dragging a wire from a key lights up the
  sockets it can serve and dims the rest. `farm kinds --key cloudflare-main`
  lists the same.
- **The wizard starts from the document.** "Publish this map" asks for a key
  that serves maps, lists the ones the organization already has, and only then
  offers to add one.
- **Scope checks become possible.** A provider declares which scopes each use
  needs (`Pages:Edit` to publish, `DNS:Edit` to point a domain). A key that
  lacks one is *needs* on the node that needs it. A key with far more than
  anything uses is flagged, gently, on its own face. This only works where the
  vendor lets a key report its scopes (Cloudflare's token verification does;
  GitHub's fine-grained tokens do not report them the way classic tokens did),
  and otherwise the scopes are what the person ticked, recorded when they
  added it.

# 4. Monitors: keys cost money and expire

> API keys also might involve costs and money. Similar to an expiration date as
> well. Thats why there should be further "monitor" nodes or something for API
> keys. maybe in monitoring their specific api usage, monitoring usage on
> specific profiles in the network.

| node | in | out | source of truth | reaches outside? |
|---|---|---|---|---|
| **Usage** | `key` | `calls`, `by profile`, `by device`, `by node` (Value) | **our own dispatcher log**: every effect that used the key is logged with who ran it and where | no |
| **Vendor usage** | `key` | `requests`, `spend`, `quota left` (Value) | the vendor's usage API, where one exists | yes, gated |
| **Budget** | `spend` or `calls`, a limit, a period | readiness | turns *needs* when the period's use passes the limit, and can hold effects that would spend more | no |
| **Expiry** | `key` | `days left` (Value) | the date recorded on the key, or reported by the vendor | no |

**Usage from our own log comes first on purpose.** Every vendor exposes usage
differently, and many not at all. The dispatcher log already records every
effect with its author and device, so "who used the Google Maps key this month,
from which device, for which document" is answerable for every key, offline,
for free. Vendor numbers enrich it where they exist.

**Attribution** is the author's *"attributed to device context and profiles"*:
each key rune records the profile and device that added it, and each use is
attributed in the log. A key's face shows both.

# 5. Rotation

Replacing a key's value is `farm key set <key>` (value from a prompt). The new
value travels to every receiving member over the next sealed session, and the
face shows which members have the current value and which still hold the old
one: *"current on 3 of 4 devices; Ana's laptop last synced 2 days ago"*. That is
what makes the 2026-09-16 advice (short-lived tokens) livable: rotating is
one command, and everyone else gets it without asking.
