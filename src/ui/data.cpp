/* ui/data.cpp — the Data section: kind sidebar, rune list, detail form,
 * the typed person form, Data Tools and Notes (the tag editor and recommender
 * moved to ui/tags.cpp on 2026-09-13). Split out
 * of app.cpp 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * The management UI, and the place the widget protocol (voidmaiz/widget.hpp) is
 * exercised hardest. Every edit here compiles to a dispatcher command; there is
 * no write path in this file that does not go through one.
 */
#include "app/app_internal.hpp"
#include "domain/submission.hpp"   // ONE gate for every proposed transcript
#include "json.hpp" // one typed-form field decodes a setjson value

// ── the Data section: sidebar → list → detail (the management UI) ───────────

// #2: compile a flat condition list + connector into the ONE tag grammar.
// "" (no conditions) matches everything, per the grammar's empty convention.
std::string HormigaApp::compile_filter(const std::vector<FilterTerm>& terms,
                                       int join) {
    std::string s;
    const char* op = (join == 1) ? " OR " : " AND ";
    for (const auto& t : terms) {
        if (t.tag.empty()) continue;
        if (!s.empty()) s += op;
        if (t.neg) s += "NOT ";
        s += t.tag;
    }
    return s;
}

// #2: the tag-filter BUILDER — a reusable chip bar. A connector (all/any) when
// there are ≥2 conditions, then each condition as a chip (click = negate, x =
// remove), then a "+ filter" popup that type-aheads the FULL tag vocabulary
// (namespaced axis tags included — filtering is exactly where you want them).
// Recompiles into `out` on any change; the coming rules engine reuses this over
// its own term list. Returns true the frame the expression changed.
bool HormigaApp::draw_tag_filter(const char* id, std::vector<FilterTerm>& terms,
                                 int& join, char* out, size_t outsz, char* addbuf,
                                 size_t addsz) {
    ImGui::PushID(id);
    bool changed = false;
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Filter:");
    ImGui::SameLine();
    // connector: "all of" (AND) vs "any of" (OR) — only shown with ≥2 terms
    if (terms.size() >= 2) {
        auto seg = [&](const char* lbl, int v) {
            bool on = (join == v);
            if (on)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::SmallButton(lbl) && !on) { join = v; changed = true; }
            if (on) ImGui::PopStyleColor();
            ImGui::SameLine(0, 2);
        };
        seg("all", 0);
        seg("any", 1);
        ImGui::SameLine(0, 8);
    }
    // the condition chips
    for (size_t i = 0; i < terms.size(); ++i) {
        ImGui::PushID((int)i);
        bool neg = terms[i].neg;
        if (neg)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.22f, 0.22f, 1));
        std::string lbl = (neg ? "NOT " : "") + terms[i].tag;
        if (ImGui::SmallButton(lbl.c_str())) { terms[i].neg = !neg; changed = true; }
        if (neg) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("click to negate (NOT)");
        ImGui::SameLine(0, 1);
        bool removed = ImGui::SmallButton("x");
        ImGui::PopID();
        if (removed) {
            terms.erase(terms.begin() + i);
            changed = true;
            break;
        }
        ImGui::SameLine(0, 6);
    }
    // "+ filter": a popup type-ahead over EVERY tag (incl. type:/month:/status:)
    if (ImGui::SmallButton("+ filter")) { addbuf[0] = 0; ImGui::OpenPopup("addf"); }
    if (ImGui::BeginPopup("addf")) {
        ImGui::TextDisabled("add a tag condition");
        ImGui::SetNextItemWidth(240);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##ftadd", "search tags...", addbuf, (int)addsz);
        std::set<std::string> vocab;
        for (const auto& n : scene.nodes)
            for (const auto& t : n.tags) vocab.insert(t);
        std::string typed = addbuf;
        int shown = 0;
        for (const auto& t : vocab) {
            if (!typed.empty() && !contains_ci(t, typed.c_str())) continue;
            if (++shown > 14) { ImGui::TextDisabled("(keep typing...)"); break; }
            if (ImGui::Selectable(("@" + t).c_str())) {
                terms.push_back({false, t});
                changed = true;
                ImGui::CloseCurrentPopup();
            }
        }
        if (shown == 0) ImGui::TextDisabled("no tag matches");
        ImGui::EndPopup();
    }
    if (changed) {
        std::string e = compile_filter(terms, join);
        std::snprintf(out, outsz, "%s", e.c_str());
    }
    ImGui::PopID();
    return changed;
}

