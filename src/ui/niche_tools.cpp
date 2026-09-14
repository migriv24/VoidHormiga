/* ui/niche_tools.cpp — once-in-a-while utilities, in a window that is off by
 * default.
 *
 * The author (2026-09-13): *"a new tab 'niche tools' — a window that is off by
 * default … we won't worry about the ui/ux stuff for the niche tools page,
 * because they're probably used once, or not really all that much."* So this is
 * deliberately plain: collapsing headers, buttons, text. Nothing here is on the
 * path an organization walks every week.
 *
 * WHAT PROMPTED IT. The dev build was launched with no database named, so it
 * opened `demo-org.json` in the folder it was started from — the SOURCE TREE —
 * and an organization's live edits and a deploy record landed there instead of
 * in its own database. The same trap had done it once before 2026-09-01. An
 * agent working in that organization's folder found it and proposed the repair
 * by hand: merge the stray copy back, check the report first, move the
 * leftover files. The author's answer was that this should be part of the
 * system:
 *
 *   *"it would be useful to have a json merge feature with detections, testing,
 *    automatic path changes in antfarm and such."*
 *
 * So there are three things here, plus a QR code generator the author simply
 * wanted:
 *
 *   WHERE        which database is open, and a loud warning when it is inside
 *                the source tree (also shown in the menu bar, where the
 *                warning would actually have been seen)
 *   MERGE        Void Palabra's merge — the SAME call as `effect sync-merge` —
 *                with a preview that writes nothing and detections a person
 *                needs before saying yes: is this the same database or a
 *                different one, what conflicts, which runes exist on only one
 *                side, which referenced files are only in the OTHER folder, and
 *                which Antfarm paths still point there. Apply goes through
 *                `gui_sync_effect`, exactly what the console runs, so there is
 *                one merge in the application, not two.
 *   QR           a link in, a QR code out, saved as a PNG.
 */
