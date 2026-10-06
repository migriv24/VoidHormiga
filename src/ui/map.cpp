/* ui/map.cpp — Territory: web-mercator math, the tile fetcher, the
 * per-view position channels, map creation, and the slippy map itself. Split
 * out of app.cpp 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * NOTE THE TWO HALVES. In app.cpp these were separated by the entire calendar,
 * for no reason other than the order they were written in. They are one subject
 * and they are now one file, which is most of what a split is for: the mercator
 * helpers sit next to their only callers, and `static` says so.
 *
 * No geographic type touches the core. Positions are a VIEW model written
 * through the dispatcher's `place` verb (the view slice, SPEC §6) — which is
 * also why Reyna's `Site` locus is named to stay clear of it.
 */
#include "app/app_internal.hpp"
#include "domain/bestow.hpp" // one inside-the-shape test
#include "render/mercator.hpp"
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

// ── Territory placeholder (the map — concept: okf/concepts/sections/gis/territory.md) ────

// web-mercator math now lives in render/mercator.hpp — the PNG export needs the
// SAME projection as this canvas, which only became visible when it moved out
using hormiga::merc_lat;
using hormiga::merc_lon;
using hormiga::merc_x;
using hormiga::merc_y;

// ── the background tile fetcher (pure I/O; the core is never touched) ───────

void HormigaApp::TileFetcher::start(std::function<bool(const std::string&, const std::string&)> get) {
    fetch = std::move(get);
    for (int w = 0; w < 4; ++w) // parallel: fetches are latency-bound
        workers.emplace_back([this, w] {
            (void)w;
            for (;;) {
                std::string path, url;
                {
                    std::unique_lock<std::mutex> lk(mu);
                    cv.wait(lk, [this] { return stop.load() || !queue.empty(); });
                    if (stop.load()) return;
                    path = queue.front();
                    queue.pop_front();
                    url = url_of[path];
                }
                ++in_flight;
                if (!std::filesystem::exists(path)) {
                    std::filesystem::create_directories(
                        std::filesystem::path(path).parent_path());
                    // download to a temp name, then rename: the final path
                    // only ever exists COMPLETE (a half-written file would
                    // cache a permanent decode failure on the main thread)
                    std::string tmp = path + ".part" + std::to_string(w);
                    fetch(url, tmp); // curl on a desktop, the platform's HTTP on a phone
                    std::error_code ec;
                    auto size = std::filesystem::file_size(tmp, ec);
                    if (!ec && size > 100) { // a real tile, not an error stub
                        std::filesystem::rename(tmp, path, ec);
                    } else {
                        std::filesystem::remove(tmp, ec); // retry next session
                    }
                }
                --in_flight;
            }
        });
}
void HormigaApp::TileFetcher::want(const std::string& url, const std::string& path) {
    std::lock_guard<std::mutex> lk(mu);
    if (queued.count(path)) return;
    queued.insert(path);
    url_of[path] = url;
    queue.push_back(path);
    cv.notify_one();
}
int HormigaApp::TileFetcher::pending() {
    std::lock_guard<std::mutex> lk(mu);
    return (int)queue.size() + in_flight.load();
}
HormigaApp::TileFetcher::~TileFetcher() {
    stop = true;
    cv.notify_all();
    for (auto& w : workers)
        if (w.joinable()) w.join();
}

// ── position channels (the per-view location model) ─────────────────────────
// pos(rune, view) = geo_<channel> if set, else geo (main). Views sharing a
// channel are locked BY CONSTRUCTION (one field, no sync); a forked view
// stores only its divergences (copy-on-write). See okf/concepts/sections/gis/territory.md.

std::string HormigaApp::geo_field_for(const std::string& ch) {
    return (ch.empty() || ch == "main") ? "geo" : "geo_" + ch;
}
std::string HormigaApp::view_geo(const maiz::SceneNode& n, const std::string& ch) {
    if (!ch.empty() && ch != "main") {
        std::string v = hormiga::temper::field_value(n, ("geo_" + ch).c_str());
        if (!v.empty()) return v; // this view's own position
    }
    return hormiga::temper::field_value(n, "geo"); // fallback: main
}

/* #4: fan-out layout. For every entity whose `ref` field names a REFPOINT,
 * compute its display position = the refpoint's geo + a pixel offset so
 * co-located siblings don't overlap. Siblings ring around the point (6 per
 * ring, spiralling outward for more). The offset is in SCREEN pixels, so it
 * stays spread at any zoom. Returns entity name → {ref geo, dx, dy}. */
std::map<std::string, HormigaApp::RefFan> HormigaApp::ref_fans(
    const maiz::Scene& s, [[maybe_unused]] const std::string& ch) const {
    std::map<std::string, std::pair<double, double>> rp; // refpoint → geo
    for (const auto& n : s.nodes)
        if (n.glyph == "refpoint") {
            double la, lo;
            if (hormiga::parse_geo(hormiga::temper::field_value(n, "geo"), la, lo))
                rp[n.name] = {la, lo};
        }
    std::map<std::string, std::vector<std::string>> kids; // ref → child names
    for (const auto& n : s.nodes) {
        if (n.glyph == "refpoint" || n.glyph == "mapshape" || n.glyph == "map")
            continue;
        std::string r = hormiga::temper::field_value(n, "ref");
        if (!r.empty() && rp.count(r)) kids[r].push_back(n.name);
    }
    std::map<std::string, RefFan> out;
    for (auto& [r, names] : kids) {
        int N = (int)names.size();
        auto [la, lo] = rp[r];
        for (int i = 0; i < N; ++i) {
            // a child that was hand-dragged carries a MANUAL pixel offset
            // (`ref_off` = "dx,dy") — that overrides the auto-fan. The offset is
            // relative to the refpoint, so moving the refpoint carries it along
            // and the drag is a re-arrangement, never a new absolute position.
            const maiz::SceneNode* cn = s.find(names[i]);
            float mdx, mdy;
            std::string off = cn ? hormiga::temper::field_value(*cn, "ref_off") : "";
            if (!off.empty() && std::sscanf(off.c_str(), "%f,%f", &mdx, &mdy) == 2) {
                out[names[i]] = {la, lo, mdx, mdy};
                continue;
            }
            int ring = i / 6, in_ring = i % 6;
            int ring_n = std::min(6, N - ring * 6);
            float radius = 22.0f + ring * 22.0f;
            float ang = (float)in_ring * 6.2831853f / std::max(1, ring_n) - 1.5708f;
            out[names[i]] = {la, lo, std::cos(ang) * radius, std::sin(ang) * radius};
        }
    }
    return out;
}

std::string HormigaApp::rules_to_json(const std::vector<MapRule>& rules) {
    nlohmann::json rj = nlohmann::json::array();
    for (const auto& r : rules)
        rj.push_back({{"name", r.name},
                      {"tags", r.tags},
                      {"icon", r.icon},
                      {"color", r.color},
                      {"shape", r.shape}});
    return rj.dump();
}
void HormigaApp::save_rules(const std::string& view_rune) {
    pending_cmds.push_back("setjson " + view_rune + " rules " +
                           json_arg(rules_to_json(active_rules)));
}

/* The RULE EDITOR — a real window, not a popup (author: give it room to
 * grow). Edits one rule of the ACTIVE view: name, tag CHIPS (visibly added
 * and removable; matching = all tags, AND-joined through the one grammar),
 * icon + color dropdowns. Save = ONE setjson on the view rune. */
