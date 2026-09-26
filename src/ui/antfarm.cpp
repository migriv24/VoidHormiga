/* ui/antfarm.cpp — the Antfarm's tab: its warning, the node graph, the inspector.
 *
 * Moved out of ui/builder.cpp on 2026-09-25, when the Antfarm started syncing
 * across devices and grew its own tools (Arrange, the device nodes): it had
 * lived in the Builder's file since the two shared a tab, and it no longer
 * shares anything with the Builder but the canvas library.
 * okf/concepts/platform/antfarm/index.md. */
#include "app/app_internal.hpp"

// ── the Antfarm section: placeholder cards until the registry exists ────────

void HormigaApp::draw_antfarm_section() {
    /* ── SAID ON THE SCREEN: NOT READY FOR PEOPLE YET (2026-09-15) ───────────
     *
     * The author: *"antfarm in general should have like a little warning in the
     * GUI that it's not really ready for human users yet. Sure we have the node
     * graph, but i'll be honest, it does NOT work ... the bones of the antfarm
     * works, and it can currently be driven by agents in a fine enough way. it's
     * mostly the UI/UX of the antfarm is horrible to a point where it might be
     * unusable."*
     *
     * The warning is small and it is honest about which half is which: the model
     * underneath is what every publish, upload and import already runs on, and it
     * is the SCREEN that is not finished. A redesign is its own piece of work. */
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.60f, 0.15f, 1.0f));
    ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION
                       "  This screen is not ready for everyday use yet - the node graph "
                       "misbehaves. What it configures does work: agents drive it from the "
                       "command line, and the panels here (Hosting images online, Publish) "
                       "are the dependable way in. A redesign is planned.");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("the backends themselves are fine - this is about the graph's\n"
                          "editing surface, which is being redesigned");

    nested_dockspace("antfarm-dock", nullptr, nullptr, "Node graph##antfarm",
                     "Inspector##antfarm");
    ImGui::Begin("Node graph##antfarm", nullptr, ImGuiWindowFlags_NoScrollbar);
    // the node graph IS the configuration surface (okf/concepts/platform/antfarm/index.md):
    // one core hub, typed sockets, providers plugged in; faces are live
    // describe() views; the Supabase node's button imports the real org
    /* ARRANGE (2026-09-25): flow runs left to right. The default colony put the
     * core's consumers to its LEFT while its ports face RIGHT, so every wire
     * looped back around the graph, which is most of why it "misbehaves" to
     * look at. Each node goes one column right of whatever feeds it (its depth
     * along the directed wires), columns keep their current top-to-bottom
     * order, and the result is one undo frame of ordinary position commands,
     * which sync to every member like any node move. */
    if (ImGui::SmallButton(ICON_FA_TABLE_COLUMNS "  Arrange")) {
        std::map<std::string, int> depth;
        for (const auto& n : scene.nodes) depth[n.name] = 0;
        for (std::size_t pass = 0; pass < scene.nodes.size(); ++pass) { // longest path; a cycle stops at n passes
            bool moved = false;
            for (const auto& w : scene.wires)
                if (w.directed && depth.count(w.from) && depth.count(w.to) && depth[w.to] < depth[w.from] + 1) {
                    depth[w.to] = depth[w.from] + 1;
                    moved = true;
                }
            if (!moved) break;
        }
        std::map<int, std::vector<const maiz::SceneNode*>> cols;
        for (const auto& n : scene.nodes) cols[depth[n.name]].push_back(&n);
        std::vector<std::string> cmds;
        for (auto& [col, nodes] : cols) {
            std::stable_sort(nodes.begin(), nodes.end(),
                             [](const maiz::SceneNode* a, const maiz::SceneNode* b) { return a->y < b->y; });
            float y = 40.0f;
            for (const maiz::SceneNode* n : nodes) {
                char pos[96];
                std::snprintf(pos, sizeof pos, "setjson %s pos [%d,%d]", n->name.c_str(), 60 + col * 330, (int)y);
                cmds.push_back(pos);
                y += std::max(n->h, 60.0f) + 70.0f; // header + face, and air for the wires between
            }
        }
        if (!cmds.empty()) dispatch_and_reproject(maiz::compile_commit(cmds));
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("lay the graph out left to right, the way records and assets flow;\n"
                          "positions are shared, so this arranges it on every device");
    ImGui::SameLine();
    ImGui::TextDisabled("the org's backends, by payload: records + assets flow "
                        "from the core; the HTML publisher builds a site the "
                        "server/deploy nodes carry. ports only fit their own type.");
    maiz::CanvasNet anet;
    anet.surfaces = &surfaces;
    anet.roster = &roster;
    anet.display = net_settings.show;
    anet.shareable = share_now;
    anet.surface_id = "canvas:antfarm";
    maiz::CanvasIO cio = maiz::edit_canvas("antfarm-canvas", scene, ed,
                                           canvas_style, &palette_antfarm, &faces,
                                           {}, nullptr, &anet);
    for (const auto& cmd : cio.commands) dispatch_and_reproject(cmd);
    ImGui::End(); // Node graph

    ImGui::Begin("Inspector##antfarm");
    draw_hosting_panel(); // "host it online": which node, and what is waiting
    maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
    for (const auto& cmd : iio.commands) dispatch_and_reproject(cmd);
    ImGui::End(); // Inspector

}
