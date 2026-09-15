/* render/assets.cpp — files on their way into a built site.
 *
 * Staging (copy-if-changed, not copy-once: a corrected flier used to have no
 * effect on the site forever) and the downscaled derivative a gallery tile
 * points at, while the lightbox keeps the original. Image processing, not
 * markup, which is why it is its own unit. */

#include "app/app_internal.hpp"
#include "render/text.hpp" // the helpers both output domains share
#include "json.hpp" // theme/menu/embed payloads are JSON on the wire
#include "stb_image_write.h" // decls only - gallery thumbnails; the ONE
#include "lucide_icons.hpp"  // vendored SVG icon paths (ISC; vendor/icons)
#include "site_css_data.hpp"
#include "site_js_data.hpp"

std::string HormigaApp::stage_site_asset(const std::string& rel) {
    if (rel.empty()) return {};
    fs::path src = resolve_file(rel); // the database folder, else beside the program
    if (!fs::exists(src)) return {};
    fs::path dest_dir = data_dir("site") / "assets";
    std::error_code ec;
    fs::create_directories(dest_dir, ec);
    fs::path dest = dest_dir / src.filename();
    /* STAGING IS A MIRROR, NOT A ONE-TIME COPY (2026-08-19).
     *
     * This was `if (!exists(dest)) copy_file(...)`, so once a flier had been
     * staged, correcting the source file in `assets/` had NO effect on the site,
     * forever, with no warning. The workaround was to know to delete
     * `site/assets` by hand — the kind of thing discovered a week after a
     * corrected flier went live still showing the wrong date.
     *
     * Size-or-newer rather than a content hash: staging runs once per image per
     * render and a hash of a 5 MB PNG is real time for a check that a stat
     * answers. A same-size, same-mtime edit is not a thing an image editor
     * produces. */
    bool stale = true;
    if (fs::exists(dest, ec)) {
        std::error_code e1, e2;
        const auto ssz = fs::file_size(src, e1), dsz = fs::file_size(dest, e2);
        const auto stm = fs::last_write_time(src, e1);
        const auto dtm = fs::last_write_time(dest, e2);
        stale = e1 || e2 || ssz != dsz || stm > dtm;
    }
    if (stale)
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    return "assets/" + src.filename().string(); // site-relative href
}

/* ── THE OPERATOR'S ESCAPE HATCH FOR THE STYLESHEET (2026-09-02) ─────────────
 *
 * Field report, Part 3, and it is the structural one in that document:
 *
 *   > `render_site` unconditionally does `write("style.css", site_css(site_th))`.
 *   > D1, D2 and D3 are each two lines of CSS. Every one of them is blocked on
 *   > a Hormiga release, because there is nowhere for an operator to put two
 *   > lines of CSS.
 *
 * That is a real structural gap and not a styling preference. Three cosmetic
 * defects on a live public site, each two lines, each unfixable by the person
 * whose site it is.
 *
 * ── WHY A FILE AND NOT A FIELD ───────────────────────────────────────────────
 *
 * A `custom_css` field on a `page` or a `theme` rune would be the shorter
 * patch, and it is the wrong one. A field is model data: it travels in the
 * `.miga`, it merges between devices, it can be written by an import, and — the
 * part that matters — it is a way to get authored text into a `<style>` block
 * on a public page. `video` already refuses arbitrary embed markup for exactly
 * this reason, and a stylesheet field would walk it back through a side door.
 *
 * A FILE beside the database is a different act. It is the operator's own
 * machine and the operator's own hand, it does not travel with the data, and a
 * `<link rel="stylesheet">` cannot execute anything whatever the file contains.
 * The render seam is untouched: no model value reaches the page that did not
 * reach it before.
 *
 * ── AND IT IS NOT A NEW PATTERN ──────────────────────────────────────────────
 *
 * `data_dir("fonts")` has worked this way since the webfonts shipped: an
 * organization drops its own `.woff2` beside the database, `render_site` stages
 * it, and an empty folder changes nothing. This is that, with one file and one
 * `<link>`.
 *
 * A missing file is the ordinary case and is silent. A file that IS there is
 * logged, because a stylesheet an operator forgot they wrote is a very good
 * explanation for a page that looks wrong six months later.
 */
