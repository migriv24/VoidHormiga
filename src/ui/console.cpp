/* ui/console.cpp — the console: the transcript, and the command bar.
 *
 * WHY THIS LEFT `maiz::draw_log_strip` (2026-09-20). The strip draws a log; a
 * console is a place a person and an agent both READ and WORK IN, and the
 * author's asks were all about the second thing:
 *
 *   - *"i should be able to select and copy certain portions of text"*
 *   - *"i should be able to clear the console, which doesn't delete the
 *      historic record"*
 *   - *"different amounts of information ... time stamps CAN be useful
 *      sometimes ... if an agent wants to read the log, time stamps might just
 *      be a waste of tokens"*
 *   - *"the interactions between these different void applications should have
 *      different colors ... along with some shorthand claiming who the logic
 *      came from"* — CRE, MAZ, PLB, ALM, HRG.
 *
 * None of that is Hormiga-shaped, and the author says so: the console is where
 * several libraries meet over one Void Core. So this is written to be LIFTED --
 * the tagging table is data, the filters are a struct, and nothing here knows
 * about contacts or newsletters. It is proposed to Void Maiz in
 * MESSAGE_FOR_VOIDMAIZ_hormiga-the-console-is-a-shared-surface-2026-09-20.md.
 *
 * WHAT IS NOT HERE: the log itself. `clear` moves a mark; it deletes nothing.
 * The transcript IS the session (`log_to_text` still copies all of it), and a
 * console that could destroy the record would be a console that can lie.
 */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"  // the profile is typeable here too

#include <ctime>

namespace {

/* WHOSE LOGIC SPOKE. An entry carries a level and an `op`, and the op is enough
 * to say which library answered -- a dispatcher verb is Void Core's, `sync` is
 * Void Palabra's under Void Maiz's seam, a script is Allomone's. Hormiga owns
 * the rest. Kept as a table rather than a chain of ifs, because the day this
 * moves upstream the table is what each host would register into. */
struct Source {
    const char* tag;      // CRE | MAZ | PLB | ALM | HRG
    ImU32 color;
    const char* who;      // for the tooltip
};

const Source kCore    = {"CRE", IM_COL32(120, 170, 230, 255), "Void Core - the model and the dispatcher"};
const Source kMaiz    = {"MAZ", IM_COL32(150, 205, 140, 255), "Void Maiz - the view, presence, networking"};
const Source kPalabra = {"PLB", IM_COL32(205, 170, 120, 255), "Void Palabra - the merge and the sync"};
const Source kAllo    = {"ALM", IM_COL32(190, 150, 220, 255), "Allomone - the script language"};
const Source kHormiga = {"HRG", IM_COL32(200, 200, 205, 255), "Void Hormiga - this application"};

bool one_of(const std::string& s, std::initializer_list<const char*> set) {
    for (const char* k : set)
        if (s == k) return true;
    return false;
}

const Source& source_of(const maiz::LogEntry& e) {
    // a dispatched command echoes with the command line in `op`
    const std::string head = e.op.substr(0, e.op.find(' '));
    if (e.level == ">" || one_of(head, {"rune", "set", "setjson", "tag", "link", "unlink",
                                        "mantle", "use", "glyph", "glyphs", "rm", "undo",
                                        "redo", "batch", "config", "place", "relate",
                                        "unrelate", "bind", "unbind", "facet", "measure"}))
        return kCore;
    if (one_of(e.op, {"sync"})) return kPalabra;
    if (one_of(e.op, {"profile", "presence", "share", "net", "join"})) return kMaiz;
    if (one_of(e.op, {"script", "allomone", "rule"})) return kAllo;
    return kHormiga;
}

ImU32 level_color(const std::string& level) {
    if (level == "error") return IM_COL32(230, 105, 95, 255);
    if (level == "warn") return IM_COL32(230, 180, 90, 255);
    return IM_COL32(170, 175, 185, 255);
}

/* The view-tier chatter `log_to_text(condensed)` drops: the same rule, so
 * "Only what changed the database" on screen and "copy condensed" agree. */
bool is_noise(const maiz::LogEntry& e) {
    if (e.level == "error" || e.level == "warn") return false;
    const std::string head = e.op.substr(0, e.op.find(' '));
    if (head == "config") return true;
    return e.op.find(" pos ") != std::string::npos ||
           e.op.find(" size ") != std::string::npos ||
           e.op.find(" collapsed ") != std::string::npos;
}

std::string clock_stamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%H:%M:%S", &tm);
    return buf;
}

}  // namespace

