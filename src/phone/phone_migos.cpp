/* phone/phone_migos.cpp — Migos: the network, as its own screen.
 *
 * The author, 2026-09-25: "maybe networking should be its own tab", with
 * loading bars for the files being shared and, eventually, "a user's ping and
 * connection strength"; and the name was left to us, with a lean: "migos has a
 * double meaning of 'friends', maybe that's better for a networking screen".
 * It is: the people you share a database with are your migos, and the
 * databases themselves are migas (phone_migas.cpp).
 *
 * WHAT EACH NUMBER IS, so nobody reads more into it than it says:
 *   - SIGNAL is the share of a member's beacons (one every 3 s) that arrived
 *     over the last ~30 s. It falls when the Wi-Fi is weak or busy, before the
 *     member disappears. It is not the radio's signal strength.
 *   - PING is half of the last sealed handshake with that member (two round
 *     trips). Only the device that dialled measures it; "--" otherwise.
 *   - A TRANSFER is one sync frame of 128 KB or more on a member link, or the
 *     files of a join. Sending shows a fraction; receiving shows the bytes so
 *     far, because the sealed stream does not say a message's length until it
 *     has ended.
 *
 * HOSTING FROM A PHONE (asked the same day: "figure out how to possibly make
 * the mobile device host a network"). The share code has no desktop in it: it
 * needs a Share over LAN node in the Antfarm, a username, and a folder to write
 * the bundle, and a phone has all three (its temp folder is inside the app's
 * own, set by the Android shell: Android has no /tmp). So the button is here,
 * with what it costs said plainly: the room key lives on this phone. */
#include "phone/phone_ui.hpp"

#include "app/lan_internal.hpp" // lan_detail::now_seconds: the link threads' clock
#include "voidmaiz/radio.hpp"      // RadioAccess, for the switches

#include <cfloat>

using namespace hormiga::phone;

namespace {

/* Four bars, lit by `s` (0..1); `known` false draws them all dim. */
void signal_bars(ImVec2 at, float h, float s, bool known) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = h * 0.22f, gap = h * 0.12f;
    const int lit = known ? (int)std::lround(s * 4.0f) : 0;
    const ImU32 on = s > 0.6f ? IM_COL32(70, 170, 90, 255) : s > 0.3f ? IM_COL32(220, 160, 40, 255)
                                                                     : IM_COL32(210, 80, 70, 255);
    for (int i = 0; i < 4; ++i) {
        const float bh = h * (0.35f + 0.65f * (float)i / 3.0f);
        const ImVec2 p0(at.x + i * (w + gap), at.y + h - bh), p1(p0.x + w, at.y + h);
        dl->AddRectFilled(p0, p1, i < lit ? on : ImGui::GetColorU32(ImGuiCol_FrameBg), w * 0.3f);
    }
}

/* ── NEARBY, NO WI-FI NEEDED (2026-10-05) ──────────────────────────────────
 * The author: share between phones with no network in common, never through a
 * hotspot. Bluetooth works with any phone (and, later, an iPhone) but is slow;
 * Wi-Fi Direct is fast, Android to Android. Each is a switch, off until the
 * person turns it on: the first time, the system asks for "Nearby devices".
 * Only members of this database are ever connected (app/lan_radio.cpp). */