void HormigaApp::draw_rule_editor() {
    if (!show_rule_editor) return;
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(rule_edit_idx < 0 ? "New rule###ruleedit" : "Edit rule###ruleedit",
                     &show_rule_editor)) {
        ImGui::TextDisabled("entities matching ALL tags get this style");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##rname", "rule name (e.g. \"volunteers\")",
                                 rule_name, sizeof rule_name);
        // tag chips: clear add, clear remove
        for (size_t i = 0; i < rule_tags.size(); ++i) {
            ImGui::PushID((int)i);
            if (ImGui::SmallButton(("@" + rule_tags[i] + "  x").c_str()))
                rule_tags.erase(rule_tags.begin() + i--);
            ImGui::PopID();
            ImGui::SameLine();
        }
        // tag entry searches EXISTING tags as you type (author #4) — pick one or
        // create a new one; either way it lands as a chip
        std::string chose = tag_picker("##rtag", rule_tag_input, sizeof rule_tag_input,
                                       "+ tag (searches existing)");
        if (!chose.empty() &&
            std::find(rule_tags.begin(), rule_tags.end(), chose) == rule_tags.end())
            rule_tags.push_back(chose);
        ImGui::Spacing();
        ImGui::SetNextItemWidth(170);
        if (ImGui::BeginCombo("icon", rule_icon == 0
                                          ? "(none)"
                                          : kMarkerIcons[rule_icon - 1].label)) {
            if (ImGui::Selectable("(none)", rule_icon == 0)) rule_icon = 0;
            for (int ii = 0; ii < (int)(sizeof kMarkerIcons / sizeof kMarkerIcons[0]);
                 ++ii) {
                std::string lbl = std::string(kMarkerIcons[ii].glyph) + "  " +
                                  kMarkerIcons[ii].label;
                if (ImGui::Selectable(lbl.c_str(), rule_icon == ii + 1))
                    rule_icon = ii + 1;
            }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(170);
        if (ImGui::BeginCombo("color", rule_color == 0
                                           ? "(glyph default)"
                                           : kMarkerColors[rule_color - 1].tag)) {
            if (ImGui::Selectable("(glyph default)", rule_color == 0)) rule_color = 0;
            for (int ci = 0; ci < (int)(sizeof kMarkerColors / sizeof kMarkerColors[0]);
                 ++ci) {
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImGui::ColorConvertU32ToFloat4(kMarkerColors[ci].col));
                bool pick =
                    ImGui::Selectable(kMarkerColors[ci].tag, rule_color == ci + 1);
                ImGui::PopStyleColor();
                if (pick) rule_color = ci + 1;
            }
            ImGui::EndCombo();
        }
        // shape: circle (default) / pin / square / diamond
        ImGui::SetNextItemWidth(170);
        if (ImGui::BeginCombo("shape", kMarkerShapes[rule_shape])) {
            for (int si = 0; si < (int)(sizeof kMarkerShapes / sizeof kMarkerShapes[0]);
                 ++si)
                if (ImGui::Selectable(kMarkerShapes[si], rule_shape == si))
                    rule_shape = si;
            ImGui::EndCombo();
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(rule_tags.empty());
        if (ImGui::Button(rule_edit_idx < 0 ? "Add rule" : "Save rule",
                          ImVec2(110, 0))) {
            MapRule r{rule_name[0] ? rule_name : "rule",
                      rule_tags,
                      rule_icon ? kMarkerIcons[rule_icon - 1].tag : "",
                      rule_color ? kMarkerColors[rule_color - 1].tag : "",
                      rule_shape ? kMarkerShapes[rule_shape] : ""};
            if (rule_edit_idx >= 0 && rule_edit_idx < (int)active_rules.size())
                active_rules[rule_edit_idx] = r;
            else
                active_rules.push_back(r);
            if (!map_sel.empty()) save_rules(map_sel);
            show_rule_editor = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110, 0))) show_rule_editor = false;
        if (rule_tags.empty())
            ImGui::TextDisabled("add at least one tag");
    }
    ImGui::End();
}

// ── map creation + placement (all through the dispatcher) ───────────────────

/* A NEW LAYER (2026-10-05; it was "New Earth map" / "New view"). A layer is a
 * `map` rune: a name, a place in the stack, its own style rules, and the
 * position channel it reads (new layers share main's positions). It goes on
 * top, and becomes the layer being edited. Device-scoped name, so two devices
 * adding a layer at once never mint the same one. */
void HormigaApp::map_new_earth(double lat, double lon, int zoom) {
    const std::string name = mint_name("layer");
    int top = 0, count = 0;
    for (const auto* l : map_layers()) {
        top = std::max(top, std::atoi(hormiga::temper::field_value(*l, "order").c_str()));
        ++count;
    }
    char center[64];
    std::snprintf(center, sizeof center, "%.5f,%.5f", lat, lon); // the phone passes its own camera
    // default STYLE RULES: kinds read at a glance out of the box
    const char* default_rules =
        R"([{"name":"people","tags":["type:contact"],"icon":"user","color":""},)"
        R"({"name":"organizations","tags":["type:organization"],"icon":"house","color":""},)"
        R"({"name":"events","tags":["type:event"],"icon":"calendar","color":""},)"
        R"({"name":"incidents","tags":["type:incident"],"icon":"warning","color":"red"}])";
    pending_cmds.push_back(maiz::compile_commit(
        {"rune new map " + name, "set " + name + " source \"osm\"",
         "set " + name + " title " + json_str("Layer " + std::to_string(count + 1)),
         "set " + name + " order \"" + std::to_string(count ? top + 1 : 0) + "\"",
         "set " + name + " center " + json_str(center),
         "set " + name + " zoom \"" + std::to_string(zoom) + "\"",
         "set " + name + " channel \"main\"", // new layers share main's positions
         "setjson " + name + " rules " + json_arg(default_rules),
         "tag " + name + " +type:map"}));
    map_sel = name;
    toast("added a layer on top - it is the one you are editing now");
}

/* The layers, bottom to top: by `order`, then as the scene has them (a layer
 * made before `order` existed sits where it always did). */
std::vector<const maiz::SceneNode*> HormigaApp::map_layers() const {
    std::vector<const maiz::SceneNode*> out;
    for (const auto& n : scene.nodes)
        if (n.glyph == "map") out.push_back(&n);
    std::stable_sort(out.begin(), out.end(), [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
        return std::atoi(hormiga::temper::field_value(*a, "order").c_str()) <
               std::atoi(hormiga::temper::field_value(*b, "order").c_str());
    });
    return out;
}

void HormigaApp::map_place_new(const char* glyph) {
    const std::string name = mint_name(glyph);
    char geo[64];
    std::snprintf(geo, sizeof geo, "%.7g,%.7g", map_ctx_lat, map_ctx_lon);
    auto cmds = map_actions.run("place", scene,
                                {{"glyph", glyph}, {"name", name}, {"geo", geo},
                                 {"field", geo_field_for(active_channel)}});
    if (cmds.empty()) { toast("place declined", true); return; }
    pending_cmds.push_back(maiz::compile_commit(cmds));
    ed.selection = {name};
    toast("placed " + name + " - fill in its details in the inspector");
}

void HormigaApp::map_place_existing(const std::string& name) {
    char geo[64];
    std::snprintf(geo, sizeof geo, "%.7g,%.7g", map_ctx_lat, map_ctx_lon);
    auto cmds = map_actions.run("move", scene,
                                {{"name", name}, {"geo", geo},
                                 {"field", geo_field_for(active_channel)}});
    if (cmds.empty()) { toast("move declined", true); return; }
    pending_cmds.push_back(maiz::compile_commit(cmds));
    ed.selection = {name};
    toast("placed existing " + name + " on the map");
}

/* The marker action menu, ITEMS ONLY (caller owns Begin/EndPopup). Shared by
 * the map right-click AND the "On this map" list (author: the list should act
 * like the map). Everything queues to pending_cmds — this may be called from
 * inside a frame that still holds SceneNode pointers, so it never dispatches
 * directly (the dangling-pointer bug class). */
