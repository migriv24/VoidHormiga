---
type: Concept
title: Data planes — the admin database, the publication, and the inbox
description: "The three separate data planes hosting introduces, what is authoritative in each, why they must never merge, the two kinds of 'cloud save' that must not be confused, and why claiming a contact must not search the database."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-21T00:00:00Z
---

Opened 2026-08-21, from the author's constraint that hosting must not dissolve
local-first:

> the local and offline things that happen is ultimate. We have palabra to
> manage for what happens between users who are managing a miga database.
> however, we need to distinguish between the database and the management of IT,
> versus the AWS hosting, and what it calls/references to update the website.

That distinction is the whole of this page. [web platform](/concepts/platform/web-platform.md)
§7 says *what* is hosted and on which vendor; this says *which data is which*,
and it is the part that has to be right first, because every security property
in the system is a consequence of it.

# 1. Three planes, not two

Hosting introduces a second and a third body of data. Naming which is which is
the discipline, and most mistakes available here are a plane quietly becoming
another one.

| | **admin plane** | **publication plane** | **inbox plane** |
|---|---|---|---|
| what it is | the `.miga` — the organization's whole database | the published subset, as rows a website can query | proposals from visitors, awaiting a person |
| who is authoritative | **the `.miga`, always** | the admin plane. This is a derivative | the cloud, **until drained** |
| who may read it | admins holding the file | anyone, subject to the query-time gate | admins; the submitter's own |
| contains internal notes? | yes | **never** — they do not leave the seam | no |
| direction | between admins | admin → cloud, one way | cloud → admin, one way |
| managed by | Void Palabra | `publish-index` (a render domain) | `submission` runes + a drain |
| if the vendor vanishes | nothing is lost | re-publish | you lose what was undrained |

**Local is ultimate**, and here that means something precise rather than
aspirational: **every plane has a local origin or a local destination.** The
admin plane lives locally and is synced between admins. The publication plane is
*computed* locally and pushed. The inbox is *drained* locally and then it is
ours. **Nothing lives only in the cloud.** The day something does, the `.miga`
has stopped being the organization's data.

The corollary the author already stated and which this page holds to: editing
the website, managing the database, and producing the site all work with the
network unplugged. Hosting buys the *website's* reliability. It buys the
application nothing, and must cost it nothing.

# 2. The rule: a plane never becomes another plane

Two failures are available and both are quiet:

**The publication plane must never gain write-back.** The moment the website can
write into the store the admin plane reads, the cloud is a second door and every
property ground rule 3 protects — replay, undo, attribution, one reviewable
batch — is gone. Visitor input has exactly one route and it is the inbox.

**The admin plane must never be directly readable by the website.** Not "guarded
by an access rule" — *not present*. An access rule is a thing that can be
misconfigured; an absent field cannot be leaked. This is CLAUDE.md rule 6 applied
one layer out, and it is why §4 below insists that internal-notes-class data is
never uploaded rather than uploaded-and-protected.

# 3. Two kinds of "cloud save", and confusing them is the breach

The author wrote:

> everything will always have a local save and a save on a cloud database now
> with AWS

There are two very different things that sentence can mean, and only one of them
is safe:

| | **encrypted backup** | **queryable projection** |
|---|---|---|
| what AWS stores | an opaque blob | rows it can read |
| what AWS can read | nothing | exactly what you published |
| contains | the whole `.miga`, internal notes included | only what cleared the seam |
| purpose | reliability — you do not lose the org's data | serving the website |
| key | the org's, never uploaded | n/a |

**Both are wanted. They are not the same store and must never be the same
store.** A queryable cloud database containing everything is the failure this
page exists to prevent; an encrypted blob in S3 is straightforwardly good and
should be built early, because "the volunteer's laptop died" is a likelier
disaster than any attacker.

**We can already do the encrypted half.** `platform/vault.cpp` seals with
XChaCha20-Poly1305 over argon2id via vendored libsodium — the primitive is in
the tree, verified, and used. A whole-bundle encrypted backup is that primitive
applied to the `.miga` rather than to the secrets inside it, and it needs no
vendor cooperation: AWS stores bytes it cannot read.

Note the interaction with Void Palabra, who have said container encryption is
theirs at their Phase 4 and that **keys are never Palabra's** — we supply key
material, they encrypt. So an interim client-side encrypted backup is not wasted
work that Palabra will duplicate; it is the same shape, one layer up, and the
key management is ours either way.

# 4. Claiming a contact must NOT search the database

The author's phase 1 includes:

> the ability to "claim" that you are one of the contacts in the database (which
> will involve searching the database itself)

**The search is the part to remove**, and removing it makes the feature simpler
as well as safer.

## Why searching is wrong

