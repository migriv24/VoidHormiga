---
type: Concept
title: The web platform — domain, hosting, sign-in, and what a visitor is allowed to do
description: "How Hormiga puts a real website online without an always-on machine: the author's reversal of the self-hosting direction and the invariant that makes it safe (every cloud host is disposable), two new Antfarm payloads (domain, identity), five new holons, and the answer to visitor accounts — a submission is a PROPOSED COMMAND TRANSCRIPT, so the dispatcher stays the only door."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-19T00:00:00Z
---

# The decision that opens this

[antfarm](/concepts/platform/antfarm.md) records the author's direction of 2026-07-16:

> **no subscription services** … the preference is the org running its own
> infrastructure with tools Hormiga provides, on a domain the org purchases.

**Reversed by the author, 2026-08-19**, for a concrete reason rather than a
change of taste:

> *"I kinda don't wanna self host, because then I'll need a machine that's
> constantly on, which I don't have right now."*

That is a real constraint and the honest response is to design for it rather
than around it. Recorded here as a decision, because a concept page that
quietly contradicts an earlier one is how a project loses track of what it
believes.

**What survives the reversal, and is now load-bearing**, is the invariant the
same page already carried:

> **Every cloud host is disposable.**

That sentence was written about images on a free CDN that died. It is now the
**acceptance test for every vendor below**, and it has three parts:

1. **The model is local.** The `.miga` is the system of record. A host holds a
   *copy* or a *rendering*, never the truth.
2. **The site is regenerable.** `render-site` rebuilds it from the model, so a
   host is a place we push to, never a place we edit.
3. **Anything a visitor contributes must be pullable back.** The moment a
   stranger's profile edit lives only in a vendor's database, that vendor stops
   being disposable — and that is the trap this whole page is arranged to avoid.

A vendor we cannot walk away from fails, however good it is.

---

# 1. Where to buy a domain

A registrar is a **purchase, not a protocol** — the one thing here that stays
external, which the old direction already said.

- **Cloudflare Registrar** — sells at cost with no renewal markup, which over a
  decade is the difference that matters for a small nonprofit. It requires using
  Cloudflare for DNS, which is only a constraint if you were going to use
  somebody else's nameservers.
- **Porkbun** or **Namecheap** — fine, independent of who hosts, marginally more
  expensive at renewal.

Take **`.org`**. It costs the same as `.com`, it says what the organization is,
and it is the convention the audience already reads as "a real group".

Buy the name **before** anything else: it is the one decision that is expensive
to change, because it is printed on fliers.

Verify current pricing yourself — this page is design, not a quote.

# 2. Where to buy hosting

Split the question, because it is two questions wearing one word:

|   | what it needs | why |
|---|---|---|
| **the site** | a CDN that serves a folder | `render-site` produces static files; reading a website needs no server |
| **accounts** | somewhere that survives between visits | logins, profile claims, submitted events, uploaded pictures |

**The site half is nearly free and nearly disposable by construction.**
**Cloudflare Pages** or **Netlify** will serve the `site/` folder on a custom
domain at no cost at this scale. Any CDN can serve a folder, so switching is a
re-deploy.

**The accounts half is the real choice**, and the recommendation is
**Supabase**:

- **Postgres.** Your data comes back out with `pg_dump`. That is the
  disposability test passed at the level that matters.
- **Sign-in with Google is built in**, along with email links, so §3's holiday
  has something to call without writing an OAuth dance by hand.
- **Storage** for uploaded profile pictures, S3-shaped.
- **Row Level Security**, which is how "a visitor may edit their own profile and
  nothing else" is enforced *at the database* rather than in JavaScript that a
  visitor can read.
- And there is history: this database was rescued *from* Supabase, and
  `src/domain/rescue_import.hpp` still exists — so the export path has been walked.

The Cloudflare-only alternative (**Pages + Workers + D1 + R2**) is one vendor
instead of two and cheaper at scale, at the cost of writing the auth flow
yourself. For a first website with a volunteer maintaining it, that is the wrong
trade.

**None of this is in Hormiga's code.** It is configuration on Antfarm nodes, so
choosing differently later is unplugging a node — which is the point of the
Antfarm existing at all.

