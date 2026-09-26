/* phone/phone_migas.cpp — Migas: the databases on this phone, and every file a
 * phone asks the system for (photos in, .miga files in and out).
 *
 * The author, 2026-09-25: a "database manager" for "managing the creation of
 * databases, opening between other databases, and keeping track of the
 * existing miga files on the device", with a desktop version too
 * (ui/databases.cpp), and "make sure the miga filetype is able to go onto
 * mobile devices". A miga is a crumb, and what an ant carries home; the
 * network screen is Migos, the people you share them with.
 *
 * NOTHING IS LOST BY SWITCHING. Opening another database replaces the working
 * copy, so the one open now is saved into its .miga first, and one that was
 * never saved gets a dated name in this phone's databases folder instead of
 * vanishing.
 *
 * FILES COME AND GO THROUGH THE SYSTEM (voidmaiz/documents.hpp): its picker
 * copies a .miga or a photo into the app's own folder, its save dialog copies a
 * .miga out to wherever the person chose, and "Open with Void Hormiga" on a
 * .miga in a chat or a file manager arrives the same way. Every answer comes
 * back through document_arrived, the one door. */
#include "phone/phone_ui.hpp"

#include "platform/device_paths.hpp"

#include <cfloat>
#include <ctime>

using namespace hormiga::phone;
namespace fs = std::filesystem;

namespace {

fs::path incoming_dir(HormigaApp& app) { return app.base_dir / "incoming"; }

std::string when_text(long long t) {
    if (t <= 0) return {};
    const std::time_t tt = (std::time_t)t;
    char b[40];
    std::strftime(b, sizeof b, "%Y-%m-%d %H:%M", std::localtime(&tt));
    return b;
}

/* A free path for `stem`.miga in `dir`: stem, stem-2, stem-3, ... */
fs::path free_path(const fs::path& dir, const std::string& stem) {
    std::error_code ec;
    fs::path p = dir / (stem + ".miga");
    for (int n = 2; fs::exists(p, ec); ++n) p = dir / (stem + "-" + std::to_string(n) + ".miga");
    return p;
}

std::string slug_name(const char* s) {
    std::string out;
    for (const char* c = s; *c; ++c) {
        const unsigned char ch = (unsigned char)*c;
        if (std::isalnum(ch)) out += (char)std::tolower(ch);
        else if ((ch == ' ' || ch == '-' || ch == '_') && !out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

} // namespace

/* Save what is open before anything replaces it (see the header). */
void HormigaApp::PhoneUi::keep_current(HormigaApp& app) {
    if (!app.cur_miga.empty()) {
        app.save_database();
        return;
    }
    std::error_code ec;
    const fs::path dir = hormiga::device::get().databases;
    fs::create_directories(dir, ec);
    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "unsaved-%Y%m%d-%H%M", std::localtime(&t));
    app.save_database_as(free_path(dir, stamp).string());
}

void HormigaApp::PhoneUi::migas(HormigaApp& app, PhoneUi& ph, Frame&) {
    const float dp = ph.dp;
    const fs::path dir = hormiga::device::get().databases;
    if (ImGui::GetTime() - ph.listed_at > 2.0) { // the folder, re-read every couple of seconds
        ph.listed_at = ImGui::GetTime();
        ph.known = hormiga::app_settings::known_databases({dir.string()});
    }
    std::error_code ec;
    const std::string open_now = app.cur_miga.empty() ? std::string() : fs::absolute(app.cur_miga, ec).lexically_normal().string();

    // ── the one open now ─────────────────────────────────────────────────────
    ImGui::SeparatorText("Open now");
    ImGui::TextUnformatted(open_now.empty() ? "An unsaved database" : fs::path(open_now).stem().string().c_str());
    if (!open_now.empty()) ImGui::TextDisabled("%s", human_bytes((long long)fs::file_size(open_now, ec)).c_str());
    if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save")) {
        PhoneUi::keep_current(app);
        ph.listed_at = -100.0;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_EXPORT "  Save a copy to...")) {
        PhoneUi::keep_current(app);
        const std::string src = app.cur_miga;
        if (!app.on_save_document || !app.on_save_document(kDocExportMiga, src, fs::path(src).filename().string()))
            maiz::show_snackbar(ph.snack, "This device has no save dialog: the file is " + src);
    }