/* ── WHERE THE ORGANIZATION'S DATA IS EXPECTED TO LIVE (2026-09-02) ──────────
 *
 * Field report D6, and it is the expensive one in that document because it
 * cost an hour and produced no error message at any point.
 *
 * `kDataMantle` is `"demo-org"`, hardcoded. `render_site`, `render_preview`,
 * `publish_index` and `effect query` all project THAT mantle; `ls` projects the
 * ACTIVE one. So a new client doing the obvious first thing —
 *
 *     mantle new clicklafont
 *     rune new image img-cover-sky
 *     tag img-cover-sky +type:image +cover
 *
 * — had every command succeed, `validate` say `valid`, and `render-site` report
 * `ok` with every gallery on every page empty and no asset staged. What
 * eventually cracked it was two verbs the guide says are the same grammar over
 * the same data disagreeing: `ls --tag` found the rune and `effect query` found
 * nothing.
 *
 * ── THE CHEAP FIX, NOT THE EXPENSIVE ONE ─────────────────────────────────────
 *
 * The report is explicit that the ask is NOT "make the name configurable":
 *
 *   > The cheap version that removes the whole class of failure is a warning at
 *   > render time. … That is one lookup and one message, and it turns a silent
 *   > hour into a line of output.
 *
 * Making the name configurable is a bigger change with its own failure modes (a
 * database whose data mantle is named in config, and config that has been lost,
 * is a database that renders empty for a different reason). A warning removes
 * the SILENCE, which is the whole defect. Whether `demo-org` should be the
 * permanent name of a real organization's data namespace is a separate question
 * and it is in `okf/developer_questions.md`.
 *
 * Only runs when the data mantle is empty or absent — a render on a working
 * database does one scene projection per other mantle and then never speaks.
 */
void HormigaApp::warn_if_data_is_elsewhere(const maiz::Scene& data) {
    if (!data.nodes.empty()) return; // the ordinary case, and it says nothing

    /* The glyphs a block query can actually reach. Deliberately a list rather
     * than "anything that is not a block": a document mantle full of `hero` and
     * `narrative` runes is a DOCUMENT, and reporting it here as misplaced data
     * would make the warning noise on every database with two newsletters. */
    static const char* kDataGlyphs[] = {"contact", "event",    "image",
                                        "resource", "location", "organization"};
    struct Found { std::string mantle; std::map<std::string, int> by_glyph; int total = 0; };
    std::vector<Found> found;

    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' '))
            line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (line.empty() || line == "(no mantles)" || line == kDataMantle ||
            line == kAntfarmMantle || line == kAlloMantle || line == kCivicMantle)
            continue;
        maiz::ProjectOptions po;
        po.mantle = line;
        const maiz::Scene other = maiz::project_scene(core, po);
        Found f;
        f.mantle = line;
        for (const auto& n : other.nodes)
            for (const char* g : kDataGlyphs)
                if (n.glyph == g) { ++f.by_glyph[g]; ++f.total; break; }
        if (f.total) found.push_back(std::move(f));
    }

    if (found.empty()) {
        /* An empty data mantle with no data anywhere else is a NEW database,
         * which is a perfectly ordinary thing to render. Saying nothing here is
         * what keeps the message above worth reading when it does appear. */
        return;
    }
    for (const Found& f : found) {
        std::string counts;
        for (const auto& [g, n] : f.by_glyph) {
            if (!counts.empty()) counts += ", ";
            counts += std::to_string(n) + " " + g;
        }
        log.push_back(
            {"warn", "render",
             std::string("the data mantle '") + kDataMantle +
                 "' is empty, but " + counts + " rune(s) live in '" + f.mantle +
                 "'. Every block query, `effect query` and the published index "
                 "read '" + kDataMantle + "' and nothing else, so those runes "
                 "were not considered and no asset of theirs was staged. "
                 "`mantle rename " + f.mantle + " " + kDataMantle +
                 "` if that mantle is this organization's data."});
    }
}

bool HormigaApp::stage_custom_css() {
    const fs::path src = base_dir / "custom.css";
    const fs::path dest = data_dir("site") / "custom.css";
    std::error_code ec;
    if (!fs::exists(src, ec)) {
        /* A STALE COPY IS REMOVED. Deleting `custom.css` beside the database
         * has to actually turn the overrides off — otherwise the file stays in
         * `site/`, keeps being deployed, and the operator's way of undoing
         * their own change does nothing. Same reasoning as the sitemap that is
         * removed when `site.base_url` is unset. */
        fs::remove(dest, ec);
        return false;
    }
    fs::create_directories(data_dir("site"), ec);
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        log.push_back({"warn", "render",
                       "custom.css is beside the database but could not be "
                       "staged into site/: " + ec.message()});
        return false;
    }
    log.push_back({"info", "render",
                   "custom.css staged from " + src.string() +
                       " and linked after the built stylesheet - it overrides "
                       "the theme, and deleting it turns the overrides off"});
    return true;
}

