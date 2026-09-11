/* calendar_smoke.cpp — the Calendar's pure grammars, pinned where they can be
 * checked without a window.
 *
 * 1. THE SHARED TIME PARSER. The reason this file exists. Until 2026-09-10 the
 *    calendar grid parsed times with `sscanf("%d:%d")` while the renderers and
 *    the `.ics` export used `parse_clock`, so the two disagreed about what time
 *    an event was — and `render/text.hpp` says out loud that the disagreement
 *    was on the COMMON case: *"Every time in a real community database is
 *    12-hour with a meridiem, because that is what a flier prints."* A 3 PM
 *    meeting drew at 3 AM and a "9 AM" one fell off the time grid entirely.
 *
 * 2. THE iCALENDAR LENS (X0/X1). The four conformance defects found by reading
 *    the writer on 2026-09-10 — no line folding, a UID built from the editable
 *    rune name, a two-property header, floating times — and the one that
 *    matters most for a feed strangers poll: `public_categories` is an
 *    ALLOWLIST, so the first internal namespace an organization invents is not
 *    published to the world by default.
 *
 * 3. THE QUICK-ADD GRAMMAR (C1f). A one-line creation gesture is only worth
 *    binding to the Enter key if it is PREDICTABLE, so the cases that matter
 *    most are the negative ones: "Ward 5 meeting" must not become an event at
 *    five o'clock. `parse_clock` on its own is far too permissive to drive a
 *    creation gesture — it exists to read fliers — and `clock_token`'s strict
 *    gate is what makes the difference. That gate is the thing under test.
 *
 * Links no renderer, no view, no Void Core. All three are pure by construction
 * and this file is the evidence.
 */
#include "../src/domain/ical.hpp"
#include "../src/domain/quick_add.hpp"

#include <iostream>
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

/* hours-as-float, or -1 — the shape `cal_parse_hhmm` hands the grid. */
static float hours(const std::string& s) {
    int h = 0, m = 0;
    if (!parse_clock(s, h, m)) return -1.0f;
    return h + m / 60.0f;
}

static bool near(float a, float b) { return a > b - 0.001f && a < b + 0.001f; }

/* Walk a folded property, checking every physical line and rebuilding the
 * logical one. Unfolding is the inverse the RFC defines, so a fold that does
 * not survive it is not a fold. */
static bool unfold_ok(const std::string& f, std::string& logical,
                      size_t& physical_lines) {
    logical.clear();
    physical_lines = 0;
    size_t pos = 0;
    bool ok = true;
    while (pos < f.size()) {
        const size_t nl = f.find("\r\n", pos);
        if (nl == std::string::npos) return false; // every line must terminate
        std::string line = f.substr(pos, nl - pos);
        if (line.size() > 75) ok = false;          // §3.1: 75 OCTETS
        if (physical_lines > 0) {
            if (line.empty() || line[0] != ' ') ok = false; // continuation
            else line.erase(0, 1);
        }
        // a physical line must never BEGIN with a UTF-8 continuation byte —
        // that is exactly what a mid-sequence split looks like
        if (!line.empty() && ((unsigned char)line[0] & 0xC0) == 0x80) ok = false;
        logical += line;
        pos = nl + 2;
        ++physical_lines;
    }
    return ok;
}

