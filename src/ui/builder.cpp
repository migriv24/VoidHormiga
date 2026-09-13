/* section_builder.cpp — the Builder section (palette | block canvas |
 * inspector | preview) and the Antfarm placeholder. Split out of app.cpp
 * 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * The `"block"` shape kind, snap/tear/heal and face-widget arguments, against
 * the reference implementation in ../NodeBlocks/src/main.cpp. Every palette
 * click and every drag is a `doc` verb, so the CLI stays complete.
 */
#include "app/app_internal.hpp"
/* The preview must resolve a query EXACTLY as the renderer will, `date:`
 * predicates included — a Builder that shows a different set from the page is
 * worse than one that shows nothing. */
#include "domain/date_query.hpp"
#include "render/download.hpp" // the refusal list, so the canvas can warn
#include "render/video.hpp" // the preview says which video, not just "video"
#include "json.hpp" // block payloads and template bodies are JSON

// ── the Builder section: palette | block canvas | inspector, + preview ──────

/* Append a new component to the document via the `doc place` verb — the
 * palette's click IS the verb (one definition, builder.md B3). */
void HormigaApp::doc_palette_place(const std::string& glyph) {
    std::string name;
    for (int i = 1;; ++i) {
        name = glyph + "-" + std::to_string(i);
        if (!scene.find(name)) break;
    }
    int maxrow = -1;
    for (const auto& n : scene.nodes)
        maxrow = std::max(maxrow, hormiga::doc_field_int(n, "row", -1));
    pending_cmds.push_back("doc place " + glyph + " " + name + " " +
                           std::to_string(maxrow + 1));
    // W2: the new component belongs to the page being edited (home = "")
    if (!cur_page.empty())
        pending_cmds.push_back("set " + name + " page \"" + cur_page + "\"");
    ed.selection = {name};
}


/* Mint a website PAGE (W2). The first "New page" also establishes a Home page
 * so the existing (empty-`page`) components have a named home; every later
 * click adds one more. Switches the canvas to the new page. */
void HormigaApp::doc_new_page() {
    std::vector<std::string> cmds;
    int cnt = 0;
    for (const auto& n : scene.nodes)
        if (n.glyph == "page") ++cnt;
    if (cnt == 0) { // home holds everything already on the (single) document
        cmds.push_back("rune new page home");
        cmds.push_back("set home slug \"home\"");
        cmds.push_back("set home title_en \"Home\"");
        cmds.push_back("set home order \"0\"");
        cmds.push_back("set home in_nav \"1\"");
        cnt = 1;
    }
    std::string slug;
    for (int i = 1;; ++i) {
        slug = "page-" + std::to_string(i);
        if (!scene.find(slug)) break;
    }
    cmds.push_back("rune new page " + slug);
    cmds.push_back("set " + slug + " slug \"" + slug + "\"");
    cmds.push_back("set " + slug + " title_en \"New Page\"");
    cmds.push_back("set " + slug + " order \"" + std::to_string(cnt) + "\"");
    // unconnected by default (author: nav is authored, not auto) — add a link
    // to it, or toggle "in header nav" in the page manager
    cmds.push_back("set " + slug + " in_nav \"0\"");
    pending_cmds.push_back(maiz::compile_commit(cmds));
    cur_page = slug;
    toast("new page '" + slug + "' - add components; rename/re-slug it in "
          "the inspector");
}

/* The DOCUMENT CANVAS (builder.md B3, QD decided: "100% hold shape and
 * form"). A vertical page of rows; components render near-WYSIWYG — real
 * text, real image THUMBNAILS, query-backed grids showing their GENERATED
 * results. Click = select (the shared inspector); right-click = the action
 * menu; drag = reorder (drop between rows) or PAIR (drop on a row's half);
 * the right-edge grip resizes span. Reorder/pair commit as ONE batch of
 * `set row/col/span` (logged, one undo frame); single gestures ride the
 * `doc` verbs. Rows cap at two components in v1 (QA slots; 3-4 later). */