void radio_section(HormigaApp& app, HormigaApp::PhoneUi& ph) {
    LanRuntime& rt = LanRuntime::of(app);
    ImGui::SeparatorText("Nearby, no Wi-Fi needed");
    if (!LanRuntime::radio_available()) {
        maiz::dim_wrapped("This device has no Bluetooth or Wi-Fi Direct that Hormiga can use.");
        return;
    }
    if (rt.room_key.empty()) {
        maiz::dim_wrapped("Share or join a database first: the radios only ever connect members of the same one.");
        return;
    }
    struct Row {
        int kind;
        const char* icon;
        const char* name;
        const char* what;
    };
    static const Row rows[] = {
        {LanRuntime::kRadioBle, ICON_FA_SIGNAL, "Bluetooth",
         "Any phone, slower: a first sync can take a minute or two. Works later with iPhones too."},
        {LanRuntime::kRadioWifiDirect, ICON_FA_WIFI, "Wi-Fi Direct",
         "Android to Android, fast. The other phone may be asked to accept."},
    };
    for (const auto& r : rows) {
        ImGui::PushID(r.kind);
        bool on = LanRuntime::radio_on(app, r.kind);
        const int access = LanRuntime::radio_access(app, r.kind);
        const bool none = access == (int)maiz::RadioAccess::Unavailable;
        ImGui::BeginDisabled(none);
        if (ImGui::Checkbox("##on", &on)) {
            std::string why;
            if (!LanRuntime::radio_switch(app, r.kind, on, &why) && !why.empty()) maiz::show_snackbar(ph.snack, why);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("%s  %s", r.icon, r.name);
        if (none) ImGui::TextDisabled("not on this device");
        else if (access == (int)maiz::RadioAccess::Denied) ImGui::TextDisabled("not allowed: see the system's settings");
        else if (access == (int)maiz::RadioAccess::Off) ImGui::TextDisabled("switched off on the phone");
        else maiz::dim_wrapped(r.what);
        ImGui::EndGroup();
        ImGui::PopID();
    }
    const std::string status = LanRuntime::radio_status(app);
    if (!status.empty()) ImGui::TextDisabled("%s", status.c_str());
    if (!LanRuntime::radio_on(app, LanRuntime::kRadioBle) && !LanRuntime::radio_on(app, LanRuntime::kRadioWifiDirect))
        return;
    const auto near = LanRuntime::radio_near(app);
    int members = 0;
    for (const auto& n : near) members += n.member ? 1 : 0;
    if (!members) {
        maiz::dim_wrapped("Looking. Members appear here when their phone is near, Hormiga is open, and the same "
                          "switch is on there.");
        return;
    }
    for (const auto& n : near) {
        if (!n.member) continue; // other devices are none of our business, and not shown
        const char* how = n.kind == LanRuntime::kRadioBle ? ICON_FA_SIGNAL : ICON_FA_WIFI;
        std::string line = std::string(how) + "  " + (n.name.empty() ? std::string("a member's phone") : n.name);
        line += n.linked ? "   connected" : "   in range";
        ImGui::TextUnformatted(line.c_str());
        if (n.rssi) {
            ImGui::SameLine();
            ImGui::TextDisabled("%d dBm", n.rssi);
        }
    }
}

} // namespace

