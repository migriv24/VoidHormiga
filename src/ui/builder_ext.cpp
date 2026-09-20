/* ui/builder_ext.cpp — the Builder additions of 2026-09-13.
 *
 * Written while the author was making a newsletter, from their list:
 *
 *   1. shift-select multiple (for multi delete or multi drag)
 *   2. image previews in the event flier that also show the tags
 *   4. easier icon selection, and icons on narrative bits
 *   5. an explicit image + text block, with a preview
 *   7. open a tag filter in the expression editor, for exact AND / OR / NOT
 *
 * A separate file because `builder.cpp` was fifty lines from its budget in
 * `tools/find_long.py`, and because these are the seams that file already has:
 * a card's PREVIEW is separable from the canvas that lays cards out, and an
 * inspector EDITOR is separable from the inspector. `builder.cpp` keeps the
 * layout and calls in.
 */
#include "app/app_internal.hpp"
#include "render/text.hpp" // rank_tokens: the ordering fields' grammar
#include "domain/date_query.hpp" // query_matches, today_days — the ONE evaluation
#include "render/icon_set.hpp"   // the one icon vocabulary
#include "voidmaiz/code.hpp"

#include <algorithm>
#include <ctime>
#include <array>
#include <map>
#include <cctype>
#include <set>
#include <stdexcept>

namespace {

bool word_char(char c) {
    return std::isalnum((unsigned char)c) || c == ':' || c == '-' || c == '_' ||
           c == '.' || c == '@';
}

/* Which runes a query-backed block is ABOUT, so the editor can count them. */
std::string scope_glyph(const std::string& block) {
    if (block == "image_grid") return "image";
    if (block == "job_grid") return "job";
    if (block == "directory") return "contact";
    return "event";
}

} // namespace

// ── 2 + 5: previews for the blocks that fell through to their glyph name ────
/* Before this, `event_flier` and `event_feature` had no canvas case at all and
 * drew the literal word "event_flier" — so choosing a flier and tagging it, the
 * whole point of those blocks, produced nothing to look at until a render. The
 * image cache (`texture_for`) already existed and already drew a hero's banner
 * and an image grid's thumbnails; these blocks simply never asked it. */
