/* ui/settings.cpp — Settings (the application's) and Preferences (a database's).
 *
 * THE AUTHOR'S SPLIT (2026-09-25): "the global and local database settings
 * should be renamed to a 'preferences' tab, which contains 'local and global
 * preferences' ... A true 'settings' window will exist now, and that's where a
 * user's real settings go."
 *
 *   SETTINGS     this application, on this device: appearance, effects, the
 *                console, networking, updates, and which database it opens
 *                with. Never shared with anyone.
 *   PREFERENCES  a database's: LOCAL (where its files are, on this device) and
 *                GLOBAL (stored in the database, config-tier, logged, and shared
 *                with everyone in it: tag recommendations, hidden connections,
 *                the map, legacy tools).
 *
 * Appearance is still STORED in the database's config (ui.scale, ui.fx.*):
 * moving it to the device is a migration of its own, recorded in the OKF. */

#include "app/app_internal.hpp"
#include "app/lan_share.hpp" // the Networking section is Void Maiz's; its file is ours
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

#include "platform/app_settings.hpp"  // Settings > Starting Hormiga
#include "platform/device_paths.hpp"  // Preferences > where files are

void HormigaApp::draw_settings() {
    if (!show_settings) return;
    if (ImGui::Begin("Settings", &show_settings)) {
        ImGui::SeparatorText("Appearance");
        bool light = light_mode;
        if (ImGui::RadioButton("Light", light)) light = true;
        ImGui::SameLine();
        if (ImGui::RadioButton("Dark", !light)) light = false;
        if (light != light_mode) {
            light_mode = light;
            apply_theme();
        }
        // the member is the slider's storage (persists across frames — the
        // snap-back fix); the theme + config-set land once on release
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("UI scale", &ui_scale, 1.0f, 1.6f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            apply_theme();
            char cmd[64];
            std::snprintf(cmd, sizeof cmd, "config set ui.scale \"%.2f\"",
                          ui_scale);
            dispatch_and_reproject(cmd);
        }

        // ── Visual effects (UI/UX phase, 2026-08-03): the 2026-feel polish is
        // expected but costs compute, so each effect is separately opt-out, and
        // "Performance mode" clears them all in one click. Config ui.fx.*;
        // shadows/highlights re-theme immediately (apply_theme). ──────────────
        ImGui::SeparatorText("Visual effects");
        auto fx_row = [&](const char* label, bool& val, const char* key,
                          const char* help, bool retheme) {
            bool b = val;
            if (ImGui::Checkbox(label, &b)) {
                val = b;
                dispatch_and_reproject(std::string("config set ") + key + " \"" +
                                       (b ? "1" : "0") + "\"");
                if (retheme) apply_theme();
            }
            if (*help && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
        };
        fx_row("Drop shadows", fx_shadows, "ui.fx.shadows",
               "soft shadows behind panels and cards (depth)", false);
        fx_row("Accent highlights", fx_highlights, "ui.fx.highlights",
               "hover/selection glow toward the org's accent color", true);
        fx_row("Animations", gui_anim, "ui.animations",
               "drag-and-drop grow, transitions", false);
        fx_row("Translucency / blur", fx_blur, "ui.fx.blur",
               "frosted/translucent panels (e.g. Glass style)", false);
        if (ImGui::Button("Performance mode (all off)")) {
            fx_shadows = fx_highlights = fx_blur = gui_anim = false;
            dispatch_and_reproject("config set ui.fx.shadows \"0\"");
            dispatch_and_reproject("config set ui.fx.highlights \"0\"");
            dispatch_and_reproject("config set ui.fx.blur \"0\"");
            dispatch_and_reproject("config set ui.animations \"0\"");
            apply_theme();
        }
        ImGui::SameLine();
        if (ImGui::Button("All on")) {
            fx_shadows = fx_highlights = fx_blur = gui_anim = true;
            dispatch_and_reproject("config set ui.fx.shadows \"1\"");
            dispatch_and_reproject("config set ui.fx.highlights \"1\"");
            dispatch_and_reproject("config set ui.fx.blur \"1\"");
            dispatch_and_reproject("config set ui.animations \"1\"");
            apply_theme();
        }
        ImGui::TextDisabled("turn these off on a slower machine - the app stays\n"
                            "fully usable, just flatter and snappier");

        /* ── NETWORKING, IN ONE PLACE (stage B, 2026-09-19) ──────────────────
         * The author asked for it: "networking needs its own section in the
         * settings". Void Maiz draws it, so the sender's switches and the
         * receiver's switches stay visibly apart -- one hides things from you,
         * the other hides you from others -- and every Void application that
         * networks shows the same section. Saved beside the profile, because
         * these are this device's, not the database's. */
        /* ── THE CONSOLE, AND THE ONE PLACE A BUTTON IS BETTER THAN A SWITCH ──
         * The author: *"usually i would want things to be automatic, but in
         * this case settings are more sensitive and should require a couple
         * more steps"*. So the console's view settings are edited on a copy and
         * committed by Apply -- and a person can see what they are about to
         * change before it changes under them. Everything else in this window
         * still takes effect as you touch it, because everything else is one
         * switch with one visible consequence. */
        ImGui::SeparatorText(ICON_FA_TERMINAL "  Console");
        ImGui::Checkbox("Show the time each line arrived", &console_pending.timestamps);
        ImGui::TextDisabled("useful to a person reading along; wasted tokens to an\n"
                            "agent reading the transcript");
        ImGui::Checkbox("Show who said it (CRE / MAZ / PLB / ALM / HRG)",
                        &console_pending.sources);
        ImGui::TextDisabled("Void Core, Void Maiz, Void Palabra, Allomone, Void Hormiga");
        ImGui::Checkbox("Only what changed the database", &console_pending.only_changes);
        ImGui::TextDisabled("hides view chatter - the same rule as `copy condensed`");
        ImGui::Checkbox("Selectable text instead of coloured lines", &console_pending.as_text);
        const bool dirty = console_pending.timestamps != console.timestamps ||
                           console_pending.sources != console.sources ||
                           console_pending.only_changes != console.only_changes ||
                           console_pending.as_text != console.as_text;
        ImGui::BeginDisabled(!dirty);
        if (ImGui::Button("Apply console settings")) {
            console.timestamps = console_pending.timestamps;
            console.sources = console_pending.sources;
            console.only_changes = console_pending.only_changes;
            console.as_text = console_pending.as_text;
            dispatch_and_reproject(
                std::string("config set ui.console \"") +
                (console.timestamps ? "t" : "-") + (console.sources ? "s" : "-") +
                (console.only_changes ? "c" : "-") + (console.as_text ? "x" : "-") + "\"");
            toast("console settings applied");
        }
        ImGui::EndDisabled();
        if (dirty) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1), "not applied yet");
            ImGui::SameLine();
            if (ImGui::SmallButton("Discard")) console_pending = console;
        }

        ImGui::SeparatorText(ICON_FA_USERS "  Networking");
        if (maiz::draw_network_settings(net_settings)) LanRuntime::save_net_settings(*this);
        ImGui::TextDisabled("Kept on this computer (%s), not in the database.",
                            LanRuntime::net_settings_file().filename().string().c_str());

        /* ── STARTING HORMIGA: which database opens (the author, 2026-09-25):
         * "empty database" by default, or one the person chooses. Kept on this
         * device (platform/app_settings.hpp). A database named on the command
         * line always wins. */
        ImGui::SeparatorText(ICON_FA_DOOR_OPEN "  Starting Hormiga");
        {
            hormiga::app_settings::Settings as = hormiga::app_settings::load();
            const std::string shown = as.default_database.empty()
                                          ? std::string("Empty database")
                                          : std::filesystem::path(as.default_database).filename().string();
            ImGui::SetNextItemWidth(260);
            if (ImGui::BeginCombo("Open with", shown.c_str())) {
                if (ImGui::Selectable("Empty database", as.default_database.empty())) {
                    as.default_database.clear();
                    hormiga::app_settings::save(as);
                }
                if (!cur_miga.empty() && ImGui::Selectable(("This one: " + std::filesystem::path(cur_miga).filename().string()).c_str(),
                                                           as.default_database == cur_miga)) {
                    as.default_database = cur_miga;
                    hormiga::app_settings::save(as);
                }
                for (const auto& r : as.recent) {
                    if (r == cur_miga) continue;
                    if (ImGui::Selectable(std::filesystem::path(r).filename().string().c_str(), as.default_database == r)) {
                        as.default_database = r;
                        hormiga::app_settings::save(as);
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", r.c_str());
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Kept on this device, not in any database.");
        }

        ImGui::SeparatorText(ICON_FA_USER "  You");
        if (ImGui::Button("Profile...")) win_profile = true;
        ImGui::SameLine();
        ImGui::TextDisabled("your name, colour, picture and key");

        /* Updates. Last, and separated from everything above it on purpose:
         * every other knob in this window is `config set ui.*`, which is
         * config-tier and rides the saved org. An update preference is a fact
         * about THIS installation and is stored beside the install -- the
         * block says so, because a setting whose scope is invisible is a
         * setting people are surprised by later. */
        draw_update_settings();

        ImGui::SeparatorText("About");
        ImGui::TextWrapped("Settings are this application's, on this device. A database's own "
                           "preferences are under File > Preferences.");
    }
    ImGui::End();
}

void HormigaApp::draw_preferences() {
    if (!win_preferences) return;
    if (ImGui::Begin("Preferences", &win_preferences)) {
        // ── LOCAL: this database, on this device ─────────────────────────────
        ImGui::SeparatorText(ICON_FA_HARD_DRIVE "  This database, on this device");
        auto row = [](const char* label, const std::string& value) {
            ImGui::TextDisabled("%s", label);
            ImGui::SameLine(ImGui::CalcTextSize("Pictures and files  ").x + 30); // the longest label, and air
            ImGui::TextWrapped("%s", value.empty() ? "(none)" : value.c_str());
        };
        row("Name", cur_miga.empty() ? std::string("(not saved yet)")
                                     : std::filesystem::path(cur_miga).stem().string());
        row("Database file", cur_miga.empty() ? std::string("not saved yet: File > Save database as...") : cur_miga);
        row("Working copy", base_dir.string());
        row("Pictures and files", assets_dir().string());
        row("Backups", data_dir("backups").string());
        row("Joined databases go", hormiga::device::get().databases.string());
        if (on_open && ImGui::SmallButton("Show the working copy")) on_open(base_dir.string());
        ImGui::TextDisabled("Where things are kept on this device. The database file is what\n"
                            "you share and back up; the working copy is where Hormiga works.");

        // ── GLOBAL: stored in the database, shared with everyone in it ───────
        ImGui::SeparatorText(ICON_FA_USERS "  Everyone in this database");
        ImGui::TextDisabled("Stored in the database and shared with its members.");
        // ── Tag recommendations (author, 2026-08-05): when adding tags, suggest
        // tags over the tag co-occurrence graph. Three modes trade off HOW the
        // graph should grow (okf/concepts/allomone/tag-recommender.md). ────────
        ImGui::SeparatorText("Tag recommendations");
        ImGui::TextDisabled("when adding a tag, suggest tags based on the graph:");
        struct RecMode { const char* label; const char* help; };
        static const RecMode rmodes[] = {
            {"Similarity",
             "suggest tags that SIMILAR things already have - things alike get "
             "tagged alike (reinforces clusters)"},
            {"Dissimilarity",
             "suggest tags that make this thing DISTINCT from its neighbors - "
             "borrow from the far side of the graph (spreads things apart)"},
            {"Comprehensive",
             "suggest tags that CONNECT this thing to poorly-linked ones - the "
             "goal is that nothing is left stranded (knits the graph together)"}};
        for (int i = 0; i < 3; ++i) {
            if (ImGui::RadioButton(rmodes[i].label, tag_rec_mode == i)) {
                tag_rec_mode = i;
                tag_rec_key.clear(); // force a recompute next time the editor draws
                dispatch_and_reproject(
                    std::string("config set ui.tags.recommend_mode \"") +
                    std::to_string(i) + "\"");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", rmodes[i].help);
        }

        ImGui::SeparatorText("Advanced");
        bool legacy = show_legacy;
        if (ImGui::Checkbox("Show legacy tools", &legacy)) {
            show_legacy = legacy;
            dispatch_and_reproject(std::string("config set ui.show_legacy \"") +
                                   (legacy ? "1" : "0") + "\"");
        }
        ImGui::TextDisabled("the old Builder block-canvas (Scratch-style) +\n"
                            "its migrate/tidy tools - superseded by the\n"
                            "document canvas");

        ImGui::SeparatorText("Hidden connections (derived, not stored)");
        ImGui::TextWrapped(
            "Things placed on a map relate by DISTANCE. That relationship is "
            "computed, never written to the database - a hidden connection "
            "you can reveal:");
        bool sp = map_show_prox;
        if (ImGui::Checkbox("Show spatial proximity links on the map", &sp)) {
            map_show_prox = sp;
            dispatch_and_reproject(std::string("config set ui.map_proximity \"") +
                                   (sp ? "1" : "0") + "\"");
        }
        ImGui::BeginDisabled(!map_show_prox);
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Radius (m)", &map_prox_m, 100.0f, 2000.0f, "%.0f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char cmd[64];
            std::snprintf(cmd, sizeof cmd, "config set ui.map_proximity_m \"%.0f\"",
                          map_prox_m);
            dispatch_and_reproject(cmd);
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("closer = stronger; weight fades with distance");
        bool tp = time_prox;
        if (ImGui::Checkbox("Show temporal proximity (connections view)", &tp)) {
            time_prox = tp;
            dispatch_and_reproject(std::string("config set ui.time_proximity \"") +
                                   (tp ? "1" : "0") + "\"");
        }
        ImGui::BeginDisabled(!time_prox);
        ImGui::SetNextItemWidth(180);
        ImGui::SliderFloat("Window (days)", &time_prox_days, 1.0f, 90.0f, "%.0f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char cmd[64];
            std::snprintf(cmd, sizeof cmd, "config set ui.time_proximity_d \"%.0f\"",
                          time_prox_days);
            dispatch_and_reproject(cmd);
        }
        ImGui::EndDisabled();
        ImGui::TextDisabled("dated things (events, incidents) cluster by when\n"
                            "they happen - visible in the connections view");

        ImGui::SeparatorText("Map");
        ImGui::SetNextItemWidth(180);
        if (ImGui::BeginCombo("Base map", "Earth (OpenStreetMap)")) {
            ImGui::Selectable("Earth (OpenStreetMap)", true);
            ImGui::TextDisabled("image source (fantasy map...) - planned");
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("views share the base map; each view carries its\n"
                            "own camera, rules, and position channel");

    }
    ImGui::End();
}
