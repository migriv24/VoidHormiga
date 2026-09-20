/* app/lan_share.cpp — see lan_share.hpp and okf/concepts/platform/lan-sharing.md.
 *
 * THE ORDER OF A JOIN, and why each step is where it is:
 *
 *   host GUI    build the plan and the bundle (needs the core)      start_sharing
 *   host thread accept, handshake, read who is asking               host_loop
 *   host GUI    a person answers Allow or Deny                      answer
 *   host thread add the member, send the welcome and the files      host_loop
 *   join thread handshake, ask, wait, write every file              join_loop
 *   join GUI    back up, open, keep secrets, fetch pictures         finish_join
 *
 * Threads never touch the core, and nothing sensitive is sent before Allow.
 */
#include "app/lan_internal.hpp"

namespace fs = std::filesystem;
using namespace lan_detail;

/* ── lifetime ───────────────────────────────────────────────────────────────── */

/* Threads are DETACHED and hold the runtime by shared_ptr, so it outlives the
 * application if a transfer is mid-flight at exit; there is nothing to join. */
LanRuntime::~LanRuntime() {
    sharing = false;
    if (host_thread.joinable()) host_thread.detach();
    if (join_thread.joinable()) join_thread.detach();
}

LanRuntime& LanRuntime::of(HormigaApp& app) {
    if (!app.lan) {
        app.lan = std::make_shared<LanRuntime>();
        app.lan->me = hormiga::profile::load_or_create(&app.lan->profile_error);
        // this device's networking preferences, and Void Maiz's view of who I am
        load_net_settings(app);
    }
    return *app.lan;
}

std::string LanRuntime::fingerprint(const LanRuntime& rt) {
    return hormiga::sync::fingerprint_of(rt.me.public_key);
}

/* ── THE DEVICE'S NETWORKING PREFERENCES (stage B) ───────────────────────────
 *
 * Void Maiz's `NetSettings` is about THIS device and this person -- what I
 * broadcast, what I draw of what others broadcast, whether files are fetched --
 * so it is kept beside the profile, never in the shared document. The name,
 * colour and picture stay in `profile.json`; only the switches are here. */
std::filesystem::path LanRuntime::net_settings_file() {
    return hormiga::profile::dir() / "network.json";
}

void LanRuntime::load_net_settings(HormigaApp& app) {
    maiz::NetSettings& s = app.net_settings;
    std::ifstream in(net_settings_file());
    if (in) {
        const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
        if (j.is_object()) {
            auto flag = [&j](const char* k, bool& into) {
                if (j.contains(k) && j[k].is_boolean()) into = j[k].get<bool>();
            };
            flag("send_selection", s.send.selection);
            flag("send_surfaces", s.send.surfaces);
            flag("show_marks", s.show.marks);
            flag("show_surface_badges", s.show.surface_badges);
            flag("show_private_marks", s.show.private_marks);
            flag("cautious_files", s.cautious_files);
        }
    }
    refresh_self(app);
}

bool LanRuntime::save_net_settings(HormigaApp& app) {
    const maiz::NetSettings& s = app.net_settings;
    const nlohmann::json j = {{"send_selection", s.send.selection},
                              {"send_surfaces", s.send.surfaces},
                              {"show_marks", s.show.marks},
                              {"show_surface_badges", s.show.surface_badges},
                              {"show_private_marks", s.show.private_marks},
                              {"cautious_files", s.cautious_files}};
    std::error_code ec;
    std::filesystem::create_directories(hormiga::profile::dir(), ec);
    std::ofstream out(net_settings_file(), std::ios::trunc);
    if (!out) return false;
    out << j.dump(2);
    return (bool)out;
}

/* The profile is the one place a name, a colour and a picture live; Void Maiz's
 * Profile is presentation, so it is filled FROM it rather than edited beside it. */
void LanRuntime::refresh_self(HormigaApp& app) {
    LanRuntime& rt = of(app);
    maiz::Profile& self = app.net_settings.self;
    self.id = fingerprint(rt);
    self.name = hormiga::profile::display_name(rt.me);
    self.rgb = hormiga::collab::rgb_of(rt.me.color);
    self.avatar = hormiga::profile::avatar_path(rt.me).string();
}