    // ── a new one ────────────────────────────────────────────────────────────
    ImGui::SeparatorText("New database");
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Create").x - ImGui::GetStyle().FramePadding.x * 2 - 10 * dp);
    const bool go = ImGui::InputTextWithHint("##newdb", "Its name", ph.new_name, sizeof ph.new_name,
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    maiz::text_input_kind(maiz::InputKind::Text, maiz::InputAction::Go);
    ImGui::SameLine();
    if ((ImGui::Button("Create") || go) && ph.new_name[0]) {
        const std::string stem = slug_name(ph.new_name);
        if (stem.empty()) {
            maiz::show_snackbar(ph.snack, "Give it a name with a letter or a digit in it");
        } else {
            PhoneUi::keep_current(app);
            app.new_database(); // leaves a shared database: a new one is nobody's yet
            fs::create_directories(dir, ec);
            app.save_database_as(free_path(dir, stem).string());
            ph.new_name[0] = 0;
            ph.listed_at = -100.0;
            ph.screen = kData;
        }
    }
    maiz::dim_wrapped("Empty, and yours alone until you share it on Migos. Making it leaves any database you "
                      "were sharing.");

    // ── every one this phone has ─────────────────────────────────────────────
    ImGui::SeparatorText("On this phone");
    if (ph.known.empty()) ImGui::TextDisabled("None saved yet.");
    const float pad = 12.0f * dp;
    for (const auto& k : ph.known) {
        const bool is_open = k.path == open_now;
        ImGui::PushID(k.path.c_str());
        const ImVec2 r0 = ImGui::GetCursorScreenPos();
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0, pad * 0.3f));
        ImGui::Indent(pad);
        ImGui::TextUnformatted((std::string(ICON_FA_DATABASE "  ") + k.name).c_str());
        std::string meta = human_bytes(k.bytes);
        if (k.modified) meta += "  ·  " + when_text(k.modified);
        if (is_open) meta += "  ·  open now";
        if (k.is_default) meta += "  ·  opens at start";
        ImGui::TextDisabled("%s", meta.c_str());
        if (!is_open) {
            if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Open")) {
                PhoneUi::keep_current(app);
                app.open_database(k.path);
                hormiga::app_settings::note_recent(k.path);
                ph.listed_at = -100.0;
                ph.screen = kData;
            }
            ImGui::SameLine();
        }
        if (ImGui::Button(ICON_FA_FILE_EXPORT "##copy")) {
            if (!app.on_save_document || !app.on_save_document(kDocExportMiga, k.path, fs::path(k.path).filename().string()))
                maiz::show_snackbar(ph.snack, "This device has no save dialog: the file is " + k.path);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("save a copy to...");
        ImGui::SameLine();
        if (!k.is_default && ImGui::Button(ICON_FA_STAR "##default")) {
            auto st = hormiga::app_settings::load();
            st.default_database = k.path;
            hormiga::app_settings::save(st);
            ph.listed_at = -100.0;
            maiz::show_snackbar(ph.snack, k.name + " opens when Hormiga starts");
        }
        if (!is_open) {
            ImGui::SameLine();
            const bool armed = ph.confirm_remove == k.path;
            if (armed) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
            if (ImGui::Button(armed ? ICON_FA_TRASH "  Tap again to remove" : ICON_FA_TRASH "##rm")) {
                if (armed) {
                    fs::remove(k.path, ec);
                    ph.confirm_remove.clear();
                    ph.listed_at = -100.0;
                    maiz::show_snackbar(ph.snack, ec ? "Could not remove it: " + ec.message() : "Removed " + k.name);
                } else {
                    ph.confirm_remove = k.path;
                }
            }
            if (armed) ImGui::PopStyleColor();
        }
        ImGui::Unindent(pad);
        ImGui::Dummy(ImVec2(0, pad * 0.3f));
        ImGui::EndGroup();
        const ImVec2 r1(ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x, ImGui::GetItemRectMax().y);
        ImGui::GetWindowDrawList()->AddRect(r0, r1, ImGui::GetColorU32(is_open ? ImGuiCol_CheckMark : ImGuiCol_Border),
                                            10.0f * dp, 0, is_open ? 2.0f * dp : 1.0f);
        ImGui::PopID();
        ImGui::Spacing();
    }

    // ── bringing one in ──────────────────────────────────────────────────────
    ImGui::SeparatorText("Bring one in");
    if (ImGui::Button(ICON_FA_FILE_IMPORT "  Import a .miga", ImVec2(-FLT_MIN, 0))) {
        if (!app.on_pick_document || !app.on_pick_document(kDocImportMiga, "*/*"))
            maiz::show_snackbar(ph.snack, "This device has no file picker");
    }
    maiz::dim_wrapped("From Downloads, a drive or a chat. A .miga sent to you can also be opened straight from "
                      "there with Void Hormiga. Joining one shared on your network is on Migos.");
}

