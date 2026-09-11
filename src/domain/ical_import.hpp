/* domain/ical_import.hpp — an incoming calendar, as a PLAN.
 *
 * `ical.hpp` reads the format; this decides what reading it should *do*. The
 * two are separate files because they are separate jobs and have different
 * dependencies: the lens needs nothing but `clock.hpp`, and this needs to look
 * at the database to know what is already there.
 *
 * ── IT PROPOSES; `apply` WRITES ──────────────────────────────────────────────
 *
 * Nothing here dispatches. It returns commands and a report, and the caller
 * decides — the same posture `effect read-flier` takes ("proposes only: it
 * dispatches nothing") and the sync effects take ("every one of these REPORTS
 * by default and writes only on `apply`"). A calendar import is the operation
 * with the widest blast radius in the application: a feed can hold a thousand
 * entries, and an importer that silently wrote them into somebody's
 * organization would be unforgivable and unrepairable in one step.
 *
 * ── EVERY CHANGE IS A COMMAND ────────────────────────────────────────────────
 *
 * Founding commitment 1: *every contact edit, block snap, tag pass and deploy
 * is a logged, replayable dispatcher command.* An importer that wrote rows
 * directly would be the first thing in this application that is not replayable,
 * and the headless replay test would stop meaning what it means. So the output
 * is text — the same verbs a person could have typed.
 *
 * ── IDENTITY IS WHERE A HUB IS WON OR LOST ───────────────────────────────────
 *
 * The foreign `UID` is stored on the rune as `ext_uid` and is the ONLY thing
 * matched on. Re-importing the same feed must update rather than duplicate, and
 * the way that goes wrong is fuzzy matching: guessing that an entry with the
 * same title and date "is probably" one you already have. `data-planes.md`
 * already states this rule for contacts — *claiming a contact must not search
 * the database* — and it is the same rule. Match the key you were given, or
 * create. Never guess.
 */
#pragma once

#include "domain/ical.hpp"
#include "domain/quick_add.hpp"   // slug — one way to name a rune
#include "domain/scene_value.hpp" // field_value, the ONE field reader

#include "voidmaiz/embed.hpp" // maiz::arg = Void Core's own quoter
#include "voidmaiz/scene.hpp"

#include <set>
#include <string>
#include <vector>

