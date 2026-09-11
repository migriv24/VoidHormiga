/* domain/ical.hpp — THE PIVOT, as a type and a serializer.
 *
 * ── WHY THIS EXISTS ──────────────────────────────────────────────────────────
 *
 * `okf/concepts/sections/calendar-roadmap.md` is the argument in full; the short
 * version is that Void Hormiga is **the compatible calendar** — a hub that
 * speaks every other calendar's language rather than one that wants to replace
 * them. A hub that speaks N calendar systems is N-squared adapters if they are
 * written pairwise and N if there is a pivot format in the middle, and Void
 * Reyna already wrote down the rule this follows from: *never write a direct
 * A->B adapter when A->pivot->B exists.*
 *
 * **The pivot is the dated rune, and RFC 5545's VEVENT is its interchange
 * serialization.** Google Calendar, Outlook, iCloud, Nextcloud, Radicale, a
 * school district's published feed and a `.ics` somebody emailed are then all
 * *transports* onto one lens, and none of them ever learns what a Hormiga
 * `event` is.
 *
 * ── WHY IT IS A FILE AND NOT A LOOP BODY ─────────────────────────────────────
 *
 * Until now the writer was inline in `render/site.cpp`, inside the loop that
 * builds the web embed's JSON — which is exactly why it could only ever serve
 * one caller. Every transport in the X-track (the importer, the subscription
 * holiday, CalDAV) needs this in both directions, and none of them renders a
 * web page. So it moves here, pure: no ImGui, no HTML, no I/O, `domain/` and
 * therefore reachable from anywhere.
 *
 * ── THE SHAPE IS THE ENFORCEMENT ─────────────────────────────────────────────
 *
 * `Event` carries exactly the fields that may leave the machine, for the same
 * reason `render/published.hpp`'s `Person` has no `notes` member — *"a field
 * that does not exist on the type cannot be leaked by a future caller who
 * forgets, and cannot be added by accident."* CLAUDE.md rule 6 applies harder
 * here than almost anywhere else, because a published calendar feed is a URL
 * that strangers poll: `okf/concepts/sections/calendar.md` names ICE activity as
 * the canonical sensitive case. A caller must fill each field deliberately, and
 * `public_categories` is an ALLOWLIST rather than a denylist — see its comment.
 */
#pragma once

