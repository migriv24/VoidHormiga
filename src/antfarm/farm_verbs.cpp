/* antfarm/farm_verbs.cpp — the `farm` verbs. See farm_verbs.hpp. */
#include "antfarm/farm_verbs.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace farm {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    char q = 0;
    bool any = false;
    for (char c : line) {
        if (q) {
            if (c == q) q = 0;
            else cur += c;
        } else if (c == '"' || c == '\'') {
            q = c;
            any = true;
        } else if (c == ' ' || c == '\t') {
            if (!cur.empty() || any) out.push_back(cur);
            cur.clear();
            any = false;
        } else cur += c;
    }
    if (!cur.empty() || any) out.push_back(cur);
    return out;
}

std::vector<std::string> in_farm(const std::vector<std::string>& cmds, const std::string& back) {
    std::vector<std::string> r{std::string("use ") + kMantle};
    r.insert(r.end(), cmds.begin(), cmds.end());
    if (!back.empty() && back != kMantle) r.push_back("use " + back);
    return r;
}

namespace {

const char* mark(const std::string& state) {
    if (state == "ready") return "[ready]";
    if (state == "needs") return "[needs]";
    if (state == "failing") return "[FAILING]";
    if (state == "planned") return "[planned]";
    if (state == "unconfigured") return "[unset]";
    return "[idle]";
}

bool split_port(const std::string& s, std::string& node, std::string& port) {
    const auto dot = s.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= s.size()) return false;
    node = s.substr(0, dot);
    port = s.substr(dot + 1);
    return true;
}

bool valid_name(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) return false;
    return true;
}

std::string link(const std::string& a, const std::string& out, const std::string& b, const std::string& in) {
    return "link " + a + " " + b + " --relation " + out + ":" + in;
}

std::string show(const Graph& g, Evaluator* ev, const Node& n) {
    std::ostringstream o;
    const Kind& k = *n.kind;
    o << n.name << "  (" << k.label << ", " << stratum_name(k.stratum) << ")";
    if (ev) {
        const Face f = ev->face(n.name);
        o << "  " << mark(f.ready.state);
        if (!f.ready.why.empty()) o << " " << f.ready.why;
        o << "\n";
        for (const auto& l : f.lines) o << "  | " << l << "\n";
    } else o << "\n";
    o << "  " << k.doc << "\n";
    for (bool outs : {false, true}) {
        bool head = false;
        for (const auto& p : k.ports) {
            if (p.out != outs) continue;
            if (!head) o << (outs ? "  outputs:\n" : "  inputs:\n"), head = true;
            char buf[160];
            std::snprintf(buf, sizeof buf, "    %-12s %-10s%s", p.name.c_str(),
                          p.type.empty() ? "any" : p.type.c_str(),
                          outs ? "" : p.many ? " many" : p.optional ? " optional" : "");
            o << buf;
            std::string peers;
            for (const Wire* w : outs ? g.out_of(n.name, p.name) : g.into(n.name, p.name))
                peers += (peers.empty() ? "" : ", ") + (outs ? w->to + "." + w->in : w->from + "." + w->out);
            o << (outs ? "  -> " : "  <- ") << (peers.empty() ? "(nothing)" : peers) << "\n";
        }
    }
    bool head = false;
    for (const auto& f : k.fields) {
        const std::string v = n.field(f.key);
        if (v.empty()) continue;
        if (!head) o << "  fields:\n", head = true;
        o << "    " << f.key << " = " << v << "\n";
    }
    return o.str();
}

std::string help() {
    return "farm                          the Connections view: what this database is connected to\n"
           "farm init                     create the v2 Antfarm (the `farm` mantle) with its default colony\n"
           "farm ls [--stratum S]         nodes\n"
           "farm show <node>              one node: readiness, face, ports and what they are wired to\n"
           "farm kinds [group]            the palette\n"
           "farm ports <kind>             a kind's sockets\n"
           "farm eval <node>.<port>       the value of an output, now\n"
           "farm status [node]            readiness (no network), and wires that went around the door\n"
           "farm mantles                  every chamber: runes and bytes\n"
           "farm add <kind> <name> [field=value ...]\n"
           "farm set <node> field=value ...\n"
           "farm rm <node>\n"
           "farm plug <a>.<out> <b>.<in> [--replace]\n"
           "farm unplug <a>.<out> <b>.<in>\n"
           "farm place <node> on <any|each|profile>\n"
           "farm arrange                  lay the graph out by stratum and flow\n"
           "farm chambers                 bring the Assets, Network and Documents mantles in step\n"
           "farm showcase                 the Cat Colony: a v2 Antfarm that uses every kind of node\n"
           "farm migrate [apply]          build the v2 Antfarm from this database's v1 one (rehearses unless apply)\n"
           "farm run <node> [apply]       run a source, a store or a tunnel (rehearses unless apply)\n"
           "farm check <node>             the smallest real test of a folder, a domain or a key\n"
           "farm preview <document>       build it, and serve it on its local domain\n"
           "farm publish <document>       build it and send it to its web domain\n"
           "farm key add <provider> <name>   a key rune (its value: farm key set <name>, from stdin)\n"
           "farm version\n";
}

} // namespace

