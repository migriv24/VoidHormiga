/* app/farm_host.hpp — what Hormiga tells Antfarm v2 about itself.
 *
 * src/antfarm/ knows nothing about Hormiga (Q92). This is the other half: the
 * chambers read from this database, the vault, the folders beside it, this
 * device. One builder, called by the CLI (`voidhormiga-cli farm …`), the desktop
 * tab and the phone, so the three can never compute a face three ways. */
#pragma once

#include "antfarm/farm_verbs.hpp"
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/embed.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace hormiga {
class Vault;
}

namespace hormiga::farmhost {

struct Device {
    std::string kind = "desktop"; // desktop | phone
    bool serves_local = true;     // a phone cannot serve a local address today
    std::string me;               // this profile's username
    std::vector<std::filesystem::path> also; // where else a relative file may be (beside the binary: demo-assets/)
};

/* ── THE CHAMBERS (domain/chambers.hpp) ────────────────────────────────
 * The commands that bring the Assets, Network and Documents mantles in step
 * with what exists: creating a missing chamber, registering files, this
 * device's profile, one entry per document. Empty when nothing is out of step,
 * which is what lets the GUI call it whenever it looks. `back` is the mantle
 * to leave active. */
/* What one Antfarm v2 effect did (app/farm_effects.cpp): its report, and the
 * commands that record it, for the caller to dispatch into the core it serves. */
struct Effect {
    bool ok = true;
    std::string text;
    std::vector<std::string> commands;
};

struct SelfFacts {
    std::string username, color, fingerprint, device = "desktop", platform;
    bool serves_local = true;
};
SelfFacts this_device(bool phone); // from the profile on disk: the first boot already has its fingerprint
std::vector<std::string> chamber_commands(maiz::Core& core, const std::filesystem::path& base,
                                          const std::vector<std::filesystem::path>& also, const SelfFacts& self,
                                          const std::string& back);

/* THE CAT SHOWCASE (the author, 2026-09-28: "use the cat database to really
 * showcase the possibilities with the antfarm now ... the cat database is all
 * local, however, that shouldn't stop our creativity"). A Cat Colony website
 * and a birthdays calendar to present, a CSV of newly arrived cats to import,
 * and a v2 colony that uses every kind of node: filters and joins, a fan-in
 * river with three reservoirs, a store that really copies, keys that honestly
 * need values, a web domain with three documents mounted, planned nodes where
 * the future goes. Writes the CSV beside the database; returns the commands. */
std::vector<std::string> showcase_commands(maiz::Core& core, const std::filesystem::path& base,
                                           const std::string& back, std::string& note);

farm::Context make_context(maiz::Core& core, const std::filesystem::path& base, const hormiga::Vault* vault,
                           const Device& dev,
                           std::function<std::string(const std::string&)> presence = {});

bool farm_exists(maiz::Core& core);
maiz::Scene project_farm(maiz::Core& core); // with named wires resolved for drawing

/* The demo documents a fresh v2 Antfarm points at: the first Builder document,
 * the first calendar view, the first map view. */
farm::SeedInfo seed_info(maiz::Core& core, const std::string& website_doc, const std::string& actor);

/* The Connections view (canvas.md §4), as rows both front-ends draw. */
struct Row {
    std::string group, name, label, state, why, line;
};
std::vector<Row> connection_rows(const farm::Graph& g, farm::Evaluator& ev);

/* What the application keeps between frames for Antfarm v2 (one member of
 * HormigaApp, so app.hpp does not grow a field per feature). */
struct UiState {
    bool v2 = true;      // the Antfarm tab shows v2 (when this database has one)
    bool here = false;   // this database has a `farm` mantle (refreshed on reproject)
    int view = 0;        // 0 Connections, 1 Wiring
    bool fit = true;     // fit the whole graph in view on the next Wiring frame
    bool chambers_dirty = true; // reconcile the chambers at the start of the next frame
    maiz::AddPalette palette;                // from farm::kinds(), never hand-kept
    std::map<std::string, farm::Face> faces; // per node, computed on reproject
    std::vector<Row> rows;
    std::string key_node;                    // the key whose value is being typed
    char key_buf[256] = {};
};

/* Socket colours and shapes for the canvas (types.md §1). */
std::map<std::string, maiz::PortStyle> port_styles();

/* The canvas's wire gestures, written with port NAMES and checked at the door.
 * `refused` receives the sentence when a wire is not allowed. */
maiz::WireWriter wire_writer(std::function<const maiz::Scene&()> scene,
                             std::function<void(const std::string&)> refused);

/* `farm key set` / `reveal`: the vault is the host's. Reads the value from
 * `value` (never an argument on a command line). Returns a sentence. */
std::string key_set(hormiga::Vault& vault, const std::filesystem::path& vault_file, const std::string& entry,
                    const std::string& value);

} // namespace hormiga::farmhost
