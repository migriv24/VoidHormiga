/* section_web.cpp — the OUTPUT domain: the newsletter preview and the static
 * site. Split out of app.cpp 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * This is the section where CLAUDE.md rule 6 is enforced in code: an
 * internal-notes-class field must never reach an Output-interface holiday, and
 * `web-hide` must drop a rune from every query-backed block. Keeping the whole
 * render/export seam in ONE translation unit is the point — a privacy rule you
 * can only check by reading six files is a privacy rule nobody checks.
 */
#include "app/app_internal.hpp"
#include "render/theme.hpp"
#include "render/published.hpp" // the shared clearance gate
#include "render/text.hpp" // the helpers both output domains share
#include "domain/date_query.hpp" // date: predicates in the block grammar
#include "domain/ical.hpp"       // THE PIVOT: VEVENT <-> dated rune
#include "render/download.hpp" // a file a visitor can keep
#include "render/audio.hpp" // the audio block's markup, both domains
#include "render/video.hpp"      // a pasted video URL, understood
#include "render/image_text.hpp" // the image + text block's markup
#include "json.hpp" // theme/menu/embed payloads are JSON on the wire
#include "stb_image_write.h" // decls only - gallery thumbnails; the ONE
                             // implementation lives in app.cpp
#include "lucide_icons.hpp"  // vendored SVG icon paths (ISC; vendor/icons)
/* The website's stylesheet and script, as REAL FILES under src/render/web/,
 * embedded at build time (see tools/embed_asset.py and CMakeLists.txt). Editing
 * CSS in an editor that understands CSS is the entire point. */
