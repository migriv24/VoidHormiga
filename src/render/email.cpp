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
#include "render/video.hpp"      // a pasted video URL, understood
#include "render/text.hpp"
#include "render/theme.hpp"
#include "json.hpp"
#include "lucide_icons.hpp"

#include <cctype>
#include <cstring>
#include <fstream>

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
    const std::string acc = th.accent.empty() ? std::string("#3f6fae") : th.accent;

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
    int unpublished = 0;
    auto email_src = [&](const maiz::SceneNode& img, bool& ok) {
        const std::string u = field_value(img, "url");
        ok = !u.empty();
        if (!ok) ++unpublished;
        return u;
    };

    std::ostringstream html;
    std::string title;
    for (const auto* n : chain)
        if (n->glyph == "hero" && title.empty()) title = text(*n, "title");
    if (title.empty()) title = ui("Newsletter", "Boletin");

    html << "<!doctype html><html lang=\"" << lang << "\"><head><meta charset=\"utf-8\">"
         << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         << "<title>" << html_escape(title) << "</title></head>\n"
         << "<body style=\"margin:0;padding:0;background:#f2f0ea;"
            "font-family:Georgia,'Times New Roman',serif\">\n"
         << "<!-- generated by Hormiga: effect render " << lang
         << " (email domain: table layout, public image URLs) -->\n"
         // the outer table centres the 620px content across mail clients
         << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
            "cellspacing=\"0\" style=\"background:#f2f0ea\"><tr>"
            "<td align=\"center\" style=\"padding:16px\">\n"
         << "<table role=\"presentation\" width=\"620\" cellpadding=\"0\" "
            "cellspacing=\"0\" style=\"width:620px;max-width:620px;background:#ffffff\">"
            "<tr><td style=\"padding:24px\">\n";

    // one icon-less meta line; email gets no inline SVG (Gmail strips it)
    auto meta_line = [&](const std::string& t) {
        if (t.empty()) return;
        html << "<br><span style=\"color:#555\">" << html_escape(t) << "</span>";
    };

    for (const auto* n : chain) {
        if (n->glyph == "hero") {
            html << "<div style=\"border-top:6px solid " << acc
                 << ";padding-top:14px\">"
                 << "<h1 style=\"margin:0;font-size:26px;color:#2c2c2c\">"
                 << html_escape(text(*n, "title")) << "</h1>";
            const std::string hsub = text(*n, "subtitle");
            if (!hsub.empty())
                html << "<p style=\"margin:4px 0 0;color:#666;font-size:16px\">"
                     << prose(hsub) << "</p>";
            html << "</div>\n";
        } else if (n->glyph == "narrative") {
            html << "<p style=\"white-space:pre-line;line-height:1.5;color:#333\">"
                 << prose(text(*n, "text")) << "</p>\n";
        } else if (n->glyph == "section_header") {
            html << "<h2 style=\"border-bottom:2px solid " << acc
                 << ";padding-bottom:4px;margin:26px 0 6px;font-size:20px;"
                    "color:#3a3a3a\">"
                 << html_escape(text(*n, "title")) << "</h2>\n";
        } else if (n->glyph == "quote") { // email-safe pull-quote
            html << "<blockquote style=\"margin:18px 0;padding:6px 0 6px 18px;"
                    "border-left:4px solid " << acc
                 << ";font-style:italic;color:#444;font-size:17px\">"
                 << html_escape(text(*n, "text"));
            const std::string au = field_value(*n, "author");
            if (!au.empty())
                html << "<br><span style=\"font-size:13px;color:#888;"
                        "font-style:normal\">- " << html_escape(au) << "</span>";
            html << "</blockquote>\n";
        } else if (n->glyph == "stat") { // email-safe metric
            html << "<div style=\"text-align:center;margin:16px 0\">"
                    "<div style=\"font-size:34px;font-weight:bold;color:" << acc
                 << "\">" << html_escape(field_value(*n, "number"))
                 << "</div><div style=\"color:#888;font-size:14px\">"
                 << html_escape(text(*n, "label")) << "</div></div>\n";
        } else if (n->glyph == "divider") {
            const std::string ds = field_value(*n, "divider_style");
            if (ds == "space") html << "<div style=\"height:24px\"></div>\n";
            else html << "<hr style=\"border:none;border-top:1px solid #ddd;"
                         "margin:18px 0\">\n";
        } else if (n->glyph == "link") {
            /* THE NEWSLETTER HAD NO `link` CASE AT ALL until 2026-08-19 — 17
             * anchors on the website, zero in the email, silently, for a
             * publication whose entire job is *click this to RSVP*. A
             * bulletproof button: a one-cell table wrapping a padded anchor,
             * because Outlook drops padding on inline elements. */
            const std::string tgt = href_of(field_value(*n, "target"));
            const std::string label = text(*n, "label");
            if (!tgt.empty())
                html << email_button(tgt, label.empty() ? tgt : label, acc);
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
                html << "<p style=\"color:#777;margin:4px 0\">" << prose(vcap)
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
                         << "\" width=\"572\" style=\"display:block;width:100%;"
                            "max-width:572px;height:auto;border:0\" alt=\""
                         << html_escape(ui("Play the video", "Reproducir el video"))
                         << "\"></a>\n";
                html << email_button(watch,
                                     ui("Watch on ", "Ver en ") + vid.provider_label(),
                                     acc);
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
                html << "<p style=\"color:#777;margin:4px 0\">" << prose(cap)
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

            for (const maiz::SceneNode* ev : hits) {
                const maiz::SceneNode& dn = *ev;
                /* Four things here were declared fields the render simply did
                 * not honour until a person read a real issue and said it was
                 * a wall of text: the times on a DATED event, the event's own
                 * colour, the title, and how much of the summary to show. */
                std::string when = human_date(field_value(dn, "date"), lang);
                if (when.empty()) when = field_value(dn, "days");
                const std::string st = field_value(dn, "start_time");
                const std::string et = field_value(dn, "end_time");
                std::string bar = field_value(dn, "color");
                if (bar.size() < 4 || bar[0] != '#') bar = acc;

                html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                        "cellspacing=\"0\" style=\"margin:10px 0;background:#f7f9fc;"
                        "border-left:4px solid " << bar
                     << "\"><tr><td style=\"padding:8px 12px\">"
                     << "<b>" << html_escape(title_of(dn)) << "</b>";
                std::string line = when;
                if (!st.empty()) {
                    line += (line.empty() ? "" : " \xc2\xb7 ") + st;
                    if (!et.empty()) line += "\xe2\x80\x93" + et;
                }
                const std::string venue = field_value(dn, "venue");
                if (!venue.empty())
                    line += (line.empty() ? "" : " \xc2\xb7 ") + venue;
                meta_line(line);
                const std::string vlink = field_value(dn, "virtual");
                if (!vlink.empty())
                    html << "<br>"
                         << email_button(href_of(vlink),
                                         ui("Join online", "Unirse en linea"), acc);
                if (detail != "title") {
                    std::string sum = text_or(dn, "summary", "summary");
                    if (detail != "full") sum = clip(sum, 140);
                    if (!sum.empty()) html << "<br>" << prose(sum);
                }
                html << "</td></tr></table>\n";
            }
            if (hits.empty())
                html << "<p style=\"color:#999\">"
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
                html << "<p style=\"color:#777;margin:4px 0\">" << prose(dcap)
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
                html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                        "cellspacing=\"0\" style=\"margin:8px 0;background:#f7f9fc;"
                        "border-left:4px solid " << acc
                     << "\"><tr><td style=\"padding:8px 12px\"><b>"
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
                html << "<p style=\"color:#999\">"
                     << html_escape(ui("(nobody is listed yet)",
                                       "(nadie esta en la lista)"))
                     << "</p>\n";
        } else if (n->glyph == "job_grid") {
            const std::string query = field_value(*n, "query");
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:#777;margin:4px 0\">" << prose(cap)
                     << "</p>\n";
            const std::string jdetail = field_value(*n, "detail");
            const int jlimit = hormiga::doc_field_int(*n, "limit", 0);
            std::vector<const maiz::SceneNode*> jobs;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "job" && hormiga::query_matches(query, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    jobs.push_back(&dn);
            if (jlimit > 0 && (int)jobs.size() > jlimit) jobs.resize((size_t)jlimit);
            for (const maiz::SceneNode* jp : jobs) {
                const maiz::SceneNode& dn = *jp;
                html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                        "cellspacing=\"0\" style=\"margin:10px 0;background:#f6f9f4;"
                        "border-left:4px solid #5d7d3b\">"
                        "<tr><td style=\"padding:8px 12px\">"
                     << "<b>" << html_escape(title_of(dn)) << "</b>";
                const std::string org = field_value(dn, "org");
                if (!org.empty()) html << " &middot; " << html_escape(org);
                std::string l1 = field_value(dn, "pay");
                const std::string loc = field_value(dn, "location");
                if (!loc.empty()) l1 += (l1.empty() ? "" : " \xc2\xb7 ") + loc;
                meta_line(l1);
                /* PARITY WITH THE WEBSITE, found by tools/lint_glyph_fields.py:
                 * these were declared, rendered on the site, and rendered by
                 * nothing here. The contact email matters most — a newsletter
                 * that says "email your resume" and does not say where is worse
                 * than one that omits the posting. */
                std::string l2 = field_value(dn, "job_type");
                const std::string av = field_value(dn, "availability");
                if (!av.empty()) l2 += (l2.empty() ? "" : " \xc2\xb7 ") + av;
                meta_line(l2);
                const std::string dl = field_value(dn, "deadline");
                if (!dl.empty())
                    meta_line(ui("Closes ", "Cierra ") + human_date(dl, lang));
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
            }
            if (jobs.empty())
                html << "<p style=\"color:#999\">"
                     << html_escape(ui("(no open positions)", "(no hay vacantes)"))
                     << "</p>\n";
        } else if (n->glyph == "image_grid") {
            const std::string q = field_value(*n, "query");
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:#777;margin:4px 0\">" << prose(cap)
                     << "</p>\n";
            const int glimit = hormiga::doc_field_int(*n, "limit", 0);
            const bool thumb = field_value(*n, "display") == "thumb";
            int shown = 0;
            html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                    "cellspacing=\"0\"><tr>\n";
            for (const auto& dn : data.nodes) {
                if (dn.glyph != "image" || !hormiga::query_matches(q, data, dn, today) ||
                    allo_web_hidden(dn.name))
                    continue;
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
                         << "\" style=\"max-width:280px;width:100%;"
                         << (thumb ? "height:180px;object-fit:cover;" : "")
                         << "border:0;display:block\">";
                else
                    html << "<div style=\"border:1px dashed #bbb;color:#999;"
                            "padding:22px 8px;font-size:13px\">"
                         << html_escape(ui("unpublished", "sin publicar"))
                         << "</div>";
                html << "</td>\n";
                ++shown;
            }
            if (shown % 2) html << "<td width=\"50%\"></td>\n";
            html << "</tr></table>\n";
            if (!shown)
                html << "<p style=\"color:#999\">"
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
                html << "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
                        "cellspacing=\"0\" style=\"margin:14px 0;background:#f7f9fc;"
                        "border-left:4px solid " << acc
                     << "\"><tr><td style=\"padding:12px 14px\">"
                     << "<b style=\"font-size:19px\">" << html_escape(title_of(*ev))
                     << "</b>";
                std::string line = human_date(field_value(*ev, "date"), lang);
                if (line.empty()) line = field_value(*ev, "days");
                const std::string st = field_value(*ev, "start_time");
                if (!st.empty()) {
                    line += (line.empty() ? "" : " \xc2\xb7 ") + st;
                    const std::string et = field_value(*ev, "end_time");
                    if (!et.empty()) line += "\xe2\x80\x93" + et;
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
                                "display:block\">";
                }
                const std::string ctal = field_value(*n, "cta_link").empty()
                                             ? field_value(*ev, "virtual")
                                             : field_value(*n, "cta_link");
                if (!ctal.empty()) {
                    std::string cta = text(*n, "cta");
                    if (cta.empty()) cta = ui("More about this", "Mas informacion");
                    html << email_button(href_of(ctal), cta, acc);
                }
                html << "</td></tr></table>\n";
            }
        } else if (n->glyph == "map_embed" || n->glyph == "calendar_embed") {
            /* The interactive widgets are web-only by physics: a mail client
             * runs no JavaScript. The caption still carries whatever the author
             * wanted to say, so the block is not silently nothing. */
            const std::string cap = text(*n, "caption");
            if (!cap.empty())
                html << "<p style=\"color:#777;margin:8px 0\">" << prose(cap)
                     << "</p>\n";
        } else if (n->glyph == "footer") {
            html << "<hr style=\"border:none;border-top:1px solid #ddd;margin:22px 0\">"
                 << "<p style=\"color:#888;font-size:13px;line-height:1.5\">"
                 << prose(text(*n, "text")) << "</p>\n";
        }
    }

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
    if (unpublished)
        log.push_back({"warn", "render",
                       std::to_string(unpublished) +
                           " image(s) have no public URL - they will not load in "
                           "email. Use 'Get public URL' on each, or effect publish."});

    /* `render` writes into `exports/`, which is where `--describe` had always
     * claimed it went while it was writing beside the database. */
    std::error_code ec;
    fs::create_directories(data_dir("exports"), ec);
    const fs::path out = data_dir("exports") /
                         ("preview-" + std::string(lang) + ".html");
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    o << html.str();
    if (!o) {
        log.push_back({"error", "render", "could not write " + out.string()});
        return {};
    }
    return out.string();
}
