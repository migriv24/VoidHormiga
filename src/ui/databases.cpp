/* ui/databases.cpp — File > Databases: every .miga this computer knows about.
 *
 * The author, 2026-09-25, asking for the phone's database manager: "a desktop
 * version too". The phone's is Migas (phone/phone_migas.cpp); this is the same
 * list in a window: this device's databases folder (where a joined database
 * lands, one folder level down), the recent ones and the default, newest first,
 * with the open one marked.
 *
 * What the desktop does NOT do here that the phone does: delete a file. A phone
 * owns its databases folder and nothing else can reach it; a desktop's .miga
 * files are the person's files, in folders they chose, and removing one belongs
 * to their file manager (Show in folder is one click away).
 *
 * Switching saves the open database first, as File > Open always should have:
 * opening another database replaces the working copy. */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "platform/app_settings.hpp"
#include "platform/device_paths.hpp"

#include <ctime>

namespace fs = std::filesystem;

namespace {

std::string size_text(long long b) {
    char buf[32];
    if (b < 1024 * 1024) std::snprintf(buf, sizeof buf, "%.0f KB", b / 1024.0);
    else std::snprintf(buf, sizeof buf, "%.1f MB", b / (1024.0 * 1024.0));
    return buf;
}

std::string when_text(long long t) {
    if (t <= 0) return "?";
    const std::time_t tt = (std::time_t)t;
    char b[40];
    std::strftime(b, sizeof b, "%Y-%m-%d %H:%M", std::localtime(&tt));
    return b;
}

} // namespace

void HormigaApp::draw_databases() {
    if (!win_databases) return;
    ImGui::SetNextWindowSize(ImVec2(620, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Databases", &win_databases)) {
        ImGui::End();
        return;
    }
    static std::vector<hormiga::app_settings::KnownDatabase> known;
    static double listed_at = -100.0;
    static char new_name[64] = {};
    const fs::path dir = hormiga::device::get().databases;
    if (ImGui::GetTime() - listed_at > 2.0) {
        listed_at = ImGui::GetTime();
        known = hormiga::app_settings::known_databases({dir.string()});
    }
    std::error_code ec;
    const std::string open_now = cur_miga.empty() ? std::string() : fs::absolute(cur_miga, ec).lexically_normal().string();
    auto save_open = [this] { // before anything replaces the working copy
        if (!cur_miga.empty()) save_database();
    };

    ImGui::TextDisabled("Databases folder:");
    ImGui::SameLine();
    ImGui::TextUnformatted(dir.string().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN "  Show") && on_open) {
        fs::create_directories(dir, ec);
        on_open(dir.string());
    }

    // a new one, named, in the databases folder
    ImGui::SetNextItemWidth(220);
    const bool go = ImGui::InputTextWithHint("##newdb", "name of a new database", new_name, sizeof new_name,
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button(ICON_FA_CIRCLE_PLUS "  New") || go) && new_name[0]) {
        std::string stem;
        for (const char* c = new_name; *c; ++c) {
            const unsigned char ch = (unsigned char)*c;
            if (std::isalnum(ch)) stem += (char)std::tolower(ch);
            else if (!stem.empty() && stem.back() != '-') stem += '-';
        }
        while (!stem.empty() && stem.back() == '-') stem.pop_back();
        if (stem.empty()) {
            toast("give it a name with a letter or a digit in it", true);
        } else {
            fs::path out = dir / (stem + ".miga");
            for (int n = 2; fs::exists(out, ec); ++n) out = dir / (stem + "-" + std::to_string(n) + ".miga");
            fs::create_directories(dir, ec);
            save_open();
            const std::string path = out.string();
            run_busy("Creating new database…", [this, path] {
                new_database();
                save_database_as(path);
            });
            new_name[0] = 0;
            listed_at = -100.0;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Open another...") && on_pick_file) {
        const std::string p = on_pick_file("");
        if (!p.empty()) {
            save_open();
            run_busy("Opening database…", [this, p] {
                open_database(p);
                hormiga::app_settings::note_recent(p);
            });
            listed_at = -100.0;
        }
    }
    ImGui::Separator();

    if (known.empty()) ImGui::TextDisabled("No .miga files yet: save this database, or make a new one.");
    if (ImGui::BeginTable("##dbs", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Database", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Changed", ImGuiTableColumnFlags_WidthFixed, 130);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 190);
        ImGui::TableHeadersRow();
        for (const auto& k : known) {
            const bool is_open = k.path == open_now;
            ImGui::PushID(k.path.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k.name.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", k.path.c_str());
            if (is_open) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.35f, 0.70f, 0.40f, 1.0f), "open");
            }
            if (k.is_default) {
                ImGui::SameLine();
                ImGui::TextDisabled(ICON_FA_STAR " opens at start");
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", size_text(k.bytes).c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", when_text(k.modified).c_str());
            ImGui::TableNextColumn();
            if (!is_open && ImGui::SmallButton("Open")) {
                save_open();
                const std::string p = k.path;
                run_busy("Opening database…", [this, p] {
                    open_database(p);
                    hormiga::app_settings::note_recent(p);
                });
                listed_at = -100.0;
            }
            if (!k.is_default) {
                if (!is_open) ImGui::SameLine();
                if (ImGui::SmallButton(ICON_FA_STAR)) {
                    auto st = hormiga::app_settings::load();
                    st.default_database = k.path;
                    hormiga::app_settings::save(st);
                    listed_at = -100.0;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("open this one when Hormiga starts");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_FILE_EXPORT) && on_save_file) {
                const std::string to = on_save_file(fs::path(k.path).filename().string());
                if (!to.empty()) {
                    if (is_open) save_database();
                    fs::copy_file(k.path, to, fs::copy_options::overwrite_existing, ec);
                    toast(ec ? "could not copy: " + ec.message() : "saved a copy: " + to, (bool)ec);
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("save a copy to...");
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN) && on_open) on_open(fs::path(k.path).parent_path().string());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("show in its folder");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