# 3. How the Antfarm supports it

The Antfarm is a **typed dataflow graph**: ports typed by the payload that
flows, so incompatible agents cannot be plugged together. It had three payloads
— `records`, `assets`, `site`. The web platform needs **two more**, and they are
new because the existing three genuinely could not say these things.

### `domain` — a name the org owns

Emitted by a registrar/DNS agent, consumed by a deployer. One domain can feed
several deployers (the apex to the site, a subdomain to the accounts backend)
without either one guessing the other's hostname.

### `identity` — who this visitor is

Emitted by a sign-in provider, consumed by the accounts backend.

**Separating them is the whole design.** Google says *who you are*; Hormiga's
own data says *which contact that is*. The link between an identity and a
contact is a link in the `.miga` and nowhere else — so the provider can be
swapped, or lost, and the relationships survive. That is disposability applied
to login, and it is why "sign in with Google" is **its own node**: an org that
wants email links, or no accounts at all, unplugs one agent and nothing else in
the graph changes. Hormiga must not assume every website wants Google.

### Submissions need no new payload

A visitor's proposed edits arrive as a **records source**, exactly like a CSV or
a Google Sheet. The Antfarm already says record agents are *stores that persist
and sources that import* — direction is a property of the agent, not the port.
That the reverse flow needed no new machinery is decent evidence the payload
typing was cut at the right place.

### The five new agents

| holon | payload | what it is |
|---|---|---|
| `hol_dns` | → `domain` | the registrar/DNS zone |
| `hol_static_host` | `site` + `domain` → | the managed CDN deploy |
| `hol_auth` | → `identity` | one sign-in provider, deliberately one node |
| `hol_accounts` | `identity` + `records` | logins, contact claims, pending submissions |
| `hol_uploads` | `assets` | visitor-supplied bytes |

`hol_uploads` is separate from `hol_imgbb` because **the trust is different**:
the org's own fliers and a stranger's profile picture should be able to land in
different buckets with different retention, and collapsing them would make that
impossible to express.

**Secrets are never fields.** Every one of these carries a `*_file` pointing at
a gitignored key file, which is the pattern `hol_imgbb` already established.
Ground rule 2 says no credentials in the repo, ever; a config field that holds a
token is a credential in the state document, which is the same mistake wearing a
different hat.

**Nothing is seeded.** A fresh org still ships local-only. Cloud agents are
things a person adds, never a default — unchanged from 2026-08-05.

---

# 4. What a visitor is allowed to do

This is the part where a conventional design would grow a users table, a roles
column, and an admin flag. Hormiga cannot, and does not need to.

## A submission is a proposed command transcript

Ground rule 3: **the model lives in Void Core and the dispatcher is the only
door.** A backend that wrote into the org's database directly would be a second
door, and every property this project rests on — replay, undo, attribution, one
reviewable batch — is a property of going through the first one.

So a visitor does not write. **A visitor proposes, and a person accepts.**

The `submission` glyph carries the proposal as a **command transcript** — the
same text `import_reyna_transcript` already accepts, filtered by the same rule:
only model-building verbs pass, so `use`, `config`, `deploy`, `effect` and
`script` are refused. **A stranger's profile edit structurally cannot become an
effect.** That filter was written for a harvested PDF; it turns out to be exactly
the check a hostile submission needs, which is what a real seam looks like.

Approving is then not a feature — it is **dispatching the transcript**, as one
`batch`, in the admin's session, attributed to them. One undo frame. `status` and
`diff` show it like any other change, and `revert` discards it.

**This is the third time this shape has arrived**, which is the argument that it
is right rather than convenient:

- Allomone's **Weaver** proposes rules and never writes them.
- **Void Reyna** proposes a dataset as a transcript, because the confirmation
  must be a logged command and therefore must happen in the consumer.
- a **website visitor** proposes an edit.

Same seam, three sources.

## There is no `kind` field, and no role hierarchy

The civic record refused a taxonomy of change — no `amendment`/`repeal` glyphs,
just dated assertions. The same refusal applies here: whether a submission is
"claiming a contact" or "proposing an event" is **read off the commands it
carries**, a derived view computed for the reviewer, never stored and never
trusted as a summary of what the transcript actually does.

