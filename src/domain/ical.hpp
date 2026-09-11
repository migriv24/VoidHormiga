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
#include <cstdio>
#include <ctime>
#include <string>
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

} // namespace ical
} // namespace hormiga