#include "site_css_data.hpp"
#include "site_js_data.hpp"
std::string HormigaApp::render_site(std::string_view lang) {
    maiz::ProjectOptions io, dio;
    io.mantle = cur_doc;
    dio.mantle = kDataMantle;
    maiz::Scene issue = maiz::project_scene(core, io);
    maiz::Scene data = maiz::project_scene(core, dio);
    warn_if_data_is_elsewhere(data); // field report D6: the silent empty render
    std::string suf = std::string("_") + std::string(lang);
    std::string alt = (lang == "en") ? "_es" : "_en";
    /* ── HOW MUCH OF THIS PAGE IS ACTUALLY IN THIS LANGUAGE (2026-09-02) ─────
     *
     * `text()` falls back from `title_es` to `title_en` silently, and that
     * silence is what the 2026-09-02 field report ran into: a site can be 0%
     * translated, render `ok`, publish, and put a Spanish URL in a sitemap in
     * front of a Spanish-speaking reader with an English page behind it, and
     * nothing anywhere says a number.
     *
     * The report's proposed fix was a switch to stop building the second
     * language. The author's answer was the opposite — better translation
     * tooling, because bilingual is the resting state — so this counts the
     * fallbacks and the render says what it found. `effect translation-report`
     * (app/translate.cpp) is the other half: it writes the script that
     * closes the gap.
     *
     * The FALLBACK ITSELF STAYS. A Spanish page showing an English summary is
     * better than a Spanish page showing a blank, every time; what was missing
     * was anyone being told it happened. */
    int lang_hits = 0, lang_fallbacks = 0;
    auto text = [&](const maiz::SceneNode& n, const char* base) {
        std::string v = field_value(n, base + suf);
        if (!v.empty()) { ++lang_hits; return v; }
        std::string o = field_value(n, base + alt);
        /* AN ADDRESS HAS NO SPANISH (2026-09-03). A `label` holding
         * `migriv24@gmail.com` fell back on every Spanish render and counted
         * against a coverage figure that could therefore never reach 100% —
         * and a warning that cannot be cleared is one people stop reading. The
         * same test `translation-report` uses, so the two numbers agree. */
        if (!o.empty() && !untranslatable(o)) ++lang_fallbacks;
        return o;
    };
    /* ── THE BILINGUAL DATA HELPERS, WHICH THE WEBSITE NEVER GOT ─────────────
     *
     * These two shipped for the EMAIL renderer on 2026-08-19 and stopped
     * there. The consequence, measured on the first real website built with
     * `render-site`: 11 of 13 event cards printed a rune slug
     * ("Acme Education Open House Aug") instead of the `title_en` sitting in
     * the database ("ACME Education Committee Open House"), 0 of 13 printed a
     * summary because every event had `summary_en`/`summary_es` and none had
     * the legacy `summary` — and the Spanish page printed the same thirteen
     * English slugs, so the bilingual site had one language.
     *
     * A rune NAME is a command-safe slug. It is allowed to carry a typo
     * (`apoyo-para-quines-apoyan` shipped with one) and it is allowed to carry
     * an internal suffix (`-aug`). Published prose is allowed neither, which is
     * the whole reason `title_en` exists. Any block that displays a data rune
     * goes through `title_of`; nothing calls `humanize()` on a data rune again.
     *
     * They are declared here rather than shared with `render_preview` because
     * `suf`/`alt` differ per render and the closure is three lines; the RULE is
     * what has to be shared, and it is written down above. */
    auto text_or = [&](const maiz::SceneNode& n, const char* base, const char* legacy) {
        std::string v = text(n, base);
        return v.empty() ? field_value(n, legacy) : v;
    };
    auto title_of = [&](const maiz::SceneNode& n) {
        std::string v = text(n, "title");
        return v.empty() ? humanize(n.name) : v;
    };
    /* A published HEADING for a data rune, in priority order: this page's
     * language (`title_en`/`title_es`), then `display_name`, then the humanized
     * slug as the last resort.
     *
     * `title_of` cannot serve here because it skips `display_name` entirely, so
     * a rune carrying one but no `title_en` would fall all the way through to
     * the slug. Both fields are real and they answer different questions —
     * `display_name` is what this thing is CALLED, `title_en`/`title_es` is
     * what to PRINT on a page in a given language — and the ordering above is
     * the only one where neither can hide the other. */
    auto heading_of = [&](const maiz::SceneNode& n) {
        std::string v = text(n, "title");
        if (v.empty()) v = field_value(n, "display_name");
        return v.empty() ? humanize(n.name) : v;
    };
    /* ── THE CHROME HAS A LANGUAGE TOO (2026-08-20) ──────────────────────────
     *
     * Reported three times before it was fixed, which is itself the lesson: a
     * Spanish page printed "This directory is empty. (Entries appear once they
     * are marked as public.)" — English, on the half of a bilingual site whose
     * whole reason for existing is that its readers may not read English.
     *
     * Every string a VISITOR reads was language-selected except the ones the
     * renderer writes itself. Those are not incidental — an empty state is the
     * page a person sees on the day nothing has been published yet, which is
     * exactly when a site is being judged.
     *
     * A table rather than scattered ternaries so that adding a third language
     * is one column and so that `tools/lint_i18n.py` has something to check
     * against. */
    auto ui = [&](const char* en, const char* es) {
        return std::string(lang == "es" ? es : en);
    };
    /* ── WHAT TODAY IS, ONCE PER RENDER (2026-08-28) ────────────────────────
     *
     * Every query-backed block on every page of both languages resolves against
     * the SAME day. Reading the clock per block would let a render that starts
     * at 23:59:59 put an event in "Coming up" on the English page and in the
     * archive on the Spanish one — a one-second window, and the kind of bug
     * that is found by a reader rather than by a test. See date_query.hpp. */
    const long long today = hormiga::today_days();

    // grid order once migrated; the legacy adjacency chain until then (B1)
    auto chain = hormiga::doc_order(issue);

    /* The site's name. Computed just below, once the pages are known — it is
     * the HOME page's hero, and finding the home page is what the next block
     * does. See the note there for why that matters. */
    std::string site_title;

    // ── PAGES (W2, author 2026-07-23): a website is many pages. `page` runes
    // carry each page's title/slug/order/nav; a component's `page` field names
    // the page it belongs to (empty = the HOME page). No page runes = the
    // legacy single-page site (fully backward-compatible). ──────────────────
    struct PageInfo {
        std::string slug, title, desc;
        /* THE TITLE A SHARE CARD SHOWS, which is not always the title in the
         * nav (2026-09-02, field report A6). A home page reasonably called
         * "Home" in a five-item menu shares to social as "Home" — the one word
         * least likely to make anyone click. The two jobs genuinely differ: a
         * nav label is read in the context of the site it is on, and a share
         * card is read in a feed with no context at all.
         *
         * Empty falls back to the page title, so nothing changes for a site
         * that does not set it. */
        std::string social;
        int order;
        bool in_nav;
    };
    std::vector<PageInfo> pages;
    for (const auto& n : issue.nodes)
        if (n.glyph == "page") {
            PageInfo p;
            p.slug = field_value(n, "slug");
            if (p.slug.empty()) p.slug = n.name;
            p.title = text(n, "title");
            if (p.title.empty()) p.title = p.slug;
            p.desc = field_value(n, "meta_desc");
            p.social = text(n, "social_title");
            p.order = hormiga::doc_field_int(n, "order", 0);
            p.in_nav = field_value(n, "in_nav") != "0";
            pages.push_back(p);
        }
    std::sort(pages.begin(), pages.end(),
              [](const PageInfo& a, const PageInfo& b) { return a.order < b.order; });
    std::string home_slug = pages.empty() ? "" : pages.front().slug;

    /* ── THE SITE'S NAME IS THE HOME PAGE'S HERO ─────────────────────────────
     *
     * This was "the first `hero` in document order" — a GLOBAL sort across
     * every page, whose ties fall to the order runes happen to sit in the
     * array. That was survivable only while heroes were banded 20 rows apart.
     *
     * Then the Builder's row migration renumbered each page from 0, which is
     * correct and which put five heroes on row 0 at once. The tie broke toward
     * the home page's hero by luck — the field agent checked, and reported that
     * had the Events hero been minted first, every page on that site would have
     * been titled "Events". A `<title>` on every page of a live community
     * website is not a thing to leave to array order.
     *
     * A hero belongs to a page (`page` empty means home, the same rule every
     * other component follows), so the home page's hero is askable directly and
     * no migration can move it. The global scan stays as the fallback for a
     * legacy single-page site, which has no `page` runes and where "the first
     * hero" is genuinely the only answer there is. */
    for (const auto* n : chain) {
        if (n->glyph != "hero" || !site_title.empty()) continue;
        const std::string pg = field_value(*n, "page");
        if (pg.empty() || pg == home_slug) site_title = text(*n, "title");
    }
    if (site_title.empty())   // legacy single-page site: no `page` runes at all
        for (const auto* n : chain)
            if (n->glyph == "hero" && site_title.empty())
                site_title = text(*n, "title");
    if (site_title.empty()) site_title = "Community Site";
    /* A page's filename, in a NAMED language. The second argument exists so the
     * chrome can point at a page's twin — the language switch and the
     * `hreflang` alternates both need the other language's name for THIS page,
     * and reconstructing it by string surgery on the current filename is the
     * kind of thing that works until a slug ends in "-en". */
    auto page_file_in = [&](const std::string& slug, std::string_view lg) {
        return (slug.empty() || slug == home_slug)
                   ? "index-" + std::string(lg) + ".html"
                   : slug + "-" + std::string(lg) + ".html";
    };
    auto page_file = [&](const std::string& slug, std::string_view lg = {}) {
        return page_file_in(slug, lg.empty() ? lang : lg);
    };

    // shared style resolution for the embed widgets (glyph default → the
    // referenced view's rules → explicit tags), emitted as a CSS hex
    auto style_hex = [&](const maiz::SceneNode& dn,
                         const std::vector<MapRule>& rules) {
        unsigned col = dn.glyph == "incident"       ? IM_COL32(200, 50, 50, 255)
                       : dn.glyph == "organization" ? IM_COL32(138, 109, 59, 255)
                       : dn.glyph == "event"        ? IM_COL32(63, 111, 174, 255)
                                                    : IM_COL32(179, 89, 46, 255);
        for (const auto& r : rules) {
            if (r.tags.empty() || !maiz::node_matches(rule_expr_of(r), dn))
                continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) col = c.col;
            break;
        }
        std::string ct = tag_value(dn, "color");
        for (const auto& c : kMarkerColors)
            if (ct == c.tag) col = c.col;
        char hex[10];
        std::snprintf(hex, sizeof hex, "#%02x%02x%02x", col & 0xFF,
                      (col >> 8) & 0xFF, (col >> 16) & 0xFF);
        /* THE `color` FIELD BEATS ALL OF IT (2026-08-19). This read only a
         * `color:` TAG, so `event.color` — a declared field the inspector edits
         * and `--describe` advertises — reached the map and the calendar
         * nowhere. Measured: five agenda entries on a real home page all
         * carried the glyph default `#3f6fae` while two of the events had
         * `#2563eb` and `#eab308` set. "A calendar of four organizations is not
         * four identical blue blocks" is the promise the field was added for.
         *
         * Last because it is the most specific: a rule states a class, a tag
         * annotates a rune, and the field is this rune's own answer. */
        const std::string cf = field_value(dn, "color");
        if (cf.size() >= 4 && cf[0] == '#') return cf;
        return std::string(hex);
    };
    std::string ics;       // RFC 5545 body, built when a calendar block renders
    bool want_ics = false; // → site/calendar.ics (import into Google/Apple/…)
    const std::string ics_stamp = hormiga::ical::now_utc(); // one DTSTAMP per render
    std::string cb = "?v=" + std::to_string((long long)std::time(nullptr));
    /* THE OPERATOR'S OWN STYLESHEET, staged before any page is written so every
     * page of every language agrees about whether there is one. See
     * `stage_custom_css()` in render/assets.cpp for the whole argument; the
     * short form is that `fonts/` already works exactly this way. */
    const bool has_custom_css = stage_custom_css();
    SiteTheme site_th = read_site_theme(core); // config = the theme's truth
    std::error_code ec;
    fs::create_directories(data_dir("site"), ec);
    // ship the vendored webfonts alongside style.css (@font-face → fonts/*.woff2).
    // Self-contained: the deployed site carries its own fonts, no CDN. Copied
    // from the app's vendor drop; skipped silently if a face is missing.
    {
        /* FROM WHERE THE BINARY SHIPS, NOT FROM THE DATA FOLDER (2026-08-19).
         *
         * This was `fs::current_path() / "vendor" / ...`, and `current_path()`
         * is the folder holding the `.miga` — which is not a checkout and has
         * no `vendor/`. So `if (exists)` stepped over it in silence and the
         * deploy went out with an empty `site/fonts/` behind `@font-face` rules
         * pointing at nothing. Reproduced by deleting `site/fonts` and
         * re-rendering: the render reported success and wrote no font.
         *
         * The comment two lines below is the promise this broke — and it broke
         * it invisibly, into a fallback that looks fine on a developer's
         * machine, where the two paths are the same folder. */
        /* THE STAGED LAYOUT IS `vendor/fonts/` SINCE 2026-09-04, and the
         * reason is that `fonts/` beside the binary and `fonts/` beside the
         * DATABASE are two different folders with one name -- the second is
         * declared in `void.json` as the organization's own faces and is read
         * twenty lines below. They are only distinguishable when
         * `ship_dir != base_dir`, which is exactly the case a developer never
         * sees. `fonts/web` stays first as the legacy layout, so a copy staged
         * by an older build still renders. */
        std::vector<fs::path> roots;
        if (!ship_dir.empty()) {
            roots.push_back(ship_dir / "fonts" / "web");             // pre-0.1.0 layout
            roots.push_back(ship_dir / "vendor" / "fonts" / "web");  // staged beside the EXE
        }
        roots.push_back(fs::current_path() / "vendor" / "fonts" / "web"); // checkout
        fs::path fdst = data_dir("site") / "fonts";
        fs::create_directories(fdst, ec);
        int faces = 0;
        for (const auto& fsrc : roots) {
            if (!fs::exists(fsrc, ec)) continue;
            for (const auto& de : fs::directory_iterator(fsrc, ec))
                if (de.path().extension() == ".woff2") {
                    fs::copy_file(de.path(), fdst / de.path().filename(),
                                  fs::copy_options::overwrite_existing, ec);
                    ++faces;
                }
            if (faces) break;
        }
        /* THE ORGANIZATION'S OWN FACES travel from beside the DATABASE, which
         * is the one place a file that belongs to this org rather than to
         * Hormiga can live. Staged after the vendored ones so an org may
         * override a shipped face by name if it ever wants to. */
        for (const auto& de : fs::directory_iterator(data_dir("fonts"), ec))
            if (de.path().extension() == ".woff2") {
                fs::copy_file(de.path(), fdst / de.path().filename(),
                              fs::copy_options::overwrite_existing, ec);
                site_th.font_files.push_back(de.path().filename().string());
                ++faces;
            }
        /* SAY SO WHEN THE PROMISE CANNOT BE KEPT. A silent fallback to the
         * visitor's system fonts is the failure this whole comment is about;
         * it is not fatal, and it is not something to discover from a deployed
         * page. */
        if (!faces)
            log.push_back({"warn", "render",
                           "no webfonts found beside the binary - the site will "
                           "fall back to system fonts"});
    }

    // ── W5 site chrome & meta: a generated FAVICON (accent square + the site's
    // initial, an inline SVG data-URI — no asset to ship), a base URL for
    // absolute og:url/sitemap (config site.base_url; "" = relative), and a
    // site-wide default description. ─────────────────────────────────────────
    auto cfg = [&](const char* k) {
        std::string v = core.dispatch(std::string("config get ") + k).data;
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        return v == "null" ? std::string() : v;
    };
    std::string base_url = cfg("site.base_url"); // e.g. https://our-org.org
    if (!base_url.empty() && base_url.back() == '/') base_url.pop_back();
    std::string site_desc = cfg("site.desc");
    // `site.languages` was accepted, stored and read by nothing (portfolio
    // report D1). The check lives in app/translate.cpp, the file that would
    // have honoured the key and whose header the misreading came from.
    if (lang == hormiga::site_langs().front()) warn_unread_language_key();
    /* The colophon under the footer: `config site.colophon`. Unset prints the
     * Void Hormiga credit, any value replaces it, "none" prints nothing —
     * "none" and not "" because `config get` hands back the same empty string
     * for an unset key as for one set to empty (2026-08-31). */
    std::string colophon = cfg("site.colophon");
    if (colophon.empty()) colophon = "Built with Void Hormiga";
    else if (colophon == "none") colophon.clear();
    /* ── THE ORGANIZATION'S OWN MARK (2026-08-19) ────────────────────────────
     *
     * `config org.logo` has existed and the map export has used it; the website
     * read it nowhere, so an organization with an icon got the generated
     * accent-square-and-initial favicon — a good default and a bad outcome for
     * a group whose logo is already on its fliers.
     *
     * A PAIR, not one file, and this is the part that is easy to get wrong:
     * with `theme.dark = 1` the header background flips, and a black wordmark
     * disappears into it. `org.logo_dark` is optional and falls back to the
     * light one, so an org with a single mark that reads on both loses
     * nothing. */
    std::string logo_rel = stage_site_asset(cfg("org.logo"));
    std::string logo_dark_rel = stage_site_asset(cfg("org.logo_dark"));
    if (logo_dark_rel.empty()) logo_dark_rel = logo_rel;
    /* THE LOGO IS DELIBERATELY NOT DOWNSCALED, and this warning is the reason
     * it is safe to say so. A mark is very often a transparent PNG; the
     * derivative path composites onto white because JPEG has no alpha, which
     * is right for a photograph on a page and wrong for a logo that has to sit
     * on a header of any colour. So the file is served as given — and a 5 MB
     * mark drawn 34px tall is then on every page load, which is the thing the
     * operator has to fix rather than the renderer. */
    for (const std::string& lg : {logo_rel, logo_dark_rel}) {
        if (lg.empty()) continue;
        std::error_code lec;
        const auto sz = fs::file_size(data_dir("site") / lg, lec);
        if (!lec && sz > 400u * 1024u) {
            log.push_back({"warn", "render",
                           lg + " is " + std::to_string(sz / 1024) +
                               " KB and loads on every page. A logo is not "
                               "downscaled automatically (a mark is usually "
                               "transparent and would composite onto white) - "
                               "export it around 512px wide."});
            break;
        }
    }
    std::string favicon;
    if (!logo_rel.empty()) {
        favicon = logo_rel; // the org's mark wins over the generated square
    } else {
        char init = site_title.empty() ? 'H' : (char)std::toupper(site_title[0]);
        // build the SVG then percent-encode it for a data URI (single quotes so
        // the HTML attribute stays clean; encode #, <, >, space, %)
        std::string svg =
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>"
            "<rect width='64' height='64' rx='14' fill='" + site_th.accent +
            "'/><text x='32' y='45' font-size='36' font-family='sans-serif' "
            "font-weight='bold' fill='white' text-anchor='middle'>" +
            std::string(1, init) + "</text></svg>";
        std::string enc;
        for (char c : svg) {
            if (c == '#') enc += "%23";
            else if (c == '<') enc += "%3C";
            else if (c == '>') enc += "%3E";
            else if (c == ' ') enc += "%20";
            else if (c == '%') enc += "%25";
            else if (c == '"') enc += "%22";
            else enc += c;
        }
        favicon = "data:image/svg+xml," + enc;
    }

    /* ── EVERY IMAGE THIS RENDER ACTUALLY PUT ON A PAGE (2026-08-28) ────
     *
     * Collected so the staleness check below can be about what a
     * VISITOR will see rather than about what the database happens to hold. A
     * flier for a finished event sitting unused in the library is not a
     * problem; the same flier on the home page nine days later is the problem
     * the field report opened with, and only this set tells them apart. */
    std::set<std::string> published_images;

    // ── render ONE page to its file: components on the page, its own section
    // anchors, the cross-page nav (or section nav in single-page mode) ───────
    auto render_page = [&](const std::string& slug, const std::string& ptitle_in) {
        std::vector<const maiz::SceneNode*> pchain; // this page's components
        for (const auto* n : chain) {
            std::string pg = field_value(*n, "page");
            if (pg == slug || (pg.empty() && slug == home_slug))
                pchain.push_back(n);
        }
        std::string title = ptitle_in;
        for (const auto* n : pchain)
            if (n->glyph == "hero" && title.empty()) title = text(*n, "title");
        if (title.empty()) title = site_title;

        // nav: cross-PAGE links in multi-page mode (active = current), else the
        // in-page section anchors. Section headers get ids either way.
        std::map<const maiz::SceneNode*, std::string> ids;
        int anchor = 0;
        std::vector<std::tuple<std::string, std::string, bool>> nav; // href,label,active
        if (!pages.empty())
            for (const auto& p : pages) {
                if (p.in_nav)
                    nav.push_back({page_file(p.slug), p.title, p.slug == slug});
            }
        for (const auto* n : pchain)
            if (n->glyph == "section_header") {
                std::string id = "s" + std::to_string(++anchor);
                ids[n] = id;
                if (pages.empty()) nav.push_back({"#" + id, text(*n, "title"), false});
            }

        // meta description: the page's own, else the first narrative on the
        // page, else the site default. og:image: the page's hero banner.
        std::string desc, social_title;
        for (const auto& p : pages)
            if (p.slug == slug) { desc = p.desc; social_title = p.social; }
        if (desc.empty())
            for (const auto* n : pchain)
                if (n->glyph == "narrative") { desc = text(*n, "text"); break; }
        if (desc.empty()) desc = site_desc;
        /* A share card carries the site's name with it or it carries nothing
         * useful: `og:site_name` is a hint most feeds do not render, so a bare
         * "Home" arrives as a bare "Home". The `<title>` tag already solves
         * this by concatenating — this makes `og:title` agree with it, unless
         * the operator stated a share title, in which case they have said
         * exactly what they want and it is used verbatim. */
        if (social_title.empty())
            social_title = title == site_title ? title : title + " - " + site_title;
        std::string og_img;
        for (const auto* n : pchain)
            if (n->glyph == "hero") {
                std::string im = field_value(*n, "image");
                if (!im.empty()) og_img = im;
                break;
            }
        std::string page_url = base_url.empty() ? "" : base_url + "/" + page_file(slug);

        std::string footer_text;
        std::ostringstream h;
        h << "<!doctype html><html lang=\"" << lang << "\"><head>\n"
          << "<meta charset=\"utf-8\">\n"
          << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
          << "<title>" << html_escape(title) << " - " << html_escape(site_title)
          << "</title>\n";
        if (!desc.empty())
            h << "<meta name=\"description\" content=\"" << html_escape(desc)
              << "\">\n";
        h << "<link rel=\"icon\" href=\"" << favicon << "\">\n"
          // Open Graph: the social-sharing preview card
          << "<meta property=\"og:type\" content=\"website\">\n"
          << "<meta property=\"og:site_name\" content=\"" << html_escape(site_title)
          << "\">\n"
          << "<meta property=\"og:title\" content=\"" << html_escape(social_title)
          << "\">\n";
        if (!desc.empty())
            h << "<meta property=\"og:description\" content=\"" << html_escape(desc)
              << "\">\n";
        if (!page_url.empty())
            h << "<meta property=\"og:url\" content=\"" << html_escape(page_url)
              << "\">\n";
        if (!og_img.empty() && !base_url.empty())
            h << "<meta property=\"og:image\" content=\"" << html_escape(base_url)
              << "/" << html_escape(og_img) << "\">\n";
        /* hreflang ALTERNATES (2026-08-19): a bilingual site whose two halves
         * never point at each other reads to a search engine as two unrelated
         * sites, and the Spanish half — which was also missing from the sitemap
         * — simply does not get found. `x-default` sends a visitor whose
         * browser asks for neither to the English page, which is the same
         * choice the generated root `index.html` makes. */
        h << "<link rel=\"alternate\" hreflang=\"en\" href=\""
          << html_escape(base_url.empty() ? page_file(slug, "en")
                                          : base_url + "/" + page_file(slug, "en"))
          << "\">\n"
          << "<link rel=\"alternate\" hreflang=\"es\" href=\""
          << html_escape(base_url.empty() ? page_file(slug, "es")
                                          : base_url + "/" + page_file(slug, "es"))
          << "\">\n"
          << "<link rel=\"alternate\" hreflang=\"x-default\" href=\""
          << html_escape(base_url.empty() ? page_file(slug, "en")
                                          : base_url + "/" + page_file(slug, "en"))
          << "\">\n";
        h << "<meta name=\"twitter:card\" content=\"summary_large_image\">\n"
          << "<link rel=\"stylesheet\" href=\"style.css" << cb << "\">\n";
        /* ── THE OPERATOR'S TWO LINES OF CSS (2026-09-02) ────────────────────
         *
         * Field report Part 3: `render_site` unconditionally writes `style.css`,
         * so three separate two-line cosmetic defects on a live public site were
         * each blocked on a Hormiga release, because there was nowhere for an
         * operator to put two lines of CSS.
         *
         * NOT a loosening of the render seam, and the distinction is the design.
         * A `custom_css` FIELD would be a way to put markup — and therefore
         * script — on a public page through the model, which is what `video`
         * already refuses. A FILE beside the database is the operator's own
         * machine and their own hand, and a `<link rel=stylesheet>` cannot
         * execute anything whatever it contains. The pattern is `fonts/`, which
         * has worked exactly this way since it shipped. See `stage_custom_css()`
         * in render/assets.cpp. Linked AFTER our own stylesheet, so later wins
         * in the cascade and it is an override rather than a default. */
        if (has_custom_css)
            h << "<link rel=\"stylesheet\" href=\"custom.css" << cb << "\">\n";
        /* ── AND THE PAGE IS READABLE WITH JAVASCRIPT OFF (2026-09-02) ───────
         *
         * Field report D1, true of every site this renderer has ever produced.
         * `.reveal` starts at `opacity:0` and only `app.js` adds `.in`, so a
         * visitor with scripting disabled got the header, the hero and the
         * buttons and an empty page beneath them — every narrative, header,
         * quote, stat, card and gallery tile invisible, with nothing to suggest
         * anything was missing.
         *
         * It turns the animation OFF rather than reproducing it: a no-JS visitor
         * is owed the content, not the choreography, which is the same call
         * `@media(prefers-reduced-motion:reduce)` already makes. */
        h << "<noscript><style>.reveal{opacity:1;transform:none}</style>"
             "</noscript>\n";
        h << "</head>\n<body class=\"" << site_th.body_classes()
          << "\" style=\"--bdim:" << (site_th.banner_dim / 100.0) << "\">\n"
          << "<!-- generated by Void Hormiga: effect render-site " << lang << " -->\n";
        h << "<header class=\"site-head\"><div class=\"wrap\">\n"
          << "<a class=\"brand\" href=\"" << (pages.empty() ? "#top" : page_file(home_slug))
          << "\">";
        // the org's own mark when it has one, with the wordmark still beside it
        // — people search for the name, not the logo. The dark twin is swapped
        // by CSS, never by JS: a logo that flickers on load is worse than none.
        if (!logo_rel.empty())
            h << "<img class=\"brand-mark light\" src=\"" << html_escape(logo_rel)
              << "\" alt=\"" << html_escape(site_title) << "\">"
              << "<img class=\"brand-mark dark\" src=\"" << html_escape(logo_dark_rel)
              << "\" alt=\"\" aria-hidden=\"true\">";
        h << "<span class=\"brand-name\">" << html_escape(site_title)
          << "</span></a>\n";
        if (!nav.empty()) {
            h << "<nav>";
            for (auto& [href, label, active] : nav)
                h << "<a class=\"" << (active ? "on" : "")
                  << "\" href=\"" << html_escape(href) << "\">"
                  << html_escape(label) << "</a>";
            /* ── THE DOOR BETWEEN THE LANGUAGES (2026-08-19) ─────────────────
             *
             * Every page came out in two languages and the generated chrome had
             * no way from one to the other: each language's nav links only
             * within its own language, and a `link` block cannot express the
             * switch because `target` is single-valued — a button authored as
             * "Español → index-es.html" renders on the SPANISH page too,
             * pointing at Spanish.
             *
             * For a project whose house rule is "both languages, always", the
             * switch is CHROME, not content. It belongs here, generated, beside
             * the nav — the same reasoning that generates the nav rather than
             * making every site author one. */
            const std::string other = (lang == "en") ? "es" : "en";
            h << "<a class=\"langswitch\" href=\"" << html_escape(page_file(slug, other))
              << "\" hreflang=\"" << other << "\" rel=\"alternate\">"
              << (other == "es" ? "Espa&ntilde;ol" : "English") << "</a>";
            h << "</nav>";
        }
        h << "</div></header>\n<main id=\"top\" class=\"wrap\">\n";

    // resolve a link target (author 2026-07-23: AUTHORED navigation). A page
    // slug → that page's file; a URL (http… or /… or #…) → itself; empty → "".
    auto link_href = [&](const std::string& target) -> std::string {
        if (target.empty()) return "";
        if (target.rfind("http", 0) == 0 || target[0] == '/' || target[0] == '#')
            return target;
        for (const auto& p : pages) // a known page slug
            if (p.slug == target) return page_file(target);
        return target; // an unknown slug: emit as-is (page may exist later)
    };
    // emit ONE component's markup (the body below). The driver loop groups by
    // grid ROW so col/span translate to the site: a multi-component row becomes
    // a horizontal band (CSS grid), each component sized by its span — the
    // builder's side-by-side layout now renders side-by-side (author 2026-07-23).
    // A component's link_to wraps it in an <a> ("anything can be a button").
    /* `platform` is read by two blocks - `download` and `link` - and the
     * vocabulary and its complaint both live in render/download.hpp. Reading it
     * HERE, per block, means the complaint is said once whether or not the
     * block ended up in a platform set. */
    auto platform_of = [&](const maiz::SceneNode& n) {
        std::string w;
        const std::string t =
            hormiga::platform_field(field_value(n, "platform"), &w);
        if (!w.empty()) log.push_back({"warn", "render", n.name + ": " + w});
        return t;
    };

    auto emit = [&](const maiz::SceneNode* n) {
        std::string lt = link_href(field_value(*n, "link_to"));
        if (!lt.empty())
            h << "<a class=\"clickable\" href=\"" << html_escape(lt) << "\">\n";
        struct LinkGuard {
            std::ostringstream& h;
            bool on;
            ~LinkGuard() { if (on) h << "</a>\n"; }
        } lg{h, !lt.empty()};
        if (n->glyph == "link") { // the explicit link / button element
            std::string tgt = link_href(field_value(*n, "target"));
            std::string cls = field_value(*n, "link_style") == "text" ? "navlink"
                                                                      : "btn";
            h << "<a class=\"" << cls << "\" href=\"" << html_escape(tgt) << "\""
              << hormiga::platform_attr(platform_of(*n)) << ">"
              << html_escape(text(*n, "label")) << "</a>\n";
        } else if (n->glyph == "quote") { // W3: a pull-quote / testimonial
            h << "<blockquote class=\"pullquote reveal\"><p>"
              << html_escape(text(*n, "text")) << "</p>";
            std::string au = field_value(*n, "author");
            if (!au.empty()) h << "<cite>" << html_escape(au) << "</cite>";
            h << "</blockquote>\n";
        } else if (n->glyph == "stat") { // W3: a big metric + label
            h << "<div class=\"stat reveal\"><span class=\"stat-num\">"
              << html_escape(field_value(*n, "number")) << "</span>"
              << "<span class=\"stat-label\">" << html_escape(text(*n, "label"))
              << "</span></div>\n";
        } else if (n->glyph == "divider") { // W3: a visual separator
            std::string ds = field_value(*n, "divider_style");
            if (ds == "space") h << "<div class=\"divider space\"></div>\n";
            else if (ds == "dots") h << "<div class=\"divider dots\"></div>\n";
            else if (ds == "bar") {
                /* A COLOURED BAR (2026-09-02, field report A6): a brand's
                 * repeating swatch strip, which `line|dots|space` had nowhere
                 * to put. `colors` empty means the theme accent, so
                 * `divider_style bar` alone is already useful. A flex row of
                 * spans rather than one `linear-gradient`, because equal-width
                 * stops need doubled percentages per colour and that is
                 * unreadable in the one place an operator would check it.
                 * `css_colors` (render/text.hpp) validates rather than escapes
                 * — see there for why — and hands back what it dropped so the
                 * render can say so. */
                std::vector<std::string> bad;
                const std::vector<std::string> swatch =
                    css_colors(field_value(*n, "colors"), &bad);
                for (const std::string& c : bad)
                    log.push_back({"warn", "render",
                                   n->name + ": divider colour '" + c +
                                       "' is not a hex value or a CSS colour "
                                       "name and was dropped"});
                const std::string hgt = field_value(*n, "bar_height");
                const int hpx = hgt.empty()
                                    ? 10
                                    : std::max(2, std::min(80, std::atoi(hgt.c_str())));
                h << "<div class=\"divider bar reveal\" style=\"height:" << hpx
                  << "px\">";
                if (swatch.empty())
                    h << "<span style=\"background:var(--accent)\"></span>";
                else
                    for (const std::string& c : swatch)
                        h << "<span style=\"background:" << c << "\"></span>";
                h << "</div>\n";
            }
            else h << "<hr class=\"divider line\">\n";
        } else if (n->glyph == "hero") {
            // modern full-bleed banner: image behind a legibility gradient with
            // the title over it (plain, centered title when there's no image)
            std::string banner = stage_site_asset(field_value(*n, "image"));
            /* A SUBTITLE, because the drawn layout's hero is a highlighted
             * event: title, then an address line, over an image. Without it the
             * second line has to become a separate `narrative`, which loses the
             * over-image placement that is the whole reason to use a hero. */
            const std::string sub = text(*n, "subtitle");
            /* A hero may state its own treatment; empty falls back to the
             * theme's, so a site has one look by default and a single striking
             * photo can still be left alone. */
            std::string hfil = field_value(*n, "image_filter");
            if (hfil.empty() || hfil == "theme") hfil = site_th.filter_class();
            else if (hfil == "none") hfil.clear();
            else hfil = "f-" + hfil;
            const std::string hdim = field_value(*n, "image_dim");
            /* THE PORTRAIT IS IN FRONT, WHICH IS THE WHOLE POINT (2026-09-03).
             * `image` is the background and always was; a face put there gets
             * cropped to the band. This is the round inset over it. It works
             * with or without a banner, because "a portrait on a plain themed
             * hero" is the other half of what was asked for. */
            const std::string portrait = stage_site_asset(field_value(*n, "portrait"));
            auto hero_portrait = [&] {
                if (portrait.empty()) return;
                h << "<img class=\"hero-portrait\" src=\"" << html_escape(portrait)
                  << "\" alt=\"" << html_escape(text(*n, "title")) << "\">";
            };
            if (!banner.empty()) {
                h << "<section class=\"hero banner" << (portrait.empty() ? "" : " withp")
                  << "\"";
                if (!hdim.empty())
                    h << " style=\"--bdim:" << (std::atoi(hdim.c_str()) / 100.0)
                      << "\"";
                h << "><div class=\"hero-bg " << hfil << "\" "
                     "style=\"background-image:url('"
                  << html_escape(banner) << "')\"></div>"
                  << "<div class=\"hero-shade\"></div><div class=\"hero-in\">";
                hero_portrait();
                h << "<h1>" << html_escape(text(*n, "title")) << "</h1>";
                if (!sub.empty())
                    h << "<p class=\"hero-sub\">" << html_escape(sub) << "</p>";
                h << "</div></section>\n";
            } else {
                h << "<section class=\"hero plain" << (portrait.empty() ? "" : " withp")
                  << "\"><div class=\"hero-in\">";
                hero_portrait();
                h << "<h1>" << html_escape(text(*n, "title")) << "</h1>";
                if (!sub.empty())
                    h << "<p class=\"hero-sub\">" << html_escape(sub) << "</p>";
                h << "</div></section>\n";
            }
        } else if (n->glyph == "section_header") {
            h << "<h2 id=\"" << ids[n] << "\" class=\"section reveal\">"
              << html_escape(text(*n, "title")) << "</h2>\n";
        } else if (n->glyph == "narrative") {
            /* A HEADING ABOVE THE PROSE (2026-09-03, portfolio report A9).
             * `narrative` was one `<p>`, so a role, an employer, a date range
             * and four bullets were one paragraph at one weight — the client
             * reached for `::first-line{font-weight:800}`, which means "the
             * first line of this paragraph is secretly a heading". An `<h3>`
             * says it instead. `h2.section` stays the page's own structure;
             * this is a level below it, inside a block. */
            const std::string nhead = text(*n, "heading");
            const std::string nicon =
                site_th.icons ? hormiga::icons::svg(field_value(*n, "icon"), "ico prose-ico")
                              : std::string();
            if (!nhead.empty())
                h << "<h3 class=\"prose-heading reveal\">" << nicon << html_escape(nhead)
                  << "</h3>\n";
            else if (!nicon.empty())
                h << "<div class=\"prose-icon reveal\">" << nicon << "</div>\n";
            /* ── THE SAME FIELD, RENDERED THE SAME WAY, IN BOTH DOMAINS ──────
             *
             * Field report D4 (2026-09-02). The email emitted
             * `white-space:pre-line` + `prose()`; this emitted `html_escape()`
             * and no `pre-line`. One field, one text, two outputs — the email
             * kept the author's line breaks and linkified bare URLs and
             * addresses, the website did neither. AGENT-GUIDE §8 documents the
             * linkification without distinguishing the two, and the guide is
             * what an agent trusts, so the fix is to make the guide true rather
             * than to narrow it.
             *
             * What the divergence cost, measured: a site with no lists at all —
             * two eighteen-track tracklists as one hyphen-joined paragraph
             * each, and every ordinary paragraph its own block rune.
             *
             * `prose()` is escape-THEN-linkify, so this opens nothing. */
            h << "<p class=\"prose pre-line reveal\">" << web_prose(text(*n, "text"))
              << "</p>\n";
        } else if (n->glyph == "image_text") {
            // a picture and the words that belong with it; markup in
            // render/image_text.hpp, fields read here where the linter looks
            h << hormiga::image_text_web(
                html_escape(stage_site_asset(field_value(*n, "image"))),
                html_escape(text(*n, "alt")), html_escape(text(*n, "heading")),
                site_th.icons ? hormiga::icons::svg(field_value(*n, "icon"), "ico prose-ico")
                              : std::string(),
                web_prose(text(*n, "text")), field_value(*n, "side") == "right");
        } else if (n->glyph == "event_grid") {
            /* THE SAME BLOCK THE EMAIL GOT ON 2026-08-19, which stopped there.
             * `title_of`/`text_or`/`detail`/`limit`/`sort`/`color` were all
             * built for `render_preview` and none of them reached here, so the
             * first real website printed 11 rune slugs, 0 summaries, and the
             * same 13 English cards on the Spanish page. The sort was document
             * order under a `sort date` the block declared and advertised.
             *
             * The resolution and the ordering are IDENTICAL to the email's by
             * construction, not by coincidence: same helpers, same defaults,
             * same fallbacks — one issue, two domains, which is what "one block
             * graph, many outputs" has to mean if it means anything. */
            std::string q = field_value(*n, "query");
            std::string cap = text(*n, "caption");
            const std::string detail = field_value(*n, "detail");
            const int limit = hormiga::doc_field_int(*n, "limit", 0);
            const std::string sort = field_value(*n, "sort");

            std::vector<const maiz::SceneNode*> evs;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "event" && hormiga::query_matches(q, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    evs.push_back(&dn);
            if (sort == "name") {
                std::stable_sort(evs.begin(), evs.end(),
                                 [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                                     return title_of(*a) < title_of(*b);
                                 });
            } else {
                const bool desc = sort == "date-desc";
                std::stable_sort(evs.begin(), evs.end(),
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
            if (limit > 0 && (int)evs.size() > limit) evs.resize((size_t)limit);

            if (!cap.empty())
                h << "<p class=\"meta caption\">" << html_escape(cap) << "</p>\n";
            /* THE SEARCH BOX IS OPTIONAL (2026-08-20). `app.js` injects a live
             * filter above every `.cards.filterable` group, which is a gift on
             * a page of forty events and clutter above three. The author:
             * *"i should have the option to show search bar or not — mostly if
             * i'm displaying 2 or 3 things."*
             *
             * `auto` is the default and means what it says: the box appears
             * once there are enough rows for finding to beat reading. Five is
             * the threshold — below it a visitor's eye is faster than the
             * keyboard. */
            const std::string ev_search = field_value(*n, "search");
            const bool ev_filter = ev_search == "on" ||
                                   (ev_search != "off" && evs.size() >= 5);
            h << "<div class=\"cards" << (ev_filter ? " filterable" : "")
              << " reveal\">\n";
            for (const maiz::SceneNode* ev : evs) {
                const maiz::SceneNode& dn = *ev;
                const std::string st = field_value(dn, "start_time");
                const std::string et = field_value(dn, "end_time");
                // the card is filterable by the SEARCH BOX app.js generates, so
                // the visible prose has to be in the haystack too — searching
                // for "ACME" must find the card that says ACME, and searching
                // for a street must find the card that prints it
                std::string tagbag = title_of(dn) + " " +
                                     field_value(dn, "venue") + " ";
                for (const auto& t : dn.tags) tagbag += t + " ";
                std::string bar = field_value(dn, "color");
                h << "<article class=\"card event\" data-tags=\""
                  << html_escape(tagbag) << "\"";
                if (bar.size() >= 4 && bar[0] == '#')
                    h << " style=\"--card-accent:" << html_escape(bar) << "\"";
                h << ">"
                  << "<h3>" << html_escape(title_of(dn)) << "</h3>";
                /* THREE FACTS, THREE ROWS (2026-08-20). This was one grey run —
                 * "2026-08-19 · 8:00 AM–10:00 AM · Community Education Building,
                 * 100 Main St Ste #170, Room 230T (second floor), Riverton, OR
                 * 97000" — which is a sentence a reader has to parse to find an
                 * address. A date, a time and a place are different kinds of
                 * fact; giving each its own line and its own mark is the whole
                 * of the fix, and it is also what makes the long address wrap
                 * inside its own row instead of shoving the card wide. */
                const std::string date_s = field_value(dn, "date").empty()
                                               ? field_value(dn, "days")
                                               : field_value(dn, "date");
                auto meta_row = [&](const char* icon, const std::string& text) {
                    if (text.empty()) return;
                    h << "<p class=\"meta-row\">";
                    if (site_th.icons) h << hormiga::icons::svg(icon);
                    h << "<span>" << html_escape(text) << "</span></p>";
                };
                meta_row("calendar", human_date(date_s, lang));
                if (!st.empty())
                    meta_row("clock",
                             st + (et.empty() ? std::string() : " \xe2\x80\x93 " + et));
                meta_row("map-pin", field_value(dn, "venue"));
                if (!field_value(dn, "virtual").empty())
                    meta_row("video", "Online option available");
                if (detail != "title") {
                    std::string sum = text_or(dn, "summary", "summary");
                    if (detail != "full") sum = clip(sum, 220);
                    if (!sum.empty()) h << "<p>" << html_escape(sum) << "</p>";
                }
                h << "</article>\n";
            }
            h << "</div>\n";
            if (evs.empty())
                h << "<p class=\"empty\">"
                  << html_escape(ui("Nothing scheduled right now.",
                                    "No hay nada programado por ahora."))
                  << "</p>\n";
        } else if (n->glyph == "image_grid") {
            std::string q = field_value(*n, "query");
            // display MODE (author 2026-07-23): grid | masonry | carousel.
            // carousel wraps in a swipe track with prev/next; masonry is a
            // CSS-columns wall; grid is the even-cell default.
            std::string mode = field_value(*n, "display");
            if (mode.empty()) mode = "grid";
            bool carousel = (mode == "carousel");
            std::string gcap = text(*n, "caption");
            const int glimit = hormiga::doc_field_int(*n, "limit", 0);
            /* ── `columns` IS NOW READ (2026-09-02) ──────────────────────────
             *
             * Field report A6. The field was declared on the glyph, labelled
             * `combo:2,3,4` and offered in the inspector, and this renderer
             * never looked at it once — layout came entirely from the
             * stylesheet's `auto-fill(minmax(190px,1fr))` and the block's
             * `span`, so an operator wanting one album cover to fill its column
             * had to discover empirically that `span 4` gives one tile and
             * `span 5` gives two with the cover floating left. Their sentence
             * for it is the right one: *a field that does nothing is worse than
             * no field.*
             *
             * Empty still means `auto-fill`, the correct default for a gallery
             * whose length is a query result. A count is clamped to 1-6 because
             * the value reaches a class name and there is no `.cols-97`; out of
             * range warns rather than silently doing nothing, since a typo here
             * looks exactly like the bug this replaces. Masonry and carousel lay
             * themselves out and say so instead of half-honouring it. */
            std::string gcols;
            {
                const std::string cs = field_value(*n, "columns");
                const int c = cs.empty() ? 0 : std::atoi(cs.c_str());
                if (!cs.empty() && (c < 1 || c > 6))
                    log.push_back({"warn", "render",
                                   n->name + ": image_grid columns '" + cs +
                                       "' is outside 1-6 and was ignored"});
                else if (!cs.empty() && mode != "grid")
                    log.push_back({"warn", "render",
                                   n->name + ": image_grid columns is a `grid` "
                                   "setting; this block is `" + mode +
                                   "`, which lays itself out"});
                else if (!cs.empty())
                    gcols = " cols-" + std::to_string(c);
            }
            // `fit` (2026-09-13): how each image fills its tile; blank = the crop
            const std::string gfitv = field_value(*n, "fit");
            const std::string gfit =
                (gfitv == "whole" || gfitv == "natural" || gfitv == "stretch")
                    ? " fit-" + gfitv : std::string();
            if (!gcap.empty())
                h << "<p class=\"meta caption\">" << html_escape(gcap) << "</p>\n";
            if (carousel)
                h << "<div class=\"carousel-wrap reveal\">"
                     "<button class=\"cbtn prev\" aria-label=\"previous\">&lsaquo;"
                     "</button>\n<div class=\"gallery carousel" << gfit << "\">\n";
            else
                h << "<div class=\"gallery " << mode << gcols << gfit << " reveal\">\n";
            /* ── WHY THIS IS TWO PASSES NOW (2026-08-28) ────────────────
             *
             * The field report: on the Archive page an English reader got five
             * tiles and a Spanish reader got one, "with no indication that
             * anything is missing". Filtering by language is right — most of
             * that organization's fliers really are English-only — but a
             * filter that removes four fifths of a section in silence is
             * indistinguishable, to the reader, from a section that is nearly
             * empty.
             *
             * Counting requires knowing the whole candidate set before the
             * first tile is written, which the single fused loop could not do:
             * it discovered "hidden because English-only" and "skipped because
             * the file is not on this machine" one at a time, mid-emission, and
             * reported neither. Those two are the same silence with completely
             * different fixes — translate the sheet, versus fetch the asset —
             * and a whole afternoon has gone into reading one as the other. */
            std::vector<const maiz::SceneNode*> cands;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "image" &&
                    hormiga::query_matches(q, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    cands.push_back(&dn);
            /* Hidden by LANGUAGE, and in which one — so the line can say
             * "only in English" rather than the useless "not available". */
            std::vector<std::string> otherlangs;
            int off_lang = 0, no_asset = 0;
            std::vector<std::string> missing;
            std::vector<const maiz::SceneNode*> tiles;
            for (const maiz::SceneNode* dn : cands) {
                /* THE LANGUAGE OF A PICTURE (2026-08-18 finding, fixed here).
                 *
                 * A flier exists in two languages as two `image` runes — the
                 * graph-native answer, and the rule the project settled on:
                 * per-language THINGS get sibling runes and a `lang:` tag;
                 * per-language STRINGS on one thing get the `_en`/`_es` suffix.
                 * What was missing was the consuming half: an `image_grid`
                 * could not pick the viewer's language, so a bilingual
                 * newsletter needed a hand-built second section for the Spanish
                 * sheets.
                 *
                 * An image with NO `lang:` tag is language-neutral (a photo of a

                 * dinner is not in a language) and shows on both. */
                if (!lang_matches(*dn, lang)) {
                    ++off_lang;
                    for (const auto& l : hormiga::tag_values(*dn, "lang"))
                        otherlangs.push_back(l);
                    continue;
                }
                /* A rune whose file is not on this machine. This was a bare
                 * `continue` with a one-line comment, and it is the OTHER way a
                 * tile vanishes without a word — the one the report's still-open
                 * `lang:es` case looks exactly like from the outside. */
                if (stage_site_asset(field_value(*dn, "path")).empty()) {
                    ++no_asset;
                    if (missing.size() < 6) missing.push_back(dn->name);
                    continue;
                }
                tiles.push_back(dn);
            }
            /* `limit` clips AFTER the counting, so an author who asked for six
             * tiles is not also told that the seventh is missing. What the
             * author chose to hold back is not a shortfall. */
            if (glimit > 0 && (int)tiles.size() > glimit)
                tiles.resize((size_t)glimit);
            int shown = 0;
            for (const maiz::SceneNode* dn : tiles) {
                const std::string href = stage_site_asset(field_value(*dn, "path"));
                ++shown;
                published_images.insert(dn->name);
                /* The TILE gets a downscaled derivative; the lightbox href
                 * keeps the original. A 4 MB flier scan drawn at 300px is 4 MB
                 * of somebody's data plan either way until this line. */
                const std::string thumb = site_thumb(href, 900);
                /* The lightbox caption was `humanize(rune_name)` — reading
                 * "Flier Escudo Back2school Eng" under a picture. `description`
                 * and `alt` are declared, already populated, and written for a
                 * reader. */
                /* Per-language, with the legacy field last: `description`
                 * and `alt` are prose an organization wrote, and a caption
                 * under a flier on the Spanish page is exactly the text a
                 * Spanish reader needs. */
                std::string icap = lang_text(*dn, "description", lang);
                if (icap.empty()) icap = lang_text(*dn, "alt", lang);
                if (icap.empty()) icap = humanize(dn->name);
                h << "<a class=\"tile\" href=\"" << html_escape(href)
                  << "\" data-caption=\"" << html_escape(icap)
                  << "\"><img loading=\"lazy\" src=\""
                  << html_escape(thumb.empty() ? href : thumb)
                  << "\" alt=\"" << html_escape(field_value(*dn, "alt")) << "\"></a>\n";
            }
            if (carousel)
                h << "</div>\n<button class=\"cbtn next\" aria-label=\"next\">"
                     "&rsaquo;</button></div>\n";
            else
                h << "</div>\n";
            /* AN EMPTY GALLERY MUST SAY SO. `event_grid` and `job_grid` both
             * had an empty state; this one did not, so a Spanish page rendered
             * a heading immediately followed by the footer and read as broken
             * rather than as empty. That became visible the moment the language
             * filter arrived — the block had simply never been empty before. */
            if (!shown)
                h << "<p class=\"empty\">"
                  << html_escape(ui("Nothing to show here yet.",
                                    "Nada que mostrar por ahora."))
                  << "</p>\n";
            /* ── SAY WHAT WAS HELD BACK, AND WHY (2026-08-28) ──────────────
             *
             * "A 'showing 1 of 5 — the rest are only in English' line would be
             * honest, and would be a better answer than either filtering
             * silently or not filtering." It is also the only one of the three
             * that respects the reader: it neither pretends the section is
             * complete nor hands a Spanish speaker four sheets they cannot
             * read. Naming the language is the part that carries the value —
             * "not available" tells nobody anything, and "only in English"
             * tells a bilingual reader exactly where to look. */
            if (off_lang > 0)
                h << "<p class=\"meta partial\">"
                  << html_escape(lang_shortfall(shown, shown + off_lang,
                                                otherlangs, lang))
                  << "</p>\n";
            /* The MISSING-FILE case is an operator problem, not a reader one:
             * nothing on the page can fix it, so it goes to the render log
             * where the person who can is already looking. Named runes, because
             * "3 images were skipped" is the report that starts an afternoon of
             * grepping. English pass only — the fact is about the database, not
             * about the page, and saying it twice per render would train a
             * reader to skim the log. */
            if (no_asset > 0 && lang == "en") {
                std::string names;
                for (const auto& m : missing) names += (names.empty() ? "" : ", ") + m;
                if (no_asset > (int)missing.size()) names += ", ...";
                log.push_back(
                    {"warn", "render",
                     std::to_string(no_asset) +
                         " image rune(s) matched an image_grid query but have no "
                         "file on this machine, so they reach no page in any "
                         "language: " +
                         names +
                         ". The website is self-hosted - a rune whose `path` is "
                         "empty, or points at a file that is not here, cannot be "
                         "published. From the outside this is indistinguishable "
                         "from the language filter; it is not the same thing."});
            }
        } else if (n->glyph == "download") {
            /* A file a visitor can keep. Markup in `render/download.hpp`; what
             * happens here is resolving `file` — which takes EITHER a path or
             * the name of a `resource` rune — staging it, and refusing the
             * extensions that would be executable on this site's own origin. */
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
            if (dc.label.empty())
                dc.label = ui("Download", "Descargar");
            dc.caption = text(*n, "caption");
            dc.card = field_value(*n, "download_style") == "card";
            dc.platform = platform_of(*n);

            const std::string refuse =
                want.empty() ? std::string() : hormiga::download_refusal(want);
            if (!want.empty() && !refuse.empty()) {
                log.push_back({"error", "render",
                               n->name + ": refusing to publish '" + want +
                                   "' - " + refuse});
                h << "<p class=\"empty\">"
                  << html_escape(ui("This file cannot be published.",
                                    "Este archivo no se puede publicar."))
                  << "</p>\n";
            } else {
                dc.href = want.empty() ? std::string() : stage_site_asset(want);
                if (dc.href.empty()) {
                    if (want.empty())
                        log.push_back({"warn", "render",
                                       n->name + ": no file set, so this "
                                       "download button was left off the page"});
                    else
                        log.push_back({"warn", "render",
                                       n->name + ": '" + want + "'" +
                                           (via_rune.empty()
                                                ? std::string()
                                                : " (resource " + via_rune + ")") +
                                           " is not on this machine, so the "
                                           "download button was left off the "
                                           "page rather than published broken"});
                    h << "<p class=\"empty\">"
                      << html_escape(ui("This file is not available.",
                                        "Este archivo no esta disponible."))
                      << "</p>\n";
                } else {
                    /* Size and kind from the STAGED file — the same `stat` the
                     * staleness check already performs, and the one piece of
                     * metadata a visitor wants before they tap it on a phone. */
                    std::error_code se;
                    const auto sz = fs::file_size(data_dir("site") / dc.href, se);
                    dc.meta = hormiga::file_kind(dc.href);
                    if (!se) dc.meta += " \xC2\xB7 " + hormiga::human_size(sz);
                    h << hormiga::download_web(dc);
                }
            }
        } else if (n->glyph == "audio") {
            /* The markup is `render/audio.hpp`'s, beside its own concern and
             * pure — the precedent `video.hpp` set. What only this function can
             * do is resolve the cover RUNE, stage the file, and pick the
             * language; see there for why there is no click-to-load facade. */
            hormiga::AudioCard ac;
            const std::string asrc = field_value(*n, "src");
            ac.src = stage_site_asset(asrc);
            ac.title = text(*n, "title");
            ac.artist = field_value(*n, "artist");
            ac.duration = field_value(*n, "duration");
            ac.caption = text(*n, "caption");
            const std::string cover_rune = field_value(*n, "cover");
            if (!cover_rune.empty())
                for (const auto& dn : data.nodes)
                    if (dn.glyph == "image" && dn.name == cover_rune)
                        ac.cover = stage_site_asset(field_value(dn, "path"));
            /* Two different silences, two different sentences. "No file yet" is
             * an unfinished block; "not available" is a file this machine does
             * not have, and only the second is worth a warning. */
            const std::string miss =
                asrc.empty()
                    ? ui("No audio file yet. Set this block's file and it will "
                         "play here.",
                         "Aun no hay archivo de audio. Elige el archivo de este "
                         "bloque y sonara aqui.")
                    : ui("This recording is not available.",
                         "Esta grabacion no esta disponible.");
            if (ac.src.empty() && !asrc.empty())
                log.push_back({"warn", "render",
                               n->name + ": audio file '" + asrc +
                                   "' is not on this machine, so the page has a "
                                   "message where the player should be. Bring "
                                   "the file into assets/ (the block's Browse "
                                   "does it) and render again."});
            h << hormiga::audio_web(ac, miss,
                                    ui("Download the audio",
                                       "Descargar el audio"));
        } else if (n->glyph == "video") {
            /* ── A VIDEO, WITHOUT REPORTING THE READER (2026-08-28) ─────────
             *
             * A plain YouTube `<iframe>` loads the moment the page does. Every
             * visitor to the home page — every visitor, not every viewer — is
             * then known to Google to have been on it, whether or not they ever
             * pressed play, and there is no setting on the embed that changes
             * that. `youtube-nocookie.com` narrows what is stored; it does not
             * stop the request.
             *
             * So the page ships a FACADE: the organization's own still (or a
             * plain themed card), a play button, and a `data-embed` attribute.
             * `app.js` swaps in the iframe on click and not before. Nothing
             * reaches the video host until a person has asked to watch a video,
             * which is the only moment at which they have chosen to.
             *
             * This is CLAUDE.md rule 6 in its outward-facing form: the seam
             * where data leaves is the place to decide what leaves, and here
             * what leaves is the reader's address. The report names why it
             * matters for this organization in particular — members include
             * immigration and survivor-services groups — and that is a reason
             * to build the careful version once rather than to leave it to
             * whoever places the block.
             *
             * NO-JAVASCRIPT READERS get the poster wrapped in a link to the
             * video, which is a working page rather than a dead rectangle.
             * That is also what a `<noscript>` costs: three lines. */
            const std::string vurl = field_value(*n, "url");
            const hormiga::VideoRef vid = hormiga::parse_video_url(vurl);
            const std::string vcap = text(*n, "caption");
            if (!vcap.empty())
                h << "<p class=\"meta caption\">" << html_escape(vcap) << "</p>\n";
            if (!vid.ok()) {
                /* SAY WHICH FIELD AND WHAT IS ACCEPTED. "Video unavailable" is
                 * the message that makes a person re-paste the same string. */
                h << "<p class=\"empty\">"
                  << html_escape(ui("No video link yet. Paste a YouTube or Vimeo "
                                    "address into this block.",
                                    "Aun no hay enlace de video. Pega una "
                                    "direccion de YouTube o Vimeo en este "
                                    "bloque."))
                  << "</p>\n";
                if (!vurl.empty() && lang == "en")
                    log.push_back({"warn", "render",
                                   "a `video` block holds " + vurl +
                                       ", which is not a YouTube or Vimeo "
                                       "address this build recognises, so the "
                                       "page shows an empty-state instead of a "
                                       "player. Paste the address from the "
                                       "browser bar or the Share button - not an "
                                       "embed code."});
            } else {
                std::string ratio = field_value(*n, "ratio");
                if (ratio.empty()) ratio = "16:9";
                std::string poster = stage_site_asset(field_value(*n, "poster"));
                if (!poster.empty()) {
                    const std::string th = site_thumb(poster, 1200);
                    if (!th.empty()) poster = th;
                }
                h << hormiga::video_block_html(
                    vid, poster, ratio,
                    ui("Play the video", "Reproducir el video"),
                    ui("Watch on ", "Ver en ") + vid.provider_label(),
                    [](const std::string& t) { return html_escape(t); });
            }
        } else if (n->glyph == "job_grid") {
            std::string q = field_value(*n, "query");
            // `caption_en`/`caption_es` are DECLARED on this glyph, listed by
            // `--describe`, and shown by the Builder's inspector — and were
            // rendered by nothing. That is the "declared but invisible" trap
            // arriving from the far side, which is worse than the undeclared
            // one because `--describe` is what an agent is told to trust.
            std::string jcap = text(*n, "caption");
            if (!jcap.empty())
                h << "<p class=\"meta caption\">" << html_escape(jcap) << "</p>\n";
            const std::string jdetail = field_value(*n, "detail");
            const int jlimit = hormiga::doc_field_int(*n, "limit", 0);
            const std::string jsort = field_value(*n, "sort");
            std::vector<const maiz::SceneNode*> jobs;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "job" && hormiga::query_matches(q, data, dn, today) &&
                    !allo_web_hidden(dn.name))
                    jobs.push_back(&dn);
            if (jsort == "name")
                std::stable_sort(jobs.begin(), jobs.end(),
                                 [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                                     return heading_of(*a) < heading_of(*b);
                                 });
            else if (jsort == "deadline")
                std::stable_sort(jobs.begin(), jobs.end(),
                                 [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                                     const std::string da = field_value(*a, "deadline");
                                     const std::string db = field_value(*b, "deadline");
                                     // an undated posting keeps its place at the
                                     // end rather than sorting to the front as ""
                                     if (da.empty() != db.empty()) return db.empty();
                                     return da < db;
                                 });
            if (jlimit > 0 && (int)jobs.size() > jlimit) jobs.resize((size_t)jlimit);
            h << "<div class=\"cards filterable reveal\">\n";
            const int hits = (int)jobs.size();
            for (const maiz::SceneNode* jp : jobs) {
                const maiz::SceneNode& dn = *jp;
                /* `display_name(dn)` printed "Sass Survivor Access
                 * Coordinator" for `sass-survivor-access-coordinator`, because
                 * `job` declared no title field and this fell through to the
                 * humanized slug. SASS is Sexual Assault Support Services. */
                const std::string jtitle = heading_of(dn);
                std::string tagbag = jtitle + " " + field_value(dn, "org");
                h << "<article class=\"card job\" data-tags=\""
                  << html_escape(tagbag) << "\">"
                  << "<h3>" << html_escape(jtitle) << "</h3>";
                auto job_row = [&](const char* icon, const std::string& text) {
                    if (text.empty()) return;
                    h << "<p class=\"meta-row\">";
                    if (site_th.icons) h << hormiga::icons::svg(icon);
                    h << "<span>" << html_escape(text) << "</span></p>";
                };
                job_row("building-2", field_value(dn, "org"));
                job_row("circle-dollar-sign", field_value(dn, "pay"));
                job_row("map-pin", field_value(dn, "location"));
                job_row("briefcase", field_value(dn, "job_type"));
                job_row("calendar",
                        field_value(dn, "deadline").empty()
                            ? std::string()
                            : ui("Closes ", "Cierra ") +
                                  human_date(field_value(dn, "deadline"), lang));
                job_row("check", field_value(dn, "availability"));
                /* DECLARED AND RENDERED BY NOTHING until now — the posting that
                 * says "email your resume to …" buried the address in the wall
                 * of description instead of showing it as the field it is. */
                job_row("user", field_value(dn, "contact_name"));
                if (!field_value(dn, "contact_email").empty()) {
                    h << "<p class=\"meta-row\">";
                    if (site_th.icons) h << hormiga::icons::svg("mail");
                    h << "<span><a href=\"mailto:"
                      << html_escape(field_value(dn, "contact_email")) << "\">"
                      << html_escape(field_value(dn, "contact_email"))
                      << "</a></span></p>";
                }
                job_row("phone", field_value(dn, "contact_phone"));
                if (jdetail != "title" && jdetail != "line") {
                    std::string desc = field_value(dn, "description");
                    // the honest version of a clip is a clip plus somewhere to
                    // read the rest — Phase 3's detail view is that somewhere
                    if (jdetail != "full") desc = clip(desc, 260);
                    if (!desc.empty()) h << "<p>" << html_escape(desc) << "</p>";
                }
                h << "</article>\n";
            }
            h << "</div>\n";
            if (!hits)
                h << "<p class=\"empty\">"
                  << html_escape(ui("No open positions right now.",
                                    "No hay vacantes por ahora."))
                  << "</p>\n";
        } else if (n->glyph == "event_feature") {
            /* ── ONE EVENT, SHOWCASED (2026-08-20) ───────────────────────────
             *
             * A grid answers "what is coming up". This answers "come to THIS",
             * at a size a grid cell cannot give it: the image behind, the
             * details over it, one button.
             *
             * The background falls back to the event's OWN FLIER, found through
             * its `flyer-of` edge — so the common case needs no authoring at
             * all. That fallback is the first thing in this renderer to read
             * `Scene::wires`, which had 22 correct edges in a real database and
             * no reader. */
            const std::string evname = field_value(*n, "event");
            const maiz::SceneNode* ev = nullptr;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "event" && dn.name == evname) ev = &dn;
            if (!ev) {
                h << "<p class=\"empty\">"
                  << html_escape(ui("No event chosen for this feature.",
                                    "No se ha elegido un evento."))
                  << "</p>\n";
            } else if (allo_web_hidden(ev->name)) {
                // a rule says withhold it: say nothing at all, not "hidden"
            } else {
                std::string bg = stage_site_asset(field_value(*n, "image"));
                if (bg.empty()) {
                    const auto fl = related_runes(data, ev->name, kFlierRelations());
                    if (const maiz::SceneNode* f = pick_for_lang(fl, lang)) {
                        bg = stage_site_asset(field_value(*f, "path"));
                        /* A background found through the edge is still a flier
                         * ON A PAGE, so it is in scope for the staleness check
                         * below. §6 of the report is the reason this is worth
                         * saying out loud: this one is a CSS `background-image`
                         * rather than an `<img>`, and an auditor grepping the
                         * rendered HTML will not find it. */
                        if (!bg.empty()) published_images.insert(f->name);
                    }
                }
                std::string ffil = field_value(*n, "image_filter");
                if (ffil.empty() || ffil == "theme") ffil = site_th.filter_class();
                else if (ffil == "none") ffil.clear();
                else ffil = "f-" + ffil;
                const std::string fdim = field_value(*n, "image_dim");
                std::string hcls = field_value(*n, "height");
                if (hcls.empty()) hcls = "tall";

                h << "<section class=\"feature " << hcls
                  << (bg.empty() ? " plain" : " imaged") << "\"";
                if (!fdim.empty())
                    h << " style=\"--bdim:" << (std::atoi(fdim.c_str()) / 100.0)
                      << "\"";
                h << ">";
                if (!bg.empty())
                    h << "<div class=\"feature-bg " << ffil
                      << "\" style=\"background-image:url('" << html_escape(bg)
                      << "')\"></div><div class=\"feature-shade\"></div>";
                h << "<div class=\"feature-in\">";
                h << "<h2>" << html_escape(title_of(*ev)) << "</h2>";
                auto frow = [&](const char* icon, const std::string& t) {
                    if (t.empty()) return;
                    h << "<p class=\"meta-row\">";
                    if (site_th.icons) h << hormiga::icons::svg(icon);
                    h << "<span>" << html_escape(t) << "</span></p>";
                };
                const std::string fd = field_value(*ev, "date").empty()
                                           ? field_value(*ev, "days")
                                           : human_date(field_value(*ev, "date"), lang);
                frow("calendar", fd);
                const std::string fst = field_value(*ev, "start_time");
                if (!fst.empty()) {
                    const std::string fet = field_value(*ev, "end_time");
                    frow("clock", fst + (fet.empty() ? std::string()
                                                     : " \xe2\x80\x93 " + fet));
                }
                frow("map-pin", field_value(*ev, "venue"));
                const std::string fsum = text_or(*ev, "summary", "summary");
                if (!fsum.empty())
                    h << "<p class=\"feature-sum\">" << html_escape(fsum) << "</p>";
                std::string cta = text(*n, "cta");
                std::string ctal = field_value(*n, "cta_link");
                if (ctal.empty()) ctal = field_value(*ev, "virtual");
                if (!ctal.empty()) {
                    if (cta.empty()) cta = ui("More about this", "Mas informacion");
                    h << "<a class=\"btn\" href=\"" << html_escape(href_of(ctal))
                      << "\">" << html_escape(cta) << "</a>";
                }
                h << "</div></section>\n";
            }
        } else if (n->glyph == "event_flier") {
            /* ── THE EVENT AND ITS FLIER, TOGETHER ───────────────────────────
             *
             * The author's distinction from `event_feature`, and it is the
             * right one: *"this will be different because it's a flier we
             * should still be able to click."* A flier is a document a person
             * opens full size, saves and forwards — not wallpaper behind a
             * headline. So the image is a real tile that opens the lightbox,
             * and the details sit beside it.
             *
             * The flier is chosen FOR THE PAGE'S LANGUAGE: a flier exists in
             * two languages as two `image` runes carrying `lang:` tags, and a
             * Spanish page showing the English scan is the failure this
             * prevents. Blank `flier` means "whichever one belongs to this
             * event, in this language" — which is what an author means. */
            const std::string evname = field_value(*n, "event");
            const maiz::SceneNode* ev = nullptr;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "event" && dn.name == evname) ev = &dn;
            const maiz::SceneNode* fl = nullptr;
            const std::string fname = field_value(*n, "flier");
            if (!fname.empty()) {
                for (const auto& dn : data.nodes)
                    if (dn.glyph == "image" && dn.name == fname) fl = &dn;
            } else if (ev) {
                auto cands = related_runes(data, ev->name, kFlierRelations());
                // keep only images, then pick this page's language
                std::vector<const maiz::SceneNode*> imgs;
                for (const auto* c : cands)
                    if (c->glyph == "image") imgs.push_back(c);
                fl = pick_for_lang(imgs, lang);
            }
            std::string fcap = text(*n, "caption");
            if (!fcap.empty())
                h << "<p class=\"meta caption\">" << html_escape(fcap) << "</p>\n";
            if (!ev && !fl) {
                h << "<p class=\"empty\">"
                  << html_escape(ui("No event or flier chosen.",
                                    "No se ha elegido evento ni volante."))
                  << "</p>\n";
            } else {
                const std::string mode = field_value(*n, "display") == "stacked"
                                             ? "stacked" : "side";
                h << "<div class=\"evflier " << mode << " reveal\">";
                if (fl && !allo_web_hidden(fl->name)) {
                    const std::string href = stage_site_asset(field_value(*fl, "path"));
                    if (!href.empty()) {
                        published_images.insert(fl->name);
                        const std::string th = site_thumb(href, 900);
                        std::string icap =
                            lang_text(*fl, "description", lang);
                        if (icap.empty())
                            icap = lang_text(*fl, "alt", lang);
                        if (icap.empty() && ev) icap = title_of(*ev);
                        h << "<a class=\"tile flier\" href=\"" << html_escape(href)
                          << "\" data-caption=\"" << html_escape(icap)
                          << "\"><img loading=\"lazy\" src=\""
                          << html_escape(th.empty() ? href : th) << "\" alt=\""
                          << html_escape(lang_text(*fl, "alt", lang)) << "\"></a>";
                    }
                }
                if (ev && !allo_web_hidden(ev->name)) {
                    h << "<div class=\"evflier-body\">";
                    h << "<h3>" << html_escape(title_of(*ev)) << "</h3>";
                    auto xrow = [&](const char* icon, const std::string& t) {
                        if (t.empty()) return;
                        h << "<p class=\"meta-row\">";
                        if (site_th.icons) h << hormiga::icons::svg(icon);
                        h << "<span>" << html_escape(t) << "</span></p>";
                    };
                    const std::string xd = field_value(*ev, "date").empty()
                                               ? field_value(*ev, "days")
                                               : human_date(field_value(*ev, "date"), lang);
                    xrow("calendar", xd);
                    const std::string xst = field_value(*ev, "start_time");
                    if (!xst.empty()) {
                        const std::string xet = field_value(*ev, "end_time");
                        xrow("clock", xst + (xet.empty() ? std::string()
                                                         : " \xe2\x80\x93 " + xet));
                    }
                    xrow("map-pin", field_value(*ev, "venue"));
                    const std::string xs = text_or(*ev, "summary", "summary");
                    if (!xs.empty()) h << "<p>" << html_escape(xs) << "</p>";
                    h << "</div>";
                }
                h << "</div>\n";
            }
        } else if (n->glyph == "directory") {
            /* ── THE MEMBER DIRECTORY (2026-08-19) ───────────────────────────
             *
             * Asked for from a real build, and the reason a real network's
             * website shipped with a placeholder where its centrepiece goes:
             * `event_grid`, `image_grid` and `job_grid` were the only
             * query-backed blocks, `map_embed` deliberately excludes contacts,
             * and so there was NO way to put a person or an organization on a
             * Hormiga website. The organization is called a Network.
             *
             * ── WHY THE CONSENT TAG IS NOT OPTIONAL ─────────────────────────
             *
             * The same database holds 84 contacts, 70 of them carrying a
             * personal email and 66 a phone. A directory block that published
             * everything its query matched would publish a phone book, and it
             * would do it the first time somebody wrote `type:contact` — which
             * is exactly the query a person writes when they want a directory.
             *
             * So the block does NOT trust the query. A rune reaches a page only
             * if it carries `clearance:public`, and no field, flag or query
             * turns that off. This is `allo_web_hidden` inverted, and inverted
             * on purpose: web-hide is a subtraction from a default of
             * publishing, which is the right default for an event and the wrong
             * one for a person.
             *
             * ── AND CLEARANCE IS AN ANNOTATION, NOT A RANK ──────────────────
             *
             * web-platform.md §4: tags carry clearance, Allomone derives from
             * it, and nothing is a level. So `clearance:contact` is a SECOND,
             * INDEPENDENT annotation rather than a higher rung — it releases
             * the personal email and phone, and a rune can carry either tag
             * without the other. "List me" and "print my phone number" are two
             * different consents, and a rank would have collapsed them into
             * one.
             *
             * `notes` is not reachable from here at any clearance — CLAUDE.md
             * rule 6, enforced by that field never being read. */
            const std::string q = field_value(*n, "query");
            std::string kind = field_value(*n, "kind");
            if (kind.empty()) kind = "contact";
            std::string dmode = field_value(*n, "display");
            if (dmode.empty()) dmode = "card";
            const int dlimit = hormiga::doc_field_int(*n, "limit", 0);
            const std::string dcap = text(*n, "caption");

            /* THE GATE IS `render/published.hpp`, AND IT IS SHARED (2026-08-21).
             *
             * This loop used to live here, inline, mixed into HTML emission.
             * It moved out unchanged because the published subset is about to
             * have a SECOND format — index rows for a live site — and a second
             * exporter with its own copy of the clearance check would be a
             * second privacy surface that drifts silently. One function, two
             * callers; see that header. */
            auto pub = hormiga::published::directory(
                data, q, kind,
                [this](const std::string& rune) { return allo_web_hidden(rune); },
                lang);
            std::vector<hormiga::published::Person>& people = pub.people;
            const int withheld = pub.withheld;
            if (dlimit > 0 && (int)people.size() > dlimit)
                people.resize((size_t)dlimit);

            /* SAY WHAT WAS WITHHELD, in the render log. A directory that comes
             * out empty because nobody has been tagged yet is indistinguishable
             * from a broken block, and the person who has to fix it is the one
             * reading this line. */
            if (withheld)
                log.push_back(
                    {"info", "render",
                     "directory " + n->name + ": " + std::to_string(withheld) +
                         " matching rune(s) withheld - only runes tagged "
                         "`clearance:public` are published. Add `clearance:contact` "
                         "too to publish an email or phone."});

            if (!dcap.empty())
                h << "<p class=\"meta caption\">" << html_escape(dcap) << "</p>\n";
            const bool dcaro = dmode == "carousel";
            /* ── LIVE: the cards are also written as a standalone fragment ────
             *
             * The operator's complaint was redeploying the WHOLE site whenever
             * one contact changed. So a `live` directory's cards are buffered,
             * emitted into the page as normal, AND written to
             * `site/index/dir-<block>.html`. The page carries a `data-live`
             * pointer and `app.js` re-fetches it on load — so refreshing the
             * directory costs one small file republished, not a site deploy.
             *
             * A FRAGMENT RATHER THAN JSON, deliberately. Shipping rows would
             * mean re-implementing this card markup in JavaScript: a second
             * renderer, drifting from this one, for a feature whose entire
             * point is that the two agree. The fragment is produced by exactly
             * this code, so it cannot disagree with itself.
             *
             * Progressive enhancement, so there is no way for this to regress:
             * the build-time cards are still in the page. A visitor with no
             * JavaScript, or whose fetch fails, sees exactly what they see
             * today. */
            const bool dlive = field_value(*n, "live") == "on";
            const std::string dlive_src = "index/dir-" + n->name + ".html";
            std::ostringstream dcards;
            if (dcaro)
                h << "<div class=\"carousel-wrap reveal\">"
                     "<button class=\"cbtn prev\" aria-label=\"previous\">&lsaquo;"
                     "</button>\n<div class=\"cards directory carousel filterable\""
                  << (dlive ? " data-live=\"" + dlive_src + "\"" : "") << ">\n";
            else
                h << "<div class=\"cards directory " << dmode
                  << " filterable reveal\""
                  << (dlive ? " data-live=\"" + dlive_src + "\"" : "") << ">\n";
            /* The card loop below writes to `out`: the page directly, or the
             * buffer when this directory is live. One loop, two destinations —
             * rather than a second loop that would be free to differ. */
            std::ostream& out = dlive ? static_cast<std::ostream&>(dcards)
                                      : static_cast<std::ostream&>(h);
            for (const hormiga::published::Person& pp : people) {
                const std::string& nm = pp.display;
                const std::string& role = pp.role;
                const std::string& bio = pp.bio;
                const std::string& site = pp.website;
                const std::string& place = pp.place;
                std::string photo = stage_site_asset(pp.avatar);
                // the search box app.js already generates filters on data-tags,
                // so the visible prose belongs in the haystack too
                std::string bag = nm + " " + role + " " + place + " ";
                for (const auto& t : pp.tags) bag += t + " ";
                out << "<article class=\"card person\" data-tags=\""
                  << html_escape(bag) << "\">";
                if (!photo.empty()) {
                    const std::string th = site_thumb(photo, 480);
                    out << "<img class=\"avatar\" loading=\"lazy\" src=\""
                      << html_escape(th.empty() ? photo : th) << "\" alt=\""
                      << html_escape(nm) << "\">";
                }
                out << "<h3>" << html_escape(nm) << "</h3>";
                auto dir_row = [&](const char* icon, const std::string& text) {
                    if (text.empty()) return;
                    out << "<p class=\"meta-row\">";
                    if (site_th.icons) out << hormiga::icons::svg(icon);
                    out << "<span>" << html_escape(text) << "</span></p>";
                };
                dir_row(pp.glyph == "contact" ? "user" : "building-2", role);
                dir_row("map-pin", place);
                if (!bio.empty()) out << "<p>" << html_escape(clip(bio, 260)) << "</p>";
                /* THE SECOND CONSENT. An email or a phone reaches a public page
                 * only for a rune that separately says so. An organization's
                 * `email` is nominally a front desk — and it still waits for the
                 * tag, because a small mutual-aid group's "contact email" is
                 * very often one volunteer's personal inbox. */
                {
                    // the gate already applied `clearance:contact`; empty here
                    // means "not consented" and is the truth, not a mask
                    const std::string& em = pp.email;
                    const std::string& ph = pp.phone;
                    if (!em.empty()) {
                        out << "<p class=\"meta-row\">";
                        if (site_th.icons) out << hormiga::icons::svg("mail");
                        out << "<span><a href=\"mailto:" << html_escape(em) << "\">"
                          << html_escape(em) << "</a></span></p>";
                    }
                    if (!ph.empty()) {
                        out << "<p class=\"meta-row\">";
                        if (site_th.icons) out << hormiga::icons::svg("phone");
                        out << "<span>" << html_escape(ph) << "</span></p>";
                    }
                }
                if (!site.empty()) {
                    out << "<p class=\"meta-row\">";
                    if (site_th.icons) out << hormiga::icons::svg("globe");
                    out << "<span><a href=\"" << html_escape(href_of(site))
                      << "\" rel=\"noopener\">" << html_escape(site)
                      << "</a></span></p>";
                }
                out << "</article>\n";
            }
            if (dlive) {
                // the same bytes twice: into the page (so JavaScript-off is
                // unaffected) and into the fragment the page refreshes from
                h << dcards.str();
                write_live_fragment(n->name, dcards.str());
            }
            if (dcaro)
                h << "</div>\n<button class=\"cbtn next\" aria-label=\"next\">"
                     "&rsaquo;</button></div>\n";
            else
                h << "</div>\n";
            if (people.empty())
                h << "<p class=\"empty\">"
                  << html_escape(ui("This directory is empty. (Entries appear "
                                    "once they are marked as public.)",
                                    "Este directorio esta vacio. (Las entradas "
                                    "aparecen cuando se marcan como publicas.)"))
                  << "</p>\n";
        } else if (n->glyph == "map_embed") {
            // the INTERACTIVE map (author: "a whole javascript element where
            // you can explore a map") — read-only by construction: the page
            // gets positions+styles as data; there is no write path at all.
            // PRIVACY SEAM: contacts never render here (personal coordinates
            // stay on the device; territory.md boundaries).
            std::string vname = field_value(*n, "view");
            const maiz::SceneNode* view = nullptr;
            for (const auto& dn : data.nodes)
                if (dn.glyph == "map" && (vname.empty() || dn.name == vname)) {
                    view = &dn;
                    break;
                }
            std::vector<MapRule> vrules =
                view ? parse_view_rules_of(*view) : std::vector<MapRule>{};
            std::string ch = view ? field_value(*view, "channel") : "";
            if (ch.empty()) ch = "main";
            std::string center = view ? field_value(*view, "center") : "";
            std::string zoom = view ? field_value(*view, "zoom") : "13";
            auto shape_of = [&](const maiz::SceneNode& dn) -> std::string {
                std::string s;
                for (const auto& r : vrules) {
                    if (r.tags.empty() || !maiz::node_matches(rule_expr_of(r), dn))
                        continue;
                    if (!r.shape.empty()) s = r.shape;
                    break;
                }
                std::string st = tag_value(dn, "shape"); // explicit tag wins
                return st.empty() ? s : st;
            };
            auto web_fans = ref_fans(data, ch); // #4: fan ref-children
            nlohmann::json mj = nlohmann::json::array();
            for (const auto& dn : data.nodes) {
                if (dn.glyph == "contact" || dn.glyph == "map" ||
                    dn.glyph == "image" || dn.glyph == "note" ||
                    dn.glyph == "refpoint")
                    continue; // contacts: the seam; refpoints: editor-only gizmos
                double la, lo;
                float dx = 0, dy = 0;
                auto ff = web_fans.find(dn.name);
                if (ff != web_fans.end()) {
                    la = ff->second.lat; lo = ff->second.lon;
                    dx = ff->second.dx; dy = ff->second.dy;
                } else {
                    std::string g = view_geo(dn, ch);
                    if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
                }
                /* The display name, never the slug (see title_of). Extended
                 * 2026-08-20 to contacts and organizations: `display_name`
                 * landed for the directory and not for the map, so a map with
                 * 24 pins showed "Ccog" for the County Council of Governments and
                 * "Acme Valley Chapter" for ACME. Same rule
                 * everywhere — an output never renders a rune's name. */
                mj.push_back({{"n", (dn.glyph == "organization" ||
                                     dn.glyph == "contact")
                                        ? display_name(dn)
                                        : title_of(dn)},
                              {"la", la},
                              {"lo", lo},
                              {"dx", dx},
                              {"dy", dy},
                              {"c", style_hex(dn, vrules)},
                              {"s", shape_of(dn)},
                              {"k", dn.glyph}});
            }
            // #3: shapes (rect/ellipse annotations) into the widget too
            nlohmann::json sj = nlohmann::json::array();
            for (const auto& dn : data.nodes) {
                if (dn.glyph != "mapshape") continue;
                double a1, o1, a2, o2;
                if (!hormiga::parse_geo(field_value(dn, "geo1"), a1, o1) ||
                    !hormiga::parse_geo(field_value(dn, "geo2"), a2, o2))
                    continue;
                sj.push_back({{"a1", a1}, {"o1", o1}, {"a2", a2}, {"o2", o2},
                              {"e", field_value(dn, "kind") == "ellipse"},
                              {"c", style_hex(dn, vrules)}});
            }
            // tiles for the BROWSER: OSM's servers 403 referer-less requests
            // (file:// previews, some deploys), so the widget rides CARTO —
            // voyager stands in for the labeled OSM style; the no-labels
            // choices carry through. Attribution covers both projects.
            const char* tile_tmpl =
                basemap_src == 1
                    ? "https://basemaps.cartocdn.com/light_nolabels/{z}/{x}/{y}.png"
                : basemap_src == 2
                    ? "https://basemaps.cartocdn.com/dark_nolabels/{z}/{x}/{y}.png"
                    : "https://basemaps.cartocdn.com/rastertiles/voyager/{z}/{x}/{y}.png";
            h << "<div class=\"mapwidget\" data-center=\""
              << html_escape(center) << "\" data-zoom=\"" << html_escape(zoom)
              << "\" data-tiles=\"" << tile_tmpl
              << "\" data-attrib=\"(c) OpenStreetMap contributors (c) CARTO\""
              << " data-shapes=\"" << html_escape(sj.dump()) << "\""
              << " data-markers=\"" << html_escape(mj.dump()) << "\"></div>\n";
            std::string cap = text(*n, "caption");
            if (!cap.empty())
                h << "<p class=\"meta\">" << html_escape(cap) << "</p>\n";
        } else if (n->glyph == "calendar_embed") {
            // the INTERACTIVE calendar: month/week/3-day in the page, entries
            // link "add to Google Calendar", the bar offers the .ics download.
            // DEFAULT = EVENTS ONLY — incidents are published only when the
            // query names them (publishing sensitive data is a choice).
            std::string q = field_value(*n, "query");
            nlohmann::json ej = nlohmann::json::array();
            std::vector<hormiga::ical::Event> ics_entries;
            for (const auto& dn : data.nodes) {
                int yy, mm, dd;
                std::string ds = field_value(dn, "date");
                if (std::sscanf(ds.c_str(), "%d-%d-%d", &yy, &mm, &dd) != 3)
                    continue;
                if ((q.empty() ? dn.glyph != "event"
                               : !hormiga::query_matches(q, data, dn, today)) ||
                    allo_web_hidden(dn.name))
                    continue;
                std::string st = field_value(dn, "start_time");
                if (st.empty() && dn.glyph == "incident")
                    st = field_value(dn, "time");
                std::string en = field_value(dn, "end_time");
                std::string venue = field_value(dn, "venue");
                ej.push_back({{"n", title_of(dn)},
                              {"d", ds},
                              {"s", st},
                              {"e", en},
                              {"v", venue},
                              // built where the time parser is; see gcal_url
                              {"g", gcal_url(title_of(dn), ds, st, en, venue)},
                              {"c", style_hex(dn, {})},
                              {"i", dn.glyph == "incident"}});
                /* The RFC 5545 twin, through the ONE lens
                 * (`domain/ical.hpp`). This used to be forty lines of string
                 * concatenation right here, which is exactly why it could only
                 * ever serve this loop — and why it had no line folding, built
                 * its UID from the editable rune NAME, and emitted a
                 * two-property header. Filling an `ical::Event` field by field
                 * IS the privacy seam: what is not assigned here cannot leave.
                 */
                hormiga::ical::Event ie;
                /* THE FROZEN ID, NOT THE NAME. Void Core's rune spec: `id` is
                 * "minted once, never reused", `name` is "editable". A UID
                 * built from the name tells every subscriber that renaming an
                 * event DELETED it and created an unrelated new one. */
                ie.uid = (dn.id.empty() ? dn.name : dn.id) + "@voidhormiga";
                ie.summary = title_of(dn);
                ie.description = text_or(dn, "summary", "summary");
                ie.location = venue;
                ie.geo = field_value(dn, "geo");
                ie.categories = hormiga::ical::public_categories(dn.tags);
                ie.date = ds;
                ie.start_time = st;
                ie.end_time = en;
                ie.cancelled = std::find(dn.tags.begin(), dn.tags.end(),
                                         "status:cancelled") != dn.tags.end();
                if (!base_url.empty())
                    ie.url = base_url + "/" + page_file(slug, lang);
                ics_entries.push_back(ie);
            }
            if (!ics_entries.empty()) {
                hormiga::ical::Options iopt;
                /* X-WR-CALNAME: what a subscriber's client will CALL this
                 * calendar. Not cosmetic for a hub — without it the feed shows
                 * up named after its URL, which is the difference between four
                 * legible calendars in somebody's sidebar and four rows of
                 * nonsense. */
                iopt.name = site_title;
                want_ics = true;
                ics = hormiga::ical::to_vcalendar(ics_entries, iopt, ics_stamp);
            }
            h << "<div class=\"calwidget\" data-ics=\"calendar-"
              << lang << ".ics\" data-events=\""
              << html_escape(ej.dump()) << "\"></div>\n";
            std::string cap = text(*n, "caption");
            if (!cap.empty())
                h << "<p class=\"meta\">" << html_escape(cap) << "</p>\n";
        } else if (n->glyph == "footer") {
            // COLLECTED, emitted after the loop: a footer mid-chain used to
            // close <main> early, dumping every later block OUTSIDE the page
            // container (the full-width-widget bug, 2026-07-22). Block order
            // in the chain must never decide whether a block is contained.
            footer_text = text(*n, "text");
        }
    };

    // ── the driver: group consecutive components by grid ROW; a row with >1
    // component is a horizontal band (each cell spans its `span`/12). Hero and
    // footer always stand alone, full width. ───────────────────────────────
        for (size_t i = 0; i < pchain.size();) {
            const maiz::SceneNode* n = pchain[i];
            int row = hormiga::doc_field_int(*n, "row", -1);
            // gather the run of same-row, non-hero/footer components
            size_t j = i + 1;
            if (row >= 0 && n->glyph != "hero" && n->glyph != "footer")
                while (j < pchain.size() &&
                       hormiga::doc_field_int(*pchain[j], "row", -2) == row &&
                       pchain[j]->glyph != "hero" && pchain[j]->glyph != "footer")
                    ++j;
            // BAND styling (W1): the row's leader can give the whole row a
            // background and go full-bleed — the alternating-section look of a
            // modern site. Hero has its own banner; it never gets a band.
            std::string bbg = field_value(*n, "band_bg");
            bool bfull = field_value(*n, "band_full") == "1";
            std::string bimg = stage_site_asset(field_value(*n, "band_image"));
            bool banded = n->glyph != "hero" &&
                          ((!bbg.empty() && bbg != "none") || bfull || !bimg.empty());
            if (banded) {
                h << "<section class=\"band";
                if (!bimg.empty()) h << " imaged"; // photo bg → white text
                else if (!bbg.empty() && bbg != "none") h << " " << bbg;
                if (bfull || !bimg.empty()) h << " full";
                h << "\">";
                if (!bimg.empty()) {
                    std::string bfil = field_value(*n, "band_filter");
                    if (bfil.empty() || bfil == "theme")
                        bfil = site_th.filter_class();
                    else if (bfil == "none") bfil.clear();
                    else bfil = "f-" + bfil;
                    h << "<div class=\"band-bg " << bfil
                      << "\" style=\"background-image:url('"
                      << html_escape(bimg) << "')\"></div><div class=\"band-shade\">"
                      << "</div>";
                }
                h << "<div class=\"band-inner\">\n";
            }
            /* A PLATFORM SET is a grid row holding two or more blocks that each
             * name a computer (render/download.hpp): app.js marks the visitor's
             * own and moves it first, and has nothing to hide it with. Decided
             * HERE at build time, the way `data-embed` is - the page states what
             * it is and the script only reacts. One such block on a row is not a
             * set; there is nothing to choose between. */
            size_t pset_n = 0;
            for (size_t k = i; j - i > 1 && k < j; ++k)
                if (!hormiga::platform_token(field_value(*pchain[k], "platform"))
                         .empty())
                    ++pset_n;
            const bool pset = pset_n > 1;
            if (j - i > 1) { // a real horizontal band
                h << "<div class=\"wrow" << (pset ? " platform-set" : "") << "\"";
                // the badge text rides the MARKUP: one app.js serves both languages
                if (pset)
                    h << " data-yours=\""
                      << html_escape(ui("For your computer", "Para tu computadora"))
                      << "\"";
                h << ">\n";
                for (size_t k = i; k < j; ++k) {
                    int span = std::clamp(
                        hormiga::doc_field_int(*pchain[k], "span", 12), 1, 12);
                    h << "<div class=\"wcol\" style=\"--span:" << span << "\">\n";
                    emit(pchain[k]);
                    h << "</div>\n";
                }
                h << "</div>\n";
            } else {
                emit(n);
            }
            if (banded) h << "</div></section>\n";
            i = j;
        }
        h << "</main>\n";
        /* The colophon rides the footer BLOCK and does not summon one: a page
         * with no footer still has no footer, exactly as before. */
        if (!footer_text.empty()) {
            h << "<footer class=\"site-foot\"><div class=\"wrap\">"
              << html_escape(footer_text);
            if (!colophon.empty())
                h << "<br><small>" << html_escape(colophon) << "</small>";
            h << "</div></footer>\n";
        }
        h << "<div id=\"lightbox\" class=\"lightbox\" hidden><span class=\"lb-close\">"
             "&times;</span><img id=\"lb-img\"><p id=\"lb-cap\"></p></div>\n"
          << "<script src=\"app.js" << cb << "\"></script>\n</body></html>\n";
        std::ofstream o(data_dir("site") / page_file(slug),
                        std::ios::binary | std::ios::trunc);
        o << h.str();
    }; // render_page

    // render every page (or the single legacy page), then the shared assets
    if (pages.empty())
        render_page("", site_title);
    else
        for (const auto& p : pages) render_page(p.slug, p.title);

    /* ── THE FLIER THAT OUTLIVED ITS EVENT (2026-08-28) ──────────────────────
     *
     * From the field report: *"a flier says 'September 16th' on its face, and
     * nothing in the database checks that against the event it is linked to.
     * Even a warning at render time — 'this flier's linked event is 9 days
     * past' — would have caught what §2 describes."*
     *
     * That is exactly what this is, and it is worth having even now that
     * `date:future` exists, because the two answer different questions. The
     * predicate is for a block whose author has decided the section is about
     * upcoming things. This is for every other block — the hand-tagged
     * `issue:aug2026` gallery, the `event_flier` placed by name — where the
     * author has NOT said anything about time and the page quietly goes stale
     * anyway. A query language that can express freshness does not make an
     * un-expressed page fresh.
     *
     * Reported, never corrected. Which sheet belongs on a page is the
     * operator's call and sometimes the answer is genuinely "leave it up" — a
     * flier for last week's clinic still tells a reader the clinic exists. The
     * renderer's job here is to end the silence, not to make the decision.
     *
     * ENGLISH PASS ONLY, and over `published_images` rather than over the
     * database: this is one fact per stale flier per render, about what a
     * visitor will actually see. Emitting it per language would double every
     * line and teach the reader to skim the log, which is how the original
     * problem stayed invisible for nine days. */
    if (lang == "en")
        for (const auto& st : hormiga::stale_published(data, published_images, today))
            log.push_back({"warn", "render",
                           st.rune + " is on a page, and the event it is linked "
                           "to was " + std::to_string(st.days) +
                           " day(s) ago. Either the flier belongs in an archive "
                           "section, or its block wants `date:future` in the "
                           "query - `effect query '<the expression> AND "
                           "date:future'` shows what that would publish."});
    /* ── AND SAY WHAT THIS PAGE IS ACTUALLY IN (2026-09-02) ──────────────────
     *
     * Counted by the `text()` lambda at the top of this function. One line
     * naming the number an operator cannot get any other way: how much of what
     * a visitor was just handed is written in the language they asked for.
     * Warn, not info — the failure is invisible from the page, because a Spanish
     * reader sees English prose under a Spanish URL and cannot tell a gap from a
     * choice. `effect translation-report` is the other half. */
    if (lang_fallbacks > 0) {
        const int seen = lang_hits + lang_fallbacks;
        log.push_back(
            {"warn", "render",
             "the " + std::string(lang) + " site fell back to another language " +
                 std::to_string(lang_fallbacks) + " time(s) out of " +
                 std::to_string(seen) + " (" +
                 std::to_string((int)((lang_hits * 100.0) / seen + 0.5)) +
                 "% written in " + std::string(lang) +
                 "). `effect translation-report " + std::string(lang) +
                 "` writes a script with the source text already in it."});
    }
    /* ── A DELETED PAGE HAS TO LEAVE THE SITE (2026-09-02) ───────────────────
     *
     * The author: *"sometimes i make a page for the website, then decide it's
     * not needed. Instead of still navigating to an old version of that page
     * (cuz i guess the link still exists) it should reroute to a custom 404."*
     *
     * The guess in that sentence is the diagnosis. `render_site` wrote one file
     * per page and removed nothing, so deleting a `page` rune took it out of
     * the nav, the sitemap and the model — and left `about-en.html` in `site/`,
     * where the next deploy uploaded it again. The old page stayed live at its
     * old URL, showing content the database no longer contains.
     *
     * A 404 was never the fix and there already is a themed one. The fix is
     * that `site/` is a MIRROR of the document rather than an accumulation of
     * every render that ever ran — the property the GitHub deployer builds its
     * commit with, applied one layer earlier so it holds for every host. Once
     * the file is gone the host's own 404 serves that URL, which is what was
     * asked for.
     *
     * ONLY files this render would have written: `<name>-<lang>.html` at the top
     * level, for a language this site publishes. Not `assets/`, `fonts/`,
     * `index.html`, `404.html`, `sitemap.xml`, `style.css`, `custom.css`,
     * `app.js` or the calendars; nothing in a subdirectory. `site/` is a folder
     * on somebody's disk and an operator may have put something there by hand —
     * a prune reasoning "anything I did not write is stale" would delete it.
     *
     * PER-LANGUAGE, because `effect render-site es` is a legitimate preview loop
     * and must not remove the English pages. Each pass prunes its own suffix. */
    {
        std::set<std::string> keep;
        if (pages.empty()) keep.insert(page_file(""));
        else
            for (const auto& p : pages) keep.insert(page_file(p.slug));
        const std::string suffix = "-" + std::string(lang) + ".html";
        std::error_code pec;
        std::vector<fs::path> gone;
        for (const auto& de : fs::directory_iterator(data_dir("site"), pec)) {
            if (pec) break;
            if (!de.is_regular_file(pec)) continue;
            const std::string fn = de.path().filename().string();
            if (fn.size() <= suffix.size() ||
                fn.compare(fn.size() - suffix.size(), suffix.size(), suffix) != 0)
                continue;
            if (keep.count(fn)) continue;
            gone.push_back(de.path());
        }
        for (const fs::path& p : gone) {
            std::error_code rec;
            fs::remove(p, rec);
            if (!rec)
                log.push_back({"info", "render",
                               "removed " + p.filename().string() +
                                   " - no page in this document renders to it "
                                   "any more, so the next publish takes it down "
                                   "rather than leaving it live at its old URL"});
        }
    }
    auto write = [&](const char* name, const std::string& body) {
        std::ofstream o(data_dir("site") / name, std::ios::binary | std::ios::trunc);
        o << body;
    };
    write("style.css", site_css(site_th));
    write("app.js", site_js());
    /* ONE CALENDAR PER LANGUAGE. A VEVENT's SUMMARY is prose, so the two
     * language passes were writing the same `calendar.ics` with different
     * titles and whichever ran last won — the English calendar was Spanish, or
     * the other way round, depending on argument order. `calendar.ics` stays as
     * the English one because it is a URL people paste into a calendar app and
     * a subscription that 404s is worse than one in the wrong language. */
    if (want_ics) {
        write(("calendar-" + std::string(lang) + ".ics").c_str(), ics);
        if (lang == "en") write("calendar.ics", ics);
    }

    // ── W5: a sitemap (search engines) + a themed 404 (deploy completeness).
    // Emitted once per render (English pass owns them). ──────────────────────
    if (lang == "en") {
        /* NO BASE URL, NO SITEMAP (2026-08-19).
         *
         * This fell back to the bare filename — `<loc>index-en.html</loc>` —
         * and Sitemaps 0.9 requires an absolute URL in `<loc>`, so the file
         * Google was pointed at was one it rejects. Omitting it is strictly
         * better than emitting an invalid one, and it is what the same function
         * already does for `og:url` and `og:image` twenty lines up under
         * exactly this condition. A stale sitemap.xml from a previous render
         * with a base URL is removed rather than left to contradict the site. */
        std::error_code sec;
        if (base_url.empty()) {
            fs::remove(data_dir("site") / "sitemap.xml", sec);
            log.push_back({"warn", "render",
                           "site.base_url is unset, so no sitemap.xml was "
                           "written (a relative <loc> is invalid and search "
                           "engines reject the file). `config set site.base_url "
                           "https://your-domain.org`"});
        } else {
        std::ostringstream sm;
        sm << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
              "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\" "
              "xmlns:xhtml=\"http://www.w3.org/1999/xhtml\">\n";
        auto loc = [&](const std::string& file) { return base_url + "/" + file; };
        /* BOTH LANGUAGES. The sitemap listed only the `-en` pages, so the
         * Spanish half of a bilingual site was not discoverable at all — on a
         * site for a Spanish-speaking community. Each entry carries the
         * `xhtml:link` alternates so the pair is understood as one page in two
         * languages rather than two pages saying the same thing. */
        std::vector<std::string> slugs;
        if (pages.empty()) slugs.push_back("");
        else for (const auto& p : pages) slugs.push_back(p.slug);
        for (const char* lg : {"en", "es"})
            for (const auto& sl : slugs) {
                sm << "  <url><loc>" << loc(page_file(sl, lg)) << "</loc>\n"
                   << "    <xhtml:link rel=\"alternate\" hreflang=\"en\" href=\""
                   << loc(page_file(sl, "en")) << "\"/>\n"
                   << "    <xhtml:link rel=\"alternate\" hreflang=\"es\" href=\""
                   << loc(page_file(sl, "es")) << "\"/>\n"
                   << "  </url>\n";
                (void)lg;
            }
        sm << "</urlset>\n";
        write("sitemap.xml", sm.str());
        }

        std::ostringstream nf;
        nf << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
              "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
              "<title>Page not found - " << html_escape(site_title) << "</title>"
              "<link rel=\"icon\" href=\"" << favicon << "\">"
              "<link rel=\"stylesheet\" href=\"style.css\"></head><body class=\""
           << site_th.body_class()
           << "\"><main class=\"wrap\" style=\"text-align:center;padding:80px 0\">"
              "<h1>Page not found</h1><p class=\"prose\" style=\"margin:0 auto\">"
              "That page doesn't exist (or moved).</p><p><a class=\"btn\" href=\""
           << page_file(home_slug) << "\">Back home</a></p></main></body></html>\n";
        write("404.html", nf.str());

        /* ── A ROOT `index.html`, WITHOUT WHICH A STATIC HOST SERVES NOTHING ──
         *
         * The home page is `index-en.html`. Cloudflare Pages, Netlify, GitHub
         * Pages and nginx all serve `index.html` at a directory root and none
         * of them serve `index-en.html`, so a complete, correct, deployed site
         * answered `/` with a 404. `deploy_site`'s own precondition check
         * already knows both names are possibilities; only one was ever
         * written.
         *
         * A REDIRECT RATHER THAN A COPY. Copying the English page to
         * `index.html` would duplicate the site's most-linked page at two URLs
         * — bad for search, and worse, it would go stale against its twin the
         * moment anything is edited. This page is chrome: it picks a language
         * from the browser and gets out of the way, with a `<meta refresh>` and
         * two real links behind it so a visitor with JavaScript off, or a
         * crawler, still lands somewhere. `hreflang` alternates make the pair
         * legible to a search engine.
         *
         * It carries no content of its own, so nothing here can go stale. */
        std::ostringstream rt;
        rt << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
              "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
              "<title>" << html_escape(site_title) << "</title>"
              "<link rel=\"icon\" href=\"" << favicon << "\">"
              "<link rel=\"canonical\" href=\""
           << html_escape(base_url.empty() ? page_file(home_slug, "en")
                                           : base_url + "/" + page_file(home_slug, "en"))
           << "\">"
              "<link rel=\"alternate\" hreflang=\"en\" href=\""
           << page_file(home_slug, "en") << "\">"
              "<link rel=\"alternate\" hreflang=\"es\" href=\""
           << page_file(home_slug, "es") << "\">"
              "<link rel=\"alternate\" hreflang=\"x-default\" href=\""
           << page_file(home_slug, "en") << "\">"
              "<meta http-equiv=\"refresh\" content=\"0;url="
           << page_file(home_slug, "en") << "\">"
              "<link rel=\"stylesheet\" href=\"style.css\"></head><body class=\""
           << site_th.body_class() << "\">"
              "<script>(function(){var l=(navigator.language||'en').toLowerCase();"
              "location.replace(l.indexOf('es')===0?'"
           << page_file(home_slug, "es") << "':'" << page_file(home_slug, "en")
           << "');})();</script>"
              "<main class=\"wrap\" style=\"text-align:center;padding:80px 0\">"
              "<p><a class=\"btn\" href=\"" << page_file(home_slug, "en")
           << "\">English</a> <a class=\"btn\" href=\""
           << page_file(home_slug, "es") << "\">Espa&ntilde;ol</a></p>"
              "</main></body></html>\n";
        write("index.html", rt.str());
    }
    return (data_dir("site") / ("index-" + std::string(lang) + ".html")).string();
}

