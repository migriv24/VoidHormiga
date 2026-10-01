/* antfarm/farm_verbs.hpp — the `farm` grammar (okf/concepts/platform/antfarm/v2/cli.md).
 *
 * One engine, two doors: `voidhormiga-cli farm …` and the command bar inside the
 * application both call `run`. Reading verbs return text. Changing verbs return
 * ordinary Void Core commands (`rune new`, `set`, `link --relation out:in`) for
 * the caller to dispatch as one batch, so the log and replay stay in Void Core's
 * vocabulary and the check happened before anything was written. */
#pragma once

#include "antfarm/farm_eval.hpp"

#include <string>
#include <vector>

namespace farm {

struct VerbResult {
    bool ok = true;
    std::string text;                  // what to print (errors included)
    std::vector<std::string> commands; // run as ONE batch, inside the `farm` mantle
    bool needs_host = false;           // the chambers, the effects, the vault: the host's
};

/* What a fresh v2 Antfarm points its demo documents at. Empty = leave unnamed. */
struct SeedInfo {
    std::string website;   // a Builder document
    std::string calendar;  // a calview rune
    std::string map;       // a map view rune
    std::string actor;     // who is creating it
};

/* `args` is everything after the word `farm`. `exists` says whether the `farm`
 * mantle is there (a database made before v2 has none). `ev` may be null for
 * callers that only change the graph. */
VerbResult run(const std::vector<std::string>& args, const Graph& g, bool exists, Evaluator* ev,
               const SeedInfo& seed = {});

/* The default colony (cli.md §5), as commands inside the `farm` mantle. */
std::vector<std::string> seed_commands(const SeedInfo& seed);

/* Lay the graph out: strata top to bottom (surface, ground, chambers), flow left
 * to right. Positions only, one batch, so it arranges it for every member. */
std::vector<std::string> arrange_commands(const Graph& g);

/* The Connections view as text: `farm` with no arguments. */
std::string connections_text(const Graph& g, Evaluator& ev);

/* Wrap changing commands so they land in the `farm` mantle and the caller's
 * mantle is active again afterwards. */
std::vector<std::string> in_farm(const std::vector<std::string>& cmds, const std::string& back);

std::vector<std::string> tokenize(const std::string& line); // shell-ish: quotes group words

} // namespace farm