bool HormigaApp::draw_block_preview(const maiz::SceneNode& n, const maiz::Scene& data,
                                    float inner_w, unsigned acc) {
    if (n.glyph != "image_text" && n.glyph != "event_flier" &&
        n.glyph != "event_feature")
        return false;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const std::string suf = preview_lang ? "_es" : "_en";
    const std::string alt = preview_lang ? "_en" : "_es";
    const ImVec4 dark(0.15f, 0.15f, 0.18f, 1.0f);

    auto tx = [&](const maiz::SceneNode& m, const char* base) {
        std::string v = field_value(m, std::string(base) + suf);
        return v.empty() ? field_value(m, std::string(base) + alt) : v;
    };
    auto find_in = [&](const std::string& nm) -> const maiz::SceneNode* {
        if (nm.empty()) return nullptr;
        for (const auto& d : data.nodes)
            if (d.name == nm) return &d;
        return nullptr;
    };
    auto thumb_w = [&](const std::string& path, float max_w, float h) -> float {
        if (path.empty()) return 0.0f;
        HostTexture t = texture_for(path);
        return t.id ? std::min(max_w, h * (float)t.w / (float)std::max(1, t.h))
                    : h * 0.75f;
    };
    auto thumb_at = [&](const std::string& path, ImVec2 p, float w, float h) {
        HostTexture t = texture_for(path);
        if (t.id) {
            dl->AddImage((ImTextureID)(intptr_t)t.id, p, ImVec2(p.x + w, p.y + h));
        } else { // named but not on this machine: say so, do not draw nothing
            dl->AddRect(p, ImVec2(p.x + w, p.y + h), IM_COL32(170, 90, 90, 200), 4.0f);
            dl->AddText(ImVec2(p.x + 6, p.y + h * 0.5f - 7),
                        IM_COL32(170, 90, 90, 255), "missing");
        }
    };
    /* THE TAGS, which is what the author asked to see: *"at least so i can see
     * something of what comes up from the tags."* `type:` is housekeeping every
     * rune carries and would crowd out the tags a person actually chose. */
    auto chips = [&](const maiz::SceneNode& m, float x0, float y, float w) {
        float x = x0;
        int shown = 0;
        for (const auto& t : m.tags) {
            if (t.rfind("type:", 0) == 0) continue;
            const ImVec2 sz = ImGui::CalcTextSize(t.c_str());
            if (x + sz.x + 10 > x0 + w) break;
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + sz.x + 8, y + sz.y + 2),
                              (acc & 0x00FFFFFF) | 0x33000000, 6.0f);
            dl->AddText(ImVec2(x + 4, y + 1), IM_COL32(70, 70, 76, 255), t.c_str());
            x += sz.x + 12;
            if (++shown >= 6) break;
        }
    };
    /* The flier a page would show: the one named, else an image WIRED to the
     * event in this preview's language, else any wired image — the same order
     * `render_site` resolves it in, so the canvas and the page agree. */
    auto flier_of = [&](const maiz::SceneNode& ev,
                        const std::string& named) -> const maiz::SceneNode* {
        if (const maiz::SceneNode* f = find_in(named); f && f->glyph == "image")
            return f;
        const std::string want = std::string("lang:") + (preview_lang ? "es" : "en");
        const maiz::SceneNode* any = nullptr;
        for (const auto& w : data.wires) {
            const std::string other =
                w.from == ev.name ? w.to : (w.to == ev.name ? w.from : std::string());
            const maiz::SceneNode* im = find_in(other);
            if (!im || im->glyph != "image") continue;
            if (std::find(im->tags.begin(), im->tags.end(), want) != im->tags.end())
                return im;
            if (!any) any = im;
        }
        return any;
    };

    const ImVec2 p0 = ImGui::GetCursorScreenPos();

    if (n.glyph == "image_text") {
        const std::string ip = field_value(n, "image");
        const bool right = field_value(n, "side") == "right";
        const float ih = 96.0f;
        const float iw = thumb_w(ip, inner_w * 0.42f, ih);
        const float gap = iw > 0 ? 10.0f : 0.0f;
        if (iw > 0) thumb_at(ip, ImVec2(right ? p0.x + inner_w - iw : p0.x, p0.y), iw, ih);
        const float tx0 = right ? p0.x : p0.x + iw + gap;
        const float tw = std::max(40.0f, inner_w - iw - gap);
        ImGui::SetCursorScreenPos(ImVec2(tx0, p0.y));
        ImGui::PushTextWrapPos(tx0 + tw);
        const std::string ic = field_value(n, "icon");
        const std::string head = tx(n, "heading");
        if (!head.empty() || !ic.empty())
            ImGui::TextColored(dark, "%s%s%s", fa_icon_for(ic), ic.empty() ? "" : "  ",
                               head.c_str());
        std::string body = tx(n, "text");
        if (body.size() > 170) { // clip on a character, not inside one
            size_t cut = 167;
            while (cut > 0 && ((unsigned char)body[cut] & 0xC0) == 0x80) --cut;
            body = body.substr(0, cut) + "...";
        }
        ImGui::TextDisabled("%s", body.empty() ? "(write the text in the inspector)"
                                               : body.c_str());
        if (ip.empty()) ImGui::TextDisabled("(pick an image in the inspector)");
        ImGui::PopTextWrapPos();
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + ih + 2));
        ImGui::Dummy(ImVec2(1, 1));
        return true;
    }

    // event_flier / event_feature
    const maiz::SceneNode* ev = find_in(field_value(n, "event"));
    if (!ev || ev->glyph != "event") {
        ImGui::TextDisabled("%s  %s", ICON_FA_CALENDAR,
                            n.glyph == "event_flier"
                                ? "event + flier: pick an event in the inspector"
                                : "featured event: pick an event in the inspector");
        return true;
    }
    std::string ipath;
    if (n.glyph == "event_flier") {
        if (const maiz::SceneNode* f = flier_of(*ev, field_value(n, "flier")))
            ipath = field_value(*f, "path");
    } else {
        ipath = field_value(n, "image");
    }
    const float ih = 104.0f;
    const float iw = thumb_w(ipath, inner_w * 0.40f, ih);
    if (iw > 0) thumb_at(ipath, p0, iw, ih);
    const float tx0 = p0.x + iw + (iw > 0 ? 10.0f : 0.0f);
    const float tw = std::max(40.0f, inner_w - (tx0 - p0.x));
    ImGui::SetCursorScreenPos(ImVec2(tx0, p0.y));
    ImGui::PushTextWrapPos(tx0 + tw);
    std::string title = tx(*ev, "title");
    if (title.empty()) title = ev->name;
    ImGui::TextColored(dark, "%s  %s", ICON_FA_CALENDAR, title.c_str());
    std::string when = field_value(*ev, "date");
    const std::string st = field_value(*ev, "start_time");
    if (!st.empty()) when += (when.empty() ? "" : "  ") + st;
    if (when.empty()) when = field_value(*ev, "days");
    if (!when.empty()) ImGui::TextDisabled("%s", when.c_str());
    const std::string venue = field_value(*ev, "venue");
    if (!venue.empty()) ImGui::TextDisabled("%s %s", ICON_FA_LOCATION_DOT, venue.c_str());
    ImGui::PopTextWrapPos();
    const float cy = ImGui::GetCursorScreenPos().y + 2;
    chips(*ev, tx0, cy, tw);
    if (n.glyph == "event_flier" && ipath.empty())
        dl->AddText(ImVec2(tx0, cy + 22), IM_COL32(170, 90, 90, 255),
                    "no flier linked - wire an image to this event");
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + ih + 2));
    ImGui::Dummy(ImVec2(1, 1));
    return true;
}

