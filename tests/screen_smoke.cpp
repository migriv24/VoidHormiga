/* screen_smoke.cpp — the application as data, without a window
 * (domain/actions.hpp, domain/shell.hpp; okf/concepts/platform/shell.md).
 *
 * THE SAFETY OF AN ACTION IS CHECKED AGAINST THE ENGINE THAT RUNS IT, not
 * against our reading of its text (submission.hpp's rule): what a person types
 * or scans goes through a real Void Core and must come back exactly as typed,
 * as one value, whatever it holds. */
#include "domain/actions.hpp"
#include "domain/shell.hpp"

#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

namespace ax = hormiga::actions;
namespace sh = hormiga::shell;

static std::string field(maiz::Core& core, const std::string& rune, const char* key) {
    const maiz::Scene s = maiz::project_scene(core);
    const maiz::SceneNode* n = s.find(rune);
    return n ? hormiga::field_value(*n, key) : std::string("(no rune)");
}

int main() {
    maiz::Core core;
    core.register_glyph(R"({"glyph":"contact","label":"Contact","fields":["display_name","email","note"]})");
    core.dispatch("mantle new org");
    core.dispatch("rune new contact ana");
    core.dispatch("set ana display_name \"Ana\"");

    // ── an action: placeholders, one run, one fresh name ───────────────────
    int minted = 0;
    ax::Context cx;
    cx.has_input = true;
    cx.today = "2026-11-15";
    cx.mint = [&] { return "item-" + std::to_string(++minted); };
    {
        cx.input = "Ben";
        const auto c = ax::compile("# make one\nrune new contact {{new}}\n\n  set {{new}} display_name {{input}}\n"
                                   "set {{new}} note {{today}}",
                                   cx);
        CHECK(c.ok() && c.lines.size() == 3); // the comment and the blank line are skipped
        CHECK(minted == 1);                    // {{new}} is one name for the whole run
        for (const auto& l : c.lines) CHECK(core.dispatch(l).ok);
        CHECK(field(core, "item-1", "display_name") == "Ben");
        CHECK(field(core, "item-1", "note") == "2026-11-15");
    }
    // ── what is typed or scanned stays ONE VALUE, exactly as typed ─────────
    {
        const std::string nasty = "O'Brien \"the\" \\ ;\nrm ana\nset ana email 'x'";
        cx.input = nasty;
        const auto c = ax::compile("set ana note {{input}}", cx);
        CHECK(c.ok() && c.lines.size() == 1); // the newline in the input did not make a second line
        CHECK(core.dispatch(c.lines[0]).ok);
        CHECK(field(core, "ana", "note") == nasty); // stored exactly, through the real tokenizer
        CHECK(field(core, "ana", "display_name") == "Ana"); // and nothing else ran: ana is still there
        cx.input = "ends in a backslash \\";
        const auto t = ax::compile("set ana note {{input}}", cx);
        CHECK(t.ok() && core.dispatch(t.lines[0]).ok && field(core, "ana", "note") == cx.input);
    }
    // ── the selection: a field as a value, a name only when command-safe ───
    {
        const maiz::Scene s = maiz::project_scene(core);
        ax::Context sel = cx;
        sel.has_input = false;
        sel.selected = s.find("ana");
        const auto c = ax::compile("set {{selected}} email {{selected.display_name}}", sel);
        CHECK(c.ok() && c.lines[0] == "set ana email " + maiz::arg("Ana"));
        CHECK(!ax::compile("set ana note {{input}}", sel).ok()); // a button has no input
        ax::Context none = sel;
        none.selected = nullptr;
        const auto n = ax::compile("tag {{selected}} +done", none);
        CHECK(!n.ok() && n.error.find("select something") != std::string::npos);
        maiz::SceneNode odd;
        odd.name = "two words";
        sel.selected = &odd;
        CHECK(!ax::compile("tag {{selected}} +done", sel).ok()); // a name that cannot stand bare is refused
    }
    CHECK(!ax::compile("tag ana {{bogus}}", cx).ok());     // an unknown placeholder says so
    CHECK(!ax::compile("tag ana {{input", cx).ok());       // an unclosed one too
    CHECK(!ax::compile("# only a comment\n", cx).ok());    // nothing to run
    CHECK(ax::is_verb_line("map generate which=path") && ax::is_verb_line("shell hide notes") &&
          !ax::is_verb_line("mapshape-1 is not a verb") && !ax::is_verb_line("rune new contact x"));

    // ── the shell: built-ins never lost, screens kept, verbs through config ─
    {
        const auto stored = sh::parse(R"([{"id":"screen:counter","title":"Counter"},{"id":"notes","hidden":true},)"
                                      R"({"id":"screen:gone"},{"id":"nonsense"},{"id":"data"},{"id":"data"}])");
        const auto eff = sh::effective(stored, {"counter", "later"});
        CHECK(eff.size() == sh::builtins().size() + 2);
        CHECK(eff[0].id == "screen:counter" && eff[0].title == "Counter"); // its order first
        CHECK(eff[1].id == "notes" && eff[1].hidden && eff[2].id == "data");
        CHECK(eff.back().id == "screen:later"); // a screen it does not list yet still appears
        for (const auto& b : sh::builtins()) // a database cannot lose a built-in tab
            CHECK(std::any_of(eff.begin(), eff.end(), [&](const sh::Tab& t) { return t.id == b.id; }));
        const auto mv = sh::verb({"shell", "move", "data", "1"}, eff);
        CHECK(mv.ok && mv.tabs.front().id == "data" && mv.commands.size() == 1);
        CHECK(core.dispatch(mv.commands[0]).ok);
        const auto back = sh::effective(sh::parse(core.dispatch("config get shell.tabs").data), {"counter", "later"});
        CHECK(back.size() == eff.size() && back[0].id == "data" && back[1].id == "screen:counter"); // round trip
        CHECK(!sh::verb({"shell", "title", "data", "Mine"}, eff).ok);    // a built-in keeps its name
        CHECK(sh::verb({"shell", "title", "screen:counter", "Till", "one"}, eff).tabs[0].title == "Till one");
        CHECK(!sh::verb({"shell", "hide", "nowhere"}, eff).ok);
        CHECK(sh::verb({"shell", "hide", "notes"}, eff).tabs[1].hidden);
        // a line held: Settings always opens, by the verb and against a stored list that says otherwise
        CHECK(!sh::verb({"shell", "hide", "settings"}, eff).ok);
        const auto forced = sh::effective(sh::parse(R"([{"id":"settings","hidden":true}])"), {});
        bool settings_open = false;
        for (const auto& t : forced) settings_open = settings_open || (t.id == "settings" && !t.hidden);
        CHECK(settings_open);
    }

    if (failures) std::printf("%d failure(s)\n", failures);
    else std::printf("screen_smoke: ok\n");
    return failures ? 1 : 0;
}