#include "domain/clock.hpp" // parse_clock — the one wall-clock parser

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hormiga {
namespace ical {

/* One calendar entry, as it will appear to a stranger's calendar client.
 *
 * Nothing here is optional-by-accident: an empty field is omitted from the
 * VEVENT, which is the RFC's own way of saying "not stated". */
struct Event {
    /* The stable identity. MUST be built from the rune's frozen `spirit.id`
     * and never from its `name` — Void Core's rune spec is explicit that `id`
     * is "minted once, never reused" and `name` is "editable", so a UID built
     * from the name tells every subscriber that renaming an event DELETED it
     * and created an unrelated new one. */
    std::string uid;
    std::string summary;     // the title, in the language being published
    std::string description; // the summary prose
    std::string location;    // the venue
    std::string url;         // a deep link to the published page
    std::string geo;         // "lat,lon" as stored; emitted as GEO:lat;lon
    std::vector<std::string> categories; // see public_categories()
    std::string date;        // "YYYY-MM-DD"
    std::string start_time;  // as stored, in any human form; "" = all-day
    std::string end_time;
    std::string last_modified; // UTC stamp; "" = omit
    /* The recurrence rule, if this entry repeats — emitted as `RRULE` beside a
     * single `DTSTART`.
     *
     * ONE VEVENT WITH A RULE, NEVER ONE PER OCCURRENCE. `calendar.md`'s
     * standing rule is *recurrence is modeled, not simulated*, and the export
     * is where that stops being an internal nicety: a subscriber who receives
     * twelve copies of a monthly meeting has twelve things to edit and no
     * series, the file is twelve times larger, and re-importing it produces
     * twelve events where there was one. Modelling it makes the round trip
     * exact, which is the whole claim of having a pivot format. */
    std::string rrule;
    long sequence = 0;       // bumped when an entry is revised
    bool cancelled = false;  // STATUS:CANCELLED — a tombstone, see to_vevent
};

struct Options {
    /* X-WR-CALNAME. Not cosmetic for a hub: it is how a person tells four
     * subscribed calendars apart inside their own client, and a feed without
     * one shows up as the URL it came from. */
    std::string name = "Calendar";
    std::string prodid = "-//Void Hormiga//Calendar//EN";
    /* The org timezone, e.g. "America/Los_Angeles". Empty = floating times,
     * which is what shipped before and is wrong the moment a subscriber is in
     * another state. Filling this is X2 and needs a VTIMEZONE to go with it;
     * the field exists now so the seam does not move later. */
    std::string tzid;
    int refresh_minutes = 60;
};

// ── RFC 5545 §3.1: content lines are at most 75 OCTETS ──────────────────────

/* Fold one content line and terminate it.
 *
 * There was no folding at all before 2026-09-10, so a long SUMMARY emitted an
 * over-length line that strict parsers reject outright — Outlook desktop being
 * the one everybody eventually hits.
 *
 * The count is in **octets, not characters**, and a fold must never split a
 * UTF-8 sequence: this application is bilingual by decision (log, 2026-09-02),
 * so every Spanish title carries multi-byte characters and this is the common
 * case rather than a theoretical one. Continuation lines begin with one space,
 * which costs an octet from their budget. */
inline std::string fold(const std::string& line) {
    const size_t kMax = 75;
    if (line.size() <= kMax) return line + "\r\n";
    std::string out;
    size_t i = 0;
    bool first = true;
    while (i < line.size()) {
        size_t budget = first ? kMax : kMax - 1; // the leading space costs one
        size_t take = std::min(budget, line.size() - i);
        // back off the split point while it would land inside a UTF-8
        // sequence (continuation bytes are 10xxxxxx)
        size_t safe = take;
        while (safe > 0 && i + safe < line.size() &&
               ((unsigned char)line[i + safe] & 0xC0) == 0x80)
            --safe;
        if (safe > 0) take = safe; // safe == 0 means one huge malformed run
        if (!first) out += ' ';
        out.append(line, i, take);
        out += "\r\n";
        i += take;
        first = false;
    }
    return out;
}

/* Escape a TEXT value: RFC 5545 §3.3.11 gives `\` `;` `,` and newline meaning
 * inside one, so a venue like "Springfield, OR" silently ends the property and
 * starts a bogus parameter. */
inline std::string escape_text(const std::string& v) {
    std::string o;
    for (char c : v) {
        if (c == '\\' || c == ';' || c == ',') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') continue;
        else o += c;
    }
    return o;
}

/* An instant, as RFC 5545 writes one. UTC, because a stamp is an instant. */
inline std::string now_utc() {
    const std::time_t t = std::time(nullptr);
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char b[32];
    std::snprintf(b, sizeof b, "%04d%02d%02dT%02d%02d%02dZ", g.tm_year + 1900,
                  g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
    return b;
}

/* WHICH TAGS MAY BECOME `CATEGORIES` — AND WHY THIS IS AN ALLOWLIST.
 *
 * Tags map onto CATEGORIES naturally and round-trip through every client, which
 * makes them the most useful thing we can add to a VEVENT. They are also the
 * easiest thing in this codebase to leak, because an organization invents its
 * own namespaces and nothing here can know what they mean.
 *
 * A denylist is therefore unsafe **by construction**: it can only exclude the
 * namespaces that existed when it was written, so the first internal axis an
 * organization invents is published to the world by default. So: only `kw:` —
 * the keyword axis, the one that exists to describe a thing's subject matter,
 * and the one `effect read-flier` proposes into — reaches a feed, with its
 * prefix stripped. `clearance:`, `status:`, `type:` and everything an
 * organization coins stays home.
 *
 * Widening this is an edit to this function, in this file, under this comment —
 * which is the same enforcement-by-shape `published.hpp` uses, and the reason
 * rule 6 says the check must live at the seam rather than in a convention. */
inline std::vector<std::string> public_categories(
    const std::vector<std::string>& tags) {
    std::vector<std::string> out;
    for (const auto& t : tags)
        if (t.rfind("kw:", 0) == 0 && t.size() > 3) out.push_back(t.substr(3));
    return out;
}

/* The day after a date, as YYYYMMDD — an all-day VEVENT's DTEND is EXCLUSIVE
 * (RFC 5545 §3.8.2.2), and without it some clients render a zero-length day. */
inline std::string next_day_compact(int y, int m, int d) {
    std::tm tmv{};
    tmv.tm_year = y - 1900;
    tmv.tm_mon = m - 1;
    tmv.tm_mday = d + 1;
    tmv.tm_hour = 12; // midday, so a DST transition cannot roll the date
    char b[16];
    if (std::mktime(&tmv) != (std::time_t)-1)
        std::snprintf(b, sizeof b, "%04d%02d%02d", tmv.tm_year + 1900,
                      tmv.tm_mon + 1, tmv.tm_mday);
    else
        std::snprintf(b, sizeof b, "%04d%02d%02d", y, m, d);
    return b;
}

/* One VEVENT. Empty when the entry carries no parseable date — a calendar
 * entry without a date is not a calendar entry. */
inline std::string to_vevent(const Event& e, const Options& opt,
                             const std::string& stamp) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(e.date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return "";
    char ymd[16];
    std::snprintf(ymd, sizeof ymd, "%04d%02d%02d", y, m, d);

    std::string o = "BEGIN:VEVENT\r\n";
    o += fold("UID:" + e.uid);
    o += fold("DTSTAMP:" + stamp);
    if (!e.summary.empty()) o += fold("SUMMARY:" + escape_text(e.summary));
    if (!e.description.empty())
        o += fold("DESCRIPTION:" + escape_text(e.description));
    if (!e.location.empty()) o += fold("LOCATION:" + escape_text(e.location));
    if (!e.url.empty()) o += fold("URL:" + e.url); // a URI is not TEXT
    if (!e.geo.empty()) {
        double lat = 0, lon = 0;
        if (std::sscanf(e.geo.c_str(), "%lf , %lf", &lat, &lon) == 2) {
            char g[64];
            std::snprintf(g, sizeof g, "GEO:%.6f;%.6f", lat, lon);
            o += fold(g); // §3.8.1.6 separates with a SEMICOLON, not a comma
        }
    }
    if (!e.categories.empty()) {
        std::string c;
        for (const auto& t : e.categories)
            c += (c.empty() ? "" : ",") + escape_text(t);
        o += fold("CATEGORIES:" + c);
    }

    /* THE TIME. `parse_clock` rather than fixed offsets, because the model
     * stores what the flier said: "3:00 PM", "9 AM", "noon". Slicing
     * `substr(0,2)` out of those produced `DTSTART:20260819T8:0 00` — a colon
     * and a space inside a DTSTART — and threw the meridiem away besides.
     *
     * A `TZID` is emitted when the caller has one. Without it these are
     * FLOATING times, which mean "whatever o'clock it is where the reader is";
     * that is what shipped before and it is X2's job to finish. */
    const std::string tz = opt.tzid.empty() ? "" : ";TZID=" + opt.tzid;
    int sh = 0, sm = 0, eh = 0, em = 0;
    const bool have_s = parse_clock(e.start_time, sh, sm);
    const bool have_e = parse_clock(e.end_time, eh, em);
    if (have_s) {
        char t1[16], t2[16];
        std::snprintf(t1, sizeof t1, "%02d%02d00", sh, sm);
        std::snprintf(t2, sizeof t2, "%02d%02d00", eh, em);
        o += fold("DTSTART" + tz + ":" + ymd + "T" + t1);
        o += fold("DTEND" + tz + ":" + ymd + "T" + (have_e ? t2 : t1));
    } else {
        o += fold("DTSTART;VALUE=DATE:" + std::string(ymd));
        o += fold("DTEND;VALUE=DATE:" + next_day_compact(y, m, d));
    }

    if (!e.rrule.empty()) o += fold("RRULE:" + e.rrule);
    if (!e.last_modified.empty())
        o += fold("LAST-MODIFIED:" + e.last_modified);
    /* SEQUENCE says "this is a revision of the entry you already have" rather
     * than "here is a new entry", which is the difference between a subscriber
     * seeing an edit and seeing a duplicate. */
    if (e.sequence > 0) o += fold("SEQUENCE:" + std::to_string(e.sequence));
    /* A CANCELLED event must be published as a TOMBSTONE, not dropped. An entry
     * that simply stops appearing in a feed stays on every subscriber's
     * calendar forever — the client has no way to distinguish "cancelled" from
     * "the feed was filtered" or "the server was down". */
    o += fold(std::string("STATUS:") + (e.cancelled ? "CANCELLED" : "CONFIRMED"));
    o += "END:VEVENT\r\n";
    return o;
}

/* A whole VCALENDAR. */
inline std::string to_vcalendar(const std::vector<Event>& events,
                                const Options& opt, const std::string& stamp) {
    std::string body;
    for (const auto& e : events) body += to_vevent(e, opt, stamp);
    if (body.empty()) return "";
    std::string o = "BEGIN:VCALENDAR\r\n";
    o += fold("VERSION:2.0");
    o += fold("PRODID:" + opt.prodid);
    o += fold("CALSCALE:GREGORIAN");
    o += fold("METHOD:PUBLISH");
    o += fold("X-WR-CALNAME:" + escape_text(opt.name));
    if (!opt.tzid.empty()) o += fold("X-WR-TIMEZONE:" + opt.tzid);
    /* How often a subscriber should come back. Both spellings on purpose:
     * Outlook reads the `X-` one, everything else reads the standard one, and
     * emitting both costs a line. Without either, clients fall back to their
     * own default — often once a day — and a feed that updates hourly looks
     * stale to the people subscribed to it. */
    if (opt.refresh_minutes > 0) {
        const std::string dur = "PT" + std::to_string(opt.refresh_minutes) + "M";
        o += fold("REFRESH-INTERVAL;VALUE=DURATION:" + dur);
        o += fold("X-PUBLISHED-TTL:" + dur);
    }
    o += body;
    o += "END:VCALENDAR\r\n";
    return o;
}

// ════════════════════════════════════════════════════════════════════════════
//  THE READER HALF (X3) — parse any calendar, forgivingly
// ════════════════════════════════════════════════════════════════════════════
//
// THE ONE RULE THIS PARSER IS BUILT AROUND: **never reject a file for
// containing something you do not model.** A hub that refuses a `.ics` because
// it holds a VTODO, an unknown `X-` property or a parameter it has not seen is
// behaving exactly like the vendors the author defined this section against.
// Real feeds are full of things we have no use for; the correct response to all
// of them is to walk past.
//
// So every unknown component is skipped whole, every unknown property is
// ignored, and a malformed line ends that line and nothing else. The only thing
// that can make an entry fail is having no usable DTSTART — an entry with no
// date is not a calendar entry.

/* What the source said, before any of it becomes a command. `Event` is what we
 * PUBLISH; this is what we READ, and they are deliberately different types:
 * an imported entry carries things we keep but never emit (the foreign UID's
 * source, the recurrence rule we cannot yet expand) and lacks things only a
 * publisher decides. Collapsing them would make the import path able to reach
 * fields the export seam is supposed to gate. */
struct Incoming {
    std::string uid;         // the SOURCE's UID — the identity we match on
    std::string summary;
    std::string description;
    std::string location;
    std::string url;
    std::string geo;         // "lat,lon"
    std::vector<std::string> categories;
    std::string date;        // "YYYY-MM-DD"
    std::string start_time;  // "HH:MM", "" = all-day
    std::string end_time;
    std::string end_date;    // set when the entry spans days (C3b)
    std::string rrule;       // verbatim; expansion is C3a
    std::string tzid;        // the zone the source named, "" = floating/UTC
    bool utc = false;        // the time arrived as a Z instant
    bool cancelled = false;
    bool all_day = false;
};

struct ParseReport {
    std::vector<Incoming> events;
    int skipped_components = 0; // VTODO, VJOURNAL, VFREEBUSY, VALARM, …
    int skipped_no_date = 0;    // entries with no usable DTSTART
    int recurring = 0;          // carried an RRULE we stored but did not expand
    int zoned = 0;              // carried a TZID or a Z instant
    std::string calendar_name;  // X-WR-CALNAME, if the source named itself
    std::string prodid;
};

/* Undo `escape_text`. A parser that does not unescape turns "Springfield\, OR"
 * into a venue with a backslash in it, which is the kind of defect that
 * survives for months because it still looks almost right. */
inline std::string unescape_text(const std::string& v) {
    std::string o;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] != '\\' || i + 1 >= v.size()) { o += v[i]; continue; }
        const char n = v[++i];
        if (n == 'n' || n == 'N') o += '\n';
        else o += n; // \\ \; \, and anything else: the character itself
    }
    return o;
}