And for permissions, the author's own constraint — *"I dislike hierarchy as a
structural aspect"* — has a real answer in this system rather than a compromise:

**Clearance is an annotation, not a rank.** Tags carry it (`clearance:public`,
`clearance:member`, `role:admin`), Allomone derives from it, and the existing
privacy seam already works this way: `internal` is a *predicate*, and internal
fields are blanked **before the frame is built** so a rule cannot read what was
never carried. Reyna's [foundations §3](../../VoidReyna/okf/concepts/foundations.md)
notes that the provenance-semiring framework covers *clearance levels* in the
same algebra as trust and method — so "who may see this" composes by merge, and
two rules that disagree about a field surface as **⊤** rather than resolving by
seniority.

The practical consequence: an "admin" is not a row in a table with a boolean.
It is a contact whose runes carry a tag, and the rules that grant power are
readable, mergeable, and conflict-surfacing like everything else.

**Today, anyone holding the `.miga` is the admin.** That is honest and it is
enough: the file is the authority, it is encrypted, and there is exactly one
copy. Finer grain arrives when a second person needs it, not before.

---

# 5. The website stops being a newsletter

The author's framing, and it is the right one:

> *a newsletter will be news updates and information, but the website will be
> where that information lives.*

They have shared a document model until now, and the conversion between them has
been useful. It becomes less so from here, and the divergence is not a failure of
the shared model — it is two publications with different jobs:

| | newsletter | website |
|---|---|---|
| shape | one page, one moment | many pages, standing |
| blocks | hero, narrative, grids | plus about, profile, submit, archive |
| audience | subscribers | anyone, plus signed-in members |
| time | an issue is finished | continuously true |

The `page` glyph already exists and websites already use it. What is missing is
**pages whose content is a visitor's own** — a profile page, a submission form,
a members area. Those are blocks that render *against an identity* rather than
against a query, which is a genuinely new kind of block and the next real design
question.

The conversion path stays: a newsletter is a good way to *announce* what the
website now holds, which is the honest relationship between them.

---

# 6. What is built, and what is not

**Built (2026-08-19):** the two payloads, the five holons, and the `submission`
glyph. That is the *vocabulary* — an Antfarm that can express a domain, a host,
a sign-in provider, an accounts backend and an upload store, and a data model
that can hold a visitor's proposal safely.

**Built 2026-08-20, from the first real OPERATOR problem:** the Publish tab
([workspace & sections](/concepts/sections/workspace-and-sections.md)), `effect
rollback-site`, and the **`deployment` glyph** — one rune per publish.

The history decision is the one worth recording, because the obvious answer was
wrong. Every managed CDN keeps a deployment list and Cloudflare's is one `GET`
away, so why store one? Because §0 of this page makes one property the
acceptance test for every vendor on it:

> **Every cloud host is disposable.**

A history that lives only in a vendor's database is a history the organization
loses the day it leaves — and leaving is the property this whole page is
arranged to protect. So a publish writes a rune, **through the dispatcher**,
like every other change: logged, attributed, replayable, diffable against
`_baseline`, and inside the `.miga` when the data moves. Founding commitment 1
applied to the one operation that leaves the document.

The vendor's own list is still read, and it is genuinely useful — it is where
`vendor_id` and each version's permanent URL come from, which is what makes
`[view]` possible. It **enriches a record we already hold**; it is not the
record.

`rollback_cmd` is `deploy_cmd`'s sibling and exists for the same reason: the
vendor call is the operator's to state, not ours to compile in. Cloudflare,
Netlify and GitHub Pages all keep history and all expose a rollback; none of
them agree on how, and none are stable enough to bake into a binary that needs
a release to correct.

**Built later on 2026-08-19, from the first real site:** `effect deploy-site`
through `hol_static_host.deploy_cmd`, and the **`directory` block with the
clearance gate** — which is the first place §4's "clearance is an annotation,
not a rank" is enforced in code rather than described. `clearance:public`
publishes a rune; `clearance:contact` additionally releases its email and
phone; neither is a rank and neither is overridable by a query. See
[blocks & domains](/concepts/sections/blocks-and-domains.md) and
[security](/concepts/platform/security.md) §3.

