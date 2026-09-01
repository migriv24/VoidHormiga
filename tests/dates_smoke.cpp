/* dates_smoke.cpp — the three things the 2026-08-28 field report asked for,
 * pinned where they can be checked without a window or a website.
 *
 * 1. THE DATE PREDICATES. A scene is built with today held fixed, so "is this
 *    in the past" is a deterministic question rather than one whose answer
 *    depends on when the suite runs — which is the only way a date rule can be
 *    tested at all. The cases that matter are the ones the report warned about:
 *    a recurring event must not be swept into `date:past`, a flier must reach
 *    its event through the edge, and a rune with neither must match neither.
 *
 * 2. THE VIDEO URL PARSER. Every shape a volunteer can produce, plus the ones
 *    that must NOT parse — because a parser that accepts almost anything is how
 *    arbitrary text reaches an attribute on a public page.
 *
 * 3. THE FLIER READER. The mismatch line is the whole feature: a sheet printing
 *    one date while its event stores another is what put an August 19 flier on
 *    an August 28 home page.
 *
 * Links no renderer and no view. All three are pure by construction, and this
 * file is the evidence for that claim.
 */
#include "../src/domain/date_query.hpp"
#include "../src/domain/flier_read.hpp"
#include "../src/render/video.hpp"

#include "voidmaiz/embed.hpp"

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

/* A scene built by hand rather than through the dispatcher: these are pure
 * functions over a projection, and a `Core` round trip here would test the
 * dispatcher instead of the thing under test. */
static maiz::SceneNode node(const std::string& name, const std::string& glyph) {
    maiz::SceneNode n;
    n.name = name;
    n.glyph = glyph;
    return n;
}
static void field(maiz::SceneNode& n, const std::string& k, const std::string& v) {
    maiz::SceneField f;
    f.key = k;
    f.value_json = "\"" + v + "\"";
    f.is_string = true;
    n.fields.push_back(f);
}
static void wire(maiz::Scene& s, const std::string& a, const std::string& b,
                 const std::string& rel) {
    maiz::SceneWire w;
    w.from = a;
    w.to = b;
    w.relation = rel;
    s.wires.push_back(w);
}

