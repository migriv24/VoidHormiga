/* app/shell_app.cpp — the application as data, on the desktop: a database's tabs
 * and its default layout (domain/shell.hpp; okf/concepts/platform/shell.md).
 *
 * WHEN A DATABASE OPENS (`shell_load`, from adopt_core): its tabs are read and
 * each built-in tab opens or starts closed as the database says; screens are
 * listed. Its default LAYOUT is applied once per device per `shell.id`: a person
 * who rearranges keeps their arrangement, and a later save by someone else does
 * not take it from them (the author: "database default + personal").
 *
 * THE DEVICE'S RECORD of which defaults it has applied lives in imgui.ini, as a
 * section of its own: device-local, written by ImGui with the rest of the
 * arrangement, and read on the first frame. The decision therefore waits for
 * that frame (`shell_frame`), and the layout itself is loaded just BEFORE a
 * frame begins (`take_pending_layout`, from the shell's loop), because ImGui
 * cannot reload its settings in the middle of one. */
#include "app/app_internal.hpp"
#include "antfarm/farm_verbs.hpp" // farm::tokenize: quotes group words
#include "domain/shell.hpp"
#include "antfarm/farm.hpp"   // farm::kMantle: where device nodes live

#include <cstring>
#include <random>

namespace {

void* shell_read_open(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    return std::strcmp(name, "Applied") == 0 ? (void*)1 : nullptr;
}
void shell_read_line(ImGuiContext*, ImGuiSettingsHandler* h, void*, const char* line) {
    if (std::strncmp(line, "Id=", 3) == 0 && line[3]) static_cast<HormigaApp*>(h->UserData)->shellui.applied.insert(line + 3);
}
void shell_write_all(ImGuiContext*, ImGuiSettingsHandler* h, ImGuiTextBuffer* buf) {
    const auto& ids = static_cast<HormigaApp*>(h->UserData)->shellui.applied;
    if (ids.empty()) return;
    buf->appendf("[%s][Applied]\n", h->TypeName);
    for (const auto& id : ids) buf->appendf("Id=%s\n", id.c_str());
    buf->append("\n");
}

/* The parts of ImGui's settings that are an ARRANGEMENT: windows and docking.
 * Tables, this file's own record and anything else stay on the device. */
std::string arrangement_only(const std::string& ini) {
    std::string out, section;
    bool keep = false;
    size_t i = 0;
    while (i < ini.size()) {
        size_t j = ini.find('\n', i);
        if (j == std::string::npos) j = ini.size();
        const std::string line = ini.substr(i, j - i);
        if (!line.empty() && line[0] == '[')
            keep = line.rfind("[Window][", 0) == 0 || line.rfind("[Docking][", 0) == 0;
        if (keep) out += line + "\n";
        i = j + 1;
    }
    return out;
}

/* A config value as the text that was meant. `config set` keeps its argument as
 * text, and `config get` hands it back as a JSON string, so a value written as a
 * JSON string (the layout, which must carry its newlines on one command line)
 * comes back wrapped twice. Unwrap while it is still a JSON string. */
std::string config_text(maiz::Core& core, const char* key) {
    std::string v = core.dispatch(std::string("config get ") + key).data;
    if (v.empty() || v == "null") return {};
    for (int i = 0; i < 3 && v.size() >= 2 && v.front() == '"'; ++i) {
        const nlohmann::json j = nlohmann::json::parse(v, nullptr, false);
        if (!j.is_string()) break;
        v = j.get<std::string>();
    }
    return v;
}

} // namespace

void HormigaApp::shell_register() {
    static ImGuiSettingsHandler h; // ImGui keeps the pointer: it must outlive the context
    if (h.TypeName || !ImGui::GetCurrentContext()) return; // the CLI has no window and no imgui.ini
    h.TypeName = "HormigaShell";
    h.TypeHash = ImHashStr("HormigaShell");
    h.ReadOpenFn = shell_read_open;
    h.ReadLineFn = shell_read_line;
    h.WriteAllFn = shell_write_all;
    h.UserData = this;
    ImGui::AddSettingsHandler(&h);
}