/* ONE ANSWER TO "MAY THIS RUNE LEAVE?", read by sync AND by presence (Void
 * Maiz, stage A: the two cannot disagree if they call the same function). The
 * private tags come from the Antfarm's hol_lan_share node, so the Antfarm still
 * decides; what changed is that no view checks a tag itself. */
maiz::ShareFilter HormigaApp::share_filter() {
    const auto share = hormiga::collab::share_settings(project(core, kAntfarmMantle));
    return [share](const maiz::SceneNode& n) { return !hormiga::collab::is_private(n, share); };
}

/* ── NAMES THAT TWO MEMBERS CANNOT BOTH MINT (2026-09-19) ─────────────────────
 *
 * The author: two devices each made a new note, both got `note-1`, and one
 * person's PRIVATE note showed the other person's presence on it. A rune's name
 * is its identity in Void Core, so two `note-1`s are not two notes that look
 * alike -- they are one note, as far as sync and presence can tell. Counting
 * what THIS scene holds cannot see what another member minted a second ago.
 *
 * So when the database is set up for sharing (its Antfarm has a hol_lan_share
 * node), the name carries four hex of this profile's fingerprint:
 * `note-3fa9-1`. Deterministic, readable, and different on every device. A
 * database that is never shared keeps `note-1`. The durable answer -- an
 * identity for a rune that is not its name -- belongs upstream (Void Core /
 * Palabra), and is asked for in MESSAGE_FOR_VOIDMAIZ_hormiga-networking-*. */
std::string HormigaApp::device_tag() {
    if (hormiga::collab::share_settings(project(core, kAntfarmMantle)).node.empty()) return {};
    const std::string fp = LanRuntime::fingerprint(LanRuntime::of(*this));
    return fp.size() >= 4 ? fp.substr(0, 4) : std::string();
}

std::string HormigaApp::mint_name(const std::string& base, int first) {
    const std::string tag = device_tag();
    const std::string stem = tag.empty() ? base : base + "-" + tag;
    for (int i = first;; ++i) {
        std::string name = stem + "-" + std::to_string(i);
        if (!scene.find(name)) return name;
    }
}

/* ── privacy at the seam (lan-sharing.md §7) ────────────────────────────────── */

std::string LanRuntime::strip_private(HormigaApp& app, const std::string& state_json,
                                      const hormiga::collab::ShareSettings& s, int* withheld,
                                      bool antfarm_too) {
    if (withheld) *withheld = 0;
    maiz::Core probe(state_json);
    if (app.on_register_glyphs) app.on_register_glyphs(probe);
    std::string active;
    for (std::string line : probe.dispatch("mantles").lines)
        if (!line.empty() && line.front() == '*') {
            line.erase(0, 1);
            while (!line.empty() && line.front() == ' ') line.erase(line.begin());
            if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
            active = line;
        }
    int n = 0;
    for (const auto& mt : mantles_of(probe)) {
        std::vector<std::string> names;
        for (const auto& node : project(probe, mt.c_str()).nodes)
            if (hormiga::collab::is_private(node, s) || (antfarm_too && mt == kAntfarmMantle))
                names.push_back(node.name);
        if (names.empty()) continue;
        probe.dispatch("use " + mt);
        for (const auto& nm : names)
            if (probe.dispatch("rune rm " + nm).ok) ++n;
    }
    if (!n) return state_json;
    if (!active.empty()) probe.dispatch("use " + active);
    if (withheld) *withheld = n;
    return probe.export_state();
}

/* ── the plan (lan-sharing.md §3) ───────────────────────────────────────────── */

