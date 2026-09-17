---
type: Concept
title: LAN sharing — profiles, joining a database, and seeing each other
description: "The author's LAN-sharing brief (2026-09-16), as a design: a profile that belongs to the person at this computer rather than to a database; sharing as the PROVISIONING of another device (the database, the files the Antfarm says it cannot fetch for itself, and the keys), sealed and approved; a members registry as its own small database; presence sealed to a room key, with one colour per person; and the Antfarm nodes that decide all of it. The author's answers the same day: the host's Antfarm wins on credentials and keys are meant to go stale, sync after joining is automatic once it is correct, the room key and the pairing code stay stable. What is built, what is bones, and what is Void Palabra's."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-16T00:00:00Z
---

Opened 2026-09-16 on the author's brief, which is long and exact and is quoted
where a decision turns on it. It builds on
[collaboration](/concepts/platform/collaboration.md) (the beacon, the short
authentication string, the sealed stream, Palabra's merge — all built) and
revises one line of [identity](/concepts/platform/identity.md) (§1 below).

# 0. The two things this is, kept apart

| | **joining** (this page) | **syncing** ([collaboration](/concepts/platform/collaboration.md)) |
|---|---|---|
| when | once, per person, per database | every time two copies diverge |
| what moves | the database, the files, **the keys** | the state document |
| credentials | **yes, deliberately** | never |
| needs approval | a person on the host says *allow* | trust already pinned |

Collaboration's boundary — *`config`, `domains` and `bindings` do not sync, so
credentials cannot travel by accident* — stands. Joining is the one path where
they travel **on purpose**: the author's words are *"the sharing process
internally is essentially the construction and configuration of the other
device's hormiga antfarm. we are giving it all the keys and access tokens."*
Keeping the two paths separate is what keeps the old sentence true: a routine
sync still cannot carry a token, because a sync is not a join.

**And joining is not once-and-done** (the author, the same day — §3a): the
host's credentials are meant to go stale, so the credential half of a join is
repeated whenever members meet again. That repetition is a **refresh**, still
the join's path and never the sync's.

# 1. A profile is the person at THIS computer, not a database's

> a "profile" window. this is INDEPENDENT on a database. this is information that
> exists for any database.

[Identity](/concepts/platform/identity.md) §1 had an *admin profile* per database
per device. The author's brief splits that in two, and the split is better:

- **The profile** — per device, outside every database: a username, a colour, a
  picture, and a keypair. It lives in the user's settings folder
  (`%APPDATA%\VoidHormiga\profile` on Windows, `~/.config/voidhormiga/profile`
  on Linux, `~/Library/Application Support/VoidHormiga/profile` on macOS;
  `HORMIGA_PROFILE_DIR` overrides it, which is how two profiles run on one
  machine for testing).
- **The membership** — per database: a `member` rune in the members registry
  (§4) naming a profile's public key, the role it holds there, and when it
  joined.

Everything identity.md says still holds of both: **a profile is a credential,
not a person** — it never becomes a contact, never publishes, and the arrow to a
contact is still many-to-one. What changed is only *where the key lives*, and it
moved because "who is at this computer" is a fact about the computer.

**No password yet**, by the author's instruction, so the private key is a file
in that folder with owner-only permissions on POSIX — the SSH posture, and said
plainly rather than dressed up. Sealing it to a passphrase is the vault primitive
and is a later step, not a new mechanism.

**What the window detects, not stores:** operating system and version, device
name, processor architecture and threads, memory, the Hormiga and Void Core
versions, and the window size. Detected on each open, because a stored copy is
wrong after the next update. (The graphics renderer is not shown yet: only the
desktop shell holds a GL context, and it was not worth a new host seam today.)

**The default picture is drawn, not shipped:** the profile's colour as a disc
with the username's first letter. It needs no asset, reads at every size, and is
already the thing a presence strip shows.

# 2. Sharing: the host offers, a person asks, the host allows

The author's sequence, which is the design:

1. **Share database → Share over local network.** Offered only when the
   Antfarm has a `hol_lan_share` node with `allow: yes` (§5); the Cat Colony and
   a new database have one by default, and a database without one says how to
   add it rather than hiding the option.
2. The host **announces** on the beacon: the database's name and description
   (set at the top of the Share window: `org.name`, `org.description`; the icon,
   `org.logo`, is set there too but a picture does not fit in one datagram, so
   Discover shows the host's colour and initial instead), the host's username and
   colour. Only while sharing is switched on — an announced name is readable by
   everyone on the network, and the brief makes it a deliberate act.
3. The other device opens **Discover databases**, sees the offer, and asks to
   join.
4. The two devices complete the handshake (X25519, then the six-character
   **short authentication string** — [collaboration](/concepts/platform/collaboration.md)
   §4). **Both screens show the code.**
5. The host sees *"cool_username_123 wants to join Cat Colony. Their screen
   should show ABC 123. Allow?"* Nothing sensitive has moved yet.
6. **Allow** → the host sends the transfer (§3), adds the newcomer to the members
   registry (§4) and sends that too. **Deny**, or no answer in three minutes →
   the connection closes having carried nothing but two public keys and a
   username.
7. The joining device writes everything into a folder of its choosing, backs up
   the database it had open, opens the new one, and fetches the pictures that
   live online.

**Why the code is shown even though a person already clicks Allow.** Allow
answers *do I want this person*; the code answers *is this connection actually
to that person*. On a café's Wi-Fi they are different questions, and only the
second one stops a device in the middle that relays the handshake and receives
the keys. It costs a glance.

**The same two people always see the same code**, because it is derived from
their two long-term keys. **Decided 2026-09-16: keep it.** The author: *"sounds
alright for testing. though we may wanna think more about it later."* It never
was a secret — it is compared, not typed — and a stable code is what makes a
*changed* one a loud signal. Revisit when trust stops being two people in a room
and becomes vouching (§6, §9).

# 3. What the transfer carries is the Antfarm's decision

> this is determined by the antfarm though. If there is a personal SQLite server
> that's being used for things, that gets sent. however, if images CAN be
> downloaded from some online source that's hosting them … then those files don't
> need to be shared

Before sharing, the host's window shows **the plan**, built from the Antfarm and
nothing else — every line says what it is and why:

| item | sent when | why |
|---|---|---|
| the database (`.miga`) | always | it is the database |
| a local file the database references | its image rune has **no `url`** | nothing else can fetch it |
| a file that is hosted online | **never** (unless the node says `send_hosted: yes`) | the joining device downloads it from its `url` afterwards |
| a `hol_sqlite` store's file | it exists on this computer **and is not the working copy's own `.db`** (that one is rebuilt from the database on open) | a personal store is somebody's only copy |
| a key file an Antfarm node names (`token_file`, `key_file`, `secret_file`) | it exists | the brief: *"we are giving it all the keys and access tokens"* |
| a vault secret an Antfarm node names (`token_key`, `secret_key`) | the host's vault is unlocked | same; written into the joiner's vault only if theirs is unlocked, and reported otherwise |
| the members registry and the room key | always | §4, §6 |
| a rune carrying a **private tag** | **never** | §7 |

**On the wire** it is one sealed session: a JSON header naming every file and
its size, then the files in bounded chunks, then `done`, then the joiner's `ok`.
The joiner writes every file beside the new `.miga`, where
[key files are looked for](/concepts/platform/security.md), and checks each name
first (relative, no `..`, no drive) — a host naming `../../.bashrc` is refused
before anything is written. An Antfarm path that was **absolute** on the host is
rewritten to the plain file name **in the copy the host sends**, with dispatcher
`set`s on a throwaway core, so the host's own database is untouched and the
joiner opens a document that already points at the files beside it.

# 3a. The host's Antfarm wins, and keys are meant to go stale

> anyone who joins is given everything right now. this really means that im gonna
> have to start making api keys with smaller durration times, so that updates for
> rooms are required. someone can't just join once, then leave forever with my
> api key … someone has to be actively with me frequently, to recieve the new api
> keys. When sharing in general, the host's antfarm should be prioritized (but new
> content in the database should be merged of course, the priority is mostly about
> the api keys and such)

**Decided 2026-09-16.** Three rules follow, and together they replace "revoke a
member" — which Hormiga cannot do to a vendor's key — with "keys expire, and only
people who keep showing up get new ones":

1. **Credentials are short-lived by the operator's practice.** A token handed to a
   member is expected to stop working. Hormiga's job is not to prevent a copy; it
   is to make handing over the *next* one cheap.
2. **The host's Antfarm wins on credentials and wiring.** When members meet on the
   network, the host's key files, vault secrets and hosting nodes supersede the
   member's copies. "The host" is the member who shared the database — the first
   member in the registry today; `precedence: admins-first` names who else may
   act as one once roles are enforced.
3. **The organization's data still merges.** Priority is about keys and backends;
   contacts, events, notes and documents go through Palabra's merge and nobody's
   side wins by rank (§5: a conflict is a value).

**A refresh** is the credential half of a join, repeated: sealed, over the same
path, carrying only what changed. A member already in the registry, recognised
by their key, is refreshed without a new Allow while both are present — the
first Allow is what admitted them — and a refresh is logged on both sides.
**Decided, not built.**

**A joiner without an encrypted vault.** The author asked how one could exist:
a first-time user has not made one, because encrypting credentials is offered
rather than forced ([security](/concepts/platform/security.md) §2). The answer
to *"anyone who joins is given everything"* is therefore: **when a plan carries
vault secrets and the joiner has no vault, the join asks them for a passphrase
and makes one before it accepts.** Writing the secrets as plain files instead
would silently undo the host's choice to encrypt them. **Decided, not built** —
today such secrets are reported and not kept.

# 3b. After joining, syncing is automatic — once it is correct

> i would want syncing to be automatic. because i dont see a reason to share a
> database, share antfarms, and just, not sync? … as long as it WORKS correctly.
> … there should be a mini loading bar or something for an initial sync.

**Decided 2026-09-16: automatic, with a progress bar for the first sync.** A
member who wants a copy that never syncs is better served by an Antfarm that says
so than by a switch in this window.

**The condition was met the next day, and this is built** (2026-09-17). Void
Palabra shipped a **replica**: a device's enriched document, kept between
exchanges, so a removal is a recorded act rather than an absence. Our old merge
enriched both current states afresh every time, which is what brought deleted
runes back — and their point was sharper than ours: *one* peer still doing
that resurrects deletions for **everyone** it syncs with. So the old `lan-serve`
and `lan-sync` verbs are **retired**, not kept as an alternative.

**How it runs.** Each device keeps one replica per database in its profile folder
(`replicas/<database id>.replica`; the id is `config sync.database`, which travels
in the bundle). Once a second, if the document changed, the private runes are
stripped, the replica `observe`s what the person did, and **the replica is saved
before anything leaves the device** — a crash between sending and saving would
reuse tags. The version a replica *shows* rides inside the sealed presence beacon,
so two members who agree never open a connection; when they differ, the one with
the **lower key fingerprint** connects and both send their whole document, with a
progress bar. What arrives is merged at the start of the next frame: observe again
(an edit made in between is recorded, not overwritten), merge, save, splice
`mantles` and `glyphs`, put the private runes back, and swap the core — keeping
the selection and the open document, which is why this is not `reload_from_state`.

**A join hands over the host's replica document**, adopted under the joiner's own
fresh id, so the two share one history from the first minute and a deletion made
after they join reaches them. Handing it over is safe: private runes were never
observed into it, and a deleted rune's content does not stay in it — pinned in
`hormiga_lan_smoke` rather than assumed. Replica *identity* never travels, which
is Palabra's own condition on provisioning.

**The Antfarm is not merged.** Wiring and keys come from the host (§3a), so member
sync strips the Antfarm mantle the way it strips private runes and puts this
device's own back on the splice. Without that, the plan's absolute-to-relative key
rewrite on one device would flow back and change the host's own Antfarm.

**What Palabra warned an automatic loop gets wrong, and where each is answered:**
a conflicted field shown as one value is never written back as a decision (their
`observe`); an idle tick mints nothing (theirs, and ours skips an unchanged
document for the price of one hash); a delete that raced an edit is reported as
`deleted_while_edited` and shown with **Keep it** / **Delete it** rather than
vanishing; a restored or copied replica is caught as `identity_collision` and
forks to a fresh id; undo cannot revert another member's change, because the
swapped-in core starts with an empty undo stack; and whole documents are always
exchanged, so a lost delta cannot go unnoticed.

Still to build: **credential refresh from the host** (§3a), on the same meeting.

# 4. The members registry is its own small database

> After that, a new mini database will also begin to form … for the cat
> database, it'll be a separate local mini database. This database is only able
> to be edited by admins.

A **Void Core document of its own**, `members.json` by default, beside the
`.miga`: one `member` rune per profile that has joined — public key,
fingerprint, username, colour, a small picture, role, joined, invited by, last
seen. Its own dispatcher, so every change to it is a logged command like any
other; its own file, so a routine data sync does not carry it and an admin-only
rule can later be enforced at *its* door without touching the organization's
data.

**Where it lives is the Antfarm's** (`hol_membership.store`): `local-file` now;
`mantle` (inside the database) and `relay` (§8) are named so a later choice is a
value, not a redesign.

**Today everyone is an admin** — the author's instruction, and the honest state
identity.md already records: anyone holding the `.miga` is. The `role` field
exists so it is recorded from the first member; enforcing it needs signatures
(§9).

# 5. The Antfarm nodes: the bones

| node | answers | fields |
|---|---|---|
| `hol_lan_share` | may this database be shared, and how | `allow` (yes/no), `presence` (yes/no), `port`, `key_file` (the room key, default `lan-room.key`), `private_tags` (default `private`), `send_hosted` (no) |
| `hol_membership` | who is in, where that list lives, whose change wins | `store` (local-file · mantle · relay), `file` (`members.json`), `default_role` (admin · editor · viewer), `precedence` (everyone-equal · admins-first) |

`domain/collab.hpp` is to collaboration what `domain/hosting.hpp` is to images: a
table of **capabilities** — *share this database*, *keep the members*, *carry
presence* — and which kinds of node answer each, with a row marked **planned** for
the ones that do not exist. Every surface reads the table, so a relay is a row
and a branch, not a rewrite.

**`precedence` is recorded and not enforced.** Palabra reports a conflict as a
value and never picks; *admins-first* will mean "an admin's side is offered as
the default in the conflict view", never "silently wins", because a silent win is
the resolve-by-seniority the Allomone lattice was arranged to refuse.

# 6. Presence: sealed to the room, one colour per person

> as long as both devices are on the same wifi, they should sort of be able to
> see each other active on the same database. similar to how google docs …

**The room key.** When a database is first shared, the host generates 32 random
bytes — the room key — kept as a key file beside the `.miga` and handed to every
member at join. Presence is a beacon whose body is **sealed with the room key**
(XChaCha20-Poly1305, libsodium), carrying: which member, which section, which
mantle, and which runes are selected. Everyone on the network can see that *a*
Hormiga is present; only members can read *who* or *what*. A beacon in the clear
would announce the names of an organization's contacts to a café.

**The room key does not change — decided 2026-09-16.** The author's lean was no
("easier to test") with an explicit instruction not to let that decide it, so the
reasoning, for the record: the room key **reads presence and nothing else** — who
is here, which section, which runes are selected. It decrypts no data and no
credential. What a departed member really keeps is the API keys, and §3a's answer
to that is expiry at the vendor, not a new room key. So on a local network,
rotating it would buy the ability to stop an ex-member on the same Wi-Fi seeing
*which rune someone has selected*, which is not worth a re-key round today.

**When it must change**, so the decision has an exit: (a) the relay (§8), where
the room key would seal the organization's data at rest in someone else's cloud,
and a departed member holding it could read everything; (b) removing a member for
cause. Rotation is then a new key handed to present members by a refresh (§3a).

**Highlighting.** A rune another member has selected is marked in their colour:
a coloured bar and their initial on its row in the Data and Notes lists, a
coloured outline with their name on the Builder's node canvas. (Not yet on the
Builder's page view, the map, or the calendar.) The menu bar carries their avatars; hovering says what
they are on, clicking goes there (the redirection of
[workspace](/concepts/sections/workspace-and-sections.md)).

**No two people share a colour on screen.** Each profile *prefers* a colour. On
each device, present members are ordered by when they joined (then by key); each
takes its preference if nobody earlier holds it, otherwise the nearest free
colour in a twelve-colour palette, and past twelve the hue advances by the golden
angle. The rule is deterministic, so every device reaches the same assignment
without talking about it, and the profile window says when your colour is being
shown differently and why.

## With N people, not two

| grows with N | the answer here |
|---|---|
| beacon traffic | one datagram per person every three seconds, broadcast: **O(N)**, not a connection per pair |
| colours | twelve distinct, then golden-angle hues; past about twenty, colours stop being tellable apart and the initial carries identity |
| the avatar strip | the first six, then *+N* with a list |
| a rune several people have selected | stacked bars, one per person, in join order |
| the members registry | a rune per profile; `left` is declared so people who leave stay recorded (nothing sets it yet), because a history of who had the keys is the point |
| data sync | pull from any member — Palabra's join is commutative, so there is no host to wait for |
| trust | pairwise SAS scales badly; past a handful of people, trust is **vouching**: a member signs a newcomer into the registry, which is §9's capability work |

# 7. Private data

> the notes should have the option of making private and shared notes. ("shared"
> notes are only among those who the database has been shared to)

**A rune carrying a tag in `hol_lan_share.private_tags` never leaves this
device** by share or sync. The Notes tab offers *Private* and *Shared* on every
note, which adds or removes the tag. It is enforced at the two seams — the join
transfer and the LAN exchange — per ground rule 6, not by hoping a template skips
it. A private note is still in *your* `.miga` when you save one, because that
file is yours; handing the file to someone by hand is a different act.

Notes never publish to a website either way; *shared* means shared among
members, which is what the author said.

# 8. When the database lives on AWS instead

> what if eventually a database is entirely hosted on some AWS node
> configuration? how would that look like?

The same capabilities, answered by a different node, and the line
[data planes](/concepts/platform/data-planes.md) §3 already drew does not move:
**a cloud store holds ciphertext it cannot read.**

- `hol_sync_relay` (planned) answers *share* and *keep the members*: an object
  store or a small queue holding **sealed** utterance batches and a **sealed**
  members registry, addressed by the room's public id. Members pull, open with
  the room key, merge with Palabra, push their own. The relay never holds a key.
- Presence over a relay is a short-lived sealed object per member, not a beacon.
- Joining still needs the approval and the code; over the internet the code is
  read over a call instead of across a room.

What would **not** be done: a queryable cloud database of the organization's
rows. That is the breach data-planes exists to prevent, and a relay makes it
unnecessary.

# 9. The ledger is Palabra's history graph

> we can sort of think of this like blockchain technology? … every device has
> something similar to a "ledger" of things and edits. maybe Void Palabra should
> be the one to handle this?

Yes, Palabra — and the part of blockchain that is wanted is the part that is
already there. Palabra's **utterances** name their parents and are content
addressed: a hash-linked history graph that every device holds, where a changed
past is detectable. What a blockchain adds on top — **consensus** on one total
order — exists because a currency cannot tolerate two orders. A database with a
CRDT join does not need one order: any two devices that have seen the same
utterances hold the same state, in any order. So no mining, no chain, no
agreement round.

What *is* missing is on Palabra's roadmap and is asked for in
`MESSAGE_FOR_VOIDPALABRA_hormiga-lan-sharing-ledger-and-trust-2026-09-16.md`:
**signed utterances** (attribution as proof), **capability tokens** in the
Meadowcap shape that `peer-and-tier` already names (an admin grants write to a
region; roles become enforceable), a **sealed transport** so our `sync/peer.*`
can be deleted, and an **ephemeral, unversioned channel** for presence.

# 10. What is built, and what is not

Measured 2026-09-16 unless marked otherwise. "Loopback" means two processes with
two profiles on one Windows machine; **nothing here has crossed a real Wi-Fi
between two computers yet** — that is the author's test.

| | state |
|---|---|
| profile window: username, colour, picture, device facts, key | built; **seen in a window** |
| database name and description in the Share window | built; seen |
| `hol_lan_share`, `hol_membership`, `member`; defaults in new databases and a fresh Cat Colony | built; the spine test counts them |
| the share plan, read from the Antfarm | built; printed by `effect lan-share` |
| discover | **verified**: a CLI host's offer appeared in the GUI's Discover window |
| ask / code / allow / transfer | **verified on loopback** (CLI host, CLI joiner): both printed the same code; the database, key file, members registry and room key arrived; a refusal sent nothing and created no folder |
| hosted pictures left out, local ones sent | **verified** — inside the bundle and not |
| private notes withheld at the join | **verified** — the note is absent from what arrived |
| private runes withheld at the LAN exchange | built, not re-run |
| the members registry as its own document | **verified** — both people recorded |
| the GUI's Share button, the Allow dialog, the joiner opening the database, hosted pictures downloading | built; **not clicked** |
| presence and highlighting between two running windows | built; the sealing and the beacon round trip are unit-tested (`hormiga_lan_smoke`); **not seen live** |
| unique colours | unit-tested up to 34 people |
| vault secrets across a join | built; kept only when both vaults are unlocked; not run. A joiner without a vault is asked to make one — **decided (§3a), not built** |
| credential refresh from the host when members meet | **decided (§3a), not built** |
| automatic sync between present members, with a progress bar | **verified on loopback** (2026-09-17): after a join, the host's deletion and contact edit and the joiner's new note all landed, the private note never left, and each side kept its own Antfarm |
| deletions that stay deleted | **verified** — `hormiga_lan_smoke` pins it, and the two-process run shows it |
| a delete that raced an edit, shown with a choice | built; the conflict list is in Share database. Not yet provoked on purpose |
| `lan-serve` / `lan-sync` | **retired** — they resurrected deletions for every peer they met |
| roles enforced, signed changes, relay | **not built** — §8, §9, Palabra |

**Windows Firewall asks the first time Hormiga listens.** Sharing and presence
open ports on the network; Windows shows its "allow public and private networks"
prompt for `voidhormiga.exe`. That is the operating system doing its job, and
answering it is the person's decision, not the application's.

# Boundaries

- **Joining carries keys; syncing never does.** Two paths, kept two. A refresh
  is a join's credential half, not a sync.
- **The host's Antfarm wins on credentials; the data merges.** Keys are expected
  to expire, and only people who keep meeting the host get new ones.
- **Sync is automatic only once deletions propagate.** Automatic and wrong is
  worse than manual and reported.
- **Nothing sensitive moves before a person allows it and the code is on both
  screens.**
- **The Antfarm decides what is sent.** The share window shows the plan it read.
- **Presence is sealed to the room.** A name in a beacon in the clear is a leak.
- **Private tags stop at the device**, at both seams.
- **A conflict is still a value.** `precedence` offers a default; it never
  resolves silently.
- **The ledger is Palabra's.** We do not build a chain, a consensus, or a second
  history.
