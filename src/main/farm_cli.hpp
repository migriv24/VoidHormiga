/* main/farm_cli.hpp — `voidhormiga-cli farm …` (okf/concepts/platform/antfarm/v2/cli.md). */
#pragma once

#include "voidmaiz/headless.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

struct HormigaApp;

namespace hormiga {
/* Opens its own session on `state` (Void Maiz's headless session has no
 * host-verb hook), runs one farm verb, closes. Returns the process exit code. */
int run_farm_cli(const maiz::HostApp& app, const std::filesystem::path& state, const std::filesystem::path& base,
                 const std::filesystem::path& ship, const std::vector<std::string>& tok,
                 const std::vector<std::string>& flags);

/* If argv holds the word `farm` (not as --state's value), run it and return
 * the exit code; otherwise -1, and the ordinary CLI carries on. */
/* `base` and `state_name` are the CLI's own globals, UPDATED here: an effect's
 * throwaway app reads them, and it must work beside the database, never beside
 * whatever folder the process was started in. */
int farm_cli_main(const maiz::HostApp& app, int argc, char** argv, const std::string& given_state,
                  std::filesystem::path& base, std::string& state_name, const std::filesystem::path& ship);

/* The four v2 effects, for the briefing and the gate. */
std::vector<maiz::EffectOp> farm_effect_ops();

/* `effect farm-…` in a headless session: run it in a throwaway app loaded from
 * the session's state, dispatch what it records into the session's core. */
std::string farm_effect_headless(HormigaApp& app, maiz::Core& target, std::string_view op,
                                 const std::vector<std::string>& args);
} // namespace hormiga