Plan LanRuntime::build_plan(HormigaApp& app, std::string* state_out) {
    Plan p;
    maiz::Scene farm = project(app.core, kAntfarmMantle);
    const auto share = hormiga::collab::share_settings(farm);
    const std::vector<fs::path> dirs = app.key_dirs();
    std::error_code ec;

    std::string state = strip_private(app, app.core.export_state(), share, &p.private_withheld);
    maiz::Core probe(state);
    if (app.on_register_glyphs) app.on_register_glyphs(probe);
    farm = project(probe, kAntfarmMantle);

    std::vector<std::string> fixups;
    std::set<std::string> key_names;
    for (const auto& n : farm.nodes) {
        for (const char* f : {"token_file", "key_file", "secret_file"}) {
            const std::string v = hormiga::field_value(n, f);
            if (v.empty()) continue;
            if (n.glyph == "hol_lan_share" && std::string(f) == "key_file") continue;  // the room key
            std::string tried;
            const fs::path at = hormiga::find_key_file(v, dirs, &tried);
            const std::string rel = u8(at.filename());
            if (!fs::is_regular_file(at, ec)) {
                p.items.push_back({"key", rel, n.name + "." + f + " names it, but it is not on this computer (looked for " + tried + ")", {}, 0, false});
                continue;
            }
            if (fs::path(v).is_absolute()) fixups.push_back("set " + n.name + " " + f + " " + json_str(rel));
            if (key_names.insert(rel).second)
                p.items.push_back({"key", rel, "the key " + n.name + "." + f + " names", at, (long long)fs::file_size(at, ec), true});
        }
        for (const char* f : {"token_key", "secret_key"}) {
            const std::string v = hormiga::field_value(n, f);
            if (v.empty()) continue;
            const std::string secret = app.vault.unlocked() ? app.vault.get(v) : std::string();
            if (!secret.empty()) {
                p.vault[v] = secret;
                p.items.push_back({"vault", v, "a secret " + n.name + "." + f + " keeps in the vault", {}, 0, true});
            } else {
                p.items.push_back({"vault", v,
                                   app.vault.unlocked() ? n.name + "." + f + " names it, but the vault does not hold it"
                                                        : "kept in the vault, which is locked - unlock it to send this",
                                   {}, 0, false});
            }
        }
        if (n.glyph == "hol_sqlite") {
            const std::string v = hormiga::field_value(n, "file");
            if (v.empty()) continue;
            const fs::path at = fs::path(v).is_absolute() ? fs::path(v) : app.base_dir / v;
            if (fs::equivalent(at, app.db_file(), ec)) {
                p.items.push_back({"store", u8(at.filename()), "the working copy's SQLite - rebuilt from the database when it opens", {}, 0, false});
            } else if (fs::is_regular_file(at, ec)) {
                if (fs::path(v).is_absolute()) fixups.push_back("set " + n.name + " file " + json_str(u8(at.filename())));
                p.items.push_back({"store", u8(at.filename()), "the SQLite store " + n.name + " uses - somebody's only copy", at, (long long)fs::file_size(at, ec), true});
            }
        }
    }
    if (!fixups.empty()) {
        probe.dispatch(std::string("use ") + kAntfarmMantle);
        for (const auto& c : fixups) probe.dispatch(c);
        p.notes.push_back(std::to_string(fixups.size()) +
                          " Antfarm path(s) that were absolute on this computer arrive as plain file names");
    }

    // pictures that are online stay online
    for (const auto& n : project(probe, kDataMantle).nodes) {
        if (n.glyph != "image") continue;
        std::string path = unquote(hormiga::field_value(n, "path"));
        std::replace(path.begin(), path.end(), '\\', '/');
        const std::string url = hormiga::field_value(n, "url");
        if (path.empty() || url.empty() || share.send_hosted) continue;
        p.skip.push_back(path);
        ++p.hosted_skipped;
    }
    if (p.hosted_skipped)
        p.notes.push_back(std::to_string(p.hosted_skipped) +
                          " picture(s) are hosted online and are downloaded by the other device instead");
    if (p.private_withheld)
        p.notes.push_back(std::to_string(p.private_withheld) + " private rune(s) stay on this computer");

    std::string name = config(app.core, "org.name");
    if (name.empty()) name = app.cur_miga.empty() ? fs::path(app.state_name).stem().string()
                                                  : fs::path(app.cur_miga).stem().string();
    p.items.insert(p.items.begin(),
                   {"database", hormiga::lan::file_stem(name) + ".miga",
                    "the database, with every picture and file that exists only on this computer", {}, 0, true});
    probe.dispatch(std::string("use ") + kDataMantle);
    if (state_out) *state_out = probe.export_state();
    return p;
}

/* ── hosting ────────────────────────────────────────────────────────────────── */

