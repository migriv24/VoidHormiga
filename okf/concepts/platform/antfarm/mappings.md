---
type: Research
title: How the outside APIs map onto the Antfarm
description: "Research for the redesign (2026-09-22). The pivot each payload should have (rune JSON, the content address, a site manifest, a zone, issuer+subject, VEVENT, an RFC 5322 message), how each family of vendor API maps onto its pivot and where each one loses information, the common anatomy of a vendor API (auth, writes, listing, idempotency, dry-run, rollback) and what it forces on node design, and prior art in other tools: Terraform, Airbyte/Singer, Node-RED/n8n, NiFi, Home Assistant, rclone."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

**How to read this page.** The sections on Hormiga's own design are measured
against the code. **The vendor API facts and the prior art are recalled, not
re-checked against current vendor documentation on 2026-09-22.** They are
accurate to the best of this session's knowledge and good enough to design
against, but a holiday is written against the live documentation, never
against this page. Where a detail is load-bearing for a decision, it says so.

# 1. The problem the Antfarm solves

An outreach organization touches perhaps a dozen external systems: somewhere
records live, somewhere images are served, somewhere the website is published,
a domain, a sign-in, calendars, email. Each has its own API. Written directly,
every *use* of every *system* is an adapter: N systems × M uses. The Antfarm's
answer has two halves:

- **Payload types** cut M down. There are six things that flow (records,
  assets, site, domain, identity, and records-between-devices), not one per
  feature. See [the model](/concepts/platform/antfarm/model.md).
- **Pivots** cut N down. Each payload has *one* canonical shape inside
  Hormiga, and every vendor maps to that shape, never to another vendor. This
  is the pivot rule from [Void Reyna](/concepts/projects/void-reyna.md): *never
  write a direct A→B holiday when A→pivot→B exists.*

Adapters then cost N + M, and if every leg round-trips, the composite does too.

# 2. The pivot for each payload

