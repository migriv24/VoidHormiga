/* ui/connections.cpp — the Data tab's connections view: a force-directed
 * picture of what Core holds, drawn in bounded work (domain/connections.hpp,
 * okf/concepts/sections/connections.md).
 *
 * NOT the node editor: a visualization, and its positions are view state,
 * never dispatched, never stored. A graph of up to kRoots runes is drawn rune
 * by rune, as before. A larger one is drawn as GROUPS found from its links
 * (Void Maiz's overview tree): clicking one opens it, right-clicking a body
 * closes the group it is in, and at most kMaxBodies are on screen, the oldest
 * opened closing to make room. Forces are Fruchterman–Reingold with
 * Barnes–Hut repulsion, so a frame's cost is what is shown, never the
 * database. Links that leave the loaded part are counted on the bodies they
 * leave from (the frontier's mojones); the strip above loads the part around
 * a rune. */
#include "app/app_internal.hpp"

#include "ui/connections.hpp"

#include <chrono>
#include <cmath>

namespace ov = maiz::overview;
using hormiga::ConnectionsView;
using hormiga::connections::kMaxBodies;

namespace {

std::string with_commas(std::uint64_t v) {
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((std::size_t)i, ",");
    return s;
}

const std::string& label_of(const ConnectionsView::Model& m, std::uint32_t id) {
    return m.graph.names[m.label[id]];
}

/* The name a position or an open group is remembered by across a rebuild. */
std::string keep_key(const ConnectionsView::Model& m, std::uint32_t id) {
    return m.tree.is_node(id) ? m.graph.names[id] : "#" + label_of(m, id) + "#" + std::to_string(m.tree.size[id]);
}

float body_mass(const ConnectionsView::Model& m, std::uint32_t id) {
    // as the drawn radius grows: logarithmically, so one large group cannot push the rest into a ring
    return m.tree.is_node(id) ? 1.0f : 1.0f + 0.5f * std::log2((float)m.tree.size[id]);
}

float body_radius(const ConnectionsView::Model& m, std::uint32_t id) {
    if (m.tree.is_node(id)) return 3.5f + 2.0f * std::sqrt((float)m.graph.degree[id]);
    return 7.0f + 5.0f * std::log2((float)m.tree.size[id]);
}

bool under(const ov::Tree& t, std::uint32_t id, std::uint32_t group) {
    for (std::uint32_t x = id; x != ov::Tree::kNone; x = t.parent[x])
        if (x == group) return true;
    return false;
}

/* The bodies for what is open now, placed where they were, or around the
 * group they came out of. `fresh`: a new picture, settled from hot. */
void recut(ConnectionsView& v, bool fresh) {
    const auto& m = *v.model;
    for (std::size_t i = 0; i < v.cut.size() && i < v.bodies.size(); ++i) { // remember where things are
        v.pos[v.cut[i]] = ImVec2(v.bodies[i].x, v.bodies[i].y);
        v.placed[v.cut[i]] = 1;
    }
    v.cut = ov::cut(m.tree, v.open);
    v.body_of = ov::body_of(m.tree, v.cut);
    v.shown = ov::lift(m.graph.links, v.body_of);
    v.bodies.assign(v.cut.size(), {});
    std::vector<ov::Body> unplaced;
    for (std::size_t i = 0; i < v.cut.size(); ++i) {
        const std::uint32_t id = v.cut[i];
        auto& b = v.bodies[i];
        b.mass = body_mass(m, id);
        if (v.placed[id]) {
            b.x = v.pos[id].x;
            b.y = v.pos[id].y;
            continue;
        }
        const std::uint32_t p = m.tree.parent[id];
        if (p != ov::Tree::kNone && v.placed[p]) { // out of the group it was in, around it
            const float a = (float)id * 2.3999632f, r = body_radius(m, p) * (0.4f + 0.6f * (float)((id * 7) % 11) / 10.0f);
            b.x = v.pos[p].x + std::cos(a) * r;
            b.y = v.pos[p].y + std::sin(a) * r;
        } else {
            std::vector<ov::Body> one(1);
            ov::seed_disc(one, 0, 0, 60.0f + 30.0f * std::sqrt((float)v.cut.size()), id + 1);
            b.x = one[0].x;
            b.y = one[0].y;
        }
        v.pos[id] = ImVec2(b.x, b.y);
        v.placed[id] = 1;
    }
    // pieces with no links between them are laid out apart, then packed side by side (Void Maiz's pack)
    std::vector<ov::Link> tied = v.shown;
    tied.insert(tied.end(), v.prox.begin(), v.prox.end());
    v.comp = ov::components((std::uint32_t)v.cut.size(), tied);
    v.radius.resize(v.cut.size());
    for (std::size_t i = 0; i < v.cut.size(); ++i) v.radius[i] = body_radius(m, v.cut[i]);
    v.iters = 0;
    v.total = fresh ? 300 : 160;
    v.temp = fresh ? 64.0f : 24.0f;
    v.drag = -1;
    if (fresh) v.fit = true;
}

void close_group(ConnectionsView& v, std::uint32_t group) {
    const auto& t = v.model->tree;
    // where it closes to: the middle of what was showing inside it
    float sx = 0, sy = 0;
    int n = 0;
    for (std::size_t i = 0; i < v.cut.size(); ++i)
        if (under(t, v.cut[i], group)) {
            sx += v.bodies[i].x;
            sy += v.bodies[i].y;
            ++n;
        }
    std::vector<std::uint32_t> keep;
    for (std::uint32_t id : v.open_order)
        if (under(t, id, group)) v.open[id] = 0;
        else keep.push_back(id);
    v.open_order = std::move(keep);
    for (std::size_t i = 0; i < v.cut.size(); ++i) // its insides forget where they were
        if (under(t, v.cut[i], group)) v.placed[v.cut[i]] = 0;
    if (n) {
        v.pos[group] = ImVec2(sx / (float)n, sy / (float)n);
        v.placed[group] = 1;
    }
    // positions of the bodies leaving the screen must not be re-recorded by recut
    std::vector<ov::Body> keep_bodies;
    std::vector<std::uint32_t> keep_cut;
    for (std::size_t i = 0; i < v.cut.size(); ++i)
        if (!under(t, v.cut[i], group) || v.cut[i] == group) {
            keep_cut.push_back(v.cut[i]);
            keep_bodies.push_back(v.bodies[i]);
        }
    v.cut = std::move(keep_cut);
    v.bodies = std::move(keep_bodies);
    recut(v, false);
}

/* Open `group`, closing the oldest opened groups (never one it is inside)
 * while the screen would hold too many. False if it cannot fit at all. */
bool open_group(ConnectionsView& v, std::uint32_t group) {
    const auto& t = v.model->tree;
    if (t.is_node(group) || v.open[group]) return true;
    auto shown_after = [&] { return v.cut.size() - 1 + t.children[group].size(); };
    while (shown_after() > kMaxBodies) {
        auto it = std::find_if(v.open_order.begin(), v.open_order.end(),
                               [&](std::uint32_t id) { return !under(t, group, id); });
        if (it == v.open_order.end()) return false;
        close_group(v, *it);
    }
    v.open[group] = 1;
    v.open_order.push_back(group);
    recut(v, false);
    return true;
}

/* A new model: keep what can be kept, by name. */
void adopt(ConnectionsView& v, std::shared_ptr<const ConnectionsView::Model> next) {
    if (v.model) {
        for (std::size_t i = 0; i < v.cut.size() && i < v.bodies.size(); ++i)
            v.kept[keep_key(*v.model, v.cut[i])] = ImVec2(v.bodies[i].x, v.bodies[i].y);
        v.kept_open.clear();
        for (std::uint32_t id : v.open_order) v.kept_open.push_back(keep_key(*v.model, id));
    }
    const bool first = !v.model;
    v.model = std::move(next);
    const auto& m = *v.model;
    const std::uint32_t count = m.tree.count();
    v.open.assign(count, 0);
    v.open_order.clear();
    v.pos.assign(count, ImVec2(0, 0));
    v.placed.assign(count, 0);
    v.cut.clear();
    v.bodies.clear();
    std::unordered_map<std::string, std::uint32_t> by_key;
    if (!v.kept.empty() || !v.kept_open.empty())
        for (std::uint32_t id = 0; id < count; ++id) {
            const std::string k = keep_key(m, id);
            if (auto it = v.kept.find(k); it != v.kept.end()) {
                v.pos[id] = it->second;
                v.placed[id] = 1;
            }
            if (!m.tree.is_node(id)) by_key.emplace(k, id);
        }
    v.kept.clear();
    for (const auto& k : v.kept_open)
        if (auto it = by_key.find(k); it != by_key.end()) {
            v.open[it->second] = 1;
            v.open_order.push_back(it->second);
        }
    v.kept_open.clear();
    v.prox.clear();
    v.prox_on = false;
    recut(v, first);
}

} // namespace

