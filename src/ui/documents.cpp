/* ui/documents.cpp — a DOCUMENT is a project, and this is the file that
 * manages one.
 *
 * ── WHY IT LEFT builder.cpp (2026-09-02) ─────────────────────────────────────
 *
 * `tools/find_long.py` said so, and this time the ratchet was pointing at a real
 * seam rather than at growth that happened to be warranted. `builder.cpp` is the
 * Builder SECTION — palette, canvas, inspector, the page manager, the drawing of
 * a document. Managing the set of documents is a different job: listing them,
 * minting one, renaming one, deleting one, capturing one as a portable file,
 * loading one back, and the templates that seed one.
 *
 * The tell is that none of it draws the canvas and none of it reads a selection.
 * It is `builder-roadmap.md`'s "DOCUMENTS AS PROJECTS" enabler, which has always
 * been described as its own thing, living in the file it was first typed into.
 *
 * ── WHAT A DOCUMENT IS ───────────────────────────────────────────────────────
 *
 * A mantle. Everything except the data, Antfarm, Allomone and civic mantles is a
 * document, which is what `list_documents()` encodes. So "save" is the org save
 * (they persist like every other rune), "export" is a portable JSON of the
 * commands that rebuild it, and a template is that same shape with a name —
 * which is why `capture_doc_json` serves both and why saving a template and
 * exporting a document are one function apart.
 *
 * RENAME AND DELETE ARRIVED 2026-09-02, when it turned out the Void Core verbs
 * they had been listed as blocked on (`mantle rename`, `mantle rm`) had landed
 * some time earlier and nothing had told the roadmap.
 */
#include "app/app_internal.hpp"
#include "json.hpp" // template bodies and captured documents are JSON

/* Apply a TEMPLATE: clear the current document's elements, set the theme, and
 * replay the template's build commands — the clear+build as ONE undoable batch
 * (Ctrl+Z reverts the whole thing). Theme is config-tier (kept in sync with the
 * Style-tab members). Runs in the issue mantle, then restores the data home. */
