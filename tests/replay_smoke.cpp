/* replay_smoke.cpp — the phase B exit test, headless.
 * The demo org is a transcript (src/seed.hpp); a session's edits extend it.
 * Replaying the whole transcript into a fresh core must project an identical
 * scene, and replay-by-exported-state must hold too. "The transcript is the
 * session" — the founding commitment, automated (the same shape as Void
 * Maiz's own replay smoke, applied to Hormiga's glyphs and edges). */
#include "../src/domain/seed.hpp"

#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ++failures;                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                          \
    } while (0)

/* An id-independent fingerprint of what the user sees. */
static std::string fingerprint(const maiz::Scene& scene) {
    std::ostringstream out;
    out << "mantle:" << scene.mantle << "\n";
    for (const auto& n : scene.nodes) {
        out << "node:" << n.name << "|" << n.glyph << "|" << n.x << "," << n.y
            << "|" << n.w << "x" << n.h << "|";
        for (const auto& t : n.tags) out << "@" << t;
        out << "|";
        for (const auto& f : n.fields) out << f.key << "=" << f.value_json << ";";
        out << "\n";
    }
    for (const auto& w : scene.wires)
        out << "wire:" << w.from << ":" << w.from_port << "->" << w.to << ":"
            << w.to_port << "|" << (int)w.kind << "|" << w.directed << "\n";
    return out.str();
}

int main() {
    // ── the transcript: the seed plus a session's worth of edits ────────────
    std::vector<std::string> transcript = hormiga::seed_transcript();
    for (auto& cmd : hormiga::seed_issue_transcript())
        transcript.push_back(cmd); // the block stack replays too (adjacency wires)
    auto say = [&](std::string cmd) { transcript.push_back(std::move(cmd)); };

    say("config set actor human:replay-test");
    // inspector-style field edits
    say(R"(set ray email "ray@casa-comunal.example")");
    say("set junta-junio date 2026-06-19"); // the date face's commit shape
    // tag hygiene: retag an event's month, add a status
    say("tag taller-julio +status:draft");
    // a new rune wired in, then removed, then the removal taken back
    say("rune new resource guia-recursos");
    say(R"(set guia-recursos topic "housing")");
    say("setjson guia-recursos pos [790,230]");
    say("tag guia-recursos +type:resource");
    say("link guia-recursos taller-julio --relation handout-of");
    say("rm guia-recursos");
    say("undo"); // undo/redo are part of the transcript
    say("redo");
    say("undo");

    // ── session A: live editing ──────────────────────────────────────────────
    maiz::Core a;
    hormiga::register_glyphs(a);
    hormiga::register_block_glyphs(a);
    for (const auto& cmd : transcript) a.dispatch(cmd);
    std::string fp_a = fingerprint(maiz::project_scene(a));

    // ── session B: a fresh core replays the transcript ──────────────────────
    maiz::Core b;
    hormiga::register_glyphs(b);
    hormiga::register_block_glyphs(b);
    for (const auto& cmd : transcript) b.dispatch(cmd);
    std::string fp_b = fingerprint(maiz::project_scene(b));

    CHECK(!fp_a.empty() && fp_a.find("node:") != std::string::npos);
    CHECK(fp_a.find("wire:") != std::string::npos); // edges survived
    CHECK(fp_a.find("guia-recursos") != std::string::npos); // the undo held
    CHECK(fp_a == fp_b);
    if (fp_a != fp_b)
        std::cerr << "--- A ---\n" << fp_a << "--- B ---\n" << fp_b;

    // ── and once more from the exported state (the save/reload path) ────────
    maiz::Core c(a.export_state());
    hormiga::register_glyphs(c);
    hormiga::register_block_glyphs(c);
    CHECK(fingerprint(maiz::project_scene(c)) == fp_a);

    if (failures == 0) {
        std::cout << "OK — the demo org replays exactly (" << transcript.size()
                  << " commands)\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
