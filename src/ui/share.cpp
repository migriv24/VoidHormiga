/* ui/share.cpp — the Share database and Discover databases windows, the "wants
 * to join" question, the presence strip in the menu bar, and the highlighting.
 *
 * okf/concepts/platform/lan-sharing.md. The author (2026-09-16): *"there's
 * already a button for 'share database' and when that is clicked, there should
 * be an option to 'share over local LAN' … on the other device, i should have a
 * button to 'discover databases' … upon clicking it, the FIRST device … should
 * recieve a message like 'cool_username_123 is wants to be part of the network,
 * allow?'"*
 *
 * Everything that DOES something is in app/lan_share.cpp; this file draws it and
 * turns clicks into those calls. The author asked for the features to have a GUI
 * and not to worry about its polish yet, so it is plain on purpose.
 */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "app/paths.hpp"
#include "IconsFontAwesome6.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

ImVec4 hex_color(const std::string& hex, float alpha = 1.0f) {
    const unsigned v = hormiga::collab::rgb_of(hex);
    return ImVec4(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, alpha);
}

std::string unquote(std::string v) {
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v == "null" ? std::string() : v;
}

std::string human(long long b) {
    char buf[32];
    if (b >= 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f MB", b / (1024.0 * 1024.0));
    else if (b >= 1024) std::snprintf(buf, sizeof buf, "%.0f KB", b / 1024.0);
    else std::snprintf(buf, sizeof buf, "%lld B", b);
    return buf;
}

std::string default_dest() {
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? (fs::path(home) / "Documents" / "HormigaFiles").string() : std::string();
}

std::string first_letter(const std::string& name) {
    if (name.empty()) return "?";
    std::size_t n = 1;
    const unsigned char c = (unsigned char)name[0];
    if (c >= 0xF0) n = 4;
    else if (c >= 0xE0) n = 3;
    else if (c >= 0xC0) n = 2;
    std::string s = name.substr(0, n);
    if (n == 1) s[0] = (char)std::toupper(c);
    return s;
}

}  // namespace

void HormigaApp::draw_share_window() {
    LanRuntime::draw_windows(*this);
}

void LanRuntime::draw_windows(HormigaApp& app) {
    if (app.lan) app.lan->discovering = app.win_discover;
    tick(app, ImGui::GetTime());
    if (app.win_profile) draw_profile(app);
    if (app.win_share) draw_share(app);
    if (app.win_discover) draw_discover(app);
    if (app.lan) draw_request(app);
}

void LanRuntime::draw_avatar(HormigaApp& app, const std::string& png_path, const std::string& name,
                             const std::string& color, float x, float y, float size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 c(x + size * 0.5f, y + size * 0.5f);
    HormigaApp::HostTexture t = png_path.empty() ? HormigaApp::HostTexture{} : app.texture_for(png_path);
    if (t.id) {
        dl->AddImageRounded((ImTextureID)(intptr_t)t.id, ImVec2(x, y), ImVec2(x + size, y + size),
                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, size * 0.5f);
    } else {
        dl->AddCircleFilled(c, size * 0.5f, ImGui::ColorConvertFloat4ToU32(hex_color(color)));
        const std::string l = first_letter(name);
        ImFont* f = ImGui::GetFont();
        const float fs_ = size * 0.55f;
        const ImVec2 ts = f->CalcTextSizeA(fs_, FLT_MAX, 0, l.c_str());
        dl->AddText(f, fs_, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32_WHITE, l.c_str());
    }
    dl->AddCircle(c, size * 0.5f, ImGui::ColorConvertFloat4ToU32(hex_color(color)), 0, 2.0f);
}

/* ── the menu bar: who else is here, then you ───────────────────────────────── */

