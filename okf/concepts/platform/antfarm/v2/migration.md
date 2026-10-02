---
type: Guide
title: Migrating an Antfarm from v1 to v2
description: "For agents and people with a v1 database (written 2026-09-28, before any of it is built, so it can be built to): why v2 is a new version and not an upgrade in place, the migration command and what it does step by step (a backup first, then one batch), the glyph-by-glyph mapping, the v1 script idioms and their v2 lines, what does not migrate, how mixed-version members are kept apart, and how to verify and undo."
tags: [status:direction, audience:agent, audience:dev, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Built 2026-10-01** (`farmhost::migrate_plan`; `farm migrate`, `farm migrate
apply`, in the CLI and the command bar), with these departures from the plan
below, each for a reason:

- **The v1 `antfarm` mantle is neither tagged `superseded` nor removed, and
  `retire-v1` is not built.** LAN sharing (`hol_lan_share`, `hol_membership`) and
  the Publish tab still read it, so it must stay in force until they read v2. The
  rehearsal lists those nodes under *STAYS IN V1*.
- **The Assets, Network and Documents chambers come from the reconcile** that
  every database runs (the chambers page), not from the migration, so step 3 to 5
  below are already true before it runs.
- **A v1 peer is not refused by version.** The handshake carries no version yet.
  Instead, once the Miga node records `migrated_from`, v2 members stop sharing the
  v1 mantle (`lan_net.cpp`), so a member still on v1 cannot change what v2 members
  run on. The hard refusal needs a version token in Void Maiz's handshake.
- **The rescue import (`hol_supabase`) is not migrated**: v2 has no kind for it
  yet. It is listed under *NOT MIGRATED*.

Measured on a replay of v1's default colony plus a GitHub host with a key file:
34 commands, every node mapped, the key file sealed into the vault and its
key node *ready*. The rest of this page is the plan as written.

# 1. Why this is a new version

The author, 2026-09-28:

> since this is a NEW design for the antfarm (different socket types and such),
> it is valid to consider this a NEW version of the antfarm, internally as well.
> i don't think old versions of the antfarm will be compatible with this new
> version […] we should also be considering the documentation for agents, to
> describe how to update their old antfarm into this newer version.

v2 changes the types on the wires, names ports instead of numbering them,
moves files into an Assets chamber and members into a Network chamber, and
shares keys. A v1 graph cannot be read as a v2 graph. Three things still hold
from ground rule 3 and bound how the break is made:

1. **Old logs must still replay.** A v1 transcript says
   `link out-html out-localhost --relation 3:1`. The v1 glyphs therefore stay
   **registered, as legacy**: they are not in the palette and nothing new uses
   them, but a replay finds them.
2. **The migration is a command, not a file rewrite.** It is logged,
   attributed, rehearsable and undoable like any other change.
3. **Nothing is lost silently.** Whatever does not migrate is listed, by name,
   in the rehearsal.

# 2. The two graphs, side by side

v2 lives in a **new mantle, `farm`**. The v1 `antfarm` mantle is not edited by
the migration. After migrating, it is tagged `superseded`, read by nothing, and
kept, so the migration can be compared and undone. `farm migrate retire-v1`
removes it once the organization is satisfied.

**The migration is offered only when v2 can do everything v1 does** (the end of
[phase](/concepts/platform/antfarm/v2/phases.md) V6). Until then a v2 graph can
be built only in a database created with v2 enabled, for development and the
Cat Dataset. No organization runs half its connections on each version.

# 3. What `farm migrate` does

```
$ hormiga --state org.state.json farm migrate
```

rehearses and prints every step below with counts. With `apply`, it:

1. **Writes a sealed backup first** (`backup-database`), and names it in its
   report. The backup is the undo that survives the session.
2. **Builds the graph** from the v1 nodes (§4), with every port by name.
3. **Creates the Assets chamber.** One `asset` rune per distinct file, from the
   content hashes files have carried since 2026-09-19. Every `image` rune gains
   `asset`. Every image-typed field (`avatar`, a flier's picture) that held a
   path or a URL is set to its asset. A file known only by a remote URL
   becomes an asset with its public link and **no bytes held**, and the
   Data→Assets tunnel reports it as *unheld*, so the mirror can fetch it.
4. **Creates the Network chamber** from `members.json` (one `profile` per
   member, keeping id, key fingerprint, username, colour, role) and the paired
   `peer` runes. `members.json` is left where it is, unread.
5. **Creates the Documents chamber.** One `document` rune per Builder document
   mantle; one calendar document from the `calview` runes; one map document per
   map, holding its view runes.
6. **Moves credentials into keys.** A `token_key` or `secret_key` vault entry
   becomes a key rune naming it. A `token_file`, `key_file` or `secret_file` is
   read, sealed into this device's vault, and becomes a key rune. **The file is
   not deleted.** The report lists each one so a person can delete it after
   checking.
7. **Carries configuration over.** `config hosting.images` names the new river.
   `deployment` runes stay, and gain the domain node that replaced their host.
8. **Stamps the version**: `farm version` answers `2`.

Steps 2 to 8 are **one batch**, one undo frame.

# 4. The mapping, glyph by glyph

| v1 glyph | v2 |
|---|---|
| `org_core` (Hormiga Core) | **Miga** + **Separate chambers** |
| `hol_sqlite` | the **home** river's **Folder** reservoir, `live`; `Miga.rests-in` |
| `hol_fs_assets` | the same Folder reservoir (`dir` becomes its `path`) |
| `hol_csv` | **Import CSV**, wired into the Data chamber's `import` |
| `hol_supabase` | **Import rescue** (a source; *Import now* becomes `farm run`) |
| `hol_sheets` | **Import Sheets**, *planned* |
| `hol_imgbb` | **Image host** reservoir with a **Key**, inside a `photos-online` river |
| `hol_object_store` | **Bucket** reservoir with a **Key**; `public_url` kept |
| `hol_uploads` | *planned* (visitor uploads, the inbox plane) |
| `hol_html` | one **document** per Builder document; the publisher is no longer a node |
| `hol_localhost` | **Local domain**, placed `each` |
| `hol_github` | **Web domain** (GitHub Pages) with a **Key**; `repo`, `branch`, `message` kept |
| `hol_static_host` | **Web domain** (Cloudflare Pages and kin) with a **Key**; `deploy_cmd` and `rollback_cmd` kept |
| `hol_dns` | the web domain's custom name; its token becomes a **Key** |
| `hol_lan_share` | the **Network** chamber's sharing settings (allow, presence, port, private tags); the room key becomes a key rune of kind `room` |
| `hol_membership` | the **Network** chamber |
| `hol_lan_peer` | removed: peers are profiles |
| `hol_accounts`, `hol_auth` | *planned*, unchanged in intent |
| `hol_device`, `hol_device_paths` | the device facet of this device's **Profile** |
| `poly_router` | **Reroute** |
| `math_*`, `str_*` | **not migrated** (test nodes); listed in the report |
| `deployment` | kept as is, linked to its domain |

**What does not migrate, and is listed:** the test nodes; any wire whose ports
did not exist or whose types did not match (v1 accepted those, see
[CLI §8](/concepts/platform/antfarm/cli-examples.md)); and any v1 node nothing
read and that the person never configured.

# 5. v1 idioms, and their v2 lines

For agents updating scripts and notes written against v1:

| v1 | v2 |
|---|---|
| `rune new hol_github site-pages` + `set site-pages repo …` | `farm add web-domain site-pages host=github repo=…` |
| `link core site-pages --relation 1:1` (wrong, and accepted) | refused; `farm plug site.publish site-pages.in` |
| `link out-html out-localhost --relation 3:1` | `farm plug site.preview preview-here.in` |
| `set imgbb key_file imgbb.key` | `farm key add imgbb imgbb-main` (value at the prompt), `farm plug imgbb-main.key photos-imgbb.key` |
| `config hosting.images site-pages` | unchanged: it now names a river, and the migration rewrites it |
| `effect check-host site-pages` | `farm check site-pages` |
| `effect deploy-site` | `farm publish site` |
| `effect host-online flier-3` | unchanged (a capability call); it now answers from a river |
| `cat core` / `links core` to read the graph | `farm show this-db`, `farm` |

# 6. Mixed versions on one network

**The dangerous case.** The Antfarm syncs between members (since 2026-09-25).
A member still on v1 would keep editing the `antfarm` mantle that the others
have superseded, would receive key runes it cannot use, and would never see the
Assets chamber its images now point into.

So **a v2 member refuses to sync a migrated database with a v1 member**. The
check has to live on the v2 side, because v1 builds cannot be changed after the
fact. The sync handshake carries the Antfarm version. A v1 peer is refused with
the reason, and both sides see the same sentence:

> This database moved to Antfarm v2 on 2026-11-02 (by ana, on office-pc).
> Update Hormiga on this device to keep syncing it.

The update client already knows how to tell the person that a newer version
exists.

# 7. Verifying, and undoing

After `farm migrate apply`:

1. `farm status`: every node's readiness, with no network. Nothing should be
   *failing*. *Needs* is expected on this device for keys whose values live on
   another member's device.
2. `farm check <node>` for each Surface node, with its grant
   (`--allow-effects=farm-check`, the op as built). This is the only step that
   goes to the network.
3. The Data→Assets tunnel: *Missing* should be 0, and *remote only* lists
   files the mirror can fetch. **As built, read it with `farm show pictures`**;
   `farm check pictures` answers "has no check yet" (2026-09-29).
4. Open the Connections view and compare it with the v1 tab. They should say
   the same things in different words.

**To undo:** `undo` in the same session reverses the batch. After the session
ends, restore the backup from step 1 (`restore-database`).

# 8. Until `farm migrate` exists: by hand, on a copy

What an agent with a v1 database can do today, as done on 2026-09-28 against a
partner organization's database (findings in the
[index](/concepts/platform/antfarm/v2/index.md) §"Found on a copy of a real
database"):

1. **Back up the real state document first**, and hash it, so "untouched" can
   be proven afterwards.
2. **Copy it into a folder of its own**, with the assets it names and **no
   credential files**. Key nodes then say *needs*, `farm publish` refuses, and
   the copy cannot overwrite a live site. Keep LAN sharing off if the copy is
   opened in the application: it has the original's identity.
3. Run everything through `voidhormiga-cli --state <absolute path> farm …`.
   **Do not open the original in a dev desktop build** while
   [Q98](/developer_questions.md) is open: the first frame adds the chambers.
4. `farm init`, then **correct the website's `document`** (init may pick a
   leftover), then translate §4 line by line, as a script so it replays.
5. **Put a consent filter in front of the map** before wiring it anywhere
   (finding 1 in the index).
6. Compare `farm preview <website>` with v1's `effect render-site <doc>` from
   the same binary, ignoring `?v=` stamps and `.ics` DTSTAMPs. They should be
   identical.

What a real v1 graph brought that §4 does not yet say how to translate:

- **one credential file named by two v1 nodes** (a static host and its DNS):
  one key, two consumers, which v2 allows; the migration should mint one key;
- **an object store with no bucket** that names a vault entry, not a file: it
  translates to a bucket that is honestly *unset*;
- **two `hol_sheets` nodes**, one empty: `import-sheets` is *planned*, so the
  configured one is kept as a placeholder and the empty one dropped;
- **`config hosting.images` naming the ImgBB node**, which becomes a reservoir
  inside a river: the config must be rewritten to the river's name (step 7 of
  §3), or the capability answers from nothing.
