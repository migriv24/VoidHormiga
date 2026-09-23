---
type: Guide
title: The Antfarm from the command line
description: "Example usage, run for real on 2026-09-22 against the shipped voidhormiga-cli in a throwaway folder: building the default colony from its seed transcript, reading it (mantles, tree, links, cat, get --json), the port-index table every link needs, wiring a GitHub Pages target and a domain, rehearsing with --no-save and --dry-run-effects, the effect gate, undo within a session, and the eight gaps the session found. Written as the specification the GUI overhaul must match: every gesture here needs a GUI twin, and every GUI gesture needs one here."
tags: [status:current, audience:dev, audience:agent, confidence:measured]
timestamp: 2026-09-22T00:00:00Z
---

**Every transcript on this page was run**, not composed. It used the
`voidhormiga-cli` built from `main` at `50e1f48`, in an empty scratch folder,
so no organization's data was involved. Outputs are verbatim. Only long
absolute paths are shortened, to `…\af`. Where this page proposes something
that does not exist, it says **proposal** in the heading.

**Why this page exists** (the author, 2026-09-22): *"while we have been
changing the structure of the CLI and such, this is still important enough to
me, specifically in the development and planning for the eventual antfarm GUI
overhaul."* The CLI is the Antfarm's dependable way in today. It is also the
contract: the canvas, the inspector, a wizard and an agent must all compile to
these commands. A GUI gesture with no line here cannot be replayed, and so,
by ground rule 3, it is designed wrong.

# 0. Where you are

The CLI works on one state document. With none named, it creates an empty one
in the current folder and says so:

```
$ hormiga mantles
note: no database in this folder - creating an EMPTY demo-org.json in
        …\af
      To open an existing one instead, name it:  --state <path-to>.state.json
(no mantles)
```

**Always pass `--state` on a real database.** The warning exists because an
organization's live edits once landed in the repository's copy this way
(2026-09-13). Options that matter here:

| option | what it does |
|---|---|
| `--state <path>` | which database |
| `--script <file>` / `--atomic` | run a file of commands; with `--atomic`, as one all-or-nothing batch |
| `--repl` | read commands from stdin, in **one session** (undo works, see §7) |
| `--json` | machine-readable envelopes, for agents |
| `--no-save` | run everything, write nothing |
| `--dry-run-effects` | rehearse effects, perform none |
| `--allow-effects=<op>` | grant one effect op for this run |
| `--describe` | the capability briefing: verbs, every glyph, every effect with its consequence |

# 1. Build the default colony

A fresh database's Antfarm is written by `seed_antfarm_transcript()`
(`src/domain/seeds.hpp`) plus the two collaboration nodes. It is plain commands,
so it can be saved as a script and replayed. This is the whole default
Antfarm:

```
mantle new demo-org
mantle new antfarm
rune new org_core core
set core org_name "demo-org"
setjson core pos [430,220]
rune new hol_sqlite data-sqlite
set data-sqlite file "demo-org.db"
setjson data-sqlite pos [80,60]
rune new hol_csv import-csv
setjson import-csv pos [80,200]
rune new hol_fs_assets assets-fs
set assets-fs dir "assets"
setjson assets-fs pos [80,320]
rune new hol_html out-html
setjson out-html pos [790,120]
rune new hol_localhost out-localhost
set out-localhost port "8780"
setjson out-localhost pos [1120,120]
link core data-sqlite --relation 1:1
link core import-csv --relation 1:1
link core assets-fs --relation 2:1
link core out-html --relation 1:1
link core out-html --relation 2:2
link out-html out-localhost --relation 3:1
rune new hol_lan_share lan-share
set lan-share allow yes
set lan-share presence yes
set lan-share port 47733
set lan-share key_file lan-room.key
set lan-share private_tags private
set lan-share send_hosted no
setjson lan-share pos [790,400]
rune new hol_membership members
set members store local-file
set members file members.json
set members default_role admin
set members precedence everyone-equal
setjson members pos [1120,400]
```