std::vector<std::string> seed_commands(const SeedInfo& s) {
    std::vector<std::string> c = {
        "rune new farm_miga this-db", "set this-db version " + json_quote("2"),
        "rune new farm_folder here", "set here path " + json_quote("assets"),
        "set here placement " + json_quote("each"),
        "rune new farm_river home",
        link("here", "river", "home", "reservoirs"),
        link("home", "river", "this-db", "rests-in"),
        "rune new farm_separate chambers",
        link("this-db", "all", "chambers", "mantle"),
        "rune new farm_tunnel_assets pictures",
        link("chambers", "data", "pictures", "data"),
        link("chambers", "assets", "pictures", "assets"),
        // the grant (documents.md §5): the website may never see anything private
        "rune new farm_q_tag not-private", "set not-private expr " + json_quote("NOT private"),
        "rune new farm_filter public",
        link("chambers", "data", "public", "mantle"),
        link("not-private", "query", "public", "where"),
        "rune new farm_website site",
        link("public", "kept", "site", "data"),
        "rune new farm_calendar events", "set events mount " + json_quote("/events/"),
        link("public", "kept", "events", "data"),
        "rune new farm_map places", "set places mount " + json_quote("/map/"),
        link("public", "kept", "places", "data"),
        "rune new farm_local_domain preview-here", "set preview-here port " + json_quote("8780"),
        "set preview-here placement " + json_quote("each"),
        link("site", "preview", "preview-here", "renditions"),
        link("events", "preview", "preview-here", "renditions"),
        link("places", "preview", "preview-here", "renditions"),
        "rune new farm_profile me",
    };
    if (!s.website.empty()) c.push_back("set site document " + json_quote(s.website));
    if (!s.calendar.empty()) c.push_back("set events document " + json_quote(s.calendar));
    if (!s.map.empty()) c.push_back("set places document " + json_quote(s.map));
    return c;
}

std::vector<std::string> arrange_commands(const Graph& g) {
    std::map<std::string, int> depth;
    for (const auto& n : g.nodes) depth[n.name] = 0;
    for (std::size_t pass = 0; pass < g.nodes.size(); ++pass) { // longest path; a cycle stops at n passes
        bool moved = false;
        for (const auto& w : g.wires)
            if (depth.count(w.from) && depth.count(w.to) && depth[w.to] < depth[w.from] + 1) {
                depth[w.to] = depth[w.from] + 1;
                moved = true;
            }
        if (!moved) break;
    }
    auto band = [](const Node& n) {
        if (!n.kind) return 2;
        switch (n.kind->stratum) {
        case Stratum::Surface:
        case Stratum::Network: return 0;
        case Stratum::Ground: return 1;
        default: return 2;
        }
    };
    std::vector<std::string> cmds;
    float y0 = 40.0f;
    for (int b = 0; b < 3; ++b) {
        std::map<int, float> col_y;
        float band_h = 0;
        for (const auto& n : g.nodes) {
            if (band(n) != b) continue;
            const int col = depth[n.name];
            float& y = col_y[col];
            char pos[128];
            std::snprintf(pos, sizeof pos, "setjson %s pos [%d,%d]", n.name.c_str(), 60 + col * 280,
                          (int)(y0 + y));
            cmds.push_back(pos);
            int rows = 0, ins = 0, outs = 0; // inputs and outputs share rows, side by side
            if (n.kind)
                for (const auto& p : n.kind->ports) (p.out ? outs : ins) += 1;
            rows = std::max(ins, outs);
            const float h = (n.sn && n.sn->h > 0 ? n.sn->h : (n.kind ? (float)n.kind->face_h : 60.0f)) + 26.0f +
                            20.0f * (float)rows;
            y += h + 30.0f;
            band_h = std::max(band_h, y);
        }
        if (band_h > 0) y0 += band_h + 70.0f; // air for the surface line between bands
    }
    return cmds;
}