void HormigaApp::apply_template(const hormiga::DocTemplate& t) {
    maiz::ProjectOptions io;
    io.mantle = cur_doc;
    maiz::Scene issue = maiz::project_scene(core, io);
    std::vector<std::string> batch;
    for (const auto& n : issue.nodes) batch.push_back("rm " + n.name);
    for (const auto& c : t.commands) batch.push_back(c);

    pending_cmds.push_back(std::string("use ") + cur_doc);
    pending_cmds.push_back(maiz::compile_commit(batch)); // clear+build, one undo
    auto sethex = [&](const char* key, const std::string& hex, float out[3]) {
        unsigned r, g, b;
        if (hex.size() >= 7 && std::sscanf(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
            out[0] = r / 255.0f; out[1] = g / 255.0f; out[2] = b / 255.0f;
            pending_cmds.push_back(std::string("config set ") + key + " \"" + hex + "\"");
        }
    };
    sethex("theme.accent", t.accent, theme_accent);
    sethex("theme.bg", t.bg, theme_bg);
    sethex("theme.ink", t.ink, theme_ink);
    if (t.preset >= 0) {
        theme_preset = t.preset;
        pending_cmds.push_back("config set theme.preset \"" + std::to_string(t.preset) + "\"");
    }
    if (t.font >= 0) {
        theme_font = t.font;
        pending_cmds.push_back("config set theme.font \"" + std::to_string(t.font) + "\"");
    }
    if (t.dark >= 0) {
        theme_dark = (t.dark != 0);
        pending_cmds.push_back(std::string("config set theme.dark \"") + (t.dark ? "1" : "0") + "\"");
    }
    pending_cmds.push_back(std::string("use ") + kDataMantle);
    ed.selection.clear();
    cur_page.clear();
    show_templates = false;
    toast("applied template '" + t.name + "' - edit it, then Live preview");
}

/* Save the current document as a user template (templates/<slug>.json): each
 * element becomes rune new + set commands, plus the current theme. The same
 * shape as the built-ins, so it lists beside them. */
void HormigaApp::save_current_as_template(const std::string& name) {
    std::string slug = name;
    for (char& c : slug)
        if (!std::isalnum((unsigned char)c)) c = '-';
    std::error_code ec;
    fs::create_directories(data_dir("templates"), ec);
    std::ofstream o(data_dir("templates") / (slug + ".json"),
                    std::ios::binary | std::ios::trunc);
    o << capture_doc_json(name);
    toast("saved template '" + name + "' to templates/" + slug + ".json");
}

/* Capture the CURRENT document (cur_doc's elements + the theme) into the
 * portable name/kind/theme/commands JSON shared by templates and document
 * files — the replayable-transcript philosophy, serialized. */
std::string HormigaApp::capture_doc_json(const std::string& name) {
    maiz::ProjectOptions io;
    io.mantle = cur_doc;
    maiz::Scene issue = maiz::project_scene(core, io);
    bool has_pages = false;
    for (const auto& n : issue.nodes)
        if (n.glyph == "page") has_pages = true;
    nlohmann::json j;
    j["name"] = name;
    j["kind"] = has_pages ? "website" : "newsletter";
    j["desc"] = "Saved from a Hormiga document.";
    SiteTheme th = read_site_theme(core);
    j["accent"] = th.accent; j["bg"] = th.bg; j["ink"] = th.ink;
    j["preset"] = th.preset; j["font"] = th.font; j["dark"] = th.dark ? 1 : 0;
    nlohmann::json cmds = nlohmann::json::array();
    for (const auto& n : issue.nodes) {
        cmds.push_back("rune new " + n.glyph + " " + n.name);
        for (const auto& f : n.fields) {
            std::string v = f.value_json;
            if (f.is_string && v.size() >= 2) v = v.substr(1, v.size() - 2);
            if (v.empty() || v == "null") continue;
            cmds.push_back("set " + n.name + " " + f.key + " " + json_str(v));
        }
    }
    j["commands"] = cmds;
    return j.dump(2);
}

/* Export the current document to a portable FILE (documents/<slug>.json). Load
 * it back with import_document — or on another machine/org (it references data
 * by query, so it re-binds to whatever contacts/events live there). */
void HormigaApp::export_document() {
    std::string slug = cur_doc;
    std::error_code ec;
    fs::create_directories(data_dir("documents"), ec);
    fs::path out = data_dir("documents") / (slug + ".json");
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    o << capture_doc_json(cur_doc);
    toast("saved document to documents/" + slug + ".json");
    if (on_open) on_open(out.string());
}

/* Import a document FILE → a NEW document mantle (never clobbers the current
 * one; unlike a template, which replaces). Switches the Builder to it. */
void HormigaApp::import_document() {
    if (!on_pick_file) {
        toast("no file picker available", true);
        return;
    }
    std::string path = on_pick_file("");
    if (path.empty()) return;
    nlohmann::json j;
    try {
        std::ifstream in(path);
        j = nlohmann::json::parse(in);
    } catch (...) {
        toast("could not read that document file", true);
        return;
    }
    // a unique mantle name from the file's name (check the mantles list —
    // never probe with `use`, which would mutate the active mantle)
    std::string base = j.value("name", fs::path(path).stem().string());
    std::string mantle = base;
    for (char& c : mantle)
        if (!std::isalnum((unsigned char)c) && c != '-') c = '-';
    std::set<std::string> existing;
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' '))
            line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        existing.insert(line);
    }
    std::string uniq = mantle;
    for (int n = 2; existing.count(uniq); ++n)
        uniq = mantle + "-" + std::to_string(n);
    pending_cmds.push_back("mantle new " + uniq);
    for (const auto& c : j.value("commands", nlohmann::json::array()))
        pending_cmds.push_back(c.get<std::string>());
    pending_cmds.push_back(std::string("use ") + kDataMantle);
    cur_doc = uniq;
    cur_page.clear();
    ed.selection.clear();
    toast("imported document '" + uniq + "' - now the active document");
}

