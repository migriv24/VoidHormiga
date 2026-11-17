/* domain/shell.hpp — the application as data: which tabs a database has, in
 * what order, and which start closed (okf/concepts/platform/shell.md).
 *
 * The author (2026-11-15): *"the layout of the tabs, the tabs it has, etc, should
 * be able to be modifiable ... those modifications should be able to store
 * themselves in a miga file. It's not just the database that's being loaded
 * anymore, but the configuration of the application itself."*
 *
 * STORED as config keys, so they travel in the .miga, are dispatched and logged,
 * and are undo-exempt like every other piece of view state:
 *   shell.tabs    [{"id":"data"}, {"id":"screen:counter","title":"Counter"},
 *                  {"id":"notes","hidden":true}, ...]   (order is the list's)
 *   shell.layout  the database's default arrangement (ImGui's docking text)
 *   shell.id      minted the first time a layout is saved; a device applies a
 *                 database's default once per id, so a person's own arrangement
 *                 is never overwritten by someone else's later save
 *
 * A BUILT-IN TAB IS NEVER LOST. A database may reorder one or start it closed,
 * never remove it: `effective` appends every built-in a stored list leaves out,
 * because a database must not be able to lock a person out of their own data.
 *
 * Pure: strings in, strings out. The `shell` verbs compile to `config set`. */
#pragma once

#include "json.hpp"
#include "voidmaiz/embed.hpp" // maiz::arg

#include <algorithm>
#include <string>
#include <vector>

namespace hormiga::shell {

struct Tab {
    std::string id;    // a built-in section, or "screen:<document mantle>"
    std::string title; // a screen's (a built-in keeps its own name)
    bool hidden = false; // starts closed when the database opens
};

/* The built-in tabs, in the application's own order, with the names their
 * windows have (window names are layout keys in imgui.ini: never change one). */
struct Builtin {
    const char* id;
    const char* window;
};
inline const std::vector<Builtin>& builtins() {
    static const std::vector<Builtin> b = {
        {"data", "Data"},         {"builder", "Builder"},   {"antfarm", "Antfarm"},
        {"map", "Map"},           {"calendar", "Calendar"}, {"notes", "Notes"},
        {"publish", "Publish"},   {"allomone", "Allomone"}, {"settings", "Settings"},
        {"style", "Style"},       {"console", "Console"},
    };
    return b;
}
inline const Builtin* builtin(const std::string& id) {
    for (const auto& b : builtins())
        if (id == b.id) return &b;
    return nullptr;
}
inline bool is_screen(const std::string& id) { return id.rfind("screen:", 0) == 0; }
inline std::string screen_mantle(const std::string& id) { return is_screen(id) ? id.substr(7) : std::string(); }

/* Read a stored list; anything unreadable is skipped, never fatal. */
inline std::vector<Tab> parse(const std::string& text) {
    std::vector<Tab> out;
    nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
    if (j.is_string()) j = nlohmann::json::parse(j.get<std::string>(), nullptr, false); // a config value read back quoted
    if (!j.is_array()) return out;
    for (const auto& e : j) {
        if (!e.is_object() || !e.contains("id") || !e["id"].is_string()) continue;
        Tab t;
        t.id = e["id"].get<std::string>();
        if (e.contains("title") && e["title"].is_string()) t.title = e["title"].get<std::string>();
        t.hidden = e.value("hidden", false);
        if (!t.id.empty() && std::none_of(out.begin(), out.end(), [&](const Tab& x) { return x.id == t.id; }))
            out.push_back(std::move(t));
    }
    return out;
}

inline std::string serialize(const std::vector<Tab>& tabs) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& t : tabs) {
        nlohmann::json e = {{"id", t.id}};
        if (!t.title.empty()) e["title"] = t.title;
        if (t.hidden) e["hidden"] = true;
        j.push_back(e);
    }
    return j.dump();
}

/* The tabs this database shows: its stored list, in its order, keeping only
 * built-ins and screens that exist, then every built-in it leaves out and every
 * screen it does not list yet (a screen made on another device, or by a command). */
inline std::vector<Tab> effective(const std::vector<Tab>& stored, const std::vector<std::string>& screens) {
    std::vector<Tab> out;
    auto has = [&](const std::string& id) {
        return std::any_of(out.begin(), out.end(), [&](const Tab& t) { return t.id == id; });
    };
    for (const auto& t : stored) {
        const bool ok = builtin(t.id) ||
                        (is_screen(t.id) && std::find(screens.begin(), screens.end(), screen_mantle(t.id)) != screens.end());
        if (ok && !has(t.id)) out.push_back(t);
    }
    for (const auto& b : builtins())
        if (!has(b.id)) out.push_back({b.id, "", false});
    for (const auto& m : screens)
        if (!has("screen:" + m)) out.push_back({"screen:" + m, "", false});
    return out;
}

/* ── the `shell` verbs (the console, an agent, a screen's action) ─────────────
 *   shell                    the tabs, in order
 *   shell move <id> <n>      put a tab at position n (1 = first)
 *   shell hide <id>          start it closed in this database
 *   shell show <id>          start it open
 *   shell title <id> <text>  a screen tab's title
 * Each writes `shell.tabs` with one `config set`. */
struct VerbResult {
    bool ok = true;
    std::vector<std::string> commands;
    std::vector<Tab> tabs; // the list the commands write (empty: nothing changed)
    std::string text;
};

inline VerbResult verb(const std::vector<std::string>& a, const std::vector<Tab>& tabs) {
    VerbResult r;
    auto fail = [&](std::string m) {
        r.ok = false;
        r.text = std::move(m);
        return r;
    };
    if (a.size() <= 1) {
        int k = 0;
        for (const auto& t : tabs)
            r.text += std::to_string(++k) + ". " + t.id + (t.title.empty() ? "" : " \"" + t.title + "\"") +
                      (t.hidden ? "  (starts closed)" : "") + "\n";
        return r;
    }
    const std::string& v = a[1];
    if (a.size() < 3) return fail("usage: shell [move <tab> <n> | hide <tab> | show <tab> | title <tab> <text>]");
    std::vector<Tab> next = tabs;
    auto it = std::find_if(next.begin(), next.end(), [&](const Tab& t) { return t.id == a[2]; });
    if (it == next.end()) return fail("no tab called " + a[2] + " - `shell` lists them");
    if (v == "move") {
        if (a.size() < 4) return fail("usage: shell move <tab> <n>");
        const int n = std::atoi(a[3].c_str());
        if (n < 1 || n > (int)next.size()) return fail("a position from 1 to " + std::to_string(next.size()));
        Tab t = *it;
        next.erase(it);
        next.insert(next.begin() + (n - 1), t);
    } else if (v == "hide" || v == "show") {
        it->hidden = v == "hide";
    } else if (v == "title") {
        if (!is_screen(it->id)) return fail("a built-in tab keeps its own name");
        std::string t;
        for (size_t i = 3; i < a.size(); ++i) t += (i > 3 ? " " : "") + a[i];
        it->title = t;
    } else {
        return fail("usage: shell [move <tab> <n> | hide <tab> | show <tab> | title <tab> <text>]");
    }
    r.commands.push_back("config set shell.tabs " + maiz::arg(serialize(next))); // the JSON array, one argument
    r.tabs = next;
    r.text = "the tabs of this database are saved with it";
    return r;
}

} // namespace hormiga::shell