A claim happens *before* a person is listed. So a searchable claim index would
have to contain contacts who have **not** consented to be public — which is
precisely the set `clearance:public` exists to keep off the website. Publishing
it under a different name does not make it a different disclosure.

And it is worse than a leak of one record: a search box over the member list is
a **membership enumeration oracle**. Anyone may type a name and learn whether
that person is in the network. For an organization serving Latine communities
including people who may be undocumented, the member list is exactly the artifact
that must not be queryable by strangers, and "you need an account first" is not a
defence against someone who can make an account.

## The inversion: assert, don't search

The claimant **asserts** an identity; the admin **matches** it, locally, against
the full database, in Hormiga, where they are already allowed to see everything.

    visitor signs in                      -> an `identity` (Cognito/Google)
    visitor states who they are           -> name, email, phone, organization
    that becomes a `submission`           -> a proposed transcript, nothing linked
    admin opens Hormiga                   -> Hormiga searches LOCALLY, ranks
                                             candidate contacts, shows them
    admin picks one                       -> dispatches the link, one batch

What this buys:

- **The website never holds a member list.** There is nothing to enumerate,
  because the matching data never leaves.
- **The search happens where the data already is**, by someone already entitled
  to it. No new privacy surface exists at all — the strongest possible version
  of the seam.
- **The admin's job gets easier, not harder.** They see ranked candidates rather
  than a name to look up by hand, and Hormiga can rank on signals the website
  never sees: does the Google email match a contact's email, does the phone
  match, is there an organization edge.
- **Nothing is lost.** Admin approval was always required, so the visitor waits
  either way. The search bought them no confirmation they were going to get.

**A public directory is a different feature and still exists.** Browsing the
people who *are* `clearance:public` is a published projection and is fine.
Claiming is not browsing, and conflating them is what produces the oracle.

## The one thing to get right in the transcript

A claim's transcript must **propose a link, never assert an identity field**. It
carries "this identity claims to be this person" as evidence for a human, not
"set contact.email = …". The existing verb filter already refuses `use`,
`config`, `effect`, `deploy` and `script`; a claim should additionally be
readable at a glance as *"link identity X to contact Y"*, because a reviewer
approving twenty claims will not read twenty transcripts closely, and a design
that depends on them doing so is a design that fails on a Tuesday.

# 5. The admin↔admin plane is Palabra's, and its schedule is not ours

The author's phase 2 — two computers, one `.miga`, end-to-end encryption,
version control, no duplicated data — is **exactly Void Palabra's job**, and
their own agent was explicit on 2026-08-21:

> adopt `save`/`load` before anything else and leave the multi-user path alone
> until Phase 4 exists … the sync story has one large unmeasured claim in it
> still (that the protocol survives arbitrary *message* order, as distinct from
> arbitrary *merge* order, which is what the join suite actually proves).

A library author volunteering the boundary of what their own tests prove should
be believed. **So phase 2 as stated is blocked on somebody else's unbuilt
phase** — and that is worth knowing before it is scheduled rather than after.

**But it splits, and the risky half is available now.** Palabra's own README:

> Merging needs **only the two current states** — no history, no server, no peer
> registry.

So:

| half | needs | available |
|---|---|---|
| **the merge is correct** — no duplicates, versions resolve, conflicts surface | `archive` + `join`, both built and verified | **now** |
| **the transport is secure and automatic** — E2EE over a network, signatures | Palabra Phase 4 | not yet |

The first half is where the author's stated fears live — *"bugs like duplicating
data will also need to be handled here"* — and it can be exercised with **two
folders on one machine** and a file passed between them. No second computer, no
network, no encryption, no Phase 4. If the merge is wrong, that is where it is
cheapest to find out.

The second half, meanwhile, has an honest interim: a `.miga` encrypted client-side
(§3) and passed through any dumb file store — S3, a USB stick, a shared drive.
That is end-to-end encrypted by construction, because the store holds bytes it
cannot read, and it is not a compromise so much as the same guarantee with a
manual transport.

# 6. Testing without a domain, and without touching the org's data

Two constraints from the author, and they point the same way:

> i dont wanna buy a domain just to test with the cat database

> you are the hormiga developer agent. you are not to touch any other database,
> as hormiga itself should not be specifically designed for the partner organization

**The Cat Colony is the answer to both**, and it is already what it is for: 50
fictional cats, public, tag-rich, shipped as the default and named in
[cat-dataset](/concepts/projects/cat-dataset.md) as the testing ground. Designing the
publication pipeline against cats rather than against a real org is not a
limitation — it is the thing that forces the design to be generalizable, which
is the stated goal. A pipeline that only works because it knows what one organization
contact looks like is the failure mode, and cats make it visible immediately.