The clearance tags are **also** the shape the accounts half will need: an
identity that claims a contact is asking for exactly this annotation, and
approving that claim is a logged `tag` command like any other. That the
directory needed it first, before any login existed, is decent evidence it was
cut at the right place.

## Publishing is not a per-language operation (2026-08-20)

A rule with its own heading because it was broken three different ways at once,
and the failure was invisible from the side anyone was looking at.

> *publishing isn't just in one language, its for all languages.* — the author

`site/` is **one artifact carrying every language**. `deploy-site` uploads the
folder, not a page, so "which languages went out" is not a choice the publish
makes — it is whatever the last render happened to leave behind. That is a
terrible thing for it to depend on, and it went wrong exactly as you would
expect: the operator edited both halves of a bilingual site, ran `effect
render-site` (which defaulted to English), published, and a stale Spanish page
went live under the same URL as a current English one.

**It is invisible from the English page**, which is the one the person who
published it reads. The Spanish half of this site is not a translation of the
site; for most of the people it is published for it *is* the site.

So, in code:

- `hormiga::site_langs()` is the one list of languages a site carries. Nothing
  that publishes names a language, so a third language is that list plus the
  `ui()` table in `render/site.cpp`, and no publish path changes.
- **`effect render-site` with no language argument builds every language.** The
  old default — English — is what shipped the stale page. A default that
  silently does half of a bilingual job is worse than an error.
- **`deploy_site` refuses** unless every language has an index *and* they were
  built in one pass. Missing is the easy half; present-but-stale is the half
  that actually happened, and a valid week-old file looks exactly like a fresh
  one until you check the clock.
- The GUI **renders every language as part of pressing Publish**, so the green
  button's promise — "whatever is in site/ becomes the live page" — is only
  made when `site/` is what the database currently says.
- The `deployment` rune records **every** language published, not one. It used
  to record `lang 'en'` for a deploy that sent both, which made the history
  assert something the deploy had not done.

The general rule underneath, worth more than the language case: **a per-page
choice must never leak into a whole-artifact operation.** The GUI's language
toggle is a *preview* control, and the day it looked like a publish control is
the day a community's Spanish site went stale in public.

**Not built, deliberately, and in this order:**

1. **The effect handlers behind the holons.** Deploying to Cloudflare Pages,
   pulling submissions from Supabase, uploading to Storage — these need
   credentials, and credentials belong to the operator, not the developer. The
   operator's own deployment has them, and is the right place for the first
   implementation to be *driven*.
2. **The inbox surface in the GUI.** A section listing pending submissions with
   the derived "what does this transcript actually do" view, and Approve /
   Reject as one batch. Waiting on there being a submission to look at, which is
   the correct order: the headless path (`get transcript` → `--script --atomic`)
   works today with no new code, so the UI is convenience rather than capability.
3. **Identity-bound blocks** (§5), which need a real signed-in visitor to design
   against.
4. **Reyna in the loop** — a vision model reading a flier a visitor uploaded.
   The author is right that this is later; it is also the case that Reyna already
   emits transcripts, so it arrives as another proposer through the same seam.

---

# 7. Hosting on AWS (2026-08-21)

The author named hosting as the next focus, on AWS specifically, with four
eventual capabilities: **logins with levels of access**, a **live queryable
directory** that responds to database changes without a redeploy, **visitor
posts and uploads** including claiming to be a contact, and **eventually
video**.

§§1–6 above still stand — the five holons, the identity/contact separation, and
submissions-as-transcripts are unchanged and are what makes this tractable. What
follows is the vendor mapping and the two genuinely new pieces.

## 7.0 A constraint changed, and it changes §2

The author, 2026-08-21:

> we are now going to override a previous statement that i wanted no
> subscriptions. of course AWS costs money. […] so subscriptions are ok
> (already have one with cloudflare anyways)

That supersedes the founding "no package managers, no CDNs, no subscriptions"
instinct **for infrastructure only**. It does not touch ground rule 5 — third
party *code* is still vendored, no package managers — and it does not touch
local-first: a fresh org still ships working with no network at all. What is now
permitted is *paying a vendor to run a service the org has chosen to add*.

