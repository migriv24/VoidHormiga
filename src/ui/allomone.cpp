/* section_allomone.cpp — Allomone: the derive-only rules engine's host side.
 * Split out of app.cpp 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * The LANGUAGE lives upstream in Void Maiz and the DOMAIN vocabulary lives in
 * hormiga_allomone.{hpp,cpp}; what is here is the host — the per-surface style
 * cache, the `web-hide` output effect, the conflict inspector, the reference
 * pane, and the script IDE built on `maiz::code_editor`.
 *
 * It also carries the LEGACY dialect behind `HORMIGA_LEGACY_ALLOMONE`, which is
 * a good reason for this file to be its own unit: the frozen interpreter and
 * the shipped one now sit side by side where the difference is legible, instead
 * of two thousand lines apart in a file about everything.
 */
#include "app/app_internal.hpp"
#include "json.hpp" // script bodies travel through setjson
#include "domain/allomone_legacy.hpp" // the frozen dialect, still deriving

// ── ALLOMONE ENGINE (derive-only): the color the scripts derived for this node,
// looked up from the cache the interpreter filled (refresh_allo_rules → the
// scripts ran over the data). NEVER writes the model — styling is derived.
bool HormigaApp::rule_color_for(const maiz::SceneNode& n, unsigned& out) const {
    auto it = allo_colors.find(n.name);
    if (it == allo_colors.end()) return false;
    out = it->second;
    return true;
}

// What Allomone derived for this rune on this SURFACE, or null. Null covers
// both "no rule said anything" and "two sources disagreed" — deliberately the
// same answer, because a renderer must treat a contested value exactly as it
// treats an absent one.
const HormigaApp::AlloStyle* HormigaApp::allo_style_for(std::string_view domain,
                                                        const std::string& rune) const {
    auto d = allo_style.find(std::string(domain));
    if (d == allo_style.end()) return nullptr;
    auto it = d->second.find(rune);
    return it == d->second.end() ? nullptr : &it->second;
}

// ── THE OUTPUT DOMAIN'S ONE EFFECT: `web-hide "1"` keeps a rune out of every
// query-backed block, in the newsletter, on the website, and in the Builder's
// live preview — which shows what will be published, so it must agree.
//
// This is domains.md's third domain arriving: content management at the export
// seam, "which runes flow", scoped to the OUTPUT codomain and distinct from the
// on-screen filters. It is what makes `when internal "" then web-hide "1"` a
// real sentence — the privacy seam expressed as a rule the org writes once,
// rather than a template convention somebody has to remember.
//
// A CONFLICTED `web-hide` does NOT hide. That is the deliberate direction: two
// scripts disagreeing about whether something may be published is exactly when
// a human should decide, and silently suppressing content nobody agreed to
// suppress is the failure that looks like the export being broken. The
// complementary guarantee — that no derived value can CARRY internal content in
// the first place — is enforced a layer down, in the frame, and does not depend
// on this one (hormiga_allomone.hpp, the privacy seam).
bool HormigaApp::allo_web_hidden(const std::string& rune) const {
    const AlloStyle* st = allo_style_for("web", rune);
    return st && st->hide;
}

// ════════════════════════════════════════════════════════════════════════════
// THE ALLOMONE TAB (Void Maiz dialect, 2026-08-10)
//
// Three panes over one derivation. The whole tab reads `allo_derived` — the
// same object the cards are painted from — rather than re-deriving anything,
// so the UI cannot disagree with what is on screen. Nothing here writes to the
// model except through the dispatcher.
//
//   Scripts    — the sources, their diagnostics, and a staged editor
//   Conflicts  — the ⊤ cells, WHO disagrees, and settling one as a command
//   Reference  — the merge laws and our domain predicates, which are the real
//                surface area of the language for someone writing a rule
// ════════════════════════════════════════════════════════════════════════════


// ════════════════════════════════════════════════════════════════════════════
// THE CIVIC RECORD (okf/concepts/projects/civic-record.md)
//
// A reading surface, not an editor. Everything here is editable in the Data tab
// already; what the Data tab cannot show is the part that makes this domain
// interesting, because **none of the interesting facts live on a single rune**:
//
//   "what did 3.3.1 say in 2024"  is a composition of dated assertions
//   "who sat in ward 2 then"      is a query over terms
//   "these two amendments clash"  is a merge reaching ⊤
//
// A list of runes shows none of that. So the date control at the top is not a
// filter — it is **the projection made visible**, and moving it is the clearest
// demonstration in the application that derived state is derived.
// ════════════════════════════════════════════════════════════════════════════