std::string connections_text(const Graph& g, Evaluator& ev) {
    std::ostringstream o;
    auto row = [&](const Node& n, const std::string& extra) {
        const Face f = ev.face(n.name);
        char buf[256];
        std::snprintf(buf, sizeof buf, "  %-10s %-16s %s", mark(f.ready.state), n.name.c_str(),
                      (f.ready.why.empty() ? extra : f.ready.why).c_str());
        o << buf << "\n";
    };
    auto group = [&](const char* title, auto pred, auto extra) {
        bool head = false;
        for (const auto& n : g.nodes) {
            if (!n.kind || !pred(n)) continue;
            if (!head) o << title << "\n", head = true;
            row(n, extra(n));
        }
    };
    auto is = [](std::initializer_list<const char*> ids) {
        return [ids](const Node& n) {
            for (const char* i : ids)
                if (n.kind->id == i && !n.kind->planned) return true;
            return false;
        };
    };
    auto first_line = [&](const Node& n) {
        const Face f = ev.face(n.name);
        return f.lines.empty() ? std::string() : f.lines.front();
    };
    group("WHERE THE DATABASE RESTS", is({"miga"}), [&](const Node& n) {
        const auto r = g.into(n.name, "rests-in");
        return r.empty() ? std::string("resting nowhere") : "rests in " + r[0]->from;
    });
    group("WHERE FILES ARE KEPT", is({"river"}), first_line);
    group("DOCUMENTS AND WHERE THEY GO", is({"website", "newsletter", "calendar", "map"}), [&](const Node& n) {
        const Face f = ev.face(n.name);
        std::string s;
        for (const auto& l : f.lines)
            if (l.rfind("preview", 0) == 0 || l.rfind("publish", 0) == 0) s += (s.empty() ? "" : " · ") + l;
        return s;
    });
    group("DOMAINS", is({"local-domain", "web-domain"}), first_line);
    group("KEYS", is({"key"}), first_line);
    group("THE NETWORK", is({"profile"}), [&](const Node& n) {
        const Face f = ev.face(n.name);
        return f.lines.size() > 1 ? f.lines[0] + " · " + f.lines[1] : first_line(n);
    });
    group("PLANNED (placed, no code yet)", [](const Node& n) { return n.kind->planned; },
          [](const Node& n) { return n.kind->doc; });
    const auto bad = audit(g);
    if (!bad.empty()) {
        o << "WIRES THAT WENT AROUND THE DOOR\n";
        for (const auto& b : bad) o << "  " << b << "\n";
    }
    return o.str();
}