/* Unfold: RFC 5545 §3.1 says a line beginning with a space or a tab continues
 * the previous one. Accepts CRLF, bare LF and bare CR, because a file that
 * reached us through a mail client or a text editor may carry any of them and
 * refusing over a line ending would be the opposite of the point. */
inline std::vector<std::string> unfold(std::string_view text) {
    std::vector<std::string> raw;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\r' || c == '\n') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            raw.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) raw.push_back(cur);

    std::vector<std::string> out;
    for (auto& line : raw) {
        if (!out.empty() && !line.empty() && (line[0] == ' ' || line[0] == '\t'))
            out.back() += line.substr(1);
        else
            out.push_back(line);
    }
    return out;
}

/* One content line split into name, parameters and value.
 *
 * The colon that ends the property is the first one NOT inside a quoted
 * parameter value — `ATTENDEE;CN="Ruiz, Ana":mailto:…` has three colons and
 * only the second ends the parameters. Splitting on the first colon is the
 * classic way to mangle exactly the lines that carry punctuation. */
struct Line {
    std::string name;                                         // upper-cased
    std::vector<std::pair<std::string, std::string>> params;  // names upper-cased
    std::string value;                                        // still escaped

    std::string param(std::string_view key) const {
        for (const auto& p : params)
            if (p.first == key) return p.second;
        return {};
    }
};