// ── 1: a group of components, acted on at once ─────────────────────────────
void HormigaApp::draw_multi_select_panel() {
    const int count = (int)ed.selection.size();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(theme_accent[0], theme_accent[1], theme_accent[2], 1.0f),
                       "%s  %d selected", ICON_FA_CHECK, count);
    ImGui::SameLine();
    /* ONE BATCH, so the whole group comes back with one Ctrl+Z. Removing eight
     * components as eight commands would take eight undos to reverse, which is
     * the kind of cost that teaches a person not to try the feature twice. */
    if (ImGui::SmallButton("Remove all")) {
        std::vector<std::string> rm;
        for (const auto& s : ed.selection)
            if (const maiz::SceneNode* sn = scene.find(s); sn && sn->glyph != "page")
                rm.push_back("doc remove " + s);
        if (const std::string b = doc_batch(rm); !b.empty()) pending_cmds.push_back(b);
        ed.selection.clear();
        return;
    }
    static const struct { const char* label; int span; } kWidths[] = {
        {"Full", 12}, {"Half", 6}, {"Third", 4}};
    for (const auto& w : kWidths) {
        ImGui::SameLine();
        if (ImGui::SmallButton(w.label)) {
            std::vector<std::string> rs;
            for (const auto& s : ed.selection)
                rs.push_back("doc resize " + s + " " + std::to_string(w.span));
            if (const std::string b = doc_batch(rs); !b.empty()) pending_cmds.push_back(b);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) ed.selection.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("shift/ctrl-click adds or removes - Delete removes - "
                        "drag moves the group");
}

// ── 4: the icon picker, as an inspector editor ─────────────────────────────
/* Registered as an editor KIND rather than drawn by the Builder, the way the
 * "image" editor is (app.cpp): one registration, and every field declared
 * `"icon":"icon"` gets a picker wherever the inspector renders it. Lazily, from
 * the canvas, so this needed no line in `app.cpp`, which is at its budget. */
/* ── ORDER, AS A FILTER'S SIBLING (2026-09-15) ───────────────────────────────
 *
 * The author, on `rank_up` / `rank_down`: *"the GUI part of it should be pretty
 * similar to the filter tags ... i still want that smart search for tags. i want
 * the GUI stuf to easily delete tags or move them arround. in fact, its very
 * similar to a filter. but where the filter determines what even shows up, these
 * list order determine the order of things."*
 *
 * So it is built as one: tags searched from the DATA vocabulary as you type (the
 * vocabulary the filter offers, not the document's), each shown with how many
 * entries carry it. Unlike a filter the list is ORDERED, because order is the
 * meaning here (render/text.hpp: an earlier tag outweighs every later one), so
 * the tags are a numbered column: drag a row onto another to move it, or use the
 * arrows; x removes it. Every change is one `set`, and the value stays the plain
 * `leader, board` an agent writes from the CLI.
 *
 * An editor KIND, registered beside the icon picker, so every field declared
 * `"taglist"` gets it wherever the inspector draws that field. */
namespace {
struct RankVocab {
    double at = -100.0;
    std::map<std::string, int> counts; // tag -> how many data runes carry it
};
RankVocab g_rank_vocab;
std::map<std::string, std::array<char, 64>> g_rank_bufs; // "rune/field" -> the add box
} // namespace

/* One ordered tag list: the chips, the reordering and the smart search. Shared by
 * the Builder's Order section and the `taglist` editor kind; its commands go to
 * `cmds`, and `core` is passed in so this stays a plain function. */
