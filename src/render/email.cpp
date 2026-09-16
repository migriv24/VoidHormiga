/* render/email.cpp — the NEWSLETTER, in the email domain.
 *
 * ── THIS FILE IS A RECONSTRUCTION (2026-08-20), AND THAT IS WORTH KNOWING ────
 *
 * A `split_tu` invocation that had appeared to hang on shell quoting was still
 * alive in the background. It executed later, against an ALREADY-SPLIT
 * `render/site.cpp`, and moved 646 lines of `render_site` into this file —
 * overwriting `render_preview` entirely. `site.cpp` was recovered exactly,
 * because its cut text was sitting here intact. This half was not: the
 * overwrite destroyed the only copy.
 *
 * What it was rebuilt FROM, so a reader can judge how much to trust it:
 *   - `tests/headless_smoke.sh`, which pins eleven of this renderer's
 *     behaviours and every one of them was written after a real newsletter went
 *     out wrong;
 *   - `render/text.hpp`, which survived and holds every shared helper —
 *     linkify, prose, clip, email_button, human_date, title_of's fallback rule;
 *   - `render/site.cpp`, the sibling domain, for the block vocabulary and the
 *     order the chain is walked in.
 *
 * The BEHAVIOUR is pinned. The exact byte output is not, so the golden's
 * `exports/preview-en.html` hash was re-captured rather than matched — see
 * okf/log.md. Anything here that reads as new is new; the intent was to restore
 * what the tests describe, not to improve on it while pretending otherwise.
 *
 * ── what the email domain IS ─────────────────────────────────────────────────
 *
 * Table layout, inlined styles, public image URLs. A mail client strips flex
 * and grid and cannot fetch a local file, so this is not the website with
 * different CSS — it is a different machine with different physics. Everything
 * both domains share lives in `render/text.hpp`, shared by dependency rather
 * than by proximity, which is what stops the two drifting the way they did on
 * 2026-08-19.
 */
#include "app/app_internal.hpp"
#include "domain/date_query.hpp" // date: predicates in the block grammar
#include "render/download.hpp" // a file a visitor can keep
#include "render/audio.hpp" // the audio block's markup, both domains
#include "render/video.hpp"      // a pasted video URL, understood
#include "render/text.hpp"
#include "render/theme.hpp"
#include "json.hpp"
#include "lucide_icons.hpp"

#include <cctype>
#include <cstring>
#include <fstream>
#include "render/icon_set.hpp"   // emoji: the email half of the icon vocabulary
#include "render/image_text.hpp" // the image + text block's markup
#include "render/email_theme.hpp" // the newsletter's own theme: palette, type, shape
#include <set>

