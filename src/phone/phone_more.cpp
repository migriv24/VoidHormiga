/* phone/phone_more.cpp — the phone's Notes, Antfarm, Settings and Profile.
 *
 * NOTES (the author, 2026-09-25: "notes should also be a thing to add to the
 * mobile version, such that notes can be shared"). A note is the desktop's own
 * `note` rune, edited by the same commands, so a note written on the phone is
 * on every member's desktop Notes tab. Shared or private is the Antfarm's
 * private tag, exactly as on the desktop (lan-sharing.md §7): a private note
 * never leaves this phone.
 *
 * THE ANTFARM. The author set it aside for phones until its redesign
 * (2026-09-23), then listed it among the screens a person may put on the bar
 * (2026-09-25), right after it began syncing between members. So the phone
 * gets what it can do well: a list of the nodes with what each says, each
 * node's fields, and the graph itself on a canvas a finger can pan. */
#include "phone/phone_ui.hpp"

#include "json.hpp"
#include "platform/device_paths.hpp"

#include <cfloat>

using namespace hormiga::phone;

namespace {

/* The Antfarm's private tag, re-read every few seconds (projecting the Antfarm
 * every frame on a phone is work for nothing). */
std::string private_tag(maiz::Core& core) {
    static std::string tag = "private";
    static double at = -100.0;
    if (ImGui::GetTime() - at > 3.0) {
        at = ImGui::GetTime();
        maiz::ProjectOptions po;
        po.mantle = kAntfarmMantle;
        const auto share = hormiga::collab::share_settings(maiz::project_scene(core, po));
        tag = share.private_tags.empty() ? std::string("private") : share.private_tags.front();
    }
    return tag;
}

bool has_tag(const maiz::SceneNode& n, const std::string& t) {
    return std::find(n.tags.begin(), n.tags.end(), t) != n.tags.end();
}

/* A note's title is its first line; the rune's handle only when it has none. */
std::string note_title(const maiz::SceneNode& n) {
    std::string t = hormiga::temper::field_value(n, "text");
    const auto nl = t.find('\n');
    if (nl != std::string::npos) t.resize(nl);
    return t.empty() ? std::string("Untitled note") : t;
}

std::string note_body(const maiz::SceneNode& n) {
    const std::string t = hormiga::temper::field_value(n, "text");
    const auto nl = t.find('\n');
    return nl == std::string::npos ? std::string() : t.substr(nl + 1);
}

} // namespace

void HormigaApp::PhoneUi::flush_note(HormigaApp& app, PhoneUi& ph) {
    if (!ph.note_dirty || ph.note_frame == ImGui::GetFrameCount()) return;
    ph.note_dirty = false;
    if (!app.scene.find(ph.note_for)) return; // removed meanwhile: nothing to write into
    const std::string js = nlohmann::json(std::string(ph.note_buf.data())).dump();
    app.dispatch_and_reproject("setjson " + ph.note_for + " text " + json_arg(js));
}