static bool rank_list_ui(maiz::Core& core, const maiz::SceneNode& n, const std::string& key,
                         std::vector<std::string>& cmds) {
        const bool down = key.find("down") != std::string::npos;
        std::vector<std::string> tags =
            rank_tokens(hormiga::temper::field_value(n, key.c_str()));
        bool committed = false;
        auto commit = [&](const std::vector<std::string>& ts) {
            std::string v;
            for (size_t i = 0; i < ts.size(); ++i) v += (i ? ", " : "") + ts[i];
            cmds.push_back("set " + n.name + " " + key + " " + json_str(v));
            committed = true;
        };

        // the data vocabulary and its counts, re-read every two seconds at most
        const double now = ImGui::GetTime();
        if (now - g_rank_vocab.at > 2.0) {
            g_rank_vocab = RankVocab{};
            g_rank_vocab.at = now;
            maiz::ProjectOptions dpo;
            dpo.mantle = kDataMantle;
            const maiz::Scene dsc = maiz::project_scene(core, dpo);
            for (const auto& dn : dsc.nodes)
                for (const auto& tg : dn.tags) ++g_rank_vocab.counts[tg];
        }
        // the renderer's rule: a bare tag also matches under any namespace
        auto carriers = [](const std::string& t) {
            int c = 0;
            const bool bare = t.find(':') == std::string::npos;
            for (const auto& [tag, k] : g_rank_vocab.counts) {
                const size_t colon = tag.rfind(':');
                if (tag == t || (bare && colon != std::string::npos &&
                                 tag.compare(colon + 1, std::string::npos, t) == 0))
                    c += k;
            }
            return c;
        };

        ImGui::PushID(key.c_str());
        const ImVec4 col = down ? ImVec4(0.72f, 0.47f, 0.06f, 1.0f)
                                : ImVec4(0.18f, 0.50f, 0.26f, 1.0f);
        ImGui::TextColored(col, "%s", down ? ICON_FA_ARROW_DOWN "  List last"
                                           : ICON_FA_ARROW_UP "  List first");
        const char* payload = down ? "HORMIGA_RANK_DOWN" : "HORMIGA_RANK_UP";
        int move_from = -1, move_to = -1, remove = -1;
        for (int i = 0; i < (int)tags.size(); ++i) {
            ImGui::PushID(i);
            ImGui::PushStyleColor(ImGuiCol_Button, col);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImVec4(col.x + 0.08f, col.y + 0.08f, col.z + 0.08f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            const std::string chip = std::string(ICON_FA_GRIP_VERTICAL "  ") +
                                     std::to_string(i + 1) + ".  " + tags[(size_t)i];
            ImGui::Button(chip.c_str());
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                ImGui::SetTooltip("drag onto another tag to move it");
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload(payload, &i, sizeof i);
                ImGui::Text("%s", tags[(size_t)i].c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(payload)) {
                    move_from = *(const int*)p->Data;
                    move_to = i;
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%d", carriers(tags[(size_t)i]));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("entries in the data that carry this tag");
            ImGui::SameLine();
            ImGui::BeginDisabled(i == 0);
            if (ImGui::SmallButton(ICON_FA_ARROW_UP)) { move_from = i; move_to = i - 1; }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 == (int)tags.size());
            if (ImGui::SmallButton(ICON_FA_ARROW_DOWN)) { move_from = i; move_to = i + 1; }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_XMARK)) remove = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("remove");
            ImGui::PopID();
        }
        if (move_from >= 0 && move_to >= 0 && move_from != move_to &&
            move_from < (int)tags.size() && move_to < (int)tags.size()) {
            const std::string t = tags[(size_t)move_from];
            tags.erase(tags.begin() + move_from);
            tags.insert(tags.begin() + move_to, t);
            commit(tags);
        } else if (remove >= 0) {
            tags.erase(tags.begin() + remove);
            commit(tags);
        }

        // the smart search: tags already in the data, as you type
        auto& buf = g_rank_bufs[n.name + "/" + key];
        ImGui::SetNextItemWidth(-1);
        const bool enter = ImGui::InputTextWithHint(
            "##rankadd", down ? "+ a tag to list last..." : "+ a tag to list first...",
            buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue);
        std::string typed = buf.data();
        typed.erase(std::remove_if(typed.begin(), typed.end(),
                                   [](char c) { return c == ' ' || c == ','; }),
                    typed.end()); // a tag has neither, and both separate the list
        if (!typed.empty()) {
            std::string chosen;
            bool exact = false;
            int shown = 0;
            for (const auto& [tg, k] : g_rank_vocab.counts) {
                if (!contains_ci(tg, typed)) continue;
                if (tg == typed) exact = true;
                if (std::find(tags.begin(), tags.end(), tg) != tags.end()) continue;
                if (++shown > 8) {
                    ImGui::TextDisabled("(keep typing...)");
                    break;
                }
                if (ImGui::Selectable(
                        ("@" + tg + "  (" + std::to_string(k) + ")##rk" + tg).c_str()))
                    chosen = tg;
            }
            if (shown == 0) ImGui::TextDisabled("no tag in the data matches");
            if (!exact &&
                ImGui::Selectable(("+ use \"" + typed + "\" anyway##rknew").c_str()))
                chosen = typed;
            if (enter && chosen.empty()) chosen = typed;
            if (!chosen.empty()) {
                if (std::find(tags.begin(), tags.end(), chosen) == tags.end()) {
                    tags.push_back(chosen);
                    commit(tags);
                }
                buf[0] = 0;
            }
        }
        ImGui::TextDisabled(down ? "the first tag sinks furthest; ties keep name order"
                                 : "the first tag rises highest; ties keep name order");
        ImGui::PopID();
        return committed;
}

/* The Builder's ORDER section, directly under Filter, because they are one idea
 * in two halves: the filter decides what a block shows, this decides the order
 * it shows it in. Drawn here rather than left to the generic field list, where
 * the author found two plain text boxes at the bottom (2026-09-15). */
/* ── A GROUP OF `doc` COMMANDS AS ONE UNDO (2026-09-15) ───────────────────────
 *
 * The author: *"multi select doesn't multi delete things. i cant select multiple
 * then delete multiple."* Every group action — Delete, "Remove N selected",
 * "Remove all", the widths — built `doc remove a`, `doc remove b` and handed the
 * lines to `maiz::compile_commit`, which makes one Void Core batch of them. But
 * `doc` is not a Void Core verb: it is this application's, expanded by
 * `try_doc_verb` one command at a time. A batch of them went straight to the core,
 * which knows no `doc`, and the whole group failed. A single remove worked because
 * a lone command passes through `try_doc_verb` first.
 *
 * So the expansion happens here, action by action, against the same scene, and the
 * core commands they produce are committed together: still one Ctrl+Z for the
 * group, which is what the batch was for. Lines that are not `doc` pass through. */