int main() {
    using namespace hormiga;

    /* ── 1. the date predicates ─────────────────────────────────────────── */

    // Hinnant's algorithm against dates whose spacing is known: a leap day, a
    // century that is not a leap year, and the epoch itself.
    CHECK(days_from_civil(1970, 1, 1) == 0);
    CHECK(days_from_civil(2026, 8, 28) - days_from_civil(2026, 8, 19) == 9);
    CHECK(days_from_civil(2024, 3, 1) - days_from_civil(2024, 2, 28) == 2); // leap
    CHECK(days_from_civil(1900, 3, 1) - days_from_civil(1900, 2, 28) == 1); // not

    int y = 0, m = 0, d = 0;
    CHECK(parse_iso_date("2026-09-16", y, m, d) && y == 2026 && m == 9 && d == 16);
    CHECK(!parse_iso_date("", y, m, d));
    CHECK(!parse_iso_date("Last Friday of the Month", y, m, d));
    CHECK(!parse_iso_date("2026-13-01", y, m, d)); // a month that does not exist

    const long long today = days_from_civil(2026, 8, 28); // the day of the report

    maiz::Scene s;
    s.nodes.push_back(node("openhouse-aug", "event"));
    field(s.nodes.back(), "date", "2026-08-19"); // nine days ago
    s.nodes.push_back(node("relaunch-sep", "event"));
    field(s.nodes.back(), "date", "2026-09-16");
    s.nodes.push_back(node("meeting-today", "event"));
    field(s.nodes.back(), "date", "2026-08-28");
    s.nodes.push_back(node("monthly-meeting", "event"));
    field(s.nodes.back(), "days", "Last Friday of the Month"); // no date at all
    s.nodes.push_back(node("flier-openhouse", "image"));
    s.nodes.push_back(node("flier-relaunch", "image"));
    s.nodes.push_back(node("flier-orphan", "image")); // linked to nothing
    s.nodes.push_back(node("photo-dinner", "image"));
    s.nodes.push_back(node("job-open", "job"));
    field(s.nodes.back(), "deadline", "2026-12-01");
    s.nodes.push_back(node("job-closed", "job"));
    field(s.nodes.back(), "deadline", "2026-01-15");
    wire(s, "flier-openhouse", "openhouse-aug", "flyer-of");
    wire(s, "relaunch-sep", "flier-relaunch", "flier-for"); // the OTHER direction
    wire(s, "photo-dinner", "monthly-meeting", "flyer-of");

    auto when = [&](const char* n) { return when_of(s, *s.find(n), today); };
    CHECK(when("openhouse-aug") == When::Past);
    CHECK(when("relaunch-sep") == When::Future);
    CHECK(when("meeting-today") == When::Today);

    /* THE RULE THE REPORT SINGLED OUT: five events carry `days` and no `date`,
     * and a naive implementation reads the missing date as year zero and sweeps
     * every standing meeting into the past. */
    CHECK(when("monthly-meeting") == When::Recurring);

    // reaching through the edge, in both directions
    CHECK(when("flier-openhouse") == When::Past);
    CHECK(when("flier-relaunch") == When::Future);
    CHECK(when("photo-dinner") == When::Recurring);
    // and a rune with no date and no dated neighbour claims nothing
    CHECK(when("flier-orphan") == When::Undated);

    // a job's when is its deadline
    CHECK(when("job-open") == When::Future);
    CHECK(when("job-closed") == When::Past);

    auto match = [&](const char* expr, const char* n) {
        return query_matches(expr, s, *s.find(n), today);
    };
    CHECK(match("date:past", "openhouse-aug"));
    CHECK(!match("date:future", "openhouse-aug"));
    CHECK(match("date:future", "relaunch-sep"));
    CHECK(!match("date:past", "relaunch-sep"));

    /* Today has not happened yet: an event at 6pm belongs on "Coming up" at
     * 9am, so `date:today` answers `date:future` as well as itself. */
    CHECK(match("date:today", "meeting-today"));
    CHECK(match("date:future", "meeting-today"));
    CHECK(!match("date:past", "meeting-today"));

    // a standing series is upcoming and is never past
    CHECK(match("date:future", "monthly-meeting"));
    CHECK(match("date:recurring", "monthly-meeting"));
    CHECK(!match("date:past", "monthly-meeting"));

    // the undated rune matches neither half, so `flier AND date:future` holds
    // it back rather than guessing on its behalf
    CHECK(!match("date:future", "flier-orphan"));
    CHECK(!match("date:past", "flier-orphan"));
    CHECK(match("date:undated", "flier-orphan"));

    // it is the ORDINARY grammar over an extra fact: AND / OR / NOT compose,
    // and `glyph:` still works because the bag is upstream's plus ours
    CHECK(match("glyph:event AND date:future", "relaunch-sep"));
    CHECK(!match("glyph:event AND date:future", "openhouse-aug"));
    CHECK(match("glyph:event AND NOT date:past", "monthly-meeting"));
    CHECK(match("date:past OR date:future", "openhouse-aug"));
    // an empty expression still matches everything
    CHECK(match("", "flier-orphan"));
    /* An unknown `date:` value is a tag nothing carries, so it matches nothing
     * rather than everything - which is what a typo should do here. (`AND`
     * with a missing operand and an unbalanced paren are BOTH accepted by Void
     * Core's parser rather than rejected, so neither reaches the
     * invalid_argument path; that catch is upstream's contract, kept verbatim,
     * and it is not this suite's to provoke.) */
    CHECK(!match("date:futur", "relaunch-sep"));
    CHECK(!match("date:past", "flier-relaunch"));

    // the staleness number the render warning prints
    CHECK(days_stale(s, *s.find("flier-openhouse"), today) == 9);
    CHECK(days_stale(s, *s.find("flier-relaunch"), today) == 0);
    CHECK(days_stale(s, *s.find("photo-dinner"), today) == 0);  // recurring
    CHECK(days_stale(s, *s.find("flier-orphan"), today) == 0);  // nothing to be late for
    CHECK(days_stale(s, *s.find("openhouse-aug"), today) == 0); // has its own date

    /* ── 2. the video URL parser ────────────────────────────────────────── */

    auto yt = [](const char* u) { return parse_video_url(u); };
    CHECK(yt("https://www.youtube.com/watch?v=dQw4w9WgXcQ").id == "dQw4w9WgXcQ");
    CHECK(yt("https://www.youtube.com/watch?v=dQw4w9WgXcQ").provider == "youtube");
    CHECK(yt("https://youtu.be/dQw4w9WgXcQ").id == "dQw4w9WgXcQ");
    CHECK(yt("https://youtu.be/dQw4w9WgXcQ?si=abc123").id == "dQw4w9WgXcQ");
    CHECK(yt("https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42s").id == "dQw4w9WgXcQ");
    CHECK(yt("https://youtube.com/shorts/dQw4w9WgXcQ").id == "dQw4w9WgXcQ");
    CHECK(yt("https://www.youtube.com/embed/dQw4w9WgXcQ").id == "dQw4w9WgXcQ");
    CHECK(yt("https://www.youtube.com/live/dQw4w9WgXcQ").id == "dQw4w9WgXcQ");
    CHECK(yt("https://m.youtube.com/watch?v=dQw4w9WgXcQ").id == "dQw4w9WgXcQ");
    CHECK(yt("youtu.be/dQw4w9WgXcQ").id == "dQw4w9WgXcQ");   // no scheme
    CHECK(yt("  youtu.be/dQw4w9WgXcQ  ").id == "dQw4w9WgXcQ"); // pasted with space
    CHECK(yt("dQw4w9WgXcQ").provider == "youtube");            // the bare id
    CHECK(yt("https://vimeo.com/123456789").provider == "vimeo");
    CHECK(yt("https://vimeo.com/123456789").id == "123456789");
    CHECK(yt("https://player.vimeo.com/video/123456789").id == "123456789");

    // the player URL is the no-cookie host, and it is the ONLY place one is built
    CHECK(yt("https://youtu.be/dQw4w9WgXcQ").embed_url().rfind(
              "https://www.youtube-nocookie.com/embed/", 0) == 0);
    CHECK(yt("https://vimeo.com/123456789").watch_url() ==
          "https://vimeo.com/123456789");

    /* WHAT MUST NOT PARSE. Everything here comes back empty, which is what
     * makes it safe to interpolate `id` into an attribute without escaping:
     * nothing that passes can carry a quote, a bracket or a slash. */
    CHECK(!yt("").ok());
    CHECK(!yt("https://example.com/video.mp4").ok());
    CHECK(!yt("<iframe src=\"https://youtube.com/embed/x\"></iframe>").ok());
    CHECK(!yt("https://youtu.be/\"onerror=alert(1)").ok());
    CHECK(!yt("https://www.youtube.com/watch?v=").ok());
    CHECK(!yt("https://vimeo.com/not-a-number").ok());
    CHECK(!yt("javascript:alert(1)").ok());

    /* ── 3. the flier reader ────────────────────────────────────────────── */

    const int hint = 2026;
    auto only_iso = [&](const std::string& text) {
        const auto f = flier::dates_in(text, hint);
        return f.empty() ? std::string() : f.front().iso;
    };
    CHECK(only_iso("Open House September 16th at the library") == "2026-09-16");
    CHECK(only_iso("SEPTEMBER 16, 2026") == "2026-09-16");
    CHECK(only_iso("16 de septiembre") == "2026-09-16");
    CHECK(only_iso("Sept 16") == "2026-09-16");
    CHECK(only_iso("9/16/2026") == "2026-09-16");
    CHECK(only_iso("9/16/26") == "2026-09-16");
    CHECK(only_iso("2026-09-16") == "2026-09-16");
    // a bare `9/16` is not a date on a flier — it is as likely a time or a score
    CHECK(only_iso("doors 9/16 pm").empty());
    // and a phone number is not three dates
    CHECK(flier::dates_in("call 541-555-0134", hint).empty());
    // a flier repeats its date; it is proposed once
    CHECK(flier::dates_in("September 16th - September 16th", hint).size() == 1);

    const auto kws = flier::keywords_in(
        "Back to School Drive - free backpacks for all the students", 12);
    bool has_backpacks = false, has_stop = false;
    for (const auto& k : kws) {
        if (k == "backpacks") has_backpacks = true;
        if (k == "free" || k == "the" || k == "for" || k == "all") has_stop = true;
    }
    CHECK(has_backpacks);
    CHECK(!has_stop);

    /* THE LINE THE WHOLE FEATURE EXISTS FOR: the sheet prints one date and the
     * event stores another, and until now nothing anywhere noticed. */
    const auto bad = flier::propose("flier-openhouse", "Open House September 16th",
                                    "openhouse-aug", "2026-08-19", today);
    bool shouted = false;
    for (const auto& n : bad.notes)
        if (n.rfind("MISMATCH", 0) == 0) shouted = true;
    CHECK(shouted);

    const auto good = flier::propose("flier-relaunch", "Relaunch September 16th",
                                     "relaunch-sep", "2026-09-16", today);
    for (const auto& n : good.notes) CHECK(n.rfind("MISMATCH", 0) != 0);

    // it PROPOSES: every command is a `tag`, and none of them is a delete
    for (const auto& c : bad.commands) CHECK(c.rfind("tag ", 0) == 0);

    if (failures == 0) {
        std::cout << "OK - date predicates (past/today/future/recurring/undated, "
                     "through the edge) + video URL parsing + flier date "
                     "mismatch\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