/* ── NOTES: the list, and one note ─────────────────────────────────────── */
void HormigaApp::PhoneUi::notes(HormigaApp& app, PhoneUi& ph, Frame& f) {
    const float dp = ph.dp;
    const std::string priv = private_tag(app.core);
    if (f.route.rfind("note:", 0) == 0) {
        const std::string name = f.route.substr(5);
        const maiz::SceneNode* n = app.scene.find(name);
        if (!n || n->glyph != "note") {
            maiz::dim_wrapped("This note was removed, here or on another device.");
            if (ImGui::Button("Back")) f.stack->pop();
            return;
        }
        app.ed.selection = {name}; // presence: the note on my screen
        if (ph.note_for != name) { // stage its text once, then the box owns it
            flush_note(app, ph);
            ph.note_for = name;
            std::snprintf(ph.note_buf.data(), ph.note_buf.size(), "%s",
                          hormiga::temper::field_value(*n, "text").c_str());
            ph.note_dirty = false;
        }
        ph.note_frame = ImGui::GetFrameCount();
        const bool is_priv = has_tag(*n, priv);
        if (ImGui::Button(is_priv ? ICON_FA_LOCK "  Private" : ICON_FA_USERS "  Shared"))
            f.out.push_back("tag " + name + (is_priv ? " -" : " +") + priv);
        ImGui::SameLine();
        maiz::presence_item(app.surfaces, app.roster, app.net_settings.show, "list:notes", n->id, maiz::Mark::Badge,
                            app.share_now && !app.share_now(*n));
        maiz::dim_wrapped(is_priv ? "Only on this phone: never sent to the database's members."
                                  : "Everyone this database is shared with sees it.");
        // the text: the first line is the title, the rest is the note
        const float h = std::max(ImGui::GetFontSize() * 8.0f,
                                 ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 5.0f);
        if (ImGui::InputTextMultiline("##note", ph.note_buf.data(), ph.note_buf.size(), ImVec2(-FLT_MIN, h)))
            ph.note_dirty = true;
        maiz::text_input_kind(maiz::InputKind::Multiline);
        if (ImGui::IsItemDeactivated()) ph.note_frame = -1; // lost focus: commit now
        ImGui::SeparatorText("Tags");
        app.draw_tag_editor(*n, f.out);
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
        if (ImGui::Button(ICON_FA_TRASH "  Delete note", ImVec2(-FLT_MIN, 0))) {
            ph.note_dirty = false;
            f.out.push_back("rm " + name);
            maiz::show_snackbar(ph.snack, "Deleted " + note_title(*n), "UNDO");
            f.stack->pop();
        }
        ImGui::PopStyleColor();
        return;
    }

    app.ed.selection.clear();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##nsearch", ICON_FA_MAGNIFYING_GLASS "  Search notes", ph.note_search,
                             sizeof ph.note_search);
    maiz::text_input_kind(maiz::InputKind::Search);
    const std::string q = lower(ph.note_search);
    std::vector<const maiz::SceneNode*> rows;
    for (const auto& n : app.scene.nodes)
        if (n.glyph == "note" && (q.empty() || lower(hormiga::temper::field_value(n, "text")).find(q) != std::string::npos ||
                                  lower(n.name).find(q) != std::string::npos))
            rows.push_back(&n);
    std::sort(rows.begin(), rows.end(), [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
        return lower(note_title(*a)) < lower(note_title(*b));
    });
    if (rows.empty())
        maiz::dim_wrapped(q.empty() ? "No notes yet. Write one with +: it is shared with everyone in this database "
                                      "unless you make it private."
                                    : "No note matches.");
    const float card_h = 84.0f * dp, pad = 12.0f * dp;
    for (const maiz::SceneNode* n : rows) {
        ImGui::PushID(n->name.c_str());
        const bool tapped = ImGui::InvisibleButton("##card", ImVec2(-FLT_MIN, card_h));
        const bool held = ImGui::IsItemActive();
        const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        maiz::presence_item(app.surfaces, app.roster, app.net_settings.show, "list:notes", n->id, maiz::Mark::Badge,
                            app.share_now && !app.share_now(*n));
        ImGui::PopID();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(r0, r1, ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg), 12.0f * dp);
        dl->AddRectFilled(r0, ImVec2(r0.x + 4.0f * dp, r1.y), kind_colour("note"), 12.0f * dp,
                          ImDrawFlags_RoundCornersLeft);
        const bool is_priv = has_tag(*n, priv);
        const float title_px = ImGui::GetFontSize() * 1.08f, sub_px = ImGui::GetFontSize() * 0.9f;
        const char* mark = is_priv ? ICON_FA_LOCK : ICON_FA_USERS;
        const float mark_w = ImGui::CalcTextSize(mark).x;
        dl->PushClipRect(r0, ImVec2(r1.x - pad - mark_w - pad, r1.y), true);
        dl->AddText(ImGui::GetFont(), title_px, ImVec2(r0.x + pad * 1.4f, r0.y + pad),
                    ImGui::GetColorU32(ImGuiCol_Text), note_title(*n).c_str());
        const std::string body = note_body(*n);
        dl->AddText(ImGui::GetFont(), sub_px, ImVec2(r0.x + pad * 1.4f, r0.y + pad + title_px + 4 * dp),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), body.c_str(), nullptr, r1.x - r0.x - pad * 3 - mark_w);
        dl->PopClipRect();
        dl->AddText(ImVec2(r1.x - pad - mark_w, r0.y + pad), ImGui::GetColorU32(ImGuiCol_TextDisabled), mark);
        if (tapped && ImGui::GetIO().MouseDragMaxDistanceSqr[0] < 36.0f * dp * dp) f.stack->push("note:" + n->name);
    }
    ImGui::Dummy(ImVec2(0, card_h)); // room under the last card for the add button
}