std::string HormigaApp::doc_batch(const std::vector<std::string>& cmds) {
    std::vector<std::string> core_cmds;
    for (const auto& c : cmds) {
        const std::vector<std::string> tok = tokenize(c);
        if (tok.size() < 2 || tok[0] != "doc") {
            core_cmds.push_back(c);
            continue;
        }
        const maiz::ActionDescriptor* a = doc_actions.find(tok[1]);
        if (!a) continue;
        maiz::ActionArgs args;
        size_t ti = 2;
        for (const auto& p : a->params)
            if (ti < tok.size()) args[p.name] = tok[ti++];
        for (auto& x : doc_actions.run(tok[1], scene, args)) core_cmds.push_back(std::move(x));
    }
    return core_cmds.empty() ? std::string() : maiz::compile_commit(core_cmds);
}

void HormigaApp::draw_block_extras(const maiz::SceneNode& sel) {
    /* THE FEATURED EVENT, CHOSEN (2026-09-15). The author: *"for the 'featured
     * event' there should be an easier way to just pick a singular featured event.
     * not necesarily pick on by tags."* `event` was a hidden field with no control
     * in its place. This is that control: every event, upcoming first and soonest
     * at the top, then the undated, then the past, each with its date, searchable.
     * One click sets `event`. */
    if (sel.glyph == "event_feature" || sel.glyph == "event_flier") {
        ImGui::SeparatorText(sel.glyph == "event_flier" ? "Event (and its flier)"
                                                        : "Featured event");
        maiz::ProjectOptions dpo;
        dpo.mantle = kDataMantle;
        const maiz::Scene dsc = maiz::project_scene(core, dpo);
        const std::string cur = field_value(sel, "event");
        char today[16];
        const std::time_t now = std::time(nullptr);
        std::strftime(today, sizeof today, "%Y-%m-%d", std::localtime(&now));
        struct Ev {
            const maiz::SceneNode* n;
            std::string date, label;
            int bucket; // 0 upcoming, 1 no date, 2 past
        };
        std::vector<Ev> evs;
        for (const auto& dn : dsc.nodes) {
            if (dn.glyph != "event") continue;
            Ev e{&dn, field_value(dn, "date"), {}, 0};
            e.bucket = e.date.empty() ? 1 : (e.date >= today ? 0 : 2);
            std::string t = field_value(dn, "title_en");
            if (t.empty()) t = field_value(dn, "title");
            if (t.empty()) t = dn.name;
            e.label = (e.date.empty() ? std::string("no date   ") : e.date + "   ") + t;
            evs.push_back(std::move(e));
        }
        std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
            if (a.bucket != b.bucket) return a.bucket < b.bucket;
            if (a.bucket == 0) return a.date < b.date;
            if (a.bucket == 2) return a.date > b.date;
            return a.label < b.label;
        });
        std::string shown_as = cur.empty() ? std::string("(choose an event)") : cur;
        for (const auto& e : evs)
            if (e.n->name == cur) shown_as = e.label;
        static char ev_search[64] = {};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##featuredevent", shown_as.c_str(), ImGuiComboFlags_HeightLarge)) {
            if (ImGui::IsWindowAppearing()) {
                ev_search[0] = 0;
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##featuredeventq", "search events...", ev_search,
                                     sizeof ev_search);
            int shown = 0, last = -1;
            for (const auto& e : evs) {
                if (ev_search[0] && !contains_ci(e.label, ev_search)) continue;
                if (e.bucket != last) {
                    ImGui::TextDisabled("%s", e.bucket == 0 ? "upcoming"
                                              : e.bucket == 1 ? "no date" : "past");
                    last = e.bucket;
                }
                if (++shown > 300) break;
                if (ImGui::Selectable((e.label + "##" + e.n->name).c_str(), e.n->name == cur))
                    pending_cmds.push_back("set " + sel.name + " event " + json_str(e.n->name));
            }
            if (shown == 0) ImGui::TextDisabled("no event matches");
            ImGui::EndCombo();
        }
        if (evs.empty()) ImGui::TextDisabled("there are no events in the data yet");
        ImGui::Spacing();
    }

    bool ranked = false;
    for (const auto& f : sel.fields)
        if (f.key == "rank_up") ranked = true;
    if (!ranked) return;
    ImGui::SeparatorText("Order (who comes first)");
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::TextDisabled("The filter decides what shows; this decides the order. "
                        "Name order stays underneath.");
    ImGui::PopTextWrapPos();
    rank_list_ui(core, sel, "rank_up", pending_cmds);
    ImGui::Spacing();
    rank_list_ui(core, sel, "rank_down", pending_cmds);
    ImGui::Spacing();
}