void LanRuntime::draw_presence_strip(HormigaApp& app) {
    LanRuntime& rt = of(app);
    const float size = ImGui::GetFrameHeight() - 2.0f;
    int shown = 0;
    const int others = (int)rt.present.size();
    ImGui::SameLine(0, 18);
    for (const auto& [fp, a] : rt.present) {
        if (shown == 6) {
            ImGui::TextDisabled("+%d", others - 6);
            ImGui::SameLine();
            break;
        }
        ImGui::PushID(fp.c_str());
        const ImVec2 p = ImGui::GetCursorScreenPos();
        auto av = rt.member_avatar.find(fp);
        draw_avatar(app, av != rt.member_avatar.end() ? av->second : "", a.user, color_of(rt, fp), p.x, p.y + 1,
                    size);
        if (ImGui::InvisibleButton("presence", ImVec2(size, size)) && !a.selection.empty())
            app.redirect_to(a.selection.front(), a.mantle, "where " + a.user + " is");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s - in %s%s%s", a.user.c_str(), a.section.c_str(),
                              a.selection.empty() ? "" : ", on ",
                              a.selection.empty() ? "" : a.selection.front().c_str());
        ImGui::PopID();
        ImGui::SameLine(0, 4);
        ++shown;
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    draw_avatar(app, hormiga::profile::avatar_path(rt.me).string(), rt.me.username.empty() ? "?" : rt.me.username,
                color_of(rt, fingerprint(rt)) == "#888888" ? rt.me.color : color_of(rt, fingerprint(rt)), p.x,
                p.y + 1, size);
    if (ImGui::InvisibleButton("##me", ImVec2(size, size))) app.win_profile = true;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nYour profile", hormiga::profile::display_name(rt.me).c_str());
}

/* ── highlighting ───────────────────────────────────────────────────────────── */

void LanRuntime::mark_item(HormigaApp& app, const std::string& rune) {
    if (!app.lan || app.lan->present.empty()) return;
    const auto who = on_rune(*app.lan, rune);
    if (who.empty()) return;
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float x = mx.x - 4.0f;
    const float r = (mx.y - mn.y) * 0.32f;
    for (std::size_t i = 0; i < who.size(); ++i) {
        const ImU32 col = ImGui::ColorConvertFloat4ToU32(hex_color(color_of(*app.lan, who[i]->fingerprint)));
        dl->AddRectFilled(ImVec2(mn.x - 6.0f - 4.0f * i, mn.y), ImVec2(mn.x - 3.0f - 4.0f * i, mx.y), col);
        dl->AddCircleFilled(ImVec2(x - r, (mn.y + mx.y) * 0.5f), r, col);
        const std::string l = first_letter(who[i]->user);
        const ImVec2 ts = ImGui::CalcTextSize(l.c_str());
        dl->AddText(ImVec2(x - r - ts.x * 0.5f, (mn.y + mx.y) * 0.5f - ts.y * 0.5f), IM_COL32_WHITE, l.c_str());
        x -= r * 2.0f + 3.0f;
    }
    if (ImGui::IsItemHovered()) {
        std::string names;
        for (const auto* a : who) names += (names.empty() ? "" : ", ") + a->user;
        ImGui::SetTooltip("%s %s here", names.c_str(), who.size() == 1 ? "is" : "are");
    }
}

