---
type: Concept
title: Antfarm v2 — documents, renditions, and domains
description: "A document is a way the database is presented (the author, 2026-09-28): newsletters, websites, calendars and maps are one kind, each a rune in the Documents chamber, each edited in its own tab, and a database may have several of each. Every document has a preview and a publish output; what comes out is a rendition (a site or a message), already through the privacy seam; a domain is where it goes, local domains need no key and web or mail domains need one, and a document with nowhere to go says so. How the Antfarm's filter and the Builder's filter compose without conflict: the Antfarm grants, the document chooses, the seam removes."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Design, not built.** See the [v2 index](/concepts/platform/antfarm/v2/index.md).

# 1. What a document is

The author, over the two conversations of 2026-09-28:

> it makes sense that they should be considered a "document". Because they
> depend on the data coming from the database itself. In a way we are designing
> some way for the database to be presented. That's what a "document" is.
> Therefore, the calendar and the map are both documents.

> The difference between documents and the mantle is mostly in what a document
> actually needs and does. most importantly, that a document needs to have some
> kind of output.

A **document** is a presentation of the database: which runes, laid out how,
styled how. It is itself a mantle (a newsletter is a mantle of block runes, a
calendar document is its views and filters, a map document is its views and
rules). What makes it a document rather than just a mantle is that **it takes
the database in and has outputs**.

| document kind | edited in | a database may have | renders to |
|---|---|---|---|
| **newsletter** | Builder | many | an email; a web archive page |
| **website** | Builder | many | a site |
| **calendar** | Calendar | **many** (a public events calendar, a volunteers' one) | an `.ics` feed; a calendar page |
| **map** | Territory | **many**, each with several views | a map page; a GeoJSON layer; an image |

**The tabs stay.** The author: *"They stay on their own tabs of course, and more
or less retain the same functionality as they currently have."* A tab is the
editor for a kind of document. What changes in the Calendar and Territory tabs
is one thing: a **document switcher**, because there can now be more than one.
An existing database's calendar and map become its first calendar document and
its first map document.

**The interior needs no Antfarm.** Drawing a calendar inside the application
is the application being its own render target
([three DSLs](/concepts/foundation/dsls.md)). The Antfarm is involved only when
a document's output goes somewhere: a local preview server, a website, an inbox.

# 2. The Document node

| port | type | dir | means |
|---|---|---|---|
| `data` | Mantle | in | what this document is *allowed* to read (§5). The seed wires the Data chamber |
| `audience` | Mantle | in, optional | newsletters only: the contacts it is sent to |
| `preview` | Rendition | out | the document rendered for looking at before it is public |
| `publish` | Rendition | out | the document rendered for the public |

Its face shows the document's name and kind, a thumbnail of the last
rendition, how many runes it can see and how many it uses, and when it was last
published and where.

# 3. Renditions

A rendition is what comes out of `preview` or `publish`: **a document rendered
for one output, after the privacy seam.** Internal-notes-class fields are
removed at the render seam (ground rule 6), and a rendition is on the far side
of it by definition. That gives one seam for all four kinds of document. Today
a calendar export and a map export are separate code paths. In v2 an `.ics`
feed cannot carry an internal note any more than a newsletter can, because
there is one seam, and it is testable.

There are two rendition shapes, because there are two kinds of place things go:

| shape | what it is | examples | goes to |
|---|---|---|---|
| **site** | a manifest: `path → (sha256, size)` plus the bytes | a website; a calendar page with its `.ics`; a map page with its GeoJSON | a web or local domain |
| **message** | an RFC 5322 message plus a recipient set | a newsletter issue | a mail domain |

The site manifest is the pivot [mappings](/concepts/platform/antfarm/mappings.md)
§2 said was missing and mattered most. Every deploy target Hormiga supports
already takes "a manifest of path → hash, send what is missing", so this is
where incremental publishing comes from.

**Preview and publish differ in what they include, not in the seam.** A
preview includes items marked draft and carries a *preview* mark. A publish
leaves drafts out. Both are through the seam.

# 4. Domains

The author:

> as a default, all documents should have some sort of local build of course.
> They can have a "preview" output, and a "publish" output. with both able to
> be wired to the same domain node

> a local domain needs no key of course. however, we have to consider the
> possibility of a device not being able to provide a local domain for a
> document to publish, such as for mobile devices right now. therefore publish
> and preview will just not work if there's nowhere to go to

> publishing requires a domain with an api key.

| domain node | takes | key | placement | examples |
|---|---|---|---|---|
| **Local domain** | site renditions | none | a profile whose device `serves_local` | `localhost:8780` on this PC |
| **Web domain** | site renditions | one key, required | any | GitHub Pages, Cloudflare Pages, an S3 website, with an optional custom name |
| **Mail domain** | message renditions | one key, required | any | a transactional sender (planned) |

**A domain takes many renditions, each at a mount.** A document declares where
it mounts (`/` for the website, `/events/` for the calendar page,
`/events.ics` for its feed), so one domain can serve a website and a calendar
together. Two renditions at the same mount are refused by `farm plug`, naming
both documents.

**Preview and publish to the same domain.** Wiring both is allowed. A preview
sent to a local domain is private to the network. A preview sent to a web
domain goes to that host's preview address (a branch deploy, where the host
has one) and its face says, plainly: *"public to anyone with the link, not
listed"*. Where the host has no preview address, the preview wire is refused,
with the reason.