void HormigaApp::ensure_icon_editor() {
    if (!widgets.editors.count("taglist"))
        widgets.editors["taglist"] = [this](maiz::WidgetContext& ctx, const maiz::SceneNode& n,
                                            const maiz::SceneField& f, std::string_view) {
            return rank_list_ui(core, n, f.key, ctx.commands);
        };
    if (widgets.editors.count("icon")) return;
    widgets.editors["icon"] = [](maiz::WidgetContext& ctx, const maiz::SceneNode& n,
                                 const maiz::SceneField& f, std::string_view) -> bool {
        const std::string cur = hormiga::temper::field_value(n, f.key.c_str());
        if (!f.label.empty()) ImGui::TextUnformatted(f.label.c_str());
        bool committed = false;
        auto commit = [&](const std::string& v) {
            ctx.commands.push_back("set " + n.name + " " + f.key + " \"" + v + "\"");
            committed = true;
        };
        // the current choice, NAMED — a glyph alone is not always legible
        ImGui::TextDisabled("%s  %s", fa_icon_for(cur),
                            cur.empty() ? "(no icon)" : hormiga::iconset::label(cur));
        const float cell = ImGui::GetFrameHeight() + 6.0f;
        const float avail = ctx.width > 0 ? ctx.width : ImGui::GetContentRegionAvail().x;
        const int per_row = std::max(
            4, (int)(avail / (cell + ImGui::GetStyle().ItemSpacing.x)));
        ImGui::PushID(f.key.c_str());
        if (ImGui::Button("none", ImVec2(0, cell))) commit("");
        int i = 1;
        for (const auto& e : hormiga::iconset::kIcons) {
            if (i++ % per_row != 0) ImGui::SameLine();
            const bool on = cur == e.name;
            if (on)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button((std::string(fa_icon_for(e.name)) + "##" + e.name).c_str(),
                              ImVec2(cell, cell)))
                commit(e.name);
            if (on) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", e.label);
        }
        ImGui::PopID();
        return committed;
    };
}

// ── 7: the filter, as an expression ────────────────────────────────────────
/* The author: *"any filter section with the tags should have a little button …
 * but it opens up the filter in the allomone text editor instead. This is if we
 * want more precise control over exactly which tags we are wanting to select and
 * how, especially useful with the 'and' and 'or' statements. Idk if allomone
 * supports parenthesis right now."*
 *
 * The grammar needed nothing. A block query is Void Core's tag grammar — AND /
 * OR / NOT, `&&` `||` `!`, PARENTHESES, implicit AND — plus our `date:`
 * predicates, and it has supported grouping all along. What was missing was a
 * place to write one: the chip builder can only say "all of" or "any of", and
 * its fallback was a single-line text box with no feedback at all.
 *
 * So this is Void Maiz's code editor — the widget the Allomone tab uses, not
 * the Allomone LANGUAGE, which is a rules language and a different grammar —
 * with a highlighter, tag completion from the data the block is about, and a
 * LIVE COUNT computed by `query_matches`, the same evaluation the renderers
 * run. A person can see "matches 4 of 31" change as they add a parenthesis,
 * which is the feedback that makes precise control usable rather than merely
 * possible. */
void HormigaApp::open_tag_expr_editor(const maiz::SceneNode& sel) {
    tagexpr_target = sel.name;
    tagexpr_editor.set_text(field_value(sel, "query"));
    tagexpr_editor.clear_dirty();
    tagexpr_open_req = true;
}