| payload | the pivot (Hormiga's canonical shape) | exists in code? |
|---|---|---|
| `records` | **the rune**, as in the state document: `spirit.id`, `glyph`, `tags`, `content`, `relations` | yes, Void Core's |
| `assets` | **the content address**: `sha256 → bytes`, plus a media type | yes, since 2026-09-19 |
| `site` | **a site manifest**: `path → (sha256, size)`, plus the bytes | **no**; a folder on disk is the de facto one |
| `domain` | **a zone**: a name plus its records | partly (`hol_dns` fields) |
| `identity` | **(issuer, subject)**, never an email address | named in the web platform, not built |
| dated runes | **VEVENT** (RFC 5545), with its `UID` | yes, the calendar hub's lens |
| email (future) | **an RFC 5322 message** plus a recipient set | no |
| geometry (Territory) | **unnamed**, the real work item inside Q32 | no |

**The one missing pivot that matters most is the site manifest** (§4.4). Every
deploy target Hormiga supports or plans, whether GitHub, Cloudflare Pages or
Netlify, is already shaped like "a set of paths with hashes". Naming that shape
would make each deployer a thin transport, make "what changed since the last
publish" a diff instead of a re-upload, and give each `deployment` rune a
content-derived id to record: provenance, the way Void Core's `materialize`
stamps it.

# 3. Records: row stores

**The mapping is the same for every row store**, and so are its failure modes:

| rune | row store |
|---|---|
| glyph | table (or sheet tab) |
| rune | row |
| field | column |
| `spirit.id` | primary key. **Not the name**: names collide across devices (Q74), and an id does not |
| tags | a join table, an array column, or one delimited text column |
| relations | a foreign key, or an edge table (`from`, `to`, `label`, `weight`) |
| facets | columns, or dropped |

**Where information is lost**, which is where round-trip tests must look:

- **Tags** in a delimited column lose order and anything containing the
  delimiter. Portfolio Manager shipped exactly this bug, with read, write and
  persistence written separately. It is the reason to write the mapping once.
- **Types.** A row store types columns and runes do not. Dates are the usual
  casualty (`2026-09-22` becomes a serial number in Sheets).
- **Identity in spreadsheets.** A Sheets row has **no stable id**, only a row
  number that shifts when someone inserts a row. A two-way Sheets holiday must
  own an id column, or it is import-only. That is the technical content of
  Q4's "import-only lean".

| system | how it is reached | notes for the holiday |
|---|---|---|
| **SQLite** | in-process SQL (vendored amalgamation) | the default store. Tags and links are join tables |
| **Supabase / Postgres** | PostgREST over HTTPS: `GET /rest/v1/<table>?select=…&col=eq.v`, upsert by `POST` with `Prefer: resolution=merge-duplicates`; `apikey` plus bearer headers | today a copy-only rescue *import*. Row-level security is the vendor's permission model and must not be confused with ours |
| **Google Sheets** | API v4: `spreadsheets.values.get` / `update` / `append` over A1 ranges; OAuth 2 or a service account | every cell is a string. No row id. OAuth secrets cannot ship in a public repo ([Q69](/developer_questions.md)) |
| **CSV** | a file | the degenerate row store: one table, no types, no id unless a column is declared one |
| **a LAN peer** | Void Palabra frames over a sealed session | not a row store. It exchanges runes whole, which is why it needs no mapping at all |

# 4. The other payloads, family by family

## 4.1 Assets: object stores (`assets ⇄ world`)

**S3 and everything compatible with it** (Cloudflare R2, MinIO, Backblaze B2's
S3 endpoint): `PUT`/`GET`/`DELETE` on a key, `ListObjectsV2`, every request
signed with **SigV4**. `hol_object_store` already handles this family with one
`endpoint` field: blank means AWS, anything else means compatible, and the
signing is identical.

- **The API endpoint and the public address are different things.** R2
  answers the API at `<account>.r2.cloudflarestorage.com`, and serves the
  public at an `r2.dev` address or a custom domain. That is why the node grew
  `public_url` on 2026-09-15, and why `public_url` is what "host it online"
  needs.
- **The mapping to the pivot is exact.** The key is `prefix + sha256 +
  extension`, so a `PUT` is idempotent, a second upload of the same file is a
  no-op, and any object store can replace any other.

## 4.2 Assets: image hosts (`asset → url`, one way)

**ImgBB**: `POST /1/upload?key=…` with the image (base64 or a URL), and an
optional expiry. It answers with `url`, `display_url` and a `delete_url`. There
is **no listing and no fetch-by-id**. A host is not a store: you cannot ask
ImgBB what it holds. That is why the shape table in
[holidays](/concepts/platform/antfarm/holidays.md) separates *host* from *file
store*, and why the mirror exists. The only record of what was hosted is the
`url` field Hormiga wrote. **`delete_url` is a capability, and today it is
thrown away.** A redesign that wants "take this image offline" needs to keep
it, which means it is a secret-shaped value (whoever holds it can delete) and
belongs in the vault, not a field.

## 4.3 Assets: the website as a host

A static host or GitHub Pages "hosts" an image by copying it into
`site/assets/`. The answer is a URL that works **after the next publish**
(`after_publish`). No other API does this, and it is the only one that needs
no extra account. It works because a site is a folder, which is §4.4's pivot.

## 4.4 Site: static deploys (`site × domain → world`, returns a deployment)

The three deploy APIs look different and are the same underneath:

| target | how a deploy happens | rollback |
|---|---|---|
| **GitHub Pages** | Git Data API: create blobs, create a tree (optionally on a base tree), create a commit, move the branch ref (`PATCH /git/refs/heads/<branch>`). The Pages settings say which branch is served | move the ref back |
| **Cloudflare Pages** | direct upload: a manifest of paths and content hashes, then only the files the service lacks; a deployment id comes back | re-promote a past deployment id |
| **Netlify** | `POST` a deploy with `{path: sha1}` for every file, receive the list of hashes it needs, `PUT` those | restore a past deploy id |

**All three are "here is a manifest of path → hash, send what is missing, get an
id".** Git's blob hash is SHA-1 over a header plus the bytes, Netlify's is SHA-1
of the bytes, and Hormiga's asset address is SHA-256. So a site-manifest pivot
stores SHA-256 and each transport computes what its vendor wants. That is
cheap, and it is the one place a holiday does the vendor's arithmetic.

What the pivot buys, concretely:

- **Incremental publishes.** Unchanged files are not sent. `hol_github` today
  uploads **every file as a new blob on every publish**, then builds the full
  tree with no `base_tree`. The second half is deliberate and right
  (`publish/github.cpp`: with a base tree, a page deleted from the model would
  stay live forever, and without one the deploy is a mirror). A manifest keeps
  the mirror and drops the waste: a blob whose git hash the previous tree
  already holds need not be uploaded again. The full tree can name it.
- **A reviewable diff before a publish:** "3 pages changed, 1 image added".
  That is what the confirmation dialog should show beside the consequence
  sentence.
- **Honest deployment records.** A `deployment` rune stores the manifest's
  hash, so "is what is live what we published?" is a comparison, not an
  investigation.

## 4.5 Domain: DNS (`→ domain`)

Registrars sell the name, and **DNS providers host the zone.** Often one company
is both (Cloudflare, Porkbun), and the node keeps them separate
(`provider`, `zone_id`), because moving the zone and moving the registration are
different operations. The APIs are record CRUD on a zone (Cloudflare:
`/zones/<id>/dns_records`). **Buying a domain stays outside the Antfarm**: it
is a purchase, not a protocol (2026-07-16, still true).

What flows on the `domain` port is "this name, and the right to set records in
its zone". Consumers are deployers (a CNAME to the host). **Email will be a
consumer too.** Sending a newsletter from the org's own domain needs SPF, DKIM
and DMARC records in that zone, so `hol_dns.domain` will feed an email sender
the same way it feeds a static host. The port type was right before its second
consumer existed.

## 4.6 Identity: sign-in (`→ identity`)

OpenID Connect providers (Google first) hand back an ID token whose **`iss`
and `sub` together** identify a person stably. **Email addresses are not
identities.** They change, get recycled, and one person has several. The
[identity page](/concepts/platform/identity.md) already separates the profile,
the signed-in identity and the contact. The mapping rule for any provider is
that the pivot is `(issuer, subject)`, and the claim that this identity *is*
contact X is Hormiga's data, reached through `hol_accounts`. Swap the provider
and the claims survive. That is disposability applied to login.

## 4.7 Dated runes: calendars

The calendar hub is the Antfarm's best-developed pivot, even though it does not
yet present as Antfarm nodes:

| system | reached by | incremental? |
|---|---|---|
| an `.ics` feed | HTTP GET, read-only | no; re-read and diff by `UID` |
| CalDAV (RFC 4791) | `REPORT` queries, per-event ETags | yes, by ETag |
| Google Calendar | REST, OAuth; events carry `iCalUID` | yes, by `syncToken` |

Every one maps through **VEVENT ⟷ rune**, keyed on `UID`. That is the single
lens the [calendar roadmap](/concepts/sections/calendar-roadmap.md) insists on.
**The redesign's open question here** is whether calendars become Antfarm
nodes (a `hol_ics` source, a CalDAV store) or stay a section's own settings.
The payload is `records`, so the port already exists. The lean is nodes,
because "where does this calendar come from" is exactly the question the
Antfarm exists to answer visibly.

## 4.8 Email dispatch (planned: "Courier")

**SMTP** (the protocol), or a **transactional API** (Amazon SES, Postmark,
Resend and similar), each taking a message and recipients. Two facts shape the
design more than the API does:

- **Bulk senders must authenticate the domain** (SPF, DKIM, DMARC), and the
  large mailbox providers have required one-click unsubscribe for bulk mail
  since 2024 (the `List-Unsubscribe` header, RFC 8058). So the email holiday
  needs the `domain` payload, and the unsubscribe list is organization data
  that must flow back in, as a records source.
- **The recipient list is admin-plane data leaving the device.** That is the
  most sensitive crossing the Antfarm will ever make. It goes through the render
  seam like a publication, and its capability row must say what leaves
  ([capabilities](/concepts/platform/antfarm/capabilities.md)).

## 4.9 Models (`model`, reserved)

A hosted API (a messages-style endpoint: a model id, a system prompt, turns)
or a local endpoint (Ollama-class, over HTTP on localhost). The pivot is
**prompt + context → text (+ proposed commands)**. Anything a model proposes
must arrive as *proposed* dispatcher commands, like a website submission,
never applied directly. The privacy seam applies to the context sent. This is
the one payload where "local" and "cloud" differ most for a person, so its
locality badge will matter most.

# 5. The anatomy of a vendor API

Across the families, every vendor API varies along the same axes. **This table
is what node design should be derived from**, because each axis becomes a
field, a check, or a limitation the GUI must say out loud.

| axis | the range | what it forces on a node |
|---|---|---|
| **auth** | API key (ImgBB); bearer token (GitHub fine-grained PAT, Cloudflare); OAuth 2 with refresh (Google); request signing (S3 SigV4) | a *credential reference* (vault key or file), never a value. OAuth needs a refresh flow and a place for the refresh token, which is also a secret |
| **writes** | idempotent put-by-key (S3); create-returns-id (ImgBB); a commit (GitHub); manifest-then-upload (CF, Netlify) | content addressing makes retries safe. Create-returns-id is not idempotent, so a retry duplicates |
| **listing** | full (S3, Git trees); none (ImgBB); paginated with cursors (most REST) | whether *mirror* and *check* are even possible |
| **incremental read** | none (`.ics`, CSV); ETag (CalDAV); sync token (Google); a cursor column (SQL) | where the cursor lives. It is holiday cache, not model truth, so it goes in the config tier or beside the database, never in a rune field |
| **dry run** | none (most); plan-like (Terraform-style tools); partial (a manifest diff) | Hormiga's `--dry-run-effects` can only rehearse as honestly as the API allows. A dry run that cannot see the far side must say so |
| **rollback** | ref move (git); re-promote an id (CF, Netlify); none (ImgBB, email) | whether a `deployment` rune can offer *Restore* |
| **limits** | rate limits, size limits (ImgBB's per-file cap), quotas | a face must show "failing: rate limited" as distinct from "misconfigured" |
| **consistency** | immediate (S3 now); eventual (DNS propagation, CDN caches) | "done" and "visible" are different states. `after_publish` is the first instance of saying so |

**The design consequence:** every node's fields decompose into three groups,
and the GUI should show them as three groups:

1. **target**: where (a repo and branch, a bucket, a zone, a URL);
2. **credential**: a reference to a secret, with presence shown and never the
   value;
3. **behaviour**: options (a commit message, a key prefix, send hosted files
   or not).

And every holiday implements three operations: **check** (can I reach it with
these credentials, no side effects: today's `check-host` and `check-store`),
**plan** (what exactly would this do: the dry run, as specific as the API
allows), and **apply**. That is Terraform's shape, and it earned its place
there for the same reason it would here: a person should see what will change
before it changes.

# 6. Prior art: how other tools do this

Recalled from general knowledge and not re-checked for this page. None of it
bears on correctness here. It bears on which patterns people already
understand.

| tool | its shape | take | refuse |
|---|---|---|---|
| **Terraform / OpenTofu** | providers (≈ holidays) declare schemas; resources (≈ nodes) are configured; `plan` shows a diff, `apply` performs it; a state file records what exists | check / plan / apply; drift ("the host no longer matches what we published") as a first-class state | a DSL for arbitrary infrastructure. Hormiga's vocabulary is closed on purpose |
| **Airbyte / Singer** | sources and destinations; a *catalog* of streams with schemas; sync modes (full refresh or incremental with a cursor); state saved between runs | incremental sources with a saved cursor; a source *declares its streams* (Sheets declares its tabs) | a warehouse as the centre. Our centre is the rune |
| **Node-RED / n8n** | flow graphs of nodes, with **credentials as separate objects referenced by id**, encrypted with an instance key | exactly our vault references, and the argument for making a credential its own visible thing that several nodes can share | function nodes (arbitrary code in the graph). The Antfarm runs no logic |
| **Apache NiFi** | typed flow files, back-pressure, full **data provenance** for every item | `deployment` runes and the log already are provenance. Show it on the node ("last published 2 days ago, 14 files") | its scale and its server |
| **Home Assistant** | integrations added through a *config flow* (a wizard asking only what that integration needs), and entities that report *availability* | the "add a connection" wizard, and readiness as a first-class, per-node state | discovery that adds things without asking. Every node is added knowingly |
| **Zapier / IFTTT** | a *Connections* page (accounts) separate from the automations that use them | the dashboard framing: "what is this organization connected to", listed plainly, before any graph | being the logic layer. That is Allomone's job |
| **rclone** | one interface over dozens of storage backends; "remotes" are configured instances; **`crypt`, `union`, `cache` and `chunker` are remotes that wrap other remotes** | the clearest precedent for holiday → holiday wrappers: an encryption wrapper, a fallback union, a cache, each wrapping a store and each itself a store | nothing. It is the closest match, for assets especially |

**What the survey says, in one line:** the tools people trust separate *what
you are connected to* (accounts, credentials, remotes) from *what flows where*
(the graph), and show you a plan before they act. The Antfarm has the second,
half of the first (credentials as references), and the effect gate's dry run
as a start on the plan. The [redesign](/concepts/platform/antfarm/redesign.md)
builds on that.

# 7. The category reading, and where to stop

Payload types are objects. Holidays are arrows. Wiring is composition.
`hol_html` is an arrow from a *product* (`records × assets → site`).
Wrappers are arrows from holidays to holidays. The typed canvas refusing a plug
is composition refusing to typecheck. This reading is useful because it
predicts things that turned out true: the transform is not a holiday, the
wrappers already existed, and the `domain` port gains a second consumer.

**It stops being useful** at the point where it would ask for machinery. The
standing rule from [Allomone's foundations](/concepts/allomone/foundations.md)
§7 is *adopt the behaviour, not the vocabulary; the burden is to show it makes
an implementation simpler.* Named pivots and round-trip tests pass that test.
Anything more abstract should wait for a bug it would have prevented.
