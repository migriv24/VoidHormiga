/* main/farm_cli.cpp — `voidhormiga-cli farm …`, Antfarm v2 from a terminal.
 * Split out of headless.cpp, which is at its length budget. */
#include "main/farm_cli.hpp"

#include "app/app_internal.hpp" // HormigaApp: the throwaway app a farm effect runs in
#include "app/farm_host.hpp"
#include "platform/profile.hpp" // the vault opens by the profile's key
#include "platform/vault.hpp"
#include "voidmaiz/gesture.hpp"

#include "json.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>

namespace hormiga {

std::vector<maiz::EffectOp> farm_effect_ops() {
    return {
        {"farm-run",
         "Run an Antfarm v2 node: an Import CSV (rows into Data), a Store (files into a river), the "
         "Data->Assets tunnel (register files). Args: <node> [apply]. Rehearses unless `apply`.",
         false,
         "an import adds runes (one undoable batch); a store COPIES FILES into another folder or PUTS "
         "THEM ONLINE through a bucket or image host, where anyone with the link can see them"},
        {"farm-check", "The smallest real test of a v2 folder, domain, bucket or key. Args: <node>.", true,
         "a folder check writes and deletes one small file; a domain, bucket or key check sends the "
         "key to that vendor and reads, and changes nothing there"},
        {"farm-preview", "Build a v2 document and everything mounted beside it on its local domain. Args: <doc>.",
         true, "rewrites site/ beside the database; in the application it also serves it on localhost"},
        {"farm-publish",
         "Build everything mounted on a v2 document's web domain and publish it there. Args: <doc>.", false,
         "PUBLISHES THE WEBSITE (and the calendars and maps mounted on the same domain). It becomes the "
         "live page everyone sees, immediately, and nothing in this application can take it back"},
    };
}

std::string farm_effect_headless(HormigaApp& app, maiz::Core& target, std::string_view op,
                                 const std::vector<std::string>& args) {
    app.load_throwaway(target.export_state());
    const HormigaApp::FarmFx fx = app.farm_effect(std::string(op), args);
    std::cerr << fx.text << "\n";
    if (!fx.commands.empty()) {
        std::string back;
        for (const auto& l : target.dispatch("mantles").lines)
            if (!l.empty() && l.front() == '*') {
                back = l.substr(1);
                while (!back.empty() && back.front() == ' ') back.erase(back.begin());
                if (auto p = back.find(" ("); p != std::string::npos) back.resize(p);
            }
        std::vector<std::string> c = fx.commands;
        if (!back.empty()) c.push_back("use " + back);
        target.dispatch(maiz::compile_commit(c)); // into the SESSION's core: journalled, attributed
    }
    return fx.ok ? farm::json_quote(fx.text.substr(0, fx.text.find('\n'))) : std::string();
}

/* ── `voidhormiga-cli farm …` — Antfarm v2 (okf/concepts/platform/antfarm/v2/cli.md)
 *
 * Void Maiz's headless session has no host-verb hook (Void Core has no
 * host-registered verbs), so `farm` is a subcommand of the PROCESS, like
 * `update`: it opens its own session on the same state document, with the same
 * glyphs, the same lock and the same journal, runs one farm verb, and closes.
 * The changes it makes are ordinary core commands in one batch, attributed to
 * the actor, so a person reviews them exactly like any other agent work. The
 * effects (`run`, `check`, `preview`, `publish`) go through `effect farm-…`, so
 * the session's gate refuses them until `--allow-effects=farm-run` (or the op)
 * grants them, exactly as it refuses `deploy-site`.
 *
 *     voidhormiga-cli --state org.state.json farm              the Connections view
 *     voidhormiga-cli --state org.state.json farm plug a.out b.in
 *     voidhormiga-cli --state org.state.json --allow-effects=farm-run farm run members apply
 *     echo sk_live_... | voidhormiga-cli farm key set cloudflare-main
 */
int run_farm_cli(const maiz::HostApp& app, const std::filesystem::path& state, const std::filesystem::path& base,
                 const std::filesystem::path& ship, const std::vector<std::string>& tok_in,
                 const std::vector<std::string>& flags) {
    maiz::SessionOptions opts;
    opts.state_path = state.string();
    if (const char* a = std::getenv("HORMIGA_ACTOR"); a && *a) opts.actor = a;
    for (const auto& f : flags) {
        if (f == "--dry-run-effects") opts.effects = maiz::EffectPolicy::DryRun;
        else if (f.rfind("--allow-effects=", 0) == 0) {
            opts.effects = maiz::EffectPolicy::Allow;
            std::string list = f.substr(16) + ",", cur;
            for (char c : list)
                if (c == ',') {
                    if (!cur.empty() && cur != "all") opts.allowed_effects.push_back(cur);
                    cur.clear();
                } else cur += c;
        } else if (f == "--allow-effects") opts.effects = maiz::EffectPolicy::Allow;
    }
    std::vector<std::string> tok = tok_in;
    maiz::Session session(app, opts);
    if (!session.start()) {
        std::cerr << "error: " << session.error() << "\n";
        return 1;
    }
    maiz::Core& core = session.core();
    std::string active;
    for (const auto& l : core.dispatch("mantles").lines)
        if (!l.empty() && l.front() == '*') {
            active = l.substr(1);
            while (!active.empty() && active.front() == ' ') active.erase(active.begin());
            if (auto p = active.find(" ("); p != std::string::npos) active.resize(p);
        }
    // this device's vault, opened the way the application opens it: by the profile's key
    hormiga::Vault vault;
    hormiga::Vault::global_init();
    const std::filesystem::path vault_file = base / "org.miga";
    const hormiga::profile::Profile me = hormiga::profile::load_or_create();
    if (hormiga::Vault::exists(vault_file.string()))
        vault.unlock(vault_file.string(), hormiga::profile::credentials_key(me));
    hormiga::farmhost::Device dev;
    dev.me = me.username;
    dev.also = {ship};

    auto reconcile = [&]() -> bool {
        const auto cmds = hormiga::farmhost::chamber_commands(core, base, {ship}, hormiga::farmhost::this_device(false),
                                                              active);
        if (cmds.empty()) return true;
        const maiz::Result res = session.batch(cmds);
        if (!res.ok) std::cerr << "error: " << res.text() << "\n";
        return res.ok;
    };
    if (!tok.empty() && tok[0] == "init" && !reconcile()) return 1; // the chambers before the graph that reads them

    const farm::Context ctx = hormiga::farmhost::make_context(core, base, &vault, dev);
    const maiz::Scene fs_ = hormiga::farmhost::project_farm(core);
    const farm::Graph g = farm::read(fs_);
    farm::Evaluator ev(g, ctx);
    const farm::VerbResult r = farm::run(tok, g, hormiga::farmhost::farm_exists(core), &ev,
                                         hormiga::farmhost::seed_info(core, "", me.username.empty() ? opts.actor : me.username));
    if (r.needs_host) {
        const std::string v = tok[0];
        if (v == "migrate") {
            const bool apply = tok.size() > 1 && tok[1] == "apply";
            if (!reconcile()) return 1; // the chambers first: documents become document nodes
            const hormiga::farmhost::MigratePlan plan = hormiga::farmhost::migrate_plan(core);
            if (!plan.refused.empty() || !apply) {
                std::cout << hormiga::farmhost::migrate_report(plan, false);
                session.close();
                return plan.refused.empty() ? 0 : 1;
            }
            std::vector<std::string> cmds = plan.commands;
            if (!active.empty()) cmds.push_back("use " + active);
            if (!session.batch(cmds).ok) {
                std::cerr << "error: the migration batch was refused; nothing changed\n";
                return 1;
            }
            // key files: sealed into this device's vault (the files stay where they are)
            if (!plan.key_files.empty()) {
                if (!vault.unlocked() && !hormiga::Vault::exists(vault_file.string()))
                    vault.create(hormiga::profile::credentials_key(me));
                for (const auto& kf : plan.key_files) {
                    std::ifstream in(base / kf.file, std::ios::binary);
                    std::string value((std::istreambuf_iterator<char>(in)), {});
                    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' '))
                        value.pop_back();
                    std::cout << kf.file << ": "
                              << (value.empty() ? std::string("not found here; set it with `farm key set`")
                                                : hormiga::farmhost::key_set(vault, vault_file, kf.entry, value))
                              << "\n";
                }
            }
            const auto arrange = farm::arrange_commands(farm::read(hormiga::farmhost::project_farm(core)));
            if (!arrange.empty()) session.batch(farm::in_farm(arrange, active));
            std::cout << hormiga::farmhost::migrate_report(plan, true);
            session.close();
            return 0;
        }
        if (v == "showcase") {
            std::string note;
            const auto cmds = hormiga::farmhost::showcase_commands(core, base, active, note);
            if (cmds.empty() || !session.batch(cmds).ok) {
                std::cerr << "error: " << (note.empty() ? "the showcase could not be built" : note) << '\n';
                return 1;
            }
            if (!reconcile()) return 1; // the new documents join the Documents chamber
            const auto arrange = farm::arrange_commands(farm::read(hormiga::farmhost::project_farm(core)));
            if (!arrange.empty()) session.batch(farm::in_farm(arrange, active));
            std::cout << note << '\n'
                      << "`farm` shows it; `farm run new-cats` imports the new arrivals; "
                         "`farm run mirror-photos` copies the photos to the USB stick folder\n";
            session.close();
            return 0;
        }
        if (v == "chambers") {
            if (!reconcile()) return 1;
            std::cout << "the chambers are in step (assets, network, documents)\n";
            session.close();
            return 0;
        }
        if (v == "run" || v == "check" || v == "preview" || v == "publish") {
            std::string c = "effect farm-" + v;
            for (std::size_t i = 1; i < tok.size(); ++i) c += " " + tok[i];
            const maiz::Result res = session.dispatch(c);
            if (!res.ok) {
                std::cerr << res.text() << "\n";
                return 1;
            }
            session.close();
            return 0;
        }
        const farm::Node* n = tok.size() > 2 ? g.find(tok[2]) : nullptr;
        if (!n || !n->kind || n->kind->id != "key") {
            std::cerr << "no key called " << (tok.size() > 2 ? tok[2] : "?") << "\n";
            return 1;
        }
        if (tok[1] == "reveal") {
            std::cerr << "refused: revealing a key is not built (keys.md: it will be a gated, logged effect)\n";
            return 1;
        }
        if (!vault.unlocked() && !hormiga::Vault::exists(vault_file.string()))
            vault.create(hormiga::profile::credentials_key(me)); // the app makes one at boot the same way
        std::string value;
        std::getline(std::cin, value);
        while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) value.pop_back();
        std::cout << hormiga::farmhost::key_set(vault, vault_file, n->field("vault_entry"), value) << "\n";
        return 0;
    }
    std::cout << r.text;
    if (!r.text.empty() && r.text.back() != '\n') std::cout << '\n';
    if (!r.ok) return 1;
    if (r.commands.empty()) return 0;
    std::vector<std::string> cmds;
    if (!tok.empty() && tok[0] == "init") {
        cmds = r.commands;
        if (!active.empty()) cmds.push_back("use " + active);
    } else cmds = farm::in_farm(r.commands, active);
    const maiz::Result res = session.batch(cmds);
    if (!res.ok) {
        std::cerr << "error: " << res.text() << "\n";
        return 1;
    }
    if (!tok.empty() && tok[0] == "init") { // lay out what was just made
        const maiz::Scene made = hormiga::farmhost::project_farm(core);
        const auto arrange = farm::arrange_commands(farm::read(made));
        if (!arrange.empty()) session.batch(farm::in_farm(arrange, active));
    }
    session.close();
    return 0;
}

int farm_cli_main(const maiz::HostApp& app, int argc, char** argv, const std::string& given_state,
                  std::filesystem::path& base, std::string& state_name, const std::filesystem::path& ship) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) != "farm" || std::string_view(argv[i - 1]) == "--state") continue;
        std::vector<std::string> tok, flags;
        for (int j = 1; j < argc; ++j) {
            const std::string a = argv[j];
            if (a.rfind("--allow-effects", 0) == 0 || a == "--dry-run-effects") flags.push_back(a);
            else if (j > i) tok.push_back(a);
        }
        const std::filesystem::path st =
            given_state.empty() ? (base / "demo-org.json") : std::filesystem::path(given_state);
        if (!given_state.empty()) { // the database's folder, not the caller's (as headless.cpp does)
            std::error_code ec;
            const auto abs = std::filesystem::absolute(st, ec);
            if (!ec && !abs.parent_path().empty()) base = abs.parent_path();
        }
        state_name = st.filename().string();
        return run_farm_cli(app, st, base, ship, tok, flags);
    }
    return -1;
}

} // namespace hormiga
