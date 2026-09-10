/* download.hpp — publishing a file a visitor can keep.
 *
 * ── why this file exists ─────────────────────────────────────────────────────
 *
 * The portfolio agent, 2026-09-02, and it was blocking a deploy:
 *
 *   > **A Hormiga website cannot publish a file a visitor can download.** …
 *   > The single most important control on it is **Download Resume** — it is
 *   > the conversion the whole site is arranged around, and it is the one thing
 *   > on the page a visitor is meant to leave with.
 *
 * Their diagnosis was right and unusually complete: three of the four pieces
 * already existed. `link` emits the correct markup, `stage_site_asset()` is
 * type-agnostic and would have staged a PDF correctly today, and `resource`
 * (`path`, `topic`) was declared, editable in the GUI and **rendered by
 * nothing** — the "declared but invisible" trap this codebase has now hit three
 * times (`image_grid.columns`, the whole of `hol_github`, and this).
 *
 * The missing piece was one block that calls the staging and emits the anchor.
 *
 * ── it is a resource for an ORGANIZATION too, not a portfolio nicety ─────────
 *
 * A résumé is what surfaced it, but the thing being asked for is "a named file,
 * on the site, with a link to it": a flier PDF, the bylaws, an annual report, a
 * know-your-rights sheet, a printable calendar. Those are the documents an
 * outreach organization is most often asked for, and until now the answer was
 * to host them somewhere Hormiga does not manage.
 *
 * ── ONE FILE, NAMED, RATHER THAN A QUERY ─────────────────────────────────────
 *
 * The report weighed a `download_grid` over `resource` runes against a single
 * block and landed on the single block; taking their lean, and their reason is
 * the right one:
 *
 *   > A résumé, a flier PDF, a bylaws document and an annual report are all one
 *   > named file placed on purpose, which is a different act from a gallery.
 *   > The grid can come later with a clearance conversation attached; the
 *   > single block needs none, because **naming the file is the consent**.
 *
 * That last clause is why this block does not carry a `clearance:` gate the way
 * `directory` does. `directory` publishes a query result, so it must ask
 * permission per person; this publishes the one file an author pointed at.
 */
#pragma once

#include "render/text.hpp"

#include <cctype>
#include <cstdio>
#include <string>

