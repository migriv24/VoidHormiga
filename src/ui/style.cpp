/* ui/style.cpp — the Style tab: the theme, as a view of CONFIG.
 *
 * Lived in `section_map.cpp` until 2026-08-20, which is the clearest single
 * piece of evidence for this restructure: the theme editor sat inside the map
 * file for months, and when it was rewritten this session it was rewritten
 * THERE, because that is where it already was. Files grouped by which tab they
 * happened to grow in will keep placing code by accident.
 *
 * Nothing here holds state: every control emits one `config set theme.*`, so an
 * agent setting a key from a script and a person moving a slider are the same
 * change in the same log. The colour arithmetic lives in `app/app_shared.cpp`
 * where the RENDERER calls it — so the number this tab shows is by construction
 * the number the website uses. */

#include "app/app_internal.hpp"
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

void HormigaApp::draw_style_tab() {
    if (!win_style) return;
    ImGui::SetNextWindowSize(ImVec2(430, 640), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Style", &win_style)) {
        ImGui::TextDisabled("the theme both packs read - turn on Live preview\n"
                            "in the Builder to see changes as you make them");
        auto hexify = [](const float c[3], char* b, size_t n) {
            std::snprintf(b, n, "#%02x%02x%02x", (int)(c[0] * 255),
                          (int)(c[1] * 255), (int)(c[2] * 255));
        };
        auto hex_of = [&](const float c[3]) {
            char b[10];
            hexify(c, b, sizeof b);
            return std::string(b);
        };
        auto color_row = [&](const char* label, float col[3], const char* key) {
            ImGui::SetNextItemWidth(200);
            ImGui::ColorEdit3(label, col);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                char b[10];
                hexify(col, b, sizeof b);
                pending_cmds.push_back(std::string("config set ") + key +
                                       " \"" + b + "\"");
            }
        };
        auto int_combo = [&](const char* id, int& val, const char* const* items,
                             int n, const char* key) {
            ImGui::SetNextItemWidth(200);
            if (ImGui::Combo(id, &val, items, n))
                pending_cmds.push_back(std::string("config set ") + key + " \"" +
                                       std::to_string(val) + "\"");
        };
        auto labeled = [&](const char* text) {
            ImGui::TextDisabled("%s", text);
            ImGui::SameLine(118);
        };
        // a slider that commits ONE config command on release (not per frame —
        // a logged command per pixel of drag would be a useless journal)
        auto int_slider = [&](const char* id, int& val, int lo, int hi,
                              const char* key, const char* fmt) {
            ImGui::SetNextItemWidth(200);
            ImGui::SliderInt(id, &val, lo, hi, fmt);
            if (ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set ") + key + " \"" +
                                       std::to_string(val) + "\"");
        };

        if (ImGui::BeginTabBar("styletabs")) {

        // ── COLOR ───────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Color")) {
            color_row("Accent", theme_accent, "theme.accent");
            color_row("Accent 2", theme_accent2, "theme.accent2");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("a second accent - gradient bands and hover\n"
                                  "motion; leave equal to Accent for a solid look");
            color_row("Background", theme_bg, "theme.bg");
            color_row("Text", theme_ink, "theme.ink");

            /* ── THE LIGHT AND DARK PARTNERS ─────────────────────────────────
             * Derived by default so one chosen colour yields a coherent set,
             * and stated explicitly when an organization has real brand values.
             * Showing the derived swatches is what makes "derived" a choice
             * rather than a surprise. */
            ImGui::SeparatorText("Lighter & darker partners");
            const std::string acc = hex_of(theme_accent);
            const std::string lite = theme_accent_lite[0] ? theme_accent_lite
                                                          : shade(acc, 0.62);
            const std::string dark = theme_accent_dark[0] ? theme_accent_dark
                                                          : shade(acc, -0.38);
            auto swatch = [&](const std::string& hexs, const char* what) {
                float c[3];
                unsigned v = (unsigned)std::strtoul(hexs.c_str() + 1, nullptr, 16);
                c[0] = ((v >> 16) & 0xFF) / 255.f;
                c[1] = ((v >> 8) & 0xFF) / 255.f;
                c[2] = (v & 0xFF) / 255.f;
                ImGui::ColorButton(what, ImVec4(c[0], c[1], c[2], 1),
                                   ImGuiColorEditFlags_NoTooltip, ImVec2(34, 22));
                ImGui::SameLine();
                ImGui::TextDisabled("%s  %s", what, hexs.c_str());
            };
            swatch(lite, "lighter");
            swatch(acc, "accent ");
            swatch(dark, "darker ");
            ImGui::TextDisabled("used for tints, hovers and chips. Leave blank to\n"
                                "derive them from Accent; type a hex to pin one.");
            ImGui::SetNextItemWidth(96);
            if (ImGui::InputTextWithHint("##lite", "auto", theme_accent_lite,
                                         sizeof theme_accent_lite,
                                         ImGuiInputTextFlags_EnterReturnsTrue) ||
                ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set theme.accent_lite \"") +
                                       theme_accent_lite + "\"");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(96);
            if (ImGui::InputTextWithHint("##dark", "auto", theme_accent_dark,
                                         sizeof theme_accent_dark,
                                         ImGuiInputTextFlags_EnterReturnsTrue) ||
                ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set theme.accent_dark \"") +
                                       theme_accent_dark + "\"");
            ImGui::SameLine();
            ImGui::TextDisabled("lighter / darker");

            // ── the contrast floor, and the readout that justifies it ───────
            ImGui::SeparatorText("Readability");
            const char* levels[] = {"Off (use my colours exactly)",
                                    "AA - 4.5:1 (recommended)",
                                    "AAA - 7:1 (high contrast)"};
            labeled("Minimum");
            int_combo("##contrast", theme_contrast, levels, 3, "theme.contrast");
            const double target =
                theme_contrast == 2 ? 7.0 : (theme_contrast == 1 ? 4.5 : 1.0);
            const std::string bg = hex_of(theme_bg), ink = hex_of(theme_ink);
            /* THE PAIRINGS THE SITE ACTUALLY GENERATES — not a generic swatch
             * grid. Each row is a real place text lands on the built page, so a
             * red row is a specific thing a visitor would struggle to read. */
            struct Pair { const char* what; std::string fg, bg; };
            const Pair pairs[] = {
                {"body text on page", ink, bg},
                {"button label on accent", ink_on(acc), acc},
                {"text on accent band", ink_on(acc), acc},
                {"quiet text on page", meet_contrast("#6b6b6b", bg, target), bg},
                {"text on darker accent", ink_on(dark), dark},
            };
            ImGui::BeginTable("contrast", 3,
                              ImGuiTableFlags_SizingStretchProp |
                                  ImGuiTableFlags_RowBg);
            for (const Pair& pr : pairs) {
                const double r = contrast_ratio(pr.fg, pr.bg);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(pr.what);
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%.1f:1", r);
                ImGui::TableNextColumn();
                const bool pass = r >= (target > 1.0 ? target : 4.5);
                ImGui::TextColored(pass ? ImVec4(0.35f, 0.75f, 0.45f, 1)
                                        : ImVec4(0.90f, 0.55f, 0.30f, 1),
                                   pass ? "ok" : "low");
            }
            ImGui::EndTable();
            ImGui::TextWrapped(
                "The site picks black or white text for every generated "
                "background, so a light accent gets dark labels automatically. "
                "With a minimum set, quiet text is nudged until it clears it.");
            ImGui::EndTabItem();
        }

        // ── TYPE ────────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Type")) {
            labeled("Heading");
            int_combo("##font", theme_font, kFontLabels, kNumFonts, "theme.font");
            labeled("Body");
            int_combo("##bodyfont", theme_bodyfont, kFontLabels, kNumFonts,
                      "theme.bodyfont");
            const char* scales[] = {"Compact (90%)", "Snug (95%)", "Normal (100%)",
                                    "Roomy (108%)", "Large (118%)"};
            labeled("Scale");
            int_combo("##scale", theme_scale, scales, 5, "theme.scale");
            ImGui::TextDisabled("the first three are embedded (Inter, Source Serif,\n"
                                "Space Grotesk) - identical on every visitor's screen");

            ImGui::SeparatorText("Your organization's own typeface");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputTextWithHint("##fontcustom",
                                         "family name, e.g. Mandali",
                                         theme_font_custom,
                                         sizeof theme_font_custom,
                                         ImGuiInputTextFlags_EnterReturnsTrue) ||
                ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(
                    std::string("config set theme.font_custom \"") +
                    theme_font_custom + "\"");
            ImGui::TextWrapped(
                "Put the .woff2 files in a `fonts` folder beside this database, "
                "named like mandali-400.woff2 and mandali-700.woff2. They ship "
                "with the site; nothing is fetched from anyone else's server.");
            {   // say plainly whether the files are actually there
                std::error_code ec;
                int found = 0;
                for (const auto& de :
                     std::filesystem::directory_iterator(data_dir("fonts"), ec))
                    if (de.path().extension() == ".woff2") ++found;
                if (theme_font_custom[0]) {
                    if (found)
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 0.45f, 1),
                                           "%d font file(s) found", found);
                    else
                        ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.30f, 1),
                                           "no .woff2 in %s/fonts - the name will "
                                           "fall back",
                                           base_dir.filename().string().c_str());
                }
            }
            ImGui::EndTabItem();
        }

        // ── LAYOUT ──────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Layout")) {
            ImGui::SeparatorText("Preset (aesthetic mode)");
            const char* presets[] = {"Clean", "Soft (neumorphic)", "Bold (maximal)",
                                     "Editorial", "Glass"};
            int_combo("##preset", theme_preset, presets, 5, "theme.preset");
            ImGui::TextDisabled("Clean = cards + shadows; Soft = neumorphic;\n"
                                "Bold = hard edges, huge type; Editorial =\n"
                                "print-like hairlines; Glass = frosted cards");

            ImGui::SeparatorText("Shape & texture");
            const char* radii[] = {"Square (0)", "Subtle (6)", "Rounded (14)",
                                   "Soft (22)", "Pill (32)"};
            labeled("Corners");
            int_combo("##radius", theme_radius, radii, 5, "theme.radius");
            const char* textures[] = {"None", "Dots", "Grid", "Hatch"};
            labeled("Texture");
            int_combo("##texture", theme_texture, textures, 4, "theme.texture");

            /* ── SPACING AND ALIGNMENT, the design-tool axis ─────────────────
             * One spacing STEP for the whole page rather than a literal per
             * component: that is what a design tool's scale is for, and it is
             * why moving this changes the rhythm everywhere at once. */
            ImGui::SeparatorText("Grid rhythm");
            const char* gaps[] = {"Tight", "Snug", "Normal", "Roomy", "Airy"};
            labeled("Spacing");
            int_combo("##gap", theme_grid_gap, gaps, 5, "theme.grid_gap");
            bool even = theme_grid_even;
            if (ImGui::Checkbox("Equal heights across a row", &even)) {
                theme_grid_even = even;
                pending_cmds.push_back(
                    std::string("config set theme.grid_even \"") +
                    (even ? "1" : "0") + "\"");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "cards in the same row line up top and bottom, however much\n"
                    "text each one holds - the 'distribute' alignment from a\n"
                    "design tool. Off = each card is its own height.");

            ImGui::SeparatorText("Banner images");
            const char* filters[] = {"None (as photographed)", "Muted",
                                     "Black & white", "Warm", "Cool", "Soft blur"};
            labeled("Treatment");
            int_combo("##bfilter", theme_banner_filter, filters, 6,
                      "theme.banner_filter");
            labeled("Scrim");
            int_slider("##bdim", theme_banner_dim, 0, 100, "theme.banner_dim",
                       "%d%%");
            ImGui::TextWrapped(
                "A photo behind white text needs help. The treatment styles the "
                "image; the scrim darkens it just enough to read over. The source "
                "file is never changed - the same picture stays full colour in a "
                "gallery. Any hero or band can override both.");

            ImGui::SeparatorText("Icons");
            bool ic = theme_icons;
            if (ImGui::Checkbox("Icons beside dates, places and roles", &ic)) {
                theme_icons = ic;
                pending_cmds.push_back(std::string("config set theme.icons \"") +
                                       (ic ? "1" : "0") + "\"");
            }
            ImGui::TextDisabled("inline SVG, shipped with the site (Lucide, ISC)");
            ImGui::EndTabItem();
        }

        // ── SITE ────────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Site")) {
            ImGui::SeparatorText("Dark mode");
            if (ImGui::Checkbox("Follow the visitor's browser theme", &theme_dark))
                pending_cmds.push_back(std::string("config set theme.dark \"") +
                                       (theme_dark ? "1" : "0") + "\"");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("emits a prefers-color-scheme dark variant;\n"
                                  "quiet text is re-derived for the dark ground");

            ImGui::SeparatorText("Address & sharing");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##baseurl", "https://your-domain.org",
                                     site_base_url, sizeof site_base_url);
            if (ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set site.base_url \"") +
                                       site_base_url + "\"");
            if (!site_base_url[0])
                ImGui::TextColored(ImVec4(0.90f, 0.55f, 0.30f, 1),
                                   "without this there is no sitemap.xml");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##sitedesc",
                                     "default description (search + social)",
                                     site_desc_buf, sizeof site_desc_buf);
            if (ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set site.desc \"") +
                                       site_desc_buf + "\"");

            /* The colophon. It lives here rather than on the footer block
             * because it is site-wide — one line on every page — while the
             * footer block is per-page content. */
            ImGui::SeparatorText("Colophon");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##colophon", "Built with Void Hormiga",
                                     site_colophon_buf,
                                     sizeof site_colophon_buf);
            if (ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set site.colophon \"") +
                                       site_colophon_buf + "\"");
            ImGui::TextDisabled("the small line under the footer - leave empty "
                                "for the default,\ntype  none  to print no line "
                                "at all");

            ImGui::SeparatorText("Branding (this organization)");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##orgname", "organization name", org_name,
                                     sizeof org_name);
            if (ImGui::IsItemDeactivatedAfterEdit())
                pending_cmds.push_back(std::string("config set org.name \"") +
                                       org_name + "\"");
            ImGui::SetNextItemWidth(-70);
            ImGui::InputTextWithHint("##orglogo", "logo image path", org_logo_buf,
                                     sizeof org_logo_buf);
            ImGui::SameLine();
            if (ImGui::Button("Browse##logo") && on_pick_file) {
                std::string pth = on_pick_file(org_logo);
                if (!pth.empty()) {
                    std::snprintf(org_logo_buf, sizeof org_logo_buf, "%s",
                                  pth.c_str());
                    org_logo = pth;
                    pending_cmds.push_back(
                        std::string("config set org.logo \"") + pth + "\"");
                }
            }
            if (!org_logo.empty()) {
                HostTexture t = texture_for(org_logo);
                if (t.id) {
                    float hh = 40.0f;
                    ImGui::Image((ImTextureID)(intptr_t)t.id,
                                 ImVec2(hh * (float)t.w / std::max(1, t.h), hh));
                }
            }
            ImGui::TextDisabled("the site header and favicon, and a watermark on\n"
                                "map PNG exports");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        }

        /* ── EVERY KNOB HAS A COMMAND, AND THE TAB SAYS SO ────────────────────
         * The tab is one caller of the theme verbs, not the owner of them. A
         * person who wants to script a seasonal restyle, or hand one to an
         * agent, should be able to see the vocabulary rather than infer it. */
        ImGui::Separator();
        if (ImGui::TreeNode("Everything here is a command")) {
            ImGui::TextWrapped(
                "Each control below writes one `config set`. The same commands "
                "work in the console, in a --script batch, and from an agent - "
                "this tab has no private state.");
            ImGui::TextDisabled(
                "config set theme.accent \"#2e6b4f\"\n"
                "config set theme.accent_lite \"#a8cbb6\"   (blank = derived)\n"
                "config set theme.accent_dark \"#1d4230\"\n"
                "config set theme.contrast \"1\"            0 off 1 AA 2 AAA\n"
                "config set theme.font_custom \"Mandali\"\n"
                "config set theme.grid_gap \"2\"            0 tight .. 4 airy\n"
                "config set theme.grid_even \"1\"           equal-height rows\n"
                "config set theme.banner_filter \"1\"       0 none 1 mute 2 mono\n"
                "config set theme.banner_dim \"45\"         scrim, 0-100\n"
                "config set theme.icons \"1\"");
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

/* Settings — a dockable window; every knob is CONFIG-TIER state (`config set
 * ui.*`), so preferences are logged and ride the saved org like the cameras.
 * Home of the "hidden connections" switches: system-DERIVED relationships
 * (spatial proximity first) are invisible by default and turned on here. */