**Nowhere to go.** On a phone, which cannot serve a local domain today, a
document whose only domain is local shows *"nowhere to go from this device"* on
its preview and publish, and the buttons are absent, not failing. This is
readiness per device ([types](/concepts/platform/antfarm/v2/types.md) §6).

**Custom names.** A web domain with a custom name (`example.org`) needs the
name's DNS pointed at the host. When the key's provider also serves DNS
(Cloudflare), the same key does both, and the domain's face shows whether the
name resolves. Otherwise it shows the record to create, by hand.

**Publish history stays ours.** Each publish writes a `deployment` rune, as
today, with the document, the rendition's manifest id and the domain.

# 5. Two places to filter, and why they cannot conflict

> **Enforced since 2026-10-01**: the renderers narrow their data to the grant
> during a v2 preview or publish (see the v2 index §"The grant, enforced").

The author:

> in the document builder right now, you can filter with tags on what is
> presented. what gets published or not, etc. However, we could also filter with
> nodes on the antfarm on what the document even gets allowed to use. its up to
> the user to decide which flow works best for them, but we should consider how
> the internal architecture should interact with one another, such that we can't
> have any conflicts.

**The rule: the Antfarm grants, the document chooses, the seam removes.** There
are three stages, applied in this order, and each can only take away:

1. **Grant (the Antfarm).** What reaches the document's `data` input is the
   most it can ever see. A filter here is a boundary: "the public website may
   never see anything tagged `internal`."
2. **Choose (the tab).** The Builder's, Calendar's or Map's own filters select
   *within* the grant: "this block lists upcoming events tagged `family`."
3. **Remove (the seam).** Internal-notes-class fields are removed from
   whatever is left, always, with no switch.

**Because every stage only narrows, there is no conflict to resolve.** The
result is always the intersection. A Builder filter that asks for something the
Antfarm did not grant simply finds nothing, and the block says so: *"12 events
match in this database; this document can see 0 of them (narrowed in the
Antfarm)"*, with a link to the Filter node. The tab always shows the grant at
the top of its filter panel ("This document can see 312 of 1,904 runes").

Either flow works, as the author wanted. A person who never opens the Antfarm
filters in the tab, against a grant that is the whole Data chamber. An
administrator who wants a guarantee ("nothing internal ever reaches the
website, whatever anyone does in the Builder") places it in the Antfarm, where
the Builder cannot undo it.

**What travels with a granted rune.** Assets referenced by granted runes are
granted with them, through the Data→Assets tunnel. A contact's photo comes with
the contact.

**An unwired `data` input grants nothing**, and the document says so. This is
the workbook's A2 answered yes: wiring is read. The seed wires every new
document to the Data chamber, so nobody meets this by accident.

# 6. Sending a newsletter

Sending is the one output where admin-plane data leaves: the recipient list.
The newsletter's `audience` input is a Mantle of contacts, filtered like any
other. The send is an effect whose consequence sentence says **what leaves**:
*"sends this issue to 214 addresses through Cloudflare. The addresses leave
this device."* ([capabilities](/concepts/platform/antfarm/capabilities.md):
*there is no third door*.) The mail domain is planned, not built.

# 7. Live renditions (deferred)

The author will elaborate *"how live data might look like for hosted
websites"*. The constraint any design must keep is from
[data planes](/concepts/platform/data-planes.md): **the publication plane must
never gain write-back.** A live website can *read* a published rendition that
updates (a feed, a JSON file a page fetches) and can *submit* to the inbox
plane. It never writes the database. A live rendition is therefore most likely
a site rendition that is republished automatically by a rule placed on one
device, which is expressible in v2 as it stands.