```
$ hormiga --script seed.hormiga --atomic
batch applied 38 command(s)
```

That is the **text form of the Antfarm**. [The three DSLs](/concepts/foundation/dsls.md)
noted that the Antfarm "could also gain a textual DSL that converts to/from its
node graph". It has one already: its own transcript.

# 2. Read it

```
$ hormiga mantles
  demo-org
* antfarm

$ hormiga use antfarm
active mantle: antfarm

$ hormiga tree
antfarm
  - core [org_core]
  - data-sqlite [hol_sqlite]
  - import-csv [hol_csv]
  - assets-fs [hol_fs_assets]
  - out-html [hol_html]
  - out-localhost [hol_localhost]
  - lan-share [hol_lan_share]
  - members [hol_membership]
  edge core -1:1-> data-sqlite
  edge core -1:1-> import-csv
  edge core -2:1-> assets-fs
  edge core -1:1-> out-html
  edge core -2:2-> out-html
  edge out-html -3:1-> out-localhost

$ hormiga links out-html
core -1:1-> out-html (w=1)
core -2:2-> out-html (w=1)
out-html -3:1-> out-localhost (w=1)
```

One node, whole:

```
$ hormiga cat data-sqlite
{
	"spirit":	{
		"id":	"rune_84fe3ec7ba",
		"name":	"data-sqlite"
	},
	"glyph":	"hol_sqlite",
	"facets":	{ "who": "", "what": "", "when": "", "where": "", "why": "", "how": "" },
	"tags":	[],
	"content":	{
		"file":	"demo-org.db",
		"pos":	[80, 60]
	},
	"placement":	null,
	"relations":	[]
}
```

(`facets` is folded onto one line here. The CLI prints one field per line.)
`spirit.id` is the identity presence and sync use. The name is only the handle
(Q74). The position is `content.pos`, which is why moving a node on two devices
converges silently ([across devices](/concepts/platform/antfarm/collaboration.md) §4).

Fields only, for an agent:

```
$ hormiga --json get site-pages
{"ok":true,"lines":["{\"token_key\":\"\",\"token_file\":\"\",\"message\":\"\",\"repo\":\"example-org/example-org.github.io\",\"branch\":\"gh-pages\"}"],"data":{"token_key":"","token_file":"","message":"","repo":"example-org/example-org.github.io","branch":"gh-pages"}}

$ hormiga --json links out-html
{"ok":true,"lines":[…],"data":[{"from":"core","to":"out-html","relation":"1:1","weight":1,"directed":true},{"from":"core","to":"out-html","relation":"2:2","weight":1,"directed":true},{"from":"out-html","to":"out-localhost","relation":"3:1","weight":1,"directed":true},{"from":"out-html","to":"site-pages","relation":"3:1","weight":1,"directed":true}]}
```

The node kinds this build knows:

```
$ hormiga glyphs        (Antfarm rows only)
org_core     entity  host     Hormiga Core
hol_sqlite   entity  host     SQLite - local store
hol_csv      entity  host     CSV - local source
hol_supabase entity  host     Supabase - cloud store
hol_sheets   entity  host     Google Sheets - cloud source
hol_fs_assets entity  host     Local files - asset store
hol_imgbb    entity  host     ImgBB - cloud image host
hol_html     entity  host     HTML site - publisher
hol_localhost entity  host     Localhost - local server
hol_github   entity  host     GitHub Pages - cloud deploy
hol_lan_peer entity  host     LAN peer - another device
hol_lan_share entity  host     Share over LAN
hol_membership entity  host     Members
member       entity  host     Member
hol_dns      entity  host     Domain / DNS - registrar
hol_static_host entity  host     Static host - cloud deploy
hol_object_store entity  host     Object store - S3 or compatible
deployment   act     host     Deployment
hol_auth     entity  host     Sign-in - identity provider
hol_accounts entity  host     Accounts + submissions - cloud
hol_uploads  entity  host     Uploads - visitor object store
```