inline Line split_line(const std::string& raw) {
    Line L;
    bool quoted = false;
    size_t colon = std::string::npos;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '"') quoted = !quoted;
        else if (raw[i] == ':' && !quoted) { colon = i; break; }
    }
    const std::string head = colon == std::string::npos ? raw : raw.substr(0, colon);
    L.value = colon == std::string::npos ? std::string() : raw.substr(colon + 1);

    // the head is NAME;p=v;p="v;with;semicolons"
    std::vector<std::string> parts;
    {
        std::string cur;
        bool q = false;
        for (char c : head) {
            if (c == '"') { q = !q; cur += c; }
            else if (c == ';' && !q) { parts.push_back(cur); cur.clear(); }
            else cur += c;
        }
        parts.push_back(cur);
    }
    if (parts.empty()) return L;
    L.name = parts[0];
    for (char& c : L.name) c = (char)std::toupper((unsigned char)c);
    for (size_t i = 1; i < parts.size(); ++i) {
        const size_t eq = parts[i].find('=');
        if (eq == std::string::npos) continue;
        std::string k = parts[i].substr(0, eq), v = parts[i].substr(eq + 1);
        for (char& c : k) c = (char)std::toupper((unsigned char)c);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        L.params.emplace_back(std::move(k), std::move(v));
    }
    return L;
}