bool LanRuntime::start_sharing(HormigaApp& app, std::string& error) {
    LanRuntime& rt = of(app);
    if (rt.sharing) return true;
    const maiz::Scene farm = project(app.core, kAntfarmMantle);
    const auto share = hormiga::collab::share_settings(farm);
    if (share.node.empty()) {
        error = "this database's Antfarm has no Share over LAN node - add one to allow sharing";
        return false;
    }
    if (!share.allow) {
        error = "the Antfarm's " + share.node + " says allow: no";
        return false;
    }
    if (rt.me.username.empty()) {
        error = "set a username in your Profile first - it is how the other device sees you";
        return false;
    }
    /* THE SHARED HISTORY STARTS HERE. The database gets its sync id before it is
     * packed (so the joiner's copy carries it), and this device's replica observes
     * what is about to be sent (lan-sharing.md §3b). */
    if (!sync_prepare(app, 0.0, true)) {
        error = "could not prepare this database for syncing - see the log";
        return false;
    }
    std::string state;
    rt.plan = build_plan(app, &state);
    const std::vector<fs::path> dirs = app.key_dirs();
    std::error_code ec;

    // the room key: this database's, made the first time it is shared
    fs::path rk = hormiga::find_key_file(share.key_file, dirs);
    std::string key = slurp(rk);
    if (key.size() != 32) {
        rk = fs::path(share.key_file).is_absolute() ? fs::path(share.key_file) : dirs.front() / share.key_file;
        key = hormiga::lan::new_room_key();
        if (key.size() != 32 || !write_atomic(rk, key)) {
            error = "cannot write the room key to " + u8(rk);
            return false;
        }
        owner_only(rk);
        app.log.push_back({"info", "share", "made this database's room key: " + u8(rk)});
    }
    rt.room_key = key;
    rt.room_for.clear();

    // the members registry, with the host as its first member
    const auto ms = hormiga::collab::membership_settings(farm);
    fs::path mf = hormiga::find_key_file(ms.file, dirs);
    if (!fs::is_regular_file(mf, ec)) mf = fs::path(ms.file).is_absolute() ? fs::path(ms.file) : dirs.front() / ms.file;
    rt.members_file = mf;
    std::string merr;
    if (!add_member_doc(app.on_register_glyphs, mf, request_from(rt.me), "admin", "", merr))
        app.log.push_back({"warn", "share", merr});
    rt.members_read_at = -100.0;

    // the bundle
    const fs::path tmp = fs::temp_directory_path(ec) /
                         ("hormiga-share-" + fingerprint(rt).substr(0, 8) + ".miga");
    std::set<std::string> skip(rt.plan.skip.begin(), rt.plan.skip.end());
    // the registry travels as its own file, updated with the newcomer; a copy in
    // the bundle would be stale and would land in the joiner's working folder
    if (!fs::path(ms.file).is_absolute()) skip.insert(fs::path(ms.file).generic_string());
    const auto packed = hormiga::miga::pack(state, app.base_dir, tmp, rt.plan.items.front().rel,
                                            app.assets_dir(), app.referenced_files(state), skip);
    if (!packed.ok) {
        error = "could not prepare the database: " + packed.error;
        return false;
    }
    rt.plan_miga = tmp;
    rt.plan.items.front().src = tmp;
    rt.plan.items.front().bytes = packed.bytes;
    rt.plan.items.front().why += " (" + std::to_string(packed.assets) + " files inside)";
    rt.plan.items.push_back({"members", u8(mf.filename()), "who is in this database - you, and whoever you let in", mf, (long long)fs::file_size(mf, ec), true});
    rt.plan.items.push_back({"room-key", u8(rk.filename()), "the room key, so members can see each other", rk, 32, true});
    /* THE REPLICA'S DOCUMENT, so the joiner's history starts from this one and a
     * deletion made after they join reaches them. Safe to hand over: private runes
     * were never observed into it, and a deleted rune's content does not stay in
     * it (pinned in hormiga_lan_smoke). The joiner adopts it under its OWN id --
     * replica identity never travels (Palabra, provisioning). */
    {
        std::string doc;
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            doc = rt.outgoing_doc;
        }
        const fs::path seed = fs::temp_directory_path(ec) / ("hormiga-share-" + fingerprint(rt).substr(0, 8) + ".replica.json");
        if (!doc.empty() && write_atomic(seed, doc))
            rt.plan.items.push_back({"replica", kSyncSeedFile, "where the shared history starts, so later deletions reach them", seed, (long long)doc.size(), true});
    }
    rt.plan.bytes = 0;
    for (const auto& it : rt.plan.items)
        if (it.send) rt.plan.bytes += it.bytes;

    HostJob job;
    job.rt = app.lan;
    job.keys = {rt.me.public_key, rt.me.secret_key};
    job.port = share.port;
    job.plan = rt.plan;
    job.members_file = mf;
    job.reg = app.on_register_glyphs;
    job.host_user = rt.me.username;
    std::string name = config(app.core, "org.name");
    if (name.empty()) name = fs::path(rt.plan.items.front().rel).stem().string();
    job.welcome = {{"db", {{"name", name}, {"description", config(app.core, "org.description")}}},
                   {"host", {{"user", rt.me.username}, {"color", rt.me.color}, {"fp", fingerprint(rt)}}}};
    rt.sharing = true;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.host_status = "sharing " + name + " - waiting for someone to ask";
    }
    rt.host_thread = std::thread(host_loop, std::move(job));
    rt.host_thread.detach();
    app.log.push_back({"info", "share", "sharing over the local network: " + name});
    return true;
}

