/* domain/actions.hpp — what a screen's button or input does, compiled to lines
 * of the console's own language (okf/concepts/platform/shell.md, "Actions").
 *
 * The author (2026-11-15): *"give clients the tools to implement the
 * functionality themselves."* An action is a few lines a person could type in
 * the console: dispatcher commands and Hormiga's verbs, with placeholders filled
 * when it runs. No new language: logic that decides is Allomone's (derive-only,
 * Q135); effects that leave the device are the Antfarm's.
 *
 *   {{input}}            what was typed (or scanned) into the input that ran it
 *   {{selected}}         the screen's selected rune's name
 *   {{selected.<field>}} one of its fields
 *   {{new}}              a fresh rune name, the same everywhere in one run
 *   {{today}}            YYYY-MM-DD
 *
 * THE RULES THAT MAKE A RECEIVED .miga SAFE TO CLICK:
 *   - a VALUE is always its own argument, quoted by Void Core's rule
 *     (`maiz::arg`), so what someone types or scans can never become a second
 *     argument, let alone a second command: a newline in it is inside the quotes;
 *   - a NAME goes in bare only if it is command-safe; otherwise the action is
 *     refused, whole, before anything runs;
 *   - an unknown placeholder, or one with nothing to fill it (no selection), is a
 *     refusal that says which, never an empty string.
 *
 * Pure: text and a context in, lines or a reason out. */
#pragma once

#include "domain/scene_value.hpp" // field_value
#include "voidmaiz/embed.hpp"     // maiz::arg
#include "voidmaiz/scene.hpp"

#include <cctype>
#include <functional>
#include <string>
#include <vector>

namespace hormiga::actions {

struct Context {
    std::string input;                         // what the input held ("" for a button)
    bool has_input = false;                    // ran from an input (so {{input}} may be empty text)
    const maiz::SceneNode* selected = nullptr; // the screen's selection
    std::string today;                         // YYYY-MM-DD
    std::function<std::string()> mint;         // a fresh, unused rune name
};

struct Compiled {
    std::vector<std::string> lines;
    std::string error; // non-empty: nothing runs
    bool ok() const { return error.empty(); }
};

/* A name that can stand bare in a command: what `rune new` mints and accepts. */
inline bool command_safe(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')) return false;
    return true;
}

inline Compiled compile(const std::string& text, const Context& cx) {
    Compiled out;
    std::string fresh; // {{new}}: minted once per run
    auto fill = [&](const std::string& key, std::string& into) -> bool {
        if (key == "input") {
            if (!cx.has_input) return out.error = "{{input}} is only for an input's action", false;
            into += maiz::arg(cx.input);
            return true;
        }
        if (key == "today") {
            into += maiz::arg(cx.today);
            return true;
        }
        if (key == "new") {
            if (fresh.empty()) fresh = cx.mint ? cx.mint() : std::string();
            if (!command_safe(fresh)) return out.error = "no fresh name could be made", false;
            into += fresh;
            return true;
        }
        if (key == "selected" || key.rfind("selected.", 0) == 0) {
            if (!cx.selected) return out.error = "select something first: this action uses {{" + key + "}}", false;
            if (key == "selected") {
                if (!command_safe(cx.selected->name))
                    return out.error = "the selected rune's name cannot stand in a command", false;
                into += cx.selected->name;
                return true;
            }
            into += maiz::arg(field_value(*cx.selected, key.substr(9)));
            return true;
        }
        out.error = "{{" + key + "}} is not something an action knows (input, selected, selected.<field>, new, today)";
        return false;
    };
    size_t i = 0;
    while (i <= text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) j = text.size();
        std::string line = text.substr(i, j - i);
        i = j + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t b = 0;
        while (b < line.size() && (line[b] == ' ' || line[b] == '\t')) ++b;
        line = line.substr(b);
        if (line.empty() || line[0] == '#') continue; // blank lines and comments
        std::string built;
        size_t k = 0;
        while (k < line.size()) {
            const size_t open = line.find("{{", k);
            if (open == std::string::npos) {
                built += line.substr(k);
                break;
            }
            const size_t close = line.find("}}", open + 2);
            if (close == std::string::npos) {
                out.error = "a {{ with no }} in: " + line;
                out.lines.clear();
                return out;
            }
            built += line.substr(k, open - k);
            std::string key = line.substr(open + 2, close - open - 2);
            while (!key.empty() && key.front() == ' ') key.erase(0, 1);
            while (!key.empty() && key.back() == ' ') key.pop_back();
            if (!fill(key, built)) {
                out.lines.clear();
                return out;
            }
            k = close + 2;
        }
        out.lines.push_back(built);
    }
    if (out.lines.empty()) out.error = "this action has no lines yet";
    return out;
}

/* A line that is one of Hormiga's verbs runs as that verb; the rest are
 * dispatcher commands, which a run groups into one batch (one undo). */
inline bool is_verb_line(const std::string& line) {
    for (const char* v : {"map ", "doc ", "farm ", "shell "})
        if (line.rfind(v, 0) == 0) return true;
    return line == "map" || line == "doc" || line == "farm" || line == "shell";
}

} // namespace hormiga::actions