Recorded here because §2's recommendation (Supabase) was partly reasoned from
the old constraint, and a stance that changes silently is not a stance.

**Which data is which is [data planes](/concepts/platform/data-planes.md)** — written
2026-08-21 from the author's constraint that hosting must not dissolve
local-first. It supersedes §7.1's two-way split with a three-plane one (admin /
publication / inbox), and it is where the claim flow, the two kinds of cloud
save, and the testing strategy live. Read it before building any of §7.7.

## 7.1 The rule that makes a cloud safe to use: two directions, no third

Everything Hormiga puts in the cloud is exactly one of two things, and naming
which is the whole discipline:

| | what it is | who is authoritative | what happens if the vendor vanishes |
|---|---|---|---|
| **a projection** | the published subset — the directory, events, pages | the `.miga`, always | re-publish. Nothing is lost |
| **an inbox** | submissions, contact claims, posts, uploaded bytes | the cloud, *until drained* | you lose what had not been drained yet |

**There is no third category, and admitting one is how this design fails.** The
moment something lives only in the cloud and is never drained, the `.miga` stops
being the organization's data and the vendor stops being disposable — which is
the acceptance test §0 sets for every vendor on this page.

The practical consequence for the inbox: **drain often**, and treat undrained
depth as the real exposure. An inbox that has not been drained in a month is a
month of somebody's work living somewhere the org does not control.

## 7.2 The live directory, and the seam it must not go around

> only contacts or organizations with certain tags and stuff, and only in
> specific ways […] live updatable, responding to the changes in the database,
> rather than needing to be redeployed every time

The instinct to reach for is *"put the database in the cloud"*, and it is wrong
for the reason ground rule 3 exists. The right shape is smaller than it looks:

**Hormiga already computes the published subset.** `render-site`'s directory
block requires `clearance:public` on every rune, releases email and phone only
on `clearance:contact`, refuses to trust the block's own query, blanks
internal-notes-class fields before the frame is built, and honours
`web-hide`. That is the privacy seam CLAUDE.md rule 6 protects, and it exists
and works today.

So **"live" is not a new capability. It is a new FORMAT for an existing
computation.** Today that subset is rendered to HTML files; it should also be
renderable to rows. Same seam, same filter, same log line about what was
withheld — a second output domain, not a second privacy surface.

    the published subset  ->  HTML files   (render-site, today)
                          ->  index rows   (publish-index, new)

**This is the single most important safety constraint in this plan.** A new
export path that reached the data directly would bypass rule 6 in exactly the
way that rule exists to prevent, and it would do so invisibly, because the
website would look correct. Any implementation that does not route through the
same gate is wrong however well it performs.

### Two gates, doing different jobs

| gate | where | question |
|---|---|---|
| **publish-time** | in Hormiga, the existing seam | may this *leave the database at all*? Internal notes never do |
| **query-time** | in the cloud, against the visitor's identity | may *this visitor* see this published row? public vs member |

The distinction matters: an internal note must not be in the cloud store at all,
protected by an access rule. It must never have been uploaded. Access rules are
for the public/member distinction among things that were already cleared to
leave.

## 7.3 The AWS mapping

The five holons are unchanged; only their configuration is AWS-shaped. That is
the Antfarm's whole purpose and this is its first real test.

| holon | AWS | note |
|---|---|---|
| `hol_dns` | **Route 53** — or keep Cloudflare DNS | no reason to move a working zone |
| `hol_static_host` | **S3 + CloudFront** — or keep Cloudflare Pages | see 7.6 |
| `hol_auth` | **Cognito user pool**, Google as a *social* provider | the trap below |
| `hol_accounts` | **DynamoDB + Lambda** | the inbox and the identity↔contact link |
| `hol_uploads` | **S3** bucket, presigned PUT | separate bucket from the org's own assets |
| **`hol_index`** *(new)* | **DynamoDB + Lambda** | the published projection, queryable |

`hol_index` is the only new agent, and it needs **no new payload**: it is a
`records` STORE, and the Antfarm already says direction is a property of the
agent rather than the port. The payload typing surviving a requirement it was
not designed for is decent evidence it was cut in the right place — the same
thing §3 observed about submissions.

### The Cognito trap, which is worth writing down before it costs money