**A domain tests the website. It does not test the pipeline.** Almost everything
worth testing is on this side of the network:

| testable with cats, one machine, no domain, no AWS account | needs the network |
|---|---|
| `publish-index` emits the right rows, and **only** cleared ones | the actual S3/DynamoDB/Cognito calls |
| the clearance filter withholds what it should, and says so | a real OAuth round trip |
| a claim transcript is well-formed and refused when hostile | CDN behaviour, latency |
| two-folder merge: duplicates, version resolution, conflicts | |
| encrypted backup round-trips | |

And past the network boundary, this project already has a proven pattern:
**the developer builds and tests the local half; the operator drives the first
real remote call and reports the vendor's own words verbatim.** That is exactly
how the Cloudflare deploy went — and it is why the `upload-token` bug was
diagnosed in a minute rather than an afternoon, because the vendor's raw body
reached the log unparsed. The same play works here, with the field agent as the
first driver, and it keeps the developer's tree free of credentials and free of
one organization's data.

**What that asks of the field agent** is measurements, not code: does the call
reach the vendor, what did the vendor say verbatim, what did the operator see.
Same as before.

# 7. Phasing

The author proposed two phases and invited a better shape. The goal is right;
the sequencing wants one change and one split.

**The change: build a walking skeleton, not a finished layer.** The author's
instinct to include login and claiming in phase 1 is better than building a
perfect read-only index first, because the *loop* is what is unproven — publish,
identify, propose, drain — and a thin version of the whole loop de-risks more
than a thick version of one part. Nothing here is hard individually; the risk is
in the seams between planes.

**The split: phase 2's merge and phase 2's transport are different projects**
with different blockers (§5).

| phase | what it proves | blocked on |
|---|---|---|
| **A. publish-index** ✅ **built 2026-08-21** | the published subset renders as rows through the *same* seam as HTML, and a `live` directory writes a refreshable fragment the page re-fetches — the redeploy-per-edit problem is gone. Local file output; no AWS | nothing |
| **B. push it** ◑ **built 2026-08-25, awaiting a credential** | `hol_object_store` + AWS SigV4 (`publish/aws.cpp`, no SDK, libsodium only) + `effect push-store <index\|backup>`. Signing proven offline against FIPS 180-4 and RFC 4231 vectors; a real request to AWS returns `InvalidAccessKeyId` rather than `SignatureDoesNotMatch`, so the shape parses. **Only "does AWS agree" is left** | a real key and bucket |
| **C. identity** | sign in with Google; the query-time gate distinguishes public from member | B |
| **D. claim, asserted** | §4's flow end to end: submission in, ranked match in Hormiga, link dispatched. **This closes the loop** and is the author's phase 1 | C (the upstream blocker cleared 2026-08-25) |
| **E. encrypted backup** ✅ **built 2026-08-21** | the database sealed under a passphrase the store never sees — `platform/backup.cpp`, libsodium `secretstream`. Restore verified against a genuinely corrupted database, which is what revealed that a recovery tool cannot be an effect (effects need a session; a session needs a loadable document). `--restore-backup` is a flag in `main` | nothing |
| **F. two-folder merge** | duplicates, version resolution, conflicts — on one machine, with cats | Palabra `archive` adoption (Q40) |
| **G. real two-machine sync** | the author's phase 2 | **Palabra Phase 4** |

> **PHASE D WAS BLOCKED UPSTREAM; IT IS NOT ANY MORE (2026-08-25).** A newline
> inside a value ended the command and the remainder EXECUTED — one submitted
> `bio` rewrote a different contact's email, and the verb filter did not help
> because the injected verb was `set`. Reported to Void Core, who **corrected our
> diagnosis** (their argv tokenizer had always carried newlines; the fault was in
> three other scanners), found **two further routes** we had not tested, and
> fixed all of it in 0.2.7 by changing one line of SPEC §6.1: an unterminated
> quoted run is now an ERROR rather than running to end of input.
>
> The gate is now `domain/submission.hpp`, and its rule is that **we do not parse
> a transcript — we ask the engine that will run it what it will do**
> (`vc_transcript_split_json`). Three checks in order: does it parse, is it
> **flat** (no control flow, so its effect can be read off its statements), is
> every verb allowed. The third alone is what we had, and it is what the
> injection walked past. `tests/submission_smoke.cpp` pins all three routes plus
> the properties, 25 assertions.

**A and E are both unblocked and independent.** A is the one that changes what
the website is; E is the one that protects the organization from a dead laptop.
Neither needs a decision from anybody.

**D is the author's phase 1** and arrives with the loop closed. **G is the
author's phase 2** and is the only item here waiting on another project — which
is precisely why F exists separately: it takes the risk the author actually named
(duplicated data) and pays it down now, on this machine, with cats.
