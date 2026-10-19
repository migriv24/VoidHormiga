---
type: Concept
title: A database is its own room
description: "Databases are separate by construction (decided and built 2026-10-18, after a test colony kept syncing into the next database opened). A database's identity is its sync id; joining is the only way two files are the same database, and every copy, Save as and file copy alike, is a new one. Its room key and member list live in this device's keyring under that id, never in a folder; its room is the key file bound to the id; every member link proves the room before a frame; switching ends every link on every transport. What was wrong, the rules, what is built, how it was measured, what an existing database does on upgrade, and what is left."
tags: [status:current, audience:all, confidence:measured]
timestamp: 2026-10-18T00:00:00Z
---

# What went wrong

The author, 2026-10-18:

> i create a test networking database, connect devices together to test if
> stuff works … i end the tests … the devices are still connected, so even if i
> switch to another database, the transfer of data from the previous device is
> still there and it attempts to transfer everything over

and, of the fix they wanted:

> if i previously connected for a "volunteer group" database, then i want to
> temporarily go to my personal "family home" database, i shouldn't fear that
> the data from the previous one is gonna leak over … i don't actually want a
> "log off" feature … i just don't want my test databases to switch over to the
> real ones.

**The network secrets belonged to a folder, not to a database.** Four things
added up to the leak, each found in the code:

1. **One room key and one member list per folder.** `lan-room.key` and
   `members.json` were found by name beside the `.miga`, then in the working
   copy's folder, which every database searches. A phone keeps every database in
   one folder, so every database on it shared the first room ever made there.
2. **Presence defaults to on**, so a database that found a key announced itself
   to that room without anyone sharing it.
3. **Nothing asked which database the other device had open.** A member link was
   let in on its device fingerprint alone, and Void Palabra's session has no
   notion of a database: two databases in one room merged into each other.
4. **The radios kept their links across a switch.** The room changed, the radios
   restarted, the Reticulum links proven for the old room stayed, and a branch
   commented *"a new database: carry again"* handed the new database's frames
   to them.

Logging off would have hidden this behind a habit. The fix makes separation a
property of the mechanism, so there is nothing to remember.

# The rules

1. **A database's identity is its sync id** (`config sync.database`), minted
   the first time it is shared.