int main() {
    using namespace hormiga;

    // ── 1. the time parser, on the shapes that were silently wrong ──────────
    CHECK(near(hours("15:00"), 15.0f)); // what the grid always handled
    CHECK(near(hours("09:30"), 9.5f));
    // the meridiem, which the grid's old sscanf discarded: 3 PM drew at 3 AM
    CHECK(near(hours("3:00 PM"), 15.0f));
    CHECK(near(hours("3:00 p.m."), 15.0f));
    CHECK(near(hours("7:45 pm"), 19.75f));
    // no colon at all: the old parser failed and the entry became "all-day",
    // vanishing off the time grid into the chip lane
    CHECK(near(hours("9 AM"), 9.0f));
    CHECK(near(hours("9am"), 9.0f));
    CHECK(near(hours("noon"), 12.0f));
    CHECK(near(hours("midnight"), 0.0f));
    CHECK(near(hours("12:30 AM"), 0.5f)); // 12:30 AM is 00:30, not 12:30
    CHECK(near(hours("12:30 PM"), 12.5f));
    CHECK(hours("") < 0);                 // genuinely all-day; not an error

    // ── 2. the iCalendar lens ───────────────────────────────────────────────

    // FOLDING (§3.1). There was none, so a long SUMMARY emitted an over-length
    // line that strict parsers — Outlook desktop first — reject outright.
    CHECK(ical::fold("SHORT:ok") == "SHORT:ok\r\n");
    {
        const std::string longv(200, 'x');
        std::string logical;
        size_t lines = 0;
        CHECK(unfold_ok(ical::fold("SUMMARY:" + longv), logical, lines));
        CHECK(lines > 1);                        // it actually folded
        CHECK(logical == "SUMMARY:" + longv);    // and survives unfolding
    }
    {
        /* A fold must not split a UTF-8 sequence. This application is bilingual
         * by decision, so a Spanish title makes this the common case rather
         * than a theoretical one: "Reunión" is 8 octets, not 7. */
        std::string acc;
        for (int i = 0; i < 40; ++i) acc += "Reuni\xc3\xb3n ";
        std::string logical;
        size_t lines = 0;
        CHECK(unfold_ok(ical::fold("SUMMARY:" + acc), logical, lines));
        CHECK(lines > 1);
        CHECK(logical == "SUMMARY:" + acc);
    }

    // ESCAPING (§3.3.11): "Springfield, OR" must not start a bogus parameter
    CHECK(ical::escape_text("Springfield, OR") == "Springfield\\, OR");
    CHECK(ical::escape_text("a;b") == "a\\;b");
    CHECK(ical::escape_text("line\nbreak") == "line\\nbreak");

    /* CATEGORIES IS AN ALLOWLIST. A denylist can only exclude the namespaces
     * that existed when it was written, so the first internal axis an
     * organization invents would be published to the world by default. */
    {
        const std::vector<std::string> tags = {
            "kw:food", "kw:mutual-aid", "clearance:public", "status:draft",
            "type:event", "internal:do-not-share", "web-hide"};
        const auto cats = ical::public_categories(tags);
        CHECK(cats.size() == 2);
        CHECK(cats.size() == 2 && cats[0] == "food" && cats[1] == "mutual-aid");
    }

    // A WHOLE VCALENDAR: the header properties that were missing entirely
    {
        ical::Event e;
        e.uid = "01H8XYZ@voidhormiga"; // the FROZEN id, never the editable name
        e.summary = "Food drive";
        e.location = "Springfield, OR";
        e.date = "2026-09-15";
        e.start_time = "3:00 PM"; // the flier form the old writer mangled
        e.end_time = "5:00 PM";
        e.geo = "44.0462,-123.0220";
        e.categories = {"food"};
        ical::Options opt;
        opt.name = "Public Events";
        const std::string cal = ical::to_vcalendar({e}, opt, "20260910T120000Z");

        CHECK(cal.rfind("BEGIN:VCALENDAR\r\n", 0) == 0);
        CHECK(cal.find("CALSCALE:GREGORIAN\r\n") != std::string::npos);
        CHECK(cal.find("METHOD:PUBLISH\r\n") != std::string::npos);
        CHECK(cal.find("X-WR-CALNAME:Public Events\r\n") != std::string::npos);
        CHECK(cal.find("REFRESH-INTERVAL;VALUE=DURATION:PT60M") != std::string::npos);
        CHECK(cal.find("X-PUBLISHED-TTL:PT60M") != std::string::npos);
        CHECK(cal.find("UID:01H8XYZ@voidhormiga\r\n") != std::string::npos);
        CHECK(cal.find("DTSTAMP:20260910T120000Z\r\n") != std::string::npos);
        // 3:00 PM is 15:00, not 03:00 — the whole point of one time parser
        CHECK(cal.find("DTSTART:20260915T150000\r\n") != std::string::npos);
        CHECK(cal.find("DTEND:20260915T170000\r\n") != std::string::npos);
        CHECK(cal.find("LOCATION:Springfield\\, OR\r\n") != std::string::npos);
        // §3.8.1.6 separates the pair with a SEMICOLON, not a comma
        CHECK(cal.find("GEO:44.046200;-123.022000\r\n") != std::string::npos);
        CHECK(cal.find("CATEGORIES:food\r\n") != std::string::npos);
        CHECK(cal.find("STATUS:CONFIRMED\r\n") != std::string::npos);
        CHECK(cal.find("END:VCALENDAR\r\n") != std::string::npos);
    }
    {
        /* ALL-DAY: DTEND is EXCLUSIVE (§3.8.2.2) — without it some clients
         * render a zero-length day. Month rollover is the case that breaks. */
        ical::Event e;
        e.uid = "u@x";
        e.summary = "Volunteer training";
        e.date = "2026-09-30";
        const std::string cal = ical::to_vcalendar({e}, ical::Options{}, "S");
        CHECK(cal.find("DTSTART;VALUE=DATE:20260930\r\n") != std::string::npos);
        CHECK(cal.find("DTEND;VALUE=DATE:20261001\r\n") != std::string::npos);
    }
    {
        /* A CANCELLED entry is a TOMBSTONE, not an omission: one that simply
         * stops appearing in a feed stays on every subscriber's calendar
         * forever, because the client cannot tell "cancelled" from "filtered". */
        ical::Event e;
        e.uid = "u@x";
        e.date = "2026-09-15";
        e.cancelled = true;
        e.sequence = 2;
        const std::string cal = ical::to_vcalendar({e}, ical::Options{}, "S");
        CHECK(cal.find("STATUS:CANCELLED\r\n") != std::string::npos);
        CHECK(cal.find("SEQUENCE:2\r\n") != std::string::npos);
    }
    {
        // a TZID, once X2 fills it in, must reach both DTSTART and DTEND
        ical::Event e;
        e.uid = "u@x";
        e.date = "2026-09-15";
        e.start_time = "09:00";
        ical::Options opt;
        opt.tzid = "America/Los_Angeles";
        const std::string cal = ical::to_vcalendar({e}, opt, "S");
        CHECK(cal.find("X-WR-TIMEZONE:America/Los_Angeles") != std::string::npos);
        CHECK(cal.find("DTSTART;TZID=America/Los_Angeles:20260915T090000") !=
              std::string::npos);
        CHECK(cal.find("DTEND;TZID=America/Los_Angeles:20260915T090000") !=
              std::string::npos);
    }
    // an entry with no parseable date is not a calendar entry
    CHECK(ical::to_vcalendar({ical::Event{}}, ical::Options{}, "S").empty());

    // ── 3. quick-add: the shapes a person types ─────────────────────────────
    quick::Parsed q;

    CHECK(quick::parse("Food drive 3pm-5pm", q));
    CHECK(q.glyph == "event");
    CHECK(q.name == "Food drive");
    CHECK(near(q.t0, 15.0f) && near(q.t1, 17.0f));

    CHECK(quick::parse("Standup 9am", q));
    CHECK(q.name == "Standup");
    CHECK(near(q.t0, 9.0f) && near(q.t1, 10.0f)); // a start alone means an hour

    CHECK(quick::parse("Board meeting 15:00-17:30", q));
    CHECK(q.name == "Board meeting");
    CHECK(near(q.t0, 15.0f) && near(q.t1, 17.5f));

    CHECK(quick::parse("Retreat 3 pm to 5 pm", q));
    CHECK(q.name == "Retreat");
    CHECK(near(q.t0, 15.0f) && near(q.t1, 17.0f));

    CHECK(quick::parse("Volunteer training", q)); // all-day
    CHECK(q.name == "Volunteer training");
    CHECK(q.t0 < 0 && q.t1 < 0);

    // the `!` prefix keeps the two kinds visibly distinct at the point of entry
    CHECK(quick::parse("!Road closure 2pm", q));
    CHECK(q.glyph == "incident");
    CHECK(q.name == "Road closure");
    CHECK(near(q.t0, 14.0f));

    // ── 4. quick-add: what must NOT be read as a time ───────────────────────
    // These are the cases that would make the feature a trap. A bare integer is
    // part of a title far more often than it is a clock.
    CHECK(quick::parse("Ward 5 meeting", q));
    CHECK(q.name == "Ward 5 meeting" && q.t0 < 0);

    CHECK(quick::parse("Route 66 cleanup", q));
    CHECK(q.name == "Route 66 cleanup" && q.t0 < 0);

    CHECK(quick::parse("Cleanup crew 12", q));
    CHECK(q.name == "Cleanup crew 12" && q.t0 < 0); // "12" alone is not 12:00

    // a line that is ONLY a time has nothing to name, and minting `event-7`
    // from it is exactly the un-named rune quick-add exists to stop producing
    CHECK(!quick::parse("3pm", q));
    CHECK(!quick::parse("", q));
    CHECK(!quick::parse("   ", q));
    CHECK(!quick::parse("!", q));

    // ── 5. the slug: what the rune gets called ──────────────────────────────
    CHECK(quick::slug("Food drive") == "food-drive");
    CHECK(quick::slug("Know Your Rights!") == "know-your-rights");
    CHECK(quick::slug("  spaced  out  ") == "spaced-out");
    CHECK(quick::slug("!!!").empty()); // nothing nameable survives

    if (failures == 0) std::cout << "calendar smoke: ok\n";
    else std::cerr << "calendar smoke: " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