std::vector<std::string> HormigaApp::screen_mantles() {
    static std::vector<std::string> cached;
    static long long cached_gen = -1;
    if (cached_gen == (long long)model_gen) return cached;
    cached.clear();
    for (const std::string& m : list_documents()) {
        maiz::ProjectOptions po;
        po.mantle = m;
        for (const auto& n : maiz::project_scene(core, po).nodes)
            if (n.glyph == "document" && hormiga::temper::field_value(n, "kind") == "screen") cached.push_back(m);
    }
    cached_gen = (long long)model_gen;
    return cached;
}

bool* HormigaApp::shell_flag(const std::string& id) {
    if (id == "data") return &sec_open[Data];
    if (id == "builder") return &sec_open[Builder];
    if (id == "antfarm") return &sec_open[Antfarm];
    if (id == "map") return &sec_open[Map];
    if (id == "calendar") return &win_calendar;
    if (id == "notes") return &win_notes;
    if (id == "publish") return &win_publish;
    if (id == "allomone") return &win_allomone;
    if (id == "settings") return &show_settings;
    if (id == "style") return &win_style;
    if (id == "console") return &win_console;
    if (hormiga::shell::is_screen(id)) return &shellui.screen_open[hormiga::shell::screen_mantle(id)];
    return nullptr;
}

void HormigaApp::shell_load() {
    shellui.tabs = hormiga::shell::effective(hormiga::shell::parse(config_text(core, "shell.tabs")), screen_mantles());
    for (const auto& t : shellui.tabs)
        if (bool* f = shell_flag(t.id)) *f = !t.hidden;
    shellui.candidate_id = config_text(core, "shell.id");
    shellui.candidate_layout = config_text(core, "shell.layout");
    shellui.candidate_has_tabs = !config_text(core, "shell.tabs").empty();
}

void HormigaApp::shell_frame() {
    // the device's record is read on the first frame: decide after it is
    if (shellui.candidate_id.empty() || !GImGui->SettingsLoaded) return;
    if (!shellui.applied.count(shellui.candidate_id)) {
        if (!shellui.candidate_layout.empty()) shellui.pending_layout = shellui.candidate_layout; // its arrangement
        else if (shellui.candidate_has_tabs) shellui.reseed = true;                             // its order
        shellui.applied.insert(shellui.candidate_id);
        ImGui::MarkIniSettingsDirty();
    }
    shellui.candidate_id.clear();
    shellui.candidate_layout.clear();
}

std::string HormigaApp::take_pending_layout() {
    std::string l;
    l.swap(shellui.pending_layout);
    return l;
}

/* SAVE THIS LAYOUT TO THE DATABASE: the arrangement on screen, and which tabs
 * are open, become the database's default. The first save mints `shell.id`;
 * this device counts as having applied it. */
void HormigaApp::shell_save_layout() {
    size_t n = 0;
    const char* ini = ImGui::SaveIniSettingsToMemory(&n);
    const std::string layout = arrangement_only(std::string(ini, n));
    std::string id = config_text(core, "shell.id");
    if (id.empty()) {
        std::random_device rd;
        char buf[24];
        std::snprintf(buf, sizeof buf, "%08x%04x", rd(), rd() & 0xffff);
        id = buf;
    }
    std::vector<hormiga::shell::Tab> tabs = shellui.tabs;
    for (auto& t : tabs)
        if (bool* f = shell_flag(t.id)) t.hidden = !*f;
    pending_cmds.push_back("config set shell.tabs " + maiz::arg(hormiga::shell::serialize(tabs)));
    pending_cmds.push_back("config set shell.layout " + maiz::arg(nlohmann::json(layout).dump()));
    pending_cmds.push_back("config set shell.id " + maiz::arg(id));
    shellui.tabs = tabs;
    shellui.applied.insert(id);
    ImGui::MarkIniSettingsDirty();
    toast("this layout is now the database's: anyone who opens it gets it, and keeps their own changes");
}

