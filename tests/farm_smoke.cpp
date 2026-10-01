/* farm_smoke.cpp — Antfarm v2's core, alone (okf/concepts/platform/antfarm/v2/).
 *
 * Links src/antfarm/ and Void Maiz and nothing of Hormiga's, which is the test
 * that the layer is separable (Q92) as well as correct. The host is a Context of
 * lambdas over a tiny hand-made data mantle. */
#include "antfarm/farm_verbs.hpp"

#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <cstdio>
#include <memory>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

namespace {

maiz::Scene farm_scene(maiz::Core& core) {
    maiz::ProjectOptions po;
    po.mantle = farm::kMantle;
    maiz::Scene s = maiz::project_scene(core, po);
    farm::resolve_named_wires(s);
    return s;
}

bool apply(maiz::Core& core, const farm::VerbResult& r, bool init = false) {
    if (!r.ok) return false;
    const auto cmds = init ? r.commands : farm::in_farm(r.commands, "org");
    for (const auto& c : cmds)
        if (!core.dispatch(c).ok) {
            std::printf("  command refused by the core: %s\n", c.c_str());
            return false;
        }
    return true;
}

farm::VerbResult verb(maiz::Core& core, const std::string& line, farm::Evaluator* ev = nullptr) {
    const maiz::Scene s = farm_scene(core);
    const farm::Graph g = farm::read(s);
    bool exists = false;
    for (const auto& l : core.dispatch("mantles").lines) exists |= l.find(farm::kMantle) != std::string::npos;
    return farm::run(farm::tokenize(line), g, exists, ev);
}

} // namespace

int main() {
    maiz::Core core;
    core.register_glyph(R"({"glyph":"contact","label":"Contact","fields":["name","avatar"]})");
    farm::register_glyphs(core);
    core.dispatch("mantle new org");
    core.dispatch("rune new contact ana");
    core.dispatch("set ana avatar \"ana.jpg\"");
    core.dispatch("tag ana +volunteer");
    core.dispatch("rune new contact ben");
    core.dispatch("tag ben +private");
    core.dispatch("rune new contact cy");
    core.dispatch("tag cy +volunteer");

    // ── init: the default colony, checked by nothing but the core ──────────
    CHECK(!verb(core, "").ok); // no farm yet: says so
    CHECK(apply(core, verb(core, "init"), true));
    core.dispatch("use org");
    maiz::Scene s = farm_scene(core);
    farm::Graph g = farm::read(s);
    CHECK(g.find("this-db") && g.find("public") && g.find("site") && g.find("preview-here"));
    CHECK(farm::audit(g).empty()); // every seeded wire is named and well-typed
    int linguine = 0;
    for (const auto& w : s.wires) linguine += w.kind == maiz::SceneWire::Kind::Linguine;
    CHECK(linguine == (int)g.wires.size()); // named wires draw between sockets

    // ── the evaluator, through a host Context ──────────────────────────────
    auto data = std::make_shared<maiz::Scene>([&] {
        maiz::ProjectOptions po;
        po.mantle = "org";
        return maiz::project_scene(core, po);
    }());
    farm::Context ctx;
    ctx.chamber = [data](const std::string& c) {
        std::vector<farm::Rune> out;
        if (c == "data")
            for (const auto& n : data->nodes) out.push_back({"data", n, data, 100});
        return out;
    };
    ctx.exists = [](const std::string& p) { return p == "ana.jpg"; };
    ctx.serves_local = true;
    {
        farm::Evaluator ev(g, ctx);
        CHECK(ev.eval("this-db", "all").runes.size() == 3);
        CHECK(ev.eval("public", "kept").runes.size() == 2); // NOT private
        CHECK(ev.eval("public", "rest").runes.size() == 1);
        CHECK(ev.face("public").lines.at(0) == "kept 2 of 3");
        CHECK(ev.face("pictures").ready.state == "ready"); // ana.jpg exists
        CHECK(ev.face("preview-here").ready.state == "ready");
        CHECK(ev.face("site").ready.state == "unconfigured"); // no document named in this seed
    }
    {
        farm::Context phone = ctx;
        phone.serves_local = false;
        phone.device = "phone";
        farm::Evaluator ev(g, phone);
        CHECK(ev.face("preview-here").ready.state == "needs");
        CHECK(ev.face("preview-here").ready.why.find("nowhere to go") != std::string::npos);
    }

    // ── the door ────────────────────────────────────────────────────────────
    const farm::VerbResult bad = verb(core, "plug public.kept preview-here.renditions");
    CHECK(!bad.ok && bad.text.find("takes a rendition") != std::string::npos);
    CHECK(!verb(core, "plug public.nope site.data").ok);
    CHECK(apply(core, verb(core, "add web-domain pages host=github target=org/org.github.io")));
    CHECK(apply(core, verb(core, "key add github gh-main")));
    CHECK(apply(core, verb(core, "key add cloudflare cf-main")));
    CHECK(apply(core, verb(core, "plug gh-main.key pages.key")));
    const farm::VerbResult full = verb(core, "plug cf-main.key pages.key");
    CHECK(!full.ok && full.text.find("accepts one") != std::string::npos);
    CHECK(apply(core, verb(core, "plug cf-main.key pages.key --replace")));
    s = farm_scene(core);
    g = farm::read(s);
    CHECK(g.into("pages", "key").size() == 1 && g.into("pages", "key")[0]->from == "cf-main");
    CHECK(!verb(core, "add filter bad name").ok);   // names are words
    CHECK(!verb(core, "set pages colour=red").ok);  // fields are declared

    // ── a raw link goes around the door, and the audit finds it ────────────
    core.dispatch("use farm");
    CHECK(core.dispatch("link public pages --relation kept:key").ok);
    core.dispatch("use org");
    s = farm_scene(core);
    g = farm::read(s);
    const auto problems = farm::audit(g);
    // two findings: the type, and a second wire into an input that takes one
    CHECK(problems.size() == 2);
    bool typed = false, over = false;
    for (const auto& p : problems) {
        typed |= p.find("a mantle wired into a key") != std::string::npos;
        over |= p.find("takes one wire and has 2") != std::string::npos;
    }
    CHECK(typed && over);

    // ── key state comes from the host, never a field ──────────────────────
    ctx.key_state = [](const std::string& e) { return e == "farm-key:cf-main" ? "present" : "missing"; };
    {
        farm::Evaluator ev(g, ctx);
        CHECK(ev.face("cf-main").ready.state == "ready");
        CHECK(ev.face("gh-main").ready.state == "needs");
    }

    // ── rm takes its wires with it; arrange is positions only ──────────────
    CHECK(apply(core, verb(core, "rm pages")));
    s = farm_scene(core);
    g = farm::read(s);
    CHECK(!g.find("pages") && g.into("pages", "key").empty());
    const auto arr = farm::arrange_commands(g);
    CHECK(arr.size() == g.nodes.size());

    std::printf(failures ? "farm_smoke: %d FAILED\n" : "farm_smoke: ok\n", failures);
    return failures ? 1 : 0;
}
