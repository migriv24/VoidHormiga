/* app/verbs.cpp — command routing for the two verb families the app adds.
 *
 * `try_map_verb` / `try_doc_verb` take a typed command line and turn it into
 * dispatcher calls, so the command bar INSIDE the app reaches the same
 * vocabulary an agent uses. That is founding commitment 1 in its most literal
 * form, and it has nothing to do with maps — it lived in the map file only
 * because `map place` was the first verb family to need it. */

#include "app/app_internal.hpp"
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

bool HormigaApp::try_map_verb(const std::string& cmd) {
    std::vector<std::string> tok = tokenize(cmd);
    if (tok.empty() || tok[0] != "map") return false;
    if (tok.size() == 1 || tok[1] == "actions") {
        log.push_back({"info", "map", map_actions.manifest()});
        return true;
    }
    if (tok[1] == "rules") {
        // the rule AUDIT (author #4): what does each rule of the active view
        // actually match, and where do rules SHADOW each other (first-match-
        // wins means a later rule never styles an entity an earlier one took)
        if (active_rules.empty()) {
            log.push_back({"info", "map", "the active view has no rules"});
            return true;
        }
        maiz::ProjectOptions po;
        po.mantle = kDataMantle;
        maiz::Scene data = maiz::project_scene(core, po);
        auto expr = [](const MapRule& r) {
            std::string e;
            for (const auto& t : r.tags) e += (e.empty() ? "" : " AND ") + t;
            return e;
        };
        std::vector<int> match_count(active_rules.size(), 0);
        // shadowed[j][i] = rule i took entities rule j also matches
        std::map<std::pair<int, int>, int> shadows;
        for (const auto& n : data.nodes) {
            if (view_geo(n, active_channel).empty()) continue; // on-map only
            int first = -1;
            for (size_t i = 0; i < active_rules.size(); ++i) {
                if (active_rules[i].tags.empty() ||
                    !maiz::node_matches(expr(active_rules[i]), n))
                    continue;
                if (first < 0) {
                    first = (int)i;
                    ++match_count[i];
                } else {
                    ++shadows[{(int)i, first}]; // i would match, but first won
                }
            }
        }
        for (size_t i = 0; i < active_rules.size(); ++i)
            log.push_back({"info", "rules",
                           "'" + active_rules[i].name + "' [" +
                               expr(active_rules[i]) + "] styles " +
                               std::to_string(match_count[i]) + " entit" +
                               (match_count[i] == 1 ? "y" : "ies")});
        for (const auto& [pair, cnt] : shadows)
            log.push_back(
                {"warn", "rules",
                 "'" + active_rules[pair.second].name + "' SHADOWS '" +
                     active_rules[pair.first].name + "' for " +
                     std::to_string(cnt) +
                     " entit" + (cnt == 1 ? "y" : "ies") +
                     " (first match wins - reorder or narrow the tags)"});
        if (shadows.empty())
            log.push_back({"info", "rules", "no rule conflicts (no shadowing)"});
        return true;
    }
    const maiz::ActionDescriptor* a = map_actions.find(tok[1]);
    if (!a) {
        log.push_back({"error", "map",
                       "unknown action '" + tok[1] + "' - try `map actions`"});
        return true;
    }
    maiz::ActionArgs args; // positional tokens → params in declared order
    size_t ti = 2;
    for (const auto& p : a->params) {
        if (ti < tok.size()) args[p.name] = tok[ti++];
        else if (p.required) {
            log.push_back({"error", "map",
                           "map " + a->name + ": missing <" + p.name + "> (" +
                               p.doc + ")"});
            return true;
        }
    }
    if (scene.mantle != kDataMantle) // map actions act on the org's data
        dispatch_and_reproject(std::string("use ") + kDataMantle);
    auto cmds = map_actions.run(tok[1], scene, args);
    if (cmds.empty()) {
        log.push_back({"error", "map",
                       "map " + a->name + " declined (bad args, or name taken)"});
        return true;
    }
    maiz::Result r = dispatch_and_reproject(maiz::compile_commit(cmds));
    log.push_back({">", cmd,
                   r.ok ? "ok - " + std::to_string(cmds.size()) +
                              " command(s), one undo frame"
                        : "failed: " + r.text()});
    return true;
}

/* The `doc` verb front-end (builder.md B1; the Q18 elevation): the Builder's
 * vocabulary as first-class commands — `doc actions` prints the manifest,
 * `doc migrate` converts the legacy chain to the grid, place/move/resize/
 * remove are the SAME compiles the document canvas will call. Acts on the
 * ISSUE mantle (documents live there). */
bool HormigaApp::try_doc_verb(const std::string& cmd) {
    std::vector<std::string> tok = tokenize(cmd);
    if (tok.empty() || tok[0] != "doc") return false;
    if (tok.size() == 1 || tok[1] == "actions") {
        log.push_back({"info", "doc", doc_actions.manifest()});
        return true;
    }
    const maiz::ActionDescriptor* a = doc_actions.find(tok[1]);
    if (!a) {
        log.push_back({"error", "doc",
                       "unknown action '" + tok[1] + "' - try `doc actions`"});
        return true;
    }
    maiz::ActionArgs args; // positional tokens → params in declared order
    size_t ti = 2;
    for (const auto& p : a->params) {
        if (ti < tok.size()) args[p.name] = tok[ti++];
        else if (p.required) {
            log.push_back({"error", "doc",
                           "doc " + a->name + ": missing <" + p.name + "> (" +
                               p.doc + ")"});
            return true;
        }
    }
    if (scene.mantle != cur_doc) // documents live in the issue mantle
        dispatch_and_reproject(std::string("use ") + cur_doc);
    auto cmds = doc_actions.run(tok[1], scene, args);
    if (cmds.empty()) {
        /* ── "NOTHING TO DO" IS NOT AN ERROR ─────────────────────────────────
         *
         * `doc migrate` is documented as idempotent — "re-running is a no-op" —
         * and it produces no commands when every component already has a grid
         * row, which is the ordinary state of every migrated document. That
         * success was logged at `[error]`, in red, in the same console strip
         * that reports a failed publish.
         *
         * The report named the real cost: an operator who sees red text after a
         * successful action learns that red text here is normal, and that is
         * precisely the habit you least want in the panel that also has to tell
         * him a deploy failed.
         *
         * The three-causes-in-one-message problem is the other half of it.
         * "bad args, name taken, or nothing to do" is not a diagnosis, it is a
         * list of things a diagnosis would choose between — so the idempotent
         * actions say what they mean and the rest keep the honest uncertainty. */
        const bool idempotent = a->name == "migrate";
        log.push_back({idempotent ? "info" : "error", "doc",
                       idempotent
                           ? "doc " + a->name + ": nothing to do - already done"
                           : "doc " + a->name +
                                 " declined (bad args, name taken, or nothing "
                                 "to do)"});
        return true;
    }
    maiz::Result r = dispatch_and_reproject(maiz::compile_commit(cmds));
    log.push_back({">", cmd,
                   r.ok ? "ok - " + std::to_string(cmds.size()) +
                              " command(s), one undo frame"
                        : "failed: " + r.text()});
    return true;
}

/* The reusable search picker: an input + inline type-ahead results (name,
 * glyph, first-field subtitle). Returns the picked rune name ("" = none this
 * frame) and clears the buffer on pick. One component, many features. */
