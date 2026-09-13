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
#include "domain/date_query.hpp" // query_matches, today_days — the ONE evaluation
#include "render/icon_set.hpp"   // the one icon vocabulary
#include "voidmaiz/code.hpp"

#include <algorithm>
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
        if (!rm.empty()) pending_cmds.push_back(maiz::compile_commit(rm));
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
            pending_cmds.push_back(maiz::compile_commit(rs));
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
void HormigaApp::ensure_icon_editor() {
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