# 3. The port-index table (you need it for every `link`)

`--relation i:j` means port `i` on the source and port `j` on the target,
**1-based, counting a glyph's inputs and outputs together in declaration
order** ([model](/concepts/platform/antfarm/model.md) §"What an edge is").
Nothing on screen prints this table, so here it is:

| glyph | 1 | 2 | 3 |
|---|---|---|---|
| `org_core` | `records` out | `assets` out | |
| `hol_sqlite`, `hol_csv`, `hol_supabase`, `hol_sheets`, `hol_lan_peer`, `hol_lan_share`, `hol_membership` | `records` in | | |
| `hol_fs_assets`, `hol_imgbb`, `hol_object_store`, `hol_uploads` | `assets` in | | |
| `hol_html` | `records` in | `assets` in | `site` out |
| `hol_localhost` | `site` in | | |
| `hol_github`, `hol_static_host` | `site` in | `domain` in | |
| `hol_dns` | `domain` out | | |
| `hol_auth` | `identity` out | | |
| `hol_accounts` | `records` in | `identity` in | |

So: records to a store is `1:1` from `core`, assets to a file store is `2:1`,
the publisher takes `1:1` and `2:2`, and a site reaches a host as `3:1`.

# 4. Wire a deploy target

Add GitHub Pages, point it at a repository, and plug the publisher's `site`
into it:

```
$ hormiga rune new hol_github site-pages
created rune 'site-pages' (glyph hol_github)
$ hormiga set site-pages repo example-org/example-org.github.io
site-pages.repo = example-org/example-org.github.io
$ hormiga set site-pages branch gh-pages
site-pages.branch = gh-pages
$ hormiga link out-html site-pages --relation 3:1
link out-html -3:1-> site-pages (w=1)
```

