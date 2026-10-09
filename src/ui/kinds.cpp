/* ui/kinds.cpp — the Kinds window: name and shape the kinds of thing
 * (okf/concepts/foundation/kinds.md).
 *
 * The author, 2026-10-06: "what we want is to be able to organize and name our
 * data whatever we want. so contact, organization, event, etc. could be
 * renamed to: product, vendor, invoice, expiration date."
 *
 * Every change here is a dispatcher command in the `kinds` mantle (one batch,
 * one undo), so it is logged, undone, synced and replayable like any other.
 * Renaming changes what people READ: a contact stays glyph `contact`, a field
 * keeps its key. A kind the database makes is registered as a glyph from these
 * runes at the next projection (domain/kinds.hpp). */
#include "app/app_internal.hpp"

#include "voidmaiz/mobile.hpp" // dim_wrapped

#include <cfloat>

namespace {

std::string slug(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum(c)) o += (char)std::tolower(c);
        else if (!o.empty() && o.back() != '_') o += '_';
    }
    while (!o.empty() && o.back() == '_') o.pop_back();
    return o;
}

bool kinds_mantle_exists(maiz::Core& core) {
    for (const std::string& line : core.dispatch("mantles").lines)
        if (line.find(hormiga::kinds::kMantle) != std::string::npos) return true;
    return false;
}

const char* kEditors[] = {"text", "date", "multiline:60", "image"};
const char* kEditorLabels[] = {"Text", "Date", "Long text", "Picture"};

} // namespace