std::vector<hormiga::DocTemplate> HormigaApp::load_user_templates() const {
    std::vector<hormiga::DocTemplate> out;
    std::error_code ec;
    fs::path dir = data_dir("templates");
    if (!fs::exists(dir)) return out;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.path().extension() != ".json") continue;
        try {
            std::ifstream in(e.path());
            auto j = nlohmann::json::parse(in);
            hormiga::DocTemplate t;
            t.name = j.value("name", e.path().stem().string());
            t.kind = j.value("kind", "newsletter");
            t.desc = j.value("desc", "");
            t.accent = j.value("accent", ""); t.bg = j.value("bg", "");
            t.ink = j.value("ink", "");
            t.preset = j.value("preset", -1); t.font = j.value("font", -1);
            t.dark = j.value("dark", -1);
            for (const auto& c : j.value("commands", nlohmann::json::array()))
                t.commands.push_back(c.get<std::string>());
            out.push_back(std::move(t));
        } catch (...) {}
    }
    return out;
}

/* The TEMPLATES window: start from a designed layout (built-ins + user), or
 * save the current document as one. Newsletter vs website templates are
 * labeled; the website ones are the richer, multi-page kind. */
void HormigaApp::draw_templates_window() {
    if (!show_templates) return;
    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Templates", &show_templates)) {
        ImGui::TextWrapped("Start from a designed layout. Applying a template "
                           "REPLACES the current document (undo with Ctrl+Z).");
        ImGui::Spacing();
        auto row = [&](const hormiga::DocTemplate& t, bool user) {
            ImGui::PushID(t.name.c_str());
            ImGui::SeparatorText(
                (t.name + "   [" + t.kind + (user ? ", saved]" : "]")).c_str());
            ImGui::PushTextWrapPos(0);
            ImGui::TextDisabled("%s", t.desc.c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button(("Use this template##" + t.name).c_str()))
                apply_template(t);
            ImGui::PopID();
        };
        ImGui::SeparatorText("Built-in");
        for (const auto& t : hormiga::builtin_templates()) row(t, false);
        auto user = load_user_templates();
        if (!user.empty()) {
            ImGui::Spacing();
            for (const auto& t : user) row(t, true);
        }
        ImGui::Spacing();
        ImGui::SeparatorText("Save the current document as a template");
        ImGui::SetNextItemWidth(-90);
        ImGui::InputTextWithHint("##tplname", "template name...",
                                 template_save_name, sizeof template_save_name);
        ImGui::SameLine();
        ImGui::BeginDisabled(!template_save_name[0]);
        if (ImGui::Button("Save")) {
            save_current_as_template(template_save_name);
            template_save_name[0] = 0;
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
}

/* Documents are mantles (author 2026-07-23): everything except the Data and
 * Antfarm mantles is a builder DOCUMENT (a newsletter or website you can save
 * and switch between). They persist in the database like everything else. */
std::vector<std::string> HormigaApp::list_documents() {
    std::vector<std::string> out;
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' '))
            line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (line.empty() || line == "(no mantles)" || line == kDataMantle ||
            line == "antfarm")
            continue;
        out.push_back(line);
    }
    return out;
}

/* Create a new document (a fresh mantle) with a starter hero, switch the
 * Builder to it. One undoable-ish sequence; the data home is restored. */
void HormigaApp::new_document(const std::string& name) {
    std::string mantle = name;
    for (char& c : mantle) // command-safe mantle name
        if (!std::isalnum((unsigned char)c) && c != '-') c = '-';
    if (mantle.empty()) return;
    if (scene.find(mantle)) { toast("a document named that exists", true); return; }
    pending_cmds.push_back("mantle new " + mantle); // creates + makes active
    pending_cmds.push_back("rune new hero doc-hero");
    pending_cmds.push_back("set doc-hero title_en " + json_str(name));
    pending_cmds.push_back("set doc-hero row \"0\"");
    pending_cmds.push_back(std::string("use ") + kDataMantle);
    cur_doc = mantle;
    cur_page.clear();
    ed.selection.clear();
    toast("new document '" + mantle + "' - build it, then Email preview / "
          "Build website");
}

