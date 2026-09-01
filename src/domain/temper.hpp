/* temper.hpp — idempotent tag-hygiene passes (Void Core's `temper` concept,
 * host-side). A temper pass reads the projected scene and emits `tag` commands
 * that normalize/derive tags; re-running it is a no-op because it only adds
 * what is missing. The caller wraps the result in one compile_commit batch —
 * one undo frame, logged, replayable — exactly like the importers.
 *
 * First pass: EVENT DATE → month:/season: tags. The rescued events carry an
 * ISO date (or recurring `days`) but no month/season tag, so a query like
 * `@season:summer` matched nothing. Deriving the tags makes the newsletter's
 * query-backed blocks filter by season and STAY live as events are added.
 *
 * Headless: Scene in, commands out — testable without a window.
 */
#pragma once

#include "domain/scene_value.hpp" // the ONE field reader (see that file's header)
#include "voidmaiz/scene.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace hormiga {
namespace temper {

inline const char* month_name(int m) {
    static const char* n[] = {"",     "january", "february", "march",
                              "april", "may",     "june",     "july",
                              "august", "september", "october", "november",
                              "december"};
    return (m >= 1 && m <= 12) ? n[m] : "";
}

/* Northern-hemisphere meteorological seasons (Dec–Feb winter, etc.). A future
 * org-config could flip the hemisphere; not worth a knob until asked. */
inline const char* season_of(int m) {
    if (m == 12 || m == 1 || m == 2) return "winter";
    if (m >= 3 && m <= 5) return "spring";
    if (m >= 6 && m <= 8) return "summer";
    if (m >= 9 && m <= 11) return "fall";
    return "";
}

/* THIS USED TO BE A SECOND COPY of the field reader, byte-identical to the one
 * in `app_shared.cpp`, and the pair is what let a decoding fix reach one caller
 * and not the other. Both now name the same primitive; keeping these aliases
 * means the temper passes below read unchanged. */
using hormiga::field_value;
using hormiga::has_tag;

/* For every `event` rune with a parseable ISO date (YYYY-MM-DD), add its
 * `month:` and `season:` tags if absent. Recurring events (no `date`, only
 * `days`) are skipped — they're always current, not tied to one month. */
inline std::vector<std::string> compile_date_tags(const maiz::Scene& data) {
    std::vector<std::string> out;
    for (const auto& n : data.nodes) {
        if (n.glyph != "event") continue;
        std::string date = field_value(n, "date");
        int y = 0, mo = 0, d = 0;
        if (std::sscanf(date.c_str(), "%d-%d-%d", &y, &mo, &d) != 3) continue;
        if (mo < 1 || mo > 12) continue;
        std::string add;
        std::string mtag = std::string("month:") + month_name(mo);
        std::string stag = std::string("season:") + season_of(mo);
        if (!has_tag(n, mtag)) add += " +" + mtag;
        if (!has_tag(n, stag)) add += " +" + stag;
        if (!add.empty()) out.push_back("tag " + n.name + add);
    }
    return out;
}

} // namespace temper
} // namespace hormiga