/* An ISO 8601 duration, as `DURATION` writes one: `PT1H30M`, `P2D`, `P1W`.
 * Returns minutes; 0 when it cannot be read.
 *
 * This exists because DTEND is OPTIONAL. An entry may carry a DURATION instead,
 * and Google Calendar in particular emits them — an importer without this reads
 * every such event as ending when it starts. */
inline long duration_minutes(const std::string& v) {
    long total = 0, n = 0;
    bool time_part = false, any = false;
    for (char c : v) {
        if (std::isdigit((unsigned char)c)) { n = n * 10 + (c - '0'); any = true; continue; }
        switch (std::toupper((unsigned char)c)) {
            case 'P': break;
            case 'T': time_part = true; break;
            case 'W': total += n * 7 * 24 * 60; n = 0; break;
            case 'D': total += n * 24 * 60; n = 0; break;
            case 'H': total += n * 60; n = 0; break;
            case 'M': total += time_part ? n : n * 30 * 24 * 60; n = 0; break;
            case 'S': n = 0; break; // seconds do not survive an HH:MM model
            default: break;
        }
    }
    return any ? total : 0;
}

/* A DATE-TIME value → civil date and wall-clock minutes.
 *
 * `20260915`            an all-day DATE
 * `20260915T150000`     floating — whatever o'clock it is where the reader is
 * `20260915T220000Z`    a UTC instant
 *
 * A trailing Z is converted to THIS MACHINE's local time, which is the honest
 * v1 of Q68's lean — *read any zone, normalize on import, author in one*. It is
 * recorded in the report rather than done quietly, because it is the one step
 * of an import that can move an event by hours. A named TZID is kept as a label
 * and its offset is NOT applied: pretending to know the rules of an arbitrary
 * zone without a tzdb would be worse than saying we read it as written. */
