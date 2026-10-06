---
type: Concept
title: Radios — members near by with no Wi-Fi in common
description: "The author's 2026-10-05 ask: phones share a database with no network in common, over Bluetooth LE and Wi-Fi Direct, never a hotspot, with Reticulum on top. The four layers and whose each is (Void Maiz's radio holiday and bridge, Void Palabra's Reticulum node, Hormiga's membership, the same Network a LAN link feeds); how only members ever connect; why frames are compressed on a radio; what was measured on two processes and in the phone harness, and what still needs two phones in hand."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-10-05T00:00:00Z
---

Opened 2026-10-05. It answers Q103 (phones with no
Wi-Fi in common), which this page replaces, and extends
[LAN sharing](/concepts/platform/lan-sharing.md) §3b: after joining, members
keep each other in sync over whatever reaches, now including the radios.

# 0. What the author asked

> "for networking and such, we should begin development of wifi direct with
> android phones and bluetooth. bluetooth will be substantially slower (so a lot
> more progress bars and such will be needed), however, it would be compatible
> with an iOS device and android device in the future. and bluetooth seems like
> reticulum already is good for it, so might as well try to integrate it into
> hormiga. i don't wanna do hostpot, because that would be a big waste of data
> plans and money. also not everyone can easily do hotspot, also that assumes a
> cellular data plan as well."

So: two radios, both phone to phone, both carrying Reticulum. **No hotspot**
path is planned or tested; it was lean (a) of Q103 and the author ruled it out.

| radio | reaches | speed | why it is here |
|---|---|---|---|
| **Bluetooth LE** | any two phones; an iPhone later | a few KB/s | LE GATT is what an iPhone may speak to anything; classic Bluetooth (RFCOMM) needs Apple's accessory programme |
| **Wi-Fi Direct** | Android to Android | megabytes a second | fast enough that a first sync is not a wait; Android only |

# 1. Four layers, and whose each is

The rule from 2026-09-19 holds: Void Maiz owns what networking *looks like* and
how a device reaches another, Void Palabra owns what it *is*, Hormiga owns what
is shared and with whom. So the code sits in three repositories:

| layer | where | does |
|---|---|---|
| the radio holiday | Void Maiz `voidmaiz/radio.hpp` | bytes between nearby devices. LE: advertise a short tag, scan, connect, one byte stream each way over two GATT characteristics. Wi-Fi Direct: a DNS-SD service with the tag, a group, an IP address. Android half in `MaizRadio.java`; a desktop stand-in (`loopback_radio`) that is a throttled local TCP stream |
| the bridge | Void Maiz `voidmaiz/rnsradio.hpp` | a connected LE peer becomes a Reticulum *pipe* (`ble:<peer>`, HDLC-framed, declared at LE's real speed so Reticulum waits as long as LE needs); a Wi-Fi Direct group becomes a UDP interface (`p2p`) |
| Reticulum | Void Palabra `voidpalabra/reticulum.hpp` | one node per process: announces, encrypted links, large messages as Resources (with progress). Gained pipes, interfaces added and removed while running, and `transfers()` for this |
| membership and frames | Hormiga `app/lan_radio.cpp` | who may connect, and handing `maiz::Network`'s frames to a Reticulum link and back |

**A radio link is a link.** `maiz::Network` (the sync seam since stage C) cannot
tell a radio link from a LAN one: both are a `link_io` entry keyed by the
member's fingerprint. So a member is carried by one or the other, never both.
Whichever is up first carries; a radio link that finds the LAN carrying stays up
and idle and takes over the moment the LAN link goes. Nothing above the link,
the merge, the conflicts, the files, presence on the roster, the transfer bars,
knows which.

# 2. Only members ever connect

The room key (32 bytes, from the join) is the boundary, as on the LAN:

1. **Discovery.** Each device advertises `room6 + self4`: six bytes of a keyed
   hash of the room key (a different context from the LAN beacon's room id, so
   the two never match), then four random bytes for this run. Nobody without
   the key can compute room6 or recover anything from it. A device connects only
   to a peer whose room6 matches, and **only the side with the lower self4
   dials**, so two phones make one stream, not two. Devices of other databases
   are not connected and **not shown**.
2. **Announces** carry `{room, fp, user}`. A link is opened only to a fingerprint
   in this database's members registry, by the lower fingerprint (the LAN's
   rule).
3. **Proof before any frame.** On a new link each side sends
   `HZP1 · fingerprint · name · colour · crypto_auth(room key,
   "hormiga-rns-member|<link id>|<fingerprint>")`. The link id is fresh per
   link, so a proof cannot be replayed onto another. A link that has not proven
   itself in 30 s is closed; a failed proof closes it and says so in the log.
   Proofs are single packets, which Reticulum does not resend, so each side
   resends its own every 4 s until it has the other's, and answers a proof that
   arrives on an already-proven link (its own was lost).

Reticulum encrypts the link itself, forward-secret; the proof is ours. Inside a
room every member is trusted the same, exactly as on the LAN (lan_net.cpp point
4): roles and signed changes are still Palabra's to build.