**Verified 2026-08-21:** Cognito user pools give **10,000 monthly active users
free**, perpetually, on the Lite/Essentials tiers — and **social sign-in
(Google, Facebook, Apple) counts in that tier**. Users federated through
**SAML or generic OIDC** get a free tier of **50 MAU**, and $0.015/MAU after.

Google can be configured either way. Wiring "Continue with Google" as a generic
**OIDC** provider instead of the built-in **Google social** provider is a
supported thing to do, produces an identical login experience, and drops the org
from 10,000 free users to 50. For an org of a few hundred members that is the
difference between free and not — for no visible reason and with no error.

### Cost, honestly

At a real deployment's scale (about 150 contacts, a few hundred members, low traffic) the recurring
cost of everything except video is **plausibly under $5/month**, and most of
that is the Route 53 hosted zone.

- **Cognito: $0** — under 10,000 MAU. *Verified today.*
- **DynamoDB on-demand: ~$1/month** — on-demand pricing is $1.25 per million
  writes and $0.25 per million reads; a comparable small workload (1M reads,
  500K writes, 5 GB) prices at about **$0.88/month**. *Verified today.*
- **Aurora Serverless v2 for the same workload: ~$44/month** — $0.12/ACU-hour
  with a 0.5 ACU floor. Scale-to-zero exists (since Nov 2024) but storage still
  bills and a cold cluster has to wake. *Verified today.* **A relational
  database is ~50x the cost here and buys nothing the directory needs**, which
  settles a question §2 left open by recommending Postgres for its `pg_dump`
  disposability: that argument was strong when the cloud held the ONLY copy, and
  it is weak now, because §7.1 makes the cloud copy a projection you can throw
  away and regenerate.
- **S3, CloudFront, Lambda, Route 53:** small at this scale, and **not verified
  today** — the web search budget ran out. Check them before committing.

Following the page's own convention: **this is design, not a quote.** Verify
current pricing.

**The real cost of AWS here is not money, it is operational surface.** Six
services, IAM policies, and failure modes a volunteer cannot debug. Supabase
(§2) is one dashboard; this is a console. The mitigation is that the *operator's*
surface stays Hormiga — the Publish panel, the inbox, the toast — and the AWS
console is somewhere only the developer goes. If that stops being true, the
choice was wrong.

## 7.4 The question this plan cannot answer: does a Hormiga run in the cloud?

Here is the tension, stated plainly, because everything about posts depends on
it:

- Ground rule 3: **the dispatcher is the only door.** A submission becomes real
  by being dispatched as a transcript, in a session, attributed.
- That dispatcher runs on **a volunteer's desktop, when they open the app.**
- A live website with member posts needs writes to appear **without a person at
  a keyboard.**

Three ways out, and only one is good:

1. **Everything waits for a human.** Honest, matches the current design, and
   means a member's post appears whenever someone next opens Hormiga — possibly
   days. Fine for contact claims. Poor for posts.
2. **A headless Hormiga runs in the cloud**, holds the `.miga`, and drains the
   inbox on a schedule, dispatching what policy allows and leaving the rest for
   review. **This is not a second door — it is the same door, running somewhere
   else.** `voidhormiga-cli` is already a real headless front-end over the same
   dispatcher, with the same glyphs, the same effect gate and the same journal.
   The architecture already permits this; nothing new is needed to make it
   possible.
3. **The cloud writes directly.** Breaks replay, undo, attribution and rule 3.
   Not an option.

Option 2 is right and it carries a serious consequence the author must decide,
not the developer: **the organization's whole database — 146 contacts, ~70
emails, ~66 phone numbers, and the vault's secrets — would live on a server.**
Today it lives on one machine and the file is the authority. That is a real
privacy posture and moving it is a real change, especially for an org serving
communities where a leaked contact list has consequences that are not financial.

Tracked as **Q43**. It is the gate on posts, and it should not be answered by
drifting into it.

## 7.5 Does Void Palabra make this easier or harder? Both, in specific places

The author asked directly. The honest answer has three parts and the third is
the useful one.

**Easier — the publish half, meaningfully.** Palabra names a state by the
canonical hash of its versioned slice, and stores content-addressed. Applied to
publishing:

