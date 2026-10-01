/* antfarm/farm_face.cpp — readiness and the lines each node shows.
 *
 * The readiness contract (A6): one state per node, computed per device, with
 * the one thing missing said in words. A status that would need the network is
 * never faked as "online" (rivers.md §5): the prototype has no checks that
 * reach out, so a cloud node's best state is "ready", meaning nothing is KNOWN to
 * be wrong, and its face says it has not been checked. */
#include "antfarm/farm_eval.hpp"

#include <cctype>
#include <map>
#include <set>

namespace farm {

namespace {

std::string commas(long long n) {
    std::string s = std::to_string(n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

void set(Ready& r, const char* state, std::string why) {
    // a worse state wins; `planned` and `unconfigured` are worst because nothing can run
    static const char* order[] = {"ready", "idle", "needs", "failing", "unconfigured", "planned"};
    auto rank = [](const std::string& s) {
        for (int i = 0; i < 6; ++i)
            if (s == order[i]) return i;
        return 0;
    };
    if (rank(state) >= rank(r.state)) {
        r.state = state;
        r.why = std::move(why);
    }
}

bool is_url(const std::string& s) { return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0; }

std::string base_name(const std::string& p) {
    const auto s = p.find_last_of("/\\");
    return s == std::string::npos ? p : p.substr(s + 1);
}

} // namespace

Face Evaluator::face(const std::string& name) {
    Face f;
    const Node* n = g_.find(name);
    if (!n || !n->kind) {
        f.ready = {"failing", "not a v2 node"};
        return f;
    }
    const Kind& kd = *n->kind;
    const std::string& k = kd.id;
    if (kd.planned) {
        f.ready = {"planned", "no code answers this yet"};
        f.lines.push_back(kd.doc);
        return f;
    }

    // generic: a required input with nothing plugged in
    for (const auto& p : kd.ports)
        if (!p.out && !p.optional && g_.into(n->name, p.name).empty())
            set(f.ready, "needs", "not connected: " + p.name);

    // outputs: strands, and whether anything uses this node (A2: wiring is read)
    bool used = false;
    for (const auto& p : kd.ports) {
        if (!p.out) continue;
        if (!g_.out_of(n->name, p.name).empty()) used = true;
        const Value v = eval(n->name, p.name);
        f.strands[p.name] = p.type == "mantle" ? strands_for((long long)v.runes.size())
                            : p.type == "river" ? std::max<int>(1, std::min<int>(5, (int)v.refs.size()))
                                                : 1;
    }

    if (k == "miga") {
        long long total = 0;
        for (const char* c : {"data", "assets", "network", "documents"}) {
            const auto rs = chamber(c);
            long long b = 0;
            for (const auto& r : rs) b += r.bytes;
            total += b;
            std::string label = c;
            label[0] = (char)std::toupper((unsigned char)label[0]);
            f.lines.push_back(label + "  " + commas((long long)rs.size()) + " runes  " + human_bytes(b));
        }
        f.lines.push_back("in all " + human_bytes(total));
        const auto rest = g_.into(n->name, "rests-in");
        if (!rest.empty()) {
            bool live = false;
            for (const auto& r : eval(rest[0]->from, rest[0]->out).refs)
                if (const Node* res = g_.find(r); res && res->kind && res->kind->id == "folder") live = true;
            if (!live)
                set(f.ready, "failing", "a database being written must rest on this device: its river has no folder");
        }
    } else if (k == "separate") {
        for (const char* c : {"data", "assets", "network", "documents"})
            f.lines.push_back(std::string(c) + "  " + commas((long long)eval(n->name, c).runes.size()));
    } else if (k == "filter") {
        const long long kept = (long long)eval(n->name, "kept").runes.size();
        const long long all = kept + (long long)eval(n->name, "rest").runes.size();
        f.lines.push_back("kept " + commas(kept) + " of " + commas(all));
        f.lines.push_back("where " + eval(n->name, "kept").query.text());
    } else if (k == "join" || k == "count" || k == "measure") {
        const Value m = k == "join" ? eval(n->name, "mantle") : input(*n, "mantle", "mantle");
        long long b = 0;
        for (const auto& r : m.runes) b += r.bytes;
        f.lines.push_back(commas((long long)m.runes.size()) + " runes" + (k == "count" ? "" : ", " + human_bytes(b)));
    } else if (k == "tag" || k == "glyph" || k == "date" || k == "field" || k == "and" || k == "or" || k == "not") {
        const Query q = eval(n->name, "query").query;
        f.lines.push_back("= " + q.text());
        if ((k == "tag" && n->field("expr").empty()) || (k == "glyph" && n->field("glyphs").empty()) ||
            (k == "date" && n->field("when").empty()) || (k == "field" && n->field("key").empty()))
            set(f.ready, "unconfigured", "selects everything until it is given a rule");
    } else if (k == "tunnel-assets") {
        /* Data -> Assets, checked against the REAL Assets chamber. A reference is
         * known by the file it names or by the content address inside that name. */
        std::set<std::string> reg_files, reg_shas, referenced;
        std::map<std::string, std::string> asset_file; // rune -> file
        for (const auto& r : input(*n, "assets", "mantle").runes) {
            const std::string file = farm::field_of(r.node, "file"), sha = farm::field_of(r.node, "sha256");
            if (!file.empty()) reg_files.insert(file);
            if (!sha.empty()) reg_shas.insert(sha);
            asset_file[r.node.name] = file;
        }
        auto sha_in = [](const std::string& p) {
            int run = 0;
            for (std::size_t i = 0; i < p.size(); ++i) {
                run = std::isxdigit((unsigned char)p[i]) ? run + 1 : 0;
                if (run == 64) return p.substr(i + 1 - 64, 64);
            }
            return std::string();
        };
        long long refd = 0, missing = 0, remote = 0, unregistered = 0;
        for (const auto& r : input(*n, "data", "mantle").runes)
            for (const auto& fl : r.node.fields) {
                if (fl.editor != "image" && !(r.node.glyph == "image" && fl.key == "path")) continue;
                const std::string v = farm::field_of(r.node, fl.key);
                if (v.empty()) continue;
                ++refd;
                if (is_url(v)) { ++remote; continue; }
                referenced.insert(v);
                const std::string sha = sha_in(v);
                if (!reg_files.count(v) && (sha.empty() || !reg_shas.count(sha))) ++unregistered;
                if (ctx_.exists && !ctx_.exists(v)) ++missing;
            }
        long long unheld = 0, orphans = 0;
        for (const auto& [rune, file] : asset_file) {
            if (ctx_.exists && !file.empty() && !ctx_.exists(file)) ++unheld;
            if (!referenced.count(file)) ++orphans;
        }
        f.lines.push_back(commas((long long)asset_file.size()) + " assets registered, " + commas(refd) + " references");
        f.lines.push_back("missing " + commas(missing) + "   remote only " + commas(remote));
        f.lines.push_back("unregistered " + commas(unregistered) + "   not on this device " + commas(unheld));
        f.lines.push_back("orphans " + commas(orphans) + " (assets no rune names)");
        if (unregistered > 0) set(f.ready, "needs", commas(unregistered) + " referenced files are not in the Assets chamber (farm chambers)");
        if (missing > 0) set(f.ready, "needs", commas(missing) + " pictures are missing from this device");
    } else if (k == "website" || k == "newsletter" || k == "calendar" || k == "map") {
        const std::string doc = n->field("document");
        const long long sees = (long long)input(*n, "data", "mantle").runes.size();
        long long all = 0;
        for (const char* c : {"data", "assets", "network", "documents"}) all += (long long)chamber(c).size();
        f.lines.push_back("document: " + (doc.empty() ? std::string("(none named)") : doc));
        f.lines.push_back("can see " + commas(sees) + " of " + commas(all) + " runes");
        if (g_.into(n->name, "data").empty()) set(f.ready, "needs", "not connected: this document can see nothing");
        if (doc.empty()) set(f.ready, "unconfigured", "no document named");
        else if (ctx_.document_exists && !ctx_.document_exists(doc))
            set(f.ready, "unconfigured", "there is no " + k + " called " + doc);
        bool anywhere = false;
        for (const char* p : {"preview", "publish"}) {
            const auto outs = g_.out_of(n->name, p);
            std::string to;
            for (const Wire* w : outs) {
                to += (to.empty() ? "" : ", ") + w->to;
                const Face df = face(w->to);
                if (df.ready.state == "ready") anywhere = true;
                else if (std::string(p) == "publish") set(f.ready, "needs", w->to + " needs: " + df.ready.why);
            }
            f.lines.push_back(std::string(p) + " -> " + (to.empty() ? "nowhere yet" : to));
        }
        if (!anywhere) set(f.ready, "needs", "nowhere to go from this device");
    } else if (k == "river") {
        const Value v = eval(n->name, "river");
        bool home = false;
        for (const auto& r : v.refs)
            if (const Node* res = g_.find(r); res && res->kind && res->kind->id == "folder") home = true;
        f.lines.push_back(std::to_string(v.refs.size()) + " reservoir" + (v.refs.size() == 1 ? "" : "s"));
        for (const auto& r : v.refs) {
            const Node* res = g_.find(r);
            const std::string reach = !res || !res->kind ? "?" : res->kind->id == "folder" ? "here" : "cloud";
            f.lines.push_back("  " + r + "  (" + reach + ")");
        }
        if (!home && !v.refs.empty()) f.lines.push_back("remote only: nothing is kept on this device");
    } else if (k == "folder") {
        const std::string p = n->field("path");
        if (p.empty()) set(f.ready, "unconfigured", "no folder named");
        else if (ctx_.folder) {
            const Folder fo = ctx_.folder(p);
            f.lines.push_back(p + (fo.exists ? "" : "  (not there yet)"));
            f.lines.push_back(commas(fo.files) + " files, " + human_bytes(fo.bytes));
            if (fo.free >= 0) f.lines.push_back(human_bytes(fo.free) + " free on this disk");
        }
        f.lines.push_back("runs on: " + n->placement());
    } else if (k == "bucket" || k == "image-host") {
        if (k == "bucket" && n->field("bucket").empty()) set(f.ready, "unconfigured", "no bucket named");
        const auto kw = g_.into(n->name, "key");
        if (!kw.empty()) {
            const Face kf = face(kw[0]->from);
            if (kf.ready.state != "ready") set(f.ready, "needs", "its key: " + kf.ready.why);
            f.lines.push_back("key: " + kw[0]->from);
        }
        if (k == "bucket" && !n->field("public_url").empty()) f.lines.push_back("public at " + n->field("public_url"));
        f.lines.push_back("not checked (checks reach the network; not in the prototype)");
    } else if (k == "store") {
        f.lines.push_back("writes when run, never by itself");
    } else if (k == "gauge") {
        f.lines.push_back("used " + eval(n->name, "used").text);
        f.lines.push_back("free " + eval(n->name, "free").text);
    } else if (k == "key") {
        const std::string entry = n->field("vault_entry");
        f.lines.push_back("provider: " + (n->field("provider").empty() ? std::string("?") : n->field("provider")));
        if (entry.empty()) set(f.ready, "unconfigured", "no vault entry named");
        else {
            const std::string st = ctx_.key_state ? ctx_.key_state(entry) : "missing";
            f.lines.push_back(st == "present" ? "value: on this device" : "value: " + st);
            if (st == "missing") set(f.ready, "needs", "no value on this device (farm key set " + n->name + ")");
            else if (st == "locked") set(f.ready, "needs", "the vault is locked");
            else if (st == "no-vault") set(f.ready, "needs", "this database has no vault yet");
        }
        if (!n->field("expires").empty()) f.lines.push_back("expires " + n->field("expires"));
        if (!n->field("added_by").empty()) f.lines.push_back("added by " + n->field("added_by"));
        long long users = (long long)g_.out_of(n->name, "key").size();
        f.lines.push_back("used by " + commas(users) + " node" + (users == 1 ? "" : "s"));
    } else if (k == "expiry") {
        const Value v = eval(n->name, "days");
        f.lines.push_back(v.text);
        if (v.has_number && v.number < 0) set(f.ready, "failing", "the key has expired");
        else if (v.has_number && v.number < 14) set(f.ready, "needs", "the key expires in " + v.text);
    } else if (k == "local-domain" || k == "web-domain") {
        if (k == "local-domain") {
            const std::string port = n->field("port").empty() ? "8780" : n->field("port");
            f.lines.push_back("http://localhost:" + port);
            if (!ctx_.serves_local)
                set(f.ready, "needs", "nowhere to go from this device: a " + ctx_.device +
                                          " cannot serve a local address yet");
            f.lines.push_back("runs on: " + n->placement());
        } else {
            const std::string host = n->field("host"), target = n->field("target");
            f.lines.push_back((host.empty() ? std::string("(no host)") : host) + "  " + target);
            if (!n->field("name").empty()) f.lines.push_back("name: " + n->field("name"));
            if (host.empty() || target.empty()) set(f.ready, "unconfigured", "no host or target");
            const auto kw = g_.into(n->name, "key");
            if (!kw.empty()) {
                const Face kf = face(kw[0]->from);
                if (kf.ready.state != "ready") set(f.ready, "needs", "its key: " + kf.ready.why);
            }
        }
        std::set<std::string> mounts;
        for (const Wire* w : g_.into(n->name, "renditions")) {
            const Node* d = g_.find(w->from);
            std::string m = d ? d->field("mount") : "";
            if (m.empty()) m = "/";
            f.lines.push_back(m + "  <- " + w->from + " (" + w->out + ")");
            if (!mounts.insert(m).second) set(f.ready, "failing", "two documents are mounted at " + m);
        }
        if (g_.into(n->name, "renditions").empty()) f.lines.push_back("nothing is served here yet");
    } else if (k == "profile") {
        const std::string u = n->field("username").empty() ? ctx_.me : n->field("username");
        f.lines.push_back(u.empty() ? std::string("(no username yet)") : u);
        const std::string pres = ctx_.presence ? ctx_.presence(u) : "";
        f.lines.push_back(pres.empty() ? "not seen on this network" : pres);
        if (n->field("username").empty()) f.lines.push_back("this device (" + ctx_.device + ")");
    } else if (k == "import-csv") {
        const std::string file = n->field("file");
        if (file.empty()) set(f.ready, "unconfigured", "no file named");
        else if (ctx_.exists && !ctx_.exists(file)) set(f.ready, "failing", file + " is not there");
        f.lines.push_back(file.empty() ? "(no file)" : file);
        f.lines.push_back("imports when run: one batch, one undo");
    }
    if (!used && k != "store" && k != "tunnel-assets" && k != "local-domain" && k != "web-domain" &&
        k != "gauge" && k != "expiry" && k != "reroute") {
        bool has_out = false;
        for (const auto& p : kd.ports) has_out |= p.out;
        if (has_out) f.lines.push_back("nothing uses this yet");
    }
    return f;
}

} // namespace farm
