/* domain/rrule.hpp — recurrence, AUTHORED HERE.
 *
 * ── WHY THIS EXISTS ──────────────────────────────────────────────────────────
 *
 * Because until 2026-09-11 it did not, and the consequence was the sharpest
 * thing anybody had said about this section. A community organization's most
 * ordinary recurring event —
 *
 *     set standing days "Last Friday of the Month"
 *
 * — appeared NOWHERE. Not on the month grid, not in week view, not in the
 * agenda, not in the `.ics`, not to a subscriber. `days` is free text that the
 * newsletter prints and the calendar cannot read, and `cal_entries_on` needs a
 * parseable `date`, which a recurring event does not have.
 *
 * Meanwhile the importer could read a recurring event out of Google and store
 * its `RRULE`. **So Hormiga could express somebody else's standing meeting and
 * not its own**, which is exactly backwards. The author's correction:
 *
 *   "We shouldn't NEED other calendars. Just like the database, where we have
 *    a local version of our own data, the calendar data doesn't NEED to live
 *    somewhere outside of us. We can also create it."
 *
 * A capability you can only obtain by importing it is a hole in your own
 * application. This is the hole closed.
 *
 * ── THE MODEL IS RFC 5545's, BECAUSE IT ALREADY WAS ──────────────────────────
 *
 * A recurring event is `DTSTART` + `RRULE`: a first occurrence, and a rule for
 * the rest. That is what the standard says, what every other calendar stores,
 * and — not coincidentally — what our `date` + `rrule` fields already are. So
 * this is not a parallel recurrence model bolted beside the interop one; it IS
 * the interop one, authored from our side. Round-tripping is then a property
 * rather than a feature: what you write here is what a subscriber receives, and
 * what the importer reads is what this expands.
 *
 * ── AND IT IS DELIBERATELY A SUBSET ──────────────────────────────────────────
 *
 * RFC 5545 recurrence is enormous and most of it is unreachable in practice.
 * The roadmap's research already recorded the reason to stop early: *"Outlook
 * desktop is the strictest. Recurring events with complex RRULEs often fail to
 * import. If you are targeting Outlook desktop, stick to weekly/monthly with
 * simple BYDAY clauses."* So the authored vocabulary is the set an outreach
 * organization actually uses and every client actually honours:
 *
 *     every N days · weekly on chosen weekdays · monthly on a date
 *     monthly on the Nth weekday (incl. LAST) · yearly
 *
 * Anything richer that ARRIVES from a feed is still stored verbatim and still
 * expanded when it is one of the shapes below; when it is not, the entry shows
 * on its start date and says so rather than guessing. Silence would be the
 * wrong answer, and so would a wrong date.
 *
 * Pure: string in, dates out. No ImGui, no Core, no I/O.
 */
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace hormiga {
namespace rrule {

enum class Freq { None, Daily, Weekly, Monthly, Yearly };

/* A parsed rule. `by_day` holds 0=Sun … 6=Sat; `set_pos` is the Nth weekday of
 * a month (1..5, or -1 for LAST). */
struct Rule {
    Freq freq = Freq::None;
    int interval = 1;
    std::vector<int> by_day;      // weekly: which days. monthly: with set_pos.
    int by_month_day = 0;         // monthly on a date (1..31)
    int set_pos = 0;              // monthly: 1..5, or -1 = last
    int count = 0;                // stop after N occurrences (0 = no limit)
    std::string until;            // "YYYY-MM-DD" inclusive ("" = no limit)
    bool understood = false;      // false = store it, do not pretend to expand
};

// ── civil-calendar helpers (the same arithmetic the grid uses) ──────────────

inline bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

inline int days_in_month(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && leap(y)) ? 29 : d[m - 1];
}

/* Day of week, 0=Sunday. Sakamoto's method — no `mktime`, so it is pure and
 * cannot be moved by the host's timezone. */
inline int dow(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

struct Date {
    int y = 0, m = 0, d = 0;
    bool operator<(const Date& o) const {
        return y != o.y ? y < o.y : (m != o.m ? m < o.m : d < o.d);
    }
    bool operator<=(const Date& o) const { return !(o < *this); }
    bool operator==(const Date& o) const { return y == o.y && m == o.m && d == o.d; }
};

inline std::string to_string(const Date& x) {
    char b[16];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d", x.y, x.m, x.d);
    return b;
}

inline bool from_string(const std::string& s, Date& out) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = {y, m, d};
    return true;
}

/* Move a civil date by whole days, in EITHER direction.
 *
 * The backwards half is not decoration: the weekly expander walks from the
 * start date back to its own Sunday before stepping week by week, and the first
 * version of this function had `while (n > 0)` and silently did nothing for a
 * negative delta. Every weekly rule was then anchored to the wrong weekday —
 * "every Tuesday and Thursday" produced Thursdays and Saturdays — and it was
 * wrong by a consistent offset, which is the kind of wrong that looks like a
 * plausible calendar until somebody misses a meeting. Caught by the tests
 * before anything was wired to it. */
inline void add_days(Date& x, int n) {
    while (n > 0) {
        const int dim = days_in_month(x.y, x.m);
        if (x.d < dim) { ++x.d; }
        else { x.d = 1; if (++x.m > 12) { x.m = 1; ++x.y; } }
        --n;
    }
    while (n < 0) {
        if (x.d > 1) { --x.d; }
        else {
            if (--x.m < 1) { x.m = 12; --x.y; }
            x.d = days_in_month(x.y, x.m);
        }
        ++n;
    }
}

inline void add_months(Date& x, int n) {
    int total = (x.y * 12 + (x.m - 1)) + n;
    x.y = total / 12;
    x.m = total % 12 + 1;
    if (x.d > days_in_month(x.y, x.m)) x.d = days_in_month(x.y, x.m);
}

// ── parsing an RRULE ────────────────────────────────────────────────────────

inline int weekday_code(const std::string& s) {
    static const char* names[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    for (int i = 0; i < 7; ++i)
        if (s == names[i]) return i;
    return -1;
}

inline std::string weekday_name(int d) {
    static const char* names[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    return (d >= 0 && d < 7) ? names[d] : "";
}

/* Parse `FREQ=MONTHLY;BYDAY=-1FR;INTERVAL=1`. `understood` says whether we can
 * expand it — a rule we cannot is kept, reported, and never guessed at. */
inline Rule parse(const std::string& raw) {
    Rule r;
    if (raw.empty()) return r;
    std::string s;
    for (char c : raw) s += (char)std::toupper((unsigned char)c);
    if (s.rfind("RRULE:", 0) == 0) s = s.substr(6);

    std::vector<std::string> parts;
    {
        std::string cur;
        for (char c : s) {
            if (c == ';') { parts.push_back(cur); cur.clear(); }
            else cur += c;
        }
        parts.push_back(cur);
    }
    bool has_freq = false, exotic = false;
    for (const auto& part : parts) {
        const size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = part.substr(0, eq), v = part.substr(eq + 1);
        if (k == "FREQ") {
            has_freq = true;
            if (v == "DAILY") r.freq = Freq::Daily;
            else if (v == "WEEKLY") r.freq = Freq::Weekly;
            else if (v == "MONTHLY") r.freq = Freq::Monthly;
            else if (v == "YEARLY") r.freq = Freq::Yearly;
            else exotic = true; // HOURLY / MINUTELY / SECONDLY
        } else if (k == "INTERVAL") {
            r.interval = std::max(1, std::atoi(v.c_str()));
        } else if (k == "COUNT") {
            r.count = std::atoi(v.c_str());
        } else if (k == "UNTIL") {
            int y = 0, m = 0, d = 0;
            if (std::sscanf(v.c_str(), "%4d%2d%2d", &y, &m, &d) == 3) {
                char b[16];
                std::snprintf(b, sizeof b, "%04d-%02d-%02d", y, m, d);
                r.until = b;
            }
        } else if (k == "BYDAY") {
            std::string item;
            auto take = [&](const std::string& it) {
                if (it.empty()) return;
                // an optional signed ordinal prefix: "-1FR", "3TU"
                size_t i = 0;
                int sign = 1;
                if (it[i] == '+' || it[i] == '-') { sign = it[i] == '-' ? -1 : 1; ++i; }
                int ord = 0;
                while (i < it.size() && std::isdigit((unsigned char)it[i]))
                    ord = ord * 10 + (it[i++] - '0');
                const int wd = weekday_code(it.substr(i));
                if (wd < 0) { exotic = true; return; }
                r.by_day.push_back(wd);
                if (ord) r.set_pos = sign * ord;
            };
            for (char c : v) {
                if (c == ',') { take(item); item.clear(); }
                else item += c;
            }
            take(item);
        } else if (k == "BYMONTHDAY") {
            r.by_month_day = std::atoi(v.c_str());
        } else if (k == "WKST") {
            // harmless for the shapes we expand
        } else {
            // BYSETPOS, BYWEEKNO, BYYEARDAY, BYMONTH, ... — real and rare
            exotic = true;
        }
    }
    if (!has_freq) return r;
    r.understood = !exotic && r.freq != Freq::None;
    // a monthly rule with several weekdays and an ordinal is beyond the subset
    if (r.freq == Freq::Monthly && r.set_pos && r.by_day.size() > 1)
        r.understood = false;
    return r;
}

/* The rule as a string, for storage and for the `.ics`. */
inline std::string to_string(const Rule& r) {
    if (r.freq == Freq::None) return {};
    std::string s = "FREQ=";
    switch (r.freq) {
        case Freq::Daily: s += "DAILY"; break;
        case Freq::Weekly: s += "WEEKLY"; break;
        case Freq::Monthly: s += "MONTHLY"; break;
        case Freq::Yearly: s += "YEARLY"; break;
        default: return {};
    }
    if (r.interval > 1) s += ";INTERVAL=" + std::to_string(r.interval);
    if (!r.by_day.empty()) {
        s += ";BYDAY=";
        for (size_t i = 0; i < r.by_day.size(); ++i) {
            if (i) s += ',';
            if (r.set_pos) s += std::to_string(r.set_pos);
            s += weekday_name(r.by_day[i]);
        }
    }
    if (r.by_month_day) s += ";BYMONTHDAY=" + std::to_string(r.by_month_day);
    if (r.count) s += ";COUNT=" + std::to_string(r.count);
    if (!r.until.empty()) {
        Date u;
        if (from_string(r.until, u)) {
            char b[20];
            std::snprintf(b, sizeof b, "%04d%02d%02d", u.y, u.m, u.d);
            s += ";UNTIL=" + std::string(b);
        }
    }
    return s;
}

/* Plain English, for a person deciding whether the rule says what they meant.
 * A rule nobody can read is a rule nobody can check. */
inline std::string describe(const Rule& r) {
    if (r.freq == Freq::None) return "does not repeat";
    static const char* full[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};
    static const char* ord[] = {"", "first", "second", "third", "fourth", "fifth"};
    std::string s;
    const int n = r.interval;
    switch (r.freq) {
        case Freq::Daily:
            s = n == 1 ? "every day" : "every " + std::to_string(n) + " days";
            break;
        case Freq::Weekly: {
            s = n == 1 ? "every week" : "every " + std::to_string(n) + " weeks";
            if (!r.by_day.empty()) {
                s += " on ";
                for (size_t i = 0; i < r.by_day.size(); ++i) {
                    if (i) s += i + 1 == r.by_day.size() ? " and " : ", ";
                    s += full[r.by_day[i]];
                }
            }
            break;
        }
        case Freq::Monthly:
            s = n == 1 ? "every month" : "every " + std::to_string(n) + " months";
            if (r.set_pos && !r.by_day.empty()) {
                s += " on the ";
                s += r.set_pos < 0 ? "last" : ord[std::min(r.set_pos, 5)];
                s += std::string(" ") + full[r.by_day[0]];
            } else if (r.by_month_day) {
                s += " on day " + std::to_string(r.by_month_day);
            }
            break;
        case Freq::Yearly:
            s = n == 1 ? "every year" : "every " + std::to_string(n) + " years";
            break;
        default: return "does not repeat";
    }
    if (r.count) s += ", " + std::to_string(r.count) + " times";
    if (!r.until.empty()) s += ", until " + r.until;
    if (!r.understood)
        s += "  (this rule is kept exactly as written, but it is outside what "
             "this calendar expands, so only the first date is shown)";
    return s;
}

/* Every occurrence of `start` under `r` that falls within [from, to].
 *
 * Bounded twice on purpose — by the window AND by `kMaxOccurrences` — because
 * an unbounded rule is an infinite sequence and a calendar asking for a decade
 * of a daily event should get a large answer rather than a hang. */
inline std::vector<Date> expand(const Date& start, const Rule& r,
                                const Date& from, const Date& to,
                                size_t max_occurrences = 4000) {
    std::vector<Date> out;
    if (to < start) return out;
    if (r.freq == Freq::None || !r.understood) {
        if (from <= start && start <= to) out.push_back(start);
        return out;
    }
    Date until_d{9999, 12, 31};
    if (!r.until.empty()) from_string(r.until, until_d);
    const Date stop = to < until_d ? to : until_d;

    int emitted = 0;
    auto offer = [&](const Date& d) {
        if (d < start || stop < d) return;
        ++emitted;                       // COUNT counts occurrences, not hits
        if (r.count && emitted > r.count) return;
        if (from <= d) out.push_back(d);
    };

    switch (r.freq) {
        case Freq::Daily: {
            Date c = start;
            while (c <= stop && out.size() < max_occurrences) {
                offer(c);
                if (r.count && emitted >= r.count) break;
                add_days(c, r.interval);
            }
            break;
        }
        case Freq::Weekly: {
            std::vector<int> days = r.by_day;
            if (days.empty()) days.push_back(dow(start.y, start.m, start.d));
            std::sort(days.begin(), days.end());
            // walk week by week from the start's own week
            Date week = start;
            add_days(week, -dow(week.y, week.m, week.d)); // back to Sunday
            while (week <= stop && out.size() < max_occurrences) {
                for (int wd : days) {
                    Date c = week;
                    add_days(c, wd);
                    offer(c);
                    if (r.count && emitted >= r.count) break;
                }
                if (r.count && emitted >= r.count) break;
                add_days(week, 7 * r.interval);
            }
            break;
        }
        case Freq::Monthly: {
            Date month{start.y, start.m, 1};
            while ((month.y < stop.y || (month.y == stop.y && month.m <= stop.m)) &&
                   out.size() < max_occurrences) {
                Date c{month.y, month.m, 1};
                bool have = false;
                if (r.set_pos && !r.by_day.empty()) {
                    const int want = r.by_day[0];
                    const int dim = days_in_month(month.y, month.m);
                    if (r.set_pos < 0) { // the LAST such weekday
                        for (int d = dim; d >= 1; --d)
                            if (dow(month.y, month.m, d) == want) {
                                c.d = d; have = true; break;
                            }
                    } else {
                        int seen = 0;
                        for (int d = 1; d <= dim; ++d)
                            if (dow(month.y, month.m, d) == want && ++seen == r.set_pos) {
                                c.d = d; have = true; break;
                            }
                    }
                } else {
                    const int want = r.by_month_day ? r.by_month_day : start.d;
                    const int dim = days_in_month(month.y, month.m);
                    /* A 31st in a 30-day month is SKIPPED, not clamped. RFC
                     * 5545 §3.3.10 is explicit, and it is also the only
                     * defensible reading: "the 31st" in November is not the
                     * 30th, it is nothing. Clamping silently invents a meeting. */
                    if (want >= 1 && want <= dim) { c.d = want; have = true; }
                }
                if (have) {
                    offer(c);
                    if (r.count && emitted >= r.count) break;
                }
                add_months(month, r.interval);
            }
            break;
        }
        case Freq::Yearly: {
            Date c = start;
            while (c <= stop && out.size() < max_occurrences) {
                offer(c);
                if (r.count && emitted >= r.count) break;
                const int ny = c.y + r.interval;
                c = {ny, start.m, std::min(start.d, days_in_month(ny, start.m))};
            }
            break;
        }
        default: break;
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace rrule
} // namespace hormiga