inline bool parse_datetime(const std::string& raw, bool& all_day, std::string& date,
                           int& minutes, bool& utc) {
    std::string v;
    for (char c : raw)
        if (!std::isspace((unsigned char)c)) v += c;
    if (v.size() < 8) return false;
    int y = 0, mo = 0, d = 0;
    if (std::sscanf(v.c_str(), "%4d%2d%2d", &y, &mo, &d) != 3) return false;
    if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
    utc = !v.empty() && (v.back() == 'Z' || v.back() == 'z');
    all_day = v.size() < 9 || (v[8] != 'T' && v[8] != 't');
    minutes = 0;
    if (!all_day) {
        int hh = 0, mi = 0, ss = 0;
        if (std::sscanf(v.c_str() + 9, "%2d%2d%2d", &hh, &mi, &ss) < 2) return false;
        if (hh > 23 || mi > 59) return false;
        minutes = hh * 60 + mi;
        if (utc) {
            /* UTC → this machine's local wall clock, through the C library so
             * the DST rules are the system's rather than ours. */
            std::tm g{};
            g.tm_year = y - 1900; g.tm_mon = mo - 1; g.tm_mday = d;
            g.tm_hour = hh; g.tm_min = mi; g.tm_sec = 0; g.tm_isdst = -1;
#ifdef _WIN32
            const std::time_t t = _mkgmtime(&g);
#else
            const std::time_t t = timegm(&g);
#endif
            if (t != (std::time_t)-1) {
                std::tm l{};
#ifdef _WIN32
                localtime_s(&l, &t);
#else
                localtime_r(&t, &l);
#endif
                y = l.tm_year + 1900; mo = l.tm_mon + 1; d = l.tm_mday;
                minutes = l.tm_hour * 60 + l.tm_min;
            }
        }
    }
    char b[16];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d", y, mo, d);
    date = b;
    return true;
}

inline std::string hhmm(int minutes) {
    char b[8];
    std::snprintf(b, sizeof b, "%02d:%02d", (minutes / 60) % 24, minutes % 60);
    return b;
}

/* Add days to a "YYYY-MM-DD", through the C library's civil calendar. */
inline std::string add_days(const std::string& date, int delta) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return date;
    std::tm t{};
    t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d + delta;
    t.tm_hour = 12; // midday, so a DST transition cannot roll the date
    if (std::mktime(&t) == (std::time_t)-1) return date;
    char b[16];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1,
                  t.tm_mday);
    return b;
}

/* Parse a whole iCalendar document.
 *
 * Component nesting is tracked rather than assumed, which matters for exactly
 * one reason and it is not pedantry: **a VALARM lives INSIDE a VEVENT** and
 * carries its own DESCRIPTION and TRIGGER. A parser that keys on property names
 * without knowing which component it is in will happily overwrite a meeting's
 * description with the text of its reminder. */
