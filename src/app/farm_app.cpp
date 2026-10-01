/* app/farm_app.cpp — Antfarm v2 inside the application: the verb, the faces,
 * the canvas style, the inspector. Shared by the desktop tab (ui/farm.cpp) and
 * the phone (phone/phone_farm.cpp), which is why it lives in app/ and not ui/.
 * okf/concepts/platform/antfarm/v2/canvas.md. */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp" // LanRuntime: who is at this computer

#include "voidmaiz/gesture.hpp"


void HormigaApp::reconcile_chambers() {
    fv2.chambers_dirty = false;
    const auto cmds = hormiga::farmhost::chamber_commands(core, base_dir, {ship_dir}, hormiga::farmhost::this_device((bool)phone),
                                                          scene.mantle);
    if (!cmds.empty()) dispatch_and_reproject(maiz::compile_commit(cmds));
}

std::string HormigaApp::antfarm_mantle() const {
    return fv2.v2 && fv2.here ? std::string(farm::kMantle) : std::string(kAntfarmMantle);
}

farm::Context HormigaApp::farm_context() {
    hormiga::farmhost::Device dev;
    dev.kind = phone ? "phone" : "desktop";
    dev.serves_local = !phone; // the phone serves nothing yet (documents.md §4)
    dev.me = lan ? lan->me.username : std::string();
    dev.also = {ship_dir};
    const maiz::Roster* ros = &roster;
    const std::string me = dev.me;
    return hormiga::farmhost::make_context(core, base_dir, &vault, dev, [ros, me](const std::string& u) {
        if (u == me) return std::string("here (this device)");
        for (const auto& p : ros->peers())
            if (p.state.who.name == u)
                return "here, on " + (p.state.device.empty() ? std::string("a device") : p.state.device);
        return std::string();
    });
}

void HormigaApp::refresh_farm() {
    fv2.faces.clear();
    fv2.rows.clear();
    if (scene.mantle != farm::kMantle) return;
    const farm::Graph g = farm::read(scene);
    const farm::Context ctx = farm_context();
    farm::Evaluator ev(g, ctx);
    for (const auto& n : g.nodes)
        if (n.kind) fv2.faces[n.name] = ev.face(n.name);
    // strands (types.md §5): each wire as many as its source says it carries
    for (auto& w : scene.wires) {
        const auto colon = w.relation.find(':');
        auto it = fv2.faces.find(w.from);
        if (colon == std::string::npos || it == fv2.faces.end()) continue;
        auto s = it->second.strands.find(w.relation.substr(0, colon));
        if (s != it->second.strands.end()) w.strands = s->second;
    }
    fv2.rows = hormiga::farmhost::connection_rows(g, ev);
    fv2.chambers_dirty = true; // looking at the Antfarm keeps the chambers in step
}