/* ── a DOWNSCALED derivative for a gallery tile (2026-08-19) ─────────────────
 *
 * Reported from a real build: three of one organization's fliers are 3-5 MB
 * PNG scans, and the gallery displayed 23 MB of images as ~300px thumbnails.
 * Every one of them downloads in full on a phone, on the org's audience's data
 * plan, to be drawn a tenth of the size.
 *
 * `stage_site_asset` is the choke point every image already passes through, so
 * the derivative is made here and nowhere else. The TILE points at the small
 * one; the lightbox keeps pointing at the original, because "see the flier full
 * size" is the whole reason a flier is on the page.
 *
 * JPEG rather than WebP: `stb_image_write` is what is vendored, a WebP encoder
 * is not, and vendoring an encoder to save a further 30% on an image that is
 * already 4% of its former size is not a trade worth a new dependency. Images
 * already small enough are left alone and the tile points at the original —
 * a derivative that is bigger than its source is a bug, not an optimization.
 *
 * Returns "" when no derivative was made, which the caller reads as "use the
 * original": a decode failure must never cost a page its picture. */
std::string HormigaApp::site_thumb(const std::string& staged_rel, int max_w) {
    if (staged_rel.empty() || max_w <= 0) return {};
    const fs::path src = data_dir("site") / staged_rel;
    std::error_code ec;
    if (!fs::exists(src, ec)) return {};
    // under ~200 KB is already a thumbnail's worth of bytes; leave it be
    if (fs::file_size(src, ec) < 200u * 1024u || ec) return {};
    const std::string stem = src.stem().string();
    const fs::path dest = src.parent_path() / (stem + "-thumb.jpg");
    const std::string rel = "assets/" + dest.filename().string();
    if (fs::exists(dest, ec)) {
        std::error_code e1, e2;
        if (fs::last_write_time(dest, e1) >= fs::last_write_time(src, e2) && !e1 && !e2)
            return rel; // still current
    }
    int w = 0, h = 0, ch = 0;
    /* FOUR channels, then composited over white below. Decoding straight to 3
     * throws the alpha away rather than resolving it, so every transparent
     * pixel of a logo or a cut-out PNG arrives as whatever RGB happened to sit
     * under it — usually black — and JPEG has no alpha to put it back into.
     * Compositing is the only honest answer at this seam; white because that is
     * what a page is, and because a mark that needs a dark ground has
     * `org.logo_dark`. */
    unsigned char* px = stbi_load(src.string().c_str(), &w, &h, &ch, 4);
    if (!px) return {};
    if (w <= max_w) { stbi_image_free(px); return {}; } // already small enough
    const int dw = max_w;
    const int dh = std::max(1, (int)((long long)h * max_w / w));
    std::vector<unsigned char> out((size_t)dw * dh * 3);
    /* Box filter over the source rectangle each destination pixel covers.
     * Nearest-neighbour on a 5000px scan aliases text into noise, which on a
     * flier is the one thing a thumbnail has to keep legible. */
    for (int y = 0; y < dh; ++y) {
        const int sy0 = (int)((long long)y * h / dh);
        const int sy1 = std::max(sy0 + 1, (int)((long long)(y + 1) * h / dh));
        for (int x = 0; x < dw; ++x) {
            const int sx0 = (int)((long long)x * w / dw);
            const int sx1 = std::max(sx0 + 1, (int)((long long)(x + 1) * w / dw));
            long long acc[3] = {0, 0, 0};
            long long n = 0;
            for (int sy = sy0; sy < sy1 && sy < h; ++sy)
                for (int sx = sx0; sx < sx1 && sx < w; ++sx, ++n) {
                    const unsigned char* p = px + ((size_t)sy * w + sx) * 4;
                    const int a = p[3];
                    // over white, per-sample so a soft edge averages correctly
                    for (int c = 0; c < 3; ++c)
                        acc[c] += (p[c] * a + 255 * (255 - a)) / 255;
                }
            unsigned char* d = out.data() + ((size_t)y * dw + x) * 3;
            for (int c = 0; c < 3; ++c)
                d[c] = (unsigned char)(n ? acc[c] / n : 255);
        }
    }
    stbi_image_free(px);
    const int ok = stbi_write_jpg(dest.string().c_str(), dw, dh, 3, out.data(), 82);
    if (!ok) return {};
    // a derivative larger than its source helps nobody
    std::error_code e3, e4;
    if (fs::file_size(dest, e3) >= fs::file_size(src, e4) && !e3 && !e4) {
        fs::remove(dest, e3);
        return {};
    }
    return rel;
}