void install_kind_editors(maiz::WidgetRegistry& widgets) {
    /* WHICH KIND AN EVENT GRID SHOWS: events (blank, as always), or one of the
     * database's own dated kinds (domain/kinds.hpp, in_event_grid). */
    widgets.editors["datedkind"] = [](maiz::WidgetContext& ctx, const maiz::SceneNode& n,
                                      const maiz::SceneField& f, std::string_view) -> bool {
        const std::string cur = hormiga::temper::field_value(n, f.key.c_str());
        const auto& reg = hormiga::kinds::current();
        bool committed = false;
        const std::string lbl = f.label.empty() ? f.key : f.label; // beside it, as every field editor does
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.65f); // the label may clip, as others do
        const std::string shown = cur.empty() || cur == "event" ? reg.plural("event") : reg.plural(cur);
        if (ImGui::BeginCombo(("##" + f.key).c_str(), shown.c_str())) {
            if (ImGui::Selectable(reg.plural("event").c_str(), cur.empty() || cur == "event")) {
                ctx.commands.push_back("set " + n.name + " " + f.key + " \"\"");
                committed = true;
            }
            for (const hormiga::kinds::Kind* k : reg.own())
                if (k->dated && ImGui::Selectable((k->plural + "  (tagged clearance:public)").c_str(), cur == k->glyph)) {
                    ctx.commands.push_back("set " + n.name + " " + f.key + " " + json_str(k->glyph));
                    committed = true;
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(lbl.c_str());
        return committed;
    };
    /* WHICH KIND A DIRECTORY LISTS (2026-10-06, the kinds release): every
     * kind with the trait `listed`, as the database calls it, or all of
     * them ("both", the value a directory has always stored for that). */
    widgets.editors["listedkind"] = [](maiz::WidgetContext& ctx, const maiz::SceneNode& n,
                                       const maiz::SceneField& f, std::string_view) -> bool {
        std::string cur = hormiga::temper::field_value(n, f.key.c_str());
        const auto& reg = hormiga::kinds::current();
        auto name_of = [&](const std::string& g) {
            return g.empty() ? std::string("Contacts (the default)")
                   : g == "both" ? std::string("All of them") : reg.plural(g);
        };
        bool committed = false;
        const std::string lbl = f.label.empty() ? f.key : f.label; // beside it, as every field editor does
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.65f); // the label may clip, as others do
        if (ImGui::BeginCombo(("##" + f.key).c_str(), name_of(cur).c_str())) {
            for (const auto& k : reg.all)
                if (k.listed && ImGui::Selectable(k.plural.c_str(), cur == k.glyph)) {
                    ctx.commands.push_back("set " + n.name + " " + f.key + " " + json_str(k.glyph));
                    committed = true;
                }
            if (ImGui::Selectable("All of them", cur == "both")) {
                ctx.commands.push_back("set " + n.name + " " + f.key + " \"both\"");
                committed = true;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(lbl.c_str());
        return committed;
    };
}

/* Commands in the `kinds` mantle, as one batch, back in the mantle on screen. */
static void kind_commit(HormigaApp& app, maiz::Core& core, std::vector<std::string>& out, const std::string& back,
                        const std::vector<std::string>& cmds) {
    (void)app;
    if (cmds.empty()) return;
    std::vector<std::string> all;
    if (!kinds_mantle_exists(core)) all.push_back(std::string("mantle new ") + hormiga::kinds::kMantle);
    all.push_back(std::string("use ") + hormiga::kinds::kMantle);
    for (const auto& c : cmds) all.push_back(c);
    all.push_back("use " + back);
    out.push_back(maiz::compile_commit(all));
}

void HormigaApp::draw_kinds_window() {
    if (!show_kinds) return;
    ImGui::SetNextWindowSize(ImVec2(760, 560), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Kinds##window", &show_kinds)) {
        ImGui::End();
        return;
    }
    static std::string sel;
    static char new_title[64] = {}, new_field[64] = {};
    const auto& reg = hormiga::kinds::current();
    const std::string back = scene.mantle.empty() ? std::string(kDataMantle) : scene.mantle;
    auto commit = [&](const std::vector<std::string>& cmds) { kind_commit(*this, core, pending_cmds, back, cmds); };
    // a built-in kind gets its rune the first time it is renamed
    auto ensure = [&](const hormiga::kinds::Kind& k, std::vector<std::string>& cmds) {
        maiz::Scene ks;
        try {
            maiz::ProjectOptions po;
            po.mantle = hormiga::kinds::kMantle;
            ks = maiz::project_scene(core, po);
        } catch (...) {
        }
        if (!ks.find(k.glyph)) cmds.push_back("rune new kind " + k.glyph);
    };

    maiz::dim_wrapped("Call the kinds of thing in this database whatever you call them, and make new ones. "
                      "Renaming changes what people read; what is stored stays the same.");
    for (const auto& c : reg.clashes) // a kind named like the application's own glyph is never used
        ImGui::TextColored(ImVec4(0.8f, 0.3f, 0.2f, 1.0f),
                           ICON_FA_TRIANGLE_EXCLAMATION "  A kind here is called '%s', which is a name the application "
                           "uses itself: it is ignored until it is renamed in the kinds mantle.",
                           c.c_str());
    ImGui::Separator();

    // ── the list ──────────────────────────────────────────────────────────────
    ImGui::BeginChild("##kinds-list", ImVec2(230, 0), ImGuiChildFlags_Borders);
    std::map<std::string, int> counts;
    for (const auto& n : scene.nodes) ++counts[n.glyph];
    for (int pass = 0; pass < 2; ++pass) {
        ImGui::TextDisabled(pass == 0 ? "This database's own" : "Built in");
        for (const auto& k : reg.all) {
            if (k.builtin != (pass == 1)) continue;
            if (k.legacy && !k.defined && !counts[k.glyph]) continue; // 0.1.12's product, only where used
            const std::string row = std::string(kind_icon_named(k.icon)) + "  " + k.plural + "  (" +
                                    std::to_string(counts[k.glyph]) + ")##" + k.glyph;
            if (ImGui::Selectable(row.c_str(), sel == k.glyph)) sel = k.glyph;
            if (k.builtin && k.defined && ImGui::IsItemHovered()) ImGui::SetTooltip("stored as '%s'", k.glyph.c_str());
        }
        ImGui::Spacing();
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool go = ImGui::InputTextWithHint("##newkind", "a new kind: Vendor, Device...", new_title, sizeof new_title,
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    if ((ImGui::Button(ICON_FA_PLUS "  New kind", ImVec2(-FLT_MIN, 0)) || go) && new_title[0]) {
        std::string g = slug(new_title);
        // never the name of a kind already here, nor of one of the APPLICATION's
        // glyphs (a block, the map, a note): domain/kinds.hpp, apply
        const bool app_glyph = !g.empty() && !reg.find(g) && core.dispatch("glyphs " + g).ok;
        if (g.empty() || reg.find(g) || app_glyph) {
            toast(app_glyph ? "'" + std::string(new_title) + "' is a name the application uses itself: pick another"
                            : "pick another name: '" + std::string(new_title) + "' is taken",
                  true);
        } else {
            commit({"rune new kind " + g, "set " + g + " title " + json_str(new_title),
                    "set " + g + " plural " + json_str(std::string(new_title) + "s"), "set " + g + " icon \"tag\"",
                    "set " + g + " category \"Things\"", "setjson " + g + " fields " + json_arg("[]")});
            sel = g;
            new_title[0] = 0;
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── the kind being edited ─────────────────────────────────────────────────
    ImGui::BeginChild("##kind-edit", ImVec2(0, 0));
    const hormiga::kinds::Kind* k = reg.find(sel);
    if (!k) {
        ImGui::TextDisabled("Pick a kind on the left.");
        ImGui::EndChild();
        ImGui::End();
        return;
    }
    ImGui::Text("%s  %s", kind_icon_named(k->icon), k->title.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(k->builtin ? "(built in, stored as '%s')" : "(stored as '%s')", k->glyph.c_str());

    auto text_field = [&](const char* label, const char* key, const std::string& cur) {
        static std::map<std::string, std::string> edit; // what is being typed, per kind and key
        std::string& buf = edit[k->glyph + "|" + key];
        if (!ImGui::IsItemActive() && buf.empty()) buf = cur;
        char b[96];
        std::snprintf(b, sizeof b, "%s", buf.c_str());
        ImGui::SetNextItemWidth(260);
        if (ImGui::InputText(label, b, sizeof b)) buf = b;
        if (ImGui::IsItemDeactivatedAfterEdit() && buf != cur && !buf.empty()) {
            std::vector<std::string> cmds;
            ensure(*k, cmds);
            cmds.push_back("set " + k->glyph + " " + key + " " + json_str(buf));
            commit(cmds);
        }
        if (!ImGui::IsItemActive() && buf != cur && !ImGui::IsItemDeactivatedAfterEdit()) buf = cur;
    };
    ImGui::SeparatorText("Called");
    text_field("one##title", "title", k->title);
    text_field("many##plural", "plural", k->plural);
    text_field("group in the palette##category", "category", k->category);

    ImGui::SeparatorText("Looks");
    {
        const auto& names = kind_icon_names();
        const float cell = 30 + ImGui::GetStyle().ItemSpacing.x; // as many to a row as fit
        const int per = std::max(1, (int)((ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x) / cell));
        int i = 0;
        for (const auto& nm : names) {
            if (i++ % per) ImGui::SameLine();
            ImGui::PushID(nm.c_str());
            const bool on = nm == k->icon;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(kind_icon_named(nm), ImVec2(30, 30)) && !on) {
                std::vector<std::string> cmds;
                ensure(*k, cmds);
                cmds.push_back("set " + k->glyph + " icon " + json_str(nm));
                commit(cmds);
            }
            if (on) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", nm.c_str());
            ImGui::PopID();
        }
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(kind_colour_u32(k->color, IM_COL32(120, 120, 130, 255)));
        float rgb[3] = {c.x, c.y, c.z};
        if (ImGui::ColorEdit3("colour", rgb, ImGuiColorEditFlags_NoInputs)) {
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char hex[8];
            std::snprintf(hex, sizeof hex, "#%02x%02x%02x", (int)(rgb[0] * 255 + 0.5f), (int)(rgb[1] * 255 + 0.5f),
                          (int)(rgb[2] * 255 + 0.5f));
            std::vector<std::string> cmds;
            ensure(*k, cmds);
            cmds.push_back("set " + k->glyph + " color " + json_str(hex));
            commit(cmds);
        }
    }

    ImGui::SeparatorText("What it can do");
    auto trait = [&](const char* label, const char* tag, bool on, const char* tip) {
        bool v = on;
        if (ImGui::Checkbox(label, &v)) {
            std::vector<std::string> cmds;
            ensure(*k, cmds);
            cmds.push_back("tag " + k->glyph + (v ? " +" : " -") + std::string("trait:") + tag);
            commit(cmds);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    };
    if (k->builtin) {
        const std::string can = std::string("On a map: ") + (k->located ? "yes" : "no") + ".  On the calendar: " +
                                (k->dated ? "yes, by its '" + k->date_field + "'" : std::string("no")) +
                                ".  In directories: " + (k->listed ? "yes" : "no") +
                                ". A built-in kind keeps what it can do; its name and looks are yours.";
        maiz::dim_wrapped(can.c_str());
    } else {
        trait("on a map, and on every canvas", "located", k->located,
              "it carries a position: placed on Earth, a floor plan, any canvas");
        trait("on the calendar", "dated", k->dated, "its date field puts it on the calendar");
        trait("in directories (website, newsletter)", "listed", k->listed,
              "it may be listed in a directory block");
    }

    // ── fields ───────────────────────────────────────────────────────────────
    /* A new kind's fields are all its own. A BUILT-IN's come with the
     * application (Q109, the author's lean taken 2026-10-06): their labels are
     * this database's to change and it may add fields of its own, but a field's
     * KEY, which is what the data stores, never changes and an application
     * field is never removed. Only what differs from the application is stored. */
    ImGui::SeparatorText("Fields");
    struct Row {
        hormiga::kinds::Field f;
        std::string base_label; // the application's label ("" = the database's own field)
    };
    std::vector<Row> rows;
    if (k->builtin)
        for (const auto& bf : hormiga::kinds::builtin_fields(k->glyph)) rows.push_back({bf, bf.label});
    for (const auto& f : k->fields) {
        bool known = false;
        for (auto& r : rows)
            if (r.f.key == f.key) {
                if (!f.label.empty()) r.f.label = f.label;
                known = true;
            }
        if (!known) rows.push_back({f, ""});
    }
    bool changed = false;
    int remove = -1;
    for (size_t i = 0; i < rows.size(); ++i) {
        auto& r = rows[i];
        const bool app_field = !r.base_label.empty();
        ImGui::PushID((int)i);
        char lb[64];
        std::snprintf(lb, sizeof lb, "%s", r.f.label.c_str());
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##label", lb, sizeof lb);
        if (ImGui::IsItemDeactivatedAfterEdit() && lb[0] && r.f.label != lb) {
            r.f.label = lb;
            changed = true;
        }
        if (app_field && ImGui::IsItemHovered() && r.f.label != r.base_label)
            ImGui::SetTooltip("the application calls it '%s'", r.base_label.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", r.f.key.c_str());
        ImGui::SameLine(330);
        if (app_field) {
            ImGui::TextDisabled(r.f.label != r.base_label ? "renamed here" : "");
        } else {
            int ed = 0;
            for (int e = 0; e < 4; ++e)
                if (r.f.editor == kEditors[e] || (e == 0 && r.f.editor.empty())) ed = e;
            ImGui::SetNextItemWidth(110);
            if (ImGui::Combo("##editor", &ed, kEditorLabels, 4)) {
                r.f.editor = ed == 0 ? std::string() : kEditors[ed];
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_XMARK)) remove = (int)i;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("remove the field from the kind\n(what is already stored in it stays)");
        }
        ImGui::PopID();
    }
    if (remove >= 0) {
        rows.erase(rows.begin() + remove);
        changed = true;
    }
    ImGui::SetNextItemWidth(200);
    const bool add = ImGui::InputTextWithHint("##newfield", "a new field: Price, Expires...", new_field,
                                              sizeof new_field, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button(ICON_FA_PLUS "  Field") || add) && new_field[0]) {
        const std::string key = slug(new_field);
        bool taken = key.empty() || key == "title" || key == "geo";
        for (const auto& r : rows) taken = taken || r.f.key == key;
        if (taken) toast("a field called that is already here", true);
        else {
            rows.push_back({{key, new_field, ""}, ""});
            changed = true;
        }
        new_field[0] = 0;
    }
    if (changed) { // what differs from the application, and the database's own fields
        nlohmann::json j = nlohmann::json::array();
        for (const auto& r : rows)
            if (r.base_label.empty() || r.f.label != r.base_label)
                j.push_back({{"key", r.f.key}, {"label", r.f.label}, {"editor", r.f.editor}});
        std::vector<std::string> cmds;
        ensure(*k, cmds);
        cmds.push_back("setjson " + k->glyph + " fields " + json_arg(j.dump()));
        commit(cmds);
    }
    if (!k->builtin) { // which field is its date, and which shows under its name
        auto pick = [&](const char* label, const char* key, const std::string& cur) {
            if (ImGui::BeginCombo(label, cur.empty() ? "(none)" : cur.c_str())) {
                if (ImGui::Selectable("(none)", cur.empty())) commit({"set " + k->glyph + " " + key + " \"\""});
                for (const auto& f : k->fields)
                    if (ImGui::Selectable((f.label + "##" + f.key).c_str(), f.key == cur))
                        commit({"set " + k->glyph + " " + key + " " + json_str(f.key)});
                ImGui::EndCombo();
            }
        };
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200);
        if (k->dated) pick("its date (on the calendar)", "date_field", k->date_field);
        ImGui::SetNextItemWidth(200);
        pick("shown under its name", "subtitle_field", k->subtitle_field);
    }

    /* ── LETTING A KIND GO ───────────────────────────────────────────────────
     * A database's own kind is deleted only when nothing is of that kind:
     * runes whose kind no longer exists would be data no screen can name. A
     * renamed built-in goes back to the application's own names and looks,
     * which changes nothing stored. Both are one undoable command. */
    ImGui::Spacing();
    ImGui::Separator();
    const int n_of = counts[k->glyph];
    if (!k->builtin) {
        ImGui::BeginDisabled(n_of > 0);
        if (ImGui::Button(ICON_FA_TRASH "  Delete this kind")) {
            commit({"rm " + k->glyph});
            sel.clear();
        }
        ImGui::EndDisabled();
        if (n_of > 0 && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%d %s of this kind: move or delete them first", n_of, n_of == 1 ? "thing is" : "things are");
    } else if (k->defined) {
        if (ImGui::Button(ICON_FA_ROTATE_LEFT "  Back to the application's own")) commit({"rm " + k->glyph});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("its name, looks and field labels as the application ships them;\n"
                              "nothing stored changes; values in fields you added stay\n"
                              "in the database, hidden until the field is added back");
    }
    ImGui::EndChild();
    ImGui::End();
}
