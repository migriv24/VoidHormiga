/* ui/map_panels.cpp — the map's four windows: Inspector, Overview, Layers,
 * Actions (2026-10-05).
 *
 * The author: *"we have the inspector window kinda (which changes what it shows
 * a lot), and instead there could be mini tabs there ... the normal inspector
 * (which is the last thing that was clicked) ... the general 'map overview' tab
 * (just lists all the stuff on the map) ... the layers tab (with all the ui/ux
 * stuff for managing layers for things that most drawing applications would
 * have) ... a 'map actions' tab ... exporting a well structured image ... or
 * exporting a version of the map to be shown as a javascript component in a
 * website document."*
 *
 * They are real windows, docked as tabs beside the canvas by `map.cpp` the
 * first time and free to be torn off after, like the Builder's. The canvas
 * computes the active layer's rules and channel each frame before these draw,
 * so what they show is what the canvas shows.
 *
 * LAYERS ARE `map` RUNES. What a drawing application's layer panel has, each a
 * dispatcher command like everything else: an eye (`visible`), the one being
 * edited (local selection, `map_sel`), a name (`title`), a place in the stack
 * (`order`, higher on top), opacity and brightness as seen under the edited
 * one, whether its positions are shared with the others or its own (`channel`:
 * the old "lock"), its style rules, its labels. The base map is the
 * background, drawn as the bottom row, as a drawing application draws its
 * canvas colour. */
#include "app/app_internal.hpp"
#include "domain/bestow.hpp"
#include "voidmaiz/mobile.hpp" // dim_wrapped

#include <cfloat>

namespace {
const char* layer_name_of(const maiz::SceneNode& l) {
    static std::string buf;
    buf = rune_title(l);
    return buf.c_str();
}
} // namespace