/* Stamp every line the moment it first appears. `maiz::LogEntry` carries no
 * clock and does not need one: a timestamp is a VIEW of when this session saw a
 * line, which is exactly what the console offers to turn off. */
void HormigaApp::console_stamp_new_lines() {
    while (console.stamps.size() < log.size()) console.stamps.push_back(clock_stamp());
    if (console.cleared_to > log.size()) console.cleared_to = 0;  // a new database
}

void HormigaApp::draw_console() {
    console_stamp_new_lines();

    if (ImGui::SmallButton("copy condensed"))
        ImGui::SetClipboardText(maiz::log_to_text(log, true).c_str());
    flow_button("copy all");
    if (ImGui::SmallButton("copy all"))
        ImGui::SetClipboardText(maiz::log_to_text(log, false).c_str());
    flow_button("clear");
    if (ImGui::SmallButton("clear")) console.cleared_to = log.size();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("clears the SCREEN only.\nThe transcript is untouched - "
                          "\"copy all\" still has every line.");
    if (console.cleared_to) {
        flow_button("show everything");
        if (ImGui::SmallButton("show everything")) console.cleared_to = 0;
    }
    flow_button("as text");
    if (ImGui::SmallButton(console.as_text ? "as lines" : "as text"))
        console.as_text = !console.as_text;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("as text: one box you can select and copy from.\n"
                          "as lines: coloured, with who said what.");
    flow(ImGui::CalcTextSize("the transcript IS the session").x, 16);
    ImGui::TextDisabled("the transcript IS the session");

    const float footer = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##lines", ImVec2(0, -footer));

    /* AS TEXT: a read-only multiline box, because ImGui gives real selection
     * only inside one. This is the mode for "copy these four lines", and the
     * price is that the colours and the tags go -- so it is a toggle rather
     * than the default. */
    if (console.as_text) {
        std::string body;
        for (std::size_t i = console.cleared_to; i < log.size(); ++i) {
            const maiz::LogEntry& e = log[i];
            if (console.only_changes && is_noise(e)) continue;
            if (console.timestamps) body += console.stamps[i] + "  ";
            if (console.sources) body += std::string(source_of(e).tag) + "  ";
            body += "[" + e.level + "] " + e.op + (e.msg.empty() ? "" : ": " + e.msg) + "\n";
        }
        ImGui::InputTextMultiline("##text", body.data(), body.size() + 1,
                                  ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
        ImGui::EndChild();
        console_command_bar();
        return;
    }

    for (std::size_t i = console.cleared_to; i < log.size(); ++i) {
        const maiz::LogEntry& e = log[i];
        if (console.only_changes && is_noise(e)) continue;
        ImGui::PushID((int)i);
        if (console.timestamps) {
            ImGui::TextDisabled("%s", console.stamps[i].c_str());
            ImGui::SameLine();
        }
        const Source& src = source_of(e);
        if (console.sources) {
            ImGui::PushStyleColor(ImGuiCol_Text, src.color);
            ImGui::TextUnformatted(src.tag);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", src.who);
            ImGui::SameLine();
        }
        ImGui::PushStyleColor(ImGuiCol_Text, level_color(e.level));
        ImGui::PushTextWrapPos(0.0f);
        const std::string line =
            "[" + e.level + "] " + e.op + (e.msg.empty() ? "" : ": " + e.msg);
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        /* One line at a time, for the person who wants THIS error and not the
         * whole session. "as text" is for a range. */
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::SetClipboardText(line.c_str());
            toast("line copied");
        }
        ImGui::PopID();
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    console_command_bar();
}

/* ── the command bar, and the two words a console is expected to know ────────
 *
 * `ls` and `cd` reach Void Core as holiday navigation, and the author asked for
 * something else: *"'ls' should list the mantles we're working with, and 'cd'
 * is a way to get into different mantles ... instead of directories, we are
 * working with graphs."* So they are answered HERE, before dispatch, over
 * mantles. `use` still does what it always did; this is the vocabulary a person
 * types without thinking. Asked upstream (Void Core owns the verbs) rather than
 * left as a local dialect. */
void HormigaApp::console_command_bar() {
    maiz::CanvasIO bio = maiz::draw_command_bar(cmdbar);
    for (const auto& cmd : bio.commands) {
        std::string c = cmd;
        while (!c.empty() && c.back() == ' ') c.pop_back();
        const std::string head = c.substr(0, c.find(' '));
        const std::string rest = c.find(' ') == std::string::npos
                                     ? std::string()
                                     : c.substr(c.find(' ') + 1);

        if (head == "clear") {  // a view word: the record is untouched
            console.cleared_to = log.size();
            continue;
        }
        if (head == "ls" && rest.empty()) {
            std::string out;
            for (const auto& m : mantles_here())
                out += (m == scene.mantle ? "* " : "  ") + m + "\n";
            log.push_back({">", c, out.empty() ? "no graphs yet" : out});
            continue;
        }
        if (head == "cd") {
            if (rest.empty() || rest == "~") {
                log.push_back({">", c, "the graphs here: type `ls`"});
                continue;
            }
            const auto here = mantles_here();
            if (std::find(here.begin(), here.end(), rest) == here.end()) {
                log.push_back({"error", c, "no graph called \"" + rest + "\" - `ls` lists them"});
                continue;
            }
            maiz::Result r = dispatch_and_reproject("use " + rest);
            log.push_back({">", c, r.ok ? "now in " + rest : r.text()});
            continue;
        }
        /* THE PROFILE IS VOID MAIZ'S, AND IT IS STILL TYPEABLE HERE. The
         * author: *"i understand that the profile is specific to maiz now,
         * which is all the more reason that the maiz specific stuff should
         * still be in the CLI"*. `profile` shows it; `profile username <name>`
         * and `profile color #rrggbb` change it. NOT a dispatcher command on
         * purpose: the journal travels with a shared database, and a replayed
         * "set username" would rewrite another member's profile. */
        if (head == "profile") {
            LanRuntime& rt = LanRuntime::of(*this);
            const std::string what = rest.substr(0, rest.find(' '));
            const std::string val = rest.find(' ') == std::string::npos
                                        ? std::string()
                                        : rest.substr(rest.find(' ') + 1);
            if (what == "username" && !val.empty()) {
                rt.me.username = val;
                hormiga::profile::save(rt.me);
                LanRuntime::refresh_self(*this);
                log.push_back({"info", "profile", "username set to " + val});
            } else if (what == "color" && !val.empty()) {
                rt.me.color = val;
                hormiga::profile::save(rt.me);
                LanRuntime::refresh_self(*this);
                log.push_back({"info", "profile", "colour set to " + val});
            } else if (!what.empty()) {
                log.push_back({"error", c, "usage: profile [username <name> | color #rrggbb]"});
                continue;
            }
            log.push_back({"info", "profile",
                           "you are " + hormiga::profile::display_name(rt.me) + " (" +
                               rt.me.color + "), key " +
                               LanRuntime::fingerprint(rt).substr(0, 8) + " - " +
                               "the same on every database on this computer"});
            continue;
        }
        if (head == "pwd") {
            log.push_back({">", c, scene.mantle.empty() ? "no graph" : scene.mantle});
            continue;
        }

        if (try_map_verb(c)) continue;  // `map …` verb macros (one batch)
        if (try_doc_verb(c)) continue;  // `doc …` — the Builder's verbs
        maiz::Result r = dispatch_and_reproject(c);
        log.push_back({">", c, r.text().empty() ? (r.ok ? "ok" : "failed") : r.text()});
    }
}

std::vector<std::string> HormigaApp::mantles_here() {
    std::vector<std::string> out;
    for (std::string line : core.dispatch("mantles").lines) {
        if (line.empty() || line == "(no mantles)") continue;
        if (line.front() == '*') line.erase(0, 1);
        while (!line.empty() && line.front() == ' ') line.erase(line.begin());
        if (const auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        if (!line.empty()) out.push_back(line);
    }
    return out;
}
