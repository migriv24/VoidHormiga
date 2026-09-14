/* glyphs_antfarm.hpp - the ANTFARM glyphs: one rune per backend.
 *
 * Split out of `seed.hpp` on 2026-08-20, when the file crossed its length
 * budget and `tools/find_long.py` said so. The seam was already there — these
 * were three `register_*` functions in one header — which is what a good split
 * looks like: the file was long because three things had been put in it, not
 * because any one of them was.
 *
 * A glyph declaration is DATA. It is long because an outreach organization has
 * a lot of kinds of thing, and that length is honest; what was wrong was
 * keeping every family in one place.
 */
#pragma once

#include "domain/seeds.hpp" // the demo transcripts, split out 2026-08-20

#pragma once
#include "domain/civic.hpp"            // the civic record owns its own glyphs too
#include "domain/hormiga_allomone.hpp" // the domain Allomone owns its own glyphs
#include "voidmaiz/embed.hpp"
#include "json.hpp"
#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace hormiga {

inline void register_antfarm_glyphs(maiz::Core& core) {
    // CORE — the org's data model, exposing two PAYLOAD ports: records + assets.
    core.register_glyph(
        R"({"glyph":"org_core","label":"Hormiga Core","fields":["org_name"],)"
        R"("hints":{"color":"#b3592e","face":{"w":210,"h":56},)"
        R"("labels":{"org_name":"Organization"},)"
        R"("ports":[{"name":"records","dir":"out","type":"records"},)"
        R"({"name":"assets","dir":"out","type":"assets"}]}})");

    auto reg = [&](const std::string& glyph, const std::string& label,
                   const std::string& color, const std::string& fields,
                   const std::string& labels, int face_h, const std::string& ports) {
        core.register_glyph(
            std::string(R"({"glyph":")") + glyph + R"(","label":")" + label +
            R"(","fields":[)" + fields + R"(],"hints":{"color":")" + color +
            R"(","face":{"w":230,"h":)" + std::to_string(face_h) +
            R"(},"labels":{)" + labels + R"(},"ports":[)" + ports + R"(]}})");
    };
    auto in = [](const char* type) {
        return std::string(R"({"name":"plug","dir":"in","type":")") + type + R"("})";
    };
    // colors: local agents warm/solid; cloud agents teal (the locality badge)
    const char *L_DATA = "#3f6fae", *L_ASSET = "#7d5bb0", *L_PUB = "#d4a017",
               *L_SERVE = "#2e6b4f", *CLOUD = "#4e8d85";

    // RECORD agents (payload: records) — stores persist, sources import
    reg("hol_sqlite", "SQLite - local store", L_DATA, R"("file")",
        R"("file":"Database file")", 56, in("records"));
    reg("hol_csv", "CSV - local source", L_SERVE, R"()", R"()", 40, in("records"));
    reg("hol_supabase", "Supabase - cloud store", CLOUD, R"("dump_dir")",
        R"("dump_dir":"Rescue dump folder")", 84, in("records"));
    reg("hol_sheets", "Google Sheets - cloud source", CLOUD, R"("sheet_url")",
        R"("sheet_url":"Sheet URL")", 56, in("records"));

    // ASSET agents (payload: assets) — a local store, a cloud host
    reg("hol_fs_assets", "Local files - asset store", L_ASSET, R"("dir")",
        R"("dir":"Assets folder")", 56, in("assets"));
    reg("hol_imgbb", "ImgBB - cloud image host", CLOUD, R"("key_file")",
        R"__("key_file":"Key file (gitignored)")__", 84, in("assets"));

    // PUBLISHER — the pipeline head: consumes records + assets, emits a `site`
    reg("hol_html", "HTML site - publisher", L_PUB, R"()", R"()", 72,
        std::string(R"({"name":"records","dir":"in","type":"records"},)") +
        R"({"name":"assets","dir":"in","type":"assets"},)" +
        R"({"name":"site","dir":"out","type":"site"})");

    // SERVERS / DEPLOYERS — consume a `site` (plug into the publisher's out)
    reg("hol_localhost", "Localhost - local server", L_SERVE, R"("port")",
        R"("port":"Localhost port")", 88, in("site"));
    /* ── GITHUB PAGES: the deploy target an organization already has ─────────
     *
     * This node has been in the palette since the holidays were registered,
     * with one `repo` field, a `site` input port and nothing reading any of it.
     * The 2026-09-02 field report's sentence about `image_grid.columns` applies
     * exactly: *a field that does nothing is worse than no field* — and this was
     * a whole holiday of them. `src/publish/github.cpp` now speaks the Git Data
     * API, so this is the configuration for a publish that happens.
     *
     * It is a SEPARATE HOLIDAY rather than a `provider` on `hol_static_host`,
     * and the difference is real rather than taxonomic: a managed CDN takes an
     * account id and a project name and receives an upload, and GitHub Pages
     * takes a repository and a BRANCH and receives a commit. Rollback is a
     * vendor call on one and a ref move on the other. Collapsing them would
     * mean four fields on each node that are meaningless on the other, which is
     * how a node stops being readable at a glance — the thing the Antfarm is
     * for.
     *
     * `branch` is a field because `gh-pages` and `main` are both ordinary
     * answers and the wrong one publishes into a void: a deploy that succeeds
     * and changes nothing anyone can see. `effect check-host` reads GitHub's own
     * Pages settings and says when the two disagree.
     *
     * The credential rides the two doors every other one in this application
     * does — `token_key` in the passphrase-locked vault first (it travels with
     * the .miga), `token_file` beside the database second — and it never
     * reaches a command line. */
    reg("hol_github", "GitHub Pages - cloud deploy", CLOUD,
        R"("repo","branch","token_key","token_file","message")",
        R"__("repo":"Repository (owner/repo)",)__"
        R"__("branch":"Branch GitHub Pages serves (default gh-pages)",)__"
        R"__("token_key":"Access token, kept in the encrypted vault (travels with the .miga)",)__"
        R"__("token_file":"...or a token file beside the database (gitignored)",)__"
        R"__("message":"Commit message for each publish (blank = a generated one)")__", 124,
        std::string(R"({"name":"site","dir":"in","type":"site"},)") +
        R"({"name":"domain","dir":"in","type":"domain"})");

    /* ── LAN PEER: the other device with this database (2026-08-27) ──────────
     *
     * okf/concepts/platform/collaboration.md §4. The author asked for LAN nodes
     * as a fallback for cloud sync; the OKF's own commitments invert that, and
     * the node is coloured L_SERVE rather than CLOUD to say so in the one place
     * a person actually looks. Local-first is the resting state: two laptops on
     * one Wi-Fi is the PRIMARY collaboration story, and the encrypted blob in a
     * bucket is what you fall back to when the two people are not in the same
     * room. This node needs no account, no credential and no vendor.
     *
     * `payload: records` deliberately — a peer is a records SOURCE and a records
     * STORE at once, which is exactly what the port comment above says an agent
     * may be, and it means the Antfarm draws sync in the same vocabulary as an
     * import. There is no new port type for "sync".
     *
     * NO SECRET FIELD, and that is not an oversight. The private key never
     * leaves this device, so there is nothing here for a vault to hold; what is
     * remembered about a peer is its PUBLIC key, and that is a `peer` rune
     * below rather than node configuration, because it is learned at runtime
     * rather than typed by an operator. */
    reg("hol_lan_peer", "LAN peer - another device", L_SERVE,
        R"("display","port","auto","peer_host")",
        R"__("display":"What this device calls itself to others on the network",)__"
        R"__("port":"TCP port for the sealed stream (default 47732)",)__"
        R"__("auto":"Announce on the network automatically (yes/no)",)__"
        R"__("peer_host":"Optional fixed address, when discovery is blocked")__", 116,
        in("records"));

    /* A REMEMBERED PEER: one rune per device we have paired with.
     *
     * Trust on first use, then pinned. The short authentication string is
     * compared by two humans ONCE; after that this rune is what makes a
     * substituted key visible, because a known peer arriving with a different
     * public key is a loud refusal rather than a fresh prompt. A design that
     * re-prompts on every connection trains people to say yes.
     *
     * `public_key` is not a secret and is safe in the state document — that is
     * what public means, and it is why this can travel in the `.miga` and be
     * merged like any other rune. */
    core.register_glyph(
        R"({"glyph":"peer","label":"Paired device",)"
        R"("fields":["peer_id","display","public_key","fingerprint","sas",)"
        R"("last_seen","last_version"],)"
        R"("hints":{"color":"#2e6b4f","face":{"w":230,"h":104},"category":"Antfarm",)"
        R"("editors":{"last_seen":"date"},)"
        R"__("labels":{"peer_id":"Peer id (device + database)",)__"
        R"__("display":"Device name","public_key":"Public key (base64; not a secret)",)__"
        R"__("fingerprint":"Key fingerprint","sas":"Short code confirmed when pairing",)__"
        R"__("last_seen":"Last seen","last_version":"Their cut name when last seen"}}})__");

    /* ── THE WEB PLATFORM (2026-08-19) ───────────────────────────────────────
     *
     * The author reversed the 2026-07-16 "no subscription services / self-host"
     * direction, for a concrete reason: self-hosting needs a machine that is
     * always on, and there is not one. Recorded as a decision rather than
     * quietly contradicted — see okf/concepts/platform/web-platform.md.
     *
     * What makes it safe is an invariant this page already had:
     * **every cloud host is disposable.** The model is local, the site is
     * regenerable from it, and anything a visitor contributes must be pullable
     * back into the .miga. A vendor we cannot walk away from fails the test no
     * matter how good it is.
     *
     * TWO NEW PAYLOADS, because the existing three could not say these things:
     *
     *   `domain`   — a name the org owns. Emitted by a registrar/DNS agent and
     *                consumed by a deployer, so one domain can feed several
     *                deployers (apex -> the site, a subdomain -> the accounts
     *                backend) without either guessing the other's hostname.
     *   `identity` — "who is this visitor". Emitted by a sign-in provider and
     *                consumed by the accounts backend, which is the agent that
     *                decides what an identity is ALLOWED to do. Separating them
     *                is the point: Google says who you are, and Hormiga's data
     *                says which contact that is. Swap the provider and the
     *                relationships survive, which is disposability applied to
     *                login.
     *
     * SUBMISSIONS RIDE `records`, and need no new payload at all. The comment
     * at the top of this function already says it: record agents are stores
     * that persist AND sources that import. A website's visitor submissions are
     * a cloud records SOURCE, exactly like Sheets or a CSV — the direction is a
     * property of the agent, not of the port. */
    const char* WEB = "#4e8d85"; // cloud locality, same badge as the others

    // DOMAIN — the org's name. Purchase stays external (a registrar is a
    // purchase, not a protocol); what is modelled is the zone we drive.
    reg("hol_dns", "Domain / DNS - registrar", WEB,
        R"("domain","provider","zone_id","token_file")",
        R"__("domain":"Domain (example.org)","provider":"Registrar/DNS (cloudflare, porkbun, …)",)__"
        R"__("zone_id":"DNS zone id (the ZONE lives here, not on the host)",)__"
        R"__("token_file":"API token file (gitignored)")__", 96,
        R"({"name":"domain","dir":"out","type":"domain"})");

    // STATIC HOST — consumes the built site AND a domain. `hol_github` remains
    // for a Pages repo; this is the generic managed-CDN deploy.
    reg("hol_static_host", "Static host - cloud deploy", WEB,
        R"("provider","account_id","project","token_key","token_file","deploy_cmd",)"
        R"("rollback_cmd")",
        R"__("provider":"Host (cloudflare-pages, netlify, …)",)__"
        R"__("account_id":"Cloud account id","project":"Project/site name",)__"
        R"__("token_key":"API token, kept in the encrypted vault (travels with the .miga)",)__"
        R"__("token_file":"...or a token file beside the database (gitignored)",)__"
        R"__("deploy_cmd":"Override the built-in upload command (see okf/concepts/platform/web-platform.md)",)__"
        R"__("rollback_cmd":"Command to restore a past deployment ({deployment} = its id)")__", 124,
        std::string(R"({"name":"site","dir":"in","type":"site"},)") +
        R"({"name":"domain","dir":"in","type":"domain"})");

    /* ── AN OBJECT STORE: the bucket everything small gets pushed to ─────────
     *
     * okf/concepts/platform/data-planes.md phases B and E both bottom out here, and so
     * does `hol_uploads` later. Three different jobs, one mechanism:
     *
     *   - the published INDEX (site/index/<name>.json) — a few KB, pushed on its own,
     *     so refreshing the directory costs that file rather than a site deploy;
     *   - the encrypted BACKUP — an opaque blob the store cannot read;
     *   - a visitor's UPLOAD, later, in a DIFFERENT bucket, because the org's
     *     own fliers and a stranger's file are different trust and collapsing
     *     them would make that impossible to express.
     *
     * `endpoint` is what keeps this generalizable rather than an AWS dependency
     * wearing a holon's hat: empty means real S3, and anything else points at an
     * S3-compatible store — Cloudflare R2 (which the author already pays for),
     * MinIO, a test double. The signing is identical; only the host changes.
     *
     * Secrets are NEVER fields. `secret_key` names a secret in the passphrase-
     * locked vault, the same shape `hol_static_host.token_key` established, and
     * for the same reason: a path does not travel, and a token in the state
     * document is a credential in the state document. */
    reg("hol_object_store", "Object store - S3 or compatible", WEB,
        R"("bucket","region","endpoint","access_key_id","secret_key",)"
        R"("secret_file","prefix")",
        R"__("bucket":"Bucket name",)__"
        R"__("region":"Region (us-west-2, auto for R2, ...)",)__"
        R"__("endpoint":"Blank for AWS S3; set it for an S3-compatible store",)__"
        R"__("access_key_id":"Access key id (not a secret)",)__"
        R"__("secret_key":"Secret access key, kept in the encrypted vault",)__"
        R"__("secret_file":"...or a file beside the database (gitignored)",)__"
        R"__("prefix":"Key prefix, so one bucket can hold several things")__", 132,
        std::string(R"({"name":"assets","dir":"in","type":"assets"})"));

    /* ── A DEPLOYMENT: one rune per publish (2026-08-20) ──────────────────────
     *
     * The publish history, kept by Hormiga rather than read from the host.
     *
     * The tempting version of this is a GET against the vendor — every managed
     * CDN keeps a deployment list, so why store one? Because `web-platform.md`
     * opens by making one property the acceptance test for every vendor on the
     * page: **every cloud host is disposable.** A history that lives only in
     * their database is a history the organization loses the day it leaves, and
     * leaving is the thing that whole page is arranged to keep possible.
     *
     * So a publish writes a rune, through the dispatcher, like everything else:
     * logged, attributed, replayable, diffable, and in the `.miga` when the data
     * moves. The vendor's list is still read — it is where `vendor_id` and the
     * permanent per-deploy URL come from, so a person can LOOK at an old version
     * before restoring it — but it enriches a record we already hold.
     *
     * `state` is `live` or `superseded`, and exactly one is `live`, set by a
     * command at publish time rather than inferred from timestamps later. It is
     * not a status the app computes; it is a fact the log carries.
     *
     * It is NOT in the Antfarm palette: nobody hand-places a deployment. It is
     * evidence of something that happened. */
    core.register_glyph(
        R"({"glyph":"deployment","label":"Deployment","kind":"act",)"
        R"("fields":["host","url","at","document","lang","state","vendor_id","note"],)"
        R"("hints":{"color":"#2e6b4f","face":{"w":240,"h":96},"category":"History",)"
        R"("editors":{"note":"multiline:60","state":"combo:live,superseded"},)"
        R"__("labels":{"host":"Published through (host node)",)__"
        R"__("url":"Address this version is served at",)__"
        R"__("at":"When","document":"Website document it was built from",)__"
        R"__("lang":"Language","state":"live | superseded",)__"
        R"__("vendor_id":"The host's own id for this deployment (for rollback)",)__"
        R"__("note":"What changed"}}})__");

    // SIGN-IN — one provider per node ON PURPOSE. "Log in with Google" is a
    // choice a site makes, not an assumption Hormiga gets to bake in: an org
    // that wants email links, or none at all, unplugs this and nothing else
    // changes. Only the PUBLIC client id lives here; secrets are a token file.
    reg("hol_auth", "Sign-in - identity provider", WEB,
        R"("provider","client_id","redirect")",
        R"__("provider":"google | email-link | none","client_id":"Public client id",)__"
        R"__("redirect":"Callback URL")__", 84,
        R"({"name":"identity","dir":"out","type":"identity"})");

    // ACCOUNTS — where a visitor's login, their claim on a contact, and their
    // proposed edits live until Hormiga accepts them. Plugs into `records`
    // because that is what it imports; takes `identity` because it must know
    // which provider it trusts.
    reg("hol_accounts", "Accounts + submissions - cloud", WEB,
        R"("provider","project_url","key_file")",
        R"__("provider":"Backend (supabase, cloudflare-d1, …)",)__"
        R"__("project_url":"Project URL","key_file":"Service key file (gitignored)")__", 110,
        std::string(R"({"name":"plug","dir":"in","type":"records"},)") +
        R"({"name":"identity","dir":"in","type":"identity"})");

    // UPLOADS — visitor-supplied bytes. A separate agent from `hol_imgbb`
    // because the trust is different: an org's own fliers are one thing, and a
    // stranger's profile picture is another, and they should be able to land in
    // different buckets with different retention.
    reg("hol_uploads", "Uploads - visitor object store", WEB,
        R"("provider","bucket","key_file")",
        R"__("provider":"Store (supabase-storage, r2, imgbb, …)","bucket":"Bucket/album",)__"
        R"__("key_file":"Key file (gitignored)")__", 96, in("assets"));
}

/* The default colony: every fresh org ships wired (defaults, not assembly —
 * the Antfarm is where behavior is SEEN and changed, not bootstrapped). */
/* The default Antfarm is now LOCAL-ONLY (author, 2026-08-05): the shipped Cat
 * Colony is a purely local, locally-derived database, so its backends are a
 * local SQLite store, a local assets folder, a local HTML site build, a
 * localhost preview server, and a local CSV import. NO cloud (Supabase, Google
 * Sheets, imgbb) is seeded — those are things a user ADDS when they connect a
 * cloud backend, never a default. (A deeper Antfarm redesign is open — see
 * developer_questions Q25 / okf/concepts/platform/antfarm.md.) */

} // namespace hormiga
