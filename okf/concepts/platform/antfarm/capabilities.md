---
type: Concept
title: Capabilities — a holiday as a function call
description: "The author's 'host it online' (2026-09-15): the rest of the application asks a question with a typed answer and never names a vendor. The two capability tables that exist (hosting, collaboration), how 'who answers' and 'can it answer now' are decided, why after_publish is part of the answer, the next capabilities waiting for this shape, and what capabilities should become in the redesign: declarations on node kinds, which is also the seam Allomone needs."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-09-22T00:00:00Z
---

# The idea

The author, 2026-09-15:

> a button to automatically do something like 'host it online' should have its
> protocols be called upon via the antfarm. Because then, it should return with
> a link. essentially its kinda like a function call, where we expect a link to
> be given in return.

A **capability** is a question with a typed answer that more than one kind of
node can give. The caller asks the question. The Antfarm finds a node that can
answer. The caller never learns, and never needs to learn, whether the answer
came from ImgBB, an R2 bucket or the organization's own website.

This is the Antfarm's most important idea after typed ports. It is what makes
the Antfarm an interface the rest of the application *programs against*, not
a settings screen it *reads from*.

# The two tables that exist

## Hosting: "put this file online and give me a link" (`domain/hosting.hpp`)

| | |
|---|---|
| **in** | a local file (an image rune with a `path`) |
| **out** | `HostedLink`: `url`, the `node` that answered, `after_publish`, or `error` |
| **who can answer** | one row per protocol: `hol_imgbb`, `hol_object_store`, `hol_static_host`, `hol_github` |
| **who answers** | `config hosting.images` names a node. Blank means the first that can answer now, with hosts whose link works at once ahead of hosts whose link waits for a publish |
| **can it answer now** | `host_problem(node)`: empty, or the one thing it is missing |
| **implemented by** | `HormigaApp::host_online` in `publish/push.cpp`, one branch per row |

**`after_publish` is part of the answer, not a footnote.** A website host
copies the file into `site/assets/` and answers at once with
`site.base_url/assets/<file>`. That link is real, but it only works after the
next publish. Saying so is the difference between hosting on your own domain
and a newsletter full of broken images.

**Every surface reads the same table:** the CLI (`effect host-online`), the
console, the image editor's "online / not online yet" line and button, the
Data tab, the Antfarm faces ("Use for images"), and the inspector's *Hosting
images online* panel, which shows the choice, what it does and needs, a count
of images online versus not, and one button for the rest.

## Collaboration: "can this be shared, who is in, who is here, what stays home" (`domain/collab.hpp`)

| capability | question | answered by | status |
|---|---|---|---|
| `share` | Can this database be shared, and how does a copy reach someone? | `hol_lan_share` | built |
| `share` | (same) | `hol_sync_relay`: a relay holding only ciphertext | **planned**, a named row with no node |
| `members` | Who is in, and where is that list kept? | `hol_membership` | built (`local-file`) |
| `presence` | Who else is here right now, and on what? | `hol_lan_share` | built |
| `private` | What never leaves this device? | `hol_lan_share` (`private_tags`) | built |

The **planned row** is the useful trick: a future relay is already a value in
the table, so adding it is a row and a branch, not a redesign. The Share
window, Discover, presence and the CLI read these rows rather than naming node
kinds.

# The shape, stated once

Both tables are the same five things:

1. **the question**, in a person's words;
2. **the answer's type** (a link, a yes/no with a reason, a list of members);
3. **the node kinds that can answer**, one row each, with what each *does* and
   what it *needs*, in plain language;
4. **readiness**: can this node answer *now*, and if not, the one thing
   missing;
5. **selection**: an explicit choice (a config key) or a preference order.

That is a function signature (2), a set of implementations (3), a precondition
(4) and dispatch (5). The author's "function call" was exact.

# Capabilities waiting for this shape

Each of these has been asked for or designed somewhere in the OKF, and each is
currently either a vendor-specific button or nothing:

