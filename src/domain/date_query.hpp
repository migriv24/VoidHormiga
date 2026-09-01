/* date_query.hpp — the filter grammar, told what today is.
 *
 * ── why this file exists ─────────────────────────────────────────────────────
 *
 * From the field report, 2026-08-28: on August 28 the live home page's
 * "Fliers to Share" was showing a flier for an open house held on **August
 * 19**, and "Coming up" listed three events that had already happened. Nothing
 * was broken. `flier AND issue:aug2026` is exactly what it says — and there was
 * no way to say the thing every one of those blocks actually means, which is
 * *the ones that have not happened yet*.
 *
 * The operator's workaround was two more hand-maintained tags (`site:current`,
 * `site:archive`) plus a script to tell a person when they had gone stale.
 * That is three places to remember instead of zero, and it is the exact failure
 * the query-backed block was built to remove: a page that gets *more* correct
 * as the database fills in, not one that decays until somebody re-tags it.
 *
 * ── the mechanism: a bag, not a parser ──────────────────────────────────────
 *
 * We do not fork the grammar. `maiz::node_matches` is a thin thing: it builds a
 * BAG of strings for a rune — its tags, its name, `glyph:<g>` — and hands the
 * bag plus the expression to `Core::tag_match`. Everything the grammar can
 * express (`AND` `OR` `NOT`, parentheses, exact matching) is already there, and
 * `glyph:event` proves the bag is allowed to carry things the author never
 * typed as a tag.
 *
 * So a date predicate is not a new operator. It is three more strings in the
 * bag, computed at the render seam from the clock:
 *
 *     date:past   date:today   date:future   date:recurring   date:undated
 *
 * `query 'type:event AND date:future'` is then the ordinary grammar over an
 * extra fact, `NOT date:past` composes for free, and Void Core is not patched
 * for any of it (CLAUDE.md rule 4).
 *
 * Deriving the tags rather than STORING them is the whole point. A `temper`
 * pass that wrote `date:past` into the database would be a snapshot: correct
 * the day it ran and wrong every day after, which is `site:current` again with
 * a nicer name. These exist only for the length of one match.
 *
 * ── the three rules that come from the data, not from taste ─────────────────
 *
 * 1. **Recurring events are not in the past.** Five of the reporting database's
 *    events carry no `date` at all — they carry `days: "Last Friday of the
 *    Month"`. Treating a missing date as year zero would sweep every one of
 *    them into `date:past` and drop the standing monthly meetings off "Coming
 *    up", which is the one thing a naive implementation gets wrong. A recurring
 *    event is `date:recurring` AND `date:future`: its next occurrence has not
 *    happened yet, which is what the page is asking about.
 *
 * 2. **A flier reaches through its edge.** A flier has no date of its own; its
 *    date is its event's, and 21 of that database's 55 fliers already carry the
 *    edge that says which. So a rune with no date of its own borrows the date
 *    of any `event` rune it is wired to — stated generally, by ANY relation and
 *    in both directions, for the same reason `related_runes` is general: which
 *    way somebody wired an edge is an authoring accident a reader should not
 *    pay for. When several events are linked, the LATEST one wins, because a
 *    sheet is current until the last thing it advertises is over.
 *
 * 3. **Today has not happened yet.** An event at 6pm today belongs on "Coming
 *    up" at 9am, so `date:today` also answers `date:future`. `date:past` is
 *    strictly before today. Nothing here reads a time-of-day: `start_time` is a
 *    free-text field in this model, and a predicate that silently depended on
 *    parsing it would be right on the runes that happen to fill it in.
 *
 * A rune with neither a date nor a dated neighbour is `date:undated` and
 * matches NONE of past/today/future — so `flier AND date:future` publishes the
 * fliers whose event is still coming and quietly holds back the ones with no
 * event at all, rather than guessing on their behalf.
 *
 * ── dependency-light on purpose ─────────────────────────────────────────────
 *
 * Scene in, bool out. No `HormigaApp`, no ImGui, no I/O beyond reading the
 * clock, so `tests/spine_smoke.cpp` can pin the whole thing without linking the
 * app — the same discipline `temper.hpp` and `scene_value.hpp` keep, and the
 * reason a date rule is testable at all.
 */
#pragma once

#include "domain/scene_value.hpp" // field_value

#include "voidmaiz/embed.hpp"   // Core::tag_match
#include "voidmaiz/project.hpp" // filter_bag
#include "voidmaiz/scene.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