2. **Joining is the only way two files are the same database.** Everything else
   is a copy, and **a copy is a new database** (the author: *"a copy is a copy,
   so its not the same. it's effectively an entirely new database"*):
   - **Save as** gives the open database a new id before it is packed, and
     leaves the old one's room. Saving onto the same file, or the first save of
     a database that had no file yet, is naming it, not copying it.
   - **A file copied outside Hormiga** is caught when it is opened: this device
     remembers where each id's file lives (`home.txt`), and a second, different
     file with the same id, while the first still exists, is a copy. It gets a
     new id and is saved with it at once, so it does not get another on every
     open. A file that *moved* keeps its id, because the old path is gone.
   - **A backup is a copy.** Opening one while the original exists makes it a
     database of its own. That is the safe direction: it can never sync into
     the original ([Q120](/developer_questions.md) asks whether restoring should
     be different).
3. **A database's network files live in this device's keyring, under its id**:
   `<profile>/databases/<id>/` holds `lan-room.key`, `members.json`, `home.txt`
   and `settled`. The Antfarm still names them (`hol_lan_share key_file`,
   `hol_membership file`); a relative name resolves there, and an absolute one
   is used as written, because the Antfarm decides where things live. A
   database that has never been shared has no id, so it has no keyring and no
   room.
4. **A database's room is its key file bound to its id**
   (`hormiga::lan::database_room`, keyed BLAKE2b). Two databases that hold the
   same key file are two rooms. Members derive the same room because a joined
   copy carries the host's id.
5. **Every member link proves the room before its first frame.** On the LAN:
   `HZL1` and an HMAC over the handshake's SAS and the sender's fingerprint,
   both ways, inside the sealed stream. On the radios: the existing `HZP1`
   proof, now over the database's room. A device in another database, or an
   older Hormiga, is refused, and the log says which.
6. **Switching ends every link of the old database, on every transport.** The
   room is forgotten the moment the database closes, not a second later, so
   nothing reads a stale one. A radio that was on comes back on in the new
   database's room, if it has one, and stays off if it does not. A radio link
   whose device leaves the member list is closed.

# What an existing database does on upgrade

- **It keeps its room and its members.** The first time its id has no keyring
  on a device, the room key and member list beside its bundle are copied in
  once (never from the working copy's folder, unless it has no bundle), and
  the keyring is marked settled. The old files are left in place, so a
  downgrade still works. They can be deleted.
- **A joined database's files move.** A join still sends `lan-room.key` and
  `members.json` beside the bundle (the wire is unchanged), and the app then
  moves them into the keyring.
- **A member list that sat beside several databases** was copied into each of
  them, and the log says so: *"…sat beside N other database(s), which all used
  it. This database has its own copy now; check who it lists as members."* The
  rooms are separate anyway, so a member of the wrong database cannot connect.
  The list is only untidy: **Remove** them in Share database (0.2.2,
  [Q121](/developer_questions.md)).
- **Every device must update.** A 0.2.1 device derives no room and sends no
  room proof, so it cannot see or reach a device with this change, and the
  reverse.
- **A database made by copying a test database before this change** still holds
  the test database's id. On the device that made the copy, opening it is
  enough (copy detection). Elsewhere, **Save as** a new file, then share that.
  The command bar's `config get sync.database` shows the id.

# What was measured (2026-10-18)

On one machine, two profiles (`HORMIGA_PROFILE_DIR`), loopback, synthetic Cat
Colony data, the CLI:

- **Same database:** a host shared `test` from a folder that also held
  `family`, and a second profile joined. The host's key and registry were
  written to its keyring, not the folder. Both stayed present and an edit
  crossed (`present: 1`), so the LAN room proof passes between members.
- **The author's bug, staged on purpose:** the old layout was rebuilt by hand,
  with the test database's room key and member list loose beside `family`, and
  `family` given an id. The host opened `family` while the member stayed in
  `test`. `family` adopted the loose files (and said so), its room was its own,
  and **neither side saw the other** (`present: 0` on both). Every mantle of
  both databases was unchanged.
- **The desktop, through its real code** (`HORMIGA_BOOT_CMD`, an isolated
  profile and settings): opening `orig.miga` kept its id; **Save as** `copy.miga`
  gave it a new one; reopening `orig` kept its own; opening `dup.miga`, a file
  copy of `orig`, gave it a third, saved into the file.
- `hormiga_lan_smoke`: one key file in two databases is two rooms, presence
  sealed in one does not open in the other, and a link proof changes with the
  room, the pair of devices and the sender.

**Found on the way:** `effect open-database` and `effect save-database` crashed
the desktop every time. Run inside a dispatch, they swapped the core under the
frame drawing it, the hazard the [log](/log.md) recorded for the phone on
2026-09-27. They now wait for the next frame, as the File menu does, and
`run_busy` keeps every action asked for in one frame instead of the last one.
The unchanged build crashed the same way, so this was not introduced here.

# What is left

- **Not run on two real devices yet.** The radio change (links end on a switch)
  was reasoned and built, and `radio_smoke`/`rnslink_smoke` still pass, but no
  test switches databases while a radio link is up.
- **Built in 0.2.2 (Q121, the lean taken):** an admin's **Remove** marks a
  member `left` and ends their links at once; **Leave this database** marks this
  device `left`, deletes its room key and keeps the data. That is the permanent
  thing the author guessed "log off" might be for. The phone has Leave, not yet
  Remove. **Where this database lives** (file, working copy, identity, network
  files, room) is in Share database, on Migas and in `effect home`.
- **Measured for those:** with the CLI, a removed member's edit did not arrive
  (the host refused its link), a second removal is refused (*"already left"*),
  and after `lan-leave` the device has no room and a second leave is refused.
- **The Reticulum identity is still one per device** (`<profile>/rns`), so a
  radio announce carries the same device fingerprint in every database's room
  ([Q122](/developer_questions.md)).
- **Where all of this goes next is the Antfarm**, which decides where a
  database lives on a device and whom it speaks to:
  [the networking nodes](/concepts/platform/antfarm/v2/networking-nodes.md).