namespace hormiga {

/* ── WHAT THIS BLOCK REFUSES TO PUBLISH, AND WHY IT IS AT THE SEAM ───────────
 *
 * The report raised this against its own proposal, which is the good kind of
 * ask:
 *
 *   > `stage_site_asset` copies whatever it is given, and a static host serves
 *   > by extension — so `download.file` pointing at a `.html` puts an
 *   > author-controlled page on the site's own origin.
 *
 * The comparison they draw is exact. `custom.css` is a FILE and not a model
 * field precisely because model data arrives by import and by merge, from a
 * device somebody else was using — and a `<style>` block was refused for that
 * reason. `download.file` is a model field, so a merge could set it, and a
 * merge that can publish an executable page on the organization's own origin is
 * a hole rather than a feature.
 *
 * Same-origin is the whole of it: a `.html` or `.js` served from the site can
 * read anything the site can, and an `.svg` is a document that may carry
 * script. A `.pdf` cannot. So the refusal is by extension, it is a closed list
 * of the dangerous ones rather than an allow-list of the safe ones — an
 * allow-list would refuse the `.zip` of photos an organization actually wants
 * to publish, and this block's job is to publish files.
 *
 * Returns "" when the file is publishable, or a sentence naming the problem.
 * The renderer says it out loud rather than silently skipping: a Download
 * button that is missing for an unstated reason is the silence this project
 * keeps finding. */
inline std::string download_refusal(const std::string& path) {
    std::string ext;
    const size_t dot = path.rfind('.');
    if (dot != std::string::npos)
        for (size_t i = dot + 1; i < path.size(); ++i)
            ext += (char)std::tolower((unsigned char)path[i]);
    static const char* kRefused[] = {"html", "htm", "svg", "js", "mjs",
                                     "xhtml", "shtml", "wasm"};
    for (const char* r : kRefused)
        if (ext == r)
            return "a ." + ext +
                   " served from the site's own origin can act with the site's "
                   "own authority, and this field is model data that an import "
                   "or a merge can set. Publish it as a page, or rename it to a "
                   "type a browser will not execute";
    return {};
}

/* ── PLATFORM SETS: "give me the one for MY computer" (2026-09-08) ──────────
 *
 * The author's request, verbatim: *"it should detect the system (linux,
 * windows, mac), and then provide the correct download for hormiga."*
 *
 * The Click LaFont agent refused to build it and was right to, and their report
 * is the design this implements. Three walls stood in front of the obvious
 * version, and only the third is about us:
 *
 *   1. It needs JavaScript, and there is no seam for author-supplied script —
 *      `download_refusal` below and the `custom.css`-is-a-file decision exist
 *      to keep model data from becoming code on the site's own origin. An
 *      OS-detecting `<script>` in a rune field is exactly that shape.
 *   2. `download-page.md` §5(d) argues against detection on its own merits: a
 *      page that silently offers nothing is indistinguishable from a broken
 *      page, and a user agent string is a guess.
 *   3. There is one build. Detection today can only tell a Mac visitor there is
 *      nothing for them, which is a sentence, not a feature.
 *
 * So this is RENDERER-OWNED, at the same trust level as the lightbox and the
 * video click-to-load that already ship in `app.js` — not an author script. A
 * `download` or a `link` declares which platform it is for; a grid ROW holding
 * two or more of them is a platform set; and `app.js` marks the one that
 * matches and moves it to the front of its row.
 *
 * **It never hides one, and that is the whole point.** §5(d) is satisfied by
 * construction rather than by an author remembering it, which is the difference
 * between a rule and a design. With scripting off, the author's order stands and
 * every platform is on the page — the same `<noscript>` guarantee D1 bought.
 *
 * The vocabulary is `<family>` or `<family>-<arch>`: `windows-x64`, `macos`,
 * `macos-arm64`, `linux-x64`, plus `any` (the default, so nothing changes for
 * the résumé and the flier PDF this block was built for). It is the vocabulary
 * `void.json` and `mago plan <app> --platform <name>` already speak, which is
 * what makes it compose past the one page that asked for it.
 *
 * THE ARCH IS CARRIED AND NEVER MATCHED ON. A browser will not tell you the
 * architecture it is running on; `navigator.platform` reports "Win32" on a
 * 64-bit machine and has done for twenty years. Matching a family is a guess
 * that is usually right and costs a mislabelled badge when it is wrong;
 * matching an arch would be a guess that is often wrong about the one thing the
 * visitor cannot check. So the family is the unit of detection and the arch is
 * there for the author, the filename and the release. */
inline std::string platform_family(const std::string& raw) {
    std::string t;
    for (char c : raw)
        if (!std::isspace((unsigned char)c))
            t += (char)std::tolower((unsigned char)c);
    if (t.empty() || t == "any") return {};
    static const char* kFamilies[] = {"windows", "macos", "linux"};
    for (const char* f : kFamilies) {
        const std::string fam = f;
        if (t == fam) return fam;
        if (t.size() > fam.size() + 1 && t.compare(0, fam.size(), fam) == 0 &&
            t[fam.size()] == '-')
            return fam;
    }
    return {}; // unknown — the caller warns rather than guessing
}

/* Whether a value is one this block understands at all. `any` and empty are
 * understood and mean "every computer"; `platform_family` returns "" for those
 * too, so the two questions need separate answers or a typo would render as
 * `any` in silence — the failure mode `image_grid.columns` taught us. */
inline bool platform_known(const std::string& raw) {
    std::string t;
    for (char c : raw)
        if (!std::isspace((unsigned char)c))
            t += (char)std::tolower((unsigned char)c);
    return t.empty() || t == "any" || !platform_family(t).empty();
}

/* The token as it goes into the markup: lower-cased, whitespace stripped, and
 * "" when it means every computer. `data-platform` is absent rather than
 * `any` — an attribute that is present on everything selects nothing. */
inline std::string platform_token(const std::string& raw) {
    std::string t;
    for (char c : raw)
        if (!std::isspace((unsigned char)c))
            t += (char)std::tolower((unsigned char)c);
    if (t.empty() || t == "any" || platform_family(t).empty()) return {};
    return t;
}

/* Read a `platform` field: the token for the markup, plus the sentence to say
 * out loud when the value is one this renderer does not know.
 *
 * The two answers travel together because they are one decision. A typo must
 * not render as `any` in silence — that is the `image_grid.columns` failure in
 * a shape that matters more, since an author who wrote `win64` would get a page
 * that LOOKS like a platform set and marks nobody's computer. */
inline std::string platform_field(const std::string& raw, std::string* warn) {
    if (!platform_known(raw)) {
        if (warn)
            *warn = "platform '" + raw + "' is not a value this renderer knows, "
                    "so it was treated as `any` and this block joins no platform "
                    "set. Use `any`, or a family with an optional architecture: "
                    "windows-x64, macos, macos-arm64, linux-x64";
        return {};
    }
    return platform_token(raw);
}

/* A human size for the file, because it is the one piece of metadata a visitor
 * wants before they click and it costs a `stat` the staleness check already
 * does. Deliberately coarse — nobody needs three significant figures to decide
 * whether to tap a link on a phone. */
inline std::string human_size(unsigned long long bytes) {
    char buf[32];
    if (bytes >= 1024ull * 1024ull * 1024ull)
        std::snprintf(buf, sizeof buf, "%.1f GB",
                      (double)bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ull * 1024ull)
        std::snprintf(buf, sizeof buf, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024ull)
        std::snprintf(buf, sizeof buf, "%llu KB", bytes / 1024ull);
    else
        std::snprintf(buf, sizeof buf, "%llu bytes", bytes);
    return buf;
}

/* The extension, upper-cased, as a reader recognises it: "PDF", "DOCX", "ZIP". */
inline std::string file_kind(const std::string& path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "FILE";
    std::string ext;
    for (size_t i = dot + 1; i < path.size() && ext.size() < 6; ++i)
        ext += (char)std::toupper((unsigned char)path[i]);
    return ext.empty() ? "FILE" : ext;
}

struct DownloadCard {
    std::string href;    // site-relative for web, absolute for email ("" = none)
    std::string label;
    std::string caption;
    std::string meta;    // "PDF · 12 KB" ("" = unknown, e.g. the email path)
    std::string platform; // normalized token, "" = every computer
    bool card = false;   // `download_style card` rather than a button
};

/* ` data-platform="..."`, or nothing at all. Shared by `download` and by
 * `link`, which is the block the installer button actually is — see
 * `download-page.md` §3: the 7 MB installer lives on GitHub Releases, so the
 * button that offers it is a navigation, not a staged file. A `platform` that
 * only reached `download` could not have served the page that asked for it. */
inline std::string platform_attr(const std::string& token) {
    if (token.empty()) return {};
    return " data-platform=\"" + html_escape(token) + "\"";
}

/* The WEB rendering.
 *
 * `download` on the anchor is the attribute that makes a browser SAVE rather
 * than navigate — without it a PDF opens in the viewer tab, which is a
 * different act from the one the button promises. It is same-origin-only by
 * spec, which is exactly the case here because the file is staged into the
 * site's own `assets/`.
 *
 * The meta line is inside the anchor on purpose: it is part of what is being
 * offered, and a visitor deciding whether to tap on a phone wants the size
 * before the tap rather than after it. */
inline std::string download_web(const DownloadCard& d) {
    if (d.href.empty()) return {};
    std::string h;
    if (d.card) {
        h += "<a class=\"dl-card reveal\" href=\"" + html_escape(d.href) +
             "\" download" + platform_attr(d.platform) + ">";
        h += "<span class=\"dl-icon\" aria-hidden=\"true\">" +
             html_escape(d.meta.empty() ? std::string("FILE")
                                        : d.meta.substr(0, d.meta.find(' '))) +
             "</span>";
        h += "<span class=\"dl-body\"><span class=\"dl-label\">" +
             html_escape(d.label) + "</span>";
        if (!d.caption.empty())
            h += "<span class=\"dl-caption\">" + html_escape(d.caption) + "</span>";
        if (!d.meta.empty())
            h += "<span class=\"dl-meta\">" + html_escape(d.meta) + "</span>";
        h += "</span></a>\n";
        return h;
    }
    h += "<p class=\"dl-wrap reveal\"><a class=\"btn download\" href=\"" +
         html_escape(d.href) + "\" download" + platform_attr(d.platform) + ">" +
         html_escape(d.label);
    if (!d.meta.empty())
        h += " <span class=\"dl-meta\">" + html_escape(d.meta) + "</span>";
    h += "</a>";
    if (!d.caption.empty())
        h += "<span class=\"meta dl-caption\">" + html_escape(d.caption) +
             "</span>";
    h += "</p>\n";
    return h;
}

/* The EMAIL rendering: a button, and only when there is an absolute address for
 * it to point at.
 *
 * A newsletter cannot carry the file, so the block becomes a link — and a
 * RELATIVE href in email opens nothing in anybody's inbox. The report's lean,
 * taken: emit the absolute link when `site.base_url` is set, and when it is not,
 * say so in the render log rather than emitting an anchor that silently breaks
 * in every mail client. That is the same class of failure the per-language
 * publish guard exists to prevent — invisible from the side you check it on. */
inline std::string download_email(const DownloadCard& d,
                                  const std::string& accent) {
    std::string h;
    if (d.href.empty()) {
        // no absolute address: say what is on offer, and where it lives
        h += "<p style=\"margin:12px 0;color:#444\"><strong>" +
             html_escape(d.label) + "</strong>";
        if (!d.caption.empty())
            h += "<br><span style=\"color:#777;font-size:13px\">" +
                 html_escape(d.caption) + "</span>";
        h += "<br><span style=\"color:#888;font-size:13px\">"
             "Available on the website.</span></p>\n";
        return h;
    }
    h += email_button(d.href, d.label + (d.meta.empty() ? "" : " (" + d.meta + ")"),
                      accent);
    if (!d.caption.empty())
        h += "<p style=\"color:#777;font-size:13px;margin:2px 0 12px\">" +
             html_escape(d.caption) + "</p>\n";
    return h;
}

} // namespace hormiga