void HormigaApp::shell_use_layout() {
    const std::string layout = config_text(core, "shell.layout");
    for (const auto& t : shellui.tabs)
        if (bool* f = shell_flag(t.id)) *f = !t.hidden;
    if (layout.empty()) shellui.reseed = true; // no saved arrangement: its tabs, in its order
    else shellui.pending_layout = layout;
}

/* `shell …` (domain/shell.hpp), plus the two that need the window:
 * `shell save-layout` and `shell use-layout`. */
bool HormigaApp::try_shell_verb(const std::string& cmd) {
    if (cmd != "shell" && cmd.rfind("shell ", 0) != 0) return false;
    const auto a = farm::tokenize(cmd);
    if (a.size() == 2 && a[1] == "save-layout") {
        shell_save_layout();
        log.push_back({">", cmd, "saved this layout to the database"});
        return true;
    }
    if (a.size() == 2 && a[1] == "use-layout") {
        shell_use_layout();
        log.push_back({">", cmd, "applied the database's layout"});
        return true;
    }
    const auto r = hormiga::shell::verb(a, shellui.tabs);
    for (const auto& c : r.commands) pending_cmds.push_back(c);
    if (!r.tabs.empty()) {
        shellui.tabs = r.tabs; // what was just written, without reading it back
        // the change shows at once here; another device applies it once, by this id
        if (config_text(core, "shell.id").empty()) {
            std::random_device rd;
            char buf[24];
            std::snprintf(buf, sizeof buf, "%08x%04x", rd(), rd() & 0xffff);
            pending_cmds.push_back("config set shell.id " + maiz::arg(buf));
            shellui.applied.insert(buf);
            ImGui::MarkIniSettingsDirty();
        }
        if (a[1] == "move") shellui.reseed = true;
        if (a[1] == "hide" || a[1] == "show")
            if (bool* f = shell_flag(a[2])) *f = a[1] == "show";
    }
    log.push_back({r.ok ? ">" : "error", cmd, r.text});
    return true;
}

/* ── DEVICES (okf/concepts/platform/shell.md, "Devices") ─────────────────────
 * `effect send <node> <text>` finds the Antfarm's `device` node by name and
 * queues the text, with the line ending the node asks for. What leaves is
 * exactly that text, to that address: the node's answer says so. */
bool HormigaApp::device_send(const std::string& node, const std::string& text) {
    maiz::ProjectOptions po;
    po.mantle = farm::kMantle;
    const maiz::Scene farm_scene = maiz::project_scene(core, po);
    const maiz::SceneNode* n = farm_scene.find(node);
    if (!n || n->glyph != "farm_device") {
        toast("send: no device node called " + node + " in the Antfarm", true);
        return false;
    }
    auto f = [&](const char* k) { return hormiga::temper::field_value(*n, k); };
    std::string payload = text;
    if (f("line_end") == "lf") payload += "\n";
    else if (f("line_end") == "crlf") payload += "\r\n";
    const int port = std::atoi(f("port").c_str());
    std::string err;
    if (port <= 0 || port > 65535 || !devices.queue(node, f("address"), (std::uint16_t)port, payload, &err)) {
        toast("send to " + node + ": " + (err.empty() ? std::string("its port is not set") : err), true);
        return false;
    }
    log.push_back({"info", "device", "sending " + std::to_string(payload.size()) + " bytes to " + node});
    return true;
}

void HormigaApp::devices_frame() {
    if (!devices.busy()) return;
    for (const auto& r : devices.pump()) {
        std::string line = r.who + ": " + r.text;
        if (!r.answered.empty()) line += "; it answered: " + r.answered.substr(0, 200);
        log.push_back({r.ok ? "info" : "error", "device", line});
        toast(line, !r.ok);
    }
}