| capability | in → out | candidate nodes | where it was asked for |
|---|---|---|---|
| **send this newsletter** | rendered issue + recipients (records, filtered) → a send report | SMTP, a transactional API (Courier) | the Output interface's "later nodes" since 2026-07-16 |
| **shorten this link** | url → short url | a shortener, the org's own domain | the 2026-09-15 log ("should each be a table and a call like this one") |
| **translate this** | text + lang → text | the bundled engine (offline-degraded), a REST translator, a local model | the Translate interface; the 2026-09-02 rule that a site is never published in one language |
| **publish this website** | site → a `deployment` | localhost, GitHub Pages, a static host | today a pipeline plus the Publish tab; it is already a capability in all but name |
| **back up this database** | sealed blob → a stored copy | an object store, a folder | `push-store backup` |
| **fetch this calendar** | a feed → dated runes | ICS URL, CalDAV, Google (Q69) | the calendar hub's X-track |
| **ask a model** | prompt + context → text | a local endpoint, a hosted API | the `model` payload (Q38) |
| **read this flier** | image → proposed tags + a date | the command named in `config tools.image_text` | `effect read-flier`, built as a config key, not a node |

Two of these cross the privacy seam in the dangerous direction: **send this
newsletter** carries a recipient list, which is admin-plane data leaving, and
**ask a model** carries context. A capability's row must therefore also say
**what leaves**, the same sentence an effect's consequence already carries.
The [data planes](/concepts/platform/data-planes.md) rule applies: *there is
no third door.*

# What capabilities should become

The 2026-09-15 page said it: *"the larger Antfarm overhaul should treat
capabilities as declared node ports rather than a table in a header. The table
is the honest first version."* Stated concretely, as a proposal for the
[redesign](/concepts/platform/antfarm/redesign.md) and not a decision:

**A glyph declares what it can answer**, beside its payload ports, and the
header tables become data read from those declarations:

```json
{"glyph":"hol_object_store", …,
 "hints":{
   "ports":[{"name":"plug","dir":"in","type":"assets"}],
   "answers":[
     {"id":"host-file","in":"asset","out":"url","after_publish":false,
      "does":"uploads the file to the bucket and returns its public address",
      "needs":"bucket, access key and secret, public_url",
      "leaves":"the file, to a bucket anyone can read"}]}}
```

What that buys:

- **The Antfarm can draw capabilities.** A node's face lists what it answers
  and whether it can right now. The dashboard view groups nodes by the
  question they answer ("Putting images online: 2 ready"), which is how a
  person thinks, not by payload, which is how the code thinks.
- **A wizard can start from the question.** "I want to put images online"
  lists the glyphs that declare `host-file`, with each one's `needs`.
- **Allomone gets its seam.** The missing "program-callable holiday seam"
  ([three DSLs](/concepts/foundation/dsls.md)) is exactly a typed call by
  capability id. `host-file(flier-3)` from a script and from a button would be
  the same call.
- **Glyph declarations travel in the state document** (Void Core 0.2.14), so a
  `.miga` would carry which capabilities its Antfarm can answer. That is the
  "summary of what its Antfarm can do" [Q79](/developer_questions.md) wants on
  the database's profile, for free.

**What must not move into the declaration:** the *implementation*. The branch
in `host_online` stays C++, because it is the effect. A declaration that could
describe *how* to call a vendor (a URL template, a request body) would turn
the Antfarm into a programming language, and the [three DSLs](/concepts/foundation/dsls.md)
page is clear that the Antfarm runs no logic. Allomone runs logic. The Antfarm
says who can be asked.

**Upstream fit.** Glyph hints are Void Maiz's reading (`hints.ports`,
`hints.editors`, `hints.face`). An `answers` hint would be Hormiga vocabulary
until a second host wants it. Interaction Combinators does not, so it stays
ours, stored the way `hints.category` is: data Void Maiz carries and ignores.