void HormigaApp::draw_data_section(float /*avail_h*/) {
    // toolbar
    if (ImGui::SmallButton(data_show_connections ? "browse view" : "connections view"))
        data_show_connections = !data_show_connections;
    ImGui::SameLine();
    // #3 (2026-08-03): the tab's utilities moved into a detached Data Tools
    // window — the toolbar stays about VIEWING the data, not maintaining it.
    if (ImGui::SmallButton("Data Tools")) win_data_tools = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("import CSV, tidy date tags, and other data utilities");
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(160);
    ImGui::InputTextWithHint("##search", "search names...", search, sizeof search);
    ImGui::SameLine();
    if (ImGui::SmallButton("clear")) {
        filter[0] = 0;
        search[0] = 0;
        data_filter_terms.clear();
    }
    // #1 (UI/UX phase): list vs card view of the middle pane — a little
    // segmented control, the choice persisted (ui.data_view)
    if (!data_show_connections) {
        ImGui::SameLine(0, 16);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("view:");
        auto seg = [&](const char* lbl, int mode) {
            ImGui::SameLine(0, 2);
            bool on = (data_view_mode == mode);
            if (on)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::SmallButton(lbl) && !on) {
                data_view_mode = mode;
                dispatch_and_reproject("config set ui.data_view \"" +
                                       std::to_string(mode) + "\"");
            }
            if (on) ImGui::PopStyleColor();
        };
        seg("List", 0);
        seg("Cards", 1);
        if (data_view_mode == 1) { // card size (small/medium/large), persisted
            ImGui::SameLine(0, 12);
            ImGui::TextDisabled("size:");
            const char* sz[] = {"S", "M", "L"};
            for (int i = 0; i < 3; ++i) {
                ImGui::SameLine(0, 2);
                bool on = (data_card_size == i);
                if (on)
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::SmallButton(sz[i]) && !on) {
                    data_card_size = i;
                    dispatch_and_reproject("config set ui.data_card_size \"" +
                                           std::to_string(i) + "\"");
                }
                if (on) ImGui::PopStyleColor();
            }
        }
    }

    // ── row 2: the TAG-FILTER builder (#2) — click tags instead of typing the
    // grammar. An "advanced" toggle reveals the raw expression box (nested
    // AND/OR/parens), which the chips can't express. ─────────────────────────
    if (data_filter_advanced) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Filter:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(360);
        ImGui::InputTextWithHint("##filter", "@month:july AND type:event", filter,
                                 sizeof filter);
    } else {
        draw_tag_filter("data", data_filter_terms, data_filter_join, filter,
                        sizeof filter, filter_add_buf, sizeof filter_add_buf);
    }
    ImGui::SameLine(0, 12);
    if (ImGui::SmallButton(data_filter_advanced ? "simple" : "advanced"))
        data_filter_advanced = !data_filter_advanced;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(data_filter_advanced
                              ? "back to clicking tags"
                              : "type a raw expression (nested AND/OR, parens)");

    float body_h = ImGui::GetContentRegionAvail().y;
    const float th = 6.0f;

    if (data_show_connections) {
        // Gephi-class force-directed graph (author: NOT the node editor) —
        // see draw_physics_view
        ImVec2 area = ImGui::GetContentRegionAvail();
        float main_w = std::max(120.0f, (area.x - th) * canvas_frac);
        ImGui::BeginChild("data-phys-pane", ImVec2(main_w, body_h),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);
        draw_physics_view(body_h);
        ImGui::EndChild();
        ImGui::SameLine(0, 0);
        auto vs = maiz::splitter("##data-vsplit", true, canvas_frac, area.x - th,
                                 0.4f, 0.92f, th, body_h);
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("data-inspector", ImVec2(0, body_h));
        maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
        for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
        ImGui::EndChild();
        if (vs.released) flush_panels();
        return;
    }

    // ── resizable 3-pane layout (UI/UX phase): sidebar | list | detail, two
    // draggable splitters, fractions persisted (config view.data_panels). The
    // author's #1 complaint — nothing could resize in fullscreen — fixed here.
    ImVec2 area = ImGui::GetContentRegionAvail();
    float total_w = area.x;
    /* `clamp_fit`, not `std::clamp` — both of these cross their bounds in a
     * narrow window (`total_w < 275` for the first; the second whenever
     * `rest_w < 270`, which its own 160 floor guarantees), and a crossed
     * `std::clamp` is UB that libstdc++ turns into `abort()`. This aborted the
     * application at boot on 2026-09-02. See app_internal.hpp. */
    float side_w = clamp_fit(data_side_frac * total_w, 110.0f, total_w * 0.4f);
    float rest_w = std::max(160.0f, total_w - side_w - th);
    float list_w = clamp_fit(data_list_frac * rest_w, 150.0f, rest_w - 120.0f);

    // ── sidebar: the kinds (the predecessor's tabs, as a rail) ──────────────
    panel_shadow(ImGui::GetCursorScreenPos(),
                 ImVec2(ImGui::GetCursorScreenPos().x + side_w,
                        ImGui::GetCursorScreenPos().y + body_h));
    ImGui::BeginChild("data-sidebar", ImVec2(side_w, body_h), ImGuiChildFlags_Borders);
    std::map<std::string, int> counts;
    for (const auto& n : scene.nodes) counts[n.glyph]++;
    if (ImGui::Selectable(("All (" + std::to_string(scene.nodes.size()) + ")").c_str(),
                          kind_sel.empty()))
        kind_sel.clear();
    ImGui::Separator();
    for (const auto& e : palette.entries) {
        std::string row = e.label + " (" + std::to_string(counts[e.glyph]) + ")";
        if (ImGui::Selectable(row.c_str(), kind_sel == e.glyph)) kind_sel = e.glyph;
    }
    ImGui::EndChild();

    // splitter 1: sidebar | (list + detail)
    ImGui::SameLine(0, 0);
    auto s1 = maiz::splitter("##data-side-split", true, data_side_frac,
                             total_w - th, 0.08f, 0.4f, th, body_h);
    if (s1.released) flush_data_panels();
    ImGui::SameLine(0, 0);

    // ── list: the kind's runes + the add button ─────────────────────────────
    panel_shadow(ImGui::GetCursorScreenPos(),
                 ImVec2(ImGui::GetCursorScreenPos().x + list_w,
                        ImGui::GetCursorScreenPos().y + body_h));
    ImGui::BeginChild("data-list", ImVec2(list_w, body_h), ImGuiChildFlags_Borders);
    if (kind_sel.empty()) {
        if (ImGui::Button("+ New...", ImVec2(-1, 0))) ImGui::OpenPopup("new-kind");
        if (ImGui::BeginPopup("new-kind")) {
            for (const auto& e : palette.entries)
                /* An icon per kind (2026-09-02). This menu is a list of
                 * words of similar length; the glyph is what a person actually
                 * recognises when they open it for the hundredth time. */
                if (ImGui::MenuItem(
                        (std::string(glyph_icon(e.glyph)) + "  " + e.label)
                            .c_str()))
                    new_rune(e.glyph);
            ImGui::EndPopup();
        }
    } else {
        std::string lbl = "+ New ";
        for (const auto& e : palette.entries)
            if (e.glyph == kind_sel) lbl += e.label;
        if (ImGui::Button(lbl.c_str(), ImVec2(-1, 0))) new_rune(kind_sel);
    }
    ImGui::Separator();
    // gather the filtered set once — shared by the LIST and CARD renderers
    std::vector<const maiz::SceneNode*> rows;
    for (const auto& n : scene.nodes) {
        if (n.glyph == "note" || n.glyph == "rule") continue; // own tabs
        if (!kind_sel.empty() && n.glyph != kind_sel) continue;
        if (!contains_ci(n.name, search)) continue;
        if (!maiz::node_matches(filter, n)) continue;
        rows.push_back(&n);
    }
    auto pick = [&](const std::string& nm) { // shared click → selection rule
        if (ImGui::GetIO().KeyCtrl) ed.toggle(nm);
        else ed.selection = {nm};
    };
    if (rows.empty()) {
        ImGui::TextDisabled(scene.nodes.empty() ? "nothing here yet"
                                                : "nothing matches");
    } else if (data_view_mode == 1) {
        // ── CARD view (#1): a face per entity in a responsive grid. Fully
        // custom-drawn (InvisibleButton for hit-testing, ImDrawList for the
        // card) so cards get avatars, selection glow, and shadows. ──────────
        float avail = ImGui::GetContentRegionAvail().x;
        const float targets[] = {112.0f, 150.0f, 208.0f}; // S / M / L (ui.data_card_size)
        const float gap = 10.0f, target = targets[std::clamp(data_card_size, 0, 2)];
        int cols = std::max(1, (int)((avail + gap) / (target + gap)));
        float cw = (avail - gap * (cols - 1)) / cols;
        float r = std::min(cw * 0.30f, 40.0f);
        float ch = 2 * r + 66.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < rows.size(); ++i) {
            const maiz::SceneNode& n = *rows[i];
            if (i % cols != 0) ImGui::SameLine(0, gap);
            ImGui::PushID((int)i);
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("card", ImVec2(cw, ch));
            // only DRAW visible cards — the manual ImDrawList geometry bypasses
            // ImGui's off-screen culling, so drawing all N would overflow the
            // 16-bit index buffer (the InvisibleButton above still lays out).
            if (!ImGui::IsItemVisible()) { ImGui::PopID(); continue; }
            bool sel = ed.selected(n.name), hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) pick(n.name);
            ImVec2 p1(p0.x + cw, p0.y + ch);
            panel_shadow(p0, p1, 10.0f);
            ImU32 bg = sel ? ImGui::GetColorU32(ImGuiCol_Header)
                       : hov ? ImGui::GetColorU32(ImGuiCol_HeaderHovered)
                             : ImGui::GetColorU32(ImGuiCol_FrameBg);
            // ALLOMONE (derive-only): a rule may tint this card. Blend the base
            // toward the rule color and draw a colored border — the color is
            // never written to the rune, only painted here.
            unsigned rc;
            bool ruled = rule_color_for(n, rc);
            if (ruled) {
                ImVec4 base = ImGui::ColorConvertU32ToFloat4(bg);
                ImVec4 rcv = ImGui::ColorConvertU32ToFloat4(rc);
                float a = 0.28f;
                bg = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(base.x * (1 - a) + rcv.x * a, base.y * (1 - a) + rcv.y * a,
                           base.z * (1 - a) + rcv.z * a, 1.0f));
            }
            dl->AddRectFilled(p0, p1, bg, 10.0f);
            // The rest of what Allomone derived for the `card` surface. `weight`
            // is a Sum, so several sources agreeing that something matters
            // thickens the border rather than one of them winning the argument —
            // which is the merge law being visible in the UI, not just true.
            const AlloStyle* st = allo_style_for("card", n.name);
            float border = ruled ? 2.5f : 0.0f;
            if (st && st->weight > 0) border = std::max(border, 2.0f + (float)st->weight);
            if (sel)
                dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_HeaderActive), 10.0f,
                            0, 2.0f);
            else if (border > 0)
                dl->AddRect(p0, p1, ruled ? rc : ImGui::GetColorU32(ImGuiCol_Border),
                            10.0f, 0, std::min(border, 6.0f));
            draw_avatar(n, ImVec2(p0.x + cw * 0.5f, p0.y + r + 12), r);
            dl->PushClipRect(ImVec2(p0.x + 4, p0.y), ImVec2(p1.x - 4, p1.y), true);
            auto centered = [&](const char* s, float y, ImU32 col) {
                float tw = ImGui::CalcTextSize(s).x;
                dl->AddText(ImVec2(p0.x + (cw - tw) * 0.5f, y), col, s);
            };
            // A derived `label` SHADOWS the name for display and does not write
            // the rune — disabling the script restores it with nothing to undo.
            std::string caption = (st && !st->label.empty()) ? st->label : n.name;
            centered(caption.c_str(), p0.y + 2 * r + 18,
                     ImGui::GetColorU32(ImGuiCol_Text));
            std::string sub = subtitle(n);
            if (!sub.empty())
                centered(sub.c_str(), p0.y + 2 * r + 36,
                         ImGui::GetColorU32(ImGuiCol_TextDisabled));
            if (st && !st->badges.empty()) {
                // `badge` is an All law: every source's marker is collected, so
                // these are chips rather than one winner.
                float bx = p0.x + 6, by = p0.y + 6;
                for (const std::string& b : st->badges) {
                    ImVec2 sz = ImGui::CalcTextSize(b.c_str());
                    if (bx + sz.x + 8 > p1.x - 6) break;
                    dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + sz.x + 8, by + sz.y + 3),
                                      ruled ? rc : IM_COL32(90, 90, 110, 220), 4.0f);
                    dl->AddText(ImVec2(bx + 4, by + 1), IM_COL32(255, 255, 255, 235),
                                b.c_str());
                    bx += sz.x + 12;
                }
            }
            if (st && !st->icon.empty())
                dl->AddText(ImVec2(p1.x - 22, p0.y + 6),
                            ImGui::GetColorU32(ImGuiCol_Text), st->icon.c_str());
            dl->PopClipRect();
            if (hov && st && !st->notes.empty()) {
                // `note` is why this card looks like this — collected from every
                // source, so the tooltip is the whole account rather than one.
                std::string all;
                for (const std::string& x : st->notes) all += (all.empty() ? "" : "\n") + x;
                ImGui::SetTooltip("%s", all.c_str());
            }
            ImGui::PopID();
        }
    } else {
        // ── LIST view: a row per entity, now with a small avatar for people
        // and the existing rectangular thumbnail for images ─────────────────
        for (const auto* np : rows) {
            const maiz::SceneNode& n = *np;
            bool row_clicked = false;
            if (n.glyph == "image") {
                std::string p = field_value(n, "path");
                HostTexture t = p.empty() ? HostTexture{} : texture_for(p);
                if (t.id) {
                    float th_h = 42.0f, th_w = th_h * (float)t.w / (float)t.h;
                    ImGui::Image((ImTextureID)(intptr_t)t.id, ImVec2(th_w, th_h));
                    row_clicked = ImGui::IsItemClicked();
                    ImGui::SameLine();
                } else if (!p.empty()) {
                    ImGui::Dummy(ImVec2(42, 42)); // hold space while decoding
                    ImGui::SameLine();
                }
            } else if (n.glyph == "contact" || n.glyph == "organization") {
                float r = 15.0f;
                ImVec2 cur = ImGui::GetCursorScreenPos();
                draw_avatar(n, ImVec2(cur.x + r, cur.y + r), r);
                ImGui::Dummy(ImVec2(2 * r, 2 * r));
                row_clicked = ImGui::IsItemClicked();
                ImGui::SameLine();
            }
            if (ImGui::Selectable(n.name.c_str(), ed.selected(n.name)) || row_clicked)
                pick(n.name);
            std::string sub = subtitle(n);
            if (!sub.empty()) {
                ImGui::Indent(10);
                ImGui::TextDisabled("%s", sub.c_str());
                ImGui::Unindent(10);
            }
            ImGui::Spacing();
        }
    }
    ImGui::EndChild();

    // splitter 2: list | detail (fraction of the region right of the sidebar)
    ImGui::SameLine(0, 0);
    auto s2 = maiz::splitter("##data-list-split", true, data_list_frac,
                             rest_w - th, 0.15f, 0.75f, th, body_h);
    if (s2.released) flush_data_panels();
    ImGui::SameLine(0, 0);

    // ── detail: delete + connections + the widget-protocol form ─────────────
    panel_shadow(ImGui::GetCursorScreenPos(),
                 ImVec2(ImGui::GetCursorScreenPos().x +
                            ImGui::GetContentRegionAvail().x,
                        ImGui::GetCursorScreenPos().y + body_h));
    ImGui::BeginChild("data-detail", ImVec2(0, body_h), ImGuiChildFlags_Borders);
    const maiz::SceneNode* sel = nullptr;
    if (!ed.selection.empty()) sel = scene.find(ed.selection.front());
    if (!sel) {
        ImGui::TextDisabled("select something on the left,");
        ImGui::TextDisabled("or press + New");
    } else {
        // the name is editable: staged, commits ONE `rune rename` on Enter
        // (the core keeps spirit.id and repoints every reference). The control
        // is shared with the Notes tab -- see `rune_rename_control`.
        if (rune_rename_control(*sel, 220)) {
            ImGui::EndChild();
            return; // sel points at the old projection - bail cleanly
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", sel->label.c_str());
        if (sel->glyph == "image" && field_value(*sel, "url").empty() &&
            !field_value(*sel, "path").empty()) {
            ImGui::SameLine();
            /* "HOST IT ONLINE" (2026-09-15), which was "Get public URL" and ImgBB
             * only. Still NOT "Publish" (renamed 2026-08-20): publishing the WEBSITE
             * is the Publish tab. This puts ONE image online through whichever image
             * host the Antfarm has (domain/hosting.hpp), so an email can load it. */
            if (ImGui::SmallButton(ICON_FA_CLOUD_ARROW_UP "  Host it online")) {
                const std::string rn = sel->name;
                run_busy("Putting the image online", [this, rn] { host_image(rn); });
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("put this image online through the Antfarm's image host\n"
                                  "(ImgBB, an object store, or your website) and write its\n"
                                  "link into `url`, so it loads in email. This does not\n"
                                  "publish the website - that is the Publish tab.");
        }
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 44);
        if (ImGui::SmallButton("Delete")) ImGui::OpenPopup("confirm-delete");
        if (ImGui::BeginPopupModal("confirm-delete", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete %s? Undo can bring it back.", sel->name.c_str());
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                dispatch_and_reproject("rm " + sel->name);
                ed.selection.clear();
                ImGui::CloseCurrentPopup();
                sel = nullptr;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (sel) {
            // widgets COLLECT commands; we dispatch once at the end (a
            // re-projection would rebuild `scene` and dangle `sel`).
            std::vector<std::string> out;
            if (sel->glyph == "contact" || sel->glyph == "organization") {
                draw_person_detail(*sel, out); // #4: the curated, typed form
            } else {
                maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
                for (const auto& c : iio.commands) out.push_back(c);
            }
            // #4: connections live at the BOTTOM now, under everything
            ImGui::Spacing();
            ImGui::SeparatorText("Connections");
            draw_relations(*sel, out);
            for (const auto& c : out) dispatch_and_reproject(c);
        }
    }
    ImGui::EndChild();
}

// ── #4: the TYPED person form (contact / organization). Avatar up top (click
// to set the photo), then the fields that matter through their registered
// editors (role is a dropdown), then a plain-language LOCATION section —
// deliberately NONE of the map-display noise (label_scale/"size", per-view geo,
// ref/ref_off) that made the generic form a wall of text boxes. Commands are
// collected into `out`; the caller dispatches after the frame. ───────────────
void HormigaApp::draw_person_detail(const maiz::SceneNode& n,
                                    std::vector<std::string>& out) {
    // the avatar — big, click to browse/ingest a photo into assets/
    ImVec2 c = ImGui::GetCursorScreenPos();
    float r = 34.0f;
    draw_avatar(n, ImVec2(c.x + r, c.y + r), r);
    ImGui::Dummy(ImVec2(2 * r, 2 * r));
    if (ImGui::IsItemClicked() && browse_ingest) {
        std::string p =
            browse_ingest(n, "avatar", hormiga::temper::field_value(n, "avatar"));
        if (!p.empty()) out.push_back("set " + n.name + " avatar \"" + p + "\"");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("click to change the photo");
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextDisabled("%s", n.label.c_str());
    ImGui::TextDisabled("click the photo to change it");
    ImGui::EndGroup();
    ImGui::Spacing();

    // the curated fields, each through its declared editor (widget protocol)
    maiz::WidgetContext ctx{scene, out, std::string(filter), 0.0f};
    auto row = [&](const char* key) {
        for (const auto& fl : n.fields)
            if (fl.key == key) {
                maiz::widget_field(ctx, widgets, n, fl);
                return;
            }
    };
    if (n.glyph == "contact") {
        row("role");
        row("email");
        row("phone");
        row("website");
        row("bio");
        row("notes");
    } else { // organization
        row("abbreviation");
        row("email");
        row("url");
        row("location"); // a text location (distinct from the map)
        row("bio");
    }

    draw_tag_editor(n, out); // #1: contacts/orgs can be tagged here again
    draw_location_section(n, out);
}

// The LOCATION section: if the entity is on a map, a plain status + jump/remove;
// if not, just an invitation to place it — never a raw lat/lon box for someone
// who isn't mapped (author #4: "if they're not even on the map, there's no
// point in even having the location information present").
void HormigaApp::draw_location_section(const maiz::SceneNode& n,
                                       std::vector<std::string>& out) {
    bool located =
        std::find(n.tags.begin(), n.tags.end(), "located") != n.tags.end() ||
        !hormiga::temper::field_value(n, "geo").empty();
    ImGui::Spacing();
    ImGui::SeparatorText("Location");
    if (located) {
        std::string g = hormiga::temper::field_value(n, "geo");
        ImGui::TextDisabled("On the map%s%s", g.empty() ? "" : ": ", g.c_str());
        if (ImGui::SmallButton("View on map")) {
            switch_section(Map);
            ed.selection = {n.name};
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove from map")) {
            out.push_back("set " + n.name + " geo \"\"");
            out.push_back("tag " + n.name + " -located");
        }
    } else {
        ImGui::TextDisabled("Not on any map");
        if (ImGui::SmallButton("Place on map")) {
            switch_section(Map);
            ed.selection = {n.name};
            toast("right-click the map to place " + n.name);
        }
    }
}

// Connections + the link picker, extracted so it can sit at the BOTTOM of every
// detail pane. Emits unlink/link commands into `out` (no mid-render dispatch).
void HormigaApp::draw_relations(const maiz::SceneNode& n,
                                std::vector<std::string>& out) {
    bool any = false;
    int wi = 0;
    for (const auto& w : scene.wires) {
        ++wi;
        bool o = (w.from == n.name), in = (w.to == n.name);
        if (!o && !in) continue;
        any = true;
        const std::string& other = o ? w.to : w.from;
        ImGui::PushID(wi);
        if (ImGui::SmallButton("x"))
            /* THE RELATION IS QUOTED, AND OMITTED WHEN IT IS EMPTY (2026-09-02).
             * `--relation ` with nothing after it is a flag Void Core skips
             * (it needs a following token), so the unlink silently widened to
             * "any edge between these two" — which removes the wrong one when
             * there are several. Naming no relation is the honest way to say
             * that, and it is what the empty case means. */
            out.push_back("unlink " + w.from + " " + w.to +
                          (w.relation.empty()
                               ? std::string()
                               : " --relation " + json_arg(w.relation)));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("unlink (undoable)");
        ImGui::PopID();
        ImGui::SameLine();
        std::string line = (o ? "-> " : "<- ") + w.relation + ": " + other;
        if (ImGui::Selectable(line.c_str())) ed.selection = {other};
    }
    if (!any) ImGui::TextDisabled("no connections yet");
    // LINK: search the database, pick a target, one `link` command
    ImGui::Spacing();
    ImGui::SetNextItemWidth(140);
    ImGui::InputTextWithHint("##linkrel", "relation", link_relation,
                             sizeof link_relation);
    std::string me = n.name;
    std::string picked = search_picker(
        "##linksearch", link_search, sizeof link_search,
        [&me](const maiz::SceneNode& x) { return x.name != me && x.glyph != "map"; },
        "link to... (search)");
    if (!picked.empty()) {
        /* ── THE RELATION IS FREE TEXT AND WAS SPLICED IN RAW (2026-09-02) ───
         *
         * The author reported *"cant link images together? or there's a weird
         * error"*. Linking images is not special and the CLI path is fine; the
         * defect is in how this text box reached the dispatcher. The command
         * was built by concatenation, and the box accepts anything typed:
         *
         *   "goes with" → `--relation goes with`: Void Core reads the flag's
         *                 value as `goes` and drops `with`. The link is
         *                 written, under a relation nobody asked for, silently.
         *   "maria's"   → an unterminated quote, and an error quoting SPEC
         *                 §6.1 at somebody who typed a word into a text box.
         *                 Almost certainly the "weird error" in the report.
         *
         * `json_arg` is this project's §6.1 quoter — the one with the
         * trailing-backslash fix `maiz::arg` still lacks (app_shared.cpp). The
         * rune NAMES are slugs and safe, and go through it anyway: a value that
         * is safe by convention is one convention away from not being. */
        const std::string rel = link_relation[0] ? link_relation : "connected-to";
        out.push_back("link " + json_arg(me) + " " + json_arg(picked) +
                      " --relation " + json_arg(rel));
        toast("linked " + me + " -> " + picked + " (" + rel + ")");
    }
}

// ── Data Tools (#3, 2026-08-03): a detached window for the Data tab's
// utilities — pulled out of the toolbar so the tab is about viewing the data,
// not maintaining it. CSV import and the date-tag temper live here; more
// tools will accrue as the data phase grows. ────────────────────────────────
// ── IMPORTING A VOID REYNA TRANSCRIPT ───────────────────────────────────────
//
// Void Reyna (../VoidReyna) harvests public records, archives the bytes as
// evidence, and emits **a Void Core command transcript** — not a data structure,
// not a shared header, not a schema kept in sync across two languages. This is
// the whole consumer side of that contract, and it is small on purpose.
//
// WHY A TEXT FILE IS THE RIGHT SEAM. Everything an import needs in order to be
// trustworthy comes free from commands being commands: the batch lands as ONE
// undo frame, it is logged, it is attributed, and it replays identically
// forever. There is no second write path to audit because there is no second
// write path. And a producer that emits text can be rewritten in any language —
// or replaced by a person typing — without Hormiga noticing.
//
// WHAT IS REFUSED MATTERS MORE THAN WHAT IS ACCEPTED. A transcript is
// executable input from outside the application, so it is filtered rather than
// trusted: only verbs that BUILD model content pass. `use`, `save`, `deploy`,
// `config` and `effect` are dropped, because an import that can switch mantles,
// reconfigure the app or fire a holiday is not an import — it is a script, and
// nothing about a harvested PDF should be able to become one.
void HormigaApp::import_reyna_transcript() {
    /* ── ONE GATE, TWO POLICIES (2026-08-25) ────────────────────────────────
     *
     * This used to be its own filter: read the file line by line, take the text
     * before the first space as the verb, keep the lines whose verb was in an
     * allow-list, dispatch those. It had the exact hole Void Maiz reported in
     * their own effect gate the same day, and one they did not have:
     *
     *   - the verb was read as TEXT, so `'set' x y` presented as the
     *     five-character token `'set'` and was refused while the dispatcher
     *     would have run it. (Fails closed, so annoying rather than dangerous.)
     *   - **line by line**, so a value containing a newline was split, and its
     *     second line was checked and approved as its own command. A harvested
     *     PDF whose extracted prose happened to contain a line starting `set`
     *     would have had that line dispatched as a command.
     *
     * The design already said this was the same seam as a website submission —
     * web-platform.md §4, *"same seam, three sources"* — and that was true of
     * the design and false of the code. It is true of the code now:
     * `submission::review` asks Void Core's own transcript decoder what the
     * statements ARE, so a newline inside a value is data by construction.
     *
     * A local import gets a slightly wider policy than a stranger's proposal
     * (`local_import_verbs` adds `mantle` and `place`), which is the one real
     * difference between the two and is now expressed as a parameter rather
     * than as a second implementation. */
    std::string path = on_pick_file ? on_pick_file("") : std::string();
    if (path.empty()) return;
    std::ifstream in(path, std::ios::binary);
    if (!in) { toast("could not open " + path, true); return; }
    std::stringstream ss;
    ss << in.rdbuf();

    const auto rev = hormiga::submission::review(
        ss.str(), hormiga::submission::local_import_verbs());
    if (!rev.ok) {
        /* THE WHOLE FILE IS REFUSED, not filtered. The old version silently
         * skipped what it did not like and imported the rest, which means a
         * transcript that was partly nonsense arrived as a partly-applied
         * import nobody had read. Refusing whole is the same rule the
         * submission gate follows and for the same reason. */
        toast("refused: " + rev.refusal, true);
        reyna_last_import = "refused: " + rev.refusal;
        return;
    }

    std::vector<std::string> cmds;
    cmds.reserve(rev.commands.size());
    for (const auto& c : rev.commands) cmds.push_back(c.text);

    // ONE batch, so the whole import is one undo. A partial import that cannot
    // be backed out is worse than no import at all.
    maiz::Result r = dispatch_and_reproject(maiz::compile_commit(cmds));
    if (!r.ok) return; // dispatch_and_reproject already toasted the failure
    const std::string msg =
        "imported " + std::to_string(cmds.size()) + " command(s)";
    toast(msg + " - undo reverts the whole batch");
    reyna_last_import = msg;
}

void HormigaApp::draw_data_tools_window() {
    if (!win_data_tools) return;
    ImGui::SetNextWindowSize(ImVec2(360, 280), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Data Tools", &win_data_tools)) {
        ImGui::TextDisabled("utilities for the Data tab");

        ImGui::SeparatorText("Import");
        if (ImGui::Button("Import CSV...", ImVec2(-1, 0)))
            ImGui::OpenPopup("dt-csv-import");
        if (ImGui::BeginPopup("dt-csv-import")) {
            ImGui::TextDisabled("header row maps to fields; a \"tags\" column is tags");
            for (const auto& e : palette.entries)
                if (ImGui::MenuItem(("import " + e.label + "s...").c_str()))
                    run_csv_import(e.glyph);
            ImGui::EndPopup();
        }
        ImGui::TextDisabled("map a spreadsheet's rows to new runes, one\n"
                            "replayable batch (undoable)");

        if (ImGui::Button("Import Void Reyna transcript...", ImVec2(-1, 0)))
            import_reyna_transcript();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "A command transcript emitted by Void Reyna (../VoidReyna):\n"
                "harvested public records, already cited back to archived bytes.\n"
                "Lands as ONE undoable batch. Only model-building verbs are\n"
                "accepted - `use`, `config`, `effect` and the like are refused.");
        if (!reyna_last_import.empty())
            ImGui::TextDisabled("last: %s", reyna_last_import.c_str());

        ImGui::SeparatorText("Tags");
        if (ImGui::Button("Tidy date tags", ImVec2(-1, 0))) derive_date_tags();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("temper pass: add month:/season: tags to dated events\n"
                              "(idempotent) so @season:summer filters them");

        ImGui::Spacing();
        ImGui::TextDisabled("more tools will land here as the data phase grows");
    }
    ImGui::End();
}

// ── Notes (author 2026-08-03): a bare-bones dedicated tab. A list of `note`
// runes + a plain text editor — deliberately minimal; the real Notes engine
// (markdown/Obsidian interop, Allomone text mechanics, "note → newsletter") is
// a far-future build (Q22). Everything is still a dispatcher command. ────────
void HormigaApp::draw_notes_body() {
    // top: name search + the SAME tag-filter builder as the Data tab (author
    // 2026-08-03: notes get tags + filter searching too)
    ImGui::SetNextItemWidth(160);
    ImGui::InputTextWithHint("##notesearch", "search notes...", notes_search,
                             sizeof notes_search);
    ImGui::SameLine();
    if (ImGui::SmallButton("clear")) {
        notes_search[0] = 0;
        notes_filter_terms.clear();
        notes_filter_expr[0] = 0;
    }
    draw_tag_filter("notes", notes_filter_terms, notes_filter_join,
                    notes_filter_expr, sizeof notes_filter_expr, notes_filter_add,
                    sizeof notes_filter_add);
    ImGui::Separator();

    // left: the filtered note list + create
    ImGui::BeginChild("notes-list", ImVec2(190, 0), ImGuiChildFlags_Borders);
    if (ImGui::Button(ICON_FA_SQUARE_PLUS " New note", ImVec2(-1, 0)))
        new_rune("note");
    ImGui::Separator();
    int shown = 0;
    for (const auto& n : scene.nodes) {
        if (n.glyph != "note") continue;
        if (!contains_ci(n.name, notes_search)) continue;
        if (!maiz::node_matches(notes_filter_expr, n)) continue;
        ++shown;
        if (ImGui::Selectable(n.name.c_str(), ed.selected(n.name)))
            ed.selection = {n.name};
    }
    if (shown == 0)
        ImGui::TextDisabled(notes_search[0] || notes_filter_expr[0]
                                ? "no notes match"
                                : "no notes yet");
    ImGui::EndChild();

    // right: the selected note's tags + text
    ImGui::SameLine();
    ImGui::BeginChild("notes-edit", ImVec2(0, 0));
    const maiz::SceneNode* sel =
        ed.selection.empty() ? nullptr : scene.find(ed.selection.front());
    if (!sel || sel->glyph != "note") {
        ImGui::TextDisabled("select a note on the left, or press + New note");
    } else {
        /* THE NOTE'S NAME IS EDITABLE HERE (2026-09-02). It was `TextDisabled`,
         * and `draw_data_body` -- the only place with a rename box -- skips
         * `note` runes because notes have this tab. So a note was the one kind
         * of rune that could not be renamed, which is what the author reported.
         * One control, in widgets.cpp, for both. */
        if (rune_rename_control(*sel, 220)) {
            ImGui::EndChild();
            return; // sel points at the old projection - bail cleanly
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(note)");
        /* THE SAME TAG EDITOR AS DATA, AND NOTHING DISPATCHED WHILE `sel` IS HELD
         * (2026-09-15). The author: adding `color:blue` from the suggestions
         * crashed Hormiga on Linux. This tab dispatched in two places while still
         * holding `sel`, a pointer into the projection a dispatch replaces:
         * removing a tag did it INSIDE the loop over that note's tags, and adding
         * one read `sel->name` again two lines later. Windows survived the freed
         * memory by luck; Linux did not. The editor collects commands and they
         * land next frame, the way the Data tab's always have, which also brings
         * the colon tags and the suggestions a note never showed. */
        std::vector<std::string> tag_cmds;
        draw_tag_editor(*sel, tag_cmds);
        for (auto& c : tag_cmds) pending_cmds.push_back(std::move(c));
        ImGui::Separator();
        // stage the text; commit ONE setjson on blur (handles quotes/newlines)
        if (notes_edit_for != sel->name) {
            notes_edit_for = sel->name;
            std::snprintf(notes_buf, sizeof notes_buf, "%s",
                          hormiga::temper::field_value(*sel, "text").c_str());
        }
        ImGui::InputTextMultiline("##notetext", notes_buf, sizeof notes_buf,
                                  ImVec2(-1, -1));
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string js = nlohmann::json(std::string(notes_buf)).dump();
            dispatch_and_reproject("setjson " + sel->name + " text " +
                                   json_arg(js));
        }
    }
    ImGui::EndChild();
}

