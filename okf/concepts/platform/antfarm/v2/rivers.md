---
type: Concept
title: Antfarm v2 — rivers
description: "The author's replacement for 'path' (2026-09-28): a river is a named place data goes into and comes out of, mapped closely onto an S3 bucket, and backed by one or more reservoirs (this device's folder, a LAN peer, a Reticulum peer, a cloud bucket, a write-only image host). What a reservoir can do is declared, not assumed. Reach instead of invented distances, local-first as a rule every river keeps, loud fallback, the management nodes (gauge, check, confluence, distributary, dam, store), how a live database and a sealed backup differ, and what Reticulum changes."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Design, not built.** See the [v2 index](/concepts/platform/antfarm/v2/index.md).

# 1. Why rivers

The author, 2026-09-28, answering the first conversation's argument that a
"path" type undoes content addressing:

> right now things are "location independent" sure, but, obviously that's not a
> reality of things. Data HAS to go somewhere. […] the Assets "path" isn't
> really a singular path. its a whole web of possible locations to store and
> retrieve files from. So maybe the issue is the term "file path".

That is right, and the two positions do not conflict. Content addressing says
*what* a file is. Something must still say *where* it rests, how full that
place is, whether it can be reached now, and what it costs. v1 answered that
with a node per vendor and no shared vocabulary. **A river is the shared
vocabulary.**

> Where different "Rivers" act as places where data can go into and come out
> of. […] its very similar to "buckets" from AWS. and honestly i think it should
> map very closely to it.

# 2. River and reservoir

| | **river** | **reservoir** |
|---|---|---|
| what it is | a named, logical place: `photos`, `backups`, `home` | one concrete place holding a river's objects |
| examples | `photos` | this device's `assets/` folder; the office PC over LAN; an R2 bucket |
| a person thinks | "our photos go in the photos river" | "…which is kept on this laptop, and copied to R2" |
| on the canvas | a River node, purple, square output | a row on the river's face, one strand each |

A river is to its reservoirs what an S3 bucket is to the disks behind it. The
person chooses the bucket. The reservoirs are how it is kept.

## The S3 mapping, deliberately close

| S3 | river |
|---|---|
| bucket | river |
| object key | `<prefix>/<sha256>.<ext>` for content; a name for a sealed blob |
| `PutObject` / `GetObject` / `HeadObject` / `DeleteObject` | `put` / `get` / `has` / `drop` |
| `ListObjectsV2` | `list` |
| bucket policy: public read | the river's `public` ability and its public address |
| lifecycle rules | a distributary with a date query (§5) |
| replication | a confluence (§5) |

Because an S3-compatible bucket already speaks this, R2, MinIO and Backblaze's
S3 endpoint are each one reservoir kind with an `endpoint` field, exactly as
`hol_object_store` is today.

## What a reservoir can do is declared

Not every place can do every operation. ImgBB can accept a file and hand back
a link, and cannot list or return what it holds
([mappings](/concepts/platform/antfarm/mappings.md) §4.2). v1 kept those
apart as "host" and "file store". v2 keeps them apart as **abilities** each
reservoir kind declares:

| ability | means | folder | LAN / Reticulum peer | S3-compatible | ImgBB | GitHub / static site |
|---|---|---|---|---|---|---|
| `put` | accept an object | ✓ | ✓ | ✓ | ✓ | ✓ (on the next publish) |
| `get` | return it by key | ✓ | ✓ | ✓ | by its link | by its link |
| `list` | say what it holds | ✓ | ✓ | ✓ | — | — |
| `drop` | remove it | ✓ | ask the peer | ✓ | with the `delete_url` (a secret) | on the next publish |
| `public` | give a link anyone can open | — | — | if `public_url` is set | ✓ | ✓ (`after_publish`) |
| `live` | hold a database file that is open and written in place | ✓ | — | — | — | — |
| `sealed` | everything is encrypted before it leaves | n/a | ✓ (Palabra) | when chosen | — | — |

**"Host it online" becomes a river question.** The capability that exists
today ("put this file online and give me a link") is answered by *any river
with a `public` reservoir*. The choice between ImgBB, R2 and the website keeps
its current rules: `config hosting.images` names one, and hosts whose link
works at once come before hosts whose link waits for a publish.

# 3. Local first, as a rule every river keeps

The author: *"i would like to keep a local first mindset with rivers as
well."*

