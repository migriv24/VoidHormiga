/* ui/profile_window.cpp — the Profile window: the person at this computer.
 *
 * okf/concepts/platform/lan-sharing.md §1. Not a database's setting: the author
 * asked for *"information that exists for any database"* -- a username, a
 * picture, a colour -- and for the device facts to be *"automatically"* checked.
 * The facts are detected each time the window opens; the rest is saved in the
 * profile folder the moment it changes.
 */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "IconsFontAwesome6.h"

#include <cstring>

void LanRuntime::draw_profile(HormigaApp& app) {
    LanRuntime& rt = of(app);
    ImGui::SetNextWindowSize(ImVec2(470, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profile", &app.win_profile)) {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("You, on this computer. The same for every database you open.");
    ImGui::Spacing();

    // ── picture ──────────────────────────────────────────────────────────────
    const ImVec2 at = ImGui::GetCursorScreenPos();
    draw_avatar(app, hormiga::profile::avatar_path(rt.me).string(),
                rt.me.username.empty() ? "?" : rt.me.username, rt.me.color, at.x, at.y, 76);
    ImGui::Dummy(ImVec2(76, 76));
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (ImGui::Button("Choose a picture...") && app.on_pick_file) {
        const std::string picked = app.on_pick_file("");
        std::string err;
        if (!picked.empty() && !hormiga::profile::set_avatar(rt.me, picked, &err)) app.toast(err, true);
        rt.members_read_at = -100.0;
    }
    if (!rt.me.avatar.empty() && ImGui::Button("Use the default picture")) {
        std::string err;
        hormiga::profile::set_avatar(rt.me, {}, &err);
    }
    ImGui::TextDisabled("Without a picture, your initial\non your colour is shown.");
    ImGui::EndGroup();

    // ── username ─────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Username");
    static char buf[64];
    static bool loaded = false;
    if (!loaded || ImGui::IsWindowAppearing()) {
        std::snprintf(buf, sizeof buf, "%s", rt.me.username.c_str());
        loaded = true;
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##username", "how others see you, e.g. cool_username_123", buf, sizeof buf);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        rt.me.username = buf;
        std::string err;
        if (!hormiga::profile::save(rt.me, &err)) app.toast(err, true);
        else app.toast("username saved");
    }
    ImGui::TextDisabled("No password yet. Your key below is what proves this computer is you.");

    // ── colour ───────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Colour");
    ImGui::TextDisabled("What you are highlighted in when others see you working.");
    for (int i = 0; i < 12; ++i) {
        const std::string hex = hormiga::collab::kPalette[i];
        const unsigned v = hormiga::collab::rgb_of(hex);
        const ImVec4 c(((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f, 1);
        ImGui::PushID(i);
        if (i) ImGui::SameLine(0, 4);
        const bool mine = hex == rt.me.color;
        if (ImGui::ColorButton(mine ? "yours" : hex.c_str(), c, ImGuiColorEditFlags_NoTooltip, ImVec2(26, 26))) {
            rt.me.color = hex;
            hormiga::profile::save(rt.me);
        }
        if (mine) {
            const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRect(ImVec2(mn.x - 2, mn.y - 2), ImVec2(mx.x + 2, mx.y + 2),
                                                ImGui::GetColorU32(ImGuiCol_Text), 3.0f, 0, 2.0f);
        }
        ImGui::PopID();
    }
    const std::string shown = color_of(rt, fingerprint(rt));
    if (shown != "#888888" && shown != rt.me.color)
        ImGui::TextWrapped("Right now others see you in %s: someone who joined earlier uses a colour close "
                           "to yours, and no two people are shown in the same colour.",
                           shown.c_str());

    // ── this computer ────────────────────────────────────────────────────────
    ImGui::SeparatorText("This computer");
    static std::vector<hormiga::profile::Fact> facts;
    if (facts.empty() || ImGui::IsWindowAppearing()) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        char win[48];
        std::snprintf(win, sizeof win, "%.0f x %.0f", vp->Size.x, vp->Size.y);
        facts = hormiga::profile::device_facts(
            {{"Void Core", std::string(maiz::Core::core_version())}, {"Window", win}});
    }
    if (ImGui::BeginTable("facts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        for (const auto& f : facts) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", f.label.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(f.value.c_str());
        }
        ImGui::EndTable();
    }
    if (ImGui::SmallButton("Copy these")) {
        std::string all;
        for (const auto& f : facts) all += f.label + ": " + f.value + "\n";
        ImGui::SetClipboardText(all.c_str());
    }

    // ── key ──────────────────────────────────────────────────────────────────
    ImGui::SeparatorText(ICON_FA_LOCK "  Key");
    ImGui::Text("Fingerprint %s", fingerprint(rt).c_str());
    ImGui::TextDisabled("Made on %s. The private half never leaves this computer.", rt.me.created.c_str());
    if (ImGui::SmallButton("Show the profile folder") && app.on_open)
        app.on_open(hormiga::profile::dir().string());
    if (!rt.profile_error.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.35f, 1), "%s", rt.profile_error.c_str());
    ImGui::End();
}