# 3. A frame on a radio is compressed

**Measured, and the reason this section exists**: Palabra's session sends a
member's shareable state *whole* when a session starts (`Kind::doc`, "my
shareable state, whole"). For the Cat Colony a **one-field edit is a 600 KB
frame**. On the LAN nobody notices. On Bluetooth LE at ~6 KB/s it is a hundred
seconds, every time two phones meet.

The state is JSON and deflates about 12:1 (599,720 -> 48,079 bytes with zlib;
70,352 with the compressor we have). So every frame on a radio link goes as
`Z` + deflate when that is smaller, `R` + the frame otherwise. The compressor is
stb_image_write's and the inflater stb_image's, both already vendored: nothing
new. An inflated frame larger than Palabra's own 64 MiB ceiling is dropped.

That makes the radio usable; it does not make it cheap. **Deltas**, sending a
peer only what it lacks, are Palabra's design to change, and are
[Q104](/developer_questions.md).

# 4. Progress, and what a person sees

The author: "a lot more progress bars and such will be needed". A message larger
than one Reticulum packet travels as a *Resource*, whose parts Reticulum counts;
each one moving on a member's link is a bar on **Migos > Transfers**, both ways
("changes, over Bluetooth from ana — 36 KB of 68 KB, 3 KB/s"), with the same
done-and-kept-a-moment behaviour as a LAN transfer. A member reached by radio is
on **Here now** with the rest; their signal bars come from the radio's RSSI
(-100 to -50 dBm) when there is no LAN beacon to count.

Migos gained **Nearby, no Wi-Fi needed**: a switch for Bluetooth and one for
Wi-Fi Direct, each saying what it costs and what it reaches, off until switched
on. The first switch shows Android's "Nearby devices" prompt; when it is granted,
the radio starts by itself (no second tap). Refused, it says where to allow it.
Below the switches, the members in range, connected or not, with their signal.

# 5. Android

`AndroidManifest.xml` asks for `BLUETOOTH_SCAN` (`neverForLocation`),
`BLUETOOTH_CONNECT`, `BLUETOOTH_ADVERTISE`, `NEARBY_WIFI_DEVICES`
(`neverForLocation`) and `CHANGE_WIFI_STATE`, plus the legacy `BLUETOOTH` and
`BLUETOOTH_ADMIN` capped at API 30; older Android asks for location instead,
which the map already declares. LE and Wi-Fi Direct are `required="false"`
features: a phone without them installs and says so. The shell installs
`maiz::android_radio(activity)` beside the location holiday; nothing is asked
until a switch is touched.

# 6. What is built, and what is not

| | state |
|---|---|
| Palabra pipes, runtime interfaces, HDLC, transfers | built; Palabra's suite **29/29**, including Reticulum over a throttled pipe against the Python reference |
| Maiz radio holiday, bridge, loopback radio | built; `maiz_radio_smoke` moves 30 KB over a loopback "LE" at 8 KB/s between two processes, the tag and a transfer seen |
| `MaizRadio.java` (LE GATT both roles, Wi-Fi Direct DNS-SD and groups, permissions) | built; the dex compiles and the APK builds. **Never run on a phone** |
| Hormiga membership, proof, frames, compression, progress, roster | **verified on two processes** (2026-10-05): two CLI members with no LAN beacon (`HORMIGA_NO_BEACON`) met over the loopback radio at 6 KB/s, proved membership, and each one's edit landed on the other with no conflict; the 600 KB state crossed as 70 KB |
| the phone's Migos section, permission flow, transfer bars | **seen in the phone harness**: Bluetooth switched on, ana found and connected at -40 dBm, on Here now, a bar filling at 3 KB/s, and the host's edit in the phone's database |
| Wi-Fi Direct end to end | the bridge and Java are built; **no desktop stand-in** reaches it. Two phones in hand |
| joining a database over a radio | **not built**: joining still needs the LAN once. [Q105](/developer_questions.md) |
| an iPhone | not started; LE GATT is the reason it can be, later |

To run it on one computer (the recipe the log's measurements used):
`HORMIGA_NO_BEACON=1` on both, a different `HORMIGA_PROFILE_DIR` each,
`HORMIGA_SIM_RADIO=listen:<port>` on one and `connect:<port>` on the other
(`,<bytes per second>` to change the 6 KB/s), then `effect lan-stay` on both;
`HORMIGA_SYNC_TRACE=1` prints every frame and its size on the air.

# Boundaries

- **No hotspot.** Not planned, not tested, by the author's ruling.
- **The radio holiday knows no Hormiga.** Tags, membership and frames are ours;
  bytes and interfaces are Void Maiz's; Reticulum is Void Palabra's.
- **Nobody outside the room is connected or shown.** A tag that does not match
  is ignored, not listed.
- **No frame before the proof.** And none to a fingerprint the members registry
  does not hold.
- **One carrier per member.** LAN or radio, never both; `Network` sees one link.
- **Nothing is asked until a person switches a radio on.**
