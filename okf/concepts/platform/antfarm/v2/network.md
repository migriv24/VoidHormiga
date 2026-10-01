---
type: Concept
title: Antfarm v2 — the Network chamber, profiles, and placement
description: "Profiles and devices as one thing (the author's lean, 2026-09-28): a profile is a rune in the Network chamber, created on one device, carrying what that device can do; a person with a laptop and a phone has two profiles, linked deliberately from both sides, and a shared username only suggests the link. The playful 'a device is the glyph of a profile's rune' used as a guide. Placement: where a node's effects fire (each, any, or one profile), drawn as a ring. Online status from presence, never polling. Stations (a router, a NAS) left for the author."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Design, not built.** See the [v2 index](/concepts/platform/antfarm/v2/index.md).
**This page is the least settled of the v2 pages.** The author called
profiles *"an ongoing battle"* and will elaborate the network separately.

# 1. A profile and a device are one thing

The author, 2026-09-28:

> should a device and a profile be separate? I am starting to lean toward "no",
> and instead, having the ability for these profiles to be greatly associated or
> linked with one another (since on the network, they are also a rune in a
> mantle)

**Lean: one rune, `profile`, per device.** This agrees with what is already
written. [Identity](/concepts/platform/identity.md) holds that a profile is *a
credential on a device, not a person*, and since 2026-09-16 the key belongs to
the person at that computer. A person with a laptop and a phone has two
profiles. Nothing here is a person. The person, if the organization records
them, is a **contact** in the Data chamber, reached through the Network→Data
tunnel ([chambers](/concepts/platform/antfarm/v2/mantles.md) §4).

## "A device is the glyph of a profile's rune"

The author offered this *"not as the best way to think of it, but a funny way of
imagining things"* that might guide thinking. It does guide it. A glyph
declares what a rune has and what it can do. A device does exactly that for a
profile: a phone profile cannot serve a local website today, and an always-on
PC can hold a river for everyone.

So a profile carries a **device facet**, written only by the device itself:

| field | example | used by |
|---|---|---|
| `device` | `desktop`, `phone`, `station` | the palette, the face icon |
| `platform` | `windows-x64`, `android-arm64` | updates, the face |
| `serves_local` | yes / no | local domains ([documents](/concepts/platform/antfarm/v2/documents.md) §4) |
| `always_on` | yes / no | placing rules that must run at night |
| `holds` | the rivers it keeps a reservoir for, and free space | peer reservoirs ([rivers](/concepts/platform/antfarm/v2/rivers.md) §7) |
| `reach` | LAN, mesh | which peers can reach it |

It stays a facet rather than a literal glyph per device kind, as the author
asked. It is a guide, not a rule.

# 2. Linking profiles

> links between profiles will have to be more deliberate, with these profiles
> explicitly stating that they are closely linked together. Also, usernames can
> be shared among profiles. If two profiles have the same username, then maybe a
> mini warning pops up like "should we link these profiles together?" […]
> however, different in the core ID, because they are also runes

- **A link is a `same-person` relation between two profile runes, and it takes
  both sides.** One profile proposes. The other accepts, on its own device. A
  link either side could make alone would let anyone claim to be anyone.
- **A shared username only suggests.** When two profiles share a username, both
  see *"Another profile is also called ana. Link them?"*. Nothing links by name.
- **Identity is the rune id.** Names collide across members
  ([Q74](/developer_questions.md)); ids do not. A link joins ids.
- **What a link means:** the linked profiles are shown as one person in
  presence and in attribution ("Ana, on her phone"), and a key or permission
  granted to one can be offered to the other. It does not merge the profiles:
  each keeps its own device and its own signing key.

# 3. Placement: where a node runs

Placement is the one relation in v2 that is not a wire
([types](/concepts/platform/antfarm/v2/types.md) §1). It answers *"when this
node acts, which device acts?"*

| placement | means | typical nodes |
|---|---|---|
| **each** | every device does it for itself | the home river, the local snapshot |
| **any** (default) | whichever member runs it | publishing by hand, an import |
| **a profile** | only that device | the preview server on the office PC; the nightly backup on the always-on NAS; the one device that drives an automatic rule |

It is drawn as a **ring** around the node in the placed profile's presence
colour, with the profile's name on hover. A node placed on a profile that is
not here says so on its face (*"runs on office-pc, last seen 2 days ago"*).

**Automatic rules must be placed on one profile.** This is Void Maiz's "one
device drives" (`rules.hpp`), made visible: a rule placed on `any` would run on
every member at once, so `farm plug` refuses to enable an automatic rule until
it is placed.

# 4. Online status

The first conversation asked for online indicators. For profiles they are
free: **presence already knows who is here**, sealed to the room key
([LAN sharing](/concepts/platform/lan-sharing.md)). A profile node shows the
presence dot, the tab the person is in (Q76), and when they were last seen.
Nothing polls.

`farm profiles` prints the same: name, device, here or last seen, linked
profiles, and what the device holds.

# 5. The Network chamber's door

The Network chamber holds the members registry that has lived in
`members.json` since 2026-09-16. It was kept out of the data on purpose: *"an
admin-only rule can later be enforced at its door without touching the
organization's data."* Moving it into the `.miga` must keep that door.
**Lean:** a host-side guard refuses writes to the Network chamber from a
profile that is not an admin, in the CLI and the GUI alike, exactly as `farm
plug` guards types, until signatures let every member verify it. This is
[Q94](/developer_questions.md).

# 6. Stations: devices without a person (deferred)

The author wants to elaborate *"how the wifi router itself might be important
to consider as a device in the antfarm"*. The design leaves a place for it
and decides nothing:

- the `device` facet already has a `station` value, for a device that runs no
  one's session: a router, a NAS, a small server;
- a station can hold reservoirs, serve a local domain to the whole network, or
  be the device that drives rules;
- whether a router is a *reservoir*, a *domain* (the address the local network
  reaches the preview at), a *reach* (the LAN itself), or all three is the
  question for the author.