void HormigaApp::marker_menu_items(const maiz::SceneNode& mk) {
    ImGui::TextDisabled("%s (%s)", mk.name.c_str(), mk.glyph.c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Select (edit in inspector)")) ed.selection = {mk.name};
    if (ImGui::MenuItem("Center map here")) {
        double la, lo;
        if (hormiga::parse_geo(view_geo(mk, active_channel), la, lo)) {
            map_cam.x = (float)lo;
            map_cam.y = (float)la;
            pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
        }
    }
    std::string cur_icon = tag_value(mk, "icon");
    std::string cur_color = tag_value(mk, "color");
    if (ImGui::BeginMenu("Icon")) {
        if (ImGui::MenuItem("(none)", nullptr, cur_icon.empty()) && !cur_icon.empty())
            pending_cmds.push_back("tag " + mk.name + " -icon:" + cur_icon);
        for (const auto& ic : kMarkerIcons) {
            std::string lbl = std::string(ic.glyph) + "  " + ic.label;
            if (ImGui::MenuItem(lbl.c_str(), nullptr, cur_icon == ic.tag)) {
                std::string cmd = "tag " + mk.name;
                if (!cur_icon.empty()) cmd += " -icon:" + cur_icon;
                cmd += " +icon:" + std::string(ic.tag);
                pending_cmds.push_back(cmd);
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Color")) {
        if (ImGui::MenuItem("(glyph default)", nullptr, cur_color.empty()) &&
            !cur_color.empty())
            pending_cmds.push_back("tag " + mk.name + " -color:" + cur_color);
        for (const auto& c : kMarkerColors) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(c.col));
            bool picked = ImGui::MenuItem(c.tag, nullptr, cur_color == c.tag);
            ImGui::PopStyleColor();
            if (picked) {
                std::string cmd = "tag " + mk.name;
                if (!cur_color.empty()) cmd += " -color:" + cur_color;
                cmd += " +color:" + std::string(c.tag);
                pending_cmds.push_back(cmd);
            }
        }
        ImGui::EndMenu();
    }
    std::string cur_shape = tag_value(mk, "shape");
    if (ImGui::BeginMenu("Shape")) {
        if (ImGui::MenuItem("circle (default)", nullptr, cur_shape.empty()) &&
            !cur_shape.empty())
            pending_cmds.push_back("tag " + mk.name + " -shape:" + cur_shape);
        for (const char* sh : kMarkerShapes) {
            if (std::string(sh) == "circle") continue; // = default (no tag)
            if (ImGui::MenuItem(sh, nullptr, cur_shape == sh)) {
                std::string cmd = "tag " + mk.name;
                if (!cur_shape.empty()) cmd += " -shape:" + cur_shape;
                cmd += " +shape:" + std::string(sh);
                pending_cmds.push_back(cmd);
            }
        }
        ImGui::EndMenu();
    }
    // #4: attach this marker to a reference point — it will fan out around the
    // gizmo's shared location instead of stacking on its own coordinate
    {
        std::string cur_ref = hormiga::temper::field_value(mk, "ref");
        std::vector<const maiz::SceneNode*> refs;
        for (const auto& en : scene.nodes)
            if (en.glyph == "refpoint") refs.push_back(&en);
        if (ImGui::BeginMenu("Attach to reference point", !refs.empty())) {
            for (const auto* rf : refs) {
                std::string lbl = hormiga::temper::field_value(*rf, "label");
                std::string item = (lbl.empty() ? rf->name : lbl) + "##" + rf->name;
                bool on = (cur_ref == rf->name);
                if (ImGui::MenuItem(item.c_str(), nullptr, on))
                    pending_cmds.push_back("set " + mk.name + " ref \"" + rf->name + "\"");
            }
            ImGui::EndMenu();
        }
        if (!cur_ref.empty()) {
            if (ImGui::MenuItem("Detach from reference point"))
                pending_cmds.push_back("set " + mk.name + " ref \"\"");
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Remove from map (keep the rune)")) {
        // clear THIS view's channel position (copy-on-write aware); if that was
        // the only position, drop the located tag too
        std::string fld = geo_field_for(active_channel);
        std::vector<std::string> cmds = {"set " + mk.name + " " + fld + " \"\""};
        if (fld != "geo" && hormiga::temper::field_value(mk, "geo").empty())
            cmds.push_back("tag " + mk.name + " -located");
        else if (fld == "geo")
            cmds.push_back("tag " + mk.name + " -located");
        pending_cmds.push_back(maiz::compile_commit(cmds));
    }
    if (ImGui::MenuItem("Delete rune (undoable)")) {
        pending_cmds.push_back("rm " + mk.name);
        ed.selection.clear();
    }
}

/* Static PNG export of a map view (author #5): compose the CACHED tiles +
 * the view's markers (its channel, its rules) around the CURRENT camera into
 * exports/<view>-<stamp>.png — CPU-side (stb decode → blit → circle fill →
 * stb write), no GL. What you see is what exports. Missing tiles render as
 * flat ground — pan the area once to warm the cache first. */
void HormigaApp::draw_map_section() {
    // restore the persisted viewport once (config tier, undo-exempt — the
    // blessed compile_camera(key) pattern upstream generalized for us)
    if (!map_cam_loaded) {
        map_cam_loaded = true;
        maiz::Camera saved;
        if (maiz::parse_camera(core.dispatch("config get view.map.camera").data,
                               saved))
            map_cam = saved;
    }

    /* ── THE MAP IS A DRAWING APPLICATION'S WINDOWS (2026-10-05) ─────────────
     *
     * The author: remove the top bar ("new view", "manage views", "draw
     * shape", the dropdown) and keep "just the search bar and filters"; the
     * views become LAYERS, "like layers in a drawing application", managed in a
     * window of their own; the inspector gets "mini tabs": the inspector, a map
     * overview, the layers, and the map's actions (exporting), "similar to how
     * we restructured the builder with windows". So, as the Builder does, this
     * tab hosts its own dockspace: the canvas, and four windows docked as tabs
     * beside it, each one free to be torn off, floated or re-docked, the
     * arrangement remembered in imgui.ini. The drawing tools and the zoom live
     * ON the canvas, where a drawing application keeps them. */
    const std::vector<const maiz::SceneNode*> maps = map_layers();
    const maiz::SceneNode* cur = nullptr;
    for (auto* m : maps)
        if (m->name == map_sel) cur = m;
    if (!cur && !maps.empty()) { cur = maps.back(); map_sel = cur->name; } // the top layer

#ifdef IMGUI_HAS_DOCK
    {
        const ImGuiID dock = ImGui::GetID("map-dock");
        if (ImGui::DockBuilderGetNode(dock) == nullptr) { // seeded once; a person's own layout wins after
            ImVec2 size = ImGui::GetContentRegionAvail();
            if (size.x < 200.0f || size.y < 150.0f) size = ImVec2(1200.0f, 800.0f);
            ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dock, size);
            ImGuiID c = dock;
            const ImGuiID right = ImGui::DockBuilderSplitNode(c, ImGuiDir_Right, 0.30f, nullptr, &c);
            for (const char* w : {"Inspector##map", "Overview##map", "Layers##map", "Actions##map"})
                ImGui::DockBuilderDockWindow(w, right);
            ImGui::DockBuilderDockWindow("Map##canvas", c);
            ImGui::DockBuilderFinish(dock);
        }
        ImGui::DockSpace(dock, ImVec2(0, 0));
    }
#endif

    ImGui::Begin("Map##canvas", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // ── the one bar: search the database, and filter what the map shows ──────
    ImGui::SetNextItemWidth(std::min(260.0f, ImGui::GetContentRegionAvail().x * 0.4f));
    ImGui::InputTextWithHint("##mapsearch", ICON_FA_MAGNIFYING_GLASS "  find...  @tag  type:event", map_search,
                             sizeof map_search);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("find anything in the database: words match names and titles;\n"
                          "@tag or type:event match tags; -@tag excludes.\n"
                          "Placed: jump to it. Not placed yet: click the map to put it there.");
    ImVec2 srch_min = ImGui::GetItemRectMin(), srch_max = ImGui::GetItemRectMax();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(240.0f, ImGui::GetContentRegionAvail().x * 0.45f));
    ImGui::InputTextWithHint("##mapfilter", ICON_FA_FILTER "  show only...  @volunteer  -type:note", map_filter,
                             sizeof map_filter);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("filter the map: only what matches is drawn\n"
                          "(the same grammar as every search bar)");
    if (map_filter[0]) {
        ImGui::SameLine();
        if (ImGui::SmallButton("clear##mapfilter")) map_filter[0] = 0;
    }
    ImGui::SameLine();
    {
        int located = 0, shown = 0;
        for (const auto& n : scene.nodes)
            if (std::find(n.tags.begin(), n.tags.end(), "located") != n.tags.end()) {
                ++located;
                if (search_match(n, map_filter)) ++shown;
            }
        if (map_filter[0]) ImGui::TextDisabled("%d of %d shown", shown, located);
        else ImGui::TextDisabled("%d on the map", located);
    }
    if (map_search[0]) {
        ImGui::SetNextWindowPos(ImVec2(srch_min.x, srch_max.y + 4));
        ImGui::SetNextWindowSize(ImVec2(340, 0));
        ImGui::Begin("##map-search-results", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing);
        int shown = 0;
        for (const auto& node : scene.nodes) {
            if (node.glyph == "map" || node.glyph == "image") continue;
            if (!search_match(node, map_search)) continue;
            if (++shown > 8) { ImGui::TextDisabled("(keep typing...)"); break; }
            std::string g = view_geo(node, active_channel);
            std::string lbl = rune_title(node) + "  (" + node.glyph + ")##" + node.name;
            if (ImGui::Selectable(lbl.c_str())) {
                double la, lo;
                if (!g.empty() && hormiga::parse_geo(g, la, lo)) {
                    map_cam.x = (float)lo;      // jump to it
                    map_cam.y = (float)la;
                    if (map_cam.zoom < 15) map_cam.zoom = 15;
                    pending_cmds.push_back(
                        maiz::compile_camera(map_cam, "view.map.camera"));
                    ed.selection = {node.name};
                } else {
                    map_place_arm = node.name;  // click-to-place mode
                }
                map_search[0] = 0;
            }
            ImGui::Indent(10);
            if (!g.empty())
                ImGui::TextDisabled("on the map");
            else
                ImGui::TextColored(ImVec4(0.75f, 0.45f, 0.25f, 1),
                                   "not on the map yet - click to place");
            ImGui::Unindent(10);
        }
        if (shown == 0) ImGui::TextDisabled("nothing matches");
        ImGui::End();
    }

    if (!cur) {
        ImGui::Spacing();
        ImGui::TextWrapped("No layers yet. A map is a stack of layers over a base map: each "
                           "layer has its own colours and rules, and can be shown, hidden and "
                           "exported (okf/concepts/sections/gis/).");
        if (ImGui::Button(ICON_FA_LAYER_GROUP "  Start with one layer")) map_new_earth(map_cam.y, map_cam.x, (int)map_cam.zoom);
        ImGui::End();
        draw_map_panels();
        return;
    }

    ImGui::BeginChild("map-canvas", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    maiz::presence_focus_if_active(surfaces, "map");  // inside the window it asks about
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 16 || sz.y < 16) { // collapsed pane
        ImGui::EndChild();
        ImGui::End();
        draw_map_panels();
        return;
    }
    ImVec2 ctr(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    int z = std::clamp((int)std::lround(map_cam.zoom), 3, 19);
    double cx = merc_x(map_cam.x, z), cy = merc_y(map_cam.y, z); // center tile
    auto to_screen = [&](double lat, double lon) {
        return ImVec2((float)(ctr.x + (merc_x(lon, z) - cx) * 256.0),
                      (float)(ctr.y + (merc_y(lat, z) - cy) * 256.0));
    };
    auto to_geo = [&](ImVec2 s, double& lat, double& lon) {
        lon = merc_lon(cx + (s.x - ctr.x) / 256.0, z);
        lat = merc_lat(cy + (s.y - ctr.y) / 256.0, z);
    };

    // an input surface over the whole canvas (also clips the draw)
    dl->PushClipRect(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), true);
    ImGui::InvisibleButton("##map-surface", sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;

    // ── tiles (with ancestor/child FALLBACK so zooming never goes gray) ─────
    // While the exact tile downloads, draw the best cached stand-in: a parent
    // tile's quarter (zooming in) or the four child tiles (zooming out) —
    // the slippy-map trick that makes zoom feel instant; the crisp tile
    // replaces it the frame it lands (fetcher renames only complete files).
    int n = 1 << z;
    const BaseSource& bsrc =
        kBaseSources[std::clamp(basemap_src, 0, kBaseSourceCount - 1)];
    auto tile_tex = [&](int zz, int tx, int ty) -> HostTexture {
        char rel[96];
        std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key, zz, tx, ty);
        if (!fs::exists(base_dir / rel)) return {};
        return texture_for(rel);
    };
    int tx0 = (int)std::floor(cx - sz.x * 0.5 / 256.0) - 1;
    int tx1 = (int)std::floor(cx + sz.x * 0.5 / 256.0) + 1;
    int ty0 = std::max(0, (int)std::floor(cy - sz.y * 0.5 / 256.0) - 1);
    int ty1 = std::min(n - 1, (int)std::floor(cy + sz.y * 0.5 / 256.0) + 1);
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int txr = tx0; txr <= tx1; ++txr) {
            /* THE WORLD DECIDES WHETHER IT WRAPS. This was
             * `((txr % n) + n) % n` — correct for Earth, and silently wrong for
             * anything else: an authored map has EDGES, and wrapping one tiles a
             * fictional city into an infinite plane. `tile_column` returns false
             * off the edge of a non-wrapping world so we draw nothing there
             * rather than drawing the far side. (gis/source.hpp) */
            int tx = 0;
            if (!bsrc.tile_column(txr, z, tx)) continue;
            ImVec2 a((float)(ctr.x + (txr - cx) * 256.0),
                     (float)(ctr.y + (ty - cy) * 256.0));
            ImVec2 b(a.x + 256.0f, a.y + 256.0f);
            HostTexture t = tile_tex(z, tx, ty);
            if (t.id) {
                dl->AddImage((ImTextureID)(intptr_t)t.id, a, b);
                continue;
            }
            { // not here yet: request it…
                char url[160];
                std::snprintf(url, sizeof url, bsrc.url, z, tx, ty);
                char rel[96];
                std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key,
                              z, tx, ty);
                tiles.want(url, (base_dir / rel).string());
            }
            // …and draw the best cached stand-in meanwhile
            bool drew = false;
            for (int k = 1; k <= 4 && !drew; ++k) { // ancestors (zoom-in case)
                HostTexture pa = tile_tex(z - k, tx >> k, ty >> k);
                if (!pa.id) continue;
                float f = 1.0f / (float)(1 << k);
                ImVec2 uv0((tx & ((1 << k) - 1)) * f, (ty & ((1 << k) - 1)) * f);
                ImVec2 uv1(uv0.x + f, uv0.y + f);
                dl->AddImage((ImTextureID)(intptr_t)pa.id, a, b, uv0, uv1);
                drew = true;
            }
            if (!drew && z < 19) { // children (zoom-out case)
                for (int q = 0; q < 4; ++q) {
                    HostTexture ch = tile_tex(z + 1, tx * 2 + (q & 1), ty * 2 + (q >> 1));
                    if (!ch.id) continue;
                    ImVec2 qa(a.x + (q & 1) * 128.0f, a.y + (q >> 1) * 128.0f);
                    dl->AddImage((ImTextureID)(intptr_t)ch.id, qa,
                                 ImVec2(qa.x + 128.0f, qa.y + 128.0f));
                    drew = true;
                }
            }
            if (!drew) {
                dl->AddRectFilled(a, b, IM_COL32(228, 226, 220, 255));
                dl->AddRect(a, b, IM_COL32(210, 208, 200, 255));
            }
        }
    // ── base-map color treatment (author #3): cheap full-canvas overlays drawn
    // OVER the tiles but UNDER the data, so the base map calms down while
    // markers/labels stay crisp. Brightness = a black veil (multiply); Fade =
    // a mid-gray veil (pulls color+contrast toward gray). Raster tiles can't be
    // hue/contrast-adjusted per-pixel without a shader (a noted future ask);
    // brightness + fade + the label-free source cover the "too busy" need. ───
    ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    if (basemap_brightness < 0.999f)
        dl->AddRectFilled(p0, p1,
                          IM_COL32(0, 0, 0, (int)((1.0f - basemap_brightness) * 255)));
    if (basemap_fade > 0.001f)
        dl->AddRectFilled(p0, p1, IM_COL32(150, 150, 150, (int)(basemap_fade * 200)));
    dl->AddText(ImVec2(p0.x + 6, p0.y + sz.y - 20), IM_COL32(90, 90, 90, 200),
                bsrc.attribution);
    if (int pend = tiles.pending(); pend > 0) { // the loading indicator
        char msg[48];
        std::snprintf(msg, sizeof msg, "loading %d tile%s...", pend,
                      pend == 1 ? "" : "s");
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 ta(p0.x + sz.x - ts.x - 14, p0.y + 8);
        dl->AddRectFilled(ImVec2(ta.x - 6, ta.y - 3),
                          ImVec2(ta.x + ts.x + 6, ta.y + ts.y + 3),
                          IM_COL32(255, 255, 255, 210), 4.0f);
        dl->AddText(ta, IM_COL32(60, 60, 60, 255), msg);
    }

    // ── the view's RULES (shared parser — the rules engine is cross-view)
    // and its position CHANNEL ──────────────────────────────────────────────
    auto parse_view_rules = [](const maiz::SceneNode& view) {
        return parse_view_rules_of(view);
    };
    active_rules.clear();
    active_channel = "main";
    if (cur) {
        std::string ch = hormiga::temper::field_value(*cur, "channel");
        if (!ch.empty()) active_channel = ch;
        active_rules = parse_view_rules(*cur);
    }
    /* WHAT THIS LAYER DRAWS: everything placed, unless the filter bar narrows
     * it (any layer), the layer's own `filter` keeps only what it holds (a tag
     * query; no UI for it yet, by the author's choice), or its eye is shut. */
    const std::string layer_filter = hormiga::temper::field_value(*cur, "filter");
    const bool layer_hidden = hormiga::temper::field_value(*cur, "visible") == "0";
    auto drawn_in = [&](const maiz::SceneNode& n, const std::string& lf) {
        return search_match(n, map_filter) && (lf.empty() || maiz::node_matches(lf, n));
    };
    bool show_labels = view_show_labels(cur);   // map config (#6)
    float label_scale = view_label_scale(cur);
    bool no_overlap = view_no_overlap(cur);     // labels dodge each other
    ImU32 label_col = view_label_color(cur);
    std::vector<ImVec4> placed_labels;          // x0,y0,x1,y1 of drawn labels
    auto rule_filter_expr = [](const MapRule& r) { return rule_expr_of(r); };

    // ── collect located runes once (markers, proximity, the side panel) —
    // positions come from the ACTIVE VIEW'S CHANNEL (fallback: main) ────────
    struct Located {
        const maiz::SceneNode* node;
        double lat, lon;
        ImVec2 s;
        bool on_screen;
    };
    // #4: ref-children are FANNED around their refpoint (they share coords)
    auto fans = ref_fans(scene, active_channel);
    std::vector<Located> located_nodes;
    for (const auto& node : scene.nodes) {
        if (node.glyph == "refpoint") continue; // gizmos drawn separately
        if (layer_hidden || !drawn_in(node, layer_filter)) continue;
        double la, lo;
        ImVec2 s;
        auto fit = fans.find(node.name);
        if (fit != fans.end()) { // fanned child: refpoint geo + pixel offset
            la = fit->second.lat; lo = fit->second.lon;
            s = to_screen(la, lo);
            s.x += fit->second.dx; s.y += fit->second.dy;
        } else {
            std::string g = view_geo(node, active_channel);
            if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
            s = to_screen(la, lo);
        }
        bool on = !(s.x < p0.x - 30 || s.x > p0.x + sz.x + 30 || s.y < p0.y - 30 ||
                    s.y > p0.y + sz.y + 30);
        located_nodes.push_back({&node, la, lo, s, on});
    }

    // #4: REFERENCE-POINT gizmos (editor-only — never in exports). A hollow
    // ring + crosshair at the shared location, with thin connectors out to each
    // fanned child, so the parent-child grouping is visible while editing.
    const maiz::SceneNode* ref_hit = nullptr;
    for (const auto& node : scene.nodes) {
        if (node.glyph != "refpoint") continue;
        double la, lo;
        if (!hormiga::parse_geo(hormiga::temper::field_value(node, "geo"), la, lo))
            continue;
        // while THIS gizmo is being dragged it follows the cursor live, and its
        // fanned children preview at the new anchor (they snap on release)
        bool being_dragged = (map_drag_ref == node.name);
        ImVec2 c = being_dragged ? mouse : to_screen(la, lo);
        ImU32 gc = IM_COL32(120, 100, 70, 220);
        for (const auto& [nm, f] : fans) // connectors to children
            if (std::abs(f.lat - la) < 1e-9 && std::abs(f.lon - lo) < 1e-9) {
                ImVec2 kid(c.x + f.dx, c.y + f.dy);
                dl->AddLine(c, kid, IM_COL32(120, 100, 70, 120), 1.2f);
                if (being_dragged) // ghost the child at its previewed spot
                    dl->AddCircleFilled(kid, 4.0f, IM_COL32(120, 100, 70, 150));
            }
        dl->AddCircle(c, 9.0f, gc, 0, 2.0f);
        dl->AddLine(ImVec2(c.x - 13, c.y), ImVec2(c.x + 13, c.y), gc, 1.2f);
        dl->AddLine(ImVec2(c.x, c.y - 13), ImVec2(c.x, c.y + 13), gc, 1.2f);
        std::string lbl = hormiga::temper::field_value(node, "label");
        if (lbl.empty()) lbl = node.name;
        dl->AddText(ImVec2(c.x + 12, c.y - 20), gc,
                    ("\xE2\x97\x87 " + lbl).c_str()); // ◇ prefix = gizmo
        float dx = mouse.x - c.x, dy = mouse.y - c.y;
        if (hovered && map_drag_marker.empty() && dx * dx + dy * dy < 144)
            ref_hit = &node;
    }

    // ── the HIDDEN CONNECTION, revealed: spatial proximity (Settings toggle).
    // Derived every frame from distance — never stored, never dispatched;
    // weight (alpha/width) fades with distance (Scry-style: derive, don't
    // materialize) ──────────────────────────────────────────────────────────
    if (map_show_prox) {
        for (size_t i = 0; i < located_nodes.size(); ++i)
            for (size_t j = i + 1; j < located_nodes.size(); ++j) {
                if (!located_nodes[i].on_screen && !located_nodes[j].on_screen)
                    continue;
                double d = hormiga::geo_distance_m(
                    located_nodes[i].lat, located_nodes[i].lon,
                    located_nodes[j].lat, located_nodes[j].lon);
                if (d > map_prox_m) continue;
                float w = 1.0f - (float)(d / map_prox_m); // closer = stronger
                dl->AddLine(located_nodes[i].s, located_nodes[j].s,
                            IM_COL32(70, 110, 160, (int)(40 + 140 * w)),
                            1.0f + 2.0f * w);
            }
    }

    // ── LAYERS: other VISIBLE views composite underneath, ghosted (like an
    // art program's layers — author, 2026-07-23). Each layer renders its own
    // channel's positions with its own rules; only the ACTIVE view receives
    // interaction. Toggle visibility in Manage views. ───────────────────────
    for (auto* v : maps) {
        if (v == cur) continue;
        if (hormiga::temper::field_value(*v, "visible") == "0") continue;
        std::string vch = hormiga::temper::field_value(*v, "channel");
        if (vch.empty()) vch = "main";
        auto vrules = parse_view_rules(*v);
        float lop = view_layer_opacity(v);     // per-view layer treatment (#3)
        float lbr = view_layer_brightness(v);
        int la8 = (int)(lop * 255);
        int ring8 = (int)(lop * 0.6f * 255); // the ring fades with the fill
        auto dim = [&](ImU32 c) {              // apply layer brightness to RGB
            int r = (int)((c & 0xFF) * lbr), g = (int)(((c >> 8) & 0xFF) * lbr);
            int b = (int)(((c >> 16) & 0xFF) * lbr);
            return IM_COL32(std::min(r, 255), std::min(g, 255), std::min(b, 255),
                            la8);
        };
        const std::string vfilter = hormiga::temper::field_value(*v, "filter");
        for (const auto& node : scene.nodes) {
            if (!drawn_in(node, vfilter)) continue;
            std::string g = view_geo(node, vch);
            double la, lo;
            if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
            ImVec2 s = to_screen(la, lo);
            if (s.x < p0.x - 20 || s.x > p0.x + sz.x + 20 || s.y < p0.y - 20 ||
                s.y > p0.y + sz.y + 20)
                continue;
            ImU32 col = IM_COL32(120, 120, 130, 255); // ghosted default (pre-dim)
            for (const auto& r : vrules) {
                if (r.tags.empty() || !maiz::node_matches(rule_filter_expr(r), node))
                    continue;
                for (const auto& c : kMarkerColors)
                    if (r.color == c.tag) col = c.col;
                break;
            }
            dl->AddCircleFilled(s, 5.0f, dim(col));
            dl->AddCircle(s, 5.0f, IM_COL32(255, 255, 255, ring8), 0, 1.5f);
        }
    }

    // ── #3 MAP SHAPES (drawn annotations): rect/ellipse runes over the map,
    // colored by the same rules/tags as markers, under the markers. Click a
    // shape to select it; right-click for its menu. ────────────────────────
    const maiz::SceneNode* shape_hit = nullptr;
    for (const auto& node : scene.nodes) {
        if (node.glyph != "mapshape") continue;
        double la1, lo1, la2, lo2;
        if (!hormiga::parse_geo(hormiga::temper::field_value(node, "geo1"), la1, lo1) ||
            !hormiga::parse_geo(hormiga::temper::field_value(node, "geo2"), la2, lo2))
            continue;
        ImVec2 a = to_screen(la1, lo1), b = to_screen(la2, lo2);
        ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y));
        ImVec2 mx(std::max(a.x, b.x), std::max(a.y, b.y));
        // color: rules → explicit color: tag → a default green
        ImU32 col = IM_COL32(46, 107, 79, 255);
        for (const auto& r : active_rules) {
            if (r.tags.empty() || !maiz::node_matches(rule_filter_expr(r), node))
                continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) col = c.col;
            break;
        }
        std::string ctag = tag_value(node, "color");
        for (const auto& c : kMarkerColors)
            if (ctag == c.tag) col = c.col;
        // ALLOMONE, the `map` surface (roadmap phase F's first step): a derived
        // colour beats the view's own rules and the explicit tag, because it is
        // the only one of the three that COMPOSES — several scripts' opinions
        // already met in the merge, and a conflict among them reached this point
        // as "nothing derived" rather than as a winner. Layering it under a
        // first-match-wins rule list would throw that away.
        if (const AlloStyle* st = allo_style_for("map", node.name))
            if (st->has_color) col = st->rgba;
        bool sel = ed.selected(node.name);
        ImU32 fill = (col & 0x00FFFFFF) | 0x33000000; // ~20% fill
        ImU32 line = (col & 0x00FFFFFF) | (sel ? 0xFF000000 : 0xBB000000);
        bool ellipse = hormiga::temper::field_value(node, "kind") == "ellipse";
        if (ellipse) {
            ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
            dl->AddEllipseFilled(c, ImVec2((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f),
                                 fill);
            dl->AddEllipse(c, ImVec2((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f), line,
                           0, 0, sel ? 2.5f : 1.5f);
        } else {
            dl->AddRectFilled(mn, mx, fill, 4.0f);
            dl->AddRect(mn, mx, line, 4.0f, 0, sel ? 2.5f : 1.5f);
        }
        std::string lbl = hormiga::temper::field_value(node, "label");
        if (!lbl.empty())
            dl->AddText(ImVec2(mn.x + 4, mn.y + 3), line, lbl.c_str());
        if (hovered && mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y &&
            mouse.y <= mx.y)
            shape_hit = &node;
    }

    // ── markers (click = select, drag = the `move` action) ─────────────────
    const maiz::SceneNode* hit = nullptr;
    for (const auto& L : located_nodes) {
        if (!L.on_screen) continue;
        const maiz::SceneNode& node = *L.node;
        ImVec2 s = L.s;
        bool selected = ed.selected(node.name);
        // glyph default → the view's first matching rule → the rune's own tags →
        // ALLOMONE's `map` style, which composes and so wins (a disagreement
        // among scripts already resolved to "nothing derived" rather than to a
        // winner, so nothing is overridden here that anybody chose). One
        // resolver for the desktop and the phone (marker_look).
        const AlloStyle* mst = allo_style_for("map", node.name);
        const MarkerLook look = marker_look(node, active_rules, mst);
        ImU32 col = look.col;
        const char* icon = look.icon;
        MShape msh = shape_from(look.shape);
        float r = (icon ? 11.0f : 6.5f) + (selected ? 2.5f : 0.0f);
        if (msh == MShape::Pin || msh == MShape::Balloon) r = std::max(r, 9.0f); // a pin needs a head to read as one
        /* PRESENCE ON THE MAP (2026-09-19). The author: "im looking at the map,
         * and right now there's no indication or highlight based on what users
         * are interacting with". The marker declares itself and Void Maiz draws
         * every mark, so it looks the same here as on a Data row. */
        maiz::presence_rect(surfaces, roster, net_settings.show, "map", node.id,
                            ImVec2(s.x - r - 2, s.y - r - 2), ImVec2(s.x + r + 2, s.y + r + 2),
                            maiz::Mark::Outline, share_now && !share_now(node));
        if (mst && mst->weight > 0) r += std::min((float)mst->weight, 6.0f);
        ImVec2 ic_at = draw_marker_shape(dl, s, r, col, IM_COL32(255, 255, 255, 230),
                                         msh);
        if (!icon && msh == MShape::Pin) // every map's pin: a white dot in the head
            dl->AddCircleFilled(ic_at, r * 0.36f, IM_COL32(255, 255, 255, 235));
        if (icon) {
            ImFont* f = ImGui::GetFont();
            float isz = r * 1.15f;
            ImVec2 is = f->CalcTextSizeA(isz, FLT_MAX, 0, icon);
            dl->AddText(f, isz, ImVec2(ic_at.x - is.x * 0.5f, ic_at.y - is.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), icon);
        }
        if (show_labels) { // map config: labels + size/color/no-overlap
            ImFont* lf = ImGui::GetFont();
            float lsz = ImGui::GetFontSize() * label_scale;
            // A derived `map-label` shadows the marker's caption for display
            // only — the rune's name is untouched.
            const std::string mlabel =
                !look.label.empty() ? look.label : marker_caption(node);
            ImVec2 tsz = lf->CalcTextSizeA(lsz, FLT_MAX, 0, mlabel.c_str());
            // candidate anchors: right, left, above, below the marker — the
            // first that doesn't collide with an already-placed label wins;
            // none fit => this label yields (markers always draw)
            const ImVec2 lc = ic_at; // beside the head of a pin, not its tip
            ImVec2 cand[4] = {{lc.x + r + 4, lc.y - lsz * 0.5f},
                              {lc.x - r - 4 - tsz.x, lc.y - lsz * 0.5f},
                              {lc.x - tsz.x * 0.5f, lc.y - r - 4 - tsz.y},
                              {lc.x - tsz.x * 0.5f, s.y + 4}};
            int pick = no_overlap ? -1 : 0;
            for (int c = 0; c < 4 && pick < 0; ++c) {
                bool clear = true;
                for (const auto& pl : placed_labels)
                    if (cand[c].x < pl.z && cand[c].x + tsz.x > pl.x &&
                        cand[c].y < pl.w && cand[c].y + tsz.y > pl.y) {
                        clear = false;
                        break;
                    }
                if (clear) pick = c;
            }
            if (pick >= 0) {
                ImVec2 tp = cand[pick];
                placed_labels.push_back(
                    {tp.x, tp.y, tp.x + tsz.x, tp.y + tsz.y});
                // a soft halo keeps any label color legible over tiles
                ImU32 halo = IM_COL32(255, 255, 255, 170);
                dl->AddText(lf, lsz, ImVec2(tp.x + 1, tp.y + 1), halo,
                            mlabel.c_str());
                dl->AddText(lf, lsz, ImVec2(tp.x - 1, tp.y - 1), halo,
                            mlabel.c_str());
                dl->AddText(lf, lsz, tp, label_col, mlabel.c_str());
            }
        }
        float dx = mouse.x - ic_at.x, dy = mouse.y - ic_at.y; // aim at the head
        float hr = std::max(12.0f, r + 3);
        if (hovered && dx * dx + dy * dy < hr * hr) hit = &node;
    }

    // ── #3: DRAW-SHAPE mode takes the drag — press starts a corner, release
    // creates the mapshape rune (geo1/geo2 = the two corners). Esc cancels.
    // `was_drawing` (captured now) suppresses the marker/pan input this frame,
    // even on the release frame that clears the draw arm. ────────────────────
    bool was_drawing = (map_draw_shape != 0) || map_shape_dragging;
    if (map_draw_shape) {
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape)) map_draw_shape = 0;
        if (ImGui::IsItemActivated() && hovered) {
            to_geo(mouse, map_shape_la0, map_shape_lo0);
            map_shape_dragging = true;
        }
        if (map_shape_dragging) { // live preview
            ImVec2 a = to_screen(map_shape_la0, map_shape_lo0), b = mouse;
            ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y));
            ImVec2 mx(std::max(a.x, b.x), std::max(a.y, b.y));
            if (map_draw_shape == 2) {
                ImVec2 c((mn.x + mx.x) / 2, (mn.y + mx.y) / 2);
                dl->AddEllipse(c, ImVec2((mx.x - mn.x) / 2, (mx.y - mn.y) / 2),
                               IM_COL32(46, 107, 79, 220), 0, 0, 2);
            } else {
                dl->AddRect(mn, mx, IM_COL32(46, 107, 79, 220), 4, 0, 2);
            }
        }
        if (ImGui::IsItemDeactivated() && map_shape_dragging) {
            double la, lo;
            to_geo(mouse, la, lo);
            if (std::abs(la - map_shape_la0) > 1e-6 ||
                std::abs(lo - map_shape_lo0) > 1e-6) {
                std::string name;
                for (int i = 1;; ++i) {
                    name = "shape-" + std::to_string(i);
                    if (!scene.find(name)) break;
                }
                char g1[64], g2[64];
                std::snprintf(g1, sizeof g1, "%.7g,%.7g", map_shape_la0, map_shape_lo0);
                std::snprintf(g2, sizeof g2, "%.7g,%.7g", la, lo);
                pending_cmds.push_back(maiz::compile_commit(
                    {"rune new mapshape " + name,
                     "set " + name + " kind \"" +
                         (map_draw_shape == 2 ? "ellipse" : "rect") + "\"",
                     "set " + name + " geo1 \"" + g1 + "\"",
                     "set " + name + " geo2 \"" + g2 + "\"",
                     "tag " + name + " +type:mapshape"}));
                ed.selection = {name};
            }
            map_shape_dragging = false;
            map_draw_shape = 0; // one shape per arm (re-arm from the toolbar)
        }
    }

    // ── input: shift = SELECT (box on empty, toggle on a marker); plain drag on
    // a marker MOVES it; plain drag on empty PANS; wheel zooms (author #2) ────
    bool shift = ImGui::GetIO().KeyShift;
    if (!was_drawing && ImGui::IsItemActivated()) {
        if (hit && !shift) {
            map_drag_marker = hit->name; // plain press on a marker → move it
        } else if (ref_hit && !shift) {  // plain press on a gizmo → MOVE it (#4)
            map_drag_ref = ref_hit->name;
            ed.selection = {ref_hit->name};
        } else if (shape_hit && !hit && !shift) { // a shape (no marker) → select
            ed.selection = {shape_hit->name};
        } else if (hit && shift) {       // shift-click a marker → toggle it
            if (ed.selected(hit->name))
                ed.selection.erase(std::remove(ed.selection.begin(),
                                               ed.selection.end(), hit->name),
                                   ed.selection.end());
            else
                ed.selection.push_back(hit->name);
        } else if (!hit && shift) {      // shift-drag on empty → box select
            map_box_active = true;
            map_box_start = mouse;
        }
    }
    if (!was_drawing && ImGui::IsItemDeactivated()) {
        if (!map_drag_ref.empty()) { // #4: a reference point was MOVED — its whole
            double la, lo;           // fan follows, since children are relative
            to_geo(mouse, la, lo);
            char geo[64];
            std::snprintf(geo, sizeof geo, "%.7g,%.7g", la, lo);
            pending_cmds.push_back("set " + map_drag_ref + " geo \"" + geo + "\"");
            map_drag_ref.clear();
        } else if (!map_drag_marker.empty()) {
            const maiz::SceneNode* mn = scene.find(map_drag_marker);
            std::string pref = mn ? hormiga::temper::field_value(*mn, "ref") : "";
            const maiz::SceneNode* rp =
                pref.empty() ? nullptr : scene.find(pref);
            if (rp && rp->glyph == "refpoint") {
                // #4: a CHILD of a reference point moved — that's an OFFSET from
                // the shared point (a re-arrangement), never a new absolute geo.
                double rla, rlo;
                if (hormiga::parse_geo(
                        hormiga::temper::field_value(*rp, "geo"), rla, rlo)) {
                    ImVec2 rc = to_screen(rla, rlo);
                    char off[48];
                    std::snprintf(off, sizeof off, "%.1f,%.1f", mouse.x - rc.x,
                                  mouse.y - rc.y);
                    pending_cmds.push_back("set " + map_drag_marker + " ref_off \"" +
                                           off + "\"");
                }
            } else {
                double la, lo;
                to_geo(mouse, la, lo);
                char geo[64];
                std::snprintf(geo, sizeof geo, "%.7g,%.7g", la, lo);
                auto cmds = map_actions.run(
                    "move", scene,
                    {{"name", map_drag_marker}, {"geo", geo},
                     {"field", geo_field_for(active_channel)}}); // this view's chan
                if (!cmds.empty()) pending_cmds.push_back(maiz::compile_commit(cmds));
            }
            map_drag_marker.clear();
        } else if (map_box_active) { // box select done: gather everything inside
            ImVec2 a = map_box_start, b = mouse;
            float x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x);
            float y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
            if (!shift || (x1 - x0 < 3 && y1 - y0 < 3)) {} // tiny box: keep sel
            std::vector<std::string> picked;
            for (const auto& L : located_nodes)
                if (L.on_screen && L.s.x >= x0 && L.s.x <= x1 && L.s.y >= y0 &&
                    L.s.y <= y1)
                    picked.push_back(L.node->name);
            if (!picked.empty()) ed.selection = picked;
            map_box_active = false;
        } else { // pan ended: flush the viewport (config tier, undo-exempt)
            pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
        }
    }
    if (!was_drawing && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        if (!map_drag_marker.empty() || !map_drag_ref.empty() || map_box_active) {
            // staged: marker/gizmo follows cursor / box grows — nothing to pan
        } else if (d.x != 0 || d.y != 0) {
            map_cam.x = (float)merc_lon(cx - d.x / 256.0, z);
            map_cam.y = (float)merc_lat(cy - d.y / 256.0, z);
        }
    }
    if (map_box_active) { // the selection rectangle
        ImVec2 a = map_box_start, b = mouse;
        dl->AddRectFilled(a, b, IM_COL32(70, 130, 200, 40));
        dl->AddRect(a, b, IM_COL32(70, 130, 200, 200), 0, 0, 1.5f);
    }
    // drag-and-drop feel (author #3): the picked-up marker GROWS toward the
    // cursor rather than teleporting. marker_anim eases 0->1 while dragging and
    // decays after release. Gated by the "GUI animations" advanced setting —
    // off => it just tracks the cursor at full size, no easing.
    float dt = ImGui::GetIO().DeltaTime;
    bool dragging = !map_drag_marker.empty();
    if (!gui_anim) {
        marker_anim = dragging ? 1.0f : 0.0f;
    } else {
        float target = dragging ? 1.0f : 0.0f;
        marker_anim += (target - marker_anim) * std::min(1.0f, dt * 12.0f);
        if (std::fabs(marker_anim - target) < 0.01f) marker_anim = target;
    }
    if (dragging || marker_anim > 0.01f) { // staged ghost while dragging
        float grow = 9.0f + 7.0f * marker_anim; // 9 -> 16 px as it lifts
        ImU32 shadow = IM_COL32(0, 0, 0, (int)(60 * marker_anim));
        dl->AddCircleFilled(ImVec2(mouse.x + 2, mouse.y + 3), grow, shadow);
        dl->AddCircleFilled(mouse, grow, IM_COL32(255, 255, 255, 150));
        dl->AddCircle(mouse, grow, IM_COL32(60, 60, 60, 210), 0, 2.0f);
        if (dragging)
            dl->AddText(ImVec2(mouse.x + grow + 4, mouse.y - 8),
                        IM_COL32(40, 40, 40, 255), map_drag_marker.c_str());
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0) {
        int nz = std::clamp(z + (ImGui::GetIO().MouseWheel > 0 ? 1 : -1), 3, 19);
        if (nz != z) {
            // zoom about the cursor: keep the geo under the mouse fixed
            double mlat, mlon;
            to_geo(mouse, mlat, mlon);
            double fx = (mouse.x - ctr.x) / 256.0, fy = (mouse.y - ctr.y) / 256.0;
            map_cam.zoom = (float)nz;
            map_cam.x = (float)merc_lon(merc_x(mlon, nz) - fx, nz);
            map_cam.y = (float)merc_lat(merc_y(mlat, nz) - fy, nz);
            pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
        }
    }
    if (hit && !shift && ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
        map_drag_marker.empty())
        ed.selection = {hit->name}; // plain click = replace; shift handled above

    // armed placement (from the search bar): a plain click places the chosen
    // rune right here — one `move` action, one undo frame
    if (!map_place_arm.empty()) {
        dl->AddText(ImVec2(p0.x + 8, p0.y + 8), IM_COL32(180, 60, 40, 255),
                    ("click the map to place: " + map_place_arm +
                     "   (Esc cancels)").c_str());
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) map_place_arm.clear();
        if (hovered && !hit && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            to_geo(mouse, map_ctx_lat, map_ctx_lon);
            map_place_existing(map_place_arm);
            map_place_arm.clear();
        }
    } else if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ed.selection.clear(); // easy deselect → the panel shows the map list
    }

    // right-click: a MARKER gets its own menu (act on the thing); empty map
    // gets the place menu (create here)
    if (!was_drawing && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        to_geo(mouse, map_ctx_lat, map_ctx_lon);
        if (hit) {
            map_ctx_marker = hit->name;
            ImGui::OpenPopup("map-marker-menu");
        } else if (ref_hit) {
            map_ctx_ref = ref_hit->name;
            ImGui::OpenPopup("map-ref-menu");
        } else if (shape_hit) {
            map_ctx_shape = shape_hit->name;
            ImGui::OpenPopup("map-shape-menu");
        } else {
            ImGui::OpenPopup("map-place-menu");
        }
    }
    if (ImGui::BeginPopup("map-ref-menu")) { // #4: a reference point's actions
        const maiz::SceneNode* rf = scene.find(map_ctx_ref);
        if (!rf) ImGui::CloseCurrentPopup();
        else {
            std::string lbl = hormiga::temper::field_value(*rf, "label");
            ImGui::TextDisabled("reference point: %s",
                                lbl.empty() ? rf->name.c_str() : lbl.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Edit (inspector)")) ed.selection = {rf->name};
            // how many children point here
            int kids = 0;
            for (const auto& en : scene.nodes)
                if (hormiga::temper::field_value(en, "ref") == rf->name) ++kids;
            ImGui::TextDisabled("%d child marker(s) fan out here", kids);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete (detaches children)")) {
                // detach children (clear their ref) then remove the gizmo
                std::vector<std::string> cmds;
                for (const auto& en : scene.nodes)
                    if (hormiga::temper::field_value(en, "ref") == rf->name)
                        cmds.push_back("set " + en.name + " ref \"\"");
                cmds.push_back("rm " + rf->name);
                pending_cmds.push_back(maiz::compile_commit(cmds));
                ed.selection.clear();
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("map-shape-menu")) { // #3: a drawn shape's actions
        const maiz::SceneNode* sp = scene.find(map_ctx_shape);
        if (!sp) ImGui::CloseCurrentPopup();
        else {
            ImGui::TextDisabled("%s (%s)", sp->name.c_str(),
                                hormiga::temper::field_value(*sp, "kind").c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Edit (inspector)")) ed.selection = {sp->name};
            std::string cur_color = tag_value(*sp, "color");
            if (ImGui::BeginMenu("Color")) {
                if (ImGui::MenuItem("(default)", nullptr, cur_color.empty()) &&
                    !cur_color.empty())
                    pending_cmds.push_back("tag " + sp->name + " -color:" + cur_color);
                for (const auto& c : kMarkerColors) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImGui::ColorConvertU32ToFloat4(c.col));
                    bool pick = ImGui::MenuItem(c.tag, nullptr, cur_color == c.tag);
                    ImGui::PopStyleColor();
                    if (pick) {
                        std::string cmd = "tag " + sp->name;
                        if (!cur_color.empty()) cmd += " -color:" + cur_color;
                        pending_cmds.push_back(cmd + " +color:" + std::string(c.tag));
                    }
                }
                ImGui::EndMenu();
            }
            // the tags it gives: a chip editor in the Inspector (the registry's
            // "bestow" editor), never a raw string in a menu (2026-10-05)
            {
                const auto gives = hormiga::bestow::bestowed_tags(*sp);
                std::string what;
                for (const auto& t : gives) what += (what.empty() ? "" : ", ") + t;
                if (ImGui::MenuItem((std::string(ICON_FA_GIFT "  Tags it gives") +
                                     (what.empty() ? "..." : ": " + what)).c_str()))
                    ed.selection = {sp->name}; // the Inspector shows its editor
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete shape")) {
                pending_cmds.push_back("rm " + sp->name);
                ed.selection.clear();
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("map-marker-menu")) {
        const maiz::SceneNode* mk = scene.find(map_ctx_marker);
        if (!mk) ImGui::CloseCurrentPopup();
        else marker_menu_items(*mk);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("map-place-menu")) {
        ImGui::TextDisabled("place at %.5f, %.5f", map_ctx_lat, map_ctx_lon);
        ImGui::Separator();
        if (ImGui::MenuItem("New contact here")) map_place_new("contact");
        if (ImGui::MenuItem("New organization here")) map_place_new("organization");
        if (ImGui::MenuItem("New event here")) map_place_new("event");
        if (ImGui::MenuItem("New incident here")) map_place_new("incident");
        // a note about this PLACE (2026-10-04): in the app only, never exported
        if (ImGui::MenuItem("New note here")) map_place_new("note");
        // #4: a reference point — a gizmo children fan out around
        if (ImGui::MenuItem("New reference point here")) {
            std::string name;
            for (int i = 1;; ++i) {
                name = "refpoint-" + std::to_string(i);
                if (!scene.find(name)) break;
            }
            char geo[64];
            std::snprintf(geo, sizeof geo, "%.7g,%.7g", map_ctx_lat, map_ctx_lon);
            pending_cmds.push_back(maiz::compile_commit(
                {"rune new refpoint " + name, "set " + name + " geo \"" + geo + "\"",
                 "set " + name + " label \"Reference\""}));
            ed.selection = {name};
        }
        ImGui::Separator();
        ImGui::TextDisabled("place existing (search):");
        std::string ctx_pick = search_picker(
            "##ctxplace", ctx_search, sizeof ctx_search,
            [this](const maiz::SceneNode& n) {
                return n.glyph != "map" && n.glyph != "image" &&
                       n.glyph != "mapshape" && n.glyph != "refpoint" &&
                       view_geo(n, active_channel).empty(); // unplaced here (notes too)
            },
            "type a name...");
        if (!ctx_pick.empty()) {
            map_place_existing(ctx_pick);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (layer_hidden)
        dl->AddText(ImVec2(p0.x + 60, p0.y + 12), IM_COL32(150, 60, 40, 255),
                    ICON_FA_EYE_SLASH "  this layer is hidden - its eye is in Layers");

    /* ── ON THE CANVAS: the tools and the zoom (2026-10-05) ─────────────────
     * Where a drawing application keeps them: a strip of tools at the left
     * edge (select, rectangle, ellipse) and + / - at the bottom right (the
     * author's ask 1: a wheel is not the only way to zoom). Each is a small
     * child window, so a click on one is never a click on the map under it. */
    {
        const float b = ImGui::GetFrameHeight() * 1.2f;
        const ImGuiChildFlags cf = ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX |
                                   ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(1, 1, 1, 0.92f));
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 8, p0.y + 8));
        ImGui::BeginChild("##map-tools", ImVec2(0, 0), cf, ImGuiWindowFlags_NoScrollbar);
        auto tool = [&](const char* icon, const char* tip, int which) {
            const bool on = map_draw_shape == which;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.30f, 1));
            if (on) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            ImGui::PushID(which);
            if (ImGui::Button(icon, ImVec2(b, b))) map_draw_shape = which;
            ImGui::PopID();
            if (on) ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        };
        tool(ICON_FA_ARROW_POINTER, "Select and move\n(drag the map to pan; right-click to add)", 0);
        tool(ICON_FA_VECTOR_SQUARE, "Draw a rectangle: drag across the map (Esc stops)", 1);
        tool(ICON_FA_CIRCLE, "Draw an ellipse: drag across the map (Esc stops)", 2);
        ImGui::EndChild();
        ImGui::SetCursorScreenPos(ImVec2(p0.x + sz.x - b - 18, p0.y + sz.y - 2 * b - 34));
        ImGui::BeginChild("##map-zoom", ImVec2(0, 0), cf, ImGuiWindowFlags_NoScrollbar);
        auto step = [&](int dz) {
            const int nz2 = std::clamp((int)std::lround(map_cam.zoom) + dz, 3, 19);
            if (nz2 != (int)std::lround(map_cam.zoom)) {
                map_cam.zoom = (float)nz2; // about the centre: the centre is the camera
                pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
            }
        };
        ImGui::BeginDisabled(z >= 19);
        if (ImGui::Button(ICON_FA_PLUS, ImVec2(b, b))) step(+1);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("zoom in");
        ImGui::BeginDisabled(z <= 3);
        if (ImGui::Button(ICON_FA_MINUS, ImVec2(b, b))) step(-1);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("zoom out");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    dl->PopClipRect();
    ImGui::EndChild();
    ImGui::End(); // Map##canvas
    draw_map_panels(); // ui/map_panels.cpp
}

