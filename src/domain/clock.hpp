/* domain/clock.hpp — ONE wall-clock parser, for every surface that reads a time
 * a person typed.
 *
 * Split out of `render/text.hpp` 2026-09-10. It lived there, it was pure, and
 * it was still unreachable: that header includes `app/app_internal.hpp`, so
 * anything wanting the parser had to take ImGui and the entire application with
 * it. The Calendar grid could not, so it grew its own — `sscanf("%d:%d")` — and
 * for three weeks the screen and the `.ics` export disagreed about what time an
 * event was, on the case `text.hpp` itself calls the common one:
 *
 *   "Every time in a real community database is 12-hour with a meridiem,
 *    because that is what a flier prints."
 *
 * Against that input the grid's parser read "3:00 PM" as three in the MORNING
 * and dropped "9 AM" off the time grid entirely as an all-day entry. The export
 * was right and the screen was wrong, which is the worst arrangement available:
 * what you verify is correct and what you look at is not.
 *
 * So the rule is structural now rather than aspirational. This header includes
 * `<cctype>` and `<string>` and nothing else, which is what lets the renderers,
 * the calendar, the quick-add grammar and (next) the iCalendar lens all share
 * it. Same discipline as the styling engine, whose value the OKF describes as
 * "the ABSENCE of a second styling system".
 */
#pragma once

#include <cctype>
#include <string>

/* A wall-clock time as a human writes it → 24-hour (h, m). Accepts `9:00 AM`,
 * `09:00`, `9 AM`, `3:00 p.m.`, `15:00`, `noon`, `midnight`. Returns false when
 * there is no time in there at all, which is an ALL-DAY event and not an error.
 *
 * Deliberately permissive on input and strict on output: the model stores what
 * the flier said, and the calendar file is the one place that has to be exact. */
inline bool parse_clock(const std::string& raw, int& hh, int& mm) {
    std::string s;
    for (char c : raw) s += (char)std::tolower((unsigned char)c);
    if (s.find("noon") != std::string::npos) { hh = 12; mm = 0; return true; }
    if (s.find("midnight") != std::string::npos) { hh = 0; mm = 0; return true; }
    size_t i = 0;
    while (i < s.size() && !std::isdigit((unsigned char)s[i])) ++i;
    if (i >= s.size()) return false;
    int h = 0, m = 0;
    while (i < s.size() && std::isdigit((unsigned char)s[i]))
        h = h * 10 + (s[i++] - '0');
    if (i < s.size() && s[i] == ':') {
        ++i;
        int digits = 0;
        while (i < s.size() && std::isdigit((unsigned char)s[i]) && digits < 2) {
            m = m * 10 + (s[i++] - '0');
            ++digits;
        }
    }
    if (h > 23 || m > 59) return false;
    // the meridiem, which the old arithmetic threw away — the difference
    // between a 3 PM meeting and one at three in the morning
    const size_t p = s.find('p', i ? i - 1 : 0);
    const size_t a = s.find('a', i ? i - 1 : 0);
    const bool pm = p != std::string::npos && p + 1 < s.size() &&
                    (s[p + 1] == 'm' || s[p + 1] == '.');
    const bool am = a != std::string::npos && a + 1 < s.size() &&
                    (s[a + 1] == 'm' || s[a + 1] == '.');
    if (pm && h < 12) h += 12;
    if (am && h == 12) h = 0;   // 12:30 AM is 00:30
    hh = h;
    mm = m;
    return true;
}

