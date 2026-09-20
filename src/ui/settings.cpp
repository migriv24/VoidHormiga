/* ui/settings.cpp — the Settings window.
 *
 * Also lived in the map file. Every knob is config-tier (`config set ui.*`), so
 * preferences are logged and ride the saved org exactly like the cameras. */

#include "app/app_internal.hpp"
#include "app/lan_share.hpp" // the Networking section is Void Maiz's; its file is ours
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

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

        /* ── NETWORKING, IN ONE PLACE (stage B, 2026-09-19) ──────────────────
         * The author asked for it: "networking needs its own section in the
         * settings". Void Maiz draws it, so the sender's switches and the
         * receiver's switches stay visibly apart -- one hides things from you,
         * the other hides you from others -- and every Void application that
         * networks shows the same section. Saved beside the profile, because
         * these are this device's, not the database's. */
        ImGui::SeparatorText(ICON_FA_USERS "  Networking");
        if (maiz::draw_network_settings(net_settings)) LanRuntime::save_net_settings(*this);
        ImGui::TextDisabled("Kept on this computer (%s), not in the database.",
                            LanRuntime::net_settings_file().filename().string().c_str());

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

        /* Updates. Last, and separated from everything above it on purpose:
         * every other knob in this window is `config set ui.*`, which is
         * config-tier and rides the saved org. An update preference is a fact
         * about THIS installation and is stored beside the install -- the
         * block says so, because a setting whose scope is invisible is a
         * setting people are surprised by later. */
        draw_update_settings();

        ImGui::SeparatorText("About");
        ImGui::TextWrapped("Hormiga - local-first outreach. Settings live in "
                           "the org's config tier (logged, undo-exempt).");
    }
    ImGui::End();
}