void LanRuntime::outline_nodes(HormigaApp& app, float canvas_x, float canvas_y) {
    if (!app.lan || app.lan->present.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const auto& cam = app.ed.cam;
    for (const auto& n : app.scene.nodes) {
        const auto who = on_rune(*app.lan, n.name);
        if (who.empty()) continue;
        const float w = n.w > 0 ? n.w : 180.0f, h = n.h > 0 ? n.h : 80.0f;
        const float x0 = canvas_x + (n.x - cam.x) * cam.zoom, y0 = canvas_y + (n.y - cam.y) * cam.zoom;
        for (std::size_t i = 0; i < who.size(); ++i) {
            const float pad = 3.0f + 4.0f * (float)i;
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(hex_color(color_of(*app.lan, who[i]->fingerprint)));
            dl->AddRect(ImVec2(x0 - pad, y0 - pad), ImVec2(x0 + w * cam.zoom + pad, y0 + h * cam.zoom + pad), col,
                        6.0f, 0, 3.0f);
            if (i == 0) dl->AddText(ImVec2(x0 - pad, y0 - pad - ImGui::GetTextLineHeight()), col, who[i]->user.c_str());
        }
    }
}

/* ── someone wants to join ──────────────────────────────────────────────────── */

void LanRuntime::draw_request(HormigaApp& app) {
    LanRuntime& rt = *app.lan;
    std::shared_ptr<Pending> p;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        p = rt.pending;
    }
    if (!p || p->answer != 0) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.35f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();
    if (ImGui::Begin("Someone wants to join###lan-request", nullptr,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        draw_avatar(app, "", p->req.user, p->req.color, at.x, at.y, 44);
        ImGui::Dummy(ImVec2(44, 44));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("%s wants to join this database.", p->req.user.c_str());
        ImGui::TextDisabled("key %s   Hormiga %s", p->req.fingerprint.substr(0, 8).c_str(), p->req.app.c_str());
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::TextWrapped("Their screen should show this code:");
        ImGui::SetWindowFontScale(2.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.3f, 1), "%s", p->req.sas.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 380);
        ImGui::TextDisabled("Only allow if the codes match and you know who this is. Allowing sends "
                            "them the database, its files and its keys, sealed.");
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_CHECK "  Allow", ImVec2(120, 0))) answer(app, true);
        ImGui::SameLine();
        if (ImGui::Button("Deny", ImVec2(120, 0))) answer(app, false);
    }
    ImGui::End();
}

/* ── Share database ─────────────────────────────────────────────────────────── */

