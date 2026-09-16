/* publish/panel.cpp - the operator's Publish tab.
 *
 * Split from the deploy holiday on 2026-08-20 when the file crossed its length
 * budget, and the seam was the obvious one: this is the SURFACE (what a person
 * sees, what the preflight checks, which button is green), while publish.cpp is
 * the HOLIDAY (what actually leaves the machine). Different jobs, different
 * reasons to change, and only one of them draws.
 *
 * The panel deliberately owns no capability. Build, Preview and Publish are
 * three verbs that already exist and that the command bar and an agent reach
 * identically - founding commitment 1. If something can be done here and
 * nowhere else, that is the bug. */

#include "app/app_internal.hpp"
#include "app/paths.hpp"
#include "publish/cloudflare.hpp"

using hormiga::cloudflare::trim_secret;
#include "json.hpp"
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <chrono>
#include <map>
#include <sstream>

void HormigaApp::draw_publish_body() {
    maiz::ProjectOptions ao;
    ao.mantle = kAntfarmMantle;
    const maiz::Scene farm = maiz::project_scene(core, ao);

    // ── what is being published ────────────────────────────────────────────
    std::vector<std::string> sites;
    for (const auto& m : project_all_mantles())
        for (const auto& n : m.nodes)
            if (n.glyph == "page" || n.glyph == "hero") {
                if (std::find(sites.begin(), sites.end(), m.mantle) == sites.end())
                    sites.push_back(m.mantle);
                break;
            }
    if (publish_doc.empty())
        publish_doc = cur_doc.empty() ? (sites.empty() ? "" : sites.front()) : cur_doc;

    ImGui::TextDisabled("WHAT");
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("##site", publish_doc.empty() ? "(no document)"
                                                        : publish_doc.c_str())) {
        for (const auto& sname : sites)
            if (ImGui::Selectable(sname.c_str(), sname == publish_doc))
                publish_doc = sname;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    ImGui::Combo("##plang", &publish_lang, "EN\0ES\0");
    ImGui::SameLine();
    ImGui::TextDisabled("(the website document and the language to build)");

    // ── where it goes ──────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::TextDisabled("WHERE");
    std::vector<const maiz::SceneNode*> hosts;
    /* BOTH KINDS OF HOST (2026-09-02). A `hol_github` node publishes the same
     * `site/` folder through the same `deploy_site`, so leaving it out of this
     * list would put the capability in the CLI and not in the application — the
     * exact split this file's header exists to prevent, in the same direction:
     * the person who owns the data losing an ability an agent has. */
    for (const auto& n : farm.nodes)
        if (n.glyph == "hol_static_host" || n.glyph == "hol_github")
            hosts.push_back(&n);
    if (publish_host.empty() && !hosts.empty()) publish_host = hosts.front()->name;
    const maiz::SceneNode* host = nullptr;
    for (const auto* hn : hosts)
        if (hn->name == publish_host) host = hn;
    if (hosts.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1),
                           "No publish target is wired.");
        ImGui::TextWrapped(
            "Add a `Static host - cloud deploy` node in the Antfarm tab (a "
            "managed CDN: provider, account id, project, token) or a `GitHub "
            "Pages - cloud deploy` node (a repository, a branch, a token). "
            "Hormiga does not own an account anywhere; the host is yours and it "
            "is configuration, which is why you can change it without a new "
            "version of this app.");
        return;
    }
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("##host", publish_host.c_str())) {
        for (const auto* hn : hosts)
            if (ImGui::Selectable(hn->name.c_str(), hn->name == publish_host))
                publish_host = hn->name;
        ImGui::EndCombo();
    }
    if (host) {
        /* WHAT THIS HOST IS, in its own vocabulary. A managed CDN is named by
         * its provider; a Pages repository is named by the repository and the
         * branch, and the branch is the field most likely to be wrong in a way
         * that publishes into a void. */
        std::string where = host->glyph == "hol_github"
                                ? field_value(*host, "repo") + " @ " +
                                      (field_value(*host, "branch").empty()
                                           ? std::string("gh-pages")
                                           : field_value(*host, "branch"))
                                : field_value(*host, "provider");
        // the DOMAIN comes down the wire from hol_dns, which is why it is a
        // payload and not a string duplicated on both nodes
        for (const auto& n : farm.nodes)
            if (n.glyph == "hol_dns" && !field_value(n, "domain").empty()) {
                where += " - " + field_value(n, "domain");
                break;
            }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", where.c_str());
    }

    // ── the three actions ──────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* `lang` is a PREVIEW choice, not a publish choice. It picks which page
     * the Preview button opens; it has never had any business deciding what
     * gets uploaded, and the day it looked like it did is the day a stale
     * Spanish page went live. See `hormiga::site_langs()`. */
    const std::string lang = publish_lang ? "es" : "en";
    std::error_code ec;
    const fs::path idx = data_dir("site") / ("index-" + lang + ".html");
    /* BUILT MEANS EVERY LANGUAGE IS BUILT. This asked only about the language
     * the preview toggle happened to be showing, so an operator sitting on the
     * English tab saw "site/ built just now" over a folder with no Spanish page
     * in it at all. */
    bool built = true;
    std::string unbuilt;
    for (const std::string& lg : hormiga::site_langs())
        if (!fs::exists(data_dir("site") / ("index-" + lg + ".html"), ec)) {
            built = false;
            unbuilt += (unbuilt.empty() ? "" : ", ") + lg;
        }

    if (ImGui::Button("Build", ImVec2(110, 0))) {
        if (!publish_doc.empty() && publish_doc != cur_doc) cur_doc = publish_doc;
        // EVERY language, always — a bilingual site with one language built is
        // a half-deployed site, and the house rule is all of them. The list is
        // `hormiga::site_langs()` so a third language needs no edit here.
        for (const std::string& lg : hormiga::site_langs())
            dispatch_and_reproject("effect render-site " + lg);
    }
    ImGui::SameLine();
    if (built) {
        /* THE OLDEST LANGUAGE IS THE AGE OF THE SITE. Reporting the selected
         * language's mtime would say "built just now" for a folder whose
         * Spanish half is a day old — which is the exact thing this panel is
         * now here to stop somebody believing. */
        auto ft = fs::last_write_time(
            data_dir("site") / ("index-" + hormiga::site_langs().front() + ".html"), ec);
        for (const std::string& lg : hormiga::site_langs()) {
            std::error_code fe;
            const auto t = fs::last_write_time(
                data_dir("site") / ("index-" + lg + ".html"), fe);
            if (!fe && t < ft) ft = t;
        }
        const auto age = std::chrono::duration_cast<std::chrono::minutes>(
                             std::filesystem::file_time_type::clock::now() - ft)
                             .count();
        ImGui::TextDisabled(age < 1 ? "site/ built just now"
                                    : (age < 60
                                           ? ("site/ built " + std::to_string(age) +
                                              " min ago").c_str()
                                           : "site/ built a while ago"));
    } else {
        ImGui::TextDisabled("site/ has not been built for %s yet", unbuilt.c_str());
    }

    if (ImGui::Button("Preview", ImVec2(110, 0))) {
        if (built && on_open) on_open(idx.string());
        else toast("build the site first", true);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("opens site/index-%s.html on this machine - nobody else "
                        "sees it", lang.c_str());

    // ── the one-way door ───────────────────────────────────────────────────
    /* ── THE PREFLIGHT (2026-08-20) ───────────────────────────────────────────
     *
     * The operator pressed PUBLISH and got, in the console:
     *
     *     [error] deploy: cannot read token file:
     *             C:\...\VoidHormiga\cloudflare_token.txt
     *
     * Every word of that is true and none of it is actionable from the panel.
     * The token existed — in the DATA folder — while the app had been launched
     * from the source folder, so a relative `token_file` resolved somewhere
     * with no token in it. The green button was enabled the whole time, because
     * it only checked that a host node existed and `site/` had an index.
     *
     * **A button that can be pressed should be a button that can succeed.** So
     * every precondition `deploy_site` will check is checked HERE first, each
     * one shown with the RESOLVED ABSOLUTE PATH it is looking at — because in
     * this failure the relative path was the entire problem and the absolute
     * one is the entire diagnosis.
     *
     * This is also the field agent's ask, with their correction taken: do not
     * pre-flight a credential by asking the vendor whether it is valid.
     * Cloudflare's `/tokens/verify` answers "active" for a token that cannot
     * make one useful call, because it does not enforce the token's own IP
     * condition. So this checks what WE can know for certain — is it configured,
     * is it there, is it readable, is it non-empty — and leaves the verdict on
     * the credential to the operation itself, whose error is quoted verbatim. */
    struct Check {
        bool ok;
        const char* what;
        std::string detail;
        bool fix_token = false; // offer a Browse... for the token file
    };
    std::vector<Check> checks;
    const std::string prov = host ? field_value(*host, "provider") : "";
    const bool host_is_gh = host && host->glyph == "hol_github";
    const std::string proj =
        host ? field_value(*host, host_is_gh ? "repo" : "project") : "";
    const std::string acct = host ? field_value(*host, "account_id") : "";
    const std::string keyf = host ? field_value(*host, "token_file") : "";
    const std::string dcmd = host ? field_value(*host, "deploy_cmd") : "";

    checks.push_back({!publish_doc.empty(), "a website document is selected",
                      publish_doc});
    checks.push_back({built, "site/ is built in every language",
                      built ? (data_dir("site")).string()
                            : ("not built yet: " + unbuilt)});
    checks.push_back({host != nullptr, "a publish target is wired",
                      host ? host->name
                           : std::string("add a `Static host` or `GitHub Pages` "
                                         "node in the Antfarm")});
    checks.push_back({!proj.empty(),
                      host_is_gh ? "the repository is named (owner/repo)"
                                 : "the host's project is named",
                      proj});
    /* An account id is a Cloudflare concept. Asking a GitHub Pages host for one
     * would be a red cross beside a field that node does not have, on the panel
     * whose whole job is to tell an operator whether they are ready. */
    if (!host_is_gh)
        checks.push_back({!acct.empty(), "the host's account id is set", acct});

    // the token: configured, resolved, present, readable, non-empty
    fs::path kp;
    std::string kp_tried;
    if (!keyf.empty()) kp = hormiga::find_key_file(keyf, key_dirs(), &kp_tried);
    bool tok_ok = false;
    std::string tok_detail;
    const std::string tkey = host ? field_value(*host, "token_key") : "";
    if (!tkey.empty() && vault.unlocked() && !trim_secret(vault.get(tkey)).empty()) {
        tok_ok = true;
        tok_detail = "from the vault (" + tkey + ") - travels with the .miga";
    } else if (!tkey.empty() && !vault.unlocked()) {
        tok_detail = "the vault holds it (" + tkey + ") but is locked";
    } else if (keyf.empty()) {
        tok_detail = "no credential set - paste a token, or pick a token file";
    } else if (!fs::exists(kp, ec)) {
        /* THE ABSOLUTE PATH IS THE MESSAGE. A relative `token_file` resolves
         * against the folder the application was started in, which is not
         * necessarily the folder the database lives in — and when they differ,
         * the relative form is unfixable by staring at it. */
        tok_detail = "not found - looked for " + kp_tried;
    } else {
        std::ifstream tf(kp, std::ios::binary);
        std::stringstream tb;
        if (tf) tb << tf.rdbuf();
        if (trim_secret(tb.str()).empty()) tok_detail = "file is empty: " + kp.string();
        else { tok_ok = true; tok_detail = kp.string(); }
    }
    checks.push_back({tok_ok, "the API token is readable", tok_detail, true});
    /* ── THE CHECK THAT MADE THE GREEN BUTTON UNPRESSABLE (2026-08-20) ──────
     *
     * This required `deploy_cmd` to be NON-empty. The native Cloudflare path
     * requires it to be EMPTY — `deploy_site` only takes the built-in upload
     * when no template is set, and the message to the field agent told them to
     * clear both `deploy_cmd` and `rollback_cmd` to get it.
     *
     * They did, correctly, and PUBLISH went permanently disabled. The panel was
     * demanding the exact thing the code path it leads to refuses to accept, so
     * the operator's only way to publish his own website was to type an effect
     * into the console — which is founding commitment 1 breaking in the
     * direction it always breaks, from inside the very panel written to stop it.
     *
     * The real precondition is "SOMETHING can upload this", which is true when
     * a template is set OR the provider is one we speak natively. */
    const bool native_provider = host_is_gh || prov == "cloudflare-pages" ||
                                 prov == "cloudflare_pages" || prov.empty();
    checks.push_back({!dcmd.empty() || native_provider,
                      "something can upload the site",
                      host_is_gh
                          ? std::string("built in - a commit through GitHub's "
                                        "Git Data API, no script needed")
                      : !dcmd.empty()
                          ? dcmd
                          : (native_provider
                                 ? std::string("built in - Cloudflare Pages "
                                               "Direct Upload, no script needed")
                                 : ("no built-in upload for '" + prov +
                                    "' - set deploy_cmd on the host node"))});
    checks.push_back({(bool)on_shell_capture, "this front-end can run commands",
                      on_shell_capture ? "ok" : "no shell transport"});

    bool can_publish = true;
    for (const Check& c : checks) can_publish = can_publish && c.ok;

    if (!can_publish) {
        ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.30f, 1),
                           "Not ready to publish:");
        for (const Check& c : checks) {
            if (c.ok) continue;
            ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.30f, 1), "  x");
            ImGui::SameLine();
            ImGui::TextUnformatted(c.what);
            /* THE DETAIL WRAPS, ON ITS OWN LINE. It was `SameLine` + a
             * `TextDisabled`, so the one thing the operator needed to read —
             * the absolute path the token was NOT found at — ran off the right
             * edge of a narrow panel as
             * "not found: C:\Users\migri\Documents\Projects\VoidHorm".
             * A diagnosis you cannot finish reading is not a diagnosis. */
            ImGui::Indent(18.0f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(0.62f, 0.62f, 0.64f, 1.0f));
            ImGui::TextWrapped("%s", c.detail.c_str());
            ImGui::PopStyleColor();
            ImGui::Unindent(18.0f);
            if (c.fix_token && host) {
                ImGui::Indent(18.0f);
                /* PASTE, not just browse. A path is the thing that does not
                 * travel — hand somebody this database and a `token_file`
                 * points at a file on your disk. The vault is what a `.miga` is
                 * for, so it is offered first and named as such. */
                ImGui::SetNextItemWidth(220);
                ImGui::InputTextWithHint("##pastetok", "paste an API token here",
                                         publish_token_paste,
                                         sizeof publish_token_paste,
                                         ImGuiInputTextFlags_Password);
                ImGui::SameLine();
                if (ImGui::SmallButton("Save to vault")) {
                    const std::string why =
                        hormiga::cloudflare::looks_like_token(publish_token_paste);
                    if (!why.empty()) {
                        toast("that does not look like an API token: " + why, true);
                    } else if (!vault.unlocked()) {
                        toast("unlock the credential vault first (File menu)", true);
                    } else {
                        const std::string key = "cloudflare." + host->name;
                        vault.set(key, trim_secret(publish_token_paste));
                        vault.save((base_dir / "secrets.miga").string());
                        pending_cmds.push_back("set " + host->name + " token_key " +
                                               json_str(key));
                        std::memset(publish_token_paste, 0,
                                    sizeof publish_token_paste);
                        toast("token stored in the vault - it now travels with "
                              "this database");
                    }
                }
                ImGui::Unindent(18.0f);
            }
            if (c.fix_token && host && on_pick_file) {
                ImGui::Indent(18.0f);
                /* "LOCATE CLOUDFLARE TOKEN", and it READS the file before
                 * accepting it. The operator asked for the button; the
                 * validation is because of what a wrong file costs. A token
                 * that is merely absent fails immediately and legibly. A file
                 * that contains the WRONG KIND of secret — a `cfut_…` upload
                 * token, a whole JSON blob, a pasted dashboard page — is
                 * accepted by every layer until Cloudflare rejects it, and the
                 * rejection reads like an account problem rather than a
                 * file-picking mistake. Ten lines here save that hour. */
                if (ImGui::SmallButton("Locate token...")) {
                    const std::string picked = on_pick_file(kp.string());
                    if (!picked.empty()) {
                        std::ifstream pf(picked, std::ios::binary);
                        std::stringstream pb;
                        pb << pf.rdbuf();
                        const std::string why =
                            hormiga::cloudflare::looks_like_token(pb.str());
                        if (!why.empty()) {
                            toast("that file does not look like an API token: " +
                                      why,
                                  true);
                        } else {
                            pending_cmds.push_back("set " + host->name +
                                                   " token_file " +
                                                   json_str(picked));
                            toast("token file set - press Test to confirm it "
                                  "can reach your account");
                        }
                    }
                }
                ImGui::Unindent(18.0f);
            }
        }
        ImGui::Spacing();
    }
    /* A CREDENTIAL INSIDE THE SOURCE TREE IS A CREDENTIAL ABOUT TO BE
     * COMMITTED. CLAUDE.md rule 2: this repo is a public artifact. Said here
     * rather than in a README because here is where somebody points a file
     * picker at one. */
    if (tok_ok && !ship_dir.empty()) {
        std::error_code rc;
        const fs::path repo = ship_dir.parent_path().parent_path();
        const std::string kps = kp.string(), repos = repo.string();
        if (!repos.empty() && kps.size() > repos.size() &&
            kps.compare(0, repos.size(), repos) == 0)
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1),
                               "warning: that token is inside the Hormiga source "
                               "folder. Move it beside your database.");
    }

    /* TEST THE CREDENTIAL AGAINST THE THING IT HAS TO DO.
     *
     * Not `/tokens/verify`: it answers "active" for a token that fails every
     * real call, because it does not enforce the token's own IP condition. The
     * field agent lost an afternoon to exactly that green light. This lists the
     * account's Pages projects — the resource a deploy touches — so a pass here
     * predicts the operation and a failure quotes the vendor. */
    /* THE TEST BUTTON FOLLOWS THE CREDENTIAL, WHEREVER IT LIVES. This was
     * gated on `!keyf.empty()` — a token FILE — so the moment an operator took
     * our own advice and moved the secret into the vault (`token_key`, which
     * travels with the .miga), the button vanished. The one affordance for
     * checking a credential disappeared exactly when the credential was stored
     * the way this application recommends storing it.
     *
     * `tok_ok` already means "a credential resolved, by whichever route", so it
     * is the right condition, and the resolution below is vault-first to match
     * `deploy_site`. Two places that resolve one secret must resolve it in the
     * same order or the test stops predicting the operation, which is the whole
     * point of the button. */
    if (host && tok_ok && on_shell_capture) {
        /* ── ONE CHECK, BOTH HOSTS (2026-09-02) ─────────────────────────────
         *
         * This called `cloudflare::check_token` directly, which meant the
         * button read "Test this token" and tested a vendor the selected host
         * might not be. `check_host` is the verb both front-ends now share
         * (field report A5) — it resolves the credential the same way
         * `deploy_site` does, dispatches on the host's glyph, and reports
         * several lines rather than a verdict, because "can I publish?" is four
         * questions with four different fixes. */
        if (ImGui::SmallButton("Test this host")) {
            const size_t from = log.size();
            const int rc = check_host(farm, host->name);
            std::string first_fail;
            for (size_t i = from; i < log.size(); ++i)
                if (log[i].op == "host" && log[i].level == "error" &&
                    first_fail.empty())
                    first_fail = log[i].msg;
            publish_token_check = rc == 0 ? "ok"
                                  : first_fail.empty() ? "check failed"
                                                       : first_fail;
            toast(rc == 0 ? "these credentials can reach " + host->name
                          : "host check failed - see the panel",
                  rc != 0);
        }
        if (!publish_token_check.empty()) {
            ImGui::SameLine();
            if (publish_token_check == "ok")
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 0.45f, 1),
                                   "token reaches this account");
            else
                ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.30f, 1), "%s",
                                   publish_token_check.c_str());
        }
    }


    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.52f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.62f, 0.34f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.11f, 0.44f, 0.24f, 1.0f));
    if (!can_publish) ImGui::BeginDisabled();
    if (ImGui::Button("PUBLISH", ImVec2(230, 38))) publish_confirm = true;
    if (!can_publish) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.85f, 0.72f, 0.35f, 1),
                       built ? "live immediately, for everyone"
                             : "build the site first");

    /* THE CONFIRMATION QUOTES THE EFFECT'S OWN CONSEQUENCE. `deploy-site` is
     * the only effect declared `reversible: false`, and the sentence a headless
     * refusal prints was written for exactly this moment. A GUI that softened it
     * — or skipped it because a modal "feels heavy" — would be giving the person
     * at the keyboard less information than the agent gets, which is the
     * failure this whole panel exists to correct. */
    if (publish_confirm) {
        ImGui::OpenPopup("Publish this website?");
        publish_confirm = false;
    }
    if (ImGui::BeginPopupModal("Publish this website?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "PUBLISHES THE WEBSITE. Whatever is in site/ becomes the live page "
            "everyone sees, immediately, and nothing in this application can "
            "take it back.");
        ImGui::Spacing();
        ImGui::Text("Document:  %s", publish_doc.c_str());
        ImGui::Text("Host:      %s", publish_host.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Undo does not reach past this line. A past version "
                            "can be restored from History below,");
        ImGui::TextDisabled("if the host keeps one and a rollback command is set.");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.52f, 0.28f, 1.0f));
        if (ImGui::Button("Publish now", ImVec2(150, 0))) {
            ImGui::CloseCurrentPopup();
            const std::string hostname = publish_host;
            const std::string doc = publish_doc;
            /* ── REBUILD EVERY LANGUAGE, THEN PUBLISH ────────────────────────
             *
             * Not "if the operator remembered to press Build". The upload is
             * the whole `site/` folder, so whether the Spanish page is current
             * was a property of whatever was rendered last — and a person who
             * has been editing Spanish copy in the Builder and then opens this
             * panel has no way to see that the folder is stale.
             *
             * Rendering here dispatches through the same door the Build button
             * uses, so it is in the journal like everything else, and it makes
             * the green button mean what its confirmation says it means:
             * "whatever is in site/ becomes the live page" is only a promise
             * worth making if site/ is what the database currently says.
             *
             * On the UI thread, before `run_busy`, because `render_site` reads
             * `cur_doc` and the projection. */
            if (!publish_doc.empty() && publish_doc != cur_doc) cur_doc = publish_doc;
            for (const std::string& lg : hormiga::site_langs())
                dispatch_and_reproject("effect render-site " + lg);
            const std::string langs = hormiga::site_langs_str();
            run_busy("publishing " + doc + "...", [this, hostname, doc, langs] {
                maiz::ProjectOptions o;
                o.mantle = kAntfarmMantle;
                const maiz::Scene f = maiz::project_scene(core, o);
                const std::string url = deploy_site(f, hostname);
                if (url.empty()) {
                    toast("publish failed - see the log", true);
                    return;
                }
                /* THE RECORD GOES THROUGH THE DISPATCHER, which is the whole
                 * reason it is worth having: one batch, in the journal, with an
                 * actor, replayable, and in the `.miga` if this organization
                 * ever moves to another host. */
                /* `lang` here was the preview toggle — "en" or "es", one of
                 * them, for a deploy that sent both. The record is what the
                 * history and any future rollback read; it says what actually
                 * went out. */
                for (const auto& c : deployment_record(f, hostname, url, doc, langs))
                    dispatch_and_reproject(c);
                dispatch_and_reproject(std::string("use ") + kAntfarmMantle);
                toast("published - " + (url == "ok" ? std::string("live") : url));
            });
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // ── history ────────────────────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("HISTORY");

    std::vector<const maiz::SceneNode*> deploys;
    for (const auto& n : farm.nodes)
        if (n.glyph == "deployment") deploys.push_back(&n);
    // newest first: `at` is ISO, so it sorts as a string
    std::sort(deploys.begin(), deploys.end(),
              [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                  return field_value(*a, "at") > field_value(*b, "at");
              });

    if (deploys.empty()) {
        ImGui::TextDisabled("Nothing has been published from this database yet.");
        ImGui::TextWrapped(
            "Every publish is recorded here as a rune in the antfarm mantle - "
            "not read back from the host, because a history that lives in "
            "somebody else's database is one you lose when you leave them.");
    }
    for (const auto* dn : deploys) {
        ImGui::PushID(dn->name.c_str());
        const bool live = field_value(*dn, "state") == "live";
        const std::string url = field_value(*dn, "url");
        if (live) {
            ImGui::TextColored(ImVec4(0.35f, 0.78f, 0.45f, 1), "%s", "\xe2\x97\x8f");
            ImGui::SameLine();
        } else {
            ImGui::TextDisabled(" ");
            ImGui::SameLine();
        }
        std::string when = field_value(*dn, "at");
        if (when.size() >= 16) when = when.substr(0, 10) + "  " + when.substr(11, 5);
        ImGui::TextUnformatted(when.c_str());
        ImGui::SameLine(150);
        ImGui::TextDisabled("%s", field_value(*dn, "document").c_str());
        ImGui::SameLine(320);
        /* [VIEW] BEFORE [RESTORE], which is the report's point and the right
         * one: every managed host keeps each deployment at its own permanent
         * URL, so looking at the old version before deciding is one click. That
         * is what turns a rollback from a frightening button into an ordinary
         * one. */
        if (!url.empty() && url != "ok") {
            if (ImGui::SmallButton("view") && on_open) on_open(url);
            ImGui::SameLine();
        }
        if (!live) {
            const std::string vid = field_value(*dn, "vendor_id");
            if (vid.empty()) ImGui::BeginDisabled();
            if (ImGui::SmallButton("restore")) publish_restore = dn->name;
            if (vid.empty()) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "this deployment has no host-side id, so the host cannot "
                        "be asked to put it back;\nrebuild and publish instead");
            }
        } else {
            ImGui::TextDisabled("live");
        }
        ImGui::PopID();
    }

    if (!publish_restore.empty()) {
        ImGui::OpenPopup("Restore this version?");
    }
    if (ImGui::BeginPopupModal("Restore this version?", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("This puts an earlier version back in front of "
                           "everyone, immediately. It does not change anything "
                           "in your database - only what the host is serving.");
        ImGui::Spacing();
        if (ImGui::Button("Restore", ImVec2(130, 0))) {
            const std::string target = publish_restore;
            publish_restore.clear();
            ImGui::CloseCurrentPopup();
            std::string vid, hostn;
            for (const auto& n : farm.nodes)
                if (n.name == target) {
                    vid = field_value(n, "vendor_id");
                    hostn = field_value(n, "host");
                }
            run_busy("restoring " + target + "...", [this, target, vid, hostn] {
                maiz::ProjectOptions o;
                o.mantle = kAntfarmMantle;
                if (rollback_site(maiz::project_scene(core, o), hostn, vid).empty()) {
                    toast("restore failed - see the log", true);
                    return;
                }
                // the history is a record of what is live, so it moves too
                maiz::ProjectOptions o2;
                o2.mantle = kAntfarmMantle;
                for (const auto& n : maiz::project_scene(core, o2).nodes)
                    if (n.glyph == "deployment" && field_value(n, "state") == "live")
                        dispatch_and_reproject("set " + n.name + " state superseded");
                dispatch_and_reproject("set " + target + " state live");
                toast("restored " + target);
            });
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110, 0))) {
            publish_restore.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

/* ── IMAGE HOSTING IN THE ANTFARM'S GUI (2026-09-15) ──────────────────────────
 *
 * The author: *"NOT JUST IN THE CLI, but in the GUI as well! you kinda neglect
 * the GUI of the antfarm, or you tend to. I do eventually want to work on a
 * larger overhaul of the antfarm, but the gui should still function until then."*
 *
 * Two surfaces over domain/hosting.hpp, the table the CLI reads too:
 *
 *   A FACE on every node that can host images: can it answer right now, and if
 *   not why not; is it the node images go through; and a "Use for images" button
 *   that makes it so (`config set hosting.images <node>`).
 *
 *   A PANEL at the top of the Antfarm inspector: which host (or automatic), what
 *   it does and what it needs, how many images are online and how many are not,
 *   and one button that puts the rest online.
 *
 * The state both read is refreshed at most once a frame (the Antfarm scene is a
 * handful of nodes) and the image counts every few seconds (the data is not). */
#include "domain/hosting.hpp"

namespace {
struct HostingView {
    int frame = -1;
    std::string chosen;   // config hosting.images ("" = automatic)
    std::string base_url; // site.base_url, for the website hosts
    std::string active;   // the node images would go through right now
    std::string why;      // why that node cannot answer ("" = it can)
    double counted_at = -100.0;
    int online = 0, offline = 0, no_file = 0;
};
HostingView g_hosting;
std::function<void()> g_hosting_refresh;

std::string unquote_config(std::string v) {
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v == "null" ? std::string() : v;
}
const ImVec4 kHostGreen(0.35f, 0.70f, 0.40f, 1.0f), kHostAmber(0.85f, 0.60f, 0.15f, 1.0f);
} // namespace

void HormigaApp::register_hosting_faces() {
    g_hosting_refresh = [this] {
        const int fc = ImGui::GetFrameCount();
        if (g_hosting.frame == fc) return;
        g_hosting.frame = fc;
        g_hosting.chosen = unquote_config(core.dispatch("config get hosting.images").data);
        std::string bu = unquote_config(core.dispatch("config get site.base_url").data);
        while (!bu.empty() && bu.back() == '/') bu.pop_back();
        g_hosting.base_url = bu;
        maiz::ProjectOptions ao;
        ao.mantle = kAntfarmMantle;
        const maiz::Scene farm = maiz::project_scene(core, ao);
        std::string why;
        const maiz::SceneNode* h = pick_image_host(farm, g_hosting.chosen, bu, &why);
        g_hosting.active = h ? h->name : std::string();
        g_hosting.why = why;
        if (ImGui::GetTime() - g_hosting.counted_at > 3.0) {
            g_hosting.counted_at = ImGui::GetTime();
            g_hosting.online = g_hosting.offline = g_hosting.no_file = 0;
            maiz::ProjectOptions dpo;
            dpo.mantle = kDataMantle;
            const maiz::Scene data = maiz::project_scene(core, dpo);
            for (const auto& n : data.nodes) {
                if (n.glyph != "image") continue;
                const std::string p = field_value(n, "path");
                std::error_code ec;
                if (!field_value(n, "url").empty()) ++g_hosting.online;
                else if (!p.empty() &&
                         fs::exists(fs::path(p).is_absolute() ? fs::path(p) : base_dir / p, ec))
                    ++g_hosting.offline;
                else ++g_hosting.no_file;
            }
        }
    };

    for (const auto& host : hormiga::hosting::kAssetHosts) {
        const std::string glyph = host.glyph;
        faces.by_glyph[glyph] = [this, glyph](maiz::FaceContext& ctx) {
            g_hosting_refresh();
            const std::string problem = host_problem(ctx.node, g_hosting.base_url);
            const bool chosen = g_hosting.chosen == ctx.node.name;
            const bool in_use = g_hosting.active == ctx.node.name && g_hosting.why.empty();
            ImGui::SetCursorScreenPos(ctx.pos);
            ImGui::BeginGroup();
            ImGui::PushTextWrapPos(ctx.pos.x + ctx.size.x);
            if (!problem.empty())
                ImGui::TextColored(kHostAmber, "cannot host images yet: %s", problem.c_str());
            else if (in_use)
                ImGui::TextColored(kHostGreen, ICON_FA_CIRCLE_CHECK "  images go online here");
            else
                ImGui::TextDisabled("can host images");
            if (glyph == "hol_imgbb" && mirror_count > 0)
                ImGui::TextDisabled("mirror: %d links saved locally", mirror_count);
            ImGui::PopTextWrapPos();
            ImGui::EndGroup();
            ImGui::SetCursorScreenPos(ImVec2(ctx.pos.x, ctx.pos.y + ctx.size.y - 26.0f));
            const bool imgbb = glyph == "hol_imgbb";
            const float w = imgbb ? ctx.size.x * 0.5f - 2.0f : ctx.size.x;
            ImGui::BeginDisabled(chosen);
            if (ImGui::Button(chosen ? "Used for images##host" : "Use for images##host",
                              ImVec2(w, 0)))
                ctx.commands.push_back("config set hosting.images " + json_str(ctx.node.name));
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("images you choose to host online go through this node");
            if (imgbb) {
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Mirror to local##host", ImVec2(w, 0)))
                    ctx.commands.push_back("effect mirror-images");
            }
        };
    }
}

void HormigaApp::draw_hosting_panel() {
    if (!g_hosting_refresh) return;
    if (!ImGui::CollapsingHeader(ICON_FA_CLOUD_ARROW_UP "  Hosting images online",
                                 ImGuiTreeNodeFlags_DefaultOpen))
        return;
    g_hosting_refresh();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("An email can only show an image that has a link on the internet. "
                        "Which node makes that link is the Antfarm's decision: ImgBB, an "
                        "object store (S3, or Cloudflare R2 on your own domain), or your "
                        "website itself.");
    maiz::ProjectOptions ao;
    ao.mantle = kAntfarmMantle;
    const maiz::Scene farm = maiz::project_scene(core, ao);
    std::vector<const maiz::SceneNode*> hosts;
    for (const auto& n : farm.nodes)
        if (hormiga::hosting::asset_host(n.glyph)) hosts.push_back(&n);
    if (hosts.empty()) {
        ImGui::TextColored(kHostAmber, "No node here can host images yet.");
        ImGui::TextWrapped("Drag one in from the palette: ImgBB; Object store (fill in its "
                           "public_url); or Static host / GitHub Pages to use your own "
                           "website, which needs the site's address in Style > Site.");
        ImGui::PopTextWrapPos();
        ImGui::Separator();
        return;
    }
    const char* kAuto = "automatic (the first that can answer)";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##imagehost",
                          g_hosting.chosen.empty() ? kAuto : g_hosting.chosen.c_str())) {
        if (ImGui::Selectable(kAuto, g_hosting.chosen.empty()))
            pending_cmds.push_back("config set hosting.images \"\"");
        for (const auto* h : hosts) {
            const auto* p = hormiga::hosting::asset_host(h->glyph);
            const std::string label = h->name + "  -  " + p->label;
            if (ImGui::Selectable(label.c_str(), g_hosting.chosen == h->name))
                pending_cmds.push_back("config set hosting.images " + json_str(h->name));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s\nneeds: %s", p->how, p->needs);
        }
        ImGui::EndCombo();
    }
    const maiz::SceneNode* active = nullptr;
    for (const auto* h : hosts)
        if (h->name == g_hosting.active) active = h;
    if (!active || !g_hosting.why.empty()) {
        ImGui::TextColored(kHostAmber, "%s",
                           g_hosting.why.empty() ? "no image host can answer yet"
                                                 : g_hosting.why.c_str());
    } else {
        const auto* p = hormiga::hosting::asset_host(active->glyph);
        ImGui::TextColored(kHostGreen, ICON_FA_CIRCLE_CHECK "  %s %s", active->name.c_str(),
                           p->how);
    }
    ImGui::Text("%d online    %d not online yet    %d with no file here", g_hosting.online,
                g_hosting.offline, g_hosting.no_file);
    ImGui::PopTextWrapPos();
    ImGui::BeginDisabled(g_hosting.offline == 0 || !active || !g_hosting.why.empty());
    if (ImGui::Button("Host the images that are not online yet")) {
        run_busy("Putting images online", [this] {
            const int n = host_missing_images();
            toast(std::to_string(n) + " image(s) put online");
            g_hosting.counted_at = -100.0;
        });
    }
    ImGui::EndDisabled();
    ImGui::Separator();
}
