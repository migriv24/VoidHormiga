/* ui/profile_window.cpp — the Profile window: the person at this computer.
 *
 * okf/concepts/platform/lan-sharing.md §1. Not a database's setting: the author
 * asked for *"information that exists for any database"* -- a username, a
 * picture, a colour -- and for the device facts to be *"automatically"* checked.
 * The facts are detected each time the window opens; the rest is saved in the
 * profile folder the moment it changes.
 *
 * EVERY CHANGE IS WRITTEN TO THE LOG (2026-09-19): the author changed a picture
 * and a username and saw nothing in the log panel. They are deliberately NOT
 * dispatcher commands -- the journal travels with a shared database, and a
 * replayed `set username` would rewrite the other member's profile. Whether
 * device-level settings get a journal of their own is a developer question.
 */
#include "voidmaiz/mobile.hpp" // dim_wrapped: hints that wrap, on a phone and in a narrow window
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "IconsFontAwesome6.h"

#include <cstring>

void LanRuntime::draw_profile(HormigaApp& app) {
    ImGui::SetNextWindowSize(ImVec2(470, 560), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Profile", &app.win_profile)) draw_profile_body(app);
    ImGui::End();
}

/* The window's content, shared with the phone's Me screen (phone/phone.cpp). */
void LanRuntime::draw_profile_body(HormigaApp& app) {
    LanRuntime& rt = of(app);
    const bool phone = app.phone != nullptr;
    maiz::dim_wrapped(phone ? "You, on this phone. The same for every database you open."
                            : "You, on this computer. The same for every database you open.");
    ImGui::Spacing();

    // ── picture ──────────────────────────────────────────────────────────────
    const ImVec2 at = ImGui::GetCursorScreenPos();
    draw_avatar(app, hormiga::profile::avatar_path(rt.me).string(),
                rt.me.username.empty() ? "?" : rt.me.username, rt.me.color, at.x, at.y, 76);
    ImGui::Dummy(ImVec2(76, 76));
    ImGui::SameLine();
    ImGui::BeginGroup();
    if (app.on_pick_file && ImGui::Button("Choose a picture...")) { // only where there is a file dialog
        const std::string picked = app.on_pick_file("");
        std::string err;
        if (!picked.empty() && !hormiga::profile::set_avatar(rt.me, picked, &err)) app.toast(err, true);
        else if (!picked.empty()) app.log.push_back({"info", "profile", "picture set from " + picked});
        rt.members_read_at = -100.0;
    }
    if (!rt.me.avatar.empty() && ImGui::Button("Use the default picture")) {
        std::string err;
        if (hormiga::profile::set_avatar(rt.me, {}, &err))
            app.log.push_back({"info", "profile", "picture reset to the default"});
    }
    maiz::dim_wrapped("Without a picture, your initial\non your colour is shown.");
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
        else {
            app.toast("username saved");
            app.log.push_back({"info", "profile", "username set to " + rt.me.username});
        }
    }
    maiz::dim_wrapped("No password yet. Your key below is what proves this computer is you.");

    // ── colour ───────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Colour");
    maiz::dim_wrapped("What you are highlighted in when others see you working.");
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
            app.log.push_back({"info", "profile", "colour set to " + hex});
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

    // ── this computer (a desktop's diagnostics; a phone's person does not need them)
    if (!phone) {
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
    } // !phone

    // ── key ──────────────────────────────────────────────────────────────────
    ImGui::SeparatorText(ICON_FA_LOCK "  Key");
    ImGui::Text("Fingerprint %s", fingerprint(rt).c_str());
    maiz::dim_wrapped(("Made on " + rt.me.created + ". The private half never leaves this " +
                       (phone ? "phone." : "computer.")).c_str());
    if (app.on_open && ImGui::SmallButton("Show the profile folder")) // only where there is a file browser
        app.on_open(hormiga::profile::dir().string());
    if (!rt.profile_error.empty())
        ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.35f, 1), "%s", rt.profile_error.c_str());

    // ── on another device (planned) ──────────────────────────────────────────
    ImGui::SeparatorText(ICON_FA_ARROWS_ROTATE "  On your other devices");
    maiz::dim_wrapped("Not yet: each device has its own profile for now. Later you will be able to "
                      "carry this one to another device (your key travels encrypted), so the same "
                      "you is on your phone and your computer.");

    // ── log out: deliberate, because there is no way back in yet ─────────────
    ImGui::SeparatorText(ICON_FA_RIGHT_FROM_BRACKET "  Log out");
    maiz::dim_wrapped("Removes you from this device: your key, your picture, and every saved credential "
                      "sealed with your key. Your databases stay.");
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
    if (ImGui::Button("Log out...")) ImGui::OpenPopup("Log out?");
    ImGui::PopStyleColor();
    static char typed[32] = {};
    if (ImGui::BeginPopupModal("Log out?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1), "There is currently no way to log back in.");
        ImGui::TextWrapped("You will lose your credentials for every database on this device, and this "
                           "profile's key. A new profile is made. To be in a shared database again, "
                           "ask for it to be shared with your new profile.");
        ImGui::PopTextWrapPos();
        ImGui::TextUnformatted("Type  log out  to continue:");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##typed", typed, sizeof typed);
        const bool sure = std::string(typed) == "log out";
        ImGui::BeginDisabled(!sure);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
        if (ImGui::Button("Log out")) {
            leave_database(app); // this identity is no longer who the members let in
            std::string err;
            if (hormiga::profile::erase(&err)) {
                // the credentials sealed with the old key: this database's vault goes too
                app.vault.lock();
                std::error_code ec;
                std::filesystem::remove(app.vault_path(), ec);
                app.imgbb_key.clear();
                app.secrets_locked = false;
                rt.me = hormiga::profile::load_or_create(&rt.profile_error);
                const std::string k = hormiga::profile::credentials_key(rt.me);
                if (!k.empty() && app.vault.create(k)) app.vault.save(app.vault_path().string());
                refresh_self(app);
                rt.members_read_at = -100.0;
                app.toast("logged out: this device has a new profile");
                app.log.push_back({"info", "profile", "logged out; a new profile was made"});
            } else {
                app.toast(err, true);
            }
            typed[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            typed[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