void LanRuntime::stop_sharing(HormigaApp& app) {
    if (!app.lan) return;
    LanRuntime& rt = *app.lan;
    rt.sharing = false;
    std::lock_guard<std::mutex> lk(rt.mu);
    rt.host_status = "not sharing";
    if (rt.pending) rt.pending->answer = (int)Answer::Deny;
    rt.pending.reset();
}

void LanRuntime::answer(HormigaApp& app, bool allow) {
    if (!app.lan) return;
    std::lock_guard<std::mutex> lk(app.lan->mu);
    if (app.lan->pending)
        app.lan->pending->answer = (int)(allow ? Answer::Allow : Answer::Deny);
}

/* ── discovering and joining ────────────────────────────────────────────────── */

std::vector<Offer> LanRuntime::offers(HormigaApp& app) {
    std::vector<Offer> out;
    if (!app.lan || !app.lan->beacon) return out;
    const std::string mine = "hz" + fingerprint(*app.lan);
    for (const auto& peer : app.lan->beacon->peers(12)) {
        if (peer.peer_id == mine) continue;
        hormiga::lan::ExtraParts parts;
        if (!hormiga::lan::read_extra(peer.extra, parts) || !parts.has_offer) continue;
        Offer o = parts.offer;
        o.peer_id = peer.peer_id;
        o.address = peer.address;
        o.fingerprint = peer.fingerprint;
        o.seen = peer.last_seen;
        out.push_back(o);
    }
    return out;
}

bool LanRuntime::start_join(HormigaApp& app, const Offer& o, const fs::path& dest, std::string& error) {
    LanRuntime& rt = of(app);
    if (rt.joining) {
        error = "already joining";
        return false;
    }
    if (rt.me.username.empty()) {
        error = "set a username in your Profile first - the host sees it when you ask";
        return false;
    }
    if (dest.empty()) {
        error = "choose a folder for the database first";
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.join_error.clear();
        rt.join_sas.clear();
        rt.joined_miga.clear();
        rt.join_status = "starting";
    }
    JoinJob job;
    job.rt = app.lan;
    job.keys = {rt.me.public_key, rt.me.secret_key};
    job.offer = o;
    job.dest = dest;
    job.me = request_from(rt.me);
    rt.joining = true;
    rt.join_thread = std::thread(join_loop, std::move(job));
    rt.join_thread.detach();
    return true;
}