void HormigaApp::draw_document_canvas(float body_h) {
    // auto-migrate a legacy chain once, so the canvas always has rows
    if (!doc_migrate_queued) {
        for (const auto& n : scene.nodes)
            if (n.glyph != "document" &&
                hormiga::doc_field_int(n, "row", -1) < 0) {
                pending_cmds.push_back("doc migrate");
                doc_migrate_queued = true;
                break;
            }
    }
    // the data mantle: query-backed components preview their real results
    maiz::ProjectOptions dio;
    dio.mantle = kDataMantle;
    maiz::Scene data = maiz::project_scene(core, dio);
    std::string suf = preview_lang ? "_es" : "_en";
    std::string alt = preview_lang ? "_en" : "_es";
    auto text_of = [&](const maiz::SceneNode& n, const char* base) {
        std::string v = field_value(n, base + suf);
        return v.empty() ? field_value(n, base + alt) : v;
    };
    // in-place text editing (author ask 6b: "double click and edit text right
    // on the preview"). Renders an InputText when this node/field is being
    // edited (double-click starts it), else the static text; commits on Enter
    // (single-line) or defocus, Escape cancels. Edits the CURRENT language.
    auto editable = [&](const maiz::SceneNode& n, const char* base, bool multi,
                        float w) {
        bool editing = doc_edit_node == n.name && doc_edit_base == base;
        if (!editing) {
            ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + w);
            ImGui::TextUnformatted(text_of(n, base).c_str());
            ImGui::PopTextWrapPos();
            return;
        }
        if (doc_edit_focus) {
            ImGui::SetKeyboardFocusHere();
            doc_edit_focus = false;
        }
        ImGui::SetNextItemWidth(w);
        bool done = false;
        if (multi)
            ImGui::InputTextMultiline(("##ed" + n.name).c_str(), doc_edit_buf,
                                      sizeof doc_edit_buf, ImVec2(w, 64));
        else
            done = ImGui::InputText(("##ed" + n.name).c_str(), doc_edit_buf,
                                    sizeof doc_edit_buf,
                                    ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { // cancel
            doc_edit_node.clear();
            return;
        }
        if (done || ImGui::IsItemDeactivatedAfterEdit()) { // commit
            pending_cmds.push_back("set " + n.name + " " + base + suf + " " +
                                   json_str(doc_edit_buf));
            doc_edit_node.clear();
        }
    };
    // which text field a glyph edits in place (null = not text-editable here)
    auto edit_base_of = [](const std::string& g) -> const char* {
        if (g == "hero" || g == "section_header") return "title";
        if (g == "narrative" || g == "footer") return "text";
        return nullptr; // link edits via the inspector (it has the target UI)
    };
    ImU32 acc_col = ImGui::ColorConvertFloat4ToU32(
        ImVec4(theme_accent[0], theme_accent[1], theme_accent[2], 1.0f));

    // ── PAGE tab-strip (W2): switch which page the canvas edits; "+ Page"
    // mints one. No pages yet = a single implicit document (the strip shows
    // just "+ Page" to begin). Home = the page ordered 0 (holds empty-`page`
    // components). ─────────────────────────────────────────────────────────
    std::vector<const maiz::SceneNode*> page_runes;
    for (const auto& n : scene.nodes)
        if (n.glyph == "page") page_runes.push_back(&n);
    std::sort(page_runes.begin(), page_runes.end(),
              [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                  return hormiga::doc_field_int(*a, "order", 0) <
                         hormiga::doc_field_int(*b, "order", 0);
              });
    std::string home_slug =
        page_runes.empty() ? "" : field_value(*page_runes.front(), "slug");
    if (!page_runes.empty()) {
        // a slim SWITCHER (management lives in the nothing-selected panel, like
        // the map's "On this map"); tabs just change which page you're editing
        bool ok = cur_page.empty();
        for (auto* p : page_runes)
            if (field_value(*p, "slug") == cur_page) ok = true;
        if (!ok || cur_page.empty()) cur_page = home_slug;
        ImGui::TextDisabled("page:");
        for (size_t i = 0; i < page_runes.size(); ++i) {
            std::string slug = field_value(*page_runes[i], "slug");
            std::string lbl = text_of(*page_runes[i], "title");
            if (lbl.empty()) lbl = slug;
            ImGui::SameLine();
            bool active = (slug == cur_page);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, acc_col);
            if (ImGui::SmallButton((lbl + "##pg" + slug).c_str()))
                cur_page = slug;
            if (active) ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(manage pages: click empty space to deselect)");
        ImGui::Separator();
    }

    // group THIS PAGE's components into rows (a component is on cur_page when
    // its `page` == cur_page, or it's unassigned and cur_page is home)
    auto on_page = [&](const maiz::SceneNode& n) {
        if (page_runes.empty()) return true; // single-document mode
        std::string pg = field_value(n, "page");
        return pg == cur_page || (pg.empty() && cur_page == home_slug);
    };
    auto order = hormiga::doc_order(scene);
    std::vector<std::vector<const maiz::SceneNode*>> rows;
    int last_row = INT_MIN;
    for (const auto* n : order) {
        if (n->glyph == "page" || !on_page(*n)) continue;
        int r = hormiga::doc_field_int(*n, "row", -1);
        if (rows.empty() || r != last_row || r < 0) rows.push_back({});
        rows.back().push_back(n);
        last_row = r;
    }

    ensure_icon_editor();                                     // item 4
    if (ed.selection.size() > 1) draw_multi_select_panel();   // item 1
    ImGui::BeginChild("##doccanvas", ImVec2(0, body_h));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float avail = ImGui::GetContentRegionAvail().x;
    float page_w = std::min(avail - 28.0f, 760.0f);
    float page_x = ImGui::GetCursorScreenPos().x + (avail - page_w) * 0.5f;
    ImVec2 mouse = ImGui::GetIO().MousePos;

    struct Card { const maiz::SceneNode* n; ImVec2 tl, br; int row_i; };
    std::vector<Card> cards;
    std::vector<float> row_tops;

    auto card_h = [&](const maiz::SceneNode& n) -> float {
        if (n.glyph == "hero") return field_value(n, "image").empty() ? 74.0f : 128.0f;
        if (n.glyph == "section_header") return 46.0f;
        if (n.glyph == "link") return 48.0f;
        if (n.glyph == "quote") return 88.0f;
        if (n.glyph == "stat") return 66.0f;
        if (n.glyph == "divider") return 30.0f;
        if (n.glyph == "narrative") return 96.0f;
        if (n.glyph == "footer") return 54.0f;
        if (n.glyph == "image_grid") return 116.0f;
        if (n.glyph == "map_embed") return 132.0f;
        if (n.glyph == "calendar_embed") return 124.0f;
        if (n.glyph == "download") return 92.0f;
        if (n.glyph == "audio") return 128.0f;
        if (n.glyph == "video") return 120.0f;
        if (n.glyph == "image_text") return 128.0f;
        if (n.glyph == "event_flier" || n.glyph == "event_feature") return 132.0f;
        return 108.0f; // event_grid / job_grid / anything new
    };

    // ── the page, row by row ────────────────────────────────────────────────
    ImGui::Dummy(ImVec2(1, 6));
    for (size_t ri = 0; ri < rows.size(); ++ri) {
        float y0 = ImGui::GetCursorScreenPos().y;
        row_tops.push_back(y0);
        float rh = 0;
        for (const auto* n : rows[ri]) rh = std::max(rh, card_h(*n));
        // BAND background preview (WYSIWYG): a tinted panel behind the row when
        // its leader carries a band_bg (full-bleed spills past the page edges)
        std::string bbg = field_value(*rows[ri][0], "band_bg");
        bool bfull = field_value(*rows[ri][0], "band_full") == "1";
        std::string bimg = field_value(*rows[ri][0], "band_image");
        if (!bimg.empty() || (!bbg.empty() && bbg != "none") || bfull) {
            ImU32 bc = !bimg.empty()      ? IM_COL32(40, 40, 48, 255) // photo→dark
                       : bbg == "accent"  ? acc_col
                       : bbg == "gradient" ? acc_col
                       : bbg == "dark"    ? IM_COL32(28, 28, 32, 255)
                       : bbg == "card"    ? IM_COL32(248, 247, 244, 255)
                                          : (acc_col & 0x00FFFFFF) | 0x22000000;
            float pad = 10.0f, margin = (bfull || !bimg.empty()) ? 20.0f : 0.0f;
            dl->AddRectFilled(ImVec2(page_x - margin - pad, y0 - pad),
                              ImVec2(page_x + page_w + margin + pad, y0 + rh + pad),
                              bc, (bfull || !bimg.empty()) ? 0.0f : 8.0f);
            // a photo band: try to draw the actual image faded behind the row
            if (!bimg.empty()) {
                HostTexture t = texture_for(bimg);
                if (t.id)
                    dl->AddImage((ImTextureID)(intptr_t)t.id,
                                 ImVec2(page_x - margin - pad, y0 - pad),
                                 ImVec2(page_x + page_w + margin + pad, y0 + rh + pad),
                                 ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 90));
            }
        }
        float x = page_x;
        for (size_t ci = 0; ci < rows[ri].size(); ++ci) {
            const maiz::SceneNode& n = *rows[ri][ci];
            int span = std::clamp(hormiga::doc_field_int(n, "span", 12), 1, 12);
            float w = page_w * span / 12.0f - (rows[ri].size() > 1 ? 4.0f : 0.0f);
            ImVec2 tl(x, y0), br(x + w, y0 + rh);
            bool sel = ed.selected(n.name);
            // card chrome
            dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 255), 6.0f);
            dl->AddRect(tl, br,
                        sel ? acc_col : IM_COL32(205, 202, 194, 255), 6.0f, 0,
                        sel ? 2.5f : 1.0f);
            // content (near-WYSIWYG per kind)
            ImGui::SetCursorScreenPos(ImVec2(tl.x + 10, tl.y + 8));
            ImGui::BeginGroup();
            ImGui::PushClipRect(tl, br, true);
            float inner_w = w - 20;
            if (n.glyph == "hero") {
                dl->AddRectFilled(tl, ImVec2(br.x, tl.y + 5), acc_col, 6.0f);
                ImGui::Dummy(ImVec2(1, 2));
                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.5f);
                editable(n, "title", false, inner_w);
                ImGui::PopFont();
                std::string img = field_value(n, "image");
                if (!img.empty()) {
                    HostTexture t = texture_for(img);
                    if (t.id) {
                        float ih = 58.0f, iw = ih * (float)t.w / std::max(1, t.h);
                        ImGui::Image((ImTextureID)(intptr_t)t.id,
                                     ImVec2(std::min(iw, inner_w), ih));
                    }
                }
            } else if (n.glyph == "section_header") {
                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.25f);
                editable(n, "title", false, inner_w);
                ImGui::PopFont();
                float uy = ImGui::GetCursorScreenPos().y + 1;
                dl->AddLine(ImVec2(tl.x + 10, uy), ImVec2(br.x - 10, uy),
                            acc_col, 2.0f);
            } else if (n.glyph == "narrative") {
                editable(n, "text", true, inner_w);
            } else if (n.glyph == "link") {
                // a pill button preview; shows its target for at-a-glance flow
                std::string lbl = text_of(n, "label");
                if (lbl.empty()) lbl = "(link)";
                std::string tgt = field_value(n, "target");
                ImVec2 tsz = ImGui::CalcTextSize(lbl.c_str());
                dl->AddRectFilled(ImVec2(tl.x + 10, tl.y + 12),
                                  ImVec2(tl.x + 26 + tsz.x, tl.y + 36), acc_col,
                                  6.0f);
                dl->AddText(ImVec2(tl.x + 18, tl.y + 16),
                            IM_COL32(255, 255, 255, 255), lbl.c_str());
                std::string arrow = tgt.empty() ? "-> (set a target)" : "-> " + tgt;
                dl->AddText(ImVec2(tl.x + 34 + tsz.x, tl.y + 16),
                            IM_COL32(140, 140, 145, 255), arrow.c_str());
            } else if (n.glyph == "quote") {
                dl->AddText(ImVec2(tl.x + 6, tl.y + 2),
                            (acc_col & 0x00FFFFFF) | 0x66000000, "\"");
                ImGui::SetCursorScreenPos(ImVec2(tl.x + 22, tl.y + 10));
                ImGui::PushTextWrapPos(tl.x + 10 + inner_w);
                ImGui::TextUnformatted(text_of(n, "text").c_str());
                ImGui::PopTextWrapPos();
                std::string au = field_value(n, "author");
                if (!au.empty()) ImGui::TextDisabled("- %s", au.c_str());
            } else if (n.glyph == "stat") {
                std::string num = field_value(n, "number");
                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.7f);
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(theme_accent[0], theme_accent[1],
                                             theme_accent[2], 1.0f));
                ImGui::TextUnformatted(num.empty() ? "0" : num.c_str());
                ImGui::PopStyleColor();
                ImGui::PopFont();
                ImGui::TextDisabled("%s", text_of(n, "label").c_str());
            } else if (n.glyph == "divider") {
                std::string ds = field_value(n, "divider_style");
                float my = tl.y + rh * 0.5f;
                if (ds == "space") {
                    /* nothing */
                } else if (ds == "dots") {
                    for (float x = tl.x + 20; x < br.x - 20; x += 14)
                        dl->AddCircleFilled(ImVec2(x, my), 2.0f,
                                            IM_COL32(170, 168, 160, 255));
                } else {
                    dl->AddLine(ImVec2(tl.x + 12, my), ImVec2(br.x - 12, my),
                                IM_COL32(205, 202, 194, 255), 1.5f);
                }
            } else if (n.glyph == "footer") {
                editable(n, "text", true, inner_w);
            } else if (n.glyph == "image_grid") {
                // the GENERATED rectangles, with mini previews (QD verbatim)
                std::string q = field_value(n, "query");
                int shown = 0, hits = 0;
                for (const auto& dn : data.nodes) {
                    if (dn.glyph != "image" || !hormiga::query_matches(q, data, dn) ||
                        allo_web_hidden(dn.name))
                        continue;
                    ++hits;
                    if ((shown + 1) * 92.0f > inner_w) continue;
                    HostTexture t = texture_for(field_value(dn, "path"));
                    if (t.id) {
                        ImGui::Image((ImTextureID)(intptr_t)t.id,
                                     ImVec2(84, 84));
                        ImGui::SameLine();
                        ++shown;
                    }
                }
                ImGui::NewLine();
                std::string mode = field_value(n, "display");
                if (mode.empty()) mode = "grid";
                ImGui::TextDisabled("image grid (%s): %d match @%s", mode.c_str(),
                                    hits, q.empty() ? "(all)" : q.c_str());
            } else if (n.glyph == "event_grid" || n.glyph == "job_grid") {
                bool ev = n.glyph == "event_grid";
                std::string q = field_value(n, "query");
                int hits = 0;
                for (const auto& dn : data.nodes) {
                    if (dn.glyph != (ev ? "event" : "job") ||
                        !hormiga::query_matches(q, data, dn) ||
                        allo_web_hidden(dn.name))
                        continue;
                    ++hits;
                    if (hits <= 3) {
                        ImU32 bar = ev ? IM_COL32(63, 111, 174, 255)
                                       : IM_COL32(93, 125, 59, 255);
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        dl->AddRectFilled(p, ImVec2(p.x + 3, p.y + 15), bar);
                        ImGui::Dummy(ImVec2(7, 0));
                        ImGui::SameLine();
                        std::string line = humanize(dn.name);
                        if (ev) {
                            std::string d = field_value(dn, "date");
                            if (!d.empty()) line += "  " + d;
                        }
                        ImGui::TextUnformatted(line.c_str());
                    }
                }
                ImGui::TextDisabled("%s: %d match", ev ? "events" : "jobs", hits);
            } else if (n.glyph == "directory") {
                /* THE ONE BLOCK WHOSE QUERY IS NOT THE ANSWER, so the preview
                 * has to say both numbers. A person authoring a member
                 * directory needs to see "12 listed, 71 withheld" while they
                 * author it — not to discover it from a deployed page, and not
                 * to be told a block is broken when it is doing its job. */
                std::string q = field_value(n, "query");
                std::string kind = field_value(n, "kind");
                if (kind.empty()) kind = "contact";
                int listed = 0, withheld = 0;
                for (const auto& dn : data.nodes) {
                    const bool is_c = dn.glyph == "contact";
                    const bool is_o = dn.glyph == "organization";
                    if (!is_c && !is_o) continue;
                    if (kind == "contact" && !is_c) continue;
                    if (kind == "organization" && !is_o) continue;
                    if (!q.empty() && !hormiga::query_matches(q, data, dn)) continue;
                    if (allo_web_hidden(dn.name)) continue;
                    if (!maiz::node_matches("clearance:public", dn)) { ++withheld; continue; }
                    ++listed;
                    if (listed <= 3) {
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        dl->AddRectFilled(p, ImVec2(p.x + 3, p.y + 15),
                                          IM_COL32(179, 89, 46, 255));
                        ImGui::Dummy(ImVec2(7, 0));
                        ImGui::SameLine();
                        std::string dnm = field_value(dn, "display_name");
                        if (dnm.empty()) dnm = humanize(dn.name);
                        ImGui::TextUnformatted(dnm.c_str());
                    }
                }
                ImGui::TextDisabled("directory: %d listed, %d withheld "
                                    "(only clearance:public is published)",
                                    listed, withheld);
            } else if (n.glyph == "map_embed") {
                // a STATIC map (QD): the newest export of the referenced view
                std::string vn = field_value(n, "view");
                if (vn.empty()) vn = "earth-map";
                auto it = doc_map_thumb.find(vn);
                if (it == doc_map_thumb.end()) { // scan once per view/session
                    std::string best;
                    std::error_code ec;
                    for (auto& e :
                         fs::directory_iterator(data_dir("exports"), ec)) {
                        std::string f = e.path().filename().string();
                        if (f.rfind(vn, 0) == 0 && f > best) best = f;
                    }
                    it = doc_map_thumb.emplace(vn, best).first;
                }
                HostTexture t = it->second.empty()
                                    ? HostTexture{}
                                    : texture_for("exports/" + it->second);
                if (t.id) {
                    float ih = 92.0f, iw = ih * (float)t.w / std::max(1, t.h);
                    ImGui::Image((ImTextureID)(intptr_t)t.id,
                                 ImVec2(std::min(iw, inner_w), ih));
                } else {
                    ImGui::TextUnformatted(ICON_FA_LOCATION_DOT);
                    ImGui::SameLine();
                    ImGui::TextDisabled("map (no export yet - render once)");
                }
                ImGui::TextDisabled("map of view '%s' - interactive on the "
                                    "site", vn.c_str());
            } else if (n.glyph == "calendar_embed") {
                // a mini month sketch: entry days tick in their kind's color
                int ty2, tm3, td3;
                cal_today(ty2, tm3, td3);
                int first = cal_dow(ty2, tm3, 1), dim = cal_dim(ty2, tm3);
                float cw = std::min(inner_w / 7.0f, 26.0f), chh = 13.0f;
                ImVec2 g0 = ImGui::GetCursorScreenPos();
                for (int d = 1; d <= dim; ++d) {
                    int cell = first + d - 1;
                    ImVec2 p(g0.x + (cell % 7) * cw, g0.y + (cell / 7) * chh);
                    dl->AddRect(p, ImVec2(p.x + cw - 2, p.y + chh - 2),
                                IM_COL32(215, 212, 205, 255));
                    for (const auto& dn : data.nodes) {
                        int yy, mm, dd;
                        if (std::sscanf(field_value(dn, "date").c_str(),
                                        "%d-%d-%d", &yy, &mm, &dd) != 3 ||
                            yy != ty2 || mm != tm3 || dd != d)
                            continue;
                        dl->AddRectFilled(
                            ImVec2(p.x + 2, p.y + 2),
                            ImVec2(p.x + cw - 4, p.y + chh - 4),
                            dn.glyph == "incident" ? IM_COL32(200, 50, 50, 230)
                                                   : IM_COL32(63, 111, 174, 230));
                        break;
                    }
                }
                ImGui::Dummy(ImVec2(1, ((first + dim + 6) / 7) * chh + 4));
                ImGui::TextDisabled("%s %d - interactive on the site",
                                    kMonthNames[tm3 - 1], ty2);
            } else if (n.glyph == "download") {
                /* The file's presence is the thing to see before deploying: a
                 * `download` whose file is missing leaves NO button on the page
                 * at all, so a canvas that drew a confident button would be
                 * lying about the most important control on some sites. */
                std::string want = field_value(n, "file");
                std::string via;
                if (!want.empty())
                    for (const auto& dn : data.nodes)
                        if (dn.glyph == "resource" && dn.name == want) {
                            via = want;
                            want = field_value(dn, "path");
                            break;
                        }
                const bool refused =
                    !want.empty() && !hormiga::download_refusal(want).empty();
                const bool have =
                    !want.empty() && !refused &&
                    std::filesystem::exists(
                        std::filesystem::path(want).is_absolute()
                            ? std::filesystem::path(want)
                            : base_dir / want);
                std::string lbl = text_of(n, "label");
                if (lbl.empty()) lbl = "Download";
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                const float bw = std::min(inner_w, 210.0f), bh = 30.0f;
                dl->AddRectFilled(p0, ImVec2(p0.x + bw, p0.y + bh),
                                  have ? IM_COL32(46, 107, 79, 255)
                                       : IM_COL32(110, 60, 60, 255),
                                  6.0f);
                dl->AddText(ImVec2(p0.x + 10, p0.y + 7),
                            IM_COL32(240, 240, 240, 255),
                            (std::string(ICON_FA_DOWNLOAD) + "  " + lbl).c_str());
                ImGui::Dummy(ImVec2(1, bh + 4));
                if (want.empty())
                    ImGui::TextDisabled("(no file yet)");
                else if (refused)
                    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.45f, 1),
                                       "%s - refused: not publishable",
                                       want.c_str());
                else if (!have)
                    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.45f, 1),
                                       "%s - file not found", want.c_str());
                else
                    ImGui::TextDisabled("%s%s", want.c_str(),
                                        via.empty() ? ""
                                                    : "  (resource)");
            } else if (n.glyph == "audio") {
                /* A cover, the title line, and a transport bar drawn to scale.
                 * The one thing an author needs to see before deploying is
                 * whether the FILE is actually there — a block whose `src`
                 * points at something not on this machine renders an apology on
                 * the page, and finding that out from the deployed site is the
                 * failure the `video` preview two cases up was built to end.
                 * So the bar is warm-grey when the file resolves and red when
                 * it does not, and the line underneath says which. */
                const std::string asrc = field_value(n, "src");
                const bool have =
                    !asrc.empty() &&
                    std::filesystem::exists(std::filesystem::path(asrc).is_absolute()
                                                ? std::filesystem::path(asrc)
                                                : base_dir / asrc);
                const float ch = 46.0f;
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                // the cover, if the named image rune has a file we can draw
                float x = p0.x;
                const std::string cov = field_value(n, "cover");
                if (!cov.empty()) {
                    for (const auto& dn : data.nodes)
                        if (dn.glyph == "image" && dn.name == cov) {
                            HostTexture t = texture_for(field_value(dn, "path"));
                            if (t.id)
                                dl->AddImage((ImTextureID)(intptr_t)t.id,
                                             ImVec2(x, p0.y),
                                             ImVec2(x + ch, p0.y + ch));
                            else
                                dl->AddRect(ImVec2(x, p0.y),
                                            ImVec2(x + ch, p0.y + ch),
                                            IM_COL32(120, 120, 128, 160), 4.0f);
                            x += ch + 8;
                        }
                }
                const float bw = std::max(60.0f, std::min(inner_w - (x - p0.x),
                                                          200.0f));
                dl->AddRectFilled(ImVec2(x, p0.y + ch - 16),
                                  ImVec2(x + bw, p0.y + ch - 6),
                                  have ? IM_COL32(90, 90, 98, 255)
                                       : IM_COL32(120, 60, 60, 255),
                                  5.0f);
                dl->AddCircleFilled(ImVec2(x + 10, p0.y + ch - 11), 6.0f,
                                    have ? IM_COL32(200, 200, 210, 255)
                                         : IM_COL32(210, 150, 150, 255));
                ImGui::Dummy(ImVec2(1, ch + 2));
                const std::string at = text_of(n, "title");
                ImGui::TextDisabled(
                    "%s %s", ICON_FA_MUSIC,
                    at.empty() ? (asrc.empty() ? "(no file yet)" : asrc.c_str())
                               : at.c_str());
                if (!asrc.empty() && !have) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.85f, 0.45f, 0.45f, 1),
                                       "- file not found");
                }
            } else if (n.glyph == "video") {
                /* A play mark and the RESOLVED provider + id. The preview says
                 * whether the pasted string parsed, which is the one thing an
                 * author needs to know before deploying: a `link` block that
                 * does not parse still renders a button, and a `video` block
                 * that does not parse renders an apology. Better to see that
                 * here. */
                const hormiga::VideoRef v =
                    hormiga::parse_video_url(field_value(n, "url"));
                const float bw = std::min(inner_w, 168.0f), bh = 66.0f;
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                dl->AddRectFilled(p0, ImVec2(p0.x + bw, p0.y + bh),
                                  v.ok() ? IM_COL32(38, 38, 42, 255)
                                         : IM_COL32(70, 52, 52, 255),
                                  4.0f);
                const ImVec2 c(p0.x + bw * 0.5f, p0.y + bh * 0.5f);
                dl->AddTriangleFilled(ImVec2(c.x - 7, c.y - 10),
                                      ImVec2(c.x - 7, c.y + 10),
                                      ImVec2(c.x + 11, c.y),
                                      IM_COL32(235, 233, 228, 235));
                ImGui::Dummy(ImVec2(1, bh + 4));
                if (v.ok())
                    ImGui::TextDisabled("%s  %s", v.provider_label().c_str(),
                                        v.id.c_str());
                else if (field_value(n, "url").empty())
                    ImGui::TextDisabled("video: paste a YouTube or Vimeo link");
                else
                    ImGui::TextDisabled("video: link not recognised");
            } else if (!draw_block_preview(n, data, inner_w, acc_col)) {
                ImGui::TextDisabled("%s", n.glyph.c_str());
            }
            ImGui::PopClipRect();
            ImGui::EndGroup();
            ImGui::PushID(n.name.c_str());
            // while editing THIS card in place, skip the interaction overlay so
            // the InputText underneath receives clicks + keys (author ask 6b)
            bool editing_this = (doc_edit_node == n.name);
            if (!editing_this) {
            // interaction overlay (submitted last = wins hover)
            ImGui::SetCursorScreenPos(tl);
            ImGui::InvisibleButton("##card", ImVec2(w - 10, rh),
                                   ImGuiButtonFlags_MouseButtonLeft |
                                       ImGuiButtonFlags_MouseButtonRight);
            bool card_hover = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                // shift / ctrl-click ADDS or REMOVES (2026-09-13); a plain click
                // on a member of a group keeps the group, so it can be dragged
                if (ImGui::GetIO().KeyShift || ImGui::GetIO().KeyCtrl) {
                    auto it = std::find(ed.selection.begin(), ed.selection.end(), n.name);
                    if (it == ed.selection.end()) ed.selection.push_back(n.name);
                    else ed.selection.erase(it);
                } else if (!(ed.selection.size() > 1 && ed.selected(n.name))) {
                    ed.selection = {n.name};
                }
            }
            // double-click a text element → edit it right here
            if (card_hover && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                if (const char* base = edit_base_of(n.glyph)) {
                    doc_edit_node = n.name;
                    doc_edit_base = base;
                    doc_edit_focus = true;
                    std::snprintf(doc_edit_buf, sizeof doc_edit_buf, "%s",
                                  text_of(n, base).c_str());
                    ed.selection = {n.name};
                }
            // ...and released without a drag, a plain click narrows to this one
            if (ImGui::IsItemDeactivated() && doc_drag.empty() &&
                !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl &&
                ed.selection.size() > 1 && ed.selected(n.name))
                ed.selection = {n.name};
            if (ImGui::IsItemActive() &&
                ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f) &&
                doc_grip.empty())
                doc_drag = n.name;
            if (ImGui::BeginPopupContextItem("##cardmenu")) {
                ImGui::TextDisabled("%s (%s)", n.name.c_str(), n.glyph.c_str());
                const bool multi = ed.selection.size() > 1 && ed.selected(n.name);
                if (multi)
                    ImGui::TextDisabled("%d selected - width and remove apply to all",
                                        (int)ed.selection.size());
                ImGui::Separator();
                if (ImGui::MenuItem("Edit (inspector)")) ed.selection = {n.name};
                ImGui::BeginDisabled(ri == 0);
                if (ImGui::MenuItem("Move up"))
                    pending_cmds.push_back("doc move " + n.name + " " +
                                           std::to_string((int)ri - 1));
                ImGui::EndDisabled();
                ImGui::BeginDisabled(ri + 1 >= rows.size());
                if (ImGui::MenuItem("Move down"))
                    pending_cmds.push_back("doc move " + n.name + " " +
                                           std::to_string((int)ri + 1));
                ImGui::EndDisabled();
                if (ImGui::BeginMenu("Width")) {
                    struct { const char* l; int s; } ws[] = {
                        {"Full", 12}, {"Two thirds", 8}, {"Half", 6},
                        {"Third", 4}};
                    for (auto& wch : ws)
                        if (ImGui::MenuItem(wch.l, nullptr, span == wch.s)) {
                            std::vector<std::string> rs;
                            for (const auto& sn : multi ? ed.selection
                                                        : std::vector<std::string>{n.name})
                                rs.push_back("doc resize " + sn + " " + std::to_string(wch.s));
                            pending_cmds.push_back(maiz::compile_commit(rs));
                        }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                const std::string rm_label =
                    multi ? "Remove " + std::to_string(ed.selection.size()) + " selected"
                          : std::string("Remove from document");
                if (ImGui::MenuItem(rm_label.c_str())) {
                    std::vector<std::string> rm; // one batch = one Ctrl+Z
                    for (const auto& sn : multi ? ed.selection
                                                : std::vector<std::string>{n.name})
                        rm.push_back("doc remove " + sn);
                    pending_cmds.push_back(maiz::compile_commit(rm));
                    ed.selection.clear();
                }
                ImGui::EndPopup();
            } else if (card_hover) {
                ImGui::SetTooltip("click: edit - drag: reorder (drop on a\n"
                                  "row's half to place side-by-side)\n"
                                  "right edge: resize - right-click: menu");
            }
            // the span GRIP (right edge, 8px): drag to resize
            ImGui::SetCursorScreenPos(ImVec2(br.x - 8, tl.y));
            ImGui::InvisibleButton("##grip", ImVec2(8, rh));
            if (ImGui::IsItemHovered() || doc_grip == n.name)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (ImGui::IsItemActivated()) doc_grip = n.name;
            if (doc_grip == n.name && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                int want = (int)std::lround((mouse.x - page_x) / page_w * 12.0);
                want = std::clamp(want - (int)std::lround(
                                             (tl.x - page_x) / page_w * 12.0),
                                  3, 12);
                dl->AddRect(tl, ImVec2(page_x + (tl.x - page_x) +
                                           page_w * want / 12.0f, br.y),
                            acc_col, 6.0f, 0, 2.0f);
            }
            if (doc_grip == n.name && ImGui::IsItemDeactivated()) {
                int want = (int)std::lround((mouse.x - tl.x) / page_w * 12.0);
                pending_cmds.push_back("doc resize " + n.name + " " +
                                       std::to_string(std::clamp(want, 3, 12)));
                doc_grip.clear();
            }
            } // end !editing_this
            ImGui::PopID();
            cards.push_back({&n, tl, br, (int)ri});
            x += w + 8.0f;
        }
        ImGui::SetCursorScreenPos(ImVec2(page_x, y0 + rh + 10.0f));
    }
    row_tops.push_back(ImGui::GetCursorScreenPos().y); // the end sentinel
    if (rows.empty())
        ImGui::TextDisabled("an empty document - click a component in the "
                            "palette to add one");
    ImGui::Dummy(ImVec2(1, 40)); // drop room below the last row

    // ── drag-reorder: insertion line between rows; pair on a row's half ─────
    if (!doc_drag.empty()) {
        const maiz::SceneNode* dragged = scene.find(doc_drag);
        if (!dragged || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // released: commit the drop
            if (dragged) {
                // dragging one member of a group moves the whole group (2026-09-13)
                const std::vector<std::string> moving =
                    (ed.selection.size() > 1 && ed.selected(doc_drag))
                        ? ed.selection : std::vector<std::string>{doc_drag};
                auto is_moving = [&](const std::string& nm) {
                    return std::find(moving.begin(), moving.end(), nm) != moving.end();
                };
                int pair_row = -1;
                for (const auto& c : cards) // on a half of a 1-card row? (one mover)
                    if (moving.size() == 1 && !is_moving(c.n->name) && mouse.y >= c.tl.y &&
                        mouse.y <= c.br.y && (int)rows[c.row_i].size() == 1 &&
                        mouse.x >= c.tl.x && mouse.x <= c.br.x)
                        pair_row = c.row_i;
                // rebuild the row list without the dragged component
                std::vector<std::vector<const maiz::SceneNode*>> nr;
                for (auto& row : rows) {
                    std::vector<const maiz::SceneNode*> keep;
                    for (auto* n : row)
                        if (!is_moving(n->name)) keep.push_back(n);
                    if (!keep.empty()) nr.push_back(keep);
                }
                if (pair_row >= 0) {
                    // pair: left or right of the target's single occupant
                    const maiz::SceneNode* target = rows[pair_row][0];
                    for (auto& row : nr)
                        if (row.size() == 1 && row[0] == target) {
                            bool left =
                                mouse.x < (cards[0].tl.x + page_w * 0.5f);
                            for (const auto& c : cards)
                                if (c.n == target)
                                    left = mouse.x < (c.tl.x + c.br.x) * 0.5f;
                            if (left) row.insert(row.begin(), dragged);
                            else row.push_back(dragged);
                        }
                } else {
                    // insert as its own row at the line under the mouse
                    size_t at = nr.size();
                    for (size_t i = 0; i + 1 < row_tops.size(); ++i)
                        if (mouse.y < (row_tops[i] + row_tops[i + 1]) * 0.5f) {
                            // translate: count surviving rows above line i
                            size_t k = 0;
                            for (size_t j = 0; j < i && j < rows.size(); ++j) {
                                bool only_dragged = std::all_of(
                                    rows[j].begin(), rows[j].end(),
                                    [&](const maiz::SceneNode* m) { return is_moving(m->name); });
                                if (!only_dragged) ++k;
                            }
                            at = k;
                            break;
                        }
                    std::vector<std::vector<const maiz::SceneNode*>> ins; // doc order
                    for (const auto& row : rows)
                        for (const auto* m : row)
                            if (is_moving(m->name)) ins.push_back({m});
                    nr.insert(nr.begin() + std::min(at, nr.size()), ins.begin(), ins.end());
                }
                // ONE batch: every changed row/col/span (normalized 6/6 pairs)
                std::vector<std::string> cmds;
                for (size_t i = 0; i < nr.size(); ++i)
                    for (size_t j = 0; j < nr[i].size(); ++j) {
                        const maiz::SceneNode* n = nr[i][j];
                        int wr = (int)i;
                        int wc = nr[i].size() == 2 ? (j == 0 ? 0 : 6) : 0;
                        int wsp = nr[i].size() == 2 ? 6 : 12;
                        if (hormiga::doc_field_int(*n, "row", -1) != wr)
                            cmds.push_back("set " + n->name + " row \"" +
                                           std::to_string(wr) + "\"");
                        if (hormiga::doc_field_int(*n, "col", 0) != wc)
                            cmds.push_back("set " + n->name + " col \"" +
                                           std::to_string(wc) + "\"");
                        if (hormiga::doc_field_int(*n, "span", 12) != wsp)
                            cmds.push_back("set " + n->name + " span \"" +
                                           std::to_string(wsp) + "\"");
                    }
                if (!cmds.empty())
                    pending_cmds.push_back(maiz::compile_commit(cmds));
            }
            doc_drag.clear();
        } else {
            // dragging: ghost + the insertion / pair hints
            dl->AddRect(ImVec2(mouse.x - 60, mouse.y - 14),
                        ImVec2(mouse.x + 60, mouse.y + 14), acc_col, 4.0f, 0,
                        2.0f);
            dl->AddText(ImVec2(mouse.x - 52, mouse.y - 8),
                        IM_COL32(60, 60, 60, 255), doc_drag.c_str());
            bool over_pair = false;
            for (const auto& c : cards)
                if (c.n->name != doc_drag && mouse.y >= c.tl.y &&
                    mouse.y <= c.br.y && (int)rows[c.row_i].size() == 1 &&
                    mouse.x >= c.tl.x && mouse.x <= c.br.x) {
                    bool left = mouse.x < (c.tl.x + c.br.x) * 0.5f;
                    ImVec2 a = left ? c.tl : ImVec2((c.tl.x + c.br.x) * 0.5f,
                                                    c.tl.y);
                    ImVec2 b = left ? ImVec2((c.tl.x + c.br.x) * 0.5f, c.br.y)
                                    : c.br;
                    dl->AddRectFilled(a, b, IM_COL32(70, 130, 200, 50), 6.0f);
                    over_pair = true;
                }
            if (!over_pair)
                for (size_t i = 0; i + 1 < row_tops.size(); ++i)
                    if (mouse.y < (row_tops[i] + row_tops[i + 1]) * 0.5f) {
                        float ly = row_tops[i] - 5;
                        dl->AddLine(ImVec2(page_x, ly),
                                    ImVec2(page_x + page_w, ly), acc_col, 3.0f);
                        break;
                    }
        }
    }

    // DESELECT (author ask 1): Escape, or a click on empty canvas → back to
    // ── PALETTE DROP: dragging an element from the palette inserts it AT the
    // row under the cursor (the author's one missing nicety). A blue line
    // shows where it will land; existing rows shift down. ────────────────────
    {
        ImVec2 cmin = ImGui::GetWindowPos();
        ImVec2 cmax(cmin.x + ImGui::GetWindowSize().x,
                    cmin.y + ImGui::GetWindowSize().y);
        if (ImGui::BeginDragDropTargetCustom(ImRect(cmin, cmax),
                                             ImGui::GetID("##paldrop"))) {
            // the insertion index from the cursor vs the row gaps
            int at = (int)rows.size();
            for (size_t i = 0; i + 1 < row_tops.size(); ++i)
                if (mouse.y < (row_tops[i] + row_tops[i + 1]) * 0.5f) {
                    at = (int)i;
                    break;
                }
            float ly = at < (int)row_tops.size() ? row_tops[at] - 5
                                                  : ImGui::GetCursorScreenPos().y;
            dl->AddLine(ImVec2(page_x, ly), ImVec2(page_x + page_w, ly), acc_col,
                        3.0f);
            if (const ImGuiPayload* pl =
                    ImGui::AcceptDragDropPayload("PALETTE_GLYPH")) {
                std::string glyph((const char*)pl->Data);
                std::string name;
                for (int i = 1;; ++i) {
                    name = glyph + "-" + std::to_string(i);
                    if (!scene.find(name)) break;
                }
                std::vector<std::string> cmds;
                // renumber the visual rows to CONTIGUOUS values with a gap at
                // `at` (robust when original rows had gaps; also tidies them)
                for (size_t ri = 0; ri < rows.size(); ++ri) {
                    int want = (int)ri < at ? (int)ri : (int)ri + 1;
                    for (const auto* n : rows[ri])
                        if (hormiga::doc_field_int(*n, "row", -1) != want)
                            cmds.push_back("set " + n->name + " row \"" +
                                           std::to_string(want) + "\"");
                }
                cmds.push_back("rune new " + glyph + " " + name);
                cmds.push_back("set " + name + " row \"" + std::to_string(at) + "\"");
                if (!cur_page.empty())
                    cmds.push_back("set " + name + " page \"" + cur_page + "\"");
                pending_cmds.push_back(maiz::compile_commit(cmds));
                ed.selection = {name};
            }
            ImGui::EndDragDropTarget();
        }
    }

    // the nothing-selected page manager. A card click already set selection
    // earlier this frame; we only clear when the click missed every card.
    // Delete / Backspace removes every selected component, as ONE undoable batch;
    // never while a text field has the keyboard, where Backspace means a letter
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        doc_edit_node.empty() && !ImGui::GetIO().WantTextInput &&
        !ed.selection.empty() &&
        (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))) {
        std::vector<std::string> rm;
        for (const auto& sn : ed.selection)
            if (const maiz::SceneNode* sp = scene.find(sn); sp && sp->glyph != "page")
                rm.push_back("doc remove " + sn);
        if (!rm.empty()) pending_cmds.push_back(maiz::compile_commit(rm));
        ed.selection.clear();
    }
    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
        ed.selection.clear();
    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) && doc_drag.empty() &&
        doc_grip.empty()) {
        bool on_card = false;
        for (const auto& c : cards)
            if (mouse.x >= c.tl.x && mouse.x <= c.br.x && mouse.y >= c.tl.y &&
                mouse.y <= c.br.y)
                on_card = true;
        if (!on_card) ed.selection.clear();
    }
    ImGui::EndChild();
}