void HormigaApp::draw_map_panels() {
    const std::vector<const maiz::SceneNode*> layers = map_layers(); // the canvas on screen's
    const hormiga::canvas::Canvas cvs = hormiga::canvas::find(scene, map_canvas);
    const maiz::SceneNode* cv_rune = map_canvas.empty() ? nullptr : scene.find(map_canvas);
    const maiz::SceneNode* cur = nullptr;
    for (auto* l : layers)
        if (l->name == map_sel) cur = l;
    auto rule_filter_expr = [](const MapRule& r) { return rule_expr_of(r); };

    // ── INSPECTOR: the thing last clicked ─────────────────────────────────────
    ImGui::Begin("Inspector##map");
    {
        const bool have_sel = !ed.selection.empty() && scene.find(ed.selection.front());
        if (ed.selection.size() > 1) {
        // ── MULTI-SELECT (box/shift, author #2): act on the whole set at once ─
        if (ImGui::SmallButton("deselect all")) ed.selection.clear();
        ImGui::SameLine();
        ImGui::Text("%d selected", (int)ed.selection.size());
        ImGui::Separator();
        for (const auto& nm : ed.selection) {
            const maiz::SceneNode* n = scene.find(nm);
            ImGui::BulletText("%s%s", nm.c_str(),
                              n ? ("  (" + n->glyph + ")").c_str() : "");
        }
        ImGui::Spacing();
        ImGui::TextDisabled("apply to all selected:");
        // tag every selected entity (the reusable tag picker, #4)
        std::string t = tag_picker("##multitag", tag_pick_input, sizeof tag_pick_input,
                                   "tag all selected...");
        if (!t.empty())
            for (const auto& nm : ed.selection)
                pending_cmds.push_back("tag " + nm + " +" + t);
        // color/icon submenus via a small popup
        if (ImGui::Button("Color all")) ImGui::OpenPopup("##multicolor");
        ImGui::SameLine();
        if (ImGui::Button("Icon all")) ImGui::OpenPopup("##multiicon");
        ImGui::SameLine();
        if (ImGui::Button("Remove from map")) {
            std::string fld = geo_field_for(active_channel);
            for (const auto& nm : ed.selection) {
                std::vector<std::string> c = {"set " + nm + " " + fld + " \"\""};
                if (fld == "geo") c.push_back("tag " + nm + " -located");
                pending_cmds.push_back(maiz::compile_commit(c));
            }
            ed.selection.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete all")) {
            for (const auto& nm : ed.selection) pending_cmds.push_back("rm " + nm);
            ed.selection.clear();
        }
        if (ImGui::BeginPopup("##multicolor")) {
            for (const auto& c : kMarkerColors) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::ColorConvertU32ToFloat4(c.col));
                bool pick = ImGui::MenuItem(c.tag);
                ImGui::PopStyleColor();
                if (pick)
                    for (const auto& nm : ed.selection) {
                        const maiz::SceneNode* n = scene.find(nm);
                        std::string cur = n ? tag_value(*n, "color") : "";
                        std::string cmd = "tag " + nm;
                        if (!cur.empty()) cmd += " -color:" + cur;
                        cmd += " +color:" + std::string(c.tag);
                        pending_cmds.push_back(cmd);
                    }
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##multiicon")) {
            for (const auto& ic : kMarkerIcons) {
                std::string lbl = std::string(ic.glyph) + "  " + ic.label;
                if (ImGui::MenuItem(lbl.c_str()))
                    for (const auto& nm : ed.selection) {
                        const maiz::SceneNode* n = scene.find(nm);
                        std::string cur = n ? tag_value(*n, "icon") : "";
                        std::string cmd = "tag " + nm;
                        if (!cur.empty()) cmd += " -icon:" + cur;
                        cmd += " +icon:" + std::string(ic.tag);
                        pending_cmds.push_back(cmd);
                    }
            }
            ImGui::EndPopup();
        }
        } else if (have_sel) {
            const maiz::SceneNode* sn = scene.find(ed.selection.front());
            ImGui::TextUnformatted(rune_title(*sn).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", sn->glyph.c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("deselect").x -
                            ImGui::GetStyle().FramePadding.x * 2);
            if (ImGui::SmallButton("deselect")) ed.selection.clear();
            if (sn->glyph != "map" && sn->glyph != "mapshape" && sn->glyph != "refpoint") {
                std::vector<std::string> out;
                draw_save_check(*sn, out);
                for (auto& c : out) pending_cmds.push_back(std::move(c));
            }
            ImGui::Separator();
            maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
            for (const auto& cmd : iio.commands) pending_cmds.push_back(cmd);
        } else {
            maiz::dim_wrapped("Click something on the map to see it here, or pick it in Overview. "
                              "Shift-drag on the map selects several.");
        }
    }
    ImGui::End();

    // ── OVERVIEW: everything on the map ───────────────────────────────────────
    ImGui::Begin("Overview##map");
    {
        std::string picked = search_picker(
            "##mapoverviewsearch", panel_search, sizeof panel_search,
            [](const maiz::SceneNode& n) { return n.glyph != "map" && n.glyph != "image"; },
            ICON_FA_MAGNIFYING_GLASS "  find...  @tag  type:event");
        if (!picked.empty()) {
            const maiz::SceneNode* pn = scene.find(picked);
            const std::string g = pn ? view_geo(*pn, active_channel) : "";
            double la, lo;
            if (!g.empty() && hormiga::parse_geo(g, la, lo)) {
                map_cam.x = (float)lo;
                map_cam.y = (float)la;
                if (map_cam.zoom < 15) map_cam.zoom = 15;
                pending_cmds.push_back(maiz::compile_camera(map_cam, hormiga::canvas::camera_key("view.map.camera", map_canvas)));
                ed.selection = {picked};
            } else {
                map_place_arm = picked; // click the map to place it
            }
        }
        // grouped by kind, each with its count; filtered like the map
        struct Row {
            const maiz::SceneNode* n;
            double la, lo;
        };
        std::map<std::string, std::vector<Row>> by_kind;
        int total = 0;
        for (const auto& n : scene.nodes) {
            if (n.glyph == "map" || n.glyph == "image" || n.glyph == "canvas") continue;
            if ((n.glyph == "mapshape" || n.glyph == "refpoint") && !hormiga::canvas::on(n, map_canvas)) continue;
            double la, lo;
            const std::string g = n.glyph == "mapshape" ? hormiga::temper::field_value(n, "geo1")
                                  : n.glyph == "refpoint" ? hormiga::temper::field_value(n, "geo")
                                                          : view_geo(n, active_channel);
            if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
            if (!search_match(n, map_filter)) continue;
            by_kind[n.glyph].push_back({&n, la, lo});
            ++total;
        }
        ImGui::SeparatorText(("On the map (" + std::to_string(total) + ")").c_str());
        if (map_filter[0]) ImGui::TextDisabled("filtered: %s", map_filter);
        ImGui::BeginChild("##overview-list");
        for (auto& [kind, rows] : by_kind) {
            std::sort(rows.begin(), rows.end(),
                      [](const Row& a, const Row& b) { return rune_title(*a.n) < rune_title(*b.n); });
            const std::string kname = kind == "mapshape" ? "Regions" : kind == "refpoint" ? "Reference points"
                                                                                    : humanize(kind);
            const std::string head = std::string(glyph_icon(kind)) + "  " + kname + "  (" +
                                     std::to_string(rows.size()) + ")";
            if (!ImGui::CollapsingHeader((head + "##" + kind).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;
            for (const auto& r : rows) {
                ImGui::PushID(r.n->name.c_str());
                if (ImGui::Selectable(rune_title(*r.n).c_str(), ed.selected(r.n->name))) {
                    map_cam.x = (float)r.lo;
                    map_cam.y = (float)r.la;
                    pending_cmds.push_back(maiz::compile_camera(map_cam, hormiga::canvas::camera_key("view.map.camera", map_canvas)));
                    ed.selection = {r.n->name};
                }
                if (kind != "mapshape" && kind != "refpoint" && ImGui::BeginPopupContextItem("##rowmenu")) {
                    marker_menu_items(*r.n); // the same menu as the marker on the map
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
        if (total == 0)
            ImGui::TextDisabled(map_filter[0] ? "nothing on the map matches the filter"
                                              : "nothing placed yet - right-click the map");
        ImGui::EndChild();
    }
    ImGui::End();

    // ── LAYERS: a drawing application's layer panel ──────────────────────────
    if (show_manage_views) { // "edit the map's layers" from elsewhere (the Builder's map block)
        ImGui::SetNextWindowFocus();
        show_manage_views = false;
    }
    ImGui::Begin("Layers##map");
    {
        // the stack's own toolbar: new, duplicate, delete
        if (ImGui::Button(ICON_FA_PLUS "  Layer")) map_new_earth(map_cam.y, map_cam.x, (int)map_cam.zoom);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("a new layer on top: its own colours and rules");
        ImGui::SameLine();
        ImGui::BeginDisabled(!cur);
        if (ImGui::Button(ICON_FA_CLONE "##dup") && cur) {
            const std::string name = mint_name("layer");
            int top = 0;
            for (auto* l : layers) top = std::max(top, std::atoi(hormiga::temper::field_value(*l, "order").c_str()));
            std::vector<std::string> cmds = {"rune new map " + name,
                                             "set " + name + " title " + json_str(rune_title(*cur) + " copy"),
                                             "set " + name + " order \"" + std::to_string(top + 1) + "\"",
                                             "tag " + name + " +type:map"};
            for (const char* k : {"source", "center", "zoom", "channel", "visible", "label_scale", "show_labels",
                                  "layer_opacity", "layer_brightness", "no_overlap", "label_color", "filter",
                                  "canvas"}) {
                const std::string v = hormiga::temper::field_value(*cur, k);
                if (!v.empty()) cmds.push_back("set " + name + " " + k + " " + json_str(v));
            }
            const std::string rj = hormiga::temper::field_value(*cur, "rules");
            if (!rj.empty()) cmds.push_back("setjson " + name + " rules " + json_arg(rj));
            pending_cmds.push_back(maiz::compile_commit(cmds));
            map_sel = name;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("duplicate this layer");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_TRASH "##del") && cur) {
            pending_cmds.push_back("rm " + cur->name);
            map_sel.clear();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("delete this layer (undoable).\nWhat is on the map stays: a layer is a way of\n"
                              "drawing things, not a box they live in.");
        ImGui::EndDisabled();
        ImGui::Spacing();

        // the stack, TOP FIRST, as every drawing application lists it
        int move_from = -1, move_dir = 0;
        for (int i = (int)layers.size() - 1; i >= 0; --i) {
            const maiz::SceneNode& l = *layers[(size_t)i];
            ImGui::PushID(l.name.c_str());
            const bool active = l.name == map_sel;
            bool vis = hormiga::temper::field_value(l, "visible") != "0";
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::Button(vis ? ICON_FA_EYE : ICON_FA_EYE_SLASH))
                pending_cmds.push_back("set " + l.name + " visible \"" + (vis ? "0" : "1") + "\"");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(vis ? "hide this layer" : "show this layer");
            ImGui::SameLine();
            const std::string lch = hormiga::temper::field_value(l, "channel");
            const bool own = !lch.empty() && lch != hormiga::canvas::shared_channel(map_canvas);
            const std::string lbl = std::string(layer_name_of(l)) + (own ? "   " ICON_FA_CODE_BRANCH : "");
            if (ImGui::Selectable(lbl.c_str(), active, 0,
                                  ImVec2(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 2 - 8, 0)))
                map_sel = l.name;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s%s", active ? "the layer being edited" : "click to edit this layer",
                                  own ? "\n" ICON_FA_CODE_BRANCH " keeps its own positions" : "");
            if (ImGui::BeginPopupContextItem("##layermenu")) {
                if (ImGui::MenuItem("Edit this layer")) map_sel = l.name;
                if (ImGui::MenuItem(own ? "Share positions with the other layers" : "Give it its own positions"))
                    pending_cmds.push_back("set " + l.name + " channel " +
                                           json_str(own ? hormiga::canvas::shared_channel(map_canvas)
                                                        : hormiga::canvas::own_channel(map_canvas, l.name)));
                if (ImGui::MenuItem("Delete")) {
                    pending_cmds.push_back("rm " + l.name);
                    if (active) map_sel.clear();
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(i == (int)layers.size() - 1);
            if (ImGui::ArrowButton("##up", ImGuiDir_Up)) move_from = i, move_dir = +1;
            ImGui::EndDisabled();
            ImGui::SameLine(0, 2);
            ImGui::BeginDisabled(i == 0);
            if (ImGui::ArrowButton("##down", ImGuiDir_Down)) move_from = i, move_dir = -1;
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        // a move swaps two neighbours; the stack is renumbered 0..n-1 so the
        // order never depends on numbers written before `order` existed
        if (move_from >= 0) {
            std::vector<const maiz::SceneNode*> re = layers;
            std::swap(re[(size_t)move_from], re[(size_t)(move_from + move_dir)]);
            std::vector<std::string> cmds;
            for (size_t k = 0; k < re.size(); ++k)
                if (hormiga::temper::field_value(*re[k], "order") != std::to_string(k))
                    cmds.push_back("set " + re[k]->name + " order \"" + std::to_string(k) + "\"");
            if (!cmds.empty()) pending_cmds.push_back(maiz::compile_commit(cmds));
        }
        if (layers.empty()) ImGui::TextDisabled("no layers yet");

        // the background: the base map, under every layer; on a drawn canvas,
        // its floor (name, size, grid), which is the canvas rune's own fields
        ImGui::Separator();
        if (cv_rune) {
            ImGui::TextDisabled(ICON_FA_TABLE_CELLS "  The canvas itself (under every layer)");
            maiz::WidgetContext wctx{scene, pending_cmds, std::string(), 0.0f};
            for (const auto& fl : cv_rune->fields)
                if (fl.key == "title" || fl.key == "width" || fl.key == "height" || fl.key == "grid" ||
                    fl.key == "unit") {
                    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Depth (m)").x - 12);
                    maiz::widget_field(wctx, widgets, *cv_rune, fl);
                }
            maiz::dim_wrapped("Metres from the top-left corner. Draw rooms, aisles and shelves with the "
                              "rectangle tool; a region's tags go to everything placed inside it.");
        } else {
        ImGui::TextDisabled(ICON_FA_MAP "  Base map (under every layer)");
        const int bsi = std::clamp(basemap_src, 0, kBaseSourceCount - 1);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##basesrc", kBaseSources[bsi].label)) {
            for (int i = 0; i < kBaseSourceCount; ++i)
                if (ImGui::Selectable(kBaseSources[i].label, i == bsi)) {
                    basemap_src = i;
                    dispatch_and_reproject(std::string("config set ui.basemap \"") + kBaseSources[i].key + "\"");
                }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(-90);
        ImGui::SliderFloat("Brightness##base", &basemap_brightness, 0.3f, 1.0f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char b[56];
            std::snprintf(b, sizeof b, "config set ui.basemap_brightness \"%.2f\"", basemap_brightness);
            pending_cmds.push_back(b);
        }
        ImGui::SetNextItemWidth(-90);
        ImGui::SliderFloat("Fade##base", &basemap_fade, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char b[56];
            std::snprintf(b, sizeof b, "config set ui.basemap_fade \"%.2f\"", basemap_fade);
            pending_cmds.push_back(b);
        }
        } // Earth's base map

        // THE LAYER BEING EDITED: its name, its look under others, its rules, its labels
        if (cur) {
            ImGui::SeparatorText((std::string("Layer: ") + layer_name_of(*cur)).c_str());
            for (const auto& fl : cur->fields)
                if (fl.key == "title") {
                    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Name").x - ImGui::GetStyle().ItemInnerSpacing.x - 4);
                    maiz::WidgetContext wctx{scene, pending_cmds, std::string(), 0.0f};
                    maiz::widget_field(wctx, widgets, *cur, fl);
                }
            float op, lb;
            ImGui::SetNextItemWidth(-90);
            if (slider_field("Opacity", view_layer_opacity(cur), 0.05f, 1.0f, "%.2f", &op)) {
                char b[48];
                std::snprintf(b, sizeof b, "%.2f", op);
                pending_cmds.push_back("set " + cur->name + " layer_opacity \"" + b + "\"");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("how this layer looks while another one is being edited");
            ImGui::SetNextItemWidth(-90);
            if (slider_field("Brightness", view_layer_brightness(cur), 0.3f, 1.5f, "%.2f", &lb)) {
                char b[48];
                std::snprintf(b, sizeof b, "%.2f", lb);
                pending_cmds.push_back("set " + cur->name + " layer_brightness \"" + b + "\"");
            }
            const std::string ch = hormiga::temper::field_value(*cur, "channel");
            bool own = !ch.empty() && ch != hormiga::canvas::shared_channel(map_canvas);
            if (ImGui::Checkbox("Its own positions", &own))
                pending_cmds.push_back("set " + cur->name + " channel " +
                                       json_str(own ? hormiga::canvas::own_channel(map_canvas, cur->name)
                                                    : hormiga::canvas::shared_channel(map_canvas)));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("off: things sit where they sit on every layer.\n"
                                  "on: moving something on this layer moves it only here\n"
                                  "(nothing is copied until you move it)");
        ImGui::SeparatorText("Rules: tags -> colour, icon, shape");
        if (cur) {
            bool rules_dirty = false;
            auto stage_edit = [&](int i) { // load a rule into the editor window
                const auto& r = active_rules[i];
                rule_edit_idx = i;
                std::snprintf(rule_name, sizeof rule_name, "%s", r.name.c_str());
                rule_tags = r.tags;
                rule_tag_input[0] = 0;
                rule_icon = 0;
                for (int ii = 0; ii < (int)(sizeof kMarkerIcons /
                                            sizeof kMarkerIcons[0]); ++ii)
                    if (r.icon == kMarkerIcons[ii].tag) rule_icon = ii + 1;
                rule_color = 0;
                for (int ci = 0; ci < (int)(sizeof kMarkerColors /
                                            sizeof kMarkerColors[0]); ++ci)
                    if (r.color == kMarkerColors[ci].tag) rule_color = ci + 1;
                rule_shape = 0;
                for (int si = 0; si < (int)(sizeof kMarkerShapes /
                                            sizeof kMarkerShapes[0]); ++si)
                    if (r.shape == kMarkerShapes[si]) rule_shape = si;
                show_rule_editor = true;
            };
            // live audit: for each data entity, the FIRST matching rule wins;
            // a later rule that also matches is SHADOWED (never styles it).
            std::vector<int> match_count(active_rules.size(), 0);
            std::vector<int> shadowed_by(active_rules.size(), -1);
            for (const auto& node : scene.nodes) {
                if (node.glyph == "map" || node.glyph == "image" ||
                    node.glyph == "note")
                    continue;
                int first = -1;
                for (size_t i = 0; i < active_rules.size(); ++i) {
                    if (active_rules[i].tags.empty() ||
                        !maiz::node_matches(rule_filter_expr(active_rules[i]), node))
                        continue;
                    if (first < 0) { first = (int)i; ++match_count[i]; }
                    else if (shadowed_by[i] < 0) shadowed_by[i] = first;
                }
            }
            for (size_t i = 0; i < active_rules.size(); ++i) {
                ImGui::PushID((int)i);
                if (ImGui::SmallButton("x")) {
                    active_rules.erase(active_rules.begin() + i--);
                    rules_dirty = true;
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();
                const auto& r = active_rules[i];
                const char* ig = nullptr;
                for (const auto& ic : kMarkerIcons)
                    if (r.icon == ic.tag) ig = ic.glyph;
                ImU32 rc = IM_COL32(60, 60, 60, 255);
                for (const auto& c : kMarkerColors)
                    if (r.color == c.tag) rc = c.col;
                std::string lbl = std::string(ig ? ig : "") + " " + r.name + "  [" +
                                  rule_filter_expr(r) + "]  (" +
                                  std::to_string(match_count[i]) + ")";
                if (shadowed_by[i] >= 0) lbl = ICON_FA_TRIANGLE_EXCLAMATION " " + lbl;
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::ColorConvertU32ToFloat4(rc));
                bool open_edit = ImGui::Selectable(lbl.c_str());
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    if (shadowed_by[i] >= 0)
                        ImGui::SetTooltip(
                            "conflict: '%s' matches first and wins for some\n"
                            "entities (first rule wins). Reorder or narrow tags.\n"
                            "click to edit - right-click for more",
                            active_rules[shadowed_by[i]].name.c_str());
                    else
                        ImGui::SetTooltip("styles %d on the map\n"
                                          "click to edit - right-click for more",
                                          match_count[i]);
                }
                if (open_edit) stage_edit((int)i);
                if (ImGui::BeginPopupContextItem("##rulemenu")) { // author #5
                    if (ImGui::MenuItem("Edit...")) stage_edit((int)i);
                    if (ImGui::MenuItem("Duplicate")) {
                        MapRule dup = r;
                        dup.name += " copy";
                        active_rules.insert(active_rules.begin() + i + 1, dup);
                        rules_dirty = true;
                    }
                    ImGui::BeginDisabled(i == 0);
                    if (ImGui::MenuItem("Move up (higher priority)")) {
                        std::swap(active_rules[i], active_rules[i - 1]);
                        rules_dirty = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(i + 1 >= active_rules.size());
                    if (ImGui::MenuItem("Move down (lower priority)")) {
                        std::swap(active_rules[i], active_rules[i + 1]);
                        rules_dirty = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) {
                        active_rules.erase(active_rules.begin() + i);
                        rules_dirty = true;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("+ Rule")) {
                rule_edit_idx = -1;
                rule_name[0] = 0;
                rule_tags.clear();
                rule_tag_input[0] = 0;
                rule_icon = 0;
                rule_color = 0;
                rule_shape = 0;
                show_rule_editor = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(n) = how many it styles; " ICON_FA_TRIANGLE_EXCLAMATION
                                " = shadowed");
            if (rules_dirty) save_rules(cur->name);

            // ── MAP CONFIG (author #6): view display knobs, stored as fields on
            // the view rune ("internally rules"), surfaced like settings. They
            // shape BOTH the live canvas and the PNG export so the exported
            // image shows the labels/sizes you set. ─────────────────────────
            ImGui::SeparatorText("Labels");
            bool show_lbl = view_show_labels(cur);
            if (ImGui::Checkbox("Show labels", &show_lbl))
                pending_cmds.push_back("set " + cur->name + " show_labels \"" +
                                       (show_lbl ? "1" : "0") + "\"");
            float lscale;
            ImGui::SetNextItemWidth(160);
            if (slider_field("Label size", view_label_scale(cur), 0.6f, 2.5f,
                             "%.2fx", &lscale)) {
                char b[48];
                std::snprintf(b, sizeof b, "%.2f", lscale);
                pending_cmds.push_back("set " + cur->name + " label_scale \"" +
                                       b + "\"");
            }
            bool nol = view_no_overlap(cur);
            if (ImGui::Checkbox("No overlap (labels dodge)", &nol))
                pending_cmds.push_back("set " + cur->name + " no_overlap \"" +
                                       (nol ? "1" : "0") + "\"");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("labels try right/left/above/below; when\n"
                                  "nothing fits, a label yields (markers\n"
                                  "always draw)");
            // label COLOR (author: "changes to font color, fonts in general")
            std::string cur_lc = hormiga::temper::field_value(*cur, "label_color");
            if (cur_lc.empty()) cur_lc = "dark";
            ImGui::SetNextItemWidth(160);
            if (ImGui::BeginCombo("Label color", cur_lc.c_str())) {
                const char* fixed[] = {"dark", "black", "white"};
                for (const char* f : fixed)
                    if (ImGui::Selectable(f, cur_lc == f))
                        pending_cmds.push_back("set " + cur->name +
                                               " label_color \"" + f + "\"");
                for (const auto& c : kMarkerColors) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImGui::ColorConvertU32ToFloat4(c.col));
                    bool pick = ImGui::Selectable(c.tag, cur_lc == c.tag);
                    ImGui::PopStyleColor();
                    if (pick)
                        pending_cmds.push_back("set " + cur->name +
                                               " label_color \"" + c.tag + "\"");
                }
                ImGui::EndCombo();
            }
            }
        }
    }
    ImGui::End(); // Layers

    // ── ACTIONS: the map, out of the application ──────────────────────────────
    ImGui::Begin("Actions##map");
    if (!cur) {
        maiz::dim_wrapped("Add a layer first (Layers, + Layer).");
    } else if (cvs.plan()) {
        ImGui::TextDisabled("Canvas: %s", cvs.title.c_str());
        maiz::dim_wrapped("A picture of a floor plan, and a floor plan on a website, are not built yet: "
                          "both exports draw Earth's tiles today. They come with the floor plan's own "
                          "export (okf/concepts/sections/gis/canvases.md).");
    } else {
        static int doc_pick = 0;
        ImGui::TextDisabled("Layer: %s", layer_name_of(*cur));
        ImGui::SeparatorText(ICON_FA_IMAGE "  A picture (PNG)");
        maiz::dim_wrapped("The layer being edited, at the view on screen now, with its labels and the "
                          "base map. For a newsletter, a flier, anything a picture goes in. Notes and "
                          "reference points never appear in it.");
        if (ImGui::Button(ICON_FA_DOWNLOAD "  Export a picture", ImVec2(-FLT_MIN, 0))) export_map_png(cur->name);

        ImGui::SeparatorText(ICON_FA_GLOBE "  On a website, or in a newsletter");
        maiz::dim_wrapped("A map block shows a layer: on a website, a map visitors can move and zoom but "
                          "never change; in a newsletter, a picture. Contacts and notes never appear in "
                          "either. It opens at the layer's home view.");
        const std::string home_c = hormiga::temper::field_value(*cur, "center");
        const std::string home_z = hormiga::temper::field_value(*cur, "zoom");
        ImGui::TextDisabled("home view: %s  zoom %s", home_c.empty() ? "(not set)" : home_c.c_str(),
                            home_z.empty() ? "-" : home_z.c_str());
        if (ImGui::Button(ICON_FA_CROSSHAIRS "  Make the view on screen its home", ImVec2(-FLT_MIN, 0))) {
            char c[64];
            std::snprintf(c, sizeof c, "%.6f,%.6f", (double)map_cam.y, (double)map_cam.x);
            pending_cmds.push_back(maiz::compile_commit(
                {"set " + cur->name + " center " + json_str(c),
                 "set " + cur->name + " zoom \"" + std::to_string((int)std::lround(map_cam.zoom)) + "\""}));
        }
        const std::vector<std::string> docs = list_documents();
        if (docs.empty()) {
            ImGui::TextDisabled("no documents yet (the Builder makes them)");
        } else {
            doc_pick = std::clamp(doc_pick, 0, (int)docs.size() - 1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##mapdoc", docs[(size_t)doc_pick].c_str())) {
                for (int i = 0; i < (int)docs.size(); ++i)
                    if (ImGui::Selectable(docs[(size_t)i].c_str(), i == doc_pick)) doc_pick = i;
                ImGui::EndCombo();
            }
            const std::string& doc = docs[(size_t)doc_pick];
            if (ImGui::Button((std::string(ICON_FA_PLUS "  Add this map to ") + doc).c_str(), ImVec2(-FLT_MIN, 0))) {
                // the Builder's own verb, in the document's mantle, at the end of it
                maiz::ProjectOptions po;
                po.mantle = doc;
                const maiz::Scene ds = maiz::project_scene(core, po);
                int maxrow = -1;
                for (const auto& n : ds.nodes) maxrow = std::max(maxrow, hormiga::doc_field_int(n, "row", -1));
                const std::string name = mint_name("map-block");
                pending_cmds.push_back("use " + doc);
                pending_cmds.push_back("doc place map_embed " + name + " " + std::to_string(maxrow + 1));
                pending_cmds.push_back("set " + name + " view " + json_str(cur->name));
                pending_cmds.push_back(std::string("use ") + kDataMantle);
                toast("added this map to " + doc + ", at the end - move it in the Builder");
            }
        }
    }
    ImGui::End();
}