void LanRuntime::draw_share(HormigaApp& app) {
    LanRuntime& rt = of(app);
    ImGui::SetNextWindowSize(ImVec2(560, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Share database", &app.win_share)) {
        ImGui::End();
        return;
    }
    maiz::ProjectOptions po;
    po.mantle = kAntfarmMantle;
    const maiz::Scene farm = maiz::project_scene(app.core, po);
    const auto share = hormiga::collab::share_settings(farm);

    // ── what this database says about itself ─────────────────────────────────
    ImGui::SeparatorText("This database");
    ImGui::TextDisabled("What someone looking for databases on your network sees.");
    static char name[80], desc[400];
    static std::string loaded_for;
    const std::string key = app.state_name + "|" + app.cur_miga;
    if (loaded_for != key || (!ImGui::IsAnyItemActive() && ImGui::IsWindowAppearing())) {
        loaded_for = key;
        std::snprintf(name, sizeof name, "%s", unquote(app.core.dispatch("config get org.name").data).c_str());
        std::snprintf(desc, sizeof desc, "%s", unquote(app.core.dispatch("config get org.description").data).c_str());
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##dbname", "name", name, sizeof name);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        app.pending_cmds.push_back("config set org.name " + json_str(name));
        std::snprintf(app.org_name, sizeof app.org_name, "%s", name);
    }
    ImGui::InputTextMultiline("##dbdesc", desc, sizeof desc, ImVec2(-1, 54));
    if (ImGui::IsItemDeactivatedAfterEdit()) app.pending_cmds.push_back("config set org.description " + json_str(desc));
    if (!app.org_logo.empty()) {
        HormigaApp::HostTexture t = app.texture_for(app.org_logo);
        if (t.id) {
            ImGui::Image((ImTextureID)(intptr_t)t.id, ImVec2(32.0f * t.w / std::max(1, t.h), 32));
            ImGui::SameLine();
        }
    }
    if (ImGui::SmallButton(app.org_logo.empty() ? "Choose an icon..." : "Change the icon...") && app.on_pick_file) {
        const std::string picked = app.on_pick_file("");
        if (!picked.empty()) {
            const std::string rel = app.ingest_asset(picked);
            if (!rel.empty()) {
                app.pending_cmds.push_back("config set org.logo " + json_str(rel));
                app.org_logo = rel;
            }
        }
    }

    // ── sharing ──────────────────────────────────────────────────────────────
    ImGui::SeparatorText(ICON_FA_WIFI "  Share over the local network");
    if (share.node.empty()) {
        ImGui::TextWrapped("This database's Antfarm has no Share over LAN node, so it cannot be "
                           "shared over the network yet.");
        if (ImGui::Button("Allow LAN sharing (adds Share over LAN and Members to the Antfarm)")) {
            const bool members = hormiga::collab::membership_settings(farm).node.empty();
            std::vector<std::string> cmds{std::string("use ") + kAntfarmMantle};
            for (auto& c : hormiga::collab::default_nodes(true, members)) cmds.push_back(std::move(c));
            cmds.push_back("use " + (app.scene.mantle.empty() ? std::string(kDataMantle) : app.scene.mantle));
            app.dispatch_and_reproject(maiz::compile_commit(cmds));
        }
    } else if (!share.allow) {
        ImGui::TextWrapped("The Antfarm's %s says this database may not be shared (allow: no).", share.node.c_str());
        if (ImGui::Button("Allow it")) {
            const std::string back = app.scene.mantle.empty() ? std::string(kDataMantle) : app.scene.mantle;
            app.dispatch_and_reproject(maiz::compile_commit(
                {std::string("use ") + kAntfarmMantle, "set " + share.node + " allow yes", "use " + back}));
        }
    } else {
        if (rt.me.username.empty()) {
            ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1), "Set a username first - it is how others see you.");
            ImGui::SameLine();
            if (ImGui::SmallButton("Open profile")) app.win_profile = true;
        }
        std::string status;
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            status = rt.host_status;
        }
        if (!rt.sharing) {
            ImGui::BeginDisabled(rt.me.username.empty());
            if (ImGui::Button(ICON_FA_SHARE_NODES "  Share over local network", ImVec2(-1, 34))) {
                std::string err;
                app.run_busy("Preparing the database to share", [&app] {
                    std::string e;
                    if (!LanRuntime::start_sharing(app, e)) app.toast(e, true);
                });
                (void)err;
            }
            ImGui::EndDisabled();
            if (ImGui::SmallButton("Show what would be sent")) rt.plan = build_plan(app, nullptr);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1), ICON_FA_WIFI "  %s", status.c_str());
            ImGui::TextDisabled("Anyone on this network can see that it is offered. Nothing is sent until you allow someone.");
            if (ImGui::Button("Stop sharing", ImVec2(-1, 0))) stop_sharing(app);
        }
        if (!rt.plan.items.empty()) {
            ImGui::Spacing();
            ImGui::Text("What is sent%s", rt.sharing ? "" : " (preview)");
            if (ImGui::BeginTable("plan", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                                 ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 44);
                ImGui::TableSetupColumn("what", ImGuiTableColumnFlags_WidthFixed, 150);
                ImGui::TableSetupColumn("size", ImGuiTableColumnFlags_WidthFixed, 60);
                ImGui::TableSetupColumn("why", ImGuiTableColumnFlags_WidthStretch);
                for (const auto& it : rt.plan.items) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (it.send) ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1), "send");
                    else ImGui::TextDisabled("keep");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(it.rel.c_str());
                    ImGui::TableNextColumn();
                    if (it.bytes) ImGui::TextDisabled("%s", human(it.bytes).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("%s", it.why.c_str());
                }
                ImGui::EndTable();
            }
            for (const auto& n : rt.plan.notes) ImGui::BulletText("%s", n.c_str());
        }

        // ── members ──────────────────────────────────────────────────────────
        ImGui::SeparatorText(ICON_FA_USERS "  Members");
        if (rt.member_rows.empty()) {
            ImGui::TextDisabled("Nobody yet. You become the first member the first time you share.");
        } else {
            ImGui::TextDisabled("Everyone is an admin for now (lan-sharing.md §4).");
            for (const auto& row : rt.member_rows) {
                auto get = [&row](const char* k) {
                    auto i = row.find(k);
                    return i == row.end() ? std::string() : i->second;
                };
                const std::string fp = get("fingerprint");
                const ImVec2 at = ImGui::GetCursorScreenPos();
                auto av = rt.member_avatar.find(fp);
                draw_avatar(app, av != rt.member_avatar.end() ? av->second : "", get("username"),
                            get("color").empty() ? "#888888" : get("color"), at.x, at.y, 26);
                ImGui::Dummy(ImVec2(26, 26));
                ImGui::SameLine();
                const bool here = rt.present.count(fp) || fp == fingerprint(rt);
                ImGui::Text("%s", get("username").c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("%s, joined %s%s%s", get("role").c_str(), get("joined").c_str(),
                                    get("invited_by").empty() ? "" : ", let in by ",
                                    get("invited_by").c_str());
                if (here) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1), "here");
                }
            }
        }
    }
    ImGui::End();
}