/* The `map` verb-macro front-end (Core's 2026-07-21 ruling made real): the
 * command bar calls the SAME ActionDescriptor.compile a canvas gesture will,
 * and dispatches the result as ONE `batch` — atomic, undoable, attributed.
 * `map actions` prints the manifest: an agent's discovery surface. Returns
 * true when the line was a map verb (handled + logged here). */
bool HormigaApp::view_show_labels(const maiz::SceneNode* v) const {
    if (!v) return true;
    std::string s = hormiga::temper::field_value(*v, "show_labels");
    return s != "0"; // default ON; only explicit "0" hides
}
float HormigaApp::view_layer_opacity(const maiz::SceneNode* v) const {
    if (!v) return 0.43f;
    std::string s = hormiga::temper::field_value(*v, "layer_opacity");
    if (s.empty()) return 0.43f;
    float f = (float)std::atof(s.c_str());
    return (f >= 0.05f && f <= 1.0f) ? f : 0.43f;
}
float HormigaApp::view_layer_brightness(const maiz::SceneNode* v) const {
    if (!v) return 1.0f;
    std::string s = hormiga::temper::field_value(*v, "layer_brightness");
    if (s.empty()) return 1.0f;
    float f = (float)std::atof(s.c_str());
    return (f >= 0.3f && f <= 1.5f) ? f : 1.0f;
}
bool HormigaApp::view_no_overlap(const maiz::SceneNode* v) const {
    if (!v) return true; // labels dodge each other by default (author ask)
    return hormiga::temper::field_value(*v, "no_overlap") != "0";
}
unsigned HormigaApp::view_label_color(const maiz::SceneNode* v) const {
    unsigned dark = IM_COL32(40, 40, 40, 255);
    if (!v) return dark;
    std::string s = hormiga::temper::field_value(*v, "label_color");
    if (s == "white") return IM_COL32(245, 245, 245, 255);
    if (s == "black") return IM_COL32(10, 10, 10, 255);
    for (const auto& c : kMarkerColors)
        if (s == c.tag) return c.col;
    return dark;
}