void HormigaApp::draw_civic_body() {
    maiz::ProjectOptions po;
    po.mantle = kCivicMantle;
    maiz::Scene cs;
    try { cs = maiz::project_scene(core, po); } catch (...) {}
    if (cs.nodes.empty()) {
        ImGui::TextWrapped(
            "No civic record in this database. The `civic` mantle holds "
            "policies, their provisions, and the dated assertions that change "
            "them - see okf/concepts/projects/civic-record.md.");
        return;
    }

    // ── the as-of date: the whole point of the surface ──────────────────────
    if (!civic_date[0]) { // default to today, once
        int y = 0, m = 0, d = 0;
        cal_today(y, m, d);
        std::snprintf(civic_date, sizeof civic_date, "%04d-%02d-%02d", y, m, d);
    }
    ImGui::TextUnformatted("As of");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::InputText("##asof", civic_date, sizeof civic_date);
    ImGui::SameLine();
    // A year of jumps, because the interesting question is almost always "and
    // what did it say before that?"
    auto shift_year = [&](int dy) {
        int y = 0, m = 0, d = 0;
        if (std::sscanf(civic_date, "%d-%d-%d", &y, &m, &d) == 3)
            std::snprintf(civic_date, sizeof civic_date, "%04d-%02d-%02d", y + dy, m, d);
    };
    if (ImGui::SmallButton("<< year")) shift_year(-1);
    ImGui::SameLine();
    if (ImGui::SmallButton("year >>")) shift_year(1);
    ImGui::SameLine();
    if (ImGui::SmallButton("today")) civic_date[0] = 0;
    ImGui::SameLine();
    ImGui::TextDisabled("(nothing is stored per date - this is a projection)");

    // Recompute only when the date or the model moved. It is pure and cheap,
    // but re-deriving per frame would invite someone to cache it wrongly later.
    std::string want = std::string(civic_date) + "@" + std::to_string(cs.nodes.size()) +
                       ":" + std::to_string(cs.wires.size());
    if (civic_resolved_for != want) {
        civic_resolved = hormiga::civic::resolve_at(cs, civic_date);
        civic_resolved_for = want;
    }
    const size_t nconf = civic_resolved.merged.conflicts().size();

    // ── who held office, on that date ───────────────────────────────────────
    std::vector<hormiga::civic::Holder> hs = hormiga::civic::holders_on(cs, civic_date);
    if (!hs.empty()) {
        ImGui::Separator();
        for (size_t i = 0; i < hs.size(); ++i) {
            if (i) ImGui::SameLine();
            ImGui::TextDisabled("%s:", hs[i].seat.c_str());
            ImGui::SameLine(0, 4);
            if (ImGui::SmallButton(hs[i].contact.c_str())) ed.selection = {hs[i].contact};
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("who held this seat on %s - valid time, from the\n"
                              "term runes, NOT a replay of the command log",
                              civic_date);
    }
    ImGui::Separator();

    // ── left: policies ──────────────────────────────────────────────────────
    ImGui::BeginChild("civic-policies", ImVec2(200, 0), ImGuiChildFlags_Borders);
    ImGui::TextDisabled("policies");
    std::vector<const maiz::SceneNode*> policies;
    for (const auto& n : cs.nodes)
        if (n.glyph == "policy") policies.push_back(&n);
    if (civic_policy_sel.empty() && !policies.empty())
        civic_policy_sel = policies.front()->name;
    for (const auto* p : policies) {
        std::string label = hormiga::civic::field_of(*p, "title");
        if (label.empty()) label = p->name;
        if (ImGui::Selectable(label.c_str(), civic_policy_sel == p->name))
            civic_policy_sel = p->name;
        std::string cite = hormiga::civic::field_of(*p, "citation");
        if (!cite.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", cite.c_str());
        }
    }
    if (policies.empty()) ImGui::TextDisabled("none yet");
    if (nconf) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.55f, 0.30f, 1));
        ImGui::TextWrapped("%d unsettled", (int)nconf);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("provisions two assertions disagree about on this\n"
                              "date. The text reads EMPTY rather than guessing.");
        ImGui::Checkbox("show", &civic_show_conflicts);
    }
    ImGui::EndChild();

    // ── right: the provision tree, as of the date ───────────────────────────
    ImGui::SameLine();
    ImGui::BeginChild("civic-text", ImVec2(0, 0));
    if (civic_policy_sel.empty()) {
        ImGui::TextDisabled("select a policy");
        ImGui::EndChild();
        return;
    }

    // The tree is DRAWN here and stored nowhere: `children_of` derives it from
    // `part-of` links each frame. A provision with two parents appears under
    // both, which a tree could not represent and is why containment is a link.
    std::function<void(const std::string&, int)> walk = [&](const std::string& parent,
                                                            int depth) {
        if (depth > 8) return; // links may cycle; a data error, not a crash
        for (const maiz::SceneNode* n : hormiga::civic::children_of(cs, parent)) {
            std::string num = hormiga::civic::field_of(*n, "number");
            std::string head = hormiga::civic::field_of(*n, "heading");
            const maiz::MergedCell* cell =
                civic_resolved.merged.find(n->name, "text");
            bool conflicted = cell && cell->conflicted;
            std::string text = civic_resolved.merged.value(n->name, "text");

            ImGui::PushID(n->name.c_str());
            ImGui::Indent(depth * 14.0f);
            bool shared = hormiga::civic::containers_of(cs, n->name).size() > 1;
            if (conflicted)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.55f, 0.30f, 1));
            bool open = ImGui::TreeNodeEx(
                "##p", ImGuiTreeNodeFlags_SpanAvailWidth |
                           (civic_provision_sel == n->name ? ImGuiTreeNodeFlags_Selected : 0),
                "%s  %s%s", num.c_str(), head.c_str(), shared ? "  (shared)" : "");
            if (conflicted) ImGui::PopStyleColor();
            if (ImGui::IsItemClicked()) civic_provision_sel = n->name;
            if (shared && ImGui::IsItemHovered())
                ImGui::SetTooltip("this provision is part of more than one "
                                  "container -\na tree could not hold it; the graph "
                                  "does not notice");
            if (open) {
                if (conflicted) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.55f, 0.30f, 1));
                    ImGui::TextWrapped("UNSETTLED on %s - two assertions adopted the "
                                       "same day disagree. Nothing is shown, because "
                                       "showing one would be a guess.", civic_date);
                    ImGui::PopStyleColor();
                    for (const maiz::CellVerdict& v : maiz::explain_cell(*cell))
                        ImGui::BulletText("%s (%s): %s", v.source.c_str(),
                                          v.origin.c_str(), v.value.c_str());
                } else if (text.empty()) {
                    ImGui::TextDisabled("(not in force on %s)", civic_date);
                } else {
                    ImGui::TextWrapped("%s", text.c_str());
                }
                if (cell && !cell->conflicted && !cell->source.empty())
                    ImGui::TextDisabled("from %s, adopted %s", cell->source.c_str(),
                                        cell->origin.c_str());
                walk(n->name, depth + 1);
                ImGui::TreePop();
            }
            ImGui::Unindent(depth * 14.0f);
            ImGui::PopID();
        }
    };
    walk(civic_policy_sel, 0);

    // ── the selected provision's whole history, and the votes ───────────────
    if (!civic_provision_sel.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("every assertion about %s - the record, not the "
                            "projection", civic_provision_sel.c_str());
        for (const auto& n : cs.nodes) {
            if (n.glyph != "revision") continue;
            bool about = false;
            for (const auto& w : cs.wires)
                if (w.relation == "amends" && w.from == n.name &&
                    w.to == civic_provision_sel)
                    about = true;
            if (!about) continue;

            std::string from = hormiga::civic::field_of(n, "from");
            std::string until = hormiga::civic::field_of(n, "until");
            std::string sum = hormiga::civic::field_of(n, "summary");
            bool in_force = std::find(civic_resolved.in_force.begin(),
                                      civic_resolved.in_force.end(),
                                      n.name) != civic_resolved.in_force.end();
            ImGui::PushID(n.name.c_str());
            if (!in_force)
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::BulletText("%s%s  %s", from.c_str(),
                              until.empty() ? "" : (" \xe2\x80\x93 " + until).c_str(),
                              sum.c_str());
            if (!in_force) ImGui::PopStyleColor();
            if (ImGui::IsItemClicked()) ed.selection = {n.name};

            hormiga::civic::Tally t = hormiga::civic::tally_of(cs, n.name);
            if (t.yes || t.no || t.abstain || t.absent) {
                ImGui::SameLine();
                ImGui::TextDisabled("[%d-%d%s %s]", t.yes, t.no,
                                    t.abstain ? " abs" : "",
                                    t.carried() ? "carried" : "failed");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    for (const auto& w : cs.wires)
                        if (w.to == n.name && w.relation.rfind("voted", 0) == 0)
                            ImGui::Text("%s  %s", w.from.c_str(), w.relation.c_str());
                    ImGui::EndTooltip();
                }
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void HormigaApp::draw_allomone_body() {
    maiz::ProjectOptions po;
    po.mantle = kAlloMantle;
    maiz::Scene as;
    try { as = maiz::project_scene(core, po); } catch (...) {}

    const size_t nconf = allo_derived.merged.conflicts().size();
    ImGui::TextDisabled(
        "Rules are a SET, not a sequence - order changes nothing, the sharper "
        "rule wins, and nothing here edits your data.");
    ImGui::Text("%d subject(s)  -  %d source(s)  -  %d derived cell(s)",
                (int)allo_derived.subjects.size(), (int)allo_derived.sources.size(),
                (int)allo_derived.merged.cells.size());
    if (nconf) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1), "  -  %d unsettled",
                           (int)nconf);
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("allo-tabs")) {
        if (ImGui::BeginTabItem("Scripts")) {
            allo_tab = 0;
            draw_allomone_scripts_pane(as);
            ImGui::EndTabItem();
        }
        char clabel[48];
        std::snprintf(clabel, sizeof clabel, "Conflicts%s###allo-conf",
                      nconf ? (" (" + std::to_string(nconf) + ")").c_str() : "");
        if (ImGui::BeginTabItem(clabel)) {
            allo_tab = 1;
            draw_allomone_conflicts_pane();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Reference")) {
            allo_tab = 2;
            draw_allomone_reference_pane();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void HormigaApp::draw_allomone_scripts_pane(const maiz::Scene& as) {
    std::vector<const maiz::SceneNode*> scripts;
    for (const auto& n : as.nodes)
        if (n.glyph == hormiga::allomone::kScriptGlyph) scripts.push_back(&n);
    if (allo_src_sel.empty() ||
        std::none_of(scripts.begin(), scripts.end(),
                     [&](auto s) { return s->name == allo_src_sel; }))
        allo_src_sel = scripts.empty() ? "" : scripts.front()->name;

    auto info_for = [&](const std::string& id) -> const hormiga::allomone::SourceInfo* {
        for (const auto& s : allo_derived.scripts)
            if (s.id == id) return &s;
        return nullptr;
    };

    // ── left: the sources ───────────────────────────────────────────────────
    ImGui::BeginChild("allo2-list", ImVec2(230, 0), ImGuiChildFlags_Borders);
    if (ImGui::Button("+ New script", ImVec2(-1, 0))) {
        std::string nm;
        for (int i = 1;; ++i) {
            nm = "rules-" + std::to_string(i);
            if (!as.find(nm)) break;
        }
        allo_cmd(maiz::compile_commit(
            {std::string("rune new ") + hormiga::allomone::kScriptGlyph + " " + nm,
             "set " + nm + " title \"" + nm + "\"", "set " + nm + " enabled \"1\"",
             "setjson " + nm + " source " +
                 json_arg(nlohmann::json(hormiga::allomone::starter_script()).dump())}));
        allo_src_sel = nm;
    }
    ImGui::Separator();
    for (const auto* s : scripts) {
        ImGui::PushID(s->name.c_str());
        bool en = hormiga::temper::field_value(*s, "enabled") != "0";
        if (ImGui::Checkbox("##en", &en))
            allo_cmd("set " + s->name + " enabled \"" + (en ? "1" : "0") + "\"");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("enable / disable - disabling undoes nothing, "
                              "because nothing was ever written");
        ImGui::SameLine();
        const hormiga::allomone::SourceInfo* info = info_for(s->name);
        std::string nm = hormiga::temper::field_value(*s, "title");
        if (nm.empty()) nm = s->name;
        bool bad = info && !info->diagnostics.empty();
        if (bad) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.42f, 0.42f, 1));
        else if (!en)
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(nm.c_str(), allo_src_sel == s->name))
            allo_src_sel = s->name;
        if (bad || !en) ImGui::PopStyleColor();
        if (info && en && !bad) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 26);
            ImGui::TextDisabled("%d", info->cells);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%d cell(s) this source has an opinion about",
                                  info->cells);
        }
        ImGui::PopID();
    }
    if (scripts.empty())
        ImGui::TextDisabled("no scripts yet - press + New script");
    // ── legacy sources ──────────────────────────────────────────────────────
    // The old dialect's editor is unshipped (2026-08-11), so these are listed
    // and MANAGEABLE here — enable, disable, delete. Not editable: the language
    // is frozen and we are not shipping a second editor for it. But unshipping
    // an editor must not strand data, and a script you cannot see is worse than
    // one you cannot edit: it colours cards for reasons nothing on screen
    // explains.
    std::vector<const maiz::SceneNode*> legacy;
    for (const auto& n : as.nodes)
        if (n.glyph == "script") legacy.push_back(&n);
    if (!legacy.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("legacy dialect (%d)", (int)legacy.size());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Scripts in Hormiga's old language. They still derive, as "
                "sources like any other - but the language is frozen, so they "
                "can be turned off or removed, not rewritten.");
        for (const auto* s : legacy) {
            ImGui::PushID(s->name.c_str());
            bool en = hormiga::temper::field_value(*s, "enabled") != "0";
            if (ImGui::Checkbox("##len", &en))
                allo_cmd("set " + s->name + " enabled \"" + (en ? "1" : "0") + "\"");
            ImGui::SameLine();
            std::string nm = hormiga::temper::field_value(*s, "name");
            if (nm.empty()) nm = s->name;
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextUnformatted(nm.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                // The source, read-only, on hover — enough to decide whether to
                // keep it without shipping an editor to look at it.
                std::string body =
                    hormiga::temper::field_value(*s, "body");
                if (body.size() > 900) body = body.substr(0, 900) + "\n...";
                ImGui::SetTooltip("%s", body.c_str());
            }
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
            if (ImGui::SmallButton("x")) allo_cmd("rm " + s->name);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("delete this legacy script (undoable)");
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    // ── right: the staged editor ────────────────────────────────────────────
    ImGui::SameLine();
    ImGui::BeginChild("allo2-editor", ImVec2(0, 0));
    const maiz::SceneNode* sel = allo_src_sel.empty() ? nullptr : as.find(allo_src_sel);
    if (!sel || sel->glyph != hormiga::allomone::kScriptGlyph) {
        ImGui::TextDisabled("select a script on the left, or press + New script");
        ImGui::EndChild();
        return;
    }
    {
        static char nbuf[80];
        std::string nm = hormiga::temper::field_value(*sel, "title");
        std::snprintf(nbuf, sizeof nbuf, "%s", nm.empty() ? sel->name.c_str() : nm.c_str());
        ImGui::SetNextItemWidth(240);
        if (ImGui::InputTextWithHint("##stitle", "script name", nbuf, sizeof nbuf,
                                     ImGuiInputTextFlags_EnterReturnsTrue))
            allo_cmd("set " + sel->name + " title " +
                     json_arg(nlohmann::json(std::string(nbuf)).dump()));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 52);
        if (ImGui::SmallButton("Delete")) {
            allo_cmd("rm " + sel->name);
            allo_src_sel.clear();
            allo_src_for.clear();
            ImGui::EndChild();
            return;
        }
    }
    // ── the EDITOR. Upstream's, not ours (adopted 2026-08-11).
    //
    // The first cut of this tab drew a transparent ImGui InputTextMultiline
    // with a coloured overlay on top — the same trick the old tabs used, and
    // the same three problems: the overlay drifts out of alignment with the
    // real glyphs, there is no way to put an interactive item mid-text, and
    // every affordance beyond typing has to be rebuilt from nothing.
    //
    // `maiz::code_editor` is the from-scratch editor Void Maiz shipped WITH
    // Allomone, and it owns its layout, so it has what an overlay never could:
    // ctrl+wheel ZOOM with a level-of-detail switch (zoomed out, a script
    // becomes a readable shape — where the colours are, how the rules mass),
    // RIGHT-CLICK EXPLAIN on any word, HOVER, COMPLETION, and inline WIDGETS —
    // a colour literal is a colour wheel where it sits, editing the script
    // rather than a copy of it.
    //
    // All of that came free with the adoption and we were not taking it. The
    // five callbacks below are the only part that is ours, and they are where
    // our domain vocabulary stops being something a user has to memorize.
    if (allo_src_for != sel->name) { // reseat the staged buffer on selection
        allo_src_for = sel->name;
        allo_editor.set_text(hormiga::temper::field_value(*sel, "source"));
        allo_editor.clear_dirty();
    }
    maiz::Scene data;
    {
        maiz::ProjectOptions dp;
        dp.mantle = kDataMantle;
        try { data = maiz::project_scene(core, dp); } catch (...) {}
    }

    maiz::CodeEditorOptions eo;
    eo.registry = &allo_code_kit;

    // Colouring comes from UPSTREAM'S OWN LEXER rather than a second one here:
    // the token stream is what the parser consumes and what the editor
    // hit-tests, so anything else would drift from the language by
    // construction. A colour literal draws in the colour it names.
    eo.highlight = [](std::string_view src) {
        std::vector<maiz::CodeSpan> out;
        for (const maiz::Token& t : maiz::allo_tokens(src)) {
            unsigned rgb = 0xd0d0d0;
            switch (t.kind) {
            case maiz::TokenKind::Comment: rgb = 0x6a7079; break;
            case maiz::TokenKind::Keyword: rgb = 0x9696eb; break;
            case maiz::TokenKind::Arrow:   rgb = 0xff7b72; break;
            case maiz::TokenKind::Number:  rgb = 0x5ab9c0; break;
            case maiz::TokenKind::Ident:   rgb = 0x58b27a; break;
            case maiz::TokenKind::Invalid: rgb = 0xe66e6e; break;
            case maiz::TokenKind::String: {
                unsigned rgba = 0;
                // parse_hex returns IM_COL32 (ABGR); CodeSpan wants 0xrrggbb.
                rgb = hormiga::allomone::parse_hex(t.text, rgba)
                          ? (((rgba & 0xFFu) << 16) | (rgba & 0xFF00u) |
                             ((rgba >> 16) & 0xFFu))
                          : 0xce9150u;
                break;
            }
            default: continue;
            }
            out.push_back({t.begin, t.end, rgb});
        }
        return out;
    };
    // Inline widgets: a colour literal is a wheel, an ISO date is a month grid.
    // Same `kind → renderer` binding as a field editor, so the picker in a
    // script is the picker on a contact — one kit, many surfaces.
    eo.widgets = [](std::string_view src) {
        std::vector<maiz::CodeWidgetSpan> out;
        for (const maiz::Token& t : maiz::allo_tokens(src)) {
            if (t.kind != maiz::TokenKind::String) continue;
            unsigned rgba = 0;
            if (hormiga::allomone::parse_hex(t.text, rgba))
                out.push_back({t.begin, t.end, "color", t.text, true});
            else if (t.text.size() == 10 && t.text[4] == '-' && t.text[7] == '-')
                out.push_back({t.begin, t.end, "date", t.text, true});
        }
        return out;
    };
    // Hover answers "what does this line actually refer to?" without deriving
    // anything — `allo_matches` exists upstream for exactly this question.
    eo.hover = [&](std::string_view src, size_t off) -> std::string {
        int line = 0;
        for (size_t i = 0; i < off && i < src.size(); ++i)
            if (src[i] == '\n') ++line;
        maiz::Script parsed = maiz::allo_parse("preview", src);
        for (const maiz::Rule& r : parsed.rules) {
            if (r.line != line) continue;
            maiz::PredicateRegistry preds =
                hormiga::allomone::predicates(allo_derived.frame);
            int n = 0;
            for (const maiz::Subject& sub : allo_derived.subjects)
                if (maiz::allo_matches(r, sub, nullptr, &preds)) ++n;
            return std::to_string(n) + " subject" + (n == 1 ? "" : "s") +
                   " match this rule  (strength " + std::to_string(r.strength()) + ")";
        }
        return {};
    };
    // Right-click explain. Completion tells you a word EXISTS; this tells you
    // what it does. Our vocabulary is 9 kernel conditions, 22 predicates and 8
    // properties across 4 surfaces — nobody holds that in their head, so every
    // part of it has to be reachable from the word itself. The text lives in
    // the domain library, next to the tables it describes.
    eo.explain = [](std::string_view src, size_t off) -> std::string {
        for (const maiz::Token& t : maiz::allo_tokens(src)) {
            if (t.kind == maiz::TokenKind::End) break;
            if (off < t.begin || off > t.end) continue;
            std::string w = hormiga::allomone::explain_word(t.text);
            if (!w.empty()) return w;
            unsigned rgba = 0;
            if (t.kind == maiz::TokenKind::String &&
                hormiga::allomone::parse_hex(t.text, rgba))
                return t.text +
                       " — a colour. Click it in the text to open the wheel;\n"
                       "the edit goes into the script, not into a copy of it.";
            break;
        }
        return {};
    };
    // ── completion: the vocabulary, offered rather than memorized ───────────
    // With the real org's tags, glyphs and rune names, plus 22 predicates and
    // every property across four surfaces, nobody can hold this in their head.
    // The app knows all of it, so the app offers it — with counts, because a
    // tag on two runes and a tag on two hundred should not look the same.
    eo.complete = [&](std::string_view src, size_t caret) -> maiz::CompletionSet {
        maiz::CompletionSet cs;
        // The token stream must be NAMED. Iterating `maiz::allo_tokens(src)`
        // directly and keeping pointers into it leaves them dangling the moment
        // the range-for ends — the temporary's lifetime covers the loop and not
        // one line further. Written that way once here; it compiled, ran, and
        // would have failed whenever the allocator felt like it.
        const std::vector<maiz::Token> toks = maiz::allo_tokens(src);
        const maiz::Token *cur = nullptr, *prev = nullptr;
        for (const maiz::Token& t : toks) {
            if (t.kind == maiz::TokenKind::End) break;
            if (caret >= t.begin && caret <= t.end) { cur = &t; break; }
            if (t.end <= caret) prev = &t;
        }
        if (cur && cur->kind == maiz::TokenKind::Comment) return cs;
        const maiz::TokenKind cur_kind = cur ? cur->kind : maiz::TokenKind::End;
        const std::string cur_text = cur ? cur->text : std::string();
        const size_t cur_begin = cur ? cur->begin : caret;
        const size_t cur_end = cur ? cur->end : caret;
        const std::string before = prev ? prev->text : "";
        const bool prev_arrow = prev && prev->kind == maiz::TokenKind::Arrow;

        auto add = [&](std::string text, std::string detail) {
            cs.items.push_back({std::move(text), "", std::move(detail)});
        };
        auto keep_prefix = [&](const std::string& pre) {
            if (pre.empty()) return;
            cs.items.erase(
                std::remove_if(cs.items.begin(), cs.items.end(),
                               [&](const maiz::Completion& c) {
                                   std::string t = c.text;
                                   if (!t.empty() && t.front() == '"') t.erase(0, 1);
                                   return t.size() < pre.size() ||
                                          t.compare(0, pre.size(), pre) != 0;
                               }),
                cs.items.end());
        };
        auto plural = [](int n) {
            return std::to_string(n) + (n == 1 ? " rune" : " runes");
        };
        std::map<std::string, int> tag_counts;
        for (const auto& n : data.nodes)
            if (hormiga::allomone::is_subject_glyph(n.glyph))
                for (const auto& t : n.tags) ++tag_counts[t];

        // The real values for a condition word, quoted or bare as the position
        // needs. This is the part a generic editor cannot do for us.
        auto values_for = [&](const std::string& kw, bool quote) {
            auto q = [&](const std::string& v, const std::string& d) {
                add(quote ? "\"" + v + "\"" : v, d);
            };
            if (kw == "tag" || kw == "has") {
                for (const auto& [t, n] : tag_counts) q(t, plural(n));
            } else if (kw == "under") {
                std::set<std::string> ns; // the NAMESPACES — the point of `under`
                for (const auto& [t, n] : tag_counts) {
                    (void)n;
                    size_t c = t.find(':');
                    if (c != std::string::npos) ns.insert(t.substr(0, c + 1));
                }
                for (const auto& x : ns) q(x, "tag namespace");
            } else if (kw == "kind" || kw == "glyph") {
                std::map<std::string, int> c;
                for (const auto& n : data.nodes)
                    if (hormiga::allomone::is_subject_glyph(n.glyph)) ++c[n.glyph];
                for (const auto& [g, n] : c) q(g, plural(n));
            } else if (kw == "name" || kw == "rune" || kw == "linked-to" ||
                       kw == "near-rune") {
                int n = 0;
                for (const auto& x : data.nodes) {
                    if (!hormiga::allomone::is_subject_glyph(x.glyph)) continue;
                    if (++n > 200) break; // a picker, not a dump
                    q(x.name + (kw == "near-rune" ? ",5" : ""), x.glyph);
                }
            } else if (kw == "mantle") {
                q(kDataMantle, "the data mantle");
            } else if (kw == "linked") {
                std::set<std::string> rels;
                for (const auto& w : data.wires)
                    if (!w.relation.empty()) rels.insert(w.relation);
                for (const auto& r : rels) q(r, "relation");
            } else if (kw == "role") {
                std::set<std::string> roles;
                for (const auto& n : data.nodes) {
                    std::string r = hormiga::temper::field_value(n, "role");
                    if (!r.empty()) roles.insert(r);
                }
                for (const auto& r : roles) q(r, "role");
            } else if (kw == "field" || kw == "field-has") {
                std::set<std::string> keys;
                for (const auto& n : data.nodes)
                    if (hormiga::allomone::is_subject_glyph(n.glyph))
                        for (const auto& f : n.fields)
                            if (!hormiga::allomone::is_internal_field(f.key))
                                keys.insert(f.key); // internal keys are NOT offered
                for (const auto& k : keys)
                    q(k + (kw == "field-has" ? "=" : ""), "field");
            } else {
                for (const auto& pd : hormiga::allomone::predicate_docs())
                    if (kw == pd.name) q("", std::string("e.g. ") + pd.arg);
            }
        };

        bool in_string = cur && (cur_kind == maiz::TokenKind::String ||
                                 cur_kind == maiz::TokenKind::Invalid) &&
                         caret > cur_begin;
        if (in_string) {
            cs.replace_begin = cur_begin + 1;
            cs.replace_end = (cur_kind == maiz::TokenKind::String &&
                              cur_end > cur_begin + 1)
                                 ? cur_end - 1
                                 : cur_end;
            values_for(before, false);
            keep_prefix(cur_text);
            return cs;
        }
        std::string typed =
            (cur && cur_kind == maiz::TokenKind::Ident) ? cur_text : std::string();
        if (cur && cur_kind != maiz::TokenKind::String) {
            cs.replace_begin = cur_begin;
            cs.replace_end = cur_end;
        } else {
            cs.replace_begin = cs.replace_end = caret;
        }
        // Just after a condition word the next thing is always a quoted
        // argument, so offer real ones, already quoted and ready to accept.
        bool is_pred = false;
        for (const auto& pd : hormiga::allomone::predicate_docs())
            if (before == pd.name) is_pred = true;
        bool cond_kw = before == "tag" || before == "has" || before == "kind" ||
                       before == "glyph" || before == "name" || before == "rune" ||
                       before == "mantle" || before == "with" ||
                       before == "device" || is_pred;
        if (cond_kw && cur_kind != maiz::TokenKind::String) {
            values_for(before, true);
            keep_prefix(typed);
            return cs;
        }

        size_t line_start = src.rfind('\n', caret ? caret - 1 : 0);
        line_start = (line_start == std::string_view::npos) ? 0 : line_start + 1;
        std::string_view line_so_far = src.substr(line_start, caret - line_start);
        bool has_when = line_so_far.find("when") != std::string_view::npos;
        bool has_define = line_so_far.find("define") != std::string_view::npos;
        bool after_then = prev_arrow || before == "then" || before == ",";
        if (after_then) {
            // EFFECT position. Properties are DISCOVERED, not enumerated — a
            // property exists because some rule writes it — so the list is
            // every property currently in use, plus the ones we declare a law
            // for, plus their surface-qualified spellings. What cannot be
            // discovered is what a property MEANS, which is the detail column.
            std::map<std::string, std::string> props;
            for (const auto& p : hormiga::allomone::vocabulary())
                props[p.name] = std::string(maiz::lattice_name(p.law)) +
                                " - every surface";
            for (const auto& d : hormiga::allomone::domains())
                for (const auto& p : hormiga::allomone::vocabulary())
                    props[std::string(d.prefix) + "-" + p.name] =
                        std::string(maiz::lattice_name(p.law)) + " - the " +
                        d.prefix + " surface only";
            for (const auto& s : allo_derived.sources)
                for (const auto& c : s.cells)
                    if (!props.count(c.property)) props[c.property] = "unique, in use";
            for (const auto& [name, detail] : props) add(name, detail);
        } else if (!has_when && !has_define) {
            add("when", "start a rule");
            add("define", "name a condition, to reuse it");
        } else if (before == "when" || before == "and" || before == "not" ||
                   before == "=" || !cur) {
            for (const char* k : {"has", "glyph", "rune"})
                add(k, "condition (Void Core's spelling)");
            for (const char* k : {"tag", "kind", "name", "mantle"})
                add(k, "condition");
            if (before != "not") {
                add("all", "match everything (strength 0)");
                add("not", "negate one term");
            }
            for (const auto& pd : hormiga::allomone::predicate_docs())
                add(pd.name, std::string(pd.meaning).substr(0, 54));
            // Definitions from THIS script's buffer — script-local on purpose.
            for (const maiz::Definition& d : maiz::allo_parse("", src).definitions)
                if (d.usable)
                    add(d.name + "(", "define, " +
                                          std::to_string(d.expanded.size()) +
                                          " term(s)");
        } else {
            add("then", "begin effects");
            add("and", "another condition");
        }
        keep_prefix(typed);
        return cs;
    };

    ImGui::TextDisabled("ctrl+space completes  |  right-click explains  |  "
                        "ctrl+wheel zooms (%.0f%%)  |  ctrl+enter saves",
                        allo_editor.zoom * 100.0f);
    const hormiga::allomone::SourceInfo* info = info_for(sel->name);
    float reserve = 30.0f +
                    (info ? std::min<float>(3, (float)info->diagnostics.size()) * 18.0f
                          : 0.0f);
    ImFont* mono = mono_font ? mono_font : ImGui::GetFont();
    ImGui::PushFont(mono);
    maiz::CodeEditorIO io = maiz::code_editor(
        "allo-src", allo_editor, eo, 0,
        std::max(80.0f, ImGui::GetContentRegionAvail().y - reserve));
    ImGui::PopFont();
    // ONE command per gesture, on commit — never per keystroke. Getting this
    // granularity wrong is indistinguishable from the feature being broken.
    if (io.commit) {
        allo_cmd("setjson " + sel->name + " source " +
                 json_arg(nlohmann::json(allo_editor.text).dump()));
        allo_editor.clear_dirty();
    }

    // Status. A parse error and an unknown predicate are DIFFERENT things and
    // are reported as such: the first means the line was skipped, the second
    // means the rule parsed fine and simply never matches here.
    ImGui::Separator();
    if (allo_editor.dirty) {
        ImGui::TextDisabled("unsaved - ctrl+enter, or click away");
    } else if (info && !info->diagnostics.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.42f, 0.42f, 1));
        for (size_t i = 0; i < info->diagnostics.size() && i < 3; ++i)
            ImGui::TextWrapped("! line %d: %s", info->diagnostics[i].line + 1,
                               info->diagnostics[i].message.c_str());
        ImGui::PopStyleColor();
    } else if (info && !info->enabled) {
        ImGui::TextDisabled("%d rule(s), parses OK - enable it on the left to apply",
                            info->rules);
    } else if (info) {
        ImGui::Text("%d rule(s) - an opinion about %d cell(s)", info->rules, info->cells);
    } else {
        ImGui::TextDisabled("not derived yet");
    }
    ImGui::EndChild();
}