The token is **never** set as a value. Either name a vault entry (`set
site-pages token_key github-pages`, filled from the GUI's vault), or name a
gitignored file beside the database (`set site-pages token_file
github.token`). The credential never reaches a command line, the log or a
transcript.

A domain, feeding the host's second port:

```
$ hormiga rune new hol_dns org-domain
created rune 'org-domain' (glyph hol_dns)
$ hormiga set org-domain domain example.org
org-domain.domain = example.org
$ hormiga set org-domain provider cloudflare
org-domain.provider = cloudflare
$ hormiga link org-domain site-pages --relation 1:2
link org-domain -1:2-> site-pages (w=1)
$ hormiga links site-pages
out-html -3:1-> site-pages (w=1)
org-domain -1:2-> site-pages (w=1)
```

And taking it out again:

```
$ hormiga unlink org-domain site-pages
unlinked 1 link(s): org-domain -> site-pages
$ hormiga rm org-domain
removed org-domain
```

# 5. Rehearse before you change anything

`--no-save` runs the command and writes nothing:

```
$ hormiga --no-save rune new hol_object_store bucket
created rune 'bucket' (glyph hol_object_store)
$ hormiga ls
core
data-sqlite
import-csv
assets-fs
out-html
out-localhost
lan-share
members
site-pages
```

`bucket` is not there. The rehearsal was real and it was not kept.

# 6. The effect gate

Everything that reaches outside the document is an `effect`, and an effect is
refused unless it was granted:

```
$ hormiga effect host-online missing
refused: `effect` reaches outside the document and this session was not granted
effects. Re-run with --allow-effects=effect if that is intended, or
--dry-run-effects to rehearse it.
```

Rehearsed, each op says what it would do in words written for the person
deciding whether to grant it:

```
$ hormiga --dry-run-effects effect host-online missing
dry run: `effect` would run — Put an image online through the Antfarm's image
host - ImgBB, an S3 or R2 object store with a public_url, or the website's own
host - and write the link into its `url`. Args: <image-rune|missing>
[<host-node>]. `missing` = every image with a file and no link. (UPLOADS the
image to the host the Antfarm names, where anyone with the link can see it;
through a website host the file is copied into site/ and goes live with the
next publish). Nothing was performed.

$ hormiga --dry-run-effects effect deploy-site
dry run: `effect` would run — Upload the built site/ folder through an Antfarm
hol_static_host node. Records the publish as a `deployment` rune in the antfarm
mantle. (PUBLISHES THE WEBSITE. Whatever is in site/ becomes the live page
everyone sees, immediately, and nothing in this application can take it back).
Nothing was performed.

$ hormiga --dry-run-effects effect check-host site-pages
dry run: `effect` would run — Can these credentials publish to this host?
Performs the smallest real reads a deploy performs - the repository or
project, the branch, and whether the host is serving it. Args: [<host-node>].
(reads your publish target's settings; writes nothing and publishes nothing).
Nothing was performed.
```

Granting is **per operation**. Granting the verb is not enough:

```
$ hormiga --allow-effects=effect effect check-host site-pages
refused: `effect` reaches outside the document and this session was not granted
effects (check-host: reads your publish target's settings; writes nothing and
publishes nothing). Re-run with --allow-effects=check-host if that is intended,
or --dry-run-effects to rehearse it.
```

So `--allow-effects=check-host` is the grant. (The real check was not run on
this page: it would have sent a request to GitHub for a repository that does
not exist.)

**The effects that act through Antfarm nodes:**

| op | node | the grant means |
|---|---|---|
| `check-host [<node>]` | `hol_github`, `hol_static_host` | read-only calls to the host |
| `check-store` | `hol_object_store` | lists one key |
| `host-online <image\|missing> [<node>]` | the chosen image host | uploads, and writes `url` with one `set` |
| `deploy-site` | `hol_static_host` | **publishes the website** |
| `rollback-site <node> <deployment>` | `hol_static_host` | **changes the live website** |
| `push-store <index\|backup> [<node>]` | `hol_object_store` | uploads the index, or the sealed backup |
| `lan-offers [s]`, `lan-peers [s]` | none | listens, or announces on this subnet only |
| `lan-share [s] [approve]` | `hol_lan_share` | with `approve`, sends the database and **the Antfarm's keys** |
| `lan-stay [s]`, `lan-join <addr> <port> <folder>` | `hol_lan_share` | stay in sync, or join |
| `save` | `hol_sqlite` | rewrites the SQLite mirror |

# 7. Undo lives in a session

Each CLI invocation is its own session, and Void Core's undo is per session:

```
$ hormiga undo
nothing to undo
```

To work with undo, use one session:

```
$ printf 'set site-pages branch main\nget site-pages\nundo\nget site-pages\n' | hormiga --repl
site-pages.branch = main
{"token_key":"","token_file":"","message":"","repo":"example-org/example-org.github.io","branch":"main"}
undid 1 change(s)
{"token_key":"","token_file":"","message":"","repo":"example-org/example-org.github.io","branch":"gh-pages"}
```

Across invocations, the checkpoint is **the last save**: `status` and `diff`
list what changed since it (`added 9, changed 0, removed 0` here), and `revert`
answers `reverted to last save`. Use `--no-save revert` to see what it would do
first. For anything finer, keep the transcript and replay it.

# 8. What the session found (2026-09-22)

Eight things, in order of how much they matter to the redesign. Each is
reproduced above or with a one-line command.

1. **Ill-typed links are accepted.** `records` into a `site` port:

   ```
   $ hormiga link core site-pages --relation 1:1
   link core -1:1-> site-pages (w=1)
   ```

   and a port that does not exist:

   ```
   $ hormiga link core site-pages --relation 9:1
   link core -9:1-> site-pages (w=1)
   ```

   and `validate` still passes afterwards:

   ```
   $ printf 'link core site-pages --relation 9:1\nvalidate\n' | hormiga --no-save --repl
   link core -9:1-> site-pages (w=1)
   valid
   ```

   The canvas refuses both. The dispatcher, which is the only door, refuses
   neither. **A type the door does not check is not a type.** An agent can
   build an Antfarm the GUI could never draw.
2. **No code reads the wiring.** Consumers find nodes by glyph. `site-pages`
   would publish with or without its `3:1` edge
   ([model](/concepts/platform/antfarm/model.md)).
3. **Edges print as indices.** `core -2:2-> out-html` says nothing about
   assets. Neither `links`, `tree` nor `describe` names a port.
4. **Nothing says what a node kind is.** `glyph hol_github` prints a usage
   line (`glyph` declares, `glyphs` lists). There is no verb that shows a
   kind's ports, fields and what it answers. `--describe` has it all, in one
   2,708-line JSON document.
5. **Nothing says whether a node is ready.** `describe out-html` shows the six
   facets, all empty. The readiness each face computes (key present, rescue
   dump found, *can host images now*) is GUI-only. The only CLI route is a
   `check-*` effect, which goes to the network.
6. **Two Antfarm effects have no CLI twin.** `import-rescue` (the Supabase
   node's *Import now*) and `serve-site` (the localhost node) are answered by
   the GUI host only.
7. **`ls --glyph hol_github` ignores its filter** and lists every rune.
8. **`effect` alone prints `usage: effect <op> [args...]`**, not the list of
   ops. The list is in `--describe`.

# 9. Proposal: what the CLI could say after the redesign

**Not built. Nothing below exists.** It is written so that the GUI overhaul
can be designed from its CLI outward, the way the rest of this application
was. Every proposed verb compiles to existing Core verbs (`rune new`, `set`,
`link --relation`), so replay and the log are unchanged. The names are
placeholders for the author to rename.

```
$ hormiga farm
Connections — demo-org                         (this device: maria-laptop)
  Keeping the data      data-sqlite     SQLite (local)        ready · 1,204 runes · saved 2 min ago
  Keeping files         assets-fs       Local files           ready · 381 files
  Putting images online site-pages      GitHub Pages          needs: token (vault or token_file)
  Publishing the site   out-html → out-localhost, site-pages  local preview ready · Pages not ready
  Sharing               lan-share       Share over LAN        ready · 2 members present
  Importing             import-csv      CSV                   ready

$ hormiga farm ports out-html
out-html  [hol_html · HTML site - publisher · local, pure]
  in   records   ← core.records
  in   assets    ← core.assets
  out  site      → out-localhost.site, site-pages.site

$ hormiga plug out-html.site site-pages.site
link out-html -3:1-> site-pages          (site → site)

$ hormiga plug core.records site-pages.site
refused: core.records carries `records`; site-pages.site takes `site`.
  Things that take `records`: data-sqlite, import-csv, out-html.records, lan-share, members

$ hormiga farm check
data-sqlite   ready
site-pages    not ready: no token (set token_key, or put github.token beside the database)
lan-share     ready

$ hormiga --allow-effects=host-online ask host-file flier-3
https://example-org.github.io/assets/3fa9…c1.jpg   (site-pages · works after the next publish)
```

What each one earns:

- **`farm`** is the dashboard the July design asked for ("status at a glance,
  plain language") and the phone's entire Antfarm
  ([mobile](/concepts/sections/mobile.md)). Grouped by the question a node
  answers, not by payload.
- **`farm ports`** and **`plug a.port b.port`** replace the index table in §3.
  `plug` checks types *before* dispatch and says what would fit. That is the
  door check of finding 1, applied at the host until Void Core applies it for
  everyone.
- **`farm check`** is readiness without the network: `host_problem` for every
  node.
- **`ask <capability> <args>`** is the program-callable seam Allomone needs,
  and the CLI twin of every "host it online" button
  ([capabilities](/concepts/platform/antfarm/capabilities.md)).

How findings 1 to 8 map onto this: 1 → `plug` (and an upstream ask), 2 →
redesign A2, 3 → `farm ports`, 4 → `farm ports` on a kind, 5 → `farm check`,
6 → the two ops join the CLI's effect table, 7 → a bug to fix, 8 → `effect`
with no op lists them.