/* Parse a view rune's style rules — shared by the map canvas, the layer
 * compositor, both PNG exports, and the CALENDAR (the rules engine is
 * cross-view: okf/concepts/sections/gis/territory.md). v1 {filter} read for compat. */
std::vector<HormigaApp::MapRule> HormigaApp::parse_view_rules_of(
    const maiz::SceneNode& view) {
    std::vector<MapRule> out;
    try {
        std::string rjs = hormiga::temper::field_value(view, "rules");
        auto rj = nlohmann::json::parse(rjs.empty() ? "[]" : rjs);
        for (const auto& r : rj) {
            MapRule mr;
            mr.name = r.value("name", "rule");
            mr.icon = r.value("icon", "");
            mr.color = r.value("color", "");
            mr.shape = r.value("shape", "");
            if (r.contains("tags"))
                for (const auto& t : r["tags"]) mr.tags.push_back(t);
            else if (!r.value("filter", "").empty()) // v1 compat
                mr.tags.push_back(r.value("filter", ""));
            out.push_back(std::move(mr));
        }
    } catch (...) {}
    return out;
}
std::string HormigaApp::rule_expr_of(const MapRule& r) {
    std::string e;
    for (const auto& t : r.tags) e += (e.empty() ? "" : " AND ") + t;
    return e;
}