// ── the ⊤ inspector ─────────────────────────────────────────────────────────
// A conflict is not an error; it is the engine correctly refusing to decide
// something two people disagree about. This pane is where a human decides it,
// and settling is a dispatcher command — logged, attributed, replayable and
// undoable like every other gesture.
void HormigaApp::draw_allomone_conflicts_pane() {
    std::vector<const maiz::MergedCell*> conflicts = allo_derived.merged.conflicts();
    auto settle = [&](const maiz::Resolution& r) {
        std::string prev = scene.mantle;
        for (const std::string& c : maiz::compile_resolution(kAlloMantle, r))
            pending_cmds.push_back(c); // the first is already `use allomone`
        if (!prev.empty() && prev != kAlloMantle) pending_cmds.push_back("use " + prev);
    };

    if (conflicts.empty()) {
        ImGui::TextDisabled(
            "Nothing is contested. A conflict appears when two DIFFERENT "
            "sources state different things at equal strength - two rules "
            "inside one script never conflict, because the sharper one winning "
            "is the point of writing rules.");
    }
    for (const maiz::MergedCell* c : conflicts) {
        ImGui::PushID((c->subject + "\t" + c->property).c_str());
        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.25f, 1), "%s . %s",
                           c->subject.c_str(), c->property.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("- unsettled, so nothing is drawn for it");
        std::vector<maiz::CellVerdict> vs = maiz::explain_cell(*c);
        if (ImGui::BeginTable("cands", 4,
                              ImGuiTableFlags_SizingStretchProp |
                                  ImGuiTableFlags_RowBg)) {
            for (const maiz::CellVerdict& v : vs) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(v.source.c_str());
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", v.origin.c_str());
                ImGui::TableNextColumn();
                unsigned rgba = 0;
                if (hormiga::allomone::parse_hex(v.value, rgba)) {
                    ImGui::ColorButton("##sw", ImGui::ColorConvertU32ToFloat4(rgba),
                                       ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
                    ImGui::SameLine();
                }
                ImGui::TextUnformatted(v.value.c_str());
                ImGui::TableNextColumn();
                ImGui::PushID(v.source.c_str());
                if (ImGui::SmallButton("this one wins"))
                    settle({c->subject, c->property, v.source, ""});
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("records a resolution rune - logged and undoable");
                ImGui::SameLine();
                if (ImGui::SmallButton("...always"))
                    settle({"", c->property, v.source, ""}); // every subject
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s wins `%s` for EVERY subject", v.source.c_str(),
                                      c->property.c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::PopID();
    }

    // The rulings already made — model content, so removable like any rune.
    maiz::ProjectOptions po;
    po.mantle = kAlloMantle;
    maiz::Scene as;
    try { as = maiz::project_scene(core, po); } catch (...) {}
    bool any = false;
    for (const auto& n : as.nodes)
        if (n.glyph == "allomone-resolution") any = true;
    if (!any) return;
    ImGui::Spacing();
    ImGui::SeparatorText("Settled by hand");
    for (const auto& n : as.nodes) {
        if (n.glyph != "allomone-resolution") continue;
        std::string subj = hormiga::temper::field_value(n, "subject");
        ImGui::PushID(n.name.c_str());
        ImGui::BulletText("%s . %s -> %s", subj.empty() ? "(every subject)" : subj.c_str(),
                          hormiga::temper::field_value(n, "property").c_str(),
                          hormiga::temper::field_value(n, "winner").c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("undo this")) allo_cmd("rm " + n.name);
        ImGui::PopID();
    }
}

// ── the reference pane ──────────────────────────────────────────────────────
// The merge laws are the real surface area of a domain language — more than
// the grammar, which is fixed upstream. Someone writing a rule has to know
// that `weight` accumulates while `color` does not, so it is documented HERE,
// generated from the same table merge_options() is built from and therefore
// incapable of going stale.
void HormigaApp::draw_allomone_reference_pane() {
    ImGui::BeginChild("allo2-ref");
    ImGui::SeparatorText("The shape of a rule");
    ImGui::TextWrapped(
        "when <condition> then <property> <value>, <property> <value>\n"
        "Conditions join with `and`; `not` negates one term; `all` matches "
        "everything. There is no `or` (write two rules - they then carry "
        "separate strength, which is more informative) and no `else`.");
    ImGui::TextWrapped(
        "A rule's STRENGTH is how many terms it has. Inside one script the "
        "sharper rule wins silently - that is the defaults-and-exceptions "
        "idiom. ACROSS scripts, equal strength and different values is a real "
        "question, so it becomes a conflict for you to settle.");
    ImGui::TextWrapped("define shorthand(t) = has t and has \"urgent\"   names a "
                       "condition; a call counts its expanded terms toward strength.");

    ImGui::SeparatorText("Conditions the language knows");
    if (ImGui::BeginTable("kernel", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        struct K { const char* t; const char* m; };
        static const K ks[] = {
            {"has \"x\"  /  tag \"x\"", "carries exactly that tag"},
            {"glyph \"contact\"  /  kind", "is of that glyph"},
            {"rune \"ada\"  /  name", "is that rune, by name"},
            {"mantle \"data\"", "lives in that mantle"},
            {"with / device", "read the PERSON, not the data - no user graph "
                              "here yet, so they never match"},
        };
        for (const K& k : ks) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k.t);
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", k.m);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Conditions Hormiga adds");
    ImGui::TextDisabled("dates, geography, roles and the graph - the things "
                        "tags encode badly");
    if (ImGui::BeginTable("preds", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        for (const auto& p : hormiga::allomone::predicate_docs()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s %s", p.name, p.arg);
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", p.meaning);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Surfaces");
    ImGui::TextWrapped(
        "A property with no prefix reaches every surface. Prefix it with a "
        "surface and only that one listens - `map-color \"#ff0000\"` recolours "
        "the marker and leaves the card alone. The law is the same either way, "
        "so `map-weight` accumulates exactly like `weight`.");
    if (ImGui::BeginTable("surfaces", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        for (const auto& d : hormiga::allomone::domains()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s-", d.prefix);
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", d.what);
        }
        ImGui::EndTable();
    }
    ImGui::TextWrapped(
        "`web-hide \"1\"` is the output domain's one effect: the rune drops out "
        "of every query-backed block, in the newsletter, on the site, and in "
        "the preview. A CONTESTED hide does not hide - two scripts disagreeing "
        "about whether something may be published is a question for you, and "
        "silently suppressing content nobody agreed to suppress just looks "
        "like the export is broken.");

    ImGui::SeparatorText("What a rule may never read");
    ImGui::TextWrapped(
        "Internal-notes fields are invisible to every condition - not even "
        "testable for equality, because repeated equality tests are a way to "
        "reproduce a value. The one thing a rule may learn is `internal \"\"`: "
        "that this rune HAS notes. That is exactly enough to keep it out of an "
        "export and never enough to leak one, and naming such a field in a "
        "rule tells you so rather than quietly matching nothing.");
    {
        std::string fields;
        for (const auto& f : hormiga::allomone::internal_fields())
            fields += (fields.empty() ? "" : ", ") + f;
        ImGui::TextDisabled("today: %s", fields.c_str());
    }

    ImGui::SeparatorText("Properties, and how two opinions combine");
    if (ImGui::BeginTable("laws", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        for (const auto& p : hormiga::allomone::vocabulary()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(std::string(maiz::lattice_name(p.law)).c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", p.meaning);
        }
        ImGui::EndTable();
    }
    ImGui::TextWrapped(
        "Any property you write that is not listed is `unique`: one right "
        "answer, and disagreement surfaces. Deriving a property never writes "
        "the rune's field - disabling the script restores it with nothing to "
        "undo.");

    ImGui::SeparatorText("Who is actually doing the work");
    for (const auto& si : maiz::influence(allo_derived.merged)) {
        ImGui::Text("%-24s won %3d of %3d  (%.0f%%)", si.source.c_str(), si.cells_won,
                    si.cells_offered, si.share * 100.0);
    }
    ImGui::EndChild();
}

/* ════════════════════════════════════════════════════════════════════════════
 * UNSHIPPED 2026-08-11 — the legacy Allomone tab and the Allo Dev editor.
 *
 * Both windows are gone from the application. Allomone is Void Maiz's language
 * now, the new tab is the only editor, and shipping two more surfaces for a
 * dialect nobody should write in was teaching the wrong thing to anyone who
 * opened the View menu.
 *
 * THE CODE IS KEPT, NOT DELETED, and kept COMPILING-ON-DEMAND rather than
 * rotting in a comment: build with -DHORMIGA_LEGACY_ALLOMONE=1 and both
 * windows come back. Two reasons it is worth the flag —
 *
 *   1. `code_editor` is ~370 lines of from-scratch text editing (own buffer,
 *      caret, selection, clipboard, inline widgets) that the new tab does not
 *      use yet but the inline-widget work will want. Deleting it would be
 *      throwing away the only part of Allo Dev that was ever about the editor
 *      rather than the language.
 *   2. If a database turns up with a legacy script nobody can read, one flag
 *      brings the editor back rather than a git archaeology session.
 *
 * What did NOT go away: the legacy INTERPRETER still derives
 * (src/allomone_legacy.hpp, allo_legacy_sources) so an existing database's
 * enabled `script` runes keep colouring their cards, and the new tab can
 * enable, disable and delete them. Unshipping an editor must not strand data.
 * ════════════════════════════════════════════════════════════════════════════ */
#if HORMIGA_LEGACY_ALLOMONE

// ── the LEGACY dialect's syntax highlighter ─────────────────────────────────
// Syntax highlighting: draw one monospace line of Allomone Script, token-colored
// — comments / strings / hex colors / keywords / tags / numbers, everything else
// in the default text color. Column c sits at x + c*cw. Used by the live
// highlighted preview (a dedicated colored EDITOR is the planned next step).
static void allo_draw_line(ImDrawList* dl, ImVec2 p, float cw,
                           const std::string& s) {
    static const std::set<std::string> kw = {
        "rule", "when",  "color",    "highlight", "icon",  "emphasis", "size",
        "if",   "elseif","else",     "end",       "for",   "each",     "where",
        "do",   "local", "and",      "or",        "not",   "is",       "count",
        "matching","contains","function","then",  "true",  "false",    "rune",
        "runes","glyph", "return",   "in",        "has",   "len",      "head",
        "tail", "push",  "abs",      "floor",     "min",   "max",      "lower",
        "upper","tags_of","union",   "intersect", "minus", "overlaps", "any", "all",
        "neighbours","neighbors","linked","cluster","degree",
        "centrality","community","distance","within"};
    const ImU32 C_def = ImGui::GetColorU32(ImGuiCol_Text),
                C_cmt = IM_COL32(122, 150, 120, 255),
                C_str = IM_COL32(206, 145, 80, 255),
                C_kw = IM_COL32(150, 150, 235, 255),
                C_tag = IM_COL32(88, 178, 122, 255),
                C_num = IM_COL32(90, 185, 192, 255);
    auto put = [&](size_t col, const std::string& t, ImU32 c) {
        dl->AddText(ImVec2(p.x + col * cw, p.y), c, t.c_str());
    };
    size_t i = 0, n = s.size();
    while (i < n) {
        char c = s[i];
        if (c == '-' && i + 1 < n && s[i + 1] == '-') { put(i, s.substr(i), C_cmt); return; }
        if (c == '"') {
            size_t j = i + 1;
            while (j < n && s[j] != '"') ++j;
            if (j < n) ++j;
            put(i, s.substr(i, j - i), C_str);
            i = j;
            continue;
        }
        if (c == '#' && i + 1 < n && std::isxdigit((unsigned char)s[i + 1])) {
            size_t j = i + 1;
            while (j < n && std::isxdigit((unsigned char)s[j])) ++j;
            std::string t = s.substr(i, j - i);
            ImU32 cc = C_num;
            unsigned rr, gg, bb;
            if (t.size() == 7 &&
                std::sscanf(t.c_str(), "#%02x%02x%02x", &rr, &gg, &bb) == 3)
                cc = IM_COL32(rr, gg, bb, 255);
            put(i, t, cc);
            i = j;
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < n &&
                   (std::isalnum((unsigned char)s[j]) || s[j] == '_' || s[j] == ':'))
                ++j;
            std::string t = s.substr(i, j - i);
            if (t.find(':') != std::string::npos) put(i, t, C_tag);
            else if (kw.count(t)) put(i, t, C_kw);
            else put(i, t, C_def); // identifier
            i = j;
            continue;
        }
        if (std::isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < n && (std::isdigit((unsigned char)s[j]) || s[j] == '.')) ++j;
            put(i, s.substr(i, j - i), C_num);
            i = j;
            continue;
        }
        put(i, std::string(1, c), C_def); // punctuation / space
        ++i;
    }
}

// ── the ALLOMONE SCRIPT IDE (text pivot, 2026-08-04): a document editor for
// Allomone Scripts. Left = the script list with enable toggles; right = a plain
// text editor (copy/paste/cut/undo are native to ImGui). Each enabled script is
// parsed (allo_parse) into derive-only rules the engine evaluates — the effect
// shows live in Data > Cards. Every edit is a dispatcher command. ─────────────
// ── the LEGACY Allomone tab (frozen 2026-08-10) ─────────────────────────────
// The original Hormiga-local dialect: imperative, `for each rune do …
// rune:color(…)`. It still parses and still derives — as one producer feeding
// the same merge as the Void Maiz scripts (allo_legacy_sources) — so the
// twelve seeded example scripts keep working and nobody's stored script broke
// on the day the language moved upstream. Hidden by default; no new work goes
// here. See src/allomone_legacy.hpp for the terms of the freeze.
void HormigaApp::draw_allomone_legacy_body() {
    ImGui::TextDisabled("LEGACY dialect (frozen). These `script` runes still "
                        "derive, as one source among several - but new rules "
                        "belong in the Allomone tab.");
    ImGui::SameLine();
    ImGui::TextDisabled("  [styling %d thing(s)]", (int)allo_colors.size());
    ImGui::Separator();

    maiz::ProjectOptions po;
    po.mantle = kAlloMantle;
    maiz::Scene as;
    try { as = maiz::project_scene(core, po); } catch (...) {}

    std::vector<const maiz::SceneNode*> scripts;
    for (const auto& n : as.nodes)
        if (n.glyph == "script") scripts.push_back(&n);
    if (allo_sel.empty() ||
        std::none_of(scripts.begin(), scripts.end(),
                     [&](auto s) { return s->name == allo_sel; }))
        allo_sel = scripts.empty() ? "" : scripts.front()->name;

    // ── left: the script list (enable toggles) ───────────────────────────────
    ImGui::BeginChild("allo-scripts", ImVec2(210, 0), ImGuiChildFlags_Borders);
    if (ImGui::Button("+ New script", ImVec2(-1, 0))) {
        std::string nm;
        for (int i = 1;; ++i) {
            nm = "script-" + std::to_string(i);
            if (!as.find(nm)) break;
        }
        std::string tmpl =
            "-- a new Allomone Script (derive-only for now)\n\n"
            "for each rune do\n"
            "  if rune has \"type:contact\" then\n"
            "    rune:color(\"#2e8b57\")\n"
            "  end\n"
            "end\n";
        allo_cmd(maiz::compile_commit(
            {"rune new script " + nm, "set " + nm + " name \"" + nm + "\"",
             "set " + nm + " enabled \"1\"",
             "setjson " + nm + " body " + json_arg(nlohmann::json(tmpl).dump())}));
        allo_sel = nm;
    }
    ImGui::Separator();
    for (const auto* s : scripts) {
        ImGui::PushID(s->name.c_str());
        bool en = hormiga::temper::field_value(*s, "enabled") != "0";
        if (ImGui::Checkbox("##en", &en))
            allo_cmd("set " + s->name + " enabled \"" + (en ? "1" : "0") + "\"");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("enable / disable");
        ImGui::SameLine();
        std::string nm = hormiga::temper::field_value(*s, "name");
        if (nm.empty()) nm = s->name;
        if (!en)
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(nm.c_str(), allo_sel == s->name)) allo_sel = s->name;
        if (!en) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (scripts.empty()) ImGui::TextDisabled("no scripts yet - press + New script");
    ImGui::EndChild();

    // ── right: the text editor ───────────────────────────────────────────────
    ImGui::SameLine();
    ImGui::BeginChild("allo-editor", ImVec2(0, 0));
    const maiz::SceneNode* sel = allo_sel.empty() ? nullptr : as.find(allo_sel);
    if (!sel || sel->glyph != "script") {
        ImGui::TextDisabled("select a script on the left, or press + New script");
        ImGui::EndChild();
        return;
    }
    { // name + delete
        static char nbuf[80];
        std::string nm = hormiga::temper::field_value(*sel, "name");
        std::snprintf(nbuf, sizeof nbuf, "%s",
                      nm.empty() ? sel->name.c_str() : nm.c_str());
        ImGui::SetNextItemWidth(240);
        if (ImGui::InputTextWithHint("##sname", "script name", nbuf, sizeof nbuf,
                                     ImGuiInputTextFlags_EnterReturnsTrue))
            allo_cmd("set " + sel->name + " name " +
                     json_arg(nlohmann::json(std::string(nbuf)).dump()));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 52);
        if (ImGui::SmallButton("Delete")) {
            allo_cmd("rm " + sel->name);
            allo_sel.clear();
            allo_script_for.clear();
            ImGui::EndChild();
            return;
        }
    }
    { // #3: scripts are data — tag them (commands route through the allomone mantle)
        std::vector<std::string> tcmds;
        draw_tag_editor(*sel, tcmds);
        for (const auto& c : tcmds) allo_cmd(c);
        ImGui::Separator();
    }
    // the source editor (staged; commit on blur). Copy/paste/cut/undo native.
    if (allo_script_for != sel->name) {
        allo_script_for = sel->name;
        std::snprintf(allo_script_buf, sizeof allo_script_buf, "%s",
                      hormiga::temper::field_value(*sel, "body").c_str());
    }

    // ── INLINE COLOR WIDGET (first rung of "widgets in the editor"): a toolbar
    // row of swatches, one per "#rrggbb" in the script; clicking one pops a
    // color wheel that live-edits that hex everywhere it appears. Safe over the
    // native editor — clicking a swatch deactivates the InputText, so the buffer
    // edit shows next frame. The truly IN-THE-TEXT clickable version awaits the
    // custom editor (okf/concepts/allomone/editor.md).
    {
        auto is_hex = [](const char* s) {
            for (int i = 0; i < 6; ++i)
                if (!std::isxdigit((unsigned char)s[i])) return false;
            return true;
        };
        std::vector<std::string> hexes; // distinct, in order
        std::string s(allo_script_buf);
        for (size_t i = 0; i + 6 < s.size();) {
            if (s[i] == '#' && is_hex(s.c_str() + i + 1)) {
                std::string h = s.substr(i, 7);
                if (std::find(hexes.begin(), hexes.end(), h) == hexes.end())
                    hexes.push_back(h);
                i += 7;
            } else ++i;
        }
        if (!hexes.empty()) {
            ImGui::TextDisabled("colors:");
            ImGui::SameLine();
            for (const auto& h : hexes) {
                unsigned rr = 0, gg = 0, bb = 0;
                std::sscanf(h.c_str(), "#%02x%02x%02x", &rr, &gg, &bb);
                ImVec4 col(rr / 255.0f, gg / 255.0f, bb / 255.0f, 1.0f);
                ImGui::PushID(h.c_str());
                if (ImGui::ColorButton(h.c_str(), col,
                                       ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18))) {
                    allo_color_edit = h;
                    allo_pick[0] = col.x; allo_pick[1] = col.y; allo_pick[2] = col.z;
                    ImGui::OpenPopup("allo-colorwheel");
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s - click to edit", h.c_str());
                ImGui::PopID();
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        if (ImGui::BeginPopup("allo-colorwheel")) { // shared wheel + Apply
            ImGui::ColorPicker3("##wheel", allo_pick,
                                ImGuiColorEditFlags_NoSidePreview |
                                    ImGuiColorEditFlags_NoInputs);
            if (ImGui::Button("Apply") && !allo_color_edit.empty()) {
                char nh[8];
                std::snprintf(nh, sizeof nh, "#%02x%02x%02x",
                              (int)(allo_pick[0] * 255 + 0.5f),
                              (int)(allo_pick[1] * 255 + 0.5f),
                              (int)(allo_pick[2] * 255 + 0.5f));
                std::string body(allo_script_buf), rep;
                for (size_t p = 0;;) { // replace every occurrence (same length)
                    size_t q = body.find(allo_color_edit, p);
                    if (q == std::string::npos) { rep += body.substr(p); break; }
                    rep += body.substr(p, q - p) + nh;
                    p = q + allo_color_edit.size();
                }
                std::snprintf(allo_script_buf, sizeof allo_script_buf, "%s", rep.c_str());
                allo_cmd("setjson " + sel->name + " body " +
                         json_arg(nlohmann::json(rep).dump()));
                allo_color_edit.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // ── the COMBINED syntax-highlighted editor (#5): the editable box renders
    // its text TRANSPARENT (so per-token color isn't fought), a colored overlay
    // is drawn on top (grid-aligned via the mono font), and a manual cursor is
    // drawn from the InputText state — which also gives us the scroll offset, so
    // the overlay stays in sync while scrolling. Copy/paste/cut/undo stay native.
    ImFont* mono = mono_font ? mono_font : ImGui::GetFont();
    float avail = ImGui::GetContentRegionAvail().y - 30; // room for status
    ImGui::PushFont(mono);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0)); // hide the raw glyphs
    ImGui::InputTextMultiline("##src", allo_script_buf, sizeof allo_script_buf,
                              ImVec2(-1, std::max(80.0f, avail)),
                              ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopStyleColor();
    bool commit = ImGui::IsItemDeactivatedAfterEdit();
    ImVec2 bmin = ImGui::GetItemRectMin(), bmax = ImGui::GetItemRectMax();
    ImGuiID iid = ImGui::GetItemID();
    if (commit) {
        std::string js = nlohmann::json(std::string(allo_script_buf)).dump();
        allo_cmd("setjson " + sel->name + " body " + json_arg(js));
    }
    { // overlay: colored tokens + manual cursor, scroll-synced from input state
        ImVec2 scroll(0, 0);
        int cpos = -1;
        if (ImGuiInputTextState* st = ImGui::GetInputTextState(iid)) {
            scroll = st->Scroll;
            cpos = st->GetCursorPos();
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cw = ImGui::CalcTextSize("0").x, lh = ImGui::GetTextLineHeight();
        ImVec2 fp = ImGui::GetStyle().FramePadding;
        dl->PushClipRect(ImVec2(bmin.x + 1, bmin.y + 1), ImVec2(bmax.x - 1, bmax.y - 1), true);
        ImVec2 o(bmin.x + fp.x - scroll.x, bmin.y + fp.y - scroll.y);
        std::string src(allo_script_buf);
        std::istringstream ss(src);
        std::string line;
        for (int ln = 0; std::getline(ss, line); ++ln)
            allo_draw_line(dl, ImVec2(o.x, o.y + ln * lh), cw, line);
        if (cpos >= 0) { // the cursor (state exists only while focused)
            int cl = 0, cc = 0;
            for (int i = 0; i < cpos && allo_script_buf[i]; ++i) {
                if (allo_script_buf[i] == '\n') { cl++; cc = 0; }
                else cc++;
            }
            if (std::fmod(ImGui::GetTime(), 1.06) < 0.66) {
                float cx = o.x + cc * cw, cy = o.y + cl * lh;
                dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + lh),
                            ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
            }
        }
        dl->PopClipRect();
    }
    ImGui::PopFont();

    // status: parse errors (live) or how many things this script styles
    ImGui::Separator();
    bool en = hormiga::temper::field_value(*sel, "enabled") != "0";
    allo::Script sc = allo::parse(std::string(allo_script_buf));
    if (!sc.ok()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.42f, 0.42f, 1));
        for (size_t i = 0; i < sc.errors.size() && i < 3; ++i)
            ImGui::TextWrapped("! %s", sc.errors[i].c_str());
        ImGui::PopStyleColor();
    } else if (!en) {
        ImGui::TextDisabled("parses OK - enable it on the left to apply");
    } else {
        maiz::ProjectOptions dp;
        dp.mantle = kDataMantle;
        maiz::Scene ds;
        try { ds = maiz::project_scene(core, dp); } catch (...) {}
        std::vector<allo::Thing> things;
        for (const auto& n : ds.nodes) {
            if (n.glyph == "map" || n.glyph == "script" ||
                n.glyph.rfind("allo_", 0) == 0)
                continue;
            allo::Rune r{n.name, n.glyph, n.tags, {}, {}};
            for (const auto& f : n.fields)
                r.fields.push_back({f.key, hormiga::temper::field_value(n, f.key.c_str())});
            things.push_back(std::move(r));
        }
        allo_attach_links(things, ds);
        allo_compute_measures(things);
        std::set<std::string> styled;
        std::vector<std::string> rt;
        allo::run(sc, things,
                  [&](const std::string& t, const std::string&) { styled.insert(t); }, &rt);
        if (!rt.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.42f, 0.42f, 1));
            for (size_t i = 0; i < rt.size() && i < 3; ++i)
                ImGui::TextWrapped("! %s", rt[i].c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("parses OK - styles %d thing(s)", (int)styled.size());
        }
    }
    ImGui::EndChild();
}

// ── the FROM-SCRATCH code editor (dev tab, 2026-08-05) ──────────────────────
// Own text buffer / caret / selection / clipboard / rendering — the foundation
// for inline widgets + intellisense the ImGui InputText can't host (it's a
// black box). v1: text input (UTF-8), caret movement (arrows/home/end),
// click+drag & shift selection, copy/cut/paste/select-all, syntax coloring, a
// blinking caret, scrolling + auto-scroll to caret. Returns true on edit.
bool HormigaApp::code_editor(const char* id, std::string& text, const ImVec2& size) {
    ImFont* mono = mono_font ? mono_font : ImGui::GetFont();
    ImGui::PushFont(mono);
    const float cw = ImGui::CalcTextSize("0").x, lh = ImGui::GetTextLineHeight();
    bool changed = false;
    ImGuiIO& io = ImGui::GetIO();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);

    std::vector<int> ls; // line-start indices
    auto lstarts = [&] { ls.clear(); ls.push_back(0);
        for (int i = 0; i < (int)text.size(); ++i) if (text[i] == '\n') ls.push_back(i + 1); };
    lstarts();
    auto lend = [&](int l) { return (l + 1 < (int)ls.size()) ? ls[l + 1] - 1 : (int)text.size(); };
    auto llen = [&](int l) { return lend(l) - ls[l]; };
    auto to_lc = [&](int c) { int l = 0; while (l + 1 < (int)ls.size() && ls[l + 1] <= c) ++l; return std::pair<int, int>(l, c - ls[l]); };
    auto to_idx = [&](int l, int col) { l = std::clamp(l, 0, (int)ls.size() - 1); col = std::clamp(col, 0, llen(l)); return ls[l] + col; };
    auto clampc = [&](int c) { return std::clamp(c, 0, (int)text.size()); };
    ce_caret = clampc(ce_caret); ce_sel = clampc(ce_sel);

    ImVec2 origin = ImGui::GetCursorScreenPos(); origin.x += 4; origin.y += 2;
    float availh = ImGui::GetContentRegionAvail().y;
    float surfw = std::max(ImGui::GetContentRegionAvail().x, 400.0f);
    float surfh = std::max((float)ls.size() * lh + 8.0f, availh);
    ImGui::InvisibleButton("##surf", ImVec2(surfw, surfh), ImGuiButtonFlags_MouseButtonLeft);
    bool activated = ImGui::IsItemActivated(), active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    bool focused = ImGui::IsWindowFocused();

    // ── scan STRING tokens once (skipping comments): each becomes a candidate
    // for an inline widget — a "#hex" → a clickable color swatch (feature 1),
    // a contact/organization NAME → its avatar thumbnail (feature 2). ──────────
    struct STok { int bpos, line, col, len; std::string val; };
    std::vector<STok> stoks;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '-' && i + 1 < text.size() && text[i + 1] == '-') {
            while (i < text.size() && text[i] != '\n') ++i; continue;
        }
        if (text[i] == '"') {
            size_t j = i + 1; // a string never crosses a newline (even unterminated)
            while (j < text.size() && text[j] != '"' && text[j] != '\n') ++j;
            bool closed = j < text.size() && text[j] == '"';
            auto lc = to_lc((int)i);
            stoks.push_back({(int)i, lc.first, lc.second,
                             closed ? (int)(j + 1 - i) : (int)(j - i),
                             text.substr(i + 1, j - (i + 1))});
            i = closed ? j + 1 : j;
        } else ++i;
    }
    auto is_hex = [](const std::string& v) {
        if (v.size() != 7 || v[0] != '#') return false;
        for (int k = 1; k < 7; ++k) if (!std::isxdigit((unsigned char)v[k])) return false;
        return true;
    };
    // contact/organization runes → their avatar path (for the inline thumbnail)
    std::map<std::string, std::string> avatar_of;
    for (const auto& n : ce_data.nodes)
        if (n.glyph == "contact" || n.glyph == "organization") {
            std::string a = avatar_path(n);
            if (!a.empty()) avatar_of[n.name] = a;
        }

    auto mouse_caret = [&] {
        int l = std::clamp((int)((io.MousePos.y - origin.y) / lh), 0, (int)ls.size() - 1);
        int col = (int)((io.MousePos.x - origin.x) / cw + 0.5f);
        return to_idx(l, col);
    };
    auto is_date = [](const std::string& v) { // "YYYY-MM-DD"
        if (v.size() != 10 || v[4] != '-' || v[7] != '-') return false;
        for (int k = 0; k < 10; ++k) if (k != 4 && k != 7 && !std::isdigit((unsigned char)v[k])) return false;
        return true;
    };
    // which string token is under the mouse? (for the inline color / date widgets)
    auto tok_at_mouse = [&]() -> const STok* {
        for (const auto& s : stoks) {
            float x0 = origin.x + s.col * cw, x1 = origin.x + (s.col + s.len) * cw;
            float y0 = origin.y + s.line * lh, y1 = y0 + lh;
            if (io.MousePos.x >= x0 && io.MousePos.x <= x1 &&
                io.MousePos.y >= y0 && io.MousePos.y <= y1)
                return &s;
        }
        return nullptr;
    };
    bool moved = false;
    if (activated) {
        const STok* t = tok_at_mouse();
        if (t && is_hex(t->val)) { // feature 1: click a color → the wheel, in place
            ce_color_pos = t->bpos + 1;
            unsigned rr = 0, gg = 0, bb = 0;
            std::sscanf(text.c_str() + ce_color_pos, "#%02x%02x%02x", &rr, &gg, &bb);
            allo_pick[0] = rr / 255.0f; allo_pick[1] = gg / 255.0f; allo_pick[2] = bb / 255.0f;
            ImGui::OpenPopup("ce-wheel");
        } else if (t && is_date(t->val)) { // click a date → a mini calendar
            ce_date_pos = t->bpos + 1;
            std::sscanf(t->val.c_str(), "%d-%d", &ce_date_y, &ce_date_m);
            ImGui::OpenPopup("ce-datepicker");
        } else {
            ce_caret = mouse_caret(); if (!io.KeyShift) ce_sel = ce_caret; moved = true;
        }
    }
    else if (active && ImGui::IsMouseDragging(0)) { ce_caret = mouse_caret(); moved = true; }

    auto utf8 = [](unsigned cp) { std::string s;
        if (cp < 0x80) s += (char)cp;
        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
        else { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
        return s; };

    // ── INTELLISENSE context: caret inside a string right after `has` → suggest
    // TAGS from the vocabulary (the seed of type-aware suggestions — later the
    // subject's glyph narrows this to its tags/fields via the tag recommender).
    bool ac_active = false; int ac_qstart = -1; std::string ac_partial;
    std::vector<std::string> ac_matches;
    for (const auto& s : stoks) {
        // caret must be inside the string CONTENT (past the open quote, at or
        // before the close quote / end of an unterminated string)
        int cend = (s.len > 0 && text[s.bpos + s.len - 1] == '"') ? s.bpos + s.len - 1 : s.bpos + s.len;
        if (ce_caret <= s.bpos || ce_caret > cend) continue;
        int p = s.bpos - 1;
        while (p >= 0 && (text[p] == ' ' || text[p] == '\t')) --p;
        int we = p + 1;
        while (p >= 0 && (std::isalnum((unsigned char)text[p]) || text[p] == '_')) --p;
        if (text.substr(p + 1, we - (p + 1)) == "has") {
            ac_active = true; ac_qstart = s.bpos + 1;
            ac_partial = text.substr(s.bpos + 1, ce_caret - (s.bpos + 1));
        }
        break;
    }
    if (ac_active) {
        std::set<std::string> vocab;
        for (const auto& n : ce_data.nodes)
            for (const auto& t : n.tags) vocab.insert(t);
        for (const auto& t : vocab)
            if (ac_matches.size() < 8 && t.find(ac_partial) != std::string::npos)
                ac_matches.push_back(t);
    }

    if (focused) {
        bool ctrl = io.KeyCtrl, shift = io.KeyShift;
        int sA = std::min(ce_sel, ce_caret), sB = std::max(ce_sel, ce_caret);
        bool hasSel = sA != sB;
        auto del_sel = [&] { if (ce_sel != ce_caret) { int a = std::min(ce_sel, ce_caret), b = std::max(ce_sel, ce_caret); text.erase(a, b - a); ce_caret = ce_sel = a; changed = true; } };
        auto move_to = [&](int c) { ce_caret = clampc(c); if (!shift) ce_sel = ce_caret; moved = true; };

        if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) { ce_sel = 0; ce_caret = (int)text.size(); moved = true; }
        else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_C)) { if (hasSel) ImGui::SetClipboardText(text.substr(sA, sB - sA).c_str()); }
        else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_X)) { if (hasSel) { ImGui::SetClipboardText(text.substr(sA, sB - sA).c_str()); del_sel(); moved = true; } }
        else if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V)) { if (const char* clip = ImGui::GetClipboardText()) { del_sel(); std::string cs(clip); text.insert(ce_caret, cs); ce_caret += (int)cs.size(); ce_sel = ce_caret; changed = moved = true; } }
        else {
            for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                unsigned ch = io.InputQueueCharacters[i];
                if (!ctrl && ch >= 32) { del_sel(); std::string s = utf8(ch); text.insert(ce_caret, s); ce_caret += (int)s.size(); ce_sel = ce_caret; changed = moved = true; }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) { // complete a tag, else soft-tab
                if (ac_active && !ac_matches.empty()) {
                    text.replace(ac_qstart, ce_caret - ac_qstart, ac_matches[0]);
                    ce_caret = ac_qstart + (int)ac_matches[0].size(); ce_sel = ce_caret;
                    changed = moved = true;
                } else { text.insert(ce_caret, "  "); ce_caret += 2; ce_sel = ce_caret; changed = moved = true; }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                del_sel();
                // auto-indent: carry the current line's leading whitespace, and
                // add one level after a block opener (do / then / else)
                auto lc = to_lc(ce_caret);
                int lst = ls[lc.first];
                std::string indent;
                for (int k = lst; k < (int)text.size() && (text[k] == ' ' || text[k] == '\t'); ++k)
                    indent += text[k];
                std::string cur = text.substr(lst, lend(lc.first) - lst);
                while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t')) cur.pop_back();
                size_t ws = cur.find_last_of(" \t");
                std::string lastw = ws == std::string::npos ? cur : cur.substr(ws + 1);
                if (lastw == "do" || lastw == "then" || lastw == "else") indent += "  ";
                std::string ins = "\n" + indent;
                text.insert(ce_caret, ins); ce_caret += (int)ins.size(); ce_sel = ce_caret;
                changed = moved = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) { if (hasSel) del_sel(); else if (ce_caret > 0) { text.erase(ce_caret - 1, 1); ce_caret--; ce_sel = ce_caret; changed = moved = true; } }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) { if (hasSel) del_sel(); else if (ce_caret < (int)text.size()) { text.erase(ce_caret, 1); ce_sel = ce_caret; changed = true; } }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) move_to(ce_caret - 1);
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) move_to(ce_caret + 1);
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) { auto lc = to_lc(ce_caret); move_to(ls[lc.first]); }
            if (ImGui::IsKeyPressed(ImGuiKey_End)) { auto lc = to_lc(ce_caret); move_to(lend(lc.first)); }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { auto lc = to_lc(ce_caret); if (lc.first > 0) move_to(to_idx(lc.first - 1, lc.second)); }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { auto lc = to_lc(ce_caret); if (lc.first + 1 < (int)ls.size()) move_to(to_idx(lc.first + 1, lc.second)); }
        }
    }

    lstarts(); ce_caret = clampc(ce_caret); ce_sel = clampc(ce_sel); // refresh after edits
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // selection highlight
    if (ce_sel != ce_caret) {
        int sA = std::min(ce_sel, ce_caret), sB = std::max(ce_sel, ce_caret);
        auto a = to_lc(sA), b = to_lc(sB);
        ImU32 selc = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);
        for (int l = a.first; l <= b.first; ++l) {
            int c0 = (l == a.first) ? a.second : 0;
            int c1 = (l == b.first) ? b.second : llen(l) + 1;
            dl->AddRectFilled(ImVec2(origin.x + c0 * cw, origin.y + l * lh),
                              ImVec2(origin.x + c1 * cw, origin.y + (l + 1) * lh), selc);
        }
    }
    // text (syntax coloured)
    { std::istringstream ss(text); std::string line;
      for (int l = 0; std::getline(ss, line); ++l)
          allo_draw_line(dl, ImVec2(origin.x, origin.y + l * lh), cw, line); }
    // feature 2: an inline avatar thumbnail beside a referenced contact/org name
    for (const auto& s : stoks) {
        auto it = avatar_of.find(s.val);
        if (it == avatar_of.end()) continue;
        HostTexture t = texture_for(it->second);
        if (!t.id) continue;
        float sz = lh - 2;
        ImVec2 p0(origin.x + (s.col + s.len) * cw + 3, origin.y + s.line * lh + 1);
        dl->AddImageRounded((ImTextureID)(intptr_t)t.id, p0, ImVec2(p0.x + sz, p0.y + sz),
                            ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, sz * 0.5f);
    }
    // affordance: outline a hovered color/date token + a "click to edit" hint
    if (hovered) {
        for (const auto& s : stoks) {
            bool hx = is_hex(s.val), dt = is_date(s.val);
            if (!hx && !dt) continue;
            float x0 = origin.x + s.col * cw, x1 = origin.x + (s.col + s.len) * cw;
            float y0 = origin.y + s.line * lh, y1 = y0 + lh;
            if (io.MousePos.x >= x0 && io.MousePos.x <= x1 && io.MousePos.y >= y0 && io.MousePos.y <= y1) {
                dl->AddRect(ImVec2(x0 - 1, y0), ImVec2(x1 + 1, y1),
                            ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ImGui::SetTooltip(hx ? "click to open the color wheel"
                                     : "click to open the calendar");
            }
        }
    }
    { auto lc = to_lc(ce_caret);
      if (moved) ce_blink = (float)ImGui::GetTime();
      if (moved || std::fmod(ImGui::GetTime() - ce_blink, 1.06) < 0.66) {
          float cx = origin.x + lc.second * cw, cy = origin.y + lc.first * lh;
          dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy + lh), ImGui::GetColorU32(ImGuiCol_Text), 1.5f);
      }
      if (moved) { // auto-scroll to caret
          float cy = lc.first * lh;
          if (cy < ImGui::GetScrollY()) ImGui::SetScrollY(cy);
          else if (cy + lh > ImGui::GetScrollY() + availh) ImGui::SetScrollY(cy + lh - availh);
      }
    }

    // ── the autocomplete box (drawn, not a popup, so it never steals focus) ───
    if (ac_active && !ac_matches.empty()) {
        auto lc = to_lc(ce_caret);
        ImVec2 p0(origin.x + lc.second * cw, origin.y + (lc.first + 1) * lh + 2);
        float w = 60;
        for (const auto& m : ac_matches) w = std::max(w, ImGui::CalcTextSize(m.c_str()).x + 12);
        ImVec2 p1(p0.x + w, p0.y + ac_matches.size() * lh + lh + 6);
        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_PopupBg), 3.0f);
        dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
        for (size_t i = 0; i < ac_matches.size(); ++i)
            dl->AddText(ImVec2(p0.x + 6, p0.y + 3 + i * lh),
                        ImGui::GetColorU32(i == 0 ? ImGuiCol_Text : ImGuiCol_TextDisabled),
                        ac_matches[i].c_str());
        dl->AddText(ImVec2(p0.x + 6, p0.y + 3 + ac_matches.size() * lh),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), "[Tab]");
    }

    // ── feature 3: hover a `runes where …` line → preview a few matching runes,
    // so you get a rough idea of which runes the set refers to (they aren't named).
    if (hovered && ce_color_pos < 0) {
        int hl = std::clamp((int)((io.MousePos.y - origin.y) / lh), 0, (int)ls.size() - 1);
        std::string lt = text.substr(ls[hl], llen(hl));
        size_t wp = lt.find("runes where");
        if (wp != std::string::npos) {
            std::string pred = lt.substr(wp + 11); // after "runes where"
            std::string probe = "for each rune do if (" + pred +
                                ") then rune:color(\"#000000\") end end";
            allo::Script sc = allo::parse(probe);
            if (sc.ok()) {
                std::vector<allo::Rune> rs;
                for (const auto& n : ce_data.nodes) {
                    if (n.glyph == "map" || n.glyph == "script" || n.glyph.rfind("allo_", 0) == 0)
                        continue;
                    allo::Rune r{n.name, n.glyph, n.tags, {}, {}};
                    for (const auto& f : n.fields)
                        r.fields.push_back({f.key, hormiga::temper::field_value(n, f.key.c_str())});
                    rs.push_back(std::move(r));
                }
                allo_attach_links(rs, ce_data);
                int total = 0; std::vector<std::string> names;
                allo::run(sc, rs, [&](const std::string& nm, const std::string&) {
                    ++total; if (names.size() < 8) names.push_back(nm);
                });
                ImGui::BeginTooltip();
                ImGui::Text("this set = %d rune(s), e.g.:", total);
                for (const auto& nm : names) {
                    auto it = avatar_of.find(nm);
                    if (it != avatar_of.end()) {
                        HostTexture t = texture_for(it->second);
                        if (t.id) { ImGui::Image((ImTextureID)(intptr_t)t.id, ImVec2(16, 16)); ImGui::SameLine(); }
                    }
                    ImGui::TextUnformatted(nm.c_str());
                }
                if (total > (int)names.size())
                    ImGui::TextDisabled("... and %d more", total - (int)names.size());
                ImGui::EndTooltip();
            }
        }
    }

    // ── feature 1: the color wheel popup — live-edits the "#hex" it opened on ──
    if (ImGui::BeginPopup("ce-wheel")) {
        if (ImGui::ColorPicker3("##w", allo_pick,
                                ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs)) {
            if (ce_color_pos >= 0 && ce_color_pos + 7 <= (int)text.size()) {
                char nh[8];
                std::snprintf(nh, sizeof nh, "#%02x%02x%02x",
                              (int)(allo_pick[0] * 255 + 0.5f),
                              (int)(allo_pick[1] * 255 + 0.5f),
                              (int)(allo_pick[2] * 255 + 0.5f));
                text.replace(ce_color_pos, 7, nh);
                changed = true;
            }
        }
        ImGui::EndPopup();
    } else ce_color_pos = -1;

    // ── a mini CALENDAR popup for a clicked "YYYY-MM-DD" (and the same shape
    // works for times later) — click a day to rewrite the date in place. ──────
    if (ImGui::BeginPopup("ce-datepicker")) {
        if (ImGui::SmallButton("<")) { if (--ce_date_m < 1) { ce_date_m = 12; --ce_date_y; } }
        ImGui::SameLine();
        ImGui::Text("%s %d", kMonthNames[std::clamp(ce_date_m, 1, 12) - 1], ce_date_y);
        ImGui::SameLine();
        if (ImGui::SmallButton(">")) { if (++ce_date_m > 12) { ce_date_m = 1; ++ce_date_y; } }
        int curD = 0; if (ce_date_pos >= 0) std::sscanf(text.c_str() + ce_date_pos + 8, "%d", &curD);
        if (ImGui::BeginTable("##cal", 7, ImGuiTableFlags_SizingFixedFit)) {
            static const char* dw[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
            for (int c = 0; c < 7; ++c) { ImGui::TableNextColumn(); ImGui::TextDisabled("%s", dw[c]); }
            int first = cal_dow(ce_date_y, ce_date_m, 1);
            int dim = cal_dim(ce_date_y, ce_date_m);
            int cell = 0;
            for (; cell < first; ++cell) ImGui::TableNextColumn();
            for (int d = 1; d <= dim; ++d, ++cell) {
                ImGui::TableNextColumn();
                ImGui::PushID(d);
                char lbl[8]; std::snprintf(lbl, sizeof lbl, "%2d", d);
                bool sel = (d == curD);
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::SmallButton(lbl) && ce_date_pos >= 0) {
                    char nd[12]; std::snprintf(nd, sizeof nd, "%04d-%02d-%02d", ce_date_y, ce_date_m, d);
                    text.replace(ce_date_pos, 10, nd);
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                if (sel) ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndPopup();
    } else ce_date_pos = -1;

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    return changed;
}

// The DEV Allomone tab: FULL parity with the Allomone tab (list / enable / tags /
// new / delete / save) but editing through the from-scratch custom editor. The
// production tab is untouched; the two merge once the editor is at parity.
void HormigaApp::draw_allomone_dev_body() {
    ImGui::TextDisabled("EXPERIMENTAL - the custom editor, full script management. Enable a script to apply it.");
    ImGui::SameLine();
    ImGui::TextDisabled("  [styling %d thing(s)]", (int)allo_colors.size());
    ImGui::Separator();

    maiz::ProjectOptions po; po.mantle = kAlloMantle;
    maiz::Scene as;
    try { as = maiz::project_scene(core, po); } catch (...) {}
    std::vector<const maiz::SceneNode*> scripts;
    for (const auto& n : as.nodes) if (n.glyph == "script") scripts.push_back(&n);
    if (allo_sel.empty() ||
        std::none_of(scripts.begin(), scripts.end(), [&](auto s) { return s->name == allo_sel; }))
        allo_sel = scripts.empty() ? "" : scripts.front()->name;
    auto disp = [&](const maiz::SceneNode* s) { std::string nm = hormiga::temper::field_value(*s, "name"); return nm.empty() ? s->name : nm; };

    // ── left: script list (enable toggles + new) ──────────────────────────────
    ImGui::BeginChild("dev-scripts", ImVec2(210, 0), ImGuiChildFlags_Borders);
    if (ImGui::Button("+ New script", ImVec2(-1, 0))) {
        std::string nm;
        for (int i = 1;; ++i) { nm = "script-" + std::to_string(i); if (!as.find(nm)) break; }
        std::string tmpl = "-- a new Allomone Script (derive-only for now)\n\n"
                           "for each rune do\n  if rune has \"type:contact\" then\n"
                           "    rune:color(\"#2e8b57\")\n  end\nend\n";
        allo_cmd(maiz::compile_commit(
            {"rune new script " + nm, "set " + nm + " name \"" + nm + "\"",
             "set " + nm + " enabled \"1\"",
             "setjson " + nm + " body " + json_arg(nlohmann::json(tmpl).dump())}));
        allo_sel = nm; ce_for.clear();
    }
    ImGui::Separator();
    for (const auto* s : scripts) {
        ImGui::PushID(s->name.c_str());
        bool en = hormiga::temper::field_value(*s, "enabled") != "0";
        if (ImGui::Checkbox("##en", &en))
            allo_cmd("set " + s->name + " enabled \"" + (en ? "1" : "0") + "\"");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("enable / disable");
        ImGui::SameLine();
        if (!en) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        if (ImGui::Selectable(disp(s).c_str(), allo_sel == s->name)) { allo_sel = s->name; ce_for.clear(); }
        if (!en) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    if (scripts.empty()) ImGui::TextDisabled("no scripts yet - press + New script");
    ImGui::EndChild();

    // ── right: header (name / enable / save / delete), tags, custom editor ─────
    ImGui::SameLine();
    ImGui::BeginChild("dev-editor", ImVec2(0, 0));
    const maiz::SceneNode* sel = allo_sel.empty() ? nullptr : as.find(allo_sel);
    if (!sel || sel->glyph != "script") {
        ImGui::TextDisabled("select a script on the left, or press + New script");
        ImGui::EndChild();
        return;
    }
    bool en = hormiga::temper::field_value(*sel, "enabled") != "0";
    { // name + enable + save + delete
        static char nbuf[80];
        std::string nm = hormiga::temper::field_value(*sel, "name");
        std::snprintf(nbuf, sizeof nbuf, "%s", nm.empty() ? sel->name.c_str() : nm.c_str());
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputTextWithHint("##sname", "script name", nbuf, sizeof nbuf, ImGuiInputTextFlags_EnterReturnsTrue))
            allo_cmd("set " + sel->name + " name " + json_arg(nlohmann::json(std::string(nbuf)).dump()));
        ImGui::SameLine();
        if (ImGui::Checkbox("enabled", &en))
            allo_cmd("set " + sel->name + " enabled \"" + (en ? "1" : "0") + "\"");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 108);
    }
    bool doSave = ImGui::Button("Save");
    ImGui::SameLine();
    if (ImGui::SmallButton("Delete")) {
        allo_cmd("rm " + sel->name); allo_sel.clear(); ce_for.clear();
        ImGui::EndChild(); return;
    }
    { std::vector<std::string> tcmds; draw_tag_editor(*sel, tcmds); for (const auto& c : tcmds) allo_cmd(c); ImGui::Separator(); }

    if (ce_for != sel->name) { // load the selected script's body
        ce_for = sel->name;
        ce_text = hormiga::temper::field_value(*sel, "body");
        ce_caret = ce_sel = 0; ce_dirty = false;
    }
    { maiz::ProjectOptions dp; dp.mantle = kDataMantle;
      try { ce_data = maiz::project_scene(core, dp); } catch (...) {} }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) doSave = true;
    float h = ImGui::GetContentRegionAvail().y - 34;
    if (code_editor("##devedit", ce_text, ImVec2(-1, std::max(120.0f, h)))) ce_dirty = true;
    // commit: on Save, or automatically when focus leaves with unsaved edits
    bool editing = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    if (doSave || (ce_dirty && !editing)) {
        allo_cmd("setjson " + sel->name + " body " + json_arg(nlohmann::json(ce_text).dump()));
        ce_dirty = false;
    }

    ImGui::Separator();
    allo::Script sc = allo::parse(ce_text);
    if (!sc.ok()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.42f, 0.42f, 1));
        for (size_t i = 0; i < sc.errors.size() && i < 3; ++i)
            ImGui::TextWrapped("! %s", sc.errors[i].c_str());
        ImGui::PopStyleColor();
    } else if (!en) {
        ImGui::TextDisabled("parses OK - tick 'enabled' to apply%s", ce_dirty ? "  (unsaved edits)" : "");
    } else {
        ImGui::TextDisabled("parses OK - Save (or click away) to apply%s", ce_dirty ? "  *unsaved*" : "");
    }
    ImGui::EndChild();
}

#endif // HORMIGA_LEGACY_ALLOMONE