void HormigaApp::draw_tag_expr_editor(const maiz::SceneNode& sel) {
    if (tagexpr_open_req && tagexpr_target == sel.name) {
        ImGui::OpenPopup("Filter expression");
        tagexpr_open_req = false;
    }
    ImGui::SetNextWindowSize(ImVec2(580, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Filter expression", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize))
        return;

    const std::string scope = scope_glyph(sel.glyph);
    maiz::ProjectOptions dio;
    dio.mantle = kDataMantle;
    const maiz::Scene data = maiz::project_scene(core, dio);
    std::set<std::string> tagset;
    std::vector<const maiz::SceneNode*> pool;
    for (const auto& d : data.nodes) {
        const bool in_scope =
            d.glyph == scope || (scope == "contact" && d.glyph == "organization");
        if (!in_scope) continue;
        pool.push_back(&d);
        for (const auto& t : d.tags) tagset.insert(t);
    }

    ImGui::PushTextWrapPos(560);
    ImGui::TextDisabled(
        "Exactly which %ss this block shows. Combine tags with AND, OR and NOT "
        "(or && || !) and group them with parentheses. date:future, date:past, "
        "date:today, date:recurring and date:undated work too. Ctrl+Space "
        "suggests tags.",
        scope.c_str());
    ImGui::TextDisabled("  e.g.   (kw:food OR kw:clothing) AND date:future");
    ImGui::PopTextWrapPos();

    maiz::CodeEditorOptions opts;
    opts.allow_zoom = false;
    opts.highlight = [](std::string_view s) {
        std::vector<maiz::CodeSpan> out;
        size_t i = 0;
        while (i < s.size()) {
            const char c = s[i];
            if (c == '(' || c == ')') { out.push_back({i, i + 1, 0xe0a050}); ++i; continue; }
            if (c == '!') { out.push_back({i, i + 1, 0x6aa0ff}); ++i; continue; }
            if ((c == '&' || c == '|') && i + 1 < s.size() && s[i + 1] == c) {
                out.push_back({i, i + 2, 0x6aa0ff});
                i += 2;
                continue;
            }
            if (c == '"') {
                size_t j = s.find('"', i + 1);
                j = j == std::string_view::npos ? s.size() : j + 1;
                out.push_back({i, j, 0xc8a070});
                i = j;
                continue;
            }
            if (word_char(c)) {
                size_t j = i;
                while (j < s.size() && word_char(s[j])) ++j;
                const std::string_view w = s.substr(i, j - i);
                unsigned col = 0x7fd08a; // a tag
                if (w == "AND" || w == "OR" || w == "NOT") col = 0x6aa0ff;
                else if (w.rfind("date:", 0) == 0) col = 0xc792ea;
                out.push_back({i, j, col});
                i = j;
                continue;
            }
            ++i;
        }
        return out;
    };
    opts.complete = [&tagset](std::string_view s, size_t caret) {
        maiz::CompletionSet cs;
        caret = std::min(caret, s.size());
        size_t b = caret;
        while (b > 0 && word_char(s[b - 1])) --b;
        cs.replace_begin = b;
        cs.replace_end = caret;
        const std::string_view pre = s.substr(b, caret - b);
        if (pre.empty()) return cs;
        auto offer = [&](const std::string& t, const char* detail) {
            if (t.size() > pre.size() && std::string_view(t).substr(0, pre.size()) == pre)
                cs.items.push_back({t, "", detail});
        };
        for (const char* k : {"AND", "OR", "NOT", "date:future", "date:past",
                              "date:today", "date:recurring", "date:undated"})
            offer(k, "keyword");
        for (const auto& t : tagset) {
            if (cs.items.size() >= 24) break;
            offer(t, "tag");
        }
        return cs;
    };
    maiz::code_editor("##tagexpr_ed", tagexpr_editor, opts, 560, 110);

    const std::string expr = tagexpr_editor.text;
    std::string error;
    int depth = 0;
    bool unbalanced = false;
    for (char c : expr) {
        if (c == '(') ++depth;
        else if (c == ')' && --depth < 0) unbalanced = true;
    }
    if (depth != 0) unbalanced = true;
    if (!expr.empty()) {
        /* `query_matches` degrades a malformed expression to "match everything"
         * — right for a page, which should fail full rather than blank, and
         * wrong here, where the whole job is to say whether it parses. So the
         * grammar is asked directly, against an empty bag. */
        try {
            (void)maiz::Core::tag_match(expr, std::vector<std::string>{});
        } catch (const std::invalid_argument& e) {
            error = e.what();
        }
    }
    std::vector<const maiz::SceneNode*> hits;
    if (error.empty()) {
        const long long today = hormiga::today_days();
        for (const auto* d : pool)
            if (hormiga::query_matches(expr, data, *d, today)) hits.push_back(d);
    }

    if (!error.empty())
        ImGui::TextColored(ImVec4(0.85f, 0.4f, 0.4f, 1), "%s  not valid yet: %s",
                           ICON_FA_TRIANGLE_EXCLAMATION, error.c_str());
    else if (unbalanced)
        ImGui::TextColored(ImVec4(0.9f, 0.65f, 0.3f, 1), "%s  the parentheses do not balance",
                           ICON_FA_TRIANGLE_EXCLAMATION);
    else
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 0.45f, 1), "%s  matches %d of %d %ss%s",
                           ICON_FA_CHECK, (int)hits.size(), (int)pool.size(),
                           scope.c_str(), expr.empty() ? "  (empty = everything)" : "");
    for (size_t i = 0; i < hits.size() && i < 6; ++i) {
        std::string t = field_value(*hits[i], "title_en");
        if (t.empty()) t = field_value(*hits[i], "display_name");
        if (t.empty()) t = hits[i]->name;
        ImGui::TextDisabled("   %s", t.c_str());
    }
    if (hits.size() > 6) ImGui::TextDisabled("   ... and %d more", (int)hits.size() - 6);

    ImGui::Spacing();
    /* APPLY IS A BUTTON AND NOTHING ELSE. The editor reports a commit when focus
     * leaves it after an edit — which is also what happens on the way to the
     * Cancel button, so honouring that would apply the very edit a person was
     * reaching to throw away. */
    const bool can_apply = error.empty() && !unbalanced;
    ImGui::BeginDisabled(!can_apply);
    if (ImGui::Button("Apply")) {
        pending_cmds.push_back("set " + sel.name + " query " + json_str(expr));
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

/* ── WINDOWS INSIDE A TAB (2026-09-15) ───────────────────────────────────────
 *
 * A section's tab hosts its own dockspace, and its panels are real windows
 * docked into it: resizable, tabbable, floatable, remembered in imgui.ini. The
 * arrangement below is seeded ONCE, when the node has no saved layout, so a
 * person's own rearrangement wins forever after - the same rule the main
 * dockspace in app.cpp follows. Pass nullptr for a slot a section does not use. */
void HormigaApp::nested_dockspace(const char* id, const char* top, const char* left,
                                  const char* center, const char* right) {
#ifdef IMGUI_HAS_DOCK
    const ImGuiID dock = ImGui::GetID(id);
    if (ImGui::DockBuilderGetNode(dock) == nullptr) {
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 200.0f || size.y < 150.0f) size = ImVec2(1200.0f, 800.0f);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, size);
        ImGuiID c = dock;
        if (top)
            ImGui::DockBuilderDockWindow(
                top, ImGui::DockBuilderSplitNode(c, ImGuiDir_Up, 0.16f, nullptr, &c));
        if (left)
            ImGui::DockBuilderDockWindow(
                left, ImGui::DockBuilderSplitNode(c, ImGuiDir_Left, 0.17f, nullptr, &c));
        if (right)
            ImGui::DockBuilderDockWindow(
                right, ImGui::DockBuilderSplitNode(c, ImGuiDir_Right, 0.34f, nullptr, &c));
        if (center) ImGui::DockBuilderDockWindow(center, c);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::DockSpace(dock, ImVec2(0, 0));
#else
    (void)id; (void)top; (void)left; (void)center; (void)right;
#endif
}

/* ── THE BLOCKS, BY WHAT THEY ARE (2026-09-15) ───────────────────────────────
 *
 * The author: *"the blocks window should be color coded, and the section names
 * should be like: content, data, media, generated ... We have normal content
 * blocks which includes the narrative stuff (things like footers, dividers, and
 * quotes should go here instead). We have data blocks which directly calls for
 * stuff from the database using tags. We have media blocks for video, audio, and
 * other media things ... Then we have the generative blocks, like the map and
 * calender. maybe a better word would be 'interactive'? Its a tough decision."*
 *
 * Four groups, and the fourth is called INTERACTIVE. The author named the
 * difficulty exactly: a map is interactive on a website and a picture in a
 * newsletter, and an event grid gets a search box on the website too. What
 * separates the last group is not that it moves — it is that the block IS a
 * widget: a whole map or a whole calendar, which the newsletter degrades to a
 * static stand-in. The tooltip on each heading says so in the person's words
 * rather than leaving the word to carry it alone.
 *
 * The palette was a column of identical grey buttons whose only sections were
 * "Content, Data, Content, Data" — the registration order, repeated. The list is
 * sorted by group once, in app.cpp, and each group has a colour. */
static ImVec4 category_color(const std::string& c) {
    if (c == "Content") return ImVec4(0.30f, 0.59f, 1.00f, 1.0f);
    if (c == "Data") return ImVec4(0.60f, 0.40f, 0.80f, 1.0f);
    if (c == "Media") return ImVec4(0.85f, 0.51f, 0.17f, 1.0f);
    if (c == "Interactive") return ImVec4(0.18f, 0.58f, 0.42f, 1.0f);
    return ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
}

static const char* category_about(const std::string& c) {
    if (c == "Content") return "what you write: headings, prose, quotes, buttons, dividers";
    if (c == "Data") return "filled from the database by tags: events, postings, people, galleries";
    if (c == "Media") return "a video, a recording or a file, shown or offered as it is";
    if (c == "Interactive")
        return "the block IS a widget - a whole map or calendar.\n"
               "Interactive on the website; a picture or a list in the newsletter.";
    return "";
}

void HormigaApp::draw_blocks_palette() {
    std::string last_cat;
    for (const auto& e : palette_blocks.entries) {
        if (e.category != last_cat) {
            const ImVec4 cc = category_color(e.category);
            ImGui::PushStyleColor(ImGuiCol_Text, cc);
            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(cc.x, cc.y, cc.z, 0.5f));
            ImGui::SeparatorText(e.category.c_str());
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", category_about(e.category));
            last_cat = e.category;
        }
        /* ── THE ICON GOES ON THE BUTTON, AND ON THE DRAG GHOST (2026-09-02) ─
         *
         * The author asked for *"little icons next to the drag and drop
         * button"*. The palette is a column of same-width buttons whose only
         * differentiator was a word, which is exactly the case an icon earns
         * its place in: at a glance, `image grid` and `event grid` are the same
         * shape and the same length, and a picture is not.
         *
         * `glyph_icon` (app_internal.hpp) maps the glyph to a Font Awesome
         * codepoint already merged into the ImGui atlas. Unlisted glyphs get a
         * neutral square rather than nothing, so a new block looks sparse
         * instead of broken.
         *
         * The DRAG GHOST gets it too. That ghost is the only thing visible
         * while a person is deciding where to drop, so it is the one place the
         * icon is doing the most work. */
        const std::string plabel =
            std::string(glyph_icon(e.glyph)) + "  " + e.label;
        const ImVec4 cc = category_color(e.category);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(cc.x, cc.y, cc.z, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(cc.x, cc.y, cc.z, 0.42f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(cc.x, cc.y, cc.z, 0.60f));
        bool clicked = ImGui::Button(plabel.c_str(), ImVec2(-1, 0));
        ImGui::PopStyleColor(3);
        // DRAG a palette element onto the document (author's one missing
        // nicety, 2026-07-23): drop between rows in the doc canvas to insert
        // AT a position; the click still appends.
        if (builder_doc_view &&
            ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("PALETTE_GLYPH", e.glyph.c_str(),
                                      e.glyph.size() + 1);
            ImGui::Text("%s  %s", glyph_icon(e.glyph), e.label.c_str());
            ImGui::EndDragDropSource();
        }
        if (clicked) {
            if (builder_doc_view) {
                doc_palette_place(e.glyph); // append via the `doc place` verb
            } else {
                // interim click-to-mint: lands under the lowest block
                const std::string name = mint_name(e.glyph);
                float maxb = 60.0f;
                for (const auto& n : scene.nodes)
                    maxb = std::max(maxb, n.y + n.h);
                dispatch_and_reproject(
                    maiz::compile_add(e.glyph, name, 80.0f, maxb + 50.0f));
                ed.selection = {name};
            }
        }
    }
}