- **"Is the live site current?" becomes exact.** The `deployment` rune records
  the published version name; comparing it to the document's current name
  answers the question against our own data rather than by asking the vendor or
  guessing from timestamps. Today nothing compares anything — which is precisely
  how a stale Spanish page went live on 2026-08-20.
- **Only changed chunks upload.** The same property that put 21 versions of a
  456 KB document in 464 KB applies to pushing a projection: a directory where
  one contact changed should cost one contact, not the directory.

**Neither — the accounts half.** Cognito, presigned uploads, the inbox: none of
it touches Palabra. It is a different problem and Palabra correctly has no
opinion about it.

**Harder — and this is the part worth writing down now.** Palabra makes one
wrong design *easy*, which is more dangerous than making it hard:

> Model the website as a **peer**, sync state to it bidirectionally, and let the
> join law merge visitor edits.

It will be tempting, it will demo well, and it is wrong three times over. It
puts a **writable replica of member PII** in the cloud; it replaces **human
review with automatic merge**, when the entire submission design exists because
a stranger's edit must be *proposed and accepted*; and it makes the cloud a
**second door** into the model. Palabra's Phase 4 (transport, signatures) will
make it more tempting still, which is exactly why the decision should be made
now, while it is cheap:

**The website is a publication and an inbox. It is not a peer.** Peers are
devices the organization trusts with the whole database. A web server is not
one, and no amount of merge-law elegance changes that.

## 7.6 Video: defer, and not mainly for cost

The author is unsure between embedding and self-hosting, and the instinct to
defer is right — video is the one place where "the vendor is disposable" is
weakest, because re-encoding and re-hosting an archive is genuinely expensive.

The cost comparison is the least interesting part (and is unverified — the
search budget ran out; CloudFront egress is the line item that matters, not
storage or transcoding). **The interesting part is a privacy question specific
to this organization.**

An embedded YouTube player gives Google a visitor log of everyone who watched a
video on a page about immigration resources. For an organization serving a bilingual community
serving people who may be undocumented, that is not an abstract tracking
concern. If video is embedded, it should at minimum use a no-cookie embed
domain, and the choice should be made deliberately rather than by pasting the
default share code.

**Recommendation: embed for now, behind a holon so it is swappable, and treat
"which embed, and what does it leak" as the real question rather than "embed or
self-host".** Self-hosting becomes worth it when there is an archive worth
owning, and there is not one yet.

## 7.7 The order to build it

Each phase is independently useful and independently abandonable, which is the
test that the phasing is honest rather than a waterfall with milestones.

| phase | what it delivers | needs |
|---|---|---|
| **H1** | `publish-index`: the published subset as rows; the site queries it. **The redeploy-per-edit problem is gone.** No accounts, no writes | `hol_index`, the second output domain through the existing seam |
| **H2** | Sign in with Google; members see `clearance:member` rows. Still read-only | `hol_auth` (Cognito, *social* provider), the query-time gate |
| **H3** | The inbox: contact claims and submissions, plus profile-picture uploads. Drained by a human in Hormiga | `hol_accounts`, `hol_uploads`, the GUI inbox surface (§6.2) |
| **H4** | Posts — submissions with a policy that may auto-dispatch | **Q43 first.** Everything else is blocked on custody |
| **H5** | Video, embedded | §7.6 |

**H1 is the whole of the author's second bullet and needs none of the other
three.** It is also the phase that most changes what the website *is*, and it
can be built and shipped before a single decision about identity is made. That
is where to start.

# 8. Two hosts, which is what makes "disposable" a property (2026-09-02)

§0 makes one property the acceptance test for every vendor on this page: **every
cloud host is disposable.** Until 2026-09-02 that was a *claim*. There was one
deployer that worked — Cloudflare Pages, natively, and it works well — and a
second node, `hol_github`, that had been in the Antfarm palette since the
holidays were first registered, carrying a label, a colour, a `site` input port
and a `repo` field, with nothing in the application reading any of it.

That is the 2026-09-02 field report's sentence about `image_grid.columns` at the
scale of a whole holiday: *a field that does nothing is worse than no field.*

