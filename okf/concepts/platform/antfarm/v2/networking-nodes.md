---
type: Concept
title: Antfarm v2 — the networking nodes
description: "The author's direction, 2026-10-18: the Antfarm decides where a database's data lives (on this device too, down to the folder) and whom it speaks to, so the networking siblings (Void Hammer, Void Palabra, Void Verguenza, Void Chisme) get nodes of their own in it. A node family per owner, drawn from the networking stack's ownership line; the one rule that makes database separation a property of the graph (every networking node belongs to one database, and only the radios' switch and the profile belong to the device); what exists today; the order to build it; and the questions it raises. Design, not built."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-10-18T00:00:00Z
---

**Design, not built**, apart from what §4 and §5 list. The families were
taken as leans on 2026-10-18 ([Q119](/developer_questions.md)). See the
[v2 index](/concepts/platform/antfarm/v2/index.md). The separation it rests on
is built: [a database is its own room](/concepts/platform/database-separation.md).

# 1. The direction

The author, 2026-10-18, on what separating databases really involves:

> databases have multiple components to it. the antfarm is a big one. because
> the antfarm is the logic on where data is even located and how things work
> that way. this really makes me think we need to start including networking
> nodes into the antfarm. Void Hammer, Verguenza, Chisme, Palabra, and whatever
> else helps with networking, need to start having some of their own nodes and
> behaviors in the antfarm. … we do want to store stuff in the cloud, we want
> api keys too, but also we could move on to mesh storage and such as well. the
> antfarm in general is one of the most important structures for this new
> direction for the databases. because it decides where data lives (including
> the folder we use to reference where to store data and such on a device
> itself)

v2 already says where data *rests* ([rivers](/concepts/platform/antfarm/v2/rivers.md))
and what credentials a database holds ([keys](/concepts/platform/antfarm/v2/keys.md)).
This page adds the two things it left implicit: **where the database itself
lives on this device**, and **whom it speaks to, by which means**.

# 2. The rule that makes separation a property of the graph

**Every networking node belongs to one database.** Its configuration is in that
database's Antfarm, and its secrets are in that database's keyring on each
device (`<profile>/databases/<id>/`). Two databases are two graphs, so two
rooms, two member lists and two sets of links. Opening another database is
opening another graph, and nothing of the last one stays running.

Only three things belong to the device instead, and each says so on its face:

| device-wide | why it cannot be per database |
|---|---|
| **the profile** (name, colour, device key) | it is the person at this device ([identity](/concepts/platform/identity.md)) |
| **the radios' permission and on/off** | the operating system grants them to the application, not to a file |
| **the Reticulum identity and transport** (`<profile>/rns`) | one Reticulum node per process today; [Q122](/developer_questions.md) asks whether it should be one per database |

A networking node that needs one of these reads it and shows that it did
(*"uses this device's Bluetooth"*). It never stores it.

# 3. The node families, by owner

The owners are the [networking stack](/concepts/platform/networking-stack.md)'s
line: Void Hammer reaches devices, Void Palabra owns what crosses, Void
Verguenza holds secrets, Void Chisme watches, and Hormiga decides who may join
and what may leave. A family is the nodes one owner answers for. **Every name
below is a lean.**

## Home: where this database lives on this device (Hormiga)

| node | what it shows and decides | today |
|---|---|---|
| **home** | the bundle's file, the working copy's folder, and the keyring; placement *each* | the paths exist, nothing shows them together |
| **home river** | the reservoir on this device every river keeps ([rivers](/concepts/platform/antfarm/v2/rivers.md) §3) | `assets/` beside the working copy |

`home` is the author's *"the folder we use … on a device itself"*. It has no
inputs. Its outputs are the places other nodes resolve a relative name against,
which is what `database_file` does in code today. A face that shows the keyring
path makes the rule in §2 visible: this database's room key is *here*, and no
other database's is.

## Reach: how devices find each other (Void Hammer)

