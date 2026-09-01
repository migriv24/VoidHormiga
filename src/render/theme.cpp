/* render/theme.cpp — the stylesheet and the script the website ships.
 *
 * `site_css` is the COMPUTED half of the theme: WCAG contrast tokens derived
 * host-side from the organization's own colours, so the page receives finished
 * answers and the browser decides nothing (the compute boundary). The static
 * half is a real stylesheet at render/web/style.css, embedded at build time —
 * 816 lines of CSS and JavaScript used to live in C++ raw-string literals here,
 * with no highlighting, no linter and no formatter. */

#include "app/app_internal.hpp"
#include "render/theme.hpp"
#include "json.hpp" // theme/menu/embed payloads are JSON on the wire
#include "stb_image_write.h" // decls only - gallery thumbnails; the ONE
#include "lucide_icons.hpp"  // vendored SVG icon paths (ISC; vendor/icons)
#include "site_css_data.hpp"
#include "site_js_data.hpp"
#include "render/text.hpp" // linkify/prose/clip/human_date/ics/gcal
#include <cctype>
#include <cstring>


#include "render/text.hpp" // linkify/prose/clip/human_date/ics/gcal

#include <cctype>
#include <cstring>

std::string site_css(const SiteTheme& t) {
    // the THEME socket (builder.md QE): the Style tab's values land here — the
    // Style tab and Builder grow as ONE system (author, 2026-07-23). The web
    // pack is the MODERN renderer: fluid type, full-bleed hero, scroll-reveal +
    // hover motion — all theme-parameterized and reduced-motion-degradable.
    // Three of the author's style axes live here: browser dark/light
    // reactivity (a prefers-color-scheme variant), style PRESETS (clean / soft-
    // neumorphic / bold-maximal, as body classes), and heading FONT choice.
    const char* headf0 = kFontStacks[std::clamp(t.font, 0, kNumFonts - 1)];
    const char* bodyf0 = kFontStacks[std::clamp(t.bodyfont, 0, kNumFonts - 1)];
    /* The org's own family goes FIRST and the chosen stack stays behind it, so
     * a missing or still-downloading file degrades to the design's own choice
     * rather than to whatever the browser feels like. */
    const std::string headf_s =
        t.font_custom.empty() ? std::string(headf0)
                              : "'" + t.font_custom + "'," + headf0;
    const std::string bodyf_s =
        t.font_custom.empty() ? std::string(bodyf0)
                              : "'" + t.font_custom + "'," + bodyf0;
    const char* headf = headf_s.c_str();
    const char* bodyf = bodyf_s.c_str();
    const char* presetClass = t.body_class();
    const float scales[] = {0.90f, 0.95f, 1.00f, 1.08f, 1.18f};
    const int radii[] = {0, 6, 14, 22, 32};
    char sc[24];
    std::snprintf(sc, sizeof sc, "%.1f%%",
                  100.0f * scales[std::clamp(t.scale, 0, 4)]);
    // @font-face: the VENDORED webfonts, embedded as local files under
    // site/fonts/ (copied there by render_site) — no CDN, self-contained,
    // identical rendering on every visitor's machine.
    std::string css =
        "@font-face{font-family:'Inter';font-weight:400;font-style:normal;"
        "font-display:swap;src:url('fonts/inter-400.woff2') format('woff2')}\n"
        "@font-face{font-family:'Inter';font-weight:600;font-style:normal;"
        "font-display:swap;src:url('fonts/inter-600.woff2') format('woff2')}\n"
        "@font-face{font-family:'Inter';font-weight:700;font-style:normal;"
        "font-display:swap;src:url('fonts/inter-700.woff2') format('woff2')}\n"
        "@font-face{font-family:'Source Serif 4';font-weight:400;font-style:normal;"
        "font-display:swap;src:url('fonts/sourceserif-400.woff2') format('woff2')}\n"
        "@font-face{font-family:'Source Serif 4';font-weight:700;font-style:normal;"
        "font-display:swap;src:url('fonts/sourceserif-700.woff2') format('woff2')}\n"
        "@font-face{font-family:'Space Grotesk';font-weight:500;font-style:normal;"
        "font-display:swap;src:url('fonts/spacegrotesk-500.woff2') format('woff2')}\n"
        "@font-face{font-family:'Space Grotesk';font-weight:700;font-style:normal;"
        "font-display:swap;src:url('fonts/spacegrotesk-700.woff2') format('woff2')}\n";
    /* THE ORGANIZATION'S OWN FACES. Every `.woff2` in a `fonts/` folder beside
     * the database is staged into `site/fonts/` and declared here under
     * `theme.font_custom`. The weight is read off the filename
     * (`mandali-700.woff2`), because that is how every foundry already names
     * them and asking a volunteer to write CSS would defeat the point. */
    if (!t.font_custom.empty())
        for (const std::string& f : t.font_files) {
            int weight = 400;
            const size_t dash = f.rfind('-');
            if (dash != std::string::npos && dash + 4 <= f.size() &&
                std::isdigit((unsigned char)f[dash + 1]))
                weight = std::atoi(f.c_str() + dash + 1);
            const bool italic = f.find("italic") != std::string::npos;
            css += "@font-face{font-family:'" + t.font_custom +
                   "';font-weight:" + std::to_string(weight) +
                   ";font-style:" + (italic ? "italic" : "normal") +
                   ";font-display:swap;src:url('fonts/" + f +
                   "') format('woff2')}\n";
        }
    /* ── THE DERIVED CONTRAST TOKENS (2026-08-20) ────────────────────────────
     *
     * Every one of these used to be a literal in the stylesheet, and each was
     * correct for exactly one accent colour. `#fff` on `var(--accent)` reads
     * beautifully with a navy brand and is invisible with a yellow one, and the
     * volunteer who picks the yellow has no way to discover why their buttons
     * went blank — the fix was in a compiled string.
     *
     * So the arithmetic runs here, once, over the theme the organization
     * actually chose: WCAG relative luminance, the ratio, and the floor. The
     * page receives finished tokens; the browser decides nothing. That is the
     * compute boundary, and it is also what makes the GUI's preview, the email
     * and a headless render agree by construction. */
    const double floor_ratio = t.contrast == 2 ? 7.0 : (t.contrast == 1 ? 4.5 : 1.0);
    const std::string on_accent = ink_on(t.accent);
    const std::string on_accent2 = ink_on(t.accent2);
    const std::string on_accent_dark = ink_on(t.accent_dark);
    // muted text is the classic failure: a grey chosen against white, then used
    // on a tinted band. Rescued against the page background at the chosen floor.
    const std::string muted = meet_contrast("#6b6b6b", t.bg, floor_ratio);
    // prose used to be a hardcoded #39352f, which is a dark grey on a dark page
    const std::string prose_ink = meet_contrast(shade(t.ink, 0.12), t.bg, floor_ratio);
    const int gaps[] = {8, 14, 20, 28, 40};
    const int gap = gaps[std::clamp(t.grid_gap, 0, 4)];
    css +=
        ":root{--accent:" + t.accent + ";--accent2:" + t.accent2 +
        ";--accent-lite:" + t.accent_lite + ";--accent-dark:" + t.accent_dark +
        ";--on-accent:" + on_accent + ";--on-accent2:" + on_accent2 +
        ";--on-accent-dark:" + on_accent_dark +
        ";--ink:" + t.ink + ";--muted:" + muted + ";--prose:" + prose_ink +
        ";--line:#e7e3d9;--bg:" + t.bg + ";--card:#fff;"
        "--gap:" + std::to_string(gap) + "px;"
        "--shadow:0 10px 40px -12px rgba(0,0,0,.18);--r:" +
        std::to_string(radii[std::clamp(t.radius, 0, 4)]) + "px;"
        "--headf:" + headf + ";--bodyf:" + bodyf + "}\n";
    css += std::string("html{font-size:") + sc + "}\n"; // the type-scale axis
    // preset overrides (theme presets = the aesthetic-mode axis)
    css +=
        "body.soft{--card:var(--bg);--line:transparent;--r:22px;"
        "--shadow:8px 8px 20px rgba(0,0,0,.10),-8px -8px 20px rgba(255,255,255,.7)}\n"
        "body.bold{--r:4px;--line:var(--ink);"
        "--shadow:8px 8px 0 var(--accent)}\n"
        // EDITORIAL: quiet, print-like — hairline rules, roomy, no card shadow
        "body.editorial{--card:var(--bg);--line:color-mix(in srgb,var(--ink) 16%,transparent);"
        "--shadow:none;--r:2px}\n"
        "body.editorial h1,body.editorial h2.section{font-weight:700;letter-spacing:-.01em}\n"
        "body.editorial .prose{font-size:1.12rem}\n"
        // GLASS: translucent frosted cards over the page/texture
        "body.glass{--card:color-mix(in srgb,var(--bg) 62%,transparent);--r:18px;"
        "--shadow:0 8px 32px -10px rgba(0,0,0,.28)}\n"
        "body.glass .card,body.glass .band.card{backdrop-filter:blur(10px) saturate(1.3);"
        "border:1px solid color-mix(in srgb,var(--ink) 10%,transparent)}\n";
    if (t.dark) // browser-theme reactivity: the visitor's OS setting wins
        /* The dark variant re-derives the SAME tokens against the dark ground.
         * Inheriting `--muted` and `--prose` from the light palette is how a
         * site ends up with near-black body text on a near-black page — which
         * is the same class of bug as white-on-yellow, arriving from the other
         * direction. */
        css +=
            "@media(prefers-color-scheme:dark){:root{--ink:#ededf0;--muted:" +
            meet_contrast("#a6a6ae", "#161619", floor_ratio) +
            ";--prose:" + meet_contrast("#c8c8cf", "#161619", floor_ratio) +
            ";--line:#33343a;--bg:#161619;--card:#1f2026;"
            "--shadow:0 12px 42px -10px rgba(0,0,0,.6)}"
            "body.soft{--card:var(--bg);--line:transparent;"
            "--shadow:8px 8px 20px rgba(0,0,0,.5),-8px -8px 20px rgba(255,255,255,.04)}"
            // the org's dark wordmark, swapped with the palette it was made for
            ".brand-mark.light{display:none}.brand-mark.dark{display:block}}\n";
    // stamp the preset class on <body> from CSS-adjacent JS is fragile; the
    // renderer adds it to the <body> tag directly (see render_site). This var
    // records the choice for that stamp:
    css += std::string("/*preset:") + presetClass + "*/\n";
    /* The computed half (tokens derived from this org's theme) is built
     * above; the static half is a REAL stylesheet at src/render/web/style.css,
     * embedded at build time by tools/embed_asset.py. Nothing is fetched at
     * runtime — the only thing that changed is that an editor can now read it. */
    // the leading newline the old raw literal carried (it opened with one),
    // kept here rather than as a meaningless blank first line in style.css
    return css + '\n' + kSiteCssStatic;
}

/* The theme's JS sprinkle: a gallery lightbox + a live filter box injected
 * above each filterable card group. Vanilla, no framework, no external load
 * (the CSP-clean, deploy-anywhere path). */
std::string site_js() {
    return kSiteJs;
}