void HormigaApp::PhoneUi::migos(HormigaApp& app, PhoneUi& ph, Frame&) {
    const float dp = ph.dp;
    LanRuntime& rt = LanRuntime::of(app);
    std::map<std::string, LanRuntime::Transfer> transfers;
    std::map<std::string, double> link_ms;
    const double now = lan_detail::now_seconds();
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        for (auto it = rt.transfers.begin(); it != rt.transfers.end();) // a finished one shows for a moment
            it = it->second.finished >= 0 && now - it->second.finished > 6.0 ? rt.transfers.erase(it) : std::next(it);
        transfers = rt.transfers;
        link_ms = rt.link_ms;
    }

    // ── who is here ──────────────────────────────────────────────────────────
    ImGui::SeparatorText("Here now");
    if (rt.present.empty())
        maiz::dim_wrapped(rt.sharing || !rt.member_rows.empty()
                              ? "Nobody else, right now. Members appear here when they open this database on the "
                                "same network, or near by with a radio on (below)."
                              : "Nobody: this database is not shared yet. Join one below, or share this one.");
    for (const auto& [fp, a] : rt.present) {
        ImGui::PushID(fp.c_str());
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float sz = ImGui::GetFontSize() * 2.2f;
        auto av = rt.member_avatar.find(fp);
        LanRuntime::draw_avatar(app, av != rt.member_avatar.end() ? av->second : "", a.user,
                                LanRuntime::color_of(rt, fp), at.x, at.y, sz);
        ImGui::Dummy(ImVec2(sz, sz));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(a.user.c_str());
        const maiz::SceneNode* on = a.selection.empty() ? nullptr : app.scene.find(a.selection.front());
        if (on) ImGui::TextDisabled("on %s", title_of(*on).c_str());
        else ImGui::TextDisabled("in %s", a.section.empty() ? "Hormiga" : a.section.c_str());
        ImGui::EndGroup();
        // the right edge: signal bars over the ping
        const float bars_h = ImGui::GetFontSize() * 1.1f;
        const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        auto st = rt.strength.find(fp);
        signal_bars(ImVec2(right - bars_h * 1.4f, at.y + 2 * dp), bars_h, st == rt.strength.end() ? 0.0f : st->second,
                    st != rt.strength.end());
        auto ms = link_ms.find(fp);
        char ping[24];
        if (ms != link_ms.end()) std::snprintf(ping, sizeof ping, "%.0f ms", ms->second);
        else std::snprintf(ping, sizeof ping, "--");
        const float pw = ImGui::CalcTextSize(ping).x;
        ImGui::GetWindowDrawList()->AddText(ImVec2(right - pw, at.y + bars_h + 6 * dp),
                                            ImGui::GetColorU32(ImGuiCol_TextDisabled), ping);
        if (on && ImGui::SmallButton("Go there")) {
            ph.screen = kData;
            ph.stacks[kData].reset();
            ph.stacks[kData].push("detail:" + on->name);
        }
        ImGui::PopID();
        ImGui::Spacing();
    }
    if (!rt.present.empty())
        maiz::dim_wrapped("Signal: how many of a member's announcements arrive (one every 3 s). Ping: the last "
                          "connection's round trip, when this phone made it.");

    // ── what is moving ───────────────────────────────────────────────────────
    ImGui::SeparatorText("Transfers");
    if (transfers.empty()) ImGui::TextDisabled("Nothing moving right now.");
    for (const auto& [link, t] : transfers) {
        std::string who = link;
        if (auto p = rt.present.find(link); p != rt.present.end()) who = p->second.user;
        else if (link.rfind("join", 0) == 0) who.clear(); // a join names its person in `what`
        const bool done = t.finished >= 0;
        std::string line = std::string(t.sending ? ICON_FA_ARROW_UP "  " : ICON_FA_ARROW_DOWN "  ") + t.what;
        if (!who.empty()) line += (t.sending ? " to " : " from ") + who;
        ImGui::TextUnformatted(line.c_str());
        const double secs = std::max(0.001, (done ? t.finished : now) - t.started);
        char label[96];
        if (t.total > 0)
            std::snprintf(label, sizeof label, "%s of %s   %s/s", human_bytes(t.done).c_str(), human_bytes(t.total).c_str(),
                          human_bytes((long long)(t.done / secs)).c_str());
        else
            std::snprintf(label, sizeof label, "%s   %s/s", human_bytes(t.done).c_str(),
                          human_bytes((long long)(t.done / secs)).c_str());
        const float frac = done ? 1.0f
                           : t.total > 0 ? (float)t.done / (float)t.total
                                         : -1.0f * (float)ImGui::GetTime(); // unknown length: the moving bar
        ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 0), done ? (std::string(ICON_FA_CHECK "  ") + label).c_str() : label);
    }

    radio_section(app, ph);

    // ── the sync, and the database's sharing ────────────────────────────────
    LanRuntime::draw_sync_section(app); // brings its own heading

    ImGui::SeparatorText("Share this database");
    if (rt.sharing) {
        ImGui::TextColored(ImVec4(0.35f, 0.70f, 0.40f, 1.0f), ICON_FA_TOWER_BROADCAST "  Shared from this phone");
        maiz::dim_wrapped("Devices on this network can ask to join. You are asked each time, on any screen.");
        if (ImGui::Button("Stop sharing", ImVec2(-FLT_MIN, 0))) LanRuntime::stop_sharing(app);
    } else {
        maiz::dim_wrapped("Let others on this network join this database from this phone. Its room key then lives "
                          "on this phone: if the phone is lost, the database's members should share again from "
                          "another device.");
        if (ImGui::Button(ICON_FA_TOWER_BROADCAST "  Share from this phone", ImVec2(-FLT_MIN, 0))) {
            std::string err;
            if (!LanRuntime::start_sharing(app, err)) maiz::show_snackbar(ph.snack, err);
        }
    }

    ImGui::SeparatorText("Join a shared database");
    LanRuntime::draw_discover_body(app);
}