| node | replaces or adds | fields |
|---|---|---|
| **reach: LAN** | the beacon half of `hol_lan_share` | port, presence on/off, typed addresses, the multicast lock |
| **reach: radio** | the Migos screen's Bluetooth / Wi-Fi Direct switches, per database | which radios this database may use; the device's permission is read, not stored |
| **reach: mesh** | new: Reticulum interfaces and transport | interfaces, whether this device relays for others |

Reach nodes are the surface ([canvas](/concepts/platform/antfarm/v2/canvas.md)):
everything they do leaves the device, so every one is gated.

## Room and sync: what crosses (Void Palabra)

| node | what it decides | today |
|---|---|---|
| **room** | the database's room: its key (from the keyring), the proof each link makes, and which reaches carry it | `hol_lan_share`'s `key_file`; `database_room` and the `HZL1`/`HZP1` proofs |
| **sync** | the replica, the share filter, cautious files, what a member sends unasked | `hol_lan_share`'s `private_tags`, `send_hosted`; Settings' sync switches |
| **mesh reservoir** | a member's disk as a reservoir of a river, over a room | design only ([rivers](/concepts/platform/antfarm/v2/rivers.md) §7) |

A wire from **room** to a **reach** node reads "this database is shared over
this". With no wire, the database is in no room, whatever keys exist. That is
the "a database that was never shared has no room" rule, drawn.

## Members: who may join and what may leave (Hormiga)

| node | today |
|---|---|
| **members** | `hol_membership`: the registry, roles, precedence. It becomes the Network chamber's door ([network](/concepts/platform/antfarm/v2/network.md) §5) |
| **join** | the join half of `hol_lan_share`: allow, what a join carries (the plan), approvals |

## Keys: secrets (Void Verguenza)

The [key node](/concepts/platform/antfarm/v2/keys.md) is already the design.
Verguenza is where the bytes go once its client seam (`describe` / `use`, its
phase 2) exists. The room key is the first candidate: today it is a file in the
keyring; then it is a Verguenza secret scoped to this database's id, and the
**room** node names it the way any node names a key. API keys for cloud rivers
follow the same path.

## Watching (Void Chisme)

| node | what it decides |
|---|---|
| **observer** | whether a Chisme watcher may see this database's network, and what: counters, peers, never content. Placement: one profile |

Read-only by construction. It never carries a frame. It waits for Chisme's
observer role (a later phase there).

# 4. What exists today, in these terms

| family | built |
|---|---|
| home | the keyring under the database id; `home.txt` for copy detection; the working copy's folder (no node) |
| reach | the LAN beacon and the radios (no nodes of their own; the beacon reads `hol_lan_share`) |
| room | `hol_lan_share key_file`, resolved in the keyring and bound to the id; link proofs on LAN and radio |
| sync | `hol_lan_share`'s filter fields; `maiz::Network` over Palabra |
| members | `hol_membership`, resolved in the keyring |
| keys | v1 key files; the vault; Verguenza not integrated |
| watching | nothing |

**`hol_lan_share` does four jobs** (join, reach, room and filter). Splitting it
is the first concrete step, and the [migration](/concepts/platform/antfarm/v2/migration.md)
is where it happens: one v1 node in, four v2 nodes out, with the fields
carried over.

# 5. Order of work (lean)

1. **Done 2026-10-18:** separation by construction (keyring, bound room, link
   proofs, switch ends links, copies are new databases).
2. **The `home` node, read-only:** show the bundle, working copy and keyring
   paths for the open database, in the Antfarm and on the phone's Migas screen.
   It changes nothing and makes §2 visible. **First step built in 0.2.2** as a
   view (*Where this database lives*, in Share database and Migas, and `effect
   home`), not yet a node.
3. **Split `hol_lan_share`** into join, reach: LAN, room and sync, inside the v2
   migration. The radios become a **reach: radio** node per database.
4. **Mesh reservoirs** over a room, once Q114 settles the Reticulum transport.
5. **Room keys and cloud keys in Verguenza**, when its client seam exists.
6. **The observer**, when Chisme's role exists.

# 6. Questions this raises

- [Q119](/developer_questions.md) — the node families and names above.
- [Q122](/developer_questions.md) — one Reticulum identity per device, or per
  database?
- [Q94](/developer_questions.md) — the Network chamber's door, unchanged.