/* Start the live preview (B2): server up, first render of BOTH targets, and
 * the browser opens on the site (the email twin lives at /preview-<lang>.html
 * on the same server). Idempotent. */
bool HormigaApp::preview_start() {
    if (!preview_srv.start(base_dir)) {
        toast("live preview: could not bind a localhost port", true);
        return false;
    }
    render_site(preview_lang ? "es" : "en");
    render_preview(preview_lang ? "es" : "en");
    preview_srv.bump();
    preview_live = true;
    std::string url = "http://127.0.0.1:" + std::to_string(preview_srv.port()) +
                      "/site/index-" + (preview_lang ? "es" : "en") + ".html";
    toast("live preview at " + url + " - edits re-render automatically");
    if (on_open) on_open(url);
    return true;
}

/* Start the LOCAL WEB HOST (the Antfarm's hol_localhost node): render the
 * website fresh, then serve the built site/ folder clean on its own localhost
 * port — a stand-in for a real domain (the website is no longer just a file).
 * Newsletters are NOT hosted; they stay static HTML by nature. Idempotent. */
bool HormigaApp::host_start() {
    render_site("en"); // a real site carries both languages
    render_site("es");
    if (!host_srv.start(data_dir("site"), /*host_mode=*/true, /*port=*/8780)) {
        toast("local host: could not bind a localhost port", true);
        return false;
    }
    std::string url = "http://127.0.0.1:" + std::to_string(host_srv.port()) + "/";
    toast("serving the site at " + url + " - a local stand-in for your domain");
    if (on_open) on_open(url);
    return true;
}