/* ── THE ANTFARM: its nodes, one node, or the graph ────────────────────── */
void HormigaApp::PhoneUi::antfarm(HormigaApp& app, PhoneUi& ph, Frame& f) {
    const float dp = ph.dp;
    if (f.route.rfind("detail:", 0) == 0) {
        const maiz::SceneNode* n = app.scene.find(f.route.substr(7));
        if (!n) {
            maiz::dim_wrapped("This node was removed, here or on another device.");
            return;
        }
        app.ed.selection = {n->name};
        ImGui::TextUnformatted(n->label.empty() ? n->name.c_str() : n->label.c_str());
        ImGui::TextDisabled("%s", kind_label(n->glyph).c_str());
        if (hormiga::collab::device_only(*n))
            maiz::dim_wrapped(ICON_FA_LOCK "  Names a credential file, so it stays on this device.");
        // what it is connected to, both ways
        for (const auto& w : app.scene.wires) {
            if (w.from == n->name) ImGui::TextDisabled(ICON_FA_ARROW_RIGHT "  feeds %s", w.to.c_str());
            if (w.to == n->name) ImGui::TextDisabled(ICON_FA_ARROW_LEFT "  from %s", w.from.c_str());
        }
        ImGui::Separator();
        maiz::WidgetContext ctx{app.scene, f.out, std::string(), 0.0f};
        for (const auto& fl : n->fields) {
            if (fl.editor == "hidden" || fl.key == "pos" || fl.key == "size") continue;
            ImGui::TextDisabled("%s", fl.label.empty() ? fl.key.c_str() : fl.label.c_str());
            ImGui::PushItemWidth(-FLT_MIN);
            ImGui::PushID(fl.key.c_str());
            maiz::widget_field(ctx, app.widgets, *n, fl);
            ImGui::PopID();
            ImGui::PopItemWidth();
        }
        return;
    }

    int mode = ph.farm_graph ? 1 : 0;
    if (maiz::segmented("##farm-mode", {"Nodes", "Graph"}, mode)) ph.farm_graph = mode == 1;
    if (ph.farm_graph) {
        if (!ph.farm_style_ready) {
            ph.farm_style = app.canvas_style;
            maiz::TouchProfile prof;
            prof.dp = dp;
            maiz::apply_touch_canvas(ph.farm_style, prof);
            app.ed.cam.zoom = std::clamp(dp * 0.6f, 0.4f, 2.0f); // start near a readable size
            ph.farm_style_ready = true;
        }
        // zoom: a phone has no wheel, and pinching is not wired yet
        auto zoom = [&](float k) {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float cx = app.ed.cam.x + avail.x * 0.5f / app.ed.cam.zoom;
            const float cy = app.ed.cam.y + avail.y * 0.5f / app.ed.cam.zoom;
            app.ed.cam.zoom = std::clamp(app.ed.cam.zoom * k, ph.farm_style.min_zoom, ph.farm_style.max_zoom);
            app.ed.cam.x = cx - avail.x * 0.5f / app.ed.cam.zoom;
            app.ed.cam.y = cy - avail.y * 0.5f / app.ed.cam.zoom;
        };
        if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_MINUS)) zoom(1.0f / 1.25f);
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS_PLUS)) zoom(1.25f);
        ImGui::SameLine();
        maiz::dim_wrapped("Drag to move around. Hold on a node for its menu.");
        maiz::CanvasNet anet;
        anet.surfaces = &app.surfaces;
        anet.roster = &app.roster;
        anet.display = app.net_settings.show;
        anet.shareable = app.share_now;
        anet.surface_id = "canvas:antfarm";
        maiz::CanvasIO cio = maiz::edit_canvas("phone-antfarm", app.scene, app.ed, ph.farm_style, &app.palette_antfarm,
                                               &app.faces, {}, nullptr, &anet);
        for (auto& c : cio.commands) f.out.push_back(std::move(c));
        return;
    }

    maiz::dim_wrapped("What this database runs on: its backends, where it publishes, and each device. "
                      "Shared with its members, except anything naming a credential file.");
    std::vector<const maiz::SceneNode*> rows;
    for (const auto& n : app.scene.nodes) rows.push_back(&n);
    std::sort(rows.begin(), rows.end(), [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
        return a->glyph != b->glyph ? a->glyph < b->glyph : a->label < b->label;
    });
    const float card_h = 64.0f * dp, pad = 12.0f * dp;
    for (const maiz::SceneNode* n : rows) {
        ImGui::PushID(n->name.c_str());
        const bool tapped = ImGui::InvisibleButton("##node", ImVec2(-FLT_MIN, card_h));
        const bool held = ImGui::IsItemActive();
        ImGui::PopID();
        const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(r0, r1, ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg), 10.0f * dp);
        const float r = 16.0f * dp, cy = (r0.y + r1.y) * 0.5f;
        dl->AddCircleFilled(ImVec2(r0.x + pad + r, cy), r, kind_colour(n->glyph));
        const std::string title = n->label.empty() ? n->name : n->label;
        const float title_px = ImGui::GetFontSize() * 1.04f, sub_px = ImGui::GetFontSize() * 0.88f;
        dl->PushClipRect(r0, ImVec2(r1.x - pad, r1.y), true);
        dl->AddText(ImGui::GetFont(), title_px, ImVec2(r0.x + pad * 2 + r * 2, cy - title_px), ImGui::GetColorU32(ImGuiCol_Text),
                    title.c_str());
        std::string kind = kind_label(n->glyph); // the palette's own name for it, when it has one
        for (const auto& e : app.palette_antfarm.entries)
            if (e.glyph == n->glyph) kind = e.label;
        const std::string sub = kind + (hormiga::collab::device_only(*n) ? "  " ICON_FA_LOCK : "");
        dl->AddText(ImGui::GetFont(), sub_px, ImVec2(r0.x + pad * 2 + r * 2, cy + 2 * dp),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str());
        dl->PopClipRect();
        if (tapped && ImGui::GetIO().MouseDragMaxDistanceSqr[0] < 36.0f * dp * dp) f.stack->push("detail:" + n->name);
    }
}