VerbResult run(const std::vector<std::string>& a, const Graph& g, bool exists, Evaluator* ev,
               const SeedInfo& seed) {
    VerbResult r;
    auto fail = [&](std::string msg) {
        r.ok = false;
        r.text = std::move(msg);
        return r;
    };
    const std::string verb = a.empty() ? "" : a[0];
    if (verb == "help") return r.text = help(), r;
    if (verb == "version") {
        r.text = exists ? "Antfarm v2 (the `farm` mantle)\n" : "no v2 Antfarm in this database: `farm init` creates one\n";
        return r;
    }
    if (verb == "kinds") {
        std::ostringstream o;
        std::string last;
        for (const auto& k : kinds()) {
            if (a.size() > 1 && k.group != a[1]) continue;
            if (k.group != last) o << k.group << "\n", last = k.group;
            char buf[200];
            std::snprintf(buf, sizeof buf, "  %-15s %-9s %s%s", k.id.c_str(), stratum_name(k.stratum),
                          k.planned ? "(planned) " : "", k.doc.c_str());
            o << buf << "\n";
        }
        return r.text = o.str(), r;
    }
    if (verb == "ports") {
        if (a.size() < 2) return fail("usage: farm ports <kind>");
        const Kind* k = kind_by_id(a[1]);
        if (!k) return fail("no kind called " + a[1] + " - `farm kinds` lists them");
        std::ostringstream o;
        for (const auto& p : k->ports)
            o << "  " << (p.out ? "out " : "in  ") << p.name << "  " << (p.type.empty() ? "any" : p.type)
              << (p.out ? "" : p.many ? "  (many)" : "  (one)") << (p.optional ? "  optional" : "")
              << (p.writes ? "  writes when run" : "") << "\n";
        return r.text = o.str(), r;
    }
    if (verb == "chambers" || verb == "showcase" || verb == "migrate" || verb == "run" || verb == "check" || verb == "preview" || verb == "publish") {
        r.needs_host = true; // the chambers and every effect are the host's (farm_verbs.hpp)
        return r;
    }
    if (verb == "init") {
        if (exists) return fail("this database already has a v2 Antfarm (`farm` to see it)");
        r.commands = {std::string("mantle new ") + kMantle};
        for (auto& c : seed_commands(seed)) r.commands.push_back(c);
        r.text = "created the v2 Antfarm: `farm` shows it, `farm arrange` lays it out\n";
        return r;
    }
    if (!exists) return fail("no v2 Antfarm in this database yet - `farm init` creates one");

    if (verb.empty()) {
        if (!ev) return fail("nothing to evaluate with");
        return r.text = connections_text(g, *ev), r;
    }
    if (verb == "ls") {
        std::ostringstream o;
        const std::string want = a.size() > 2 && a[1] == "--stratum" ? a[2] : "";
        for (const auto& n : g.nodes) {
            if (!n.kind) continue;
            if (!want.empty() && want != stratum_name(n.kind->stratum)) continue;
            const std::string st = ev ? ev->face(n.name).ready.state : "";
            char buf[200];
            std::snprintf(buf, sizeof buf, "  %-16s %-14s %-9s %s", n.name.c_str(), n.kind->id.c_str(),
                          stratum_name(n.kind->stratum), st.empty() ? "" : mark(st));
            o << buf << "\n";
        }
        return r.text = o.str(), r;
    }
    if (verb == "show" || (verb == "key" && a.size() == 2)) {
        if (a.size() < 2) return fail("usage: farm show <node>");
        const Node* n = g.find(a[1]);
        if (!n || !n->kind) return fail("no v2 node called " + a[1]);
        return r.text = show(g, ev, *n), r;
    }
    if (verb == "eval") {
        std::string node, port;
        if (a.size() < 2 || !split_port(a[1], node, port)) return fail("usage: farm eval <node>.<port>");
        const Node* n = g.find(node);
        if (!n || !n->kind || !n->kind->port(port) || !n->kind->port(port)->out)
            return fail("no output called " + a[1]);
        if (!ev) return fail("nothing to evaluate with");
        const Value v = ev->eval(node, port);
        std::ostringstream o;
        o << a[1] << "  (" << v.type << ")\n";
        if (v.type == "mantle") {
            std::map<std::string, int> by;
            long long bytes = 0;
            for (const auto& ru : v.runes) ++by[ru.chamber + "/" + ru.node.glyph], bytes += ru.bytes;
            o << "  " << v.runes.size() << " runes, " << human_bytes(bytes) << ", "
              << strands_for((long long)v.runes.size()) << " strand(s)\n";
            for (const auto& [k, c] : by) o << "    " << k << "  " << c << "\n";
            if (v.query.op != Query::Op::All) o << "  selected by: " << v.query.text() << "\n";
        } else if (v.type == "query") {
            o << "  = " << v.query.text() << "\n";
        } else {
            if (!v.text.empty()) o << "  " << v.text << "\n";
            for (const auto& ref : v.refs) o << "  -> " << ref << "\n";
        }
        return r.text = o.str(), r;
    }
    if (verb == "status") {
        std::ostringstream o;
        for (const auto& n : g.nodes) {
            if (!n.kind || (a.size() > 1 && n.name != a[1])) continue;
            const Face f = ev ? ev->face(n.name) : Face{};
            char buf[256];
            std::snprintf(buf, sizeof buf, "  %-10s %-16s %s", mark(f.ready.state), n.name.c_str(), f.ready.why.c_str());
            o << buf << "\n";
        }
        for (const auto& b : audit(g)) o << "  [FAILING]  " << b << "\n";
        return r.text = o.str(), r;
    }
    if (verb == "mantles") {
        if (!ev) return fail("nothing to evaluate with");
        std::ostringstream o;
        for (const char* c : {"data", "assets", "network", "documents"}) {
            const auto rs = ev->chamber(c);
            long long b = 0;
            for (const auto& x : rs) b += x.bytes;
            char buf[128];
            std::snprintf(buf, sizeof buf, "  %-10s %8zu runes  %s", c, rs.size(), human_bytes(b).c_str());
            o << buf << "\n";
        }
        return r.text = o.str(), r;
    }
    if (verb == "arrange") {
        r.commands = arrange_commands(g);
        r.text = "arranged " + std::to_string(r.commands.size()) + " nodes\n";
        return r;
    }

    // ── changing the graph ────────────────────────────────────────────────
    auto kv = [&](size_t from, const Node* n, const Kind& k) -> bool {
        for (size_t i = from; i < a.size(); ++i) {
            const auto eq = a[i].find('=');
            if (eq == std::string::npos) return fail("expected field=value, got " + a[i]), false;
            const std::string key = a[i].substr(0, eq);
            bool known = key == "placement";
            for (const auto& f : k.fields) known |= f.key == key;
            if (!known) return fail(k.label + " has no field called " + key), false;
            r.commands.push_back("set " + (n ? n->name : a[2]) + " " + key + " " + json_quote(a[i].substr(eq + 1)));
        }
        return true;
    };
    if (verb == "add") {
        if (a.size() < 3) return fail("usage: farm add <kind> <name> [field=value ...]");
        const Kind* k = kind_by_id(a[1]);
        if (!k) return fail("no kind called " + a[1] + " - `farm kinds` lists them");
        if (!valid_name(a[2])) return fail("a name is letters, digits, - and _");
        if (g.find(a[2])) return fail("there is already a node called " + a[2]);
        r.commands.push_back("rune new " + k->glyph + " " + a[2]);
        if (!kv(3, nullptr, *k)) return r;
        r.text = "added " + a[2] + " (" + k->label + ")" + (k->planned ? " - planned: no code answers it yet" : "") + "\n";
        return r;
    }
    if (verb == "set") {
        if (a.size() < 3) return fail("usage: farm set <node> field=value ...");
        const Node* n = g.find(a[1]);
        if (!n || !n->kind) return fail("no v2 node called " + a[1]);
        if (!kv(2, n, *n->kind)) return r;
        return r;
    }
    if (verb == "rm") {
        if (a.size() < 2) return fail("usage: farm rm <node>");
        const Node* n = g.find(a[1]);
        if (!n) return fail("no node called " + a[1]);
        for (const auto& w : g.wires)
            if (w.from == n->name || w.to == n->name)
                r.commands.push_back("unlink " + w.from + " " + w.to + " --relation " + w.relation);
        r.commands.push_back("rm " + n->name);
        r.text = "removed " + n->name + " and its " + std::to_string(r.commands.size() - 1) + " wire(s)\n";
        return r;
    }
    if (verb == "plug" || verb == "unplug") {
        std::string an, ap, bn, bp;
        if (a.size() < 3 || !split_port(a[1], an, ap) || !split_port(a[2], bn, bp))
            return fail("usage: farm " + verb + " <a>.<out> <b>.<in>");
        if (verb == "unplug") {
            for (const auto& w : g.wires)
                if (w.from == an && w.out == ap && w.to == bn && w.in == bp) {
                    r.commands.push_back("unlink " + an + " " + bn + " --relation " + w.relation);
                    return r;
                }
            return fail("there is no wire " + a[1] + " -> " + a[2]);
        }
        const bool replace = a.size() > 3 && a[3] == "--replace";
        const Wire* old = nullptr;
        const std::string why = check_plug(g, an, ap, bn, bp, replace ? &old : nullptr);
        if (!why.empty()) return fail(why + (replace || why.find("accepts one") == std::string::npos
                                                 ? ""
                                                 : " Or add --replace."));
        if (old) r.commands.push_back("unlink " + old->from + " " + old->to + " --relation " + old->relation);
        r.commands.push_back(link(an, ap, bn, bp));
        return r;
    }
    if (verb == "place") {
        if (a.size() < 4 || a[2] != "on") return fail("usage: farm place <node> on <any|each|profile>");
        const Node* n = g.find(a[1]);
        if (!n || !n->kind) return fail("no v2 node called " + a[1]);
        r.commands.push_back("set " + n->name + " placement " + json_quote(a[3]));
        return r;
    }
    if (verb == "key") {
        if (a.size() >= 4 && a[1] == "add") {
            const Kind* k = kind_by_id("key");
            if (!valid_name(a[3])) return fail("a name is letters, digits, - and _");
            if (g.find(a[3])) return fail("there is already a node called " + a[3]);
            r.commands = {"rune new " + k->glyph + " " + a[3], "set " + a[3] + " provider " + json_quote(a[2]),
                          "set " + a[3] + " vault_entry " + json_quote("farm-key:" + a[3])};
            if (!seed.actor.empty()) r.commands.push_back("set " + a[3] + " added_by " + json_quote(seed.actor));
            r.text = "added the key " + a[3] + ". Its value is set with `farm key set " + a[3] +
                     "` (read from standard input, never an argument).\n";
            return r;
        }
        if (a.size() >= 3 && (a[1] == "set" || a[1] == "reveal")) {
            r.needs_host = true;
            return r;
        }
        return fail("usage: farm key add <provider> <name> | farm key set <name> | farm key <name>");
    }
    return fail("unknown farm verb '" + verb + "'\n" + help());
}

} // namespace farm