namespace hormiga {

/* Days since 1970-01-01 for a proleptic Gregorian civil date.
 *
 * Howard Hinnant's `days_from_civil`, which is exact for every date this
 * application can hold and needs neither `mktime` (which mutates a `tm`, reads
 * the local timezone and can fail before 1970 on some libcs) nor a 64-bit
 * time_t. Two dates are compared as integers; nothing here is a timestamp. */
inline long long days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);              // [0, 399]
    const unsigned doy = (153u * (unsigned)(m + (m > 2 ? -3 : 9)) + 2u) / 5u +
                         (unsigned)d - 1;                        // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
    return era * 146097 + (long long)doe - 719468;
}

/* An ISO `YYYY-MM-DD` (the shape the `date` editor writes and every importer
 * normalizes to). Anything else — blank, a free-text "Last Friday", a partial
 * date — is not a date, and saying so is the point: a half-parsed date is how
 * a rune ends up sorted into the wrong half of a website. */
inline bool parse_iso_date(const std::string& s, int& y, int& m, int& d) {
    y = m = d = 0;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    return true;
}

/* Today, in the operator's own timezone.
 *
 * LOCAL rather than UTC deliberately. "Has this happened yet" is a question
 * about the day the person looking at the page is living in; a UTC comparison
 * would move an event to "past" while it is still this afternoon in Oregon. */
inline long long today_days() {
    const std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    return days_from_civil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}

/* Where a rune of this glyph keeps its when.
 *
 * `date` for everything that has one; `deadline` for a job, because a posting's
 * when IS its closing date and `job_grid AND date:future` should mean "still
 * open". One table rather than a special case at each call site, so a glyph
 * that grows a date later is one line here. */
inline std::string date_field_of(const maiz::SceneNode& n) {
    if (n.glyph == "job") return field_value(n, "deadline");
    return field_value(n, "date");
}

/* Does this rune recur instead of happening once? `days` is the free-text
 * recurrence field ("Last Friday of the Month"); the model draws the `date` /
 * `days` distinction itself and this is the consuming half of it. */
inline bool is_recurring(const maiz::SceneNode& n) {
    return date_field_of(n).empty() && !field_value(n, "days").empty();
}

enum class When { Undated, Past, Today, Future, Recurring };

/* This rune's own when, ignoring anything it is wired to. */
inline When own_when(const maiz::SceneNode& n, long long today) {
    int y = 0, m = 0, d = 0;
    const std::string iso = date_field_of(n);
    if (parse_iso_date(iso, y, m, d)) {
        const long long day = days_from_civil(y, m, d);
        return day < today ? When::Past : (day == today ? When::Today : When::Future);
    }
    return is_recurring(n) ? When::Recurring : When::Undated;
}

/* Order used to pick a winner when a rune borrows from several events: the
 * furthest-out claim wins, and a recurring series outranks any fixed date
 * because it never stops being upcoming. */
inline int when_rank(When w) {
    switch (w) {
    case When::Undated: return 0;
    case When::Past: return 1;
    case When::Today: return 2;
    case When::Future: return 3;
    case When::Recurring: return 4;
    }
    return 0;
}

/* This rune's when, reaching through its edges when it has none of its own.
 *
 * The borrow is one hop and only onto `event` runes. One hop because a chain of
 * borrows has no natural stopping point and would make a page's contents depend
 * on how deep somebody happened to wire the graph; `event` only because an
 * event is the dated thing in this model — a flier, a photograph and a note all
 * mean "the day of the thing this is about". */
inline When when_of(const maiz::Scene& scene, const maiz::SceneNode& n,
                    long long today) {
    const When mine = own_when(n, today);
    if (mine != When::Undated) return mine;

    When best = When::Undated;
    long long best_day = 0;
    for (const auto& w : scene.wires) {
        std::string other;
        if (w.from == n.name) other = w.to;
        else if (w.to == n.name) other = w.from;
        if (other.empty()) continue;
        const maiz::SceneNode* ev = scene.find(other);
        if (!ev || ev->glyph != "event") continue;
        const When theirs = own_when(*ev, today);
        if (theirs == When::Undated) continue;
        int y = 0, m = 0, d = 0;
        const long long day = parse_iso_date(date_field_of(*ev), y, m, d)
                                  ? days_from_civil(y, m, d)
                                  : 0;
        /* Latest wins: a higher rank outright, and within the same rank (two
         * past events, say) the later date — a sheet advertising two dates is
         * current until the last of them is over. */
        if (when_rank(theirs) > when_rank(best) ||
            (when_rank(theirs) == when_rank(best) && day > best_day)) {
            best = theirs;
            best_day = day;
        }
    }
    return best;
}