/* ── `farm …` from the command bar ──────────────────────────────────────── */
bool HormigaApp::try_farm_verb(const std::string& cmd) {
    std::vector<std::string> tok = farm::tokenize(cmd);
    if (tok.empty() || tok[0] != "farm") return false;
    tok.erase(tok.begin());
    const bool exists = hormiga::farmhost::farm_exists(core);
    const maiz::Scene fs_ = hormiga::farmhost::project_farm(core);
    const farm::Graph g = farm::read(fs_);
    const farm::Context ctx = farm_context();
    farm::Evaluator ev(g, ctx);
    const std::string me = lan ? lan->me.username : std::string();
    const farm::VerbResult r = farm::run(tok, g, exists, &ev,
                                         hormiga::farmhost::seed_info(core, cur_doc, me));
    if (r.needs_host) {
        const std::string v = tok.empty() ? "" : tok[0];
        if (v == "showcase") {
            std::string note;
            const auto cmds = hormiga::farmhost::showcase_commands(core, base_dir, scene.mantle, note);
            if (!cmds.empty()) dispatch_and_reproject(maiz::compile_commit(cmds));
            reconcile_chambers();
            const auto arrange = farm::arrange_commands(farm::read(hormiga::farmhost::project_farm(core)));
            if (!arrange.empty()) dispatch_and_reproject(maiz::compile_commit(farm::in_farm(arrange, scene.mantle)));
            log.push_back({cmds.empty() ? "error" : "info", "farm", note});
            fv2.fit = true;
            if (section == Antfarm && fv2.v2) dispatch_and_reproject(std::string("use ") + farm::kMantle);
        } else if (v == "chambers") {
            reconcile_chambers();
            log.push_back({"info", "farm", "the chambers are in step"});
        } else if (v == "run" || v == "check" || v == "preview" || v == "publish") {
            std::string c = "effect farm-" + v; // through the effect seam, like every crossing
            for (std::size_t i = 1; i < tok.size(); ++i) c += " " + tok[i];
            dispatch_and_reproject(c);
        } else
            log.push_back({"info", "farm",
                           "type a key's value in its inspector box: a value typed here would stay in the "
                           "command history. (`farm key set` reads standard input in voidhormiga-cli.)"});
        return true;
    }
    if (!r.text.empty()) log.push_back({r.ok ? "info" : "error", "farm", r.text});
    if (!r.ok || r.commands.empty()) return true;
    const std::string back = scene.mantle;
    if (!tok.empty() && tok[0] == "init") {
        std::vector<std::string> c = r.commands;
        c.push_back("use " + back);
        dispatch_and_reproject(maiz::compile_commit(c));
        // lay it out: a second frame, over what the first one made
        const maiz::Scene made = hormiga::farmhost::project_farm(core);
        const auto arrange = farm::arrange_commands(farm::read(made));
        if (!arrange.empty()) dispatch_and_reproject(maiz::compile_commit(farm::in_farm(arrange, back)));
        if (section == Antfarm && fv2.v2) dispatch_and_reproject(std::string("use ") + farm::kMantle);
        return true;
    }
    dispatch_and_reproject(maiz::compile_commit(farm::in_farm(r.commands, back)));
    return true;
}

/* ── the faces: readiness, then the node's own lines ───────────────────── */
namespace {
ImU32 state_color(const std::string& s) {
    if (s == "ready") return IM_COL32(80, 190, 110, 255);
    if (s == "needs") return IM_COL32(230, 170, 60, 255);
    if (s == "failing") return IM_COL32(225, 80, 70, 255);
    if (s == "planned") return IM_COL32(130, 130, 140, 255);
    if (s == "unconfigured") return IM_COL32(150, 160, 185, 255);
    return IM_COL32(110, 150, 220, 255);
}
} // namespace

void HormigaApp::draw_farm_face(maiz::FaceContext& ctx) {
    auto it = fv2.faces.find(ctx.node.name);
    if (it == fv2.faces.end()) return;
    const farm::Face& f = it->second;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float zoom = std::clamp(ctx.zoom, 0.3f, 2.0f);
    const float fs = ImGui::GetFontSize() * std::min(1.0f, zoom) * 0.92f;
    const float pad = 6.0f * zoom;
    dl->PushClipRect(ctx.pos, ImVec2(ctx.pos.x + ctx.size.x, ctx.pos.y + ctx.size.y), true);
    float y = ctx.pos.y + pad * 0.5f;
    const ImU32 col = state_color(f.ready.state);
    dl->AddCircleFilled(ImVec2(ctx.pos.x + pad + 4 * zoom, y + fs * 0.55f), 4.0f * zoom, col);
    if (zoom >= 0.45f) {
        const std::string head = f.ready.state + (f.ready.why.empty() ? "" : ": " + f.ready.why);
        dl->AddText(ImGui::GetFont(), fs, ImVec2(ctx.pos.x + pad + 12 * zoom, y), col, head.c_str());
        y += fs + 2.0f * zoom;
        const ImU32 dim = ImGui::GetColorU32(ImGuiCol_Text, 0.85f);
        for (const auto& l : f.lines) {
            if (y + fs > ctx.pos.y + ctx.size.y) break;
            dl->AddText(ImGui::GetFont(), fs, ImVec2(ctx.pos.x + pad, y), dim, l.c_str());
            y += fs + 1.0f * zoom;
        }
    }
    dl->PopClipRect();
}

void HormigaApp::register_farm_faces() {
    fv2.palette.entries.clear();
    for (const auto& k : farm::kinds()) {
        faces.by_glyph[k.glyph] = [this](maiz::FaceContext& ctx) { draw_farm_face(ctx); };
        maiz::AddPalette::Entry e{k.glyph, k.planned ? k.label + " (planned)" : k.label, k.group, {}};
        int index = 1; // the reduce contract: aux ports 1..n in declaration order
        for (const auto& p : k.ports) e.ports.push_back({index++, p.out, p.type, p.name});
        fv2.palette.entries.push_back(std::move(e));
    }
}