- **Every river has a home reservoir on this device**, unless its face says,
  in words, that it has none (*"remote only: nothing is kept on this
  device"*). A new river is created with one.
- **Writes land at home first**, then travel to the other reservoirs as the
  river's policy says. A photo added offline is in the river the moment it is
  added.
- **Reads come from the nearest reservoir that holds the hash**, and the hash
  checks the bytes, so a copy from anywhere is as good as the original.
- **Fallback is loud.** If R2 cannot be reached, reads come from home and the
  face says *"R2 unreachable since 14:02: reading from this device"*. The first
  conversation's objection to a silent `Path Switch` stands: a fallback nobody
  sees is how two copies drift. It is safe for content, because hashes cannot
  drift, and it is refused for anything `live`.

# 4. Reach, instead of distance

The first proposal gave paths distances (0, 1, 3+). Those numbers were
invented. What is real is **how a reservoir is reached**, and each way has
honest properties:

| reach | examples | what it is really like |
|---|---|---|
| **here** | this device's folder | instant; always there |
| **LAN** | the office PC, a NAS | fast; present only while both are on the same network |
| **mesh** | a Reticulum peer | may be several hops; may be very slow (a LoRa link carries a few hundred bytes a second); may take hours to reach |
| **cloud** | S3, R2, ImgBB, GitHub | fast when online; costs money; answers to a vendor |

Each reservoir row on a river's face shows its reach, and a **Gauge** measures
the rest: whether it answered, how long it took, and when it was last checked.
Reticulum reports real hop counts, and those appear as a number on mesh rows.

# 5. The management nodes

The author: *"Rivers will require several other nodes in order to manage 'can
we actually use this river?' 'how much storage does this river have?'"* and,
of the whole conversation, *"the more we are discussing these datatypes, the
more that other 'operation nodes' or 'management nodes' emerge."*

**Reservoirs are nodes too, and a River node is a confluence of them.** Each
reservoir node outputs a river of one reservoir. A River node takes many and
names the result. That keeps every reservoir's own readiness, key and
placement on its own face, and it makes the strands on a river's wire literally
its reservoirs.

| reservoir node | in | stratum | notes |
|---|---|---|---|
| **Folder** | — | Ground | a folder on the device it is placed on; `live` |
| **Peer** | `profile` (■) | Surface | a reservoir on another member's device, over LAN or Reticulum (§7) |
| **Bucket** | `key` | Surface | any S3-compatible store: `bucket`, `region`, `endpoint`, `prefix`, `public_url` |
| **Image host** | `key` | Surface | ImgBB and its kind: `put` and `public` only |
| **Website as host** | `domain` | Surface | copies into a web domain's next publish (`after_publish`) |

| node | in | out | does | reaches outside? |
|---|---|---|---|---|
| **River** | `reservoirs` (River, many) | `river` (■) | names a river over its reservoirs (a confluence: reads from the nearest that holds the hash, writes home first and then to the rest), with a policy (§6) | — |
| **Store** | `mantle`, `river` | — | puts a mantle's files (Assets), or a sealed copy of a whole mantle, into a river **when run** | if the river leaves the device |
| **Gauge** | `river` | `used`, `free`, `objects`, `reachable`, `cost` (Value) | how full, how many, how reachable, and what it costs | **yes: it is a check** |
| **Check** | `river` | readiness | the smallest real read and write | yes |
| **Distributary** | `river`, `where` (Query) | `river` | routes objects matching a query to another river: "files over 5 MB go to R2", "backups older than 90 days go to cold storage" | — |
| **Dam** | `river` | `river` | holds writes (paused, read-only), with a reason on its face | — |
| **Warn below** | `value`, a threshold | readiness | "free space under 1 GB" turns the river's face to *needs* | — |

**A Gauge is a network request, so it is gated.** Distribution's rule applies:
*a check is a network request a person did not make.* A gauge measures when
someone presses Check or runs `farm check`, or when a rule driven by one device
schedules it. Its face says *"checked 3 min ago"*, never a live-looking
"online".

# 6. What a river holds

Two kinds of thing rest in rivers, and the `live` ability separates them:

- **Objects**: files by content (assets), and **sealed blobs** (an encrypted
  `.miga` backup, an encrypted snapshot). Any reservoir with `put` can hold
  these.
- **A live database**: the SQLite file the Data chamber is written into as
  people work. It must be open and written in place, so it can only rest in a
  reservoir with `live`, which today means this device's folder. `farm plug` refuses
  `Miga.rests in` from a river with no `live` reservoir: *"refused: a database
  that is being written must rest on this device. To keep a copy in R2, store a
  backup there."* A cloud row store (Supabase, Postgres) would be a future
  `live` reservoir with a very different reach, and is not planned.

The **home river** (`home`) is created by the seed: this device's folder,
`live`, holding the working copy, the JSON snapshot and the assets. It is what
v1's `hol_sqlite` and `hol_fs_assets` were, as one thing.

# 7. Rivers over Reticulum

The author:

> we can think of peer to peer file sharing as well. This is where reticulum
> might be an important consideration in the future development of rivers. how
> does it operate over networks?

Networking is Reticulum ([decided 2026-09-23](/concepts/projects/void-snape.md)),
with Void Palabra as its Void translation. A **peer reservoir** is a river kept
on another member's device and reached over it. Content addressing fits
Reticulum well: a request is "do you have `sha256:…`?" and the answer can be
checked on arrival, whichever path it took.

What has to be researched before a peer reservoir is designed further:

1. **Reticulum's transfer primitives.** Its `Resource` API moves large data
   over an established link, with sequencing and integrity checks. How it
   behaves for many small files versus one large one, and whether it resumes an
   interrupted transfer, decides whether a river syncs file by file or in
   packs.
2. **Links that are too slow to mirror over.** On a LoRa segment a single
   photo takes minutes. A mesh reservoir should default to **fetch on demand**
   (`get` a hash when a document needs it), not to mirroring everything, and
   its face should say so.
3. **Announcing what a peer holds.** Listing a peer's holdings is a request to
   that peer. Whether a peer announces a summary (a count, a digest) is a
   privacy decision as well as a bandwidth one: it tells everyone listening
   that the device exists and what it keeps.
4. **Who may read.** A peer reservoir is another member's disk. Holding the
   organization's photos is a permission the member grants, and revoking it
   must be possible. This is the [Network](/concepts/platform/antfarm/v2/network.md)
   chamber's roles, applied.

**The NAS backup story** from the first proposal lands here: a member's
always-on device (a NAS, a mini PC) is a peer reservoir, and backing up to it
is `Store` into a river whose reservoir is that device, sealed.