/* ── SETTINGS: this app, on this phone ─────────────────────────────────── */
void HormigaApp::PhoneUi::settings(HormigaApp& app, PhoneUi& ph, Frame&) {
    const float dp = ph.dp;
    /* THE NAVIGATION BAR (the author, 2026-09-25): "should be customizable ...
     * in the mobile settings", any of the screens, with the centre button
     * locked. A preview, then every screen with a pin and arrows. The choice
     * is this phone's, saved with its settings, never the database's. */
    ImGui::SeparatorText("Navigation bar");
    maiz::dim_wrapped("Up to four screens sit on the bar, two each side of the Hormiga button. The button "
                      "itself always stays in the middle, and opens every screen.");
    bool changed = false;
    for (int s = 0; s < kScreens; ++s) {
        const ScreenInfo& in = screen_info(s);
        auto it = std::find(ph.bar.begin(), ph.bar.end(), s);
        const bool on = it != ph.bar.end();
        const int at = on ? (int)(it - ph.bar.begin()) : -1;
        ImGui::PushID(s);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(in.icon);
        ImGui::SameLine(34.0f * dp + ImGui::GetStyle().WindowPadding.x);
        ImGui::TextUnformatted(in.label);
        if (on) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d)", at + 1);
        }
        const float bw = ImGui::GetFrameHeight();
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - bw * 3 - ImGui::GetStyle().ItemSpacing.x * 2);
        ImGui::BeginDisabled(!on || at == 0);
        if (ImGui::Button(ICON_FA_ARROW_UP, ImVec2(bw, 0))) {
            std::swap(ph.bar[at], ph.bar[at - 1]);
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!on || at == (int)ph.bar.size() - 1);
        if (ImGui::Button(ICON_FA_ARROW_DOWN, ImVec2(bw, 0))) {
            std::swap(ph.bar[at], ph.bar[at + 1]);
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(ICON_FA_THUMBTACK, ImVec2(bw, 0))) {
            if (on) {
                ph.bar.erase(it);
                changed = true;
            } else if (ph.bar.size() >= 4) {
                maiz::show_snackbar(ph.snack, "The bar holds four. Unpin one first.");
            } else {
                ph.bar.push_back(s);
                changed = true;
            }
        }
        if (on) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (ImGui::Button("Back to the defaults")) {
        ph.bar = default_bar();
        changed = true;
    }
    if (changed) save_bar(ph.bar);

    ImGui::SeparatorText("Look");
    if (ImGui::Button(app.light_mode ? ICON_FA_MOON "  Dark mode" : ICON_FA_SUN "  Light mode")) {
        app.light_mode = !app.light_mode;
        app.apply_theme();
    }

    ImGui::SeparatorText("Files from others");
    bool cautious = app.net_settings.cautious_files;
    if (ImGui::Checkbox("Ask before downloading pictures", &cautious)) {
        app.net_settings.cautious_files = cautious;
        LanRuntime::save_net_settings(app);
    }
    maiz::dim_wrapped("On mobile data, a picture someone adds can wait until you tap it. Applies to the next "
                      "connection.");

    ImGui::SeparatorText("This phone");
    const auto& dev = hormiga::device::get();
#ifdef HORMIGA_VERSION
    ImGui::TextDisabled("Void Hormiga %s  (%s)", HORMIGA_VERSION, dev.platform.c_str());
#endif
    ImGui::TextDisabled("Databases: %s", dev.databases.string().c_str());
    if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save now")) app.do_save();
    maiz::dim_wrapped("The Builder and the Map are on the desktop. A phone keeps the organization's people, "
                      "dates and notes with you, and in sync.");
}

/* ── PROFILE: the desktop's own body ───────────────────────────────────── */
void HormigaApp::PhoneUi::profile(HormigaApp& app, PhoneUi&, Frame&) { LanRuntime::draw_profile_body(app); }