std::string HormigaApp::render_preview(std::string_view lang) {
    maiz::ProjectOptions io, dio;
    io.mantle = cur_doc;
    dio.mantle = kDataMantle;
    maiz::Scene issue = maiz::project_scene(core, io);
    maiz::Scene data = maiz::project_scene(core, dio);

    const std::string suf = std::string("_") + std::string(lang);
    const std::string alt = (lang == "en") ? "_es" : "_en";
    auto text = [&](const maiz::SceneNode& n, const char* base) {
        std::string v = field_value(n, base + suf);
        return v.empty() ? field_value(n, base + alt) : v; // bilingual fallback
    };
    /* A bilingual field with a graceful past: `summary_en` if present, else the
     * other language, else the original single-valued `summary`. Nothing that
     * already exists breaks, and a database can be translated one event at a
     * time. */
    auto text_or = [&](const maiz::SceneNode& n, const char* base, const char* legacy) {
        const std::string v = text(n, base);
        return v.empty() ? field_value(n, legacy) : v;
    };
    /* The display title. This used to be the rune NAME title-cased, so a
     * command-safe slug had to double as prose and "ACME" rendered "Acme". */
    auto title_of = [&](const maiz::SceneNode& n) {
        const std::string v = text(n, "title");
        return v.empty() ? humanize(n.name) : v;
    };
    // strings the RENDERER writes are language-selected like everything else
    auto ui = [&](const char* en, const char* es) {
        return std::string(lang == "es" ? es : en);
    };

    /* THE ACCENT COMES FROM CONFIG, not from an ImGui member. `theme_accent` is
     * a float triple the Style tab edits; in a HEADLESS render nobody has ever
     * touched it, so an agent's newsletter arrived in the struct's default gold
     * whatever the organization had configured. */
    const SiteTheme th = read_site_theme(core);
    /* THE NEWSLETTER'S OWN THEME (2026-09-15; render/email_theme.hpp). `et` is
     * not const on purpose: a band renders its blocks with a variant of it, and
     * `acc` is a REFERENCE so every accent written inside a band follows it.
     * The classic preset reproduces the literals this file used to carry. */
    hormiga::mail::Theme et = hormiga::mail::make(hormiga::mail::read_choice(core), th);
    const std::string& acc = et.accent;

    // document order = the grid rows once migrated; the legacy chain until then
    /* One day for the whole issue — the same rule the website keeps; see
     * domain/date_query.hpp and render/site.cpp. */
    const long long today = hormiga::today_days();

    std::vector<const maiz::SceneNode*> chain = hormiga::doc_order(issue);

    /* A NEWSLETTER RENDERS ONE PAGE'S COMPONENTS. A website document may hold
     * five pages; an issue is one. Default is the page being edited, empty =
     * the page ordered 0. A `page` rune itself never renders. */
    {
        std::string home;
        int best = INT_MAX;
        for (const auto& n : issue.nodes)
            if (n.glyph == "page" && hormiga::doc_field_int(n, "order", 0) < best) {
                best = hormiga::doc_field_int(n, "order", 0);
                home = field_value(n, "slug");
                if (home.empty()) home = n.name;
            }
        const std::string target = cur_page.empty() ? home : cur_page;
        std::vector<const maiz::SceneNode*> filtered;
        for (const auto* n : chain) {
            if (n->glyph == "page") continue;
            const std::string pg = field_value(*n, "page");
            if (pg == target || (pg.empty() && target == home))
                filtered.push_back(n);
        }
        chain.swap(filtered);
    }

    /* EMAIL IMAGES ARE PUBLIC URLs. A recipient's mail client cannot fetch a
     * local file, so an image's src is its published `url`, never its `path`
     * (the render-seam resolver, Q9). Unpublished images surface a "publish me"
     * note instead of a broken image, and are counted so the render can warn —
     * a real issue once shipped full of dashed grey boxes because that warning
     * was a GUI toast nobody saw headless. */
    /* PREVIEW WHAT WILL BE SENT, AND SHOW WHAT WILL NOT ARRIVE (2026-09-15).
     * The author: *"the side by side images dont work in an email preview ...
     * likely due to them not being correctly put into imgbb and so they dont
     * show up in the browser preview."* That was it: an image with no `url` was
     * a grey "unpublished" box or nothing, so the preview could not show the
     * layout being built. An image that is only on this computer is now drawn
     * from its local file, relative to the preview, with a dashed red outline,
     * and the issue opens with a notice counting them. An inbox still cannot
     * load a local file — nothing can change that — but the problem is now ON
     * the page, not only in a log. */
    std::set<std::string> local_only;
    int missing_images = 0;
    auto local_src = [&](const std::string& rel) -> std::string {
        if (rel.empty()) return {};
        if (rel.rfind("http://", 0) == 0 || rel.rfind("https://", 0) == 0) return rel;
        const fs::path abs = resolve_file(rel);
        std::error_code ec;
        if (!fs::exists(abs, ec)) return {};
        local_only.insert(abs.generic_string());
        const fs::path r = fs::relative(abs, data_dir("exports"), ec);
        return (ec || r.empty()) ? "file:///" + abs.generic_string() : r.generic_string();
    };
    auto email_src = [&](const maiz::SceneNode& img, bool& ok) {
        std::string u = field_value(img, "url");
        if (u.empty()) u = local_src(field_value(img, "path"));
        ok = !u.empty();
        if (!ok) ++missing_images;
        return u;
    };
    // the outline a local-only image carries, so a preview never looks sendable
    auto local_mark = [](const std::string& src) -> std::string {
        return src.empty() || src.rfind("http", 0) == 0
                   ? std::string()
                   : ";outline:3px dashed #e5484d;outline-offset:-3px";
    };
    // an image FIELD (a path): its image rune's public url, else the local file
    auto image_at = [&](const std::string& path) -> std::string {
        if (path.empty()) return {};
        for (const auto& dn : data.nodes)
            if (dn.glyph == "image" &&
                resolve_file(field_value(dn, "path")) == resolve_file(path)) {
                bool ok = false;
                return email_src(dn, ok);
            }
        return local_src(path);
    };

    std::ostringstream html;
    std::string title;
    for (const auto* n : chain)
        if (n->glyph == "hero" && title.empty()) title = text(*n, "title");
    if (title.empty()) title = ui("Newsletter", "Boletin");

    html << "<!doctype html><html lang=\"" << lang << "\"><head><meta charset=\"utf-8\">"
         << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         << "<title>" << html_escape(title) << "</title>"
         // Outlook desktop falls to Times on a font stack it cannot resolve; a
         // conditional comment is the one place it can be told otherwise
         << (et.classic_type ? "" : "<!--[if mso]><style>body,table,td,a,p,h1,h2{font-family:Arial,Helvetica,sans-serif !important}</style><![endif]-->")
         << "</head>\n"
         << "<body style=\"margin:0;padding:0;background:" << et.page
         << ";font-family:" << et.font << "\">\n"
         << "<!-- generated by Hormiga: effect render " << lang
         << " (email domain: table layout, public image URLs) -->\n"
         // the outer table centres the 620px content across mail clients
         << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
            "cellspacing=\"0\" style=\"background:" << et.page << "\"><tr>"
            "<td align=\"center\" style=\"padding:" << et.outer << "px\">\n"
         << "<table role=\"presentation\" width=\"620\" cellpadding=\"0\" "
            "cellspacing=\"0\" style=\"width:620px;max-width:620px;background:"
         << et.frame << "\">";

    /* THE FRAME'S PADDED CELL, OPENED LAZILY. A full-width banner or band closes
     * it and takes a row of its own, edge to edge; the next block reopens it.
     * For the classic theme the bytes are the ones this header always wrote. */
    bool pad_open = false;
    auto open_pad = [&] {
        if (pad_open) return;
        html << "<tr><td style=\"padding:" << et.pad << "px"
             << hormiga::mail::Theme::more(
                    {et.classic_type ? std::string() : "font-family:" + et.font,
                     et.text_css()})
             << "\">\n";
        pad_open = true;
    };
    auto close_pad = [&] {
        if (!pad_open) return;
        html << "</td></tr>\n";
        pad_open = false;
    };

    // one icon-less meta line; email gets no inline SVG (Gmail strips it)
    auto meta_line = [&](const std::string& t) {
        if (t.empty()) return;
        html << "<br><span style=\"color:" << et.meta << "\">" << html_escape(t) << "</span>";
    };

    /* CARD GRIDS, N TO A ROW (2026-09-14). The author: *"event grids should be
     * able to be side by side as well, like actual grids, not just a list"* --
     * and the same for job openings. On the website `.cards` has always been a
     * CSS grid; this renderer drew one full-width card per row. `columns` (1-3)
     * now lays the cards out as table cells, the one side-by-side layout every
     * mail client honours. Blank is 1, the list it always was, so no existing
     * issue changes. Three is the ceiling: a 620px email split four ways leaves
     * cards about 140px wide, narrower than most event titles. */
    auto grid_cols = [&](const maiz::SceneNode& b) {
        return std::clamp(hormiga::doc_field_int(b, "columns", 1), 1, 3);
    };
    auto grid_cell_open = [&](size_t i, int cols) {
        if (cols < 2) return;
        if (i % (size_t)cols == 0)
            html << (i == 0 ? "<table role=\"presentation\" width=\"100%\" "
                              "cellpadding=\"0\" cellspacing=\"0\"><tr>\n"
                            : "<tr>\n");
        html << "<td valign=\"top\" width=\"" << 100 / cols
             << "%\" style=\"vertical-align:top;padding:0 5px\">\n";
    };
    auto grid_cell_close = [&](size_t i, size_t count, int cols) {
        if (cols < 2) return;
        html << "</td>\n";
        const bool row_end = (i + 1) % (size_t)cols == 0;
        const bool last = i + 1 == count;
        if (last && !row_end) // pad the final row so every cell keeps its width
            for (size_t k = (i + 1) % (size_t)cols; k < (size_t)cols; ++k)
                html << "<td width=\"" << 100 / cols << "%\"></td>\n";
        if (row_end || last) html << "</tr>\n";
        if (last) html << "</table>\n";
    };

    /* The width, in px, the block being emitted may occupy: 572 on its own,
     * less inside a row. Anything that writes a `width=` attribute reads this,
     * because Outlook obeys that attribute over any CSS and a 572 image in a
     * half-width cell pushes the whole newsletter past its 620px frame. */
    int cell_px = et.content_px();
    bool hero_banner_done = false; // the driver already drew it edge to edge
    auto emit_block = [&](const maiz::SceneNode* n) {
        if (n->glyph == "hero") {
            /* THE BANNER REACHES THE NEWSLETTER (2026-09-15). The author:
             * *"banner image doesnt work on newsletter thing."* It had never
             * existed here: this branch read no `image` and no `portrait`, so a
             * hero was a title and a coloured rule whatever it held. A photo
             * BEHIND text is a web idea Outlook cannot place, so the email draws
             * the banner ABOVE the title — inset in the frame, or edge to edge
             * when the theme's shape says so (the driver draws that one).
             * `image_filter`/`image_dim` stay web-only: no mail client filters. */
            const std::string banner = image_at(field_value(*n, "image"));
            if (!banner.empty() && !hero_banner_done)
                html << "<img src=\"" << html_escape(banner) << "\" alt=\"\" width=\""
                     << cell_px << "\" style=\"display:block;width:100%;max-width:" << cell_px
                     << "px;height:auto;border:0;margin:0 0 16px"
                     << hormiga::mail::Theme::more({et.img_css()}) << local_mark(banner)
                     << "\">\n";
            if (et.hero_bar > 0)
                html << "<div style=\"border-top:" << et.hero_bar << "px solid " << acc
                     << ";padding-top:14px\">";
            else
                html << "<div style=\"padding-top:4px\">";
            const std::string portrait = image_at(field_value(*n, "portrait"));
            if (!portrait.empty())
                html << "<img src=\"" << html_escape(portrait) << "\" alt=\"\" width=\"88\" "
                        "height=\"88\" style=\"display:block;width:88px;height:88px;"
                        "border-radius:50%;border:0;margin:0 0 12px;object-fit:cover"
                     << local_mark(portrait) << "\">";
            html << "<h1 style=\"" << et.h1_style() << "\">"
                 << html_escape(text(*n, "title")) << "</h1>";
            const std::string hsub = text(*n, "subtitle");
            if (!hsub.empty())
                html << "<p style=\"" << et.sub_style() << "\">" << prose(hsub) << "</p>";
            html << "</div>\n";
        } else if (n->glyph == "narrative") {
            /* The heading the web domain renders as an `<h3>` (2026-09-03,
             * portfolio report A9). Inline-styled rather than a bare `<h3>`,
             * because mail clients reset heading margins in six different ways;
             * the weight and the space are what carry the hierarchy. */
            const std::string nhead = text(*n, "heading");
            /* The icon is an EMOJI here: Gmail strips inline SVG and no mail
             * client loads an icon font (render/icon_set.hpp). */
            const char* nemo =
                th.icons ? hormiga::iconset::emoji(field_value(*n, "icon")) : "";
            const std::string nmark = *nemo ? std::string(nemo) + " " : std::string();
            if (!nhead.empty())
                html << "<p style=\"" << et.h3_style("18px 0 2px") << "\">" << nmark
                     << html_escape(nhead) << "</p>\n";
            // `- ` and `1. ` lines become real lists; text without one is unchanged
            html << email_prose_block(text(*n, "text"),
                                      "white-space:pre-line;line-height:1.5;color:" + et.ink,
                                      et.ink, nhead.empty() ? nmark : std::string(), true);
        } else if (n->glyph == "section_header") {
            html << et.h2_html(html_escape(text(*n, "title")));
        } else if (n->glyph == "quote") { // email-safe pull-quote
            html << "<blockquote style=\"margin:18px 0;padding:6px 0 6px 18px;"
                    "border-left:4px solid " << acc
                 << ";font-style:italic;color:" << et.quote << ";font-size:17px\">"
                 << html_escape(text(*n, "text"));
            const std::string au = field_value(*n, "author");
            if (!au.empty())
                html << "<br><span style=\"font-size:13px;color:" << et.faint
                     << ";font-style:normal\">- " << html_escape(au) << "</span>";
            html << "</blockquote>\n";
        } else if (n->glyph == "stat") { // email-safe metric
            html << "<div style=\"text-align:center;margin:16px 0\">"
                    "<div style=\"font-size:34px;font-weight:bold;color:" << acc
                 << "\">" << html_escape(field_value(*n, "number"))
                 << "</div><div style=\"color:" << et.faint << ";font-size:14px\">"
                 << html_escape(text(*n, "label")) << "</div></div>\n";
        } else if (n->glyph == "divider") {
            const std::string ds = field_value(*n, "divider_style");
            if (ds == "space") html << "<div style=\"height:24px\"></div>\n";
            else html << "<hr style=\"border:none;border-top:1px solid " << et.rule
                      << ";margin:18px 0\">\n";
        } else if (n->glyph == "link") {
            /* THE NEWSLETTER HAD NO `link` CASE AT ALL until 2026-08-19 — 17
             * anchors on the website, zero in the email, silently, for a
             * publication whose entire job is *click this to RSVP*. A
             * bulletproof button: a one-cell table wrapping a padded anchor,
             * because Outlook drops padding on inline elements. */
            const std::string tgt = href_of(field_value(*n, "target"));
            const std::string label = text(*n, "label");
            if (!tgt.empty())
                html << et.button(tgt, label.empty() ? tgt : label);
        } else if (n->glyph == "download") {
            /* A newsletter cannot carry the file, so this is a link — and a
             * RELATIVE href opens nothing in anybody's inbox. `site.base_url`
             * is what makes an absolute one possible; without it the block says
             * the file is on the website and the render says why. */
            std::string want = field_value(*n, "file");
            std::string via_rune;
            if (!want.empty())
                for (const auto& dn : data.nodes)
                    if (dn.glyph == "resource" && dn.name == want) {
                        via_rune = want;
                        want = field_value(dn, "path");
                        break;
                    }
            hormiga::DownloadCard dc;
            dc.label = text(*n, "label");
            if (dc.label.empty()) dc.label = "Download";
            dc.caption = text(*n, "caption");

            if (!want.empty() && hormiga::download_refusal(want).empty()) {
                std::string base = core.dispatch("config get site.base_url").data;
                if (base.size() >= 2 && base.front() == '"' && base.back() == '"')
                    base = base.substr(1, base.size() - 2);
                if (base == "null") base.clear();
                while (!base.empty() && base.back() == '/') base.pop_back();
                if (!base.empty()) {
                    dc.href = base + "/assets/" +
                              std::filesystem::path(want).filename().string();
                    dc.meta = hormiga::file_kind(want);
                } else {
                    log.push_back({"warn", "render",
                                   n->name + ": the newsletter cannot link this "
                                   "file because site.base_url is unset - a "
                                   "relative link opens nothing in a mail "
                                   "client. `config set site.base_url "
                                   "https://your-domain.org`"});
                }
            }
            html << hormiga::download_email(dc, acc);
        } else if (n->glyph == "audio") {
            /* Markup in `render/audio.hpp`; what happens here is resolving the
             * one thing an email cannot do without — an ABSOLUTE address for
             * the file, which only `site.base_url` can supply. */
            hormiga::AudioCard ac;
            const std::string asrc = field_value(*n, "src");
            ac.title = text(*n, "title");
            ac.artist = field_value(*n, "artist");
            ac.duration = field_value(*n, "duration");
            ac.caption = text(*n, "caption");

            std::string base = core.dispatch("config get site.base_url").data;
            if (base.size() >= 2 && base.front() == '"' && base.back() == '"')
                base = base.substr(1, base.size() - 2);
            if (base == "null") base.clear();
            while (!base.empty() && base.back() == '/') base.pop_back();
            if (!base.empty() && !asrc.empty())
                ac.src = base + "/assets/" +
                         std::filesystem::path(asrc).filename().string();

            html << hormiga::audio_email(
                ac, acc, "Listen",
                ac.title.empty() ? "This recording is on the website."
                                 : "Listen to this on the website.");
            if (ac.src.empty() && !asrc.empty())
                log.push_back({"warn", "render",
                               n->name + ": the newsletter cannot link this "
                               "recording because site.base_url is unset - an "
                               "email has no site/ folder beside it, so a "
                               "relative path opens nothing. `config set "
                               "site.base_url https://your-domain.org`"});
        } else if (n->glyph == "video") {
            /* ── A VIDEO, IN AN EMAIL (2026-08-28) ─────────────────────
             *
             * No mail client plays one. Gmail strips `<iframe>` outright,
             * Outlook strips `<video>`, and the handful that would render
             * either are not the ones this organization's members read mail in.
             * So the newsletter's job is to get the reader TO the video, not to
             * pretend it can hold it — a poster with a play mark over it, the
             * whole thing an anchor, plus the button underneath.
             *
             * The poster is `poster`'s PUBLISHED url through `email_src`, the
             * same resolver every other image in this renderer uses: an email
             * has no `site/` folder beside it, so a local path is not something
             * a recipient can load. No poster is not a failure — the button
             * alone is a complete answer, and it is the half that gets clicked.
             *
             * NOTHING IS FETCHED FROM THE VIDEO HOST here either, for the web
             * renderer's reason and one more: remote images in mail are already
             * a tracking surface, and the address they would come from should
             * be the organization's, not a third party's. */
            const hormiga::VideoRef vid =
                hormiga::parse_video_url(field_value(*n, "url"));
            const std::string vcap = text(*n, "caption");
            if (!vcap.empty())
                html << "<p style=\"color:" << et.cap << ";margin:4px 0\">" << prose(vcap)
                     << "</p>\n";
            if (vid.ok()) {
                const std::string watch = vid.watch_url();
                const std::string poster_rune = field_value(*n, "poster");
                std::string psrc;
                if (!poster_rune.empty())
                    for (const auto& dn : data.nodes)
                        if (dn.glyph == "image" && dn.name == poster_rune) {
                            bool ok = false;
                            psrc = email_src(dn, ok);
                            if (!ok) psrc.clear();
                        }
                if (!psrc.empty())
                    html << "<a href=\"" << html_escape(watch)
                         << "\" style=\"display:block\"><img src=\""
                         << html_escape(psrc)
                         << "\" width=\"" << cell_px << "\" style=\"display:block;width:100%;"
                            "max-width:" << cell_px << "px;height:auto;border:0"
                         << hormiga::mail::Theme::more({et.img_css()}) << local_mark(psrc)
                         << "\" alt=\""
                         << html_escape(ui("Play the video", "Reproducir el video"))
                         << "\"></a>\n";
                html << et.button(watch, ui("Watch on ", "Ver en ") + vid.provider_label());
            } else {
                /* An unset or unrecognised link prints NOTHING rather than an
                 * empty-state: a website section can honestly say "no video
                 * yet", and a posted newsletter cannot be corrected. The render
                 * log is where this belongs, and `render-site` already says it
                 * with the same words. */
            }
        } else if (n->glyph == "event_grid") {
            const std::string query = field_value(*n, "query");
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:" << et.cap << ";margin:4px 0\">" << prose(cap)
                     << "</p>\n";
            const std::string detail = field_value(*n, "detail");
            const int limit = hormiga::doc_field_int(*n, "limit", 0);
            const std::string sort = field_value(*n, "sort");

            std::vector<const maiz::SceneNode*> hits;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "event" && hormiga::query_matches(query, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    hits.push_back(&dn);
            if (sort == "name") {
                std::stable_sort(hits.begin(), hits.end(),
                                 [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                                     return a->name < b->name;
                                 });
            } else {
                const bool desc = sort == "date-desc";
                std::stable_sort(hits.begin(), hits.end(),
                                 [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                                     const std::string da = field_value(*a, "date");
                                     const std::string db = field_value(*b, "date");
                                     // ISO dates sort as strings; an undated
                                     // event holds its place at the end rather
                                     // than jumping to the front as ""
                                     if (da.empty() != db.empty()) return db.empty();
                                     return desc ? db < da : da < db;
                                 });
            }
            if (limit > 0 && (int)hits.size() > limit) hits.resize((size_t)limit);

            const int ecols = grid_cols(*n);
            for (size_t ei = 0; ei < hits.size(); ++ei) {
                const maiz::SceneNode& dn = *hits[ei];
                /* Four things here were declared fields the render simply did
                 * not honour until a person read a real issue and said it was
                 * a wall of text: the times on a DATED event, the event's own
                 * colour, the title, and how much of the summary to show. */
                std::string when = human_date(field_value(dn, "date"), lang);
                if (when.empty()) when = field_value(dn, "days");
                const std::string st = field_value(dn, "start_time");
                const std::string etime = field_value(dn, "end_time");
                std::string bar = field_value(dn, "color");
                if (bar.size() < 4 || bar[0] != '#') bar = acc;

                grid_cell_open(ei, ecols);
                html << et.card_open(et.card, bar, "10px 0", "8px 12px", bar != acc)
                     << et.b_open() << html_escape(title_of(dn)) << "</b>";
                std::string line = when;
                if (!st.empty()) {
                    line += (line.empty() ? "" : " \xc2\xb7 ") + st;
                    if (!etime.empty()) line += "\xe2\x80\x93" + etime;
                }
                const std::string venue = field_value(dn, "venue");
                if (!venue.empty())
                    line += (line.empty() ? "" : " \xc2\xb7 ") + venue;
                meta_line(line);
                const std::string vlink = field_value(dn, "virtual");
                if (!vlink.empty())
                    html << "<br>"
                         << et.button(href_of(vlink), ui("Join online", "Unirse en linea"));
                if (detail != "title") {
                    std::string sum = text_or(dn, "summary", "summary");
                    if (detail != "full") sum = clip(sum, 140);
                    if (!sum.empty()) html << "<br>" << prose(sum);
                }
                html << "</td></tr></table>\n";
                grid_cell_close(ei, hits.size(), ecols);
            }
            if (hits.empty())
                html << "<p style=\"color:" << et.empty << "\">"
                     << html_escape(ui("(nothing scheduled)", "(nada programado)"))
                     << "</p>\n";
        } else if (n->glyph == "directory") {
            /* THE CONSENT RULE IS THE SAME RULE in both domains, which is the
             * point of it living at the seam: `clearance:public` to appear at
             * all, `clearance:contact` to release an email or a phone. A block
             * that is safe on the website and leaky in the newsletter would be
             * worse than no block. No photographs — a mail client cannot fetch
             * a local file, so a roster of avatars is a wall of broken boxes. */
            const std::string dq = field_value(*n, "query");
            std::string dkind = field_value(*n, "kind");
            if (dkind.empty()) dkind = "contact";
            const int dlim = hormiga::doc_field_int(*n, "limit", 0);
            const std::string dcap = text(*n, "caption");
            if (!dcap.empty())
                html << "<p style=\"color:" << et.cap << ";margin:4px 0\">" << prose(dcap)
                     << "</p>\n";
            std::vector<const maiz::SceneNode*> dpeople;
            int dwithheld = 0;
            for (const auto& dn : data.nodes) {
                const bool is_c = dn.glyph == "contact";
                const bool is_o = dn.glyph == "organization";
                if (!is_c && !is_o) continue;
                if (dkind == "contact" && !is_c) continue;
                if (dkind == "organization" && !is_o) continue;
                if (!dq.empty() && !hormiga::query_matches(dq, data, dn, today)) continue;
                if (allo_web_hidden(dn.name)) continue;
                if (!maiz::node_matches("clearance:public", dn)) { ++dwithheld; continue; }
                dpeople.push_back(&dn);
            }
            std::stable_sort(dpeople.begin(), dpeople.end(),
                             [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                                 return display_name(*a) < display_name(*b);
                             });
            rank_by_tags(dpeople, field_value(*n, "rank_up"), field_value(*n, "rank_down"));
            if (dlim > 0 && (int)dpeople.size() > dlim) dpeople.resize((size_t)dlim);
            if (dwithheld)
                log.push_back({"info", "render",
                               "directory " + n->name + ": " +
                                   std::to_string(dwithheld) +
                                   " matching rune(s) withheld - only runes tagged "
                                   "`clearance:public` are published."});
            for (const maiz::SceneNode* pn : dpeople) {
                const maiz::SceneNode& dn = *pn;
                const std::string role = dn.glyph == "contact"
                                             ? field_value(dn, "role")
                                             : field_value(dn, "abbreviation");
                html << et.card_open(et.card, acc, "8px 0", "8px 12px") << et.b_open()
                     << html_escape(display_name(dn)) << "</b>";
                meta_line(role);
                const std::string dbio = field_value(dn, "bio");
                if (!dbio.empty()) html << "<br>" << prose(clip(dbio, 200));
                if (maiz::node_matches("clearance:contact", dn)) {
                    const std::string em = field_value(dn, "email");
                    if (!em.empty())
                        html << "<br><a href=\"mailto:" << html_escape(em)
                             << "\" style=\"color:" << acc << "\">"
                             << html_escape(em) << "</a>";
                }
                html << "</td></tr></table>\n";
            }
            if (dpeople.empty())
                html << "<p style=\"color:" << et.empty << "\">"
                     << html_escape(ui("(nobody is listed yet)",
                                       "(nadie esta en la lista)"))
                     << "</p>\n";
        } else if (n->glyph == "job_grid") {
            const std::string query = field_value(*n, "query");
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:" << et.cap << ";margin:4px 0\">" << prose(cap)
                     << "</p>\n";
            const std::string jdetail = field_value(*n, "detail");
            /* ICONS ON THE JOB LINES (2026-09-13; the author: "remember icons!").
             * Emoji, for the reason on the narrative above, and only when the
             * theme's icon switch is on — the website asks the same flag. */
            auto jicon = [&](const char* icon, const std::string& v) -> std::string {
                const char* e = th.icons ? hormiga::iconset::emoji(icon) : "";
                return (v.empty() || !*e) ? v : std::string(e) + " " + v;
            };
            const int jlimit = hormiga::doc_field_int(*n, "limit", 0);
            std::vector<const maiz::SceneNode*> jobs;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "job" && hormiga::query_matches(query, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    jobs.push_back(&dn);
            rank_by_tags(jobs, field_value(*n, "rank_up"), field_value(*n, "rank_down"));
            if (jlimit > 0 && (int)jobs.size() > jlimit) jobs.resize((size_t)jlimit);
            const int jcols = jdetail == "line" ? 1 : grid_cols(*n);
            for (size_t ji = 0; ji < jobs.size(); ++ji) {
                const maiz::SceneNode& dn = *jobs[ji];
                if (jdetail == "line") {
                    /* THE TIGHTEST FORM (2026-09-13). The author: *"the compact
                     * version of job listings in the newsletter is still really
                     * long, we should have an EVEN MORE compact version."*
                     * Compact is a card with up to six lines and a clipped
                     * description; this is ONE line per posting — what it is,
                     * who, how much, where, until when, and where to write —
                     * which is the whole of what a reader needs to decide
                     * whether to click. */
                    std::string jl = "<b>" + html_escape(title_of(dn)) + "</b>";
                    auto jadd = [&](const char* icon, const std::string& v) {
                        if (!v.empty()) jl += " &middot; " + html_escape(jicon(icon, v));
                    };
                    jadd("building-2", field_value(dn, "org"));
                    jadd("circle-dollar-sign", field_value(dn, "pay"));
                    jadd("map-pin", field_value(dn, "location"));
                    const std::string jdl = field_value(dn, "deadline");
                    if (!jdl.empty())
                        jadd("calendar", ui("Closes ", "Cierra ") + human_date(jdl, lang));
                    const std::string jce = field_value(dn, "contact_email");
                    if (!jce.empty()) {
                        const char* me = th.icons ? hormiga::iconset::emoji("mail") : "";
                        jl += " &middot; " + (*me ? std::string(me) + " " : std::string()) +
                              "<a href=\"mailto:" + html_escape(jce) + "\" style=\"color:" +
                              acc + "\">" + html_escape(jce) + "</a>";
                    }
                    html << "<p style=\"margin:4px 0;padding:4px 0 4px 10px;"
                            "border-left:3px solid " << et.job_bar << ";font-size:14px;"
                            "line-height:1.45" << hormiga::mail::Theme::more({et.text_css()})
                         << "\">" << jl << "</p>\n";
                    continue;
                }
                grid_cell_open(ji, jcols);
                html << et.card_open(et.card_job, et.job_bar, "10px 0", "8px 12px")
                     << et.b_open() << html_escape(title_of(dn)) << "</b>";
                const std::string org = field_value(dn, "org");
                if (!org.empty()) html << " &middot; " << html_escape(jicon("building-2", org));
                std::string l1 = jicon("circle-dollar-sign", field_value(dn, "pay"));
                const std::string loc = field_value(dn, "location");
                if (!loc.empty()) l1 += (l1.empty() ? "" : " \xc2\xb7 ") + jicon("map-pin", loc);
                meta_line(l1);
                /* PARITY WITH THE WEBSITE, found by tools/lint_glyph_fields.py:
                 * these were declared, rendered on the site, and rendered by
                 * nothing here. The contact email matters most — a newsletter
                 * that says "email your resume" and does not say where is worse
                 * than one that omits the posting. */
                std::string l2 = jicon("briefcase", field_value(dn, "job_type"));
                const std::string av = field_value(dn, "availability");
                if (!av.empty()) l2 += (l2.empty() ? "" : " \xc2\xb7 ") + av;
                meta_line(l2);
                const std::string dl = field_value(dn, "deadline");
                if (!dl.empty())
                    meta_line(jicon("calendar", ui("Closes ", "Cierra ") + human_date(dl, lang)));
                const std::string cn = field_value(dn, "contact_name");
                const std::string ce = field_value(dn, "contact_email");
                const std::string cp = field_value(dn, "contact_phone");
                if (!cn.empty() || !cp.empty())
                    meta_line(cn + ((!cn.empty() && !cp.empty()) ? " \xc2\xb7 " : "") + cp);
                if (!ce.empty())
                    html << "<br><a href=\"mailto:" << html_escape(ce)
                         << "\" style=\"color:" << acc << "\">" << html_escape(ce)
                         << "</a>";
                if (jdetail != "title") {
                    std::string desc = field_value(dn, "description");
                    if (jdetail != "full") desc = clip(desc, 260);
                    if (!desc.empty()) html << "<br>" << prose(desc);
                }
                html << "</td></tr></table>\n";
                grid_cell_close(ji, jobs.size(), jcols);
            }
            if (jobs.empty())
                html << "<p style=\"color:" << et.empty << "\">"
                     << html_escape(ui("(no open positions)", "(no hay vacantes)"))
                     << "</p>\n";
        } else if (n->glyph == "image_grid") {
            const std::string q = field_value(*n, "query");
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:" << et.cap << ";margin:4px 0\">" << prose(cap)
                     << "</p>\n";
            const int glimit = hormiga::doc_field_int(*n, "limit", 0);
            /* FIT (2026-09-13). This read `display == "thumb"`, a value the
             * glyph never offered (it is grid / masonry / carousel), so the
             * branch could not run. `fit` replaces it. Blank keeps what the
             * newsletter always did — natural size — because `object-fit` is
             * the one property here Outlook desktop ignores: there, crop and
             * whole fall back to a stretched image, and natural is the only form
             * that looks the same in every inbox. */
            const std::string efit = field_value(*n, "fit");
            const char* efit_style =
                efit == "crop"      ? "height:200px;object-fit:cover;"
                : efit == "whole"   ? "height:200px;object-fit:contain;background:#f3f3f3;"
                : efit == "stretch" ? "height:200px;"
                                    : "height:auto;";
            int shown = 0;
            html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                    "cellspacing=\"0\"><tr>\n";
            std::vector<const maiz::SceneNode*> gimgs;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "image" && hormiga::query_matches(q, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    gimgs.push_back(&dn);
            rank_by_tags(gimgs, field_value(*n, "rank_up"), field_value(*n, "rank_down"));
            for (const maiz::SceneNode* gp : gimgs) {
                const maiz::SceneNode& dn = *gp;
                // per-language THINGS are sibling runes with a `lang:` tag; a
                // rune carrying several belongs on every page it names
                if (!lang_matches(dn, lang)) continue;
                if (glimit > 0 && shown >= glimit) break;
                bool ok = false;
                const std::string src = email_src(dn, ok);
                if (shown && shown % 2 == 0) html << "</tr><tr>\n";
                html << "<td width=\"50%\" style=\"padding:4px\" align=\"center\">";
                if (ok)
                    html << "<img src=\"" << html_escape(src) << "\" alt=\""
                         << html_escape(field_value(dn, "alt"))
                         << "\" style=\"max-width:" << std::max(80, cell_px / 2 - 8)
                         << "px;width:100%;" << efit_style << "border:0;display:block"
                         << hormiga::mail::Theme::more({et.img_css()}) << local_mark(src)
                         << "\">";
                else
                    html << "<div style=\"border:1px dashed #bbb;color:" << et.empty
                         << ";padding:22px 8px;font-size:13px\">"
                         << html_escape(ui("image not found", "imagen no encontrada"))
                         << "</div>";
                html << "</td>\n";
                ++shown;
            }
            if (shown % 2) html << "<td width=\"50%\"></td>\n";
            html << "</tr></table>\n";
            if (!shown)
                html << "<p style=\"color:" << et.empty << "\">"
                     << html_escape(ui("(nothing to show)", "(nada que mostrar)"))
                     << "</p>\n";
        } else if (n->glyph == "event_feature" || n->glyph == "event_flier") {
            /* Both featured-event blocks degrade to the event card in email: a
             * background photograph behind text is a web idea, and a mail
             * client cannot be relied on to place one behind anything. The
             * flier itself rides along as a published image when it has a URL. */
            const std::string evname = field_value(*n, "event");
            const maiz::SceneNode* ev = nullptr;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "event" && dn.name == evname) ev = &dn;
            if (ev && !allo_web_hidden(ev->name)) {
                html << et.card_open(et.card, acc, "14px 0", "12px 14px")
                     << et.feature_b_open() << html_escape(title_of(*ev)) << "</b>";
                std::string line = human_date(field_value(*ev, "date"), lang);
                if (line.empty()) line = field_value(*ev, "days");
                const std::string st = field_value(*ev, "start_time");
                if (!st.empty()) {
                    line += (line.empty() ? "" : " \xc2\xb7 ") + st;
                    const std::string etime = field_value(*ev, "end_time");
                    if (!etime.empty()) line += "\xe2\x80\x93" + etime;
                }
                const std::string venue = field_value(*ev, "venue");
                if (!venue.empty()) line += (line.empty() ? "" : " \xc2\xb7 ") + venue;
                meta_line(line);
                const std::string sum = text_or(*ev, "summary", "summary");
                if (!sum.empty()) html << "<br>" << prose(sum);
                // the flier, if one is linked and published
                const auto fl = related_runes(data, ev->name, kFlierRelations());
                std::vector<const maiz::SceneNode*> imgs;
                for (const auto* c : fl)
                    if (c->glyph == "image") imgs.push_back(c);
                if (const maiz::SceneNode* f = pick_for_lang(imgs, lang)) {
                    bool ok = false;
                    const std::string src = email_src(*f, ok);
                    if (ok)
                        html << "<br><img src=\"" << html_escape(src) << "\" alt=\""
                             << html_escape(field_value(*f, "alt"))
                             << "\" style=\"max-width:100%;border:0;margin-top:8px;"
                                "display:block" << hormiga::mail::Theme::more({et.img_css()})
                             << local_mark(src) << "\">";
                }
                const std::string ctal = field_value(*n, "cta_link").empty()
                                             ? field_value(*ev, "virtual")
                                             : field_value(*n, "cta_link");
                if (!ctal.empty()) {
                    std::string cta = text(*n, "cta");
                    if (cta.empty()) cta = ui("More about this", "Mas informacion");
                    html << et.button(href_of(ctal), cta);
                }
                html << "</td></tr></table>\n";
            }
        } else if (n->glyph == "map_embed" || n->glyph == "calendar_embed") {
            /* The interactive widgets are web-only by physics: a mail client
             * runs no JavaScript. The caption still carries whatever the author
             * wanted to say, so the block is not silently nothing. */
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:" << et.cap << ";margin:8px 0\">" << prose(cap)
                     << "</p>\n";
        } else if (n->glyph == "image_text") {
            /* A picture and the words that belong with it. An email cannot load
             * a local file, so the image is found by its path and published by
             * its `url` — the same resolver every image in this renderer uses. */
            const std::string ipath = field_value(*n, "image");
            const std::string isrc = image_at(ipath);
            if (!ipath.empty() && isrc.empty())
                log.push_back({"warn", "render",
                               n->name + ": this image + text block's picture was not "
                               "found (no public url, and no file at " + ipath +
                               "), so the newsletter shows the text alone."});
            const std::string iemo =
                th.icons ? std::string(hormiga::iconset::emoji(field_value(*n, "icon")))
                         : std::string();
            const std::string imark = iemo.empty() ? std::string() : iemo + " ";
            const std::string ihead = html_escape(text(*n, "heading"));
            const std::string itext = text(*n, "text");
            html << hormiga::image_text_email(
                html_escape(isrc), html_escape(text(*n, "alt")),
                ihead.empty() ? std::string()
                              : "<p style=\"" + et.h3_style("0 0 4px") + "\">" + imark +
                                    ihead + "</p>",
                (itext.empty() && !(ihead.empty() && !imark.empty()))
                    ? std::string()
                    : email_prose_block(
                          itext, "margin:0;white-space:pre-line;line-height:1.5;color:" + et.ink,
                          et.ink, ihead.empty() ? imark : std::string(), false),
                field_value(*n, "side") == "right", cell_px,
                hormiga::mail::Theme::more({et.img_css()}) + local_mark(isrc));
        } else if (n->glyph == "footer") {
            html << "<hr style=\"border:none;border-top:1px solid " << et.rule
                 << ";margin:22px 0\">"
                 << "<p style=\"color:" << et.faint << ";font-size:13px;line-height:1.5\">"
                 << prose(text(*n, "text")) << "</p>\n";
        }
    };

    /* ── ROWS REACH THE NEWSLETTER (2026-09-13) ─────────────────────────────
     *
     * The author, making a newsletter: *"buttons or other things don't stack
     * correctly horizontally. Maybe something is broken with the way we are
     * making tables and grids?"* Something was, and it was simpler than broken:
     * this loop walked the document and emitted every block as its own
     * full-width table, one under the next. It never read `row` at all. The
     * Builder let you place two buttons side by side, the website honoured it,
     * and the newsletter quietly stacked them — the "declared but invisible"
     * shape again, the same week Click LaFont reported `col` doing the same
     * thing on the web.
     *
     * So this mirrors the website's driver in render/site.cpp: a run of
     * consecutive blocks sharing a `row` becomes ONE table row, each block a
     * cell. Hero and footer always stand alone. Cells are sized from `span`
     * NORMALISED to the row's total, so a row whose spans sum to less than 12
     * still fills the width — an email table cannot hold a gap, and inventing
     * one would be exactly the layout surprise Click's report warned against. */
    for (size_t i = 0; i < chain.size();) {
        const maiz::SceneNode* n = chain[i];
        const int row = hormiga::doc_field_int(*n, "row", -1);
        size_t j = i + 1;
        if (row >= 0 && n->glyph != "hero" && n->glyph != "footer")
            while (j < chain.size() &&
                   hormiga::doc_field_int(*chain[j], "row", -2) == row &&
                   chain[j]->glyph != "hero" && chain[j]->glyph != "footer")
                ++j;
        /* A BAND carried by the row's leader, as on the website; what each kind
         * MEANS in an inbox is in render/email_theme.hpp, "A band, in an inbox". */
        const bool is_hero = n->glyph == "hero";
        const hormiga::mail::Band bnd = hormiga::mail::band(
            et, is_hero ? std::string() : field_value(*n, "band_bg"),
            is_hero ? std::string() : image_at(field_value(*n, "band_image")));
        const bool bfull = bnd.on && field_value(*n, "band_full") == "1";
        // a hero's banner edge to edge, when the theme's shape asks for it
        const std::string bleed_img =
            is_hero && et.bleed ? image_at(field_value(*n, "image")) : std::string();
        if (!bleed_img.empty()) {
            close_pad();
            html << "<tr><td style=\"padding:0\"><img src=\"" << html_escape(bleed_img)
                 << "\" alt=\"\" width=\"620\" style=\"display:block;width:100%;"
                    "max-width:620px;height:auto;border:0" << local_mark(bleed_img)
                 << "\"></td></tr>\n";
            hero_banner_done = true;
        }
        const hormiga::mail::Theme outer = et;
        const int base_px = bnd.on && !bfull ? et.content_px() - 48 : et.content_px();
        if (bfull) {
            close_pad();
            html << "<tr><td" << bnd.attrs << " style=\"" << bnd.css << ";padding:32px "
                 << et.pad << "px;color:" << bnd.t.ink
                 << (et.classic_type ? std::string() : ";font-family:" + et.font) << "\">\n";
        } else {
            open_pad();
            if (bnd.on)
                html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                        "cellspacing=\"0\" style=\"margin:16px 0\"><tr><td" << bnd.attrs
                     << " style=\"" << bnd.css << ";padding:22px 24px;color:" << bnd.t.ink
                     << hormiga::mail::Theme::more({hormiga::mail::Theme::corners(et.radius)})
                     << "\">\n";
        }
        if (bnd.on) et = bnd.t;
        cell_px = base_px;
        if (j - i > 1) {
            int total = 0;
            for (size_t k = i; k < j; ++k)
                total += std::clamp(hormiga::doc_field_int(*chain[k], "span", 12), 1, 12);
            html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                    "cellspacing=\"0\" style=\"margin:6px 0\"><tr>\n";
            for (size_t k = i; k < j; ++k) {
                const int span =
                    std::clamp(hormiga::doc_field_int(*chain[k], "span", 12), 1, 12);
                const int pct = std::max(1, span * 100 / std::max(1, total));
                cell_px = std::max(120, base_px * pct / 100 - 12);
                html << "<td valign=\"top\" width=\"" << pct
                     << "%\" style=\"vertical-align:top;padding:0 6px\">\n";
                emit_block(chain[k]);
                html << "</td>\n";
            }
            html << "</tr></table>\n";
        } else {
            emit_block(n);
        }
        if (bnd.on) {
            et = outer;
            html << (bfull ? "</td></tr>\n" : "</td></tr></table>\n");
        }
        cell_px = et.content_px();
        hero_banner_done = false;
        i = j;
    }

    open_pad(); // an issue with nothing in it still has its frame, as it always did
    html << "</td></tr></table>\n</td></tr></table>\n</body></html>\n";

    /* THE RENDER'S OWN REPORT. Both of these were things the render knew and
     * never said: an issue that is too long is discovered by a person reading
     * it, and an unpublished image is discovered when the newsletter arrives
     * full of dashed grey boxes. `render_from_state` forwards these to stderr
     * so a headless caller sees them too. */
    {
        const std::string body = html.str();
        size_t words = 0;
        bool in_tag = false, in_word = false;
        for (char c : body) {
            if (c == '<') { in_tag = true; in_word = false; continue; }
            if (c == '>') { in_tag = false; continue; }
            if (in_tag) continue;
            if (std::isspace((unsigned char)c)) in_word = false;
            else if (!in_word) { in_word = true; ++words; }
        }
        const int mins = (int)((words + 199) / 200); // ~200 wpm, rounded up
        log.push_back({"info", "render",
                       std::to_string(words) + " words, about " +
                           std::to_string(mins < 1 ? 1 : mins) + " min read"});
    }
    if (!local_only.empty())
        log.push_back({"warn", "render",
                       std::to_string(local_only.size()) +
                           " image(s) are not online yet - the preview draws them "
                           "outlined in red, but they will not load in anyone's inbox. "
                           "Host them online through the Antfarm (Antfarm > Hosting images "
                           "online), or with `effect host-online missing`."});
    if (missing_images)
        log.push_back({"warn", "render",
                       std::to_string(missing_images) +
                           " image(s) have neither a public url nor a file on this "
                           "computer, so the newsletter cannot show them at all."});

    /* `render` writes into `exports/`, which is where `--describe` had always
     * claimed it went while it was writing beside the database. */
    std::error_code ec;
    fs::create_directories(data_dir("exports"), ec);
    const fs::path out = data_dir("exports") /
                         ("preview-" + std::string(lang) + ".html");
    std::string page = html.str();
    if (!local_only.empty()) {
        // the notice sits at the very top, where a person about to send looks
        const std::string notice =
            "<div style=\"background:#fff1f0;border-bottom:2px solid #e5484d;color:#7a1f1f;"
            "padding:10px 16px;font:14px/1.45 Arial,Helvetica,sans-serif\">" +
            html_escape(ui("PREVIEW: ", "VISTA PREVIA: ") + std::to_string(local_only.size()) +
                        ui(" image(s) outlined in red are not online yet and will "
                           "not appear in anyone's inbox until they are hosted online.",
                           " imagen(es) con borde rojo aun no estan en linea y no "
                           "apareceran en el correo de nadie hasta que esten en linea.")) +
            "</div>\n";
        const size_t body = page.find("<body");
        const size_t at = body == std::string::npos ? body : page.find(">\n", body);
        if (at != std::string::npos) page.insert(at + 2, notice);
    }
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    o << page;
    if (!o) {
        log.push_back({"error", "render", "could not write " + out.string()});
        return {};
    }
    return out.string();
}
