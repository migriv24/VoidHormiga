/* domain/quick_add.hpp — the Calendar's one-line creation grammar (C1f).
 *
 * "Food drive 3pm-5pm" -> an event named `food-drive`, 15:00-17:00. The fastest
 * path from a thought to a dated rune, and the gesture the Calendar was missing
 * entirely: creating an entry took a right-click, a menu item, and then a trip
 * to the inspector to give the thing a name.
 *
 * Pure — string in, struct out, no `HormigaApp`, no ImGui, no I/O, and no
 * header that reaches any of them. `tests/calendar_smoke.cpp` links NOTHING to
 * check this grammar, which is the difference between being pure and being
 * shareable: `render/text.hpp` says the same thing about its helpers and is
 * right about the functions and wrong about the header (see domain/clock.hpp).
 *
 * THE GRAMMAR IS DELIBERATELY SMALL, because a quick-add that guesses wrong is
 * slower than one that does nothing: you have to notice it guessed, undo, and
 * retype.
 *
 *   !<text>          an INCIDENT rather than an event (the two kinds stay
 *                    visibly distinct, per the standing directive)
 *   <text> <time>    a start time; the end defaults to an hour later
 *   <text> <t>-<t>   a start and an end ("3pm-5pm", "15:00-17:00", "3 to 5pm")
 *   <text>           all-day
 */
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "domain/clock.hpp" // parse_clock — ONE time parser, every surface

namespace hormiga {
namespace quick {

struct Parsed {
    std::string glyph = "event"; // "event" | "incident"
    std::string name;            // the human title, as typed
    float t0 = -1.0f;            // start hour, <0 = all-day
    float t1 = -1.0f;            // end hour, <0 = all-day
};

/* Does ONE token read as a clock time?
 *
 * `parse_clock` alone is far too permissive for this job. It exists to read
 * what a flier printed, so it happily finds a time in "Ward 5 meeting" (five
 * o'clock) and in "Route 66 cleanup" (something stranger). A quick-add line is
 * a title with a time *appended*, so the gate has to be strict or the feature
 * is a trap: the token must carry a COLON or a MERIDIEM, or be the word `noon`
 * or `midnight`. That strictness is the whole reason the grammar is trustworthy
 * enough to bind to the Enter key. */
inline bool clock_token(const std::string& tok, float& out) {
    if (tok.empty()) return false;
    std::string s;
    for (char c : tok) s += (char)std::tolower((unsigned char)c);
    const bool worded = s == "noon" || s == "midnight";
    const bool meridiem = s.find("am") != std::string::npos ||
                          s.find("pm") != std::string::npos ||
                          s.find("a.m") != std::string::npos ||
                          s.find("p.m") != std::string::npos;
    if (!worded && !meridiem && s.find(':') == std::string::npos) return false;
    if (!worded && !std::isdigit((unsigned char)s[0])) return false;
    int hh = 0, mm = 0;
    if (!parse_clock(s, hh, mm)) return false;
    out = hh + mm / 60.0f;
    return true;
}

/* Parse a quick-add line. False when there is nothing nameable in it — a line
 * that is ONLY a time has no title, and minting `event-7` from it would be
 * exactly the un-named rune this feature exists to stop producing. */
inline bool parse(const std::string& line, Parsed& out) {
    out = Parsed{};
    std::string body = line;
    while (!body.empty() && std::isspace((unsigned char)body.front()))
        body.erase(body.begin());
    while (!body.empty() && std::isspace((unsigned char)body.back()))
        body.pop_back();
    if (!body.empty() && body.front() == '!') {
        out.glyph = "incident";
        body.erase(body.begin());
    }
    std::vector<std::string> tok;
    {
        std::string cur;
        for (char c : body) {
            if (std::isspace((unsigned char)c)) {
                if (!cur.empty()) { tok.push_back(cur); cur.clear(); }
            } else cur += c;
        }
        if (!cur.empty()) tok.push_back(cur);
    }
    if (tok.empty()) return false;
    /* Peel a time expression off the END, longest suffix first: up to five
     * tokens, because "3 pm to 5 pm" is four and "3pm-5pm" is one, and every
     * shape in between is something a person actually types. A standalone
     * "to"/"until"/"-" normalizes to the range separator. */
    size_t name_end = tok.size();
    for (size_t take = std::min<size_t>(5, tok.size()); take >= 1; --take) {
        // a suffix that swallows the WHOLE line leaves no title — try a
        // shorter one rather than giving up, or "Food drive 3pm-5pm" (three
        // tokens, tried at three first) never reaches the suffix that works
        if (take == tok.size()) continue;
        std::string joined;
        for (size_t i = tok.size() - take; i < tok.size(); ++i) {
            std::string t = tok[i];
            for (char& c : t) c = (char)std::tolower((unsigned char)c);
            if (t == "to" || t == "until" || t == "-") joined += '-';
            else joined += t;
        }
        std::string lhs = joined, rhs;
        for (size_t i = 1; i + 1 < joined.size(); ++i)
            if (joined[i] == '-') {
                lhs = joined.substr(0, i);
                rhs = joined.substr(i + 1);
                break;
            }
        float a = -1.0f, b = -1.0f;
        if (!clock_token(lhs, a)) continue;
        if (!rhs.empty() && !clock_token(rhs, b)) continue;
        out.t0 = a;
        out.t1 = b;
        name_end = tok.size() - take;
        break;
    }
    std::string title;
    for (size_t i = 0; i < name_end; ++i)
        title += (title.empty() ? "" : " ") + tok[i];
    if (title.empty()) return false;
    /* A line that is ONLY a time is not a quick-add. The suffix loop skips a
     * whole-line match so that "Food drive 3pm-5pm" can find a shorter suffix,
     * which leaves a bare "3pm" falling through as a TITLE — an all-day event
     * called "3pm", which is exactly the nonsense rune this grammar exists to
     * stop minting. Say no and let the caller explain. */
    if (name_end == tok.size()) {
        float only = -1.0f;
        std::string whole;
        for (const auto& t : tok) whole += t;
        if (clock_token(whole, only)) return false;
    }
    out.name = title;
    if (out.t0 >= 0 && out.t1 < 0) out.t1 = out.t0 + 1.0f; // a start means an hour
    return true;
}

/* The rune name a title becomes: lowercase, alphanumerics, single hyphens.
 * Matches how the CSV/rescue imports slugified titles into names (Q2), so the
 * calendar mints names that look like the rest of the database's. */
inline std::string slug(const std::string& title, size_t cap = 48) {
    std::string s;
    for (unsigned char u : title) {
        if (std::isalnum(u)) s += (char)std::tolower(u);
        else if (!s.empty() && s.back() != '-') s += '-';
    }
    while (!s.empty() && s.back() == '-') s.pop_back();
    if (s.size() > cap) s.resize(cap);
    while (!s.empty() && s.back() == '-') s.pop_back();
    return s;
}

} // namespace quick
} // namespace hormiga