/* ── Discover databases ─────────────────────────────────────────────────────── */

void LanRuntime::draw_discover(HormigaApp& app) {
    LanRuntime& rt = of(app);
    rt.discovering = true;
    ImGui::SetNextWindowSize(ImVec2(520, 520), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Discover databases", &app.win_discover)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Databases someone is sharing on this network right now. Ask to join one; "
                       "the person sharing it has to allow you.");
    if (rt.me.username.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1), "Set a username first - the host sees it when you ask.");
        ImGui::SameLine();
        if (ImGui::SmallButton("Open profile")) app.win_profile = true;
    }
    if (!rt.dest[0]) std::snprintf(rt.dest, sizeof rt.dest, "%s", default_dest().c_str());
    ImGui::SeparatorText("Where a joined database goes");
    ImGui::SetNextItemWidth(-130);
    ImGui::InputText("##dest", rt.dest, sizeof rt.dest);
    ImGui::SameLine();
    if (ImGui::Button("Choose folder...") && app.on_pick_folder) {
        const std::string picked = app.on_pick_folder();
        if (!picked.empty()) std::snprintf(rt.dest, sizeof rt.dest, "%s", picked.c_str());
    }
    ImGui::TextDisabled("Each database gets its own folder inside this one. The one you have open is backed up first.");

    ImGui::SeparatorText(ICON_FA_WIFI "  On this network");
    const auto found = offers(app);
    if (found.empty()) {
        ImGui::TextDisabled("Looking... (the other device needs Share database > Share over local network)");
    }
    for (const auto& o : found) {
        ImGui::PushID(o.peer_id.c_str());
        const ImVec2 at = ImGui::GetCursorScreenPos();
        draw_avatar(app, "", o.user, o.color.empty() ? "#888888" : o.color, at.x, at.y + 2, 34);
        ImGui::Dummy(ImVec2(34, 38));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("%s", o.db.c_str());
        ImGui::TextDisabled("shared by %s  -  %s", o.user.c_str(), o.address.c_str());
        if (!o.description.empty()) {
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(o.description.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::BeginDisabled(rt.joining || rt.me.username.empty());
        if (ImGui::Button("Ask to join")) {
            std::string err;
            const fs::path dest = fs::path(rt.dest) / hormiga::lan::file_stem(o.db);
            if (!start_join(app, o, dest, err)) app.toast(err, true);
        }
        ImGui::EndDisabled();
        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::PopID();
    }

    std::string status, sas, error;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        status = rt.join_status;
        sas = rt.join_sas;
        error = rt.join_error;
    }
    if (!status.empty()) {
        ImGui::SeparatorText("Joining");
        if (!sas.empty() && rt.joining) {
            ImGui::TextWrapped("Check that the other screen shows the same code:");
            ImGui::SetWindowFontScale(2.0f);
            ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.3f, 1), "%s", sas.c_str());
            ImGui::SetWindowFontScale(1.0f);
        }
        ImGui::TextWrapped("%s", status.c_str());
        if (!error.empty()) ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.35f, 1), "%s", error.c_str());
    }
    ImGui::End();
}