#include "app/app_internal.hpp"
#include "domain/glyphs_antfarm.hpp" // to read the OTHER copy's Antfarm paths
#include "domain/qr.hpp"
#include "sync/merge.hpp"
#include "stb_image_write.h" // decls only — the ONE implementation is in app.cpp

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace {

bool looks_like_source_tree(const fs::path& dir) {
    std::error_code ec;
    return fs::exists(dir / "CMakeLists.txt", ec) &&
           fs::exists(dir / "src" / "app" / "app.cpp", ec) &&
           fs::exists(dir / "okf" / "index.md", ec);
}

std::string slurp_text(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/* Relative data-folder paths a state document mentions, found by scanning its
 * text for JSON string values that begin with a data folder's name. Crude on
 * purpose: it needs no knowledge of which glyph keeps a path in which field, so
 * it cannot miss a field added next month — and a false positive only ever
 * produces a line saying a file is where it should be. */
std::set<std::string> referenced_data_paths(const std::string& text) {
    static const char* kRoots[] = {"assets/", "documents/", "templates/", "exports/"};
    std::set<std::string> out;
    for (const char* root : kRoots) {
        const size_t rl = std::strlen(root);
        size_t pos = 0;
        while ((pos = text.find(root, pos)) != std::string::npos) {
            if (pos > 0 && text[pos - 1] == '"') {
                const size_t end = text.find('"', pos);
                if (end != std::string::npos && end - pos > rl && end - pos < 400)
                    out.insert(text.substr(pos, end - pos));
            }
            pos += rl;
        }
    }
    return out;
}

struct PathFix {
    std::string node, field, from, to;
};

struct MergeTool {
    char path[1024] = {};
    bool previewed = false;
    std::string error;
    hormiga::sync::MergeResult r;
    fs::path peer_dir;
    int shared = 0;                   // runes both copies hold
    std::vector<std::string> missing; // referenced, present ONLY in the other folder
    std::vector<std::string> absent;  // referenced, present in neither folder
    std::vector<PathFix> fixes;       // Antfarm paths that point into the other folder
    std::string applied_version;      // what the open database should become
    std::string check;                // the result of "Check the result"
};
MergeTool g_merge;

struct QrTool {
    char text[1024] = "https://";
    int ecc = 1;
    int border = 4;
    int scale = 10;
    hormiga::qr::Matrix m;
    std::string made_for;
    int made_ecc = -1, made_border = -1;
};
QrTool g_qr;

const ImVec4 kRed(0.90f, 0.35f, 0.30f, 1.0f);
const ImVec4 kAmber(0.85f, 0.60f, 0.15f, 1.0f);
const ImVec4 kGreen(0.35f, 0.70f, 0.40f, 1.0f);

} // namespace

bool HormigaApp::db_in_source_tree() const {
    static std::string cached_for;
    static bool cached = false;
    const std::string key = base_dir.string();
    if (key != cached_for) {
        cached_for = key;
        cached = looks_like_source_tree(base_dir);
    }
    return cached;
}

/* In the MENU BAR, on every frame, because that is the one place it would have
 * been seen. A warning that lives only in a window nobody opens protects
 * nobody — and the first time this went wrong, two days of edits went into the
 * wrong copy before anyone noticed. */
void HormigaApp::draw_source_tree_banner() {
    if (!db_in_source_tree()) return;
    ImGui::SameLine(0, 18);
    ImGui::PushStyleColor(ImGuiCol_Text, kRed);
    ImGui::TextUnformatted(ICON_FA_TRIANGLE_EXCLAMATION
                           "  this database is inside the Hormiga SOURCE folder");
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(
            "Hormiga was started without a database named, so it opened\n"
            "demo-org.json in the folder it was launched from - the code.\n"
            "An organization's real data must not live here.\n\n"
            "Open your organization's database instead (File > Open, or\n"
            "start Hormiga with its path). Click for Niche Tools > Merge if\n"
            "work already went into this copy.");
    if (ImGui::IsItemClicked()) win_niche_tools = true;
}

void HormigaApp::draw_niche_tools_body() {
    ImGui::TextDisabled("Once-in-a-while utilities. Deliberately plain.");
    ImGui::Separator();

    // ── WHERE ───────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Where is this database?", ImGuiTreeNodeFlags_DefaultOpen)) {
        const fs::path doc = base_dir / state_name;
        ImGui::TextWrapped("Open: %s", doc.string().c_str());
        if (db_in_source_tree()) {
            ImGui::PushStyleColor(ImGuiCol_Text, kRed);
            ImGui::TextWrapped(
                "This is inside the Hormiga source folder. Hormiga opens "
                "demo-org.json in whatever folder it was started from when no "
                "database is named, and started from here that folder is the code. "
                "Anything saved, ingested, rendered or deployed right now lands "
                "beside the code instead of in your organization's own folder.");
            ImGui::PopStyleColor();
            ImGui::TextWrapped(
                "Fix: open your organization's database (File > Open), or start "
                "Hormiga with it named - VoidHormiga.bat \"C:\\path\\to\\org.state.json\". "
                "If work already went into this copy, open the REAL database and "
                "merge this one into it below.");
        } else {
            ImGui::TextDisabled("This database lives in its own folder.");
        }
        if (ImGui::SmallButton("Show this folder") && on_open) on_open(base_dir.string());
    }

    // ── MERGE ───────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Merge another copy into this database",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped(
            "Folds another copy of a database (a .state.json) into the one that "
            "is open, with Void Palabra's merge - the same as the console's "
            "`effect sync-merge`. Preview writes nothing. Nothing changes until "
            "Apply, and Apply is one undoable replacement you still have to Save.");
        ImGui::SetNextItemWidth(-90);
        if (ImGui::InputTextWithHint("##mergepath", "path to the other copy's .state.json",
                                     g_merge.path, sizeof g_merge.path))
            g_merge.previewed = false;
        ImGui::SameLine();
        if (ImGui::SmallButton("Browse") && on_pick_file) {
            const std::string p = on_pick_file("");
            if (!p.empty()) {
                std::snprintf(g_merge.path, sizeof g_merge.path, "%s", p.c_str());
                g_merge.previewed = false;
            }
        }

        if (ImGui::Button("Preview (writes nothing)")) {
            const std::string keep(g_merge.path);
            g_merge = MergeTool{};
            std::snprintf(g_merge.path, sizeof g_merge.path, "%s", keep.c_str());
            g_merge.previewed = true;
            std::error_code ec;
            const fs::path pp = fs::absolute(fs::path(keep), ec);
            const fs::path mine = fs::absolute(base_dir / state_name, ec);
            if (keep.empty()) {
                g_merge.error = "name a file first";
            } else if (!fs::exists(pp, ec)) {
                g_merge.error = "no such file: " + pp.string();
            } else if (fs::equivalent(pp, mine, ec)) {
                g_merge.error = "that is the database you have open";
            } else if (pp.extension() == ".miga") {
                g_merge.error =
                    "a .miga bundle has no detailed preview here - run it in the "
                    "console instead: effect sync-merge \"" + pp.string() +
                    "\" (it prints the same report and writes nothing without apply)";
            } else {
                const std::string text = slurp_text(pp);
                /* The SAME prefixes `sync_op` uses, so the version this preview
                 * predicts is the version Apply produces. */
                g_merge.r = hormiga::sync::merge_states(core.export_state(), text,
                                                        "local:" + state_name,
                                                        "file:" + pp.string());
                if (!g_merge.r.ok) {
                    g_merge.error = "the merge refused: " + g_merge.r.error;
                } else {
                    g_merge.peer_dir = pp.parent_path();
                    g_merge.shared =
                        std::max(0, g_merge.r.runes - (int)g_merge.r.one_sided.size());

                    // files the other copy refers to, and where they actually are
                    for (const auto& rel : referenced_data_paths(text)) {
                        const bool here = fs::exists(base_dir / rel, ec);
                        const bool there = fs::exists(g_merge.peer_dir / rel, ec);
                        if (!here && there) g_merge.missing.push_back(rel);
                        else if (!here && !there) g_merge.absent.push_back(rel);
                    }

                    /* Antfarm paths that are ABSOLUTE and point into the other
                     * copy's folder. A relative path already means "beside the
                     * database", wherever the database is; an absolute one into
                     * the other folder keeps publishing from, or storing into,
                     * the place the data is being moved OUT of. */
                    maiz::Core peer(text);
                    hormiga::register_antfarm_glyphs(peer);
                    maiz::ProjectOptions ao;
                    ao.mantle = kAntfarmMantle;
                    const maiz::Scene farm = maiz::project_scene(peer, ao);
                    static const char* kPathFields[] = {"file", "dir", "dump_dir",
                                                        "key_file", "secret_file",
                                                        "token_file"};
                    const std::string prefix =
                        fs::weakly_canonical(g_merge.peer_dir, ec).generic_string();
                    for (const auto& node : farm.nodes)
                        for (const char* f : kPathFields) {
                            const std::string v = field_value(node, f);
                            if (v.empty() || !fs::path(v).is_absolute()) continue;
                            const std::string g =
                                fs::weakly_canonical(fs::path(v), ec).generic_string();
                            if (!prefix.empty() && g.rfind(prefix + "/", 0) == 0)
                                g_merge.fixes.push_back(
                                    {node.name, f, v, g.substr(prefix.size() + 1)});
                        }
                }
            }
        }

        if (g_merge.previewed) {
            if (!g_merge.error.empty()) {
                ImGui::TextColored(kRed, "%s", g_merge.error.c_str());
            } else {
                const auto& r = g_merge.r;
                ImGui::Separator();
                ImGui::Text("this database  %s", r.version_local.c_str());
                ImGui::Text("the other copy %s", r.version_remote.c_str());
                ImGui::Text("after merging  %s  (%d runes, %d mantles)",
                            r.version_merged.c_str(), r.runes, r.mantles);

                if (r.identical) {
                    ImGui::TextColored(kGreen, "%s  identical - there is nothing to merge",
                                       ICON_FA_CHECK);
                } else {
                    /* DETECTION: the same database, diverged — or two different
                     * ones? Merging a copy back is the job this tool exists for.
                     * Merging an unrelated database combines two organizations,
                     * which is almost never meant and never obvious afterwards. */
                    const double ratio = r.runes > 0 ? (double)g_merge.shared / r.runes : 0.0;
                    if (ratio >= 0.5)
                        ImGui::TextColored(kGreen,
                                           "%s  the same database, diverged: %d of %d "
                                           "runes are shared",
                                           ICON_FA_CHECK, g_merge.shared, r.runes);
                    else
                        ImGui::TextColored(kRed,
                                           "%s  these look like DIFFERENT databases: only "
                                           "%d of %d runes are shared. Merging would "
                                           "combine two organizations' data.",
                                           ICON_FA_TRIANGLE_EXCLAMATION, g_merge.shared,
                                           r.runes);
                }
                if (looks_like_source_tree(g_merge.peer_dir))
                    ImGui::TextColored(kAmber,
                                       "the other copy is inside the Hormiga source "
                                       "folder - the usual shape of this mistake");

                int only_here = 0, only_there = 0;
                for (const auto& o : r.one_sided) (o.on_local ? only_here : only_there)++;
                if (ImGui::TreeNode("##conf", "%d conflict(s) - fields both copies changed",
                                    (int)r.conflicts.size())) {
                    for (const auto& c : r.conflicts) {
                        std::string sides;
                        for (const auto& v : c.sides) sides += (sides.empty() ? "" : "  |  ") + v;
                        ImGui::BulletText("%s / %s  %s:  %s", c.mantle.c_str(), c.rune.c_str(),
                                          c.field.c_str(), sides.c_str());
                    }
                    ImGui::TextDisabled("both values are kept as a conflict for you to settle");
                    ImGui::TreePop();
                }
                if (ImGui::TreeNode("##one", "%d rune(s) only here, %d only in the other copy",
                                    only_here, only_there)) {
                    int shown = 0;
                    for (const auto& o : r.one_sided) {
                        if (++shown > 200) { ImGui::TextDisabled("..."); break; }
                        ImGui::BulletText("%s  %s / %s", o.on_local ? "here " : "other",
                                          o.mantle.c_str(), o.name.c_str());
                    }
                    ImGui::TreePop();
                }

                // DETECTION: files that would be referenced but are not here
                if (!g_merge.missing.empty()) {
                    ImGui::TextColored(kAmber,
                                       "%d file(s) the other copy uses are only in its "
                                       "folder (%s)",
                                       (int)g_merge.missing.size(),
                                       g_merge.peer_dir.string().c_str());
                    if (ImGui::TreeNode("##missing", "which files")) {
                        for (const auto& m : g_merge.missing) ImGui::BulletText("%s", m.c_str());
                        ImGui::TreePop();
                    }
                    if (ImGui::SmallButton("Copy them into this database's folder")) {
                        int copied = 0, failed = 0;
                        std::error_code ec;
                        for (const auto& rel : g_merge.missing) {
                            const fs::path dst = base_dir / rel;
                            fs::create_directories(dst.parent_path(), ec);
                            ec.clear();
                            fs::copy_file(g_merge.peer_dir / rel, dst,
                                          fs::copy_options::skip_existing, ec);
                            if (ec) ++failed; else ++copied;
                        }
                        toast("copied " + std::to_string(copied) + " file(s)" +
                                  (failed ? ", " + std::to_string(failed) + " failed" : ""),
                              failed > 0);
                        g_merge.missing.clear();
                    }
                }
                if (!g_merge.absent.empty() &&
                    ImGui::TreeNode("##absent", "%d referenced file(s) are in neither folder",
                                    (int)g_merge.absent.size())) {
                    for (const auto& m : g_merge.absent) ImGui::BulletText("%s", m.c_str());
                    ImGui::TreePop();
                }

                // DETECTION: Antfarm paths still pointing at the other folder
                if (!g_merge.fixes.empty()) {
                    ImGui::TextColored(kAmber,
                                       "%d Antfarm path(s) point into the other copy's folder",
                                       (int)g_merge.fixes.size());
                    for (const auto& f : g_merge.fixes)
                        ImGui::BulletText("%s.%s: %s  ->  %s", f.node.c_str(), f.field.c_str(),
                                          f.from.c_str(), f.to.c_str());
                    /* Only once the merge is applied: before that these nodes are
                     * the OTHER copy's, and a `set` now would either miss (a node
                     * this database lacks) or be overwritten by Apply's
                     * replacement of the whole document a moment later. */
                    ImGui::BeginDisabled(g_merge.applied_version.empty());
                    const bool rewrite =
                        ImGui::SmallButton("Rewrite them relative to this folder");
                    ImGui::EndDisabled();
                    if (g_merge.applied_version.empty()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(after Apply)");
                    }
                    if (rewrite) {
                        std::vector<std::string> cmds;
                        for (const auto& f : g_merge.fixes)
                            cmds.push_back("set " + f.node + " " + f.field + " " + json_str(f.to));
                        pending_cmds.push_back(maiz::compile_commit(cmds));
                        toast("rewrote " + std::to_string(cmds.size()) +
                              " Antfarm path(s) - one undoable step");
                    }
                }

                ImGui::Separator();
                ImGui::BeginDisabled(r.identical);
                if (ImGui::Button("Apply the merge")) {
                    std::string res;
                    const std::string args =
                        "{\"args\":[" + json_str(g_merge.path) + ",\"apply\"]}";
                    if (gui_sync_effect("sync-merge", args, res)) {
                        g_merge.applied_version = r.version_merged;
                        g_merge.check.clear();
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                /* THE TEST: after Apply, the open database's version name must be
                 * exactly the one the preview predicted. Two canonical encodings
                 * that agree are the whole proof; anything else is reported. */
                ImGui::BeginDisabled(g_merge.applied_version.empty());
                if (ImGui::Button("Check the result")) {
                    const std::string now = hormiga::sync::version_name(core.export_state());
                    g_merge.check = now == g_merge.applied_version
                                        ? "verified: the open database is exactly the "
                                          "merged result (" + now + ") - now Save"
                                        : "NOT the predicted result: expected " +
                                              g_merge.applied_version + ", found " + now;
                }
                ImGui::EndDisabled();
                if (!g_merge.check.empty())
                    ImGui::TextColored(g_merge.check.rfind("verified", 0) == 0 ? kGreen : kRed,
                                       "%s", g_merge.check.c_str());
            }
        }
    }

    // ── QR ──────────────────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("QR code from a link", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##qrtext", "https://...", g_qr.text, sizeof g_qr.text);
        ImGui::SetNextItemWidth(170);
        ImGui::Combo("error correction", &g_qr.ecc,
                     "Low (7%)\0Medium (15%)\0Quartile (25%)\0High (30%)\0");
        ImGui::SetNextItemWidth(170);
        ImGui::SliderInt("quiet border", &g_qr.border, 0, 8);
        if (g_qr.made_for != g_qr.text || g_qr.made_ecc != g_qr.ecc ||
            g_qr.made_border != g_qr.border) {
            const std::string t = g_qr.text;
            g_qr.m = (t == "https://" || t == "http://")
                         ? hormiga::qr::Matrix{0, {}, "type a link"}
                         : hormiga::qr::make(t, (hormiga::qr::Ecc)g_qr.ecc, g_qr.border);
            g_qr.made_for = g_qr.text;
            g_qr.made_ecc = g_qr.ecc;
            g_qr.made_border = g_qr.border;
        }
        const std::string t = g_qr.text;
        if (!t.empty() && t.rfind("http://", 0) != 0 && t.rfind("https://", 0) != 0)
            ImGui::TextColored(kAmber, "not a web link - a phone will show this as text");
        if (g_qr.border < 4)
            ImGui::TextDisabled("a border under 4 modules can stop a scanner finding it");

        if (!g_qr.m.error.empty()) {
            ImGui::TextColored(kRed, "%s", g_qr.m.error.c_str());
        } else {
            const float avail = ImGui::GetContentRegionAvail().x;
            const float box = std::min(avail, 300.0f);
            const float mod = std::max(1.0f, std::floor(box / (float)g_qr.m.size));
            const float side = mod * (float)g_qr.m.size;
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + side, p0.y + side), IM_COL32(255, 255, 255, 255));
            for (int y = 0; y < g_qr.m.size; ++y)
                for (int x = 0; x < g_qr.m.size; ++x)
                    if (g_qr.m.at(x, y))
                        dl->AddRectFilled(ImVec2(p0.x + x * mod, p0.y + y * mod),
                                          ImVec2(p0.x + (x + 1) * mod, p0.y + (y + 1) * mod),
                                          IM_COL32(0, 0, 0, 255));
            ImGui::Dummy(ImVec2(side, side));
            ImGui::TextDisabled("%d x %d modules", g_qr.m.size, g_qr.m.size);

            ImGui::SetNextItemWidth(170);
            ImGui::SliderInt("pixels per module (PNG)", &g_qr.scale, 2, 24);
            const int px = g_qr.m.size * g_qr.scale;
            if (ImGui::Button("Save PNG")) {
                std::error_code ec;
                fs::create_directories(data_dir("exports"), ec);
                char stamp[32];
                const std::time_t now = std::time(nullptr);
                std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
                const fs::path out = data_dir("exports") / ("qr-" + std::string(stamp) + ".png");
                const auto rgb = hormiga::qr::to_rgb(g_qr.m, g_qr.scale);
                if (stbi_write_png(out.string().c_str(), px, px, 3, rgb.data(), px * 3)) {
                    toast("saved " + out.filename().string() + " (" + std::to_string(px) +
                          " px)");
                    if (on_open) on_open(out.parent_path().string());
                } else {
                    toast("could not write " + out.string(), true);
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%d x %d px", px, px);
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy link")) ImGui::SetClipboardText(g_qr.text);
        }
    }
}