**GitHub Pages is now a real deployer** (`src/publish/github.cpp`), and the
reason to build that one second — rather than Netlify, or a folder — is that it
is the host an organization already has. No card on file, no account to open,
and for a volunteer-run group the repository often exists before the website
does. Two deployers publishing the same `site/` folder through the same
`deploy_site` is what turns disposability from an argument into something an
organization can exercise on a Tuesday afternoon.

## Why it is a separate holiday and not a `provider` value

`hol_static_host` takes an account id and a project name and *receives an
upload*. `hol_github` takes a repository and a **branch** and *receives a
commit*. Rollback is a vendor API call on one and a ref move on the other.
Collapsing them into one node with a `provider` field would mean four fields on
every node that are meaningless for whichever vendor is selected — which is how
a node stops being readable at a glance, and being readable at a glance is what
the Antfarm is for.

What they share is the *seam*, not the shape: one `find_host` that accepts
either glyph, one `host_token` that resolves the credential vault-first, one
`deploy_site`, one `rollback_site`, one Publish panel, one `deployment` rune.

## What the second host taught, which the first could not

- **A publish is a mirror, not a patch.** The GitHub commit is built with no
  `base_tree`, so the tree *is* the built folder. With a base tree, a page
  deleted from the model would stay live on the site forever. `site/` is already
  a mirror of the database; the deploy has to be one too, or publishing can
  leave behind something the model no longer contains.
- **A host can have opinions the site does not.** GitHub runs Jekyll over the
  branch unless a `.nojekyll` file sits at its root, and Jekyll silently drops
  every path beginning with an underscore. That file goes into the *tree*, not
  into `site/` — it is a property of this host, and a Cloudflare deploy has no
  business carrying it. The general rule: anything a vendor needs that the site
  does not is the deployer's to add, never the renderer's.
- **A custom domain can live in the artifact.** GitHub reads a `CNAME` file in
  the published tree — not an API field — as the authority on which domain the
  branch serves, so a deploy that omits it silently *unsets* a domain someone
  configured in the web UI. This is why `domain` is a payload on a wire rather
  than a string duplicated on both nodes: the deployer reads it off the wired
  `hol_dns` node and writes it into the tree, and neither node has to know the
  other's business.
- **The rollback id can be the real thing.** On Cloudflare the deployment id has
  to be recovered from the shape of a preview URL, narrowly and with a hedge.
  On GitHub the commit sha *is* the id, so the history Hormiga keeps and the
  thing the host needs to act are the same string. That is the shape this page
  wants from every host: our record stands on its own, and the vendor is asked
  only to act on it.

## `effect check-host` — the sibling of `check-store`

The field report asked for it by name, with the right argument attached:

> Perform the smallest real call (list the project) and tell the operator
> whether the token, the account id and the project name line up, *before* a
> deploy is attempted.

This page's oldest lesson about credentials applies to the other one-way door:
**a green light that does not predict the operation is worse than no light.**
The same report supplies fresh evidence for it — an account-scoped Cloudflare
token (`cfat_…`) answers `Invalid API Token` to `/user/tokens/verify` while
working perfectly against every account and zone endpoint. So `check-host` makes
real reads against the resource a deploy touches and never asks a vendor to
validate a credential in the abstract.

**It reports several lines rather than a verdict**, and that is the design and
not an implementation detail. "Can I publish?" is four questions: the credential
can be wrong, the repository or project name can be wrong, the branch can not
exist yet (which is *fine* on a first publish), and Pages can be off or pointed
at a different branch. Four problems, four different fixes, and only one of them
is the token. Collapsing them into pass/fail sends an operator to regenerate a
working credential because their `branch` said `main` and the host was serving
`gh-pages`.

## What is still manual

Creating a **Cloudflare Pages project** — `deploy-site` publishes into one and
fails if it does not exist. The GitHub path turns Pages on for a repository on
first publish (one call, made after the branch exists, because GitHub refuses to
enable Pages on a branch that is not there), so half of the field report's A5 is
closed and half is not.

Writing **DNS records** is also still manual, and `hol_dns` still holds
`domain`, `provider`, `zone_id` and `token_file` with only `domain` being read.
That node is a described capability, not a built one, and this page should not
pretend otherwise.