maiz::CanvasStyle HormigaApp::farm_canvas_style(const maiz::CanvasStyle& base) {
    maiz::CanvasStyle st = base;
    st.port_types = hormiga::farmhost::port_styles();
    st.wires = hormiga::farmhost::wire_writer([this]() -> const maiz::Scene& { return scene; },
                                              [this](const std::string& why) { toast(why, true); });
    return st;
}

/* ── the key box: a value typed into THIS device's vault, never a field ── */
void HormigaApp::draw_farm_key_box(const std::string& node) {
    const maiz::SceneNode* n = scene.find(node);
    if (!n || n->glyph != "farm_key") return;
    const std::string entry = farm::field_of(*n, "vault_entry");
    ImGui::SeparatorText("Value on this device");
    if (entry.empty()) {
        ImGui::TextDisabled("name a vault entry first");
        return;
    }
    if (!vault.unlocked()) {
        ImGui::TextDisabled("the vault is locked, so no value can be stored here");
        return;
    }
    const bool present = !vault.get(entry).empty();
    ImGui::TextDisabled(present ? "stored, sealed; shown to nobody" : "no value on this device yet");
    if (fv2.key_node != node) {
        std::memset(fv2.key_buf, 0, sizeof fv2.key_buf);
        fv2.key_node = node;
    }
    ImGui::SetNextItemWidth(-90.0f);
    ImGui::InputTextWithHint("##farmkey", present ? "replace the value" : "paste the key", fv2.key_buf,
                             sizeof fv2.key_buf, ImGuiInputTextFlags_Password);
    ImGui::SameLine();
    if (ImGui::Button("Store") && fv2.key_buf[0]) {
        toast(hormiga::farmhost::key_set(vault, vault_path(), entry, fv2.key_buf));
        std::memset(fv2.key_buf, 0, sizeof fv2.key_buf); // zeroed the moment it reaches the vault
        refresh_farm();
    }
    ImGui::TextDisabled("Shared with members in a later phase (keys.md); today it stays here.");
}

void HormigaApp::draw_farm_inspector() {
    const std::string sel = ed.selection.empty() ? std::string() : ed.selection.front();
    const maiz::SceneNode* n = sel.empty() ? nullptr : scene.find(sel);
    const farm::Kind* k = n ? farm::kind_by_glyph(n->glyph) : nullptr;
    if (!n || !k) {
        ImGui::TextDisabled("Select a node, on the graph or in Connections.");
        return;
    }
    ImGui::TextUnformatted(n->name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s · %s", k->label.c_str(), farm::stratum_name(k->stratum));
    ImGui::TextWrapped("%s", k->doc.c_str());
    if (auto it = fv2.faces.find(n->name); it != fv2.faces.end()) {
        const farm::Face& f = it->second;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(state_color(f.ready.state)));
        ImGui::TextWrapped("%s%s%s", f.ready.state.c_str(), f.ready.why.empty() ? "" : ": ", f.ready.why.c_str());
        ImGui::PopStyleColor();
        for (const auto& l : f.lines) ImGui::BulletText("%s", l.c_str());
    }
    ImGui::SeparatorText("Wires");
    const farm::Graph g = farm::read(scene);
    for (const auto& p : k->ports) {
        std::string peers;
        for (const farm::Wire* w : p.out ? g.out_of(n->name, p.name) : g.into(n->name, p.name))
            peers += (peers.empty() ? "" : ", ") + (p.out ? w->to + "." + w->in : w->from + "." + w->out);
        const farm::TypeInfo* t = farm::type_info(p.type);
        ImGui::TextDisabled("%s %s (%s)", p.out ? "out" : "in ", p.name.c_str(), t ? t->label : "any");
        ImGui::SameLine();
        ImGui::TextUnformatted(peers.empty() ? (p.out ? "-> nothing" : "<- nothing") : ((p.out ? "-> " : "<- ") + peers).c_str());
    }
    draw_farm_key_box(n->name);
    ImGui::SeparatorText("Fields");
    maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
    for (const auto& c : iio.commands) dispatch_and_reproject(c);
    ImGui::TextDisabled("CLI: farm show %s", n->name.c_str());
}