inline ParseReport parse(std::string_view text) {
    ParseReport rep;
    std::vector<std::string> stack;
    Incoming cur;
    bool in_event = false;
    // what DTEND/DURATION resolved to, held until the entry closes
    std::string end_date;
    int end_minutes = -1;
    long dur_minutes = 0;
    int start_minutes = -1;

    for (const auto& raw : unfold(text)) {
        if (raw.empty()) continue;
        const Line L = split_line(raw);

        if (L.name == "BEGIN") {
            std::string comp = L.value;
            for (char& c : comp) c = (char)std::toupper((unsigned char)c);
            stack.push_back(comp);
            if (comp == "VEVENT" && stack.size() == 2) {
                in_event = true;
                cur = Incoming{};
                end_date.clear();
                end_minutes = -1;
                start_minutes = -1;
                dur_minutes = 0;
            } else if (comp != "VCALENDAR") {
                // VTODO, VJOURNAL, VFREEBUSY, VTIMEZONE, VALARM, X-anything:
                // counted and walked past, never a reason to fail
                if (!(comp == "VEVENT")) ++rep.skipped_components;
            }
            continue;
        }
        if (L.name == "END") {
            std::string comp = L.value;
            for (char& c : comp) c = (char)std::toupper((unsigned char)c);
            if (comp == "VEVENT" && in_event) {
                in_event = false;
                if (cur.date.empty()) {
                    ++rep.skipped_no_date;
                } else {
                    if (start_minutes >= 0) {
                        cur.start_time = hhmm(start_minutes);
                        if (end_minutes >= 0) cur.end_time = hhmm(end_minutes);
                        else if (dur_minutes > 0)
                            cur.end_time = hhmm(start_minutes + (int)dur_minutes);
                    }
                    /* An all-day DTEND is EXCLUSIVE, so a one-day event ends
                     * the NEXT morning. Subtracting the day back off is what
                     * stops every imported all-day entry from looking like it
                     * spans two. */
                    if (cur.all_day && !end_date.empty() && end_date != cur.date)
                        end_date = add_days(end_date, -1);
                    if (!end_date.empty() && end_date != cur.date)
                        cur.end_date = end_date;
                    if (!cur.rrule.empty()) ++rep.recurring;
                    if (!cur.tzid.empty() || cur.utc) ++rep.zoned;
                    rep.events.push_back(cur);
                }
            }
            if (!stack.empty()) stack.pop_back();
            continue;
        }

        // properties of the calendar itself
        if (!in_event) {
            if (stack.size() == 1 && stack[0] == "VCALENDAR") {
                if (L.name == "X-WR-CALNAME") rep.calendar_name = unescape_text(L.value);
                else if (L.name == "PRODID") rep.prodid = unescape_text(L.value);
            }
            continue;
        }
        // inside a VEVENT but nested deeper (a VALARM): not ours
        if (stack.size() > 2) continue;

        if (L.name == "UID") cur.uid = unescape_text(L.value);
        else if (L.name == "SUMMARY") cur.summary = unescape_text(L.value);
        else if (L.name == "DESCRIPTION") cur.description = unescape_text(L.value);
        else if (L.name == "LOCATION") cur.location = unescape_text(L.value);
        else if (L.name == "URL") cur.url = L.value;
        else if (L.name == "RRULE") cur.rrule = L.value;
        else if (L.name == "STATUS") {
            std::string s = L.value;
            for (char& c : s) c = (char)std::toupper((unsigned char)c);
            cur.cancelled = s == "CANCELLED";
        } else if (L.name == "GEO") {
            // §3.8.1.6 separates with a SEMICOLON; we store a comma
            std::string g = L.value;
            for (char& c : g)
                if (c == ';') c = ',';
            double la = 0, lo = 0;
            if (std::sscanf(g.c_str(), "%lf , %lf", &la, &lo) == 2) cur.geo = g;
        } else if (L.name == "CATEGORIES") {
            std::string item;
            bool esc = false;
            for (char c : L.value) {
                if (esc) { item += c; esc = false; continue; }
                if (c == '\\') { esc = true; continue; }
                if (c == ',') {
                    if (!item.empty()) cur.categories.push_back(item);
                    item.clear();
                    continue;
                }
                item += c;
            }
            if (!item.empty()) cur.categories.push_back(item);
        } else if (L.name == "DTSTART" || L.name == "DTEND") {
            bool ad = false, utc = false;
            std::string dt;
            int mins = 0;
            if (!parse_datetime(L.value, ad, dt, mins, utc)) continue;
            if (L.param("VALUE") == "DATE") ad = true;
            if (L.name == "DTSTART") {
                cur.date = dt;
                cur.all_day = ad;
                cur.utc = utc;
                cur.tzid = L.param("TZID");
                start_minutes = ad ? -1 : mins;
            } else {
                end_date = dt;
                end_minutes = ad ? -1 : mins;
            }
        } else if (L.name == "DURATION") {
            dur_minutes = duration_minutes(L.value);
        }
        // everything else — ORGANIZER, ATTENDEE, CLASS, TRANSP, SEQUENCE,
        // RECURRENCE-ID, EXDATE, every X- property — is walked past on purpose
    }
    return rep;
}

} // namespace ical
} // namespace hormiga