namespace hormiga {
namespace ical {

struct Plan {
    std::vector<std::string> commands; // what `apply` would dispatch
    std::vector<std::string> notes;    // what a person should read first
    int create = 0;
    int update = 0;
    int unchanged = 0;
};

/* A `cal:` tag naming where an entry came from.
 *
 * This is the invariant that makes a subscription safe to re-run (X4): a feed
 * owns exactly the runes carrying its own tag, so a hand-made event is
 * structurally out of its reach and cannot be overwritten by a refresh. */
inline std::string source_tag(const std::string& label) {
    std::string s = quick::slug(label, 40);
    return s.empty() ? std::string("cal:imported") : "cal:" + s;
}

/* Quote a value for the dispatcher.
 *
 * `maiz::arg` and not a hand-rolled quoter, and the difference was not
 * theoretical. The first version flattened newlines to spaces on the way in
 * and then compared the UNFLATTENED parsed value against what had been stored,
 * so every re-import of Google's holiday feed reported 174 updates for a file
 * that had not changed: its DESCRIPTION carries a newline ("Observance" then
 * "To hide observances, go to ..."), and a value written one way can never
 * equal a value compared another. Two bugs wearing one coat -- a silent data
 * loss, and a plan that could never reach a fixed point.
 *
 * `maiz::arg` is Void Core's own `vc_arg_quote` (SPEC 6.1), which is also what
 * gets the trailing-backslash case right -- the one this repository has now
 * hit three times. A newline round-trips through it intact, which is checked
 * rather than assumed: a `set` carrying one stores one.
 */
inline std::string q(const std::string& v) { return maiz::arg(v); }

/* Build the plan. `data` is the data mantle's projection; `label` names the
 * source (the feed's X-WR-CALNAME, or a name the operator gave). */
inline Plan plan_import(const ParseReport& in, const maiz::Scene& data,
                        const std::string& label) {
    Plan p;
    const std::string tag = source_tag(label);

    // ext_uid -> existing rune, built once. The map is the whole matching
    // strategy: present means update, absent means create, and there is no
    // third answer that involves guessing.
    std::vector<std::pair<std::string, const maiz::SceneNode*>> by_uid;
    std::set<std::string> taken;
    for (const auto& n : data.nodes) {
        taken.insert(n.name);
        const std::string u = hormiga::temper::field_value(n, "ext_uid");
        if (!u.empty()) by_uid.emplace_back(u, &n);
    }
    auto find_by_uid = [&](const std::string& u) -> const maiz::SceneNode* {
        if (u.empty()) return nullptr;
        for (const auto& e : by_uid)
            if (e.first == u) return e.second;
        return nullptr;
    };

    for (const auto& e : in.events) {
        const maiz::SceneNode* existing = find_by_uid(e.uid);

        /* The fields an imported entry sets. Deliberately a short list: an
         * import writes what a calendar actually carries and touches nothing
         * else, so an organization's own notes, tags and relations on a rune it
         * has since edited survive a re-import. */
        std::vector<std::pair<std::string, std::string>> fields = {
            {"date", e.date},
            {"start_time", e.start_time},
            {"end_time", e.end_time},
            {"title_en", e.summary},
            {"summary_en", e.description},
            {"venue", e.location},
            {"geo", e.geo},
            {"rrule", e.rrule},
            {"ext_uid", e.uid},
        };

        if (existing) {
            std::vector<std::string> sets;
            for (const auto& f : fields) {
                if (f.second.empty()) continue; // absent upstream != "clear it"
                if (hormiga::temper::field_value(*existing, f.first) == f.second)
                    continue;
                sets.push_back("set " + existing->name + " " + f.first + " " +
                               q(f.second));
            }
            if (sets.empty()) {
                ++p.unchanged;
                continue;
            }
            ++p.update;
            for (auto& c : sets) p.commands.push_back(std::move(c));
            continue;
        }

        // a new entry: mint a name from the title, uniquified against the
        // mantle AND against the names this plan has already spoken for
        std::string base = quick::slug(e.summary);
        if (base.empty()) base = "event";
        std::string name = base;
        for (int i = 2; taken.count(name); ++i) name = base + "-" + std::to_string(i);
        taken.insert(name);

        ++p.create;
        p.commands.push_back("rune new event " + name);
        for (const auto& f : fields)
            if (!f.second.empty())
                p.commands.push_back("set " + name + " " + f.first + " " + q(f.second));
        p.commands.push_back("tag " + name + " +type:event +" + tag);
        /* CATEGORIES become `kw:` tags, which is the exact inverse of what
         * `public_categories` emits — so a calendar that leaves here and comes
         * back arrives with the same tags it left with. That symmetry is what
         * "pivot" is supposed to mean. */
        for (const auto& c : e.categories) {
            const std::string k = quick::slug(c, 32);
            if (!k.empty()) p.commands.push_back("tag " + name + " +kw:" + k);
        }
    }

    // ── what the operator should know BEFORE saying yes ─────────────────────
    if (!in.calendar_name.empty())
        p.notes.push_back("source calendar: \"" + in.calendar_name + "\"");
    p.notes.push_back("imported entries are tagged " + tag +
                      " -- that tag is how a re-import finds them again");
    if (in.skipped_no_date)
        p.notes.push_back(std::to_string(in.skipped_no_date) +
                          " entr(ies) had no usable start date and were skipped");
    if (in.skipped_components)
        p.notes.push_back(
            std::to_string(in.skipped_components) +
            " component(s) were not calendar entries (VTODO, VALARM, VTIMEZONE, "
            "...) and were walked past, which is deliberate");
    if (in.recurring)
        p.notes.push_back(
            std::to_string(in.recurring) +
            " entr(ies) REPEAT. The rule is stored verbatim in `rrule` so "
            "nothing is lost, but only the first occurrence is placed on the "
            "calendar -- expanding them is not built yet (C3a)");
    if (in.zoned)
        p.notes.push_back(
            std::to_string(in.zoned) +
            " entr(ies) carried a time zone. UTC times were converted to this "
            "computer's local time; a named zone was read as written, because "
            "guessing another zone's rules without a database would be worse "
            "than saying so (Q68)");
    p.notes.push_back(
        "every imported entry becomes an `event`. Incidents are a judgement a "
        "person makes, not something a feed can declare");
    return p;
}

} // namespace ical
} // namespace hormiga