/* The STYLE tab — the PLACEHOLDER for the full theming workspace (builder.md
 * QE, author-decided): the Builder stays the daily drag-drop tool; making the
 * newsletter/website "look your own" (themes, fonts, logos, banners, shapes)
 * lives HERE, later. Today: two live theme colors proving the socket — both
 * render packs read them, so the real tab grows into an existing seam. */
/* The STYLE tab — the theme editor (builder.md QE). It and the Builder are one
 * system (author, 2026-07-23): the Builder arranges components, the Style tab
 * decides how they look. Every knob is config-tier (theme.*), lands as a
 * pending_cmd, and — with Live preview on — re-renders the site instantly
 * (dispatch_and_reproject marks the preview dirty). Three of the author's
 * seven style axes live here now: presets (aesthetic modes), heading font
 * (bold typography), and browser dark/light reactivity. */
/* ── THE STYLE TAB (rebuilt 2026-08-20) ──────────────────────────────────────
 *
 * The author: *"revamp the style tab, before it was more of an afterthought …
 * give it some more functionality … mostly think about websites right now.
 * always remember that now we are developing for a headless mode as well."*
 *
 * Both halves of that shaped this.
 *
 * ── EVERY KNOB IS A `config set`, AND THAT IS THE WHOLE DESIGN ───────────────
 *
 * Nothing here holds state of its own. Each control emits `config set theme.*`,
 * which is a dispatcher command — so the tab is a *view* of the theme rather
 * than an editor with its own copy, and an agent setting `theme.contrast` from
 * a script and a person moving this slider are the same change in the same log.
 * That is founding commitment 1 at the level of a colour picker, and it is why
 * this file contains no theme logic: the arithmetic lives in `app_shared.cpp`
 * where the RENDERER calls it, so the number shown here is by construction the
 * number the website will use.
 *
 * ── THE CONTRAST READOUT IS THE POINT OF THE REVAMP ──────────────────────────
 *
 * A colour picker that lets you choose unreadable text and says nothing is not
 * a design tool, it is a trap with a nice widget. This one computes the WCAG
 * ratio for every pairing the site actually generates, shows it, and says which
 * ones the renderer is going to override — so a person can see WHY their yellow
 * accent produced dark button text instead of watching it change under them.
 */