void HormigaApp::draw_builder_section(float /*avail_h*/) {
    // ── DOCUMENT picker (author #9): switch between projects (each a mantle);
    // "+ New" mints one. The active document is what the canvas edits and the
    // previews render. ──────────────────────────────────────────────────────
    ImGui::TextDisabled("doc:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("##docpick", cur_doc.c_str())) {
        for (const auto& d : list_documents())
            if (ImGui::Selectable(d.c_str(), d == cur_doc) && d != cur_doc) {
                cur_doc = d;
                cur_page.clear();
                ed.selection.clear();
                dispatch_and_reproject("use " + cur_doc);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_SQUARE_PLUS " New##doc")) ImGui::OpenPopup("##newdoc");
    if (ImGui::BeginPopup("##newdoc")) {
        ImGui::TextDisabled("a new document (newsletter or website)");
        ImGui::SetNextItemWidth(200);
        bool go = ImGui::InputTextWithHint("##ndname", "document name...",
                                           new_doc_name, sizeof new_doc_name,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Create") || go) && new_doc_name[0]) {
            new_document(new_doc_name);
            new_doc_name[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("documents are saved in the database (Antfarm-decided\n"
                          "store); each is its own newsletter or website");
    /* ── RENAME / DELETE (2026-09-02) ────────────────────────────────────────
     * The roadmap's "blocked on Core `mantle rm`/`rename`" note outlived its
     * blocker; both verbs are in `verbs_edit.c`. See `rename_document` /
     * `delete_document` for why one of these asks and the other does not. */
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_PEN_TO_SQUARE " Rename##doc")) {
        std::snprintf(rename_doc_name, sizeof rename_doc_name, "%s",
                      cur_doc.c_str());
        ImGui::OpenPopup("##renamedoc");
    }
    if (ImGui::BeginPopup("##renamedoc")) {
        ImGui::TextDisabled("rename this document");
        ImGui::SetNextItemWidth(200);
        const bool go =
            ImGui::InputText("##rdname", rename_doc_name, sizeof rename_doc_name,
                             ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Rename") || go) && rename_doc_name[0]) {
            rename_document(rename_doc_name);
            ImGui::CloseCurrentPopup();
        }
        ImGui::TextDisabled("every element comes with it; links repoint");
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_TRASH " Delete##doc")) ImGui::OpenPopup("##deldoc");
    if (ImGui::BeginPopup("##deldoc")) {
        /* The consequence in words, with a count, rather than "are you sure?".
         * Same reasoning as the Publish confirmation: a person can answer a
         * question about 34 elements and cannot answer a question about
         * nothing in particular. */
        ImGui::TextColored(ImVec4(0.9f, 0.55f, 0.3f, 1),
                           "Delete '%s' and its %d element(s)?", cur_doc.c_str(),
                           document_element_count(cur_doc));
        ImGui::TextDisabled("the document and everything in it. Ctrl+Z undoes it,\n"
                            "but the picker will not offer it again until you do.");
        if (ImGui::Button("Delete document")) {
            delete_document(cur_doc);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    /* ── SAVE, IN THE TAB WHERE THE WORK HAPPENS (2026-09-02) ────────────────
     *
     * The author: *"there's still no 'save' button in the builder tab. sure i
     * can save the miga file, but i would like to just save what i have in the
     * builder as well. I don't wanna 'save as' a lot of the time honestly."*
     *
     * Every edit is already persisted through the dispatcher, and `save` writes
     * the state document — File > Save and Ctrl+S have always done it. What was
     * missing is not persistence, it is **being told**, in the place a person is
     * working, that their afternoon is on disk. A `.miga` Save As is a different
     * operation entirely (it packs a portable bundle) and reaching for it as a
     * substitute is exactly what the author was doing.
     *
     * So: the same `do_save()` the File menu calls, with a live indicator beside
     * it. `edits_since_save` is incremented by `dispatch_and_reproject` — one
     * counter, at the one door every GUI edit goes through, which is the only
     * place it cannot drift from the truth. */
    ImGui::SameLine(0, 16);
    if (ImGui::SmallButton(edits_since_save ? ICON_FA_FLOPPY_DISK " Save*"
                                        : ICON_FA_FLOPPY_DISK " Save")) do_save();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            edits_since_save
                ? "write the database to disk (Ctrl+S).\n%d change(s) since the "
                  "last save.\nThis is NOT 'save as a .miga' - that packs a "
                  "portable bundle."
                : "everything is written to disk (Ctrl+S).\nThis is NOT 'save as "
                  "a .miga' - that packs a portable bundle.",
            edits_since_save);
    ImGui::SameLine();
    if (edits_since_save)
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1), "%d unsaved",
                           edits_since_save);
    else
        ImGui::TextDisabled("saved");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_DOWNLOAD " Save to file")) export_document();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("export this document to documents/<name>.json -\n"
                          "a portable file you can back up or share");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN " Load file")) import_document();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("open a document .json as a NEW document (never\n"
                          "overwrites the current one)");
    ImGui::SameLine(0, 16);
    // toolbar
    ImGui::SetNextItemWidth(60);
    ImGui::Combo("##lang", &preview_lang, "EN\0ES\0");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_ENVELOPE " Email preview"))
        dispatch_and_reproject(preview_lang ? "effect render es" : "effect render en");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("email domain: table-safe HTML for pasting into a mail client");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_GLOBE " Build website"))
        dispatch_and_reproject(preview_lang ? "effect render-site es"
                                            : "effect render-site en");
    if (ImGui::IsItemHovered())
        /* The old text ended "- deploy the folder to your domain", which was
         * true when copying a folder was the only way out and became an
         * instruction to do BY HAND the thing the application had learned to
         * do. It was also the only mention of deploying anywhere in the GUI, so
         * it pointed away from the feature. */
        ImGui::SetTooltip("web domain: a responsive static site in site/ (CSS + JS +\n"
                          "self-hosted images). This builds it; it does not put it\n"
                          "online - open the Publish tab for that.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Publish...")) win_publish = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("build, preview, and put this website online -\n"
                          "with the history of what was published when");
    ImGui::SameLine(0, 16);
    // B2: the live preview — server + browser auto-reload on every edit
    bool was_live = preview_live;
    if (was_live) ImGui::PushStyleColor(ImGuiCol_Button,
                                        ImVec4(0.18f, 0.45f, 0.25f, 1.0f));
    if (ImGui::SmallButton(preview_live ? "LIVE" : "Live preview")) {
        if (preview_live) {
            preview_live = false;
            toast("live preview paused (server stays up)");
        } else {
            preview_start();
        }
    }
    if (was_live) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("open the site in your browser; every edit here\n"
                          "re-renders and the page reloads itself.\n"
                          "email twin: /preview-%s.html on the same server",
                          preview_lang ? "es" : "en");
    ImGui::SameLine();
    if (ImGui::SmallButton("Templates")) show_templates = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("start from a designed layout + theme, or save\n"
                          "the current document as a template");
    // the legacy block canvas is hidden by default (Settings > show legacy);
    // "Migrate to grid" retired — new documents are grid-native
    if (show_legacy) {
        ImGui::SameLine(0, 16);
        if (ImGui::SmallButton(builder_doc_view ? "elements (legacy)" : "document"))
            builder_doc_view = !builder_doc_view;
        ImGui::SameLine();
        if (ImGui::SmallButton("tidy stacks")) {
            std::string cmd = maiz::compile_stack_layout(scene, canvas_style.block);
            if (!cmd.empty()) dispatch_and_reproject(cmd);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Migrate to grid"))
            pending_cmds.push_back("doc migrate");
    } else {
        builder_doc_view = true; // always the document canvas when legacy hidden
    }
    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("click: edit - double-click text: edit inline - "
                        "drag: reorder");

    float body_h = ImGui::GetContentRegionAvail().y;
    ImVec2 area = ImGui::GetContentRegionAvail();
    const float th = 6.0f;
    const float pal_w = 150.0f;

    ImGui::BeginChild("builder-palette", ImVec2(pal_w, body_h), ImGuiChildFlags_Borders);
    std::string last_cat;
    for (const auto& e : palette_blocks.entries) {
        if (e.category != last_cat) {
            ImGui::SeparatorText(e.category.c_str());
            last_cat = e.category;
        }
        /* ── THE ICON GOES ON THE BUTTON, AND ON THE DRAG GHOST (2026-09-02) ─
         *
         * The author asked for *"little icons next to the drag and drop
         * button"*. The palette is a column of same-width buttons whose only
         * differentiator was a word, which is exactly the case an icon earns
         * its place in: at a glance, `image grid` and `event grid` are the same
         * shape and the same length, and a picture is not.
         *
         * `glyph_icon` (app_internal.hpp) maps the glyph to a Font Awesome
         * codepoint already merged into the ImGui atlas. Unlisted glyphs get a
         * neutral square rather than nothing, so a new block looks sparse
         * instead of broken.
         *
         * The DRAG GHOST gets it too. That ghost is the only thing visible
         * while a person is deciding where to drop, so it is the one place the
         * icon is doing the most work. */
        const std::string plabel =
            std::string(glyph_icon(e.glyph)) + "  " + e.label;
        bool clicked = ImGui::Button(plabel.c_str(), ImVec2(-1, 0));
        // DRAG a palette element onto the document (author's one missing
        // nicety, 2026-07-23): drop between rows in the doc canvas to insert
        // AT a position; the click still appends.
        if (builder_doc_view &&
            ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("PALETTE_GLYPH", e.glyph.c_str(),
                                      e.glyph.size() + 1);
            ImGui::Text("%s  %s", glyph_icon(e.glyph), e.label.c_str());
            ImGui::EndDragDropSource();
        }
        if (clicked) {
            if (builder_doc_view) {
                doc_palette_place(e.glyph); // append via the `doc place` verb
            } else {
                // interim click-to-mint: lands under the lowest block
                std::string name;
                for (int i = 1;; ++i) {
                    name = e.glyph + "-" + std::to_string(i);
                    if (!scene.find(name)) break;
                }
                float maxb = 60.0f;
                for (const auto& n : scene.nodes)
                    maxb = std::max(maxb, n.y + n.h);
                dispatch_and_reproject(
                    maiz::compile_add(e.glyph, name, 80.0f, maxb + 50.0f));
                ed.selection = {name};
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    float main_w = std::max(120.0f, (area.x - pal_w - th) * canvas_frac);
    if (builder_doc_view) {
        ImGui::BeginChild("builder-doc-pane", ImVec2(main_w, body_h));
        draw_document_canvas(body_h);
        ImGui::EndChild();
    } else {
        ImGui::BeginChild("builder-canvas", ImVec2(main_w, body_h),
                          ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        maiz::CanvasIO cio = maiz::edit_canvas("builder-canvas", scene, ed,
                                               canvas_style, &palette_blocks,
                                               &faces);
        for (const auto& cmd : cio.commands) dispatch_and_reproject(cmd);
        ImGui::EndChild();
    }

    ImGui::SameLine(0, 0);
    auto vs = maiz::splitter("##builder-vsplit", true, canvas_frac,
                             area.x - pal_w - th, 0.4f, 0.92f, th, body_h);
    ImGui::SameLine(0, 0);
    ImGui::BeginChild("builder-inspector", ImVec2(0, body_h));
    // nothing selected → the PAGE MANAGER (map-style overview, author
    // 2026-07-23); a component selected → its inspector
    bool have_sel = !ed.selection.empty() && scene.find(ed.selection.front());
    if (builder_doc_view && !have_sel) {
        draw_page_manager();
    } else {
        // a target-PICKER for navigation fields (author ask 2): a dropdown of
        // pages (+ "external URL") beats hand-typing a slug. For a `link`
        // element it sets `target`; for anything else it sets `link_to`
        // ("anything can be a button").
        const maiz::SceneNode* sel =
            have_sel ? scene.find(ed.selection.front()) : nullptr;
        if (sel) {
            const char* field = sel->glyph == "link" ? "target" : "link_to";
            const char* label = sel->glyph == "link" ? "Links to"
                                                      : "Navigates to (optional)";
            std::string cur = field_value(*sel, field);
            ImGui::SeparatorText("Navigation");
            ImGui::SetNextItemWidth(-1);
            std::string preview = cur.empty() ? "(none)" : cur;
            if (ImGui::BeginCombo(label, preview.c_str())) {
                if (ImGui::Selectable("(none)", cur.empty()))
                    pending_cmds.push_back("set " + sel->name + " " + field + " \"\"");
                for (const auto& n : scene.nodes)
                    if (n.glyph == "page") {
                        std::string slug = field_value(n, "slug");
                        if (slug.empty()) slug = n.name;
                        std::string t = field_value(n, "title_en");
                        std::string it = t.empty() ? slug : t + "  (/" + slug + ")";
                        if (ImGui::Selectable(it.c_str(), cur == slug))
                            pending_cmds.push_back("set " + sel->name + " " +
                                                   field + " \"" + slug + "\"");
                    }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("pick a page, or type a URL in the field\n"
                                  "below (http://... or /path)");
            ImGui::Spacing();

            // BAND styling (W1): give this element's ROW a background + full-
            // bleed — the alternating colored sections of a modern site. Sets
            // the fields on ALL elements sharing the row (so it reads the same
            // whichever one is the leader). Hero has its own banner.
            if (sel->glyph != "hero") {
                int myrow = hormiga::doc_field_int(*sel, "row", -1);
                std::string mypage = field_value(*sel, "page");
                auto set_row = [&](const char* field, const std::string& val) {
                    for (const auto& n : scene.nodes)
                        if (n.glyph != "page" &&
                            hormiga::doc_field_int(n, "row", -2) == myrow &&
                            field_value(n, "page") == mypage)
                            pending_cmds.push_back("set " + n.name + " " + field +
                                                   " \"" + val + "\"");
                };
                ImGui::SeparatorText("Band (this row's section)");
                const char* bgs[] = {"none", "tint", "card",
                                     "accent", "dark", "gradient"};
                std::string cur_bg = field_value(*sel, "band_bg");
                if (cur_bg.empty()) cur_bg = "none";
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("Background", cur_bg.c_str())) {
                    for (const char* b : bgs)
                        if (ImGui::Selectable(b, cur_bg == b)) set_row("band_bg", b);
                    ImGui::EndCombo();
                }
                bool full = field_value(*sel, "band_full") == "1";
                if (ImGui::Checkbox("Full-bleed (edge to edge)", &full))
                    set_row("band_full", full ? "1" : "0");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("the band's background spans the whole\n"
                                      "window width; content stays centered");
                // a background IMAGE (photo behind white text — a modern
                // marketing section). Pick from the org's images.
                std::string cur_img = field_value(*sel, "band_image");
                std::string iprev = cur_img.empty() ? "(none)" : cur_img;
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("Background image", iprev.c_str())) {
                    if (ImGui::Selectable("(none)", cur_img.empty()))
                        set_row("band_image", "");
                    maiz::ProjectOptions ipo;
                    ipo.mantle = kDataMantle;
                    maiz::Scene isc = maiz::project_scene(core, ipo);
                    for (const auto& dn : isc.nodes) {
                        if (dn.glyph != "image") continue;
                        std::string path = field_value(dn, "path");
                        if (path.empty()) continue;
                        std::string lbl = humanize(dn.name) + "##" + dn.name;
                        if (ImGui::Selectable(lbl.c_str(), cur_img == path))
                            set_row("band_image", path);
                    }
                    ImGui::EndCombo();
                }
                if (!cur_img.empty())
                    ImGui::TextDisabled("photo band: text renders white over a\n"
                                        "dark gradient (auto full-bleed)");
                ImGui::Spacing();
            }

            // FILTER builder (author #2: no more typing AND/OR). For query-
            // backed elements — tag chips + an AND/OR joiner; the raw string is
            // an advanced escape hatch for complex expressions.
            bool has_query = false;
            for (const auto& f : sel->fields)
                if (f.key == "query") has_query = true;
            if (has_query) {
                std::string q = field_value(*sel, "query");
                ImGui::SeparatorText("Filter (what this shows)");
                if (ImGui::SmallButton(ICON_FA_CODE "  edit as an expression"))
                    open_tag_expr_editor(*sel);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("write this filter exactly: AND / OR / NOT,\n"
                                      "parentheses, date:future - with a live count");
                draw_tag_expr_editor(*sel);
                bool has_or = q.find(" OR ") != std::string::npos;
                bool has_and = q.find(" AND ") != std::string::npos;
                bool complex = q.find('(') != std::string::npos ||
                               (has_or && has_and);
                std::string joiner = has_or ? " OR " : " AND ";
                // split the query into terms on the joiner
                std::vector<std::string> terms;
                if (!q.empty() && !complex) {
                    size_t p = 0, f;
                    while ((f = q.find(joiner, p)) != std::string::npos) {
                        terms.push_back(q.substr(p, f - p));
                        p = f + joiner.size();
                    }
                    terms.push_back(q.substr(p));
                }
                auto rebuild = [&](const std::vector<std::string>& ts,
                                   const std::string& j) {
                    std::string nq;
                    for (size_t i = 0; i < ts.size(); ++i)
                        nq += (i ? j : "") + ts[i];
                    pending_cmds.push_back("set " + sel->name + " query " +
                                           json_str(nq));
                };
                if (complex || filter_raw) {
                    char qbuf[256];
                    std::snprintf(qbuf, sizeof qbuf, "%s", q.c_str());
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##rawq", qbuf, sizeof qbuf,
                                         ImGuiInputTextFlags_EnterReturnsTrue))
                        pending_cmds.push_back("set " + sel->name + " query " +
                                               json_str(qbuf));
                    ImGui::Checkbox("advanced (raw tag expression)", &filter_raw);
                } else {
                    for (size_t i = 0; i < terms.size(); ++i) {
                        ImGui::PushID((int)i);
                        if (ImGui::SmallButton(("@" + terms[i] + "  x").c_str())) {
                            auto nt = terms;
                            nt.erase(nt.begin() + i);
                            rebuild(nt, joiner);
                        }
                        ImGui::PopID();
                        ImGui::SameLine();
                    }
                    ImGui::NewLine();
                    if (terms.size() > 1) {
                        int jm = has_or ? 1 : 0;
                        ImGui::TextDisabled("match:");
                        ImGui::SameLine();
                        if (ImGui::RadioButton("all (AND)", jm == 0))
                            rebuild(terms, " AND ");
                        ImGui::SameLine();
                        if (ImGui::RadioButton("any (OR)", jm == 1))
                            rebuild(terms, " OR ");
                    }
                    // tag picker over the DATA vocabulary
                    maiz::ProjectOptions dpo;
                    dpo.mantle = kDataMantle;
                    maiz::Scene dsc = maiz::project_scene(core, dpo);
                    std::set<std::string> vocab;
                    for (const auto& dn : dsc.nodes)
                        for (const auto& tg : dn.tags) vocab.insert(tg);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::InputTextWithHint("##filtag", "+ filter tag...",
                                             filter_tag_input,
                                             sizeof filter_tag_input);
                    if (filter_tag_input[0]) {
                        int shown = 0;
                        for (const auto& tg : vocab) {
                            if (tg.find(filter_tag_input) == std::string::npos)
                                continue;
                            if (++shown > 6) break;
                            if (ImGui::Selectable((tg + "##ft").c_str())) {
                                auto nt = terms;
                                nt.push_back(tg);
                                rebuild(nt, joiner);
                                filter_tag_input[0] = 0;
                            }
                        }
                    }
                }
                ImGui::Spacing();
            }

            // MAP VIEW picker (author #1: "assign which views are present").
            // A dropdown of the saved map views + a shortcut to Manage views on
            // the Map tab (where views are created/styled). Multi-view layering
            // on one widget is noted as future.
            if (sel->glyph == "map_embed") {
                ImGui::SeparatorText("Map view");
                maiz::ProjectOptions dpo;
                dpo.mantle = kDataMantle;
                maiz::Scene dsc = maiz::project_scene(core, dpo);
                std::string cur_view = field_value(*sel, "view");
                ImGui::SetNextItemWidth(-1);
                std::string prev = cur_view.empty() ? "(first view)" : cur_view;
                if (ImGui::BeginCombo("##mapview", prev.c_str())) {
                    if (ImGui::Selectable("(first view)", cur_view.empty()))
                        pending_cmds.push_back("set " + sel->name + " view \"\"");
                    for (const auto& n : dsc.nodes)
                        if (n.glyph == "map")
                            if (ImGui::Selectable(n.name.c_str(), cur_view == n.name))
                                pending_cmds.push_back("set " + sel->name +
                                                       " view \"" + n.name + "\"");
                    ImGui::EndCombo();
                }
                if (ImGui::SmallButton("Manage views (Map tab)")) {
                    sec_open[3] = true; // the Map section
                    show_manage_views = true;
                }
                ImGui::TextDisabled("views are created & styled on the Map tab;\n"
                                    "this widget draws the chosen view's markers");
                ImGui::Spacing();
            }
        }
        maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
        for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
    }
    ImGui::EndChild();
    if (vs.released) flush_panels();
}

/* The PAGE MANAGER (author 2026-07-23): pages are managed here, in the
 * nothing-selected panel — like the map's "On this map" overview. Pages are
 * NOT views (they don't stack; they're separate). Create, reorder, set-home,
 * toggle header-nav, edit-settings (selects the page rune → its inspector),
 * delete (its components fall back to home). Navigation BETWEEN pages is
 * authored with `link` elements, not generated here. */
void HormigaApp::draw_page_manager() {
    std::string suf = preview_lang ? "_es" : "_en";
    std::vector<const maiz::SceneNode*> pages;
    for (const auto& n : scene.nodes)
        if (n.glyph == "page") pages.push_back(&n);
    std::sort(pages.begin(), pages.end(),
              [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                  return hormiga::doc_field_int(*a, "order", 0) <
                         hormiga::doc_field_int(*b, "order", 0);
              });

    ImGui::SeparatorText("Pages");
    if (pages.empty()) {
        ImGui::TextWrapped(
            "This is a single-page document. A website can have many pages "
            "(home, about, events...) - add one to begin. Navigation between "
            "pages is built with 'link / button' elements you place, so pages "
            "can also stand alone, unlinked.");
        ImGui::Spacing();
        if (ImGui::Button("+ New page", ImVec2(-1, 0))) doc_new_page();
        return;
    }
    ImGui::TextDisabled("the site's pages - the FLOW between them is authored\n"
                        "with 'link / button' elements (a page can stand alone)");
    ImGui::Spacing();

    for (size_t i = 0; i < pages.size(); ++i) {
        const maiz::SceneNode& p = *pages[i];
        std::string slug = field_value(p, "slug");
        if (slug.empty()) slug = p.name;
        std::string title = field_value(p, std::string("title") + suf);
        if (title.empty()) title = field_value(p, "title_en");
        if (title.empty()) title = slug;
        bool home = (i == 0);
        ImGui::PushID((int)i);

        bool active = (slug == cur_page);
        if (ImGui::Selectable((title + (home ? "  (home)" : "")).c_str(), active))
            cur_page = slug; // switch the canvas to this page
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("/%s - click to edit this page",
                              home ? "" : (slug + "-<lang>.html").c_str());

        ImGui::Indent(12);
        // header-nav toggle (opt-in; nav is authored, not automatic)
        bool innav = field_value(p, "in_nav") != "0";
        if (ImGui::Checkbox("in header nav", &innav))
            pending_cmds.push_back("set " + p.name + " in_nav \"" +
                                   (innav ? "1" : "0") + "\"");
        ImGui::SameLine();
        if (ImGui::SmallButton("Edit settings")) // → the rune's inspector
            ed.selection = {p.name};
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        if (ImGui::SmallButton("Up")) { // swap order with the previous page
            int o0 = hormiga::doc_field_int(*pages[i - 1], "order", (int)i - 1);
            int o1 = hormiga::doc_field_int(p, "order", (int)i);
            pending_cmds.push_back(maiz::compile_commit(
                {"set " + pages[i - 1]->name + " order \"" + std::to_string(o1) + "\"",
                 "set " + p.name + " order \"" + std::to_string(o0) + "\""}));
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(i + 1 >= pages.size());
        if (ImGui::SmallButton("Down")) {
            int o0 = hormiga::doc_field_int(p, "order", (int)i);
            int o1 = hormiga::doc_field_int(*pages[i + 1], "order", (int)i + 1);
            pending_cmds.push_back(maiz::compile_commit(
                {"set " + p.name + " order \"" + std::to_string(o1) + "\"",
                 "set " + pages[i + 1]->name + " order \"" + std::to_string(o0) + "\""}));
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(home); // home can't be deleted (it holds the rest)
        if (ImGui::SmallButton("Delete")) {
            // its components fall back to home (clear their page field)
            std::vector<std::string> cmds;
            for (const auto& n : scene.nodes)
                if (field_value(n, "page") == slug)
                    cmds.push_back("set " + n.name + " page \"\"");
            cmds.push_back("rm " + p.name);
            pending_cmds.push_back(maiz::compile_commit(cmds));
            if (cur_page == slug) cur_page.clear();
        }
        ImGui::EndDisabled();
        ImGui::Unindent(12);
        ImGui::Spacing();
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ImGui::Button("+ New page", ImVec2(-1, 0))) doc_new_page();

    // TAGS for the current page (author ask 3): pages are runes; tagging them
    // lets rules/queries treat pages like any other content (e.g. group them,
    // theme a section). The tag picker offers the existing vocabulary.
    const maiz::SceneNode* cp = nullptr;
    for (const auto* p : pages)
        if (field_value(*p, "slug") == cur_page ||
            (cur_page.empty() && p == pages.front()))
            cp = p;
    if (cp) {
        ImGui::SeparatorText(("Tags - page '" + cur_page + "'").c_str());
        for (const auto& t : cp->tags) {
            if (t.find(':') != std::string::npos) continue; // skip namespaced
            ImGui::PushID(t.c_str());
            if (ImGui::SmallButton(("#" + t + "  x").c_str()))
                pending_cmds.push_back("tag " + cp->name + " -" + t);
            ImGui::PopID();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        std::string chose =
            tag_picker("##pgtag", page_tag_input, sizeof page_tag_input,
                       "+ tag this page...");
        if (!chose.empty())
            pending_cmds.push_back("tag " + cp->name + " +" + chose);
    }
    ImGui::Spacing();
    ImGui::TextDisabled("tip: place a 'link / button' element and set its\n"
                        "target to a page's slug to navigate there. Any\n"
                        "element's 'Navigates to' field makes it a button.");
}

// ── the Antfarm section: placeholder cards until the registry exists ────────

void HormigaApp::draw_antfarm_section() {
    // the node graph IS the configuration surface (okf/concepts/platform/antfarm.md):
    // one core hub, typed sockets, providers plugged in; faces are live
    // describe() views; the Supabase node's button imports the real org
    ImGui::TextDisabled("the org's backends, by payload: records + assets flow "
                        "from the core; the HTML publisher builds a site the "
                        "server/deploy nodes carry. ports only fit their own type.");
    float body_h = ImGui::GetContentRegionAvail().y;
    ImVec2 area = ImGui::GetContentRegionAvail();
    const float th = 6.0f;
    float main_w = std::max(120.0f, (area.x - th) * canvas_frac);

    ImGui::BeginChild("antfarm-canvas", ImVec2(main_w, body_h),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    maiz::CanvasIO cio = maiz::edit_canvas("antfarm-canvas", scene, ed,
                                           canvas_style, &palette_antfarm, &faces);
    for (const auto& cmd : cio.commands) dispatch_and_reproject(cmd);
    ImGui::EndChild();

    ImGui::SameLine(0, 0);
    auto vs = maiz::splitter("##antfarm-vsplit", true, canvas_frac, area.x - th,
                             0.4f, 0.92f, th, body_h);
    ImGui::SameLine(0, 0);
    ImGui::BeginChild("antfarm-inspector", ImVec2(0, body_h));
    maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
    for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
    ImGui::EndChild();
    if (vs.released) flush_panels();
}