/* ── THE ONE DOOR EVERY SYSTEM ANSWER COMES BACK THROUGH ─────────────────── */
void HormigaApp::document_arrived(int request, const std::string& status, const std::string& path,
                                  const std::string& name) {
    if (status == "cancelled") return;
    if (status != "ok") {
        toast(name.empty() ? "that did not work" : name, true);
        return;
    }
    std::error_code ec;
    const bool ours = !path.empty() && fs::path(path).parent_path() == incoming_dir(*this); // a copy made for us
    if (request == kDocExportMiga) {
        toast("saved a copy");
        return;
    }
    if (request == kDocPhoto) {
        const std::string rune = phone ? phone->photo_rune : std::string();
        const std::string field = phone ? phone->photo_field : std::string();
        const std::string managed = ingest_asset(path);
        if (ours) fs::remove(path, ec); // the asset store has its own copy now
        if (managed.empty()) {
            toast("could not read that picture", true);
            return;
        }
        if (!rune.empty() && !field.empty() && scene.find(rune)) {
            pending_cmds.push_back("set " + rune + " " + field + " " + json_str(managed));
            toast("photo added");
        }
        return;
    }
    if (request == kDocImportMiga || request == kDocOpened) {
        if (fs::path(name.empty() ? path : name).extension() != ".miga") {
            if (ours) fs::remove(path, ec);
            toast("that is not a .miga database: " + (name.empty() ? path : name), true);
            return;
        }
        const fs::path dir = hormiga::device::get().databases;
        fs::create_directories(dir, ec);
        const fs::path dest = free_path(dir, fs::path(name.empty() ? path : name).stem().string());
        fs::rename(path, dest, ec);
        if (ec) { // across devices a rename fails: copy instead
            ec.clear();
            fs::copy_file(path, dest, ec);
            if (!ec && ours) fs::remove(path, ec);
        }
        if (ec) {
            toast("could not keep that database: " + ec.message(), true);
            return;
        }
        PhoneUi::keep_current(*this);
        open_database(dest.string());
        hormiga::app_settings::note_recent(dest.string());
        if (phone) {
            phone->listed_at = -100.0;
            phone->screen = kData;
        }
    }
}

/* The image editor's "Choose a photo" (ui/widgets.cpp), on a phone. */
void HormigaApp::phone_pick_photo(const std::string& rune, const std::string& field) {
    if (phone) {
        phone->photo_rune = rune;
        phone->photo_field = field;
    }
    if (on_pick_document && on_pick_document(kDocPhoto, "image/*")) return; // the answer comes later
    // no system picker (a desktop playing a phone): the ordinary dialog, answered now
    if (!on_pick_file) return;
    const std::string p = on_pick_file("");
    if (!p.empty()) document_arrived(kDocPhoto, "ok", p, fs::path(p).filename().string());
}