void LanRuntime::finish_join(HormigaApp& app) {
    LanRuntime& rt = of(app);
    fs::path miga;
    std::map<std::string, std::string> secrets;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        miga = rt.joined_miga;
        secrets = rt.joined_vault;
        rt.joined_miga.clear();
        rt.joined_vault.clear();
    }
    rt.finishing = false;
    if (miga.empty()) return;

    // what was open is kept, then replaced
    if (!app.cur_miga.empty()) {
        app.save_database();
    } else {
        const std::string state = app.core.export_state();
        const fs::path out = app.data_dir("backups") / ("before-joining-" + stamp() + ".miga");
        std::error_code ec;
        fs::create_directories(out.parent_path(), ec);
        const auto r = hormiga::miga::pack(state, app.base_dir, out, "before-joining", app.assets_dir(),
                                           app.referenced_files(state));
        app.log.push_back({r.ok ? "info" : "warn", "join",
                           r.ok ? "backed up the database that was open: " + u8(out)
                                : "could not back up the database that was open: " + r.error});
    }
    app.open_database(u8(miga));

    if (!secrets.empty()) {
        if (app.vault.unlocked()) {
            for (const auto& [k, v] : secrets) app.vault.set(k, v);
            const bool ok = app.vault.save(app.vault_path().string());
            app.log.push_back({ok ? "info" : "error", "join",
                               ok ? "kept " + std::to_string(secrets.size()) + " secret(s) in this device's vault"
                                  : "the vault would not save the arrived secrets"});
        } else {
            app.log.push_back({"warn", "join",
                               std::to_string(secrets.size()) +
                                   " secret(s) arrived for the vault, but this device's credentials are "
                                   "not encrypted and unlocked - they were not kept. Encrypt credentials "
                                   "(File menu), then join again or enter them by hand."});
        }
    }

    // the pictures that live online
    std::vector<std::pair<std::string, fs::path>> fetch;
    std::error_code ec;
    for (const auto& n : project(app.core, kDataMantle).nodes) {
        if (n.glyph != "image") continue;
        std::string path = unquote(hormiga::field_value(n, "path"));
        const std::string url = hormiga::field_value(n, "url");
        if (path.empty() || url.empty()) continue;
        std::replace(path.begin(), path.end(), '\\', '/');
        const fs::path at = app.resolve_file(path);
        if (fs::exists(at, ec)) continue;
        if (!url_ok(url) || !hormiga::lan::safe_rel(path) || !shell_safe(u8(at))) {
            app.log.push_back({"warn", "join", "not downloading " + n.name + ": its address or path is unusual"});
            continue;
        }
        fetch.push_back({url, at});
    }
    if (!fetch.empty() && app.on_shell_capture) {
        std::weak_ptr<LanRuntime> weak = app.lan;
        auto shell = app.on_shell_capture;
        std::thread([weak, shell, fetch] {
            int ok = 0;
            std::error_code e;
            for (const auto& [url, at] : fetch) {
                fs::create_directories(at.parent_path(), e);
                shell("curl -L -s -f -o \"" + u8(at) + "\" \"" + url + "\"");
                if (fs::exists(at, e)) ++ok;
            }
            if (auto rt = weak.lock())
                say(*rt, ok == (int)fetch.size() ? "info" : "warn",
                    "downloaded " + std::to_string(ok) + " of " + std::to_string(fetch.size()) +
                        " hosted picture(s)");
        }).detach();
        app.log.push_back({"info", "join", "downloading " + std::to_string(fetch.size()) + " hosted picture(s)"});
    }
    app.toast("joined - " + u8(miga.filename()) + " is open");
}

/* ── the members registry ───────────────────────────────────────────────────── */

bool LanRuntime::add_member(HormigaApp& app, const fs::path& file, const Request& who,
                            const std::string& role, const std::string& invited_by, std::string& error) {
    return add_member_doc(app.on_register_glyphs, file, who, role, invited_by, error);
}

std::vector<std::map<std::string, std::string>> LanRuntime::members(HormigaApp& app, const fs::path& file) {
    std::vector<std::map<std::string, std::string>> out;
    const std::string doc = slurp(file);
    if (doc.empty()) return out;
    maiz::Core c(doc);
    if (app.on_register_glyphs) app.on_register_glyphs(c);
    for (const auto& n : project(c, "members").nodes) {
        if (n.glyph != "member") continue;
        std::map<std::string, std::string> row{{"name", n.name}};
        for (const auto& f : n.fields) row[f.key] = hormiga::field_value(n, f.key);
        out.push_back(std::move(row));
    }
    return out;
}