/* The `date:` strings a when contributes to the bag. See rule 1 and rule 3 at
 * the top of the file for why `recurring` and `today` each answer `future`. */
inline void append_date_tags(When w, std::vector<std::string>& bag) {
    switch (w) {
    case When::Past: bag.push_back("date:past"); break;
    case When::Today:
        bag.push_back("date:today");
        bag.push_back("date:future");
        break;
    case When::Future: bag.push_back("date:future"); break;
    case When::Recurring:
        bag.push_back("date:recurring");
        bag.push_back("date:future");
        break;
    case When::Undated: bag.push_back("date:undated"); break;
    }
}

/* The bag a block query is matched against: everything Void Maiz already puts
 * in it, plus this rune's date facts. */
inline std::vector<std::string> query_bag(const maiz::Scene& scene,
                                          const maiz::SceneNode& n,
                                          long long today) {
    std::vector<std::string> bag = maiz::filter_bag(n);
    append_date_tags(when_of(scene, n, today), bag);
    return bag;
}

/* Does this rune match the block's query?
 *
 * THE replacement for `maiz::node_matches` at every seam that resolves a
 * query-backed block, and identical to it for every expression that does not
 * mention a date — same grammar, same bag, same degradation. A malformed
 * expression matches everything, because a filter should fail to a full page
 * rather than to a blank one; that is upstream's rule and it stays.
 *
 * `scene` is the DATA scene the rune came from — the edges are read out of it,
 * so passing the document mantle here would silently disable the borrow. */
inline bool query_matches(std::string_view expr, const maiz::Scene& scene,
                          const maiz::SceneNode& n, long long today) {
    if (expr.empty()) return true;
    try {
        return maiz::Core::tag_match(expr, query_bag(scene, n, today));
    } catch (const std::invalid_argument&) {
        return true; // malformed mid-keystroke: degrade to "show everything"
    }
}

inline bool query_matches(std::string_view expr, const maiz::Scene& scene,
                          const maiz::SceneNode& n) {
    return query_matches(expr, scene, n, today_days());
}

/* Does this expression ask about a date at all?
 *
 * Cheap and textual on purpose. Its only job is to let a caller skip work it
 * would otherwise do for nothing — the flier-staleness warning below, and the
 * `query` effect's explanation of what it evaluated. It is never used to decide
 * whether a rune matches; that is always the real evaluation. */
inline bool mentions_date(std::string_view expr) {
    return expr.find("date:") != std::string_view::npos;
}

/* How many days past its linked event a rune is, or 0 when it is not past.
 *
 * The render seam's staleness check: a flier whose event was nine days ago is
 * still on the page and nothing in the database noticed. Reported rather than
 * fixed, because which sheet belongs on a page is the operator's call — the
 * renderer's job is to make the silence stop. */
inline int days_stale(const maiz::Scene& scene, const maiz::SceneNode& n,
                      long long today) {
    if (!date_field_of(n).empty() || is_recurring(n)) return 0; // its own date
    long long latest = 0;
    bool any = false;
    for (const auto& w : scene.wires) {
        std::string other;
        if (w.from == n.name) other = w.to;
        else if (w.to == n.name) other = w.from;
        if (other.empty()) continue;
        const maiz::SceneNode* ev = scene.find(other);
        if (!ev || ev->glyph != "event") continue;
        if (is_recurring(*ev)) return 0; // a standing series is never stale
        int y = 0, m = 0, d = 0;
        if (!parse_iso_date(date_field_of(*ev), y, m, d)) continue;
        const long long day = days_from_civil(y, m, d);
        if (!any || day > latest) { latest = day; any = true; }
    }
    if (!any || latest >= today) return 0;
    return (int)(today - latest);
}


/* Every rune in `published` whose linked event has already happened, worst
 * first.
 *
 * The scan half of the render-seam staleness check, here rather than in the
 * renderer so it is Scene-in / rows-out and can be pinned without building a
 * website. The renderer keeps the sentence; this keeps the arithmetic. */
struct StaleRune {
    std::string rune;
    int days;
};

inline std::vector<StaleRune> stale_published(const maiz::Scene& scene,
                                              const std::set<std::string>& published,
                                              long long today) {
    std::vector<StaleRune> out;
    for (const auto& n : scene.nodes) {
        if (n.glyph != "image") continue;
        if (!published.count(n.name)) continue;
        const int d = days_stale(scene, n, today);
        if (d > 0) out.push_back({n.name, d});
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const StaleRune& a, const StaleRune& b) {
                         return a.days > b.days;
                     });
    return out;
}

} // namespace hormiga
