/* chambers_smoke.cpp — the Assets, Network and Documents mantles (Antfarm v2).
 *
 * domain/chambers.hpp is Scene-in / commands-out; this runs its commands through
 * a real Void Core and checks the three properties the chambers rest on:
 * reconciling twice writes nothing the second time (a boot must not be an undo
 * frame and a sync every time), the names are deterministic (two devices mint the
 * same rune for the same file), and a document whose content is gone is marked,
 * never deleted. */
#include "domain/chambers.hpp"

#include "voidmaiz/project.hpp"

#include <cstdio>

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

namespace ch = hormiga::chambers;

static maiz::Scene project(maiz::Core& core, const char* mantle) {
    maiz::ProjectOptions po;
    po.mantle = mantle;
    return maiz::project_scene(core, po);
}

static bool run(maiz::Core& core, const char* mantle, const std::vector<std::string>& cmds) {
    core.dispatch(std::string("use ") + mantle);
    for (const auto& c : cmds)
        if (!core.dispatch(c).ok) {
            std::printf("  refused: %s\n", c.c_str());
            return false;
        }
    return true;
}

int main() {
    maiz::Core core;
    ch::register_glyphs(core);
    for (const char* m : {ch::kAssets, ch::kNetwork, ch::kDocuments}) core.dispatch(std::string("mantle new ") + m);

    // ── assets: content-addressed names, idempotent ────────────────────────
    const std::string sha(64, 'a');
    const std::vector<ch::FileFact> files = {{"assets/flier-" + sha + ".jpg", 1234, "ana"},
                                             {"assets/logo.png", 88, ""},
                                             {"assets/logo.png", 88, ""}}; // the same file twice
    CHECK(ch::sha_in("assets/flier-" + sha + ".jpg") == sha);
    CHECK(ch::asset_name("assets/flier-" + sha + ".jpg") == "a-" + std::string(16, 'a'));
    CHECK(ch::asset_name("elsewhere/other-" + sha + ".png") == ch::asset_name("assets/flier-" + sha + ".jpg"));
    auto cmds = ch::register_assets(project(core, ch::kAssets), files);
    CHECK(run(core, ch::kAssets, cmds));
    maiz::Scene assets = project(core, ch::kAssets);
    CHECK(assets.nodes.size() == 2);
    const maiz::SceneNode* flier = assets.find("a-" + std::string(16, 'a'));
    CHECK(flier && hormiga::field_value(*flier, "sha256") == sha && hormiga::field_value(*flier, "media") == "image/jpeg");
    CHECK(ch::register_assets(assets, files).empty()); // the second pass writes nothing

    // ── network: this device, about itself, only when something changed ───
    ch::Self me{"0a1b2c3d4e5f", "ana", "#ff8800", "desktop", "windows-x64", true, "2026-09-28"};
    CHECK(run(core, ch::kNetwork, ch::upsert_self(project(core, ch::kNetwork), me)));
    maiz::Scene net = project(core, ch::kNetwork);
    CHECK(net.find("p-0a1b2c3d4e5f") && hormiga::field_value(*net.find("p-0a1b2c3d4e5f"), "username") == "ana");
    CHECK(ch::upsert_self(net, me).empty());
    me.color = "#0088ff";
    auto recolour = ch::upsert_self(net, me);
    CHECK(recolour.size() == 1 && recolour[0].find("color") != std::string::npos);

    // ── documents: one entry per presentation; gone is marked, not deleted ─
    std::vector<ch::DocFact> live = {{"issue-demo", "newsletter", "Cat News"},
                                     {"cat-birthdays", "calendar", "Cat birthdays"},
                                     {"usa", "map", ""}};
    CHECK(run(core, ch::kDocuments, ch::reconcile_documents(project(core, ch::kDocuments), live)));
    maiz::Scene docs = project(core, ch::kDocuments);
    CHECK(docs.nodes.size() == 3);
    CHECK(docs.find("doc-cat-birthdays") &&
          hormiga::field_value(*docs.find("doc-cat-birthdays"), "mount") == "/cat-birthdays/");
    CHECK(ch::reconcile_documents(docs, live).empty());
    live.pop_back(); // the map view was deleted
    CHECK(run(core, ch::kDocuments, ch::reconcile_documents(docs, live)));
    docs = project(core, ch::kDocuments);
    CHECK(docs.nodes.size() == 3 && hormiga::field_value(*docs.find("doc-usa"), "gone") == "yes");

    std::printf(failures ? "chambers_smoke: %d FAILED\n" : "chambers_smoke: ok\n", failures);
    return failures ? 1 : 0;
}
