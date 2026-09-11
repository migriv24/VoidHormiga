/* calendar_smoke.cpp — the Calendar's two pure grammars, pinned where they can
 * be checked without a window.
 *
 * 1. THE SHARED TIME PARSER. The reason this file exists. Until 2026-09-10 the
 *    calendar grid parsed times with `sscanf("%d:%d")` while the renderers and
 *    the `.ics` export used `parse_clock`, so the two disagreed about what time
 *    an event was — and `render/text.hpp` says out loud that the disagreement
 *    was on the COMMON case: *"Every time in a real community database is
 *    12-hour with a meridiem, because that is what a flier prints."* A 3 PM
 *    meeting drew at 3 AM and a "9 AM" one fell off the time grid entirely.
 *    These cases are the regression guard: what the export believes and what
 *    the screen believes are now one function, and this proves it for the
 *    shapes that actually broke.
 *
 * 2. THE QUICK-ADD GRAMMAR (C1f). A one-line creation gesture is only worth
 *    binding to the Enter key if it is PREDICTABLE, so the cases that matter
 *    most here are the negative ones: "Ward 5 meeting" must not become an event
 *    at five o'clock, and "Route 66 cleanup" must not become anything at all.
 *    `parse_clock` on its own is far too permissive to drive a creation gesture
 *    — it exists to read fliers — and `clock_token`'s strict gate is what makes
 *    the difference. That gate is the thing under test.
 *
 * Links no renderer, no view, no Void Core. Both grammars are pure by
 * construction and this file is the evidence.
 */
#include "../src/domain/quick_add.hpp"

#include <iostream>
#include <string>

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

int main() {
    // ── 1. the time parser, on the shapes that were silently wrong ──────────
    CHECK(near(hours("15:00"), 15.0f));   // what the grid always handled
    CHECK(near(hours("09:30"), 9.5f));
    // the meridiem, which the grid's old sscanf discarded: 3 PM was drawn at 3 AM
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

    // ── 2. quick-add: the shapes a person types ─────────────────────────────
    hormiga::quick::Parsed q;

    CHECK(hormiga::quick::parse("Food drive 3pm-5pm", q));
    CHECK(q.glyph == "event");
    CHECK(q.name == "Food drive");
    CHECK(near(q.t0, 15.0f) && near(q.t1, 17.0f));

    CHECK(hormiga::quick::parse("Standup 9am", q));
    CHECK(q.name == "Standup");
    CHECK(near(q.t0, 9.0f) && near(q.t1, 10.0f)); // a start alone means an hour

    CHECK(hormiga::quick::parse("Board meeting 15:00-17:30", q));
    CHECK(q.name == "Board meeting");
    CHECK(near(q.t0, 15.0f) && near(q.t1, 17.5f));

    CHECK(hormiga::quick::parse("Retreat 3 pm to 5 pm", q));
    CHECK(q.name == "Retreat");
    CHECK(near(q.t0, 15.0f) && near(q.t1, 17.0f));

    CHECK(hormiga::quick::parse("Volunteer training", q)); // all-day
    CHECK(q.name == "Volunteer training");
    CHECK(q.t0 < 0 && q.t1 < 0);

    // the `!` prefix keeps the two kinds visibly distinct at the point of entry
    CHECK(hormiga::quick::parse("!Road closure 2pm", q));
    CHECK(q.glyph == "incident");
    CHECK(q.name == "Road closure");
    CHECK(near(q.t0, 14.0f));

    // ── 3. quick-add: what must NOT be read as a time ───────────────────────
    // These are the cases that would make the feature a trap. A bare integer is
    // part of a title far more often than it is a clock.
    CHECK(hormiga::quick::parse("Ward 5 meeting", q));
    CHECK(q.name == "Ward 5 meeting" && q.t0 < 0);

    CHECK(hormiga::quick::parse("Route 66 cleanup", q));
    CHECK(q.name == "Route 66 cleanup" && q.t0 < 0);

    CHECK(hormiga::quick::parse("Cleanup crew 12", q));
    CHECK(q.name == "Cleanup crew 12" && q.t0 < 0); // "12" alone is not 12:00

    // a line that is ONLY a time has nothing to name, and minting `event-7`
    // from it is exactly the un-named rune quick-add exists to stop producing
    CHECK(!hormiga::quick::parse("3pm", q));
    CHECK(!hormiga::quick::parse("", q));
    CHECK(!hormiga::quick::parse("   ", q));
    CHECK(!hormiga::quick::parse("!", q));

    // ── 4. the slug: what the rune gets called ──────────────────────────────
    CHECK(hormiga::quick::slug("Food drive") == "food-drive");
    CHECK(hormiga::quick::slug("Know Your Rights!") == "know-your-rights");
    CHECK(hormiga::quick::slug("  spaced  out  ") == "spaced-out");
    CHECK(hormiga::quick::slug("!!!").empty()); // nothing nameable survives

    if (failures == 0) std::cout << "calendar smoke: ok\n";
    else std::cerr << "calendar smoke: " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