void HormigaApp::draw_physics_view(float /*body_h*/) {
    ConnectionsView& v = conn;
    namespace cx = hormiga::connections;

    // ── the model: rebuilt off the UI thread when the scene changes ─────────
    const std::uint64_t key = cx::key_of(scene);
    if (v.job.valid() && v.job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto next = v.job.get();
        if (next) adopt(v, std::move(next));
    }
    if ((!v.model || v.model->key != key) && !(v.job.valid() && v.job_key == key)) {
        cx::Graph g = cx::gather(scene); // O(scene), once per change
        v.job_key = key;
        v.job_started = ImGui::GetTime();
        v.job = std::async(std::launch::async, [g = std::move(g), key]() mutable {
            return std::make_shared<const cx::Model>(cx::build(std::move(g), key));
        });
    }

    // ── toolbar ─────────────────────────────────────────────────────────────
    if (ImGui::SmallButton("Re-run layout") && v.model) {
        std::fill(v.placed.begin(), v.placed.end(), 0);
        v.cut.clear();
        v.bodies.clear();
        recut(v, true);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Close all") && v.model && !v.open_order.empty()) {
        for (std::uint32_t r : v.model->tree.roots)
            if (v.open[r]) close_group(v, r);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) v.fit = true;
    ImGui::SameLine();
    const bool partial = wstore.partial();
    if (v.model) {
        const auto& m = *v.model;
        const std::uint64_t linked = m.graph.names.size();
        ImGui::TextDisabled("%s linked runes in %s bodies%s", with_commas(linked).c_str(),
                            with_commas(v.cut.size()).c_str(),
                            m.tree.count() > m.tree.items ? " | click a group to open it, right-click to close" : "");
        if (m.graph.stub_total) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.85f, 0.55f, 0.15f, 1.0f), "| %s links lead outside what is loaded",
                               with_commas(m.graph.stub_total).c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(partial ? "Their other ends are in parts of the database that are not loaded. "
                                            "Select a rune and load the part around it to follow them."
                                          : "Their other ends are not runes in this database.");
        }
    }
    if (partial && !ed.selection.empty() && v.model && v.model->graph.index.count(*ed.selection.begin())) {
        const std::string sel = *ed.selection.begin();
        const auto i = v.model->graph.index.at(sel);
        if (v.model->graph.stubs[i]) {
            ImGui::SameLine();
            if (ImGui::SmallButton(("Load the part around " + sel).c_str())) {
                load_part(kDataMantle, "", sel);
                return;
            }
        }
    }
    if (!v.model) {
        ImGui::TextDisabled("finding the groups in %s runes...", with_commas(scene.nodes.size()).c_str());
        return;
    }
    const auto& m = *v.model;
    const auto& t = m.tree;
    if (m.graph.names.empty()) {
        ImGui::TextDisabled("no connections yet: link two runes and they appear here");
        return;
    }

    // ── proximity springs, only for a graph drawn rune by rune ──────────────
    const bool want_prox = (map_show_prox || time_prox) && t.count() == t.items && t.items <= 2000;
    if (want_prox != v.prox_on) {
        v.prox.clear();
        v.prox_on = want_prox;
        if (want_prox) {
            std::vector<std::pair<std::uint32_t, std::pair<double, double>>> loc;
            std::vector<std::pair<std::uint32_t, long>> dated;
            for (std::uint32_t i = 0; i < t.items; ++i) {
                const maiz::SceneNode* n = scene.find(m.graph.names[i]);
                if (!n) continue;
                double la, lo;
                if (map_show_prox && hormiga::parse_geo(hormiga::temper::field_value(*n, "geo"), la, lo))
                    loc.push_back({i, {la, lo}});
                int y, mo, dd;
                if (time_prox &&
                    std::sscanf(hormiga::temper::field_value(*n, "date").c_str(), "%d-%d-%d", &y, &mo, &dd) == 3)
                    dated.push_back({i, (long)y * 372 + mo * 31 + dd});
            }
            for (size_t a = 0; a < loc.size(); ++a)
                for (size_t b = a + 1; b < loc.size(); ++b) {
                    const double d = hormiga::geo_distance_m(loc[a].second.first, loc[a].second.second,
                                                             loc[b].second.first, loc[b].second.second);
                    if (d <= map_prox_m)
                        v.prox.push_back({v.body_of[loc[a].first], v.body_of[loc[b].first],
                                          0.25f * (1.0f - (float)(d / map_prox_m))});
                }
            for (size_t a = 0; a < dated.size(); ++a)
                for (size_t b = a + 1; b < dated.size(); ++b) {
                    const long dt = std::labs(dated[a].second - dated[b].second);
                    if (dt <= (long)time_prox_days)
                        v.prox.push_back({v.body_of[dated[a].first], v.body_of[dated[b].first],
                                          0.3f * (1.0f - (float)dt / (float)time_prox_days)});
                }
        }
        std::vector<ov::Link> tied = v.shown; // a spring ties two pieces into one
        tied.insert(tied.end(), v.prox.begin(), v.prox.end());
        v.comp = ov::components((std::uint32_t)v.cut.size(), tied);
        v.iters = 0;
        v.total = 160;
        v.temp = 24.0f;
    }

    // ── settle, within a few milliseconds a frame ───────────────────────────
    if (v.iters < v.total) {
        /* A spring pulls with the log of the links it stands for: two groups
         * sharing 800 links are close, not 800 times closer (that piled every
         * linked group into one heap at 6×10^4 runes). */
        std::vector<ov::Link> springs = v.shown;
        for (auto& l : springs) l.w = 1.0f + std::log2(l.w);
        if (!v.prox.empty()) springs.insert(springs.end(), v.prox.begin(), v.prox.end());
        ov::LayoutParams lp;
        lp.spacing = t.count() > t.items ? 110.0f : 70.0f; // groups are drawn larger than runes
        const auto t0 = std::chrono::steady_clock::now();
        do {
            const float f = 1.0f - (float)v.iters / (float)v.total;
            ov::layout_step(v.bodies, springs, 2.0f + v.temp * f * f, lp, &v.comp);
            ++v.iters;
        } while (v.iters < v.total &&
                 std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(6));
        ov::pack(v.bodies, v.comp, v.radius, 24.0f);
    }

    // ── the canvas (a collapsed pane must not feed ImGui a <=0 size) ────────
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 16 || sz.y < 16) return;
    if (v.fit && v.iters >= v.total && !v.bodies.empty()) { // everything in view, once it has settled
        float x0 = v.bodies[0].x, x1 = x0, y0 = v.bodies[0].y, y1 = y0;
        for (const auto& b : v.bodies) {
            x0 = std::min(x0, b.x);
            x1 = std::max(x1, b.x);
            y0 = std::min(y0, b.y);
            y1 = std::max(y1, b.y);
        }
        v.zoom = std::clamp(std::min(sz.x / (x1 - x0 + 120.0f), sz.y / (y1 - y0 + 120.0f)), 0.05f, 4.0f);
        v.cam = ImVec2(-(x0 + x1) * 0.5f * v.zoom, -(y0 + y1) * 0.5f * v.zoom);
        v.fit = false;
    }
    const ImVec2 ctr(p0.x + sz.x * 0.5f + v.cam.x, p0.y + sz.y * 0.5f + v.cam.y);
    dl->PushClipRect(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), true);
    ImGui::InvisibleButton("##phys-surface", sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    auto to_s = [&](const ov::Body& b) { return ImVec2(ctr.x + b.x * v.zoom, ctr.y + b.y * v.zoom); };

    for (const auto& e : v.shown) { // links, thicker for many
        const float w = std::min(4.0f, 1.0f + 0.6f * std::log2(e.w));
        dl->AddLine(to_s(v.bodies[e.a]), to_s(v.bodies[e.b]), IM_COL32(110, 125, 145, e.w > 1 ? 150 : 120), w);
    }
    for (const auto& e : v.prox)
        dl->AddLine(to_s(v.bodies[e.a]), to_s(v.bodies[e.b]), IM_COL32(110, 125, 145, 45), 1.0f);

    int hit = -1;
    for (std::size_t i = 0; i < v.cut.size(); ++i) {
        const std::uint32_t id = v.cut[i];
        const ImVec2 s = to_s(v.bodies[i]);
        const float r = body_radius(m, id) * v.zoom;
        if (s.x + r < p0.x || s.y + r < p0.y || s.x - r > p0.x + sz.x || s.y - r > p0.y + sz.y) continue;
        if (t.is_node(id)) {
            const std::string& name = m.graph.names[id];
            const std::string& g = m.graph.glyphs[id];
            const bool selected = ed.selected(name);
            const ImU32 col = g == "incident"       ? IM_COL32(200, 50, 50, 255)
                              : g == "organization" ? IM_COL32(138, 109, 59, 255)
                              : g == "event"        ? IM_COL32(63, 111, 174, 255)
                              : g == "image"        ? IM_COL32(125, 91, 176, 255)
                              : g == "job"          ? IM_COL32(93, 125, 59, 255)
                                                    : IM_COL32(179, 89, 46, 255);
            dl->AddCircleFilled(s, r + (selected ? 2.0f : 0.0f), col);
            if (selected) dl->AddCircle(s, r + 3.5f, IM_COL32(30, 30, 30, 255), 0, 2.0f);
            if (m.graph.degree[id] >= 3 || selected || v.zoom > 1.4f)
                dl->AddText(ImVec2(s.x + r + 3, s.y - 8), IM_COL32(40, 40, 40, 255), name.c_str());
        } else {
            const std::uint8_t kind = t.kind[id];
            const ImU32 fill = kind == ov::Tree::Community ? IM_COL32(96, 128, 168, 200)
                               : kind == ov::Tree::Slice   ? IM_COL32(120, 146, 180, 170)
                                                           : IM_COL32(150, 160, 175, 70);
            dl->AddCircleFilled(s, r, fill);
            dl->AddCircle(s, r, IM_COL32(60, 80, 110, 220), 0, kind == ov::Tree::Bundle ? 1.0f : 1.6f);
            const std::string count = with_commas(t.size[id]);
            const ImVec2 ts = ImGui::CalcTextSize(count.c_str());
            if (ts.x < r * 1.8f) // only where it fits: a crowd of numbers reads as noise
                dl->AddText(ImVec2(s.x - ts.x * 0.5f, s.y - ts.y * 0.5f), IM_COL32(20, 25, 35, 255), count.c_str());
            if (r > 14.0f && kind != ov::Tree::Bundle)
                dl->AddText(ImVec2(s.x + r + 3, s.y - 8), IM_COL32(60, 60, 60, 255), label_of(m, id).c_str());
        }
        if (m.stubs[id]) { // the mojones: links that go on past what is loaded
            const std::string out = "+" + with_commas(m.stubs[id]);
            dl->AddText(ImVec2(s.x + r * 0.7f, s.y - r * 0.7f - 12), IM_COL32(200, 120, 30, 255), out.c_str());
        }
        const float dx = mouse.x - s.x, dy = mouse.y - s.y;
        if (hovered && dx * dx + dy * dy < (r + 4) * (r + 4)) hit = (int)i;
    }
    if (hit >= 0 && hovered) {
        const std::uint32_t id = v.cut[(std::size_t)hit];
        if (t.is_node(id))
            ImGui::SetTooltip("%s (%s) - %u connection(s)%s", m.graph.names[id].c_str(), m.graph.glyphs[id].c_str(),
                              m.graph.degree[id],
                              m.stubs[id] ? (", and " + with_commas(m.stubs[id]) + " to runes not loaded").c_str()
                                          : "");
        else
            ImGui::SetTooltip("%s runes%s, around %s\n%s%s", with_commas(t.size[id]).c_str(),
                              t.kind[id] == ov::Tree::Community ? " that link to each other"
                              : t.kind[id] == ov::Tree::Slice   ? " (a slice of one large group)"
                                                                : " in small groups with no links between them",
                              label_of(m, id).c_str(), "click: open",
                              m.stubs[id] ? (" | " + with_commas(m.stubs[id]) + " links lead outside what is loaded").c_str()
                                          : "");
    }

    if (v.iters < v.total || (v.job.valid() && v.job_key != m.key)) { // settling, or regrouping
        const bool regroup = v.job.valid() && v.job_key != m.key;
        const float frac = regroup ? 0.0f : (float)v.iters / (float)v.total;
        const ImVec2 ba(p0.x + 10, p0.y + 8), bb(p0.x + 230, p0.y + 22);
        dl->AddRectFilled(ba, bb, IM_COL32(255, 255, 255, 220), 4);
        if (!regroup) dl->AddRectFilled(ba, ImVec2(ba.x + 220 * frac, bb.y), IM_COL32(90, 140, 200, 200), 4);
        char msg[64];
        if (regroup) std::snprintf(msg, sizeof msg, "regrouping after a change...");
        else std::snprintf(msg, sizeof msg, "computing layout %d%%", (int)(frac * 100));
        dl->AddText(ImVec2(ba.x + 8, ba.y), IM_COL32(40, 40, 40, 255), msg);
    }

    // ── input: drag a body / pan / zoom / open / close / select ─────────────
    if (ImGui::IsItemActivated() && hit >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) v.drag = hit;
    if (ImGui::IsItemDeactivated()) {
        if (v.drag >= 0 && v.drag < (int)v.bodies.size()) v.bodies[(std::size_t)v.drag].pinned = false;
        v.drag = -1;
    }
    bool dragged = false;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        if (v.drag >= 0 && v.drag < (int)v.bodies.size()) {
            auto& b = v.bodies[(std::size_t)v.drag];
            b.pinned = true;
            b.x += d.x / v.zoom;
            b.y += d.y / v.zoom;
            if (v.iters >= v.total) { // let the rest follow it
                v.iters = 0;
                v.total = 60;
                v.temp = 8.0f;
            }
        } else {
            v.cam.x += d.x;
            v.cam.y += d.y;
        }
        dragged = true;
    }
    if (hit >= 0 && !dragged && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && hovered &&
        ImGui::GetIO().MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16.0f) {
        const std::uint32_t id = v.cut[(std::size_t)hit];
        if (t.is_node(id)) ed.selection = {m.graph.names[id]};
        else if (!open_group(v, id))
            toast("that group has too many parts to show at once: close another group first", true);
    }
    if (hit >= 0 && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        const std::uint32_t p = t.parent[v.cut[(std::size_t)hit]];
        if (p != ov::Tree::kNone && v.open[p]) close_group(v, p);
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0)
        v.zoom = std::clamp(v.zoom * (ImGui::GetIO().MouseWheel > 0 ? 1.15f : 0.87f), 0.05f, 4.0f);
    dl->PopClipRect();
}