/* ── RENAME AND DELETE A DOCUMENT (2026-09-02) ───────────────────────────────
 *
 * `builder-roadmap.md` has carried *"Still ⬜: delete + rename (blocked on Core
 * `mantle rm`/`rename` — MESSAGE_FOR_VOIDCORE 2026-07-23, interim is
 * clear-elements)"* since the document picker was built. **Both verbs are in
 * Void Core now** — `verbs_edit.c` implements `mantle rm <name>` and
 * `mantle rename <old> <new>`, and the 2026-09-02 field report used the latter
 * in anger to fix a misnamed data mantle. The blocker is gone and the note had
 * outlived it, which is its own small lesson about roadmap entries that name a
 * dependency: nothing tells you when it lands.
 *
 * RENAME is safe and needs no ceremony. Core rejects a name that is taken,
 * keeps every rune, and moves the active-mantle pointer with it — so the only
 * thing left here is to move `cur_doc` too, and to slug the input so a
 * document's name stays usable as a command argument.
 *
 * DELETE IS NOT SAFE and this is the only place in the Builder that will
 * permanently remove a person's work. `mantle rm` takes the mantle and every
 * rune in it, and while the journal makes it undoable in principle, the
 * document picker is not where somebody discovers `undo`. So it asks, it names
 * the document and its element count in the question, and it types the
 * consequence out rather than saying "are you sure?" — the same reasoning the
 * Publish panel's confirmation is built on.
 *
 * THE THREE MANTLES THAT ARE NOT DOCUMENTS cannot reach this menu at all,
 * because `list_documents()` already excludes them. That is worth stating
 * because it is what makes a delete button tolerable here: the worst thing this
 * control can do is destroy one newsletter, not the organization's data.
 */
void HormigaApp::rename_document(const std::string& to) {
    std::string mantle;
    for (char c : to) // command-safe, same rule `new_document` applies
        if (std::isalnum((unsigned char)c) || c == '-') mantle += c;
        else if (!mantle.empty() && mantle.back() != '-') mantle += '-';
    while (!mantle.empty() && mantle.back() == '-') mantle.pop_back();
    if (mantle.empty() || mantle == cur_doc) return;
    const maiz::Result r =
        dispatch_and_reproject("mantle rename " + cur_doc + " " + mantle);
    if (!r.ok) return; // dispatch_and_reproject has already toasted the reason
    cur_doc = mantle;
    /* The core moved the ACTIVE mantle with the rename, and the Builder's
     * convention is that the data mantle is what sits active between edits.
     * Putting it back here rather than leaving it means the next Data-side
     * command does not quietly land in the document. */
    dispatch_and_reproject(std::string("use ") + kDataMantle);
    toast("renamed to '" + mantle + "'");
}

void HormigaApp::delete_document(const std::string& name) {
    if (name.empty()) return;
    const std::vector<std::string> docs = list_documents();
    /* THE LAST DOCUMENT STAYS. Removing it would leave the Builder pointing at
     * a mantle that does not exist, with no picker entry to recover through —
     * an empty canvas that cannot be typed out of. Clearing its elements is the
     * operation somebody actually wants there, and it already exists. */
    if (docs.size() <= 1) {
        toast("this is the only document - clear its elements instead of "
              "deleting it", true);
        return;
    }
    if (!dispatch_and_reproject("mantle rm " + name).ok) return;
    for (const std::string& d : docs)
        if (d != name) { cur_doc = d; break; }
    cur_page.clear();
    ed.selection.clear();
    dispatch_and_reproject(std::string("use ") + kDataMantle);
    toast("deleted '" + name + "' - Ctrl+Z undoes it");
}

/* How many elements a document holds, for the delete confirmation. A count is
 * the difference between "delete this?" and "delete these 34 things?", and the
 * second is the question a person can actually answer. */
int HormigaApp::document_element_count(const std::string& name) {
    maiz::ProjectOptions po;
    po.mantle = name;
    return (int)maiz::project_scene(core, po).nodes.size();
}
