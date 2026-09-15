/* render/email_theme.hpp — what a NEWSLETTER looks like, apart from the website.
 *
 * The author (2026-09-15): *"currently the style and font and such of the
 * newsletter looks like its from the early 2000s ... we should provide some
 * other themes to choose from that can also overlap with colors and stuff.
 * newsletters should have the option to have a seperate theme than the website.
 * there's also different types of 'themes' so like, there's fonts and 'shape
 * style' for things. there's also like the color pallete ... i specifically
 * would like a 'modern' theme that looks like, modern and sleek ... like an
 * apple or nike website made into a newsletter."*
 *
 * THREE AXES AND A PRESET. A PALETTE (the neutrals: page, frame, text, cards,
 * rules), a TYPE (font stacks and heading sizes) and a SHAPE (cards, corners,
 * buttons, section headings, spacing, whether a banner runs edge to edge). A
 * preset names one of each, and any axis can then be changed on its own — which
 * is what "overlap" means: a sleek shape over a warm palette is a real choice,
 * not a sixth preset somebody has to add.
 *
 * ITS OWN CONFIG, `newsletter.*`, because an inbox is not a web page with less
 * CSS. The website's theme stays the website's. The one thing inherited by
 * default is the ACCENT, because that is the organization's colour rather than
 * a decision about a medium; `newsletter.accent` overrides it.
 *
 * WHY THE TOKENS ARE PLAIN STRINGS. Every value ends up inside an inline `style`
 * attribute — the only styling every mail client honours — so a token is
 * exactly the text that gets written. The `classic` values are the literals
 * render/email.cpp carried before this file existed, character for character,
 * and the golden render is the proof: the classic preset did not move a byte.
 *
 * EMAIL-SAFE BY CONSTRUCTION: no web fonts (most clients drop them), no CSS
 * variables, no filters, no flex or grid. Fonts are system stacks that fall
 * back to Helvetica/Arial or Georgia. Rounded corners and background images are
 * progressive: Outlook desktop squares the corners and shows a band's solid
 * colour, which is why every band also carries `bgcolor`.
 */
#pragma once

#include "app/app_internal.hpp" // SiteTheme, ink_on, shade, meet_contrast
#include "render/text.hpp"      // email_button, html_escape

#include <cctype>
#include <cstdio>
#include <initializer_list>
#include <string>

namespace hormiga {
namespace mail {

struct Named {
    const char* name;
    const char* label;
    const char* about;
};

inline constexpr Named kPresets[] = {
    {"classic", "Classic",
     "Serif type, ruled section headings, cards with a colour bar - the look every "
     "issue had before themes existed"},
    {"modern", "Modern",
     "Clean white, the system sans-serif, large tight headings, soft rounded cards, "
     "pill buttons and a full-width banner"},
    {"bold", "Bold",
     "Black and white, heavy uppercase headings, outlined square cards and square "
     "buttons"},
    {"editorial", "Editorial",
     "Warm paper, serif headings over sans-serif text, gently rounded cards"},
    {"night", "Night", "A dark ground with light text and modern type"},
    {"ocean", "Ocean", "Deep navy and sky blue, geometric type, gently rounded cards"},
    {"sunset", "Sunset", "Warm coral and peach, modern type, big friendly rounded shapes"},
    {"forest", "Forest", "Deep greens on warm paper, serif headings, soft cards"},
    {"newsprint", "Newsprint",
     "Off-white paper and serif type, hairline rules instead of boxes - a printed bulletin"},
    {"minimal", "Minimal", "Black type on white, nothing but hairlines and a lot of air"},
};
inline constexpr Named kPalettes[] = {
    {"classic", "Classic", "warm grey page, white frame, dark grey text"},
    {"website", "Website", "the website's own background and text colours"},
    {"clean", "Clean", "near-white page, white frame, near-black text"},
    {"ink", "Ink", "pure white and black"},
    {"sand", "Sand", "warm paper tones"},
    {"night", "Night", "a dark ground and light text"},
    {"ocean", "Ocean", "deep navy text, pale blue page, sky-blue cards"},
    {"sunset", "Sunset", "warm cream page, coral-tinted cards"},
    {"forest", "Forest", "deep green text on warm paper"},
    {"paper", "Paper", "off-white newsprint and near-black ink"},
};
inline constexpr Named kTypes[] = {
    {"classic", "Classic serif", "Georgia throughout"},
    {"modern", "Modern sans", "the system font - San Francisco, Segoe UI, Helvetica"},
    {"editorial", "Editorial", "serif headings over sans-serif text"},
    {"geometric", "Geometric", "Avenir, Century Gothic, Trebuchet"},
    {"impact", "Impact", "heavy uppercase headings"},
};
inline constexpr Named kShapes[] = {
    {"classic", "Classic", "square cards with a colour bar, ruled headings"},
    {"sleek", "Sleek",
     "rounded cards, pill buttons, small spaced-out headings, a full-width banner"},
    {"soft", "Soft", "gently rounded cards and buttons, plain headings"},
    {"sharp", "Sharp", "outlined square cards, block headings, a full-width banner"},
    {"bubbly", "Bubbly", "big round cards and pill buttons, plain headings"},
    {"ruled", "Ruled", "no boxes - hairline rules between items, like a printed page"},
};
inline constexpr int kNumPresets = 10, kNumPalettes = 10, kNumTypes = 5, kNumShapes = 6;

/* What the organization chose. Blank axes come from the preset; a blank preset
 * is `classic`; a blank accent is the website's. */
struct Choice {
    std::string preset, palette, type, shape, accent;
};

inline std::string config_str(maiz::Core& core, const char* key) {
    std::string v = core.dispatch(std::string("config get ") + key).data;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v == "null" ? std::string() : v;
}

inline Choice read_choice(maiz::Core& core) {
    return {config_str(core, "newsletter.theme"), config_str(core, "newsletter.palette"),
            config_str(core, "newsletter.type"), config_str(core, "newsletter.shape"),
            config_str(core, "newsletter.accent")};
}

inline bool is_hex(const std::string& s) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (size_t i = 1; i < 7; ++i)
        if (!std::isxdigit((unsigned char)s[i])) return false;
    return true;
}

/* `a` moved toward `b` by `t` (0 = a, 1 = b). Arithmetic in sRGB, which is what
 * a mail client will composite in anyway; used for tints and on-colour greys. */
inline std::string mix(const std::string& a, const std::string& b, double t) {
    if (!is_hex(a) || !is_hex(b)) return a;
    unsigned ar, ag, ab, br, bg, bb;
    std::sscanf(a.c_str() + 1, "%02x%02x%02x", &ar, &ag, &ab);
    std::sscanf(b.c_str() + 1, "%02x%02x%02x", &br, &bg, &bb);
    auto m = [t](unsigned x, unsigned y) { return (unsigned)(x + (y - x * 1.0) * t + 0.5); };
    char out[8];
    std::snprintf(out, sizeof out, "#%02x%02x%02x", m(ar, br) & 255, m(ag, bg) & 255,
                  m(ab, bb) & 255);
    return out;
}

struct Theme {
    // ── palette (the classic values are the renderer's historical literals) ──
    std::string page = "#f2f0ea", frame = "#ffffff";
    std::string head = "#2c2c2c", ink = "#333", h2 = "#3a3a3a", quote = "#444";
    std::string meta = "#555", sub = "#666", cap = "#777", faint = "#888", empty = "#999";
    std::string rule = "#ddd", card = "#f7f9fc", card_job = "#f6f9f4", job_bar = "#5d7d3b";
    std::string accent = "#3f6fae", accent2 = "#3f6fae";
    std::string btn_bg = "#3f6fae", btn_ink = "#ffffff";
    bool dark = false;
    /* The classic palette never stated a text colour on a cell and left it to
     * the mail client's black. Any other palette (a dark ground, a band) must,
     * or its body text is black on black. */
    bool classic_palette = true;
    std::string text_css() const {
        return classic_palette ? std::string() : "color:" + ink;
    }
    // ── type ────────────────────────────────────────────────────────────────
    std::string font = "Georgia,'Times New Roman',serif";
    std::string head_font;      // "" = the body font
    bool classic_type = true;   // no font-family repeated on cells, no MSO override
    int h1 = 26, sub_px = 16, h2px = 20, h3 = 17;
    std::string h1_css, h3_css; // extra declarations, no trailing ';'
    // ── shape ───────────────────────────────────────────────────────────────
    std::string cards = "bar";   // bar | soft | outline
    std::string h2_mode = "rule"; // rule | plain | eyebrow | block
    int radius = 0, btn_radius = 0, pad = 24, outer = 16, hero_bar = 6;
    bool bleed = false;          // a hero banner runs edge to edge of the frame
    std::string btn_pad = "11px 22px", btn_css;

    // ";a;b" for the non-empty parts — so an empty extra adds nothing at all
    static std::string more(std::initializer_list<std::string> parts) {
        std::string o;
        for (const auto& p : parts)
            if (!p.empty()) o += ";" + p;
        return o;
    }
    static std::string px(int v) { return std::to_string(v) + "px"; }
    std::string hf() const {
        return head_font.empty() ? std::string() : "font-family:" + head_font;
    }
    static std::string corners(int r) {
        return r > 0 ? "border-radius:" + px(r) : std::string();
    }
    // images get the card's corners, capped so a photo never looks like a pill
    std::string img_css() const { return corners(radius > 14 ? 14 : radius); }
    // the width a block may fill inside the frame
    int content_px() const { return 620 - 2 * pad; }

    std::string h1_style() const {
        return "margin:0;font-size:" + px(h1) + ";color:" + head + more({hf(), h1_css});
    }
    std::string sub_style() const {
        return "margin:4px 0 0;color:" + sub + ";font-size:" + px(sub_px);
    }
    std::string h3_style(const char* margin) const {
        return std::string("margin:") + margin + ";font-weight:bold;font-size:" + px(h3) +
               ";color:" + head + more({hf(), h3_css});
    }
    std::string h2_html(const std::string& title_escaped) const {
        std::string s;
        if (h2_mode == "eyebrow")
            s = "margin:40px 0 14px;font-size:13px;font-weight:600;letter-spacing:.14em;"
                "text-transform:uppercase;color:" + meta;
        else if (h2_mode == "block")
            s = "margin:36px 0 12px;padding:0 0 8px;border-bottom:4px solid " + head +
                ";font-size:" + px(h2px) +
                ";font-weight:900;text-transform:uppercase;letter-spacing:-.01em;color:" +
                head;
        else if (h2_mode == "hairline")
            s = "margin:36px 0 10px;padding:0 0 8px;border-bottom:1px solid " + rule +
                ";font-size:" + px(h2px) + ";color:" + h2;
        else if (h2_mode == "plain")
            s = "margin:30px 0 8px;font-size:" + px(h2px) + ";color:" + h2;
        else
            s = "border-bottom:2px solid " + accent + ";padding-bottom:4px;margin:26px 0 6px;"
                "font-size:" + px(h2px) + ";color:" + h2;
        return "<h2 style=\"" + s + more({hf()}) + "\">" + title_escaped + "</h2>\n";
    }
    /* A card: an event, a posting, a person. `bar` is the stripe colour; a soft
     * card shows it only when the thing has a colour of its OWN (an event's
     * `color`), because a rounded card with the accent down its side is the
     * 2000s look this axis exists to leave. */
    std::string card_open(const std::string& bg, const std::string& bar, const char* margin,
                          const char* padding, bool own_colour = false) const {
        const std::string open =
            "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" ";
        if (cards == "soft")
            return open + "bgcolor=\"" + bg + "\" style=\"margin:" + margin + ";background:" +
                   bg +
                   more({corners(radius), own_colour ? "border-left:4px solid " + bar
                                                     : std::string()}) +
                   "\"><tr><td style=\"padding:16px 20px" + more({text_css()}) + "\">";
        if (cards == "line") // the ruled shape: no box, a hairline above each item
            return open + "style=\"margin:" + margin + ";border-top:1px solid " + rule +
                   (own_colour ? ";border-left:3px solid " + bar : std::string()) +
                   "\"><tr><td style=\"padding:12px " + (own_colour ? "12px" : "0") +
                   more({text_css()}) + "\">";
        if (cards == "outline")
            return open + "style=\"margin:" + margin + ";background:" + frame +
                   ";border:2px solid " + head +
                   more({own_colour ? "border-left:8px solid " + bar : std::string()}) +
                   "\"><tr><td style=\"padding:14px 16px" + more({text_css()}) + "\">";
        return open + "style=\"margin:" + margin + ";background:" + bg +
               ";border-left:4px solid " + bar + "\"><tr><td style=\"padding:" + padding +
               more({text_css()}) + "\">";
    }
    std::string b_open() const {
        return cards == "bar" ? std::string("<b>")
                              : "<b style=\"font-size:17px;color:" + head + more({hf()}) + "\">";
    }
    std::string feature_b_open() const {
        return cards == "bar" ? std::string("<b style=\"font-size:19px\">")
                              : "<b style=\"font-size:24px;color:" + head +
                                    more({hf(), h3_css}) + "\">";
    }
    std::string button(const std::string& href, const std::string& label) const {
        if (btn_radius == 0 && btn_pad == "11px 22px" && btn_css.empty() &&
            btn_ink == "#ffffff" && btn_bg == accent && classic_type)
            return email_button(href, label, accent); // the classic button, unchanged
        return "<table role=\"presentation\" cellpadding=\"0\" cellspacing=\"0\" "
               "style=\"margin:14px 0\"><tr><td align=\"center\" bgcolor=\"" + btn_bg +
               "\" style=\"background:" + btn_bg + ";padding:" + btn_pad +
               more({corners(btn_radius)}) + "\"><a href=\"" + href + "\" style=\"color:" +
               btn_ink +
               ";text-decoration:none;font-weight:bold;font-size:15px;display:inline-block" +
               more({classic_type ? std::string() : "font-family:" + font, btn_css}) + "\">" +
               html_escape(label) + "</a></td></tr></table>\n";
    }
};

inline void dark_neutrals(Theme& t, const std::string& ground) {
    t.page = "#0b0b0c";
    t.frame = ground;
    t.head = "#f5f5f7"; t.ink = "#d1d1d6"; t.h2 = "#f5f5f7"; t.quote = "#e5e5ea";
    t.meta = "#a1a1a6"; t.sub = "#a1a1a6"; t.cap = "#8e8e93"; t.faint = "#8e8e93";
    t.empty = "#636366"; t.rule = "#3a3a3c"; t.card = "#242428"; t.card_job = "#242428";
    t.dark = true;
}

// every text token re-derived to read ON a strong colour (an accent band)
inline void on_colour_neutrals(Theme& t, const std::string& bg) {
    const std::string on = ink_on(bg);
    t.head = t.ink = t.h2 = t.quote = on;
    t.meta = t.sub = t.cap = t.faint = t.empty = mix(on, bg, 0.22);
    t.rule = mix(on, bg, 0.55);
    t.card = t.card_job = mix(bg, on, 0.12);
    t.frame = bg;
    t.accent = t.job_bar = t.btn_bg = on;
    t.btn_ink = bg;
}

inline void apply_palette(Theme& t, const std::string& name, const SiteTheme& site) {
    if (name != "classic" && !name.empty()) t.classic_palette = false;
    if (name == "clean") {
        t.page = "#f5f5f7"; t.frame = "#ffffff"; t.head = "#1d1d1f"; t.ink = "#424245";
        t.h2 = "#1d1d1f"; t.quote = "#1d1d1f"; t.meta = "#6e6e73"; t.sub = "#6e6e73";
        t.cap = "#86868b"; t.faint = "#86868b"; t.empty = "#aeaeb2"; t.rule = "#e5e5ea";
        t.card = "#f5f5f7"; t.card_job = "#f5f5f7";
    } else if (name == "ink") {
        t.page = "#ffffff"; t.frame = "#ffffff"; t.head = "#111111"; t.ink = "#222222";
        t.h2 = "#111111"; t.quote = "#111111"; t.meta = "#555555"; t.sub = "#555555";
        t.cap = "#757575"; t.faint = "#757575"; t.empty = "#9e9e9e"; t.rule = "#111111";
        t.card = "#f5f5f5"; t.card_job = "#f5f5f5";
    } else if (name == "sand") {
        t.page = "#ede6da"; t.frame = "#fbf8f2"; t.head = "#2b2620"; t.ink = "#3d362d";
        t.h2 = "#2b2620"; t.quote = "#4a4238"; t.meta = "#6b6153"; t.sub = "#6b6153";
        t.cap = "#857a6b"; t.faint = "#857a6b"; t.empty = "#a39785"; t.rule = "#e0d8ca";
        t.card = "#f2ebdf"; t.card_job = "#f2ebdf";
    } else if (name == "ocean") {
        t.page = "#e8f1f8"; t.frame = "#ffffff"; t.head = "#0b2545"; t.ink = "#243b53";
        t.h2 = "#0b2545"; t.quote = "#13315c"; t.meta = "#486581"; t.sub = "#486581";
        t.cap = "#627d98"; t.faint = "#627d98"; t.empty = "#9fb3c8"; t.rule = "#d9e6f2";
        t.card = "#eef5fb"; t.card_job = "#eef5fb";
    } else if (name == "sunset") {
        t.page = "#fdf1e8"; t.frame = "#fffaf6"; t.head = "#3d1f1a"; t.ink = "#4f2f28";
        t.h2 = "#3d1f1a"; t.quote = "#5c3228"; t.meta = "#8a5a4e"; t.sub = "#8a5a4e";
        t.cap = "#a0705f"; t.faint = "#a0705f"; t.empty = "#c9a394"; t.rule = "#f3dccf";
        t.card = "#fde6d8"; t.card_job = "#fde6d8";
    } else if (name == "forest") {
        t.page = "#eef1e8"; t.frame = "#fbfaf5"; t.head = "#1f3a2b"; t.ink = "#34473a";
        t.h2 = "#1f3a2b"; t.quote = "#2b4a37"; t.meta = "#5b6f5f"; t.sub = "#5b6f5f";
        t.cap = "#728676"; t.faint = "#728676"; t.empty = "#a3b3a5"; t.rule = "#dde4d6";
        t.card = "#f0f3ea"; t.card_job = "#f0f3ea";
    } else if (name == "paper") {
        t.page = "#e9e5dc"; t.frame = "#faf8f2"; t.head = "#111111"; t.ink = "#2a2a2a";
        t.h2 = "#111111"; t.quote = "#222222"; t.meta = "#5a5a5a"; t.sub = "#5a5a5a";
        t.cap = "#6f6f6f"; t.faint = "#6f6f6f"; t.empty = "#9a9a9a"; t.rule = "#cfc9bc";
        t.card = "#f3f0e8"; t.card_job = "#f3f0e8";
    } else if (name == "night") {
        dark_neutrals(t, "#161618");
    } else if (name == "website") {
        if (is_hex(site.bg)) t.page = site.bg;
        if (is_hex(site.ink)) {
            t.head = t.h2 = meet_contrast(site.ink, t.frame, 4.5);
            t.ink = meet_contrast(mix(site.ink, t.frame, 0.12), t.frame, 4.5);
        }
    }
    // "classic": the defaults in the struct
}

inline void apply_type(Theme& t, const std::string& name) {
    static const char* kSans = "-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,"
                               "'Helvetica Neue',Helvetica,Arial,sans-serif";
    if (name == "classic" || name.empty()) return;
    t.classic_type = false;
    if (name == "modern") {
        t.font = kSans; t.h1 = 40; t.sub_px = 19; t.h2px = 24; t.h3 = 20;
        t.h1_css = "font-weight:700;letter-spacing:-0.025em;line-height:1.08";
        t.h3_css = "font-weight:600;letter-spacing:-0.01em";
    } else if (name == "editorial") {
        t.font = "'Helvetica Neue',Helvetica,Arial,sans-serif";
        t.head_font = "Georgia,'Times New Roman',serif";
        t.h1 = 36; t.sub_px = 18; t.h2px = 24; t.h3 = 20;
        t.h1_css = "font-weight:normal;line-height:1.12";
        t.h3_css = "font-weight:normal";
    } else if (name == "geometric") {
        t.font = "'Avenir Next',Avenir,'Century Gothic','Trebuchet MS',sans-serif";
        t.h1 = 34; t.sub_px = 18; t.h2px = 20; t.h3 = 18;
        t.h1_css = "font-weight:600;letter-spacing:-0.01em;line-height:1.1";
        t.h3_css = "font-weight:600";
    } else if (name == "impact") {
        t.font = "'Helvetica Neue',Helvetica,Arial,sans-serif";
        t.head_font = "'Arial Black','Helvetica Neue',Helvetica,Arial,sans-serif";
        t.h1 = 44; t.sub_px = 18; t.h2px = 26; t.h3 = 18;
        t.h1_css = "font-weight:900;text-transform:uppercase;letter-spacing:-0.01em;"
                   "line-height:1";
        t.h3_css = "font-weight:900;text-transform:uppercase";
    }
}

inline void apply_shape(Theme& t, const std::string& name) {
    if (name == "sleek") {
        t.cards = "soft"; t.h2_mode = "eyebrow"; t.radius = 18; t.btn_radius = 980;
        t.pad = 40; t.outer = 24; t.hero_bar = 0; t.bleed = true; t.btn_pad = "13px 28px";
    } else if (name == "soft") {
        t.cards = "soft"; t.h2_mode = "plain"; t.radius = 12; t.btn_radius = 10;
        t.pad = 30; t.outer = 20; t.hero_bar = 0; t.btn_pad = "12px 24px";
    } else if (name == "sharp") {
        t.cards = "outline"; t.h2_mode = "block"; t.pad = 32; t.outer = 0;
        t.hero_bar = 0; t.bleed = true; t.btn_pad = "15px 32px";
        t.btn_css = "text-transform:uppercase;letter-spacing:.08em";
    } else if (name == "bubbly") {
        t.cards = "soft"; t.h2_mode = "plain"; t.radius = 24; t.btn_radius = 980;
        t.pad = 34; t.outer = 20; t.hero_bar = 0; t.btn_pad = "14px 30px";
    } else if (name == "ruled") {
        t.cards = "line"; t.h2_mode = "hairline"; t.pad = 44; t.outer = 24;
        t.hero_bar = 0; t.btn_pad = "12px 26px";
    }
}

inline bool known(const std::string& v, const Named* list, int n) {
    for (int i = 0; i < n; ++i)
        if (v == list[i].name) return true;
    return false;
}

inline Theme make(const Choice& c, const SiteTheme& site) {
    struct P {
        const char* preset;
        const char* palette;
        const char* type;
        const char* shape;
    };
    static const P kMap[] = {{"classic", "classic", "classic", "classic"},
                             {"modern", "clean", "modern", "sleek"},
                             {"bold", "ink", "impact", "sharp"},
                             {"editorial", "sand", "editorial", "soft"},
                             {"night", "night", "modern", "sleek"},
                             {"ocean", "ocean", "geometric", "soft"},
                             {"sunset", "sunset", "modern", "bubbly"},
                             {"forest", "forest", "editorial", "soft"},
                             {"newsprint", "paper", "classic", "ruled"},
                             {"minimal", "ink", "modern", "ruled"}};
    const P* p = &kMap[0];
    for (const auto& m : kMap)
        if (c.preset == m.preset) p = &m;
    const std::string pal = known(c.palette, kPalettes, kNumPalettes) ? c.palette : p->palette;
    const std::string typ = known(c.type, kTypes, kNumTypes) ? c.type : p->type;
    const std::string shp = known(c.shape, kShapes, kNumShapes) ? c.shape : p->shape;

    Theme t;
    apply_palette(t, pal, site);
    apply_type(t, typ);
    apply_shape(t, shp);
    t.accent = is_hex(c.accent) ? c.accent
                                : (site.accent.empty() ? std::string("#3f6fae") : site.accent);
    t.accent2 = (is_hex(site.accent2) && site.accent2 != site.accent && !is_hex(c.accent))
                    ? site.accent2
                    : shade(t.accent, -0.35);
    if (pal != "classic") t.job_bar = t.accent;
    t.btn_bg = shp == "sharp" ? t.head : t.accent;
    t.btn_ink = (pal == "classic" && shp == "classic") ? std::string("#ffffff") : ink_on(t.btn_bg);
    return t;
}

/* ── A BAND, IN AN INBOX (2026-09-15) ────────────────────────────────────────
 *
 * The author: *"the band, tint, card, and that stuff doesn't translate into the
 * newsletter ... applying that difficult css or javascript isn't possible for a
 * newsletter, but we can get creative on how to do it instead (kinda like
 * holidays themselves, we match some protocol into a different domain)."*
 *
 * That is the right frame, so this is a lens and not a port. On the website a
 * band is a `<section>` with a class and CSS decides the rest. Here it is the
 * one construct every mail client agrees on — a table cell with `bgcolor` — and
 * the band's MEANING is carried by swapping the theme the blocks inside it are
 * rendered with: an accent band re-derives every text colour to read on the
 * accent, a dark band renders its blocks with the night neutrals, a tint lifts
 * its cards to the frame colour so they still stand off the tint.
 *
 *   tint      a 10% wash of the accent        cards become the frame colour
 *   card      the palette's card colour       cards become the frame colour
 *   accent    the accent itself               every text colour reads on it
 *   dark      near-black                      the night neutrals
 *   gradient  accent -> accent2               Outlook shows the accent alone
 *   image     the photo, over near-black      white text; Outlook: the black
 *
 * Full-bleed (`band_full`) closes the frame's padded cell and gives the band a
 * row of its own, so it meets the frame's edges the way a web band meets the
 * window's. A background image needs a PUBLIC address like every other image in
 * a newsletter; the caller resolves it.
 */
struct Band {
    bool on = false;
    std::string css, attrs; // the cell's background declarations and attributes
    Theme t;                // the theme the band's blocks render with
};

inline Band band(const Theme& base, const std::string& kind, const std::string& image_src) {
    Band b;
    b.t = base;
    b.t.classic_palette = false; // a band always states its text colour
    if (!image_src.empty()) {
        b.on = true;
        dark_neutrals(b.t, "#1d1d1f");
        b.t.accent = b.t.job_bar = base.accent;
        b.attrs = " bgcolor=\"#1d1d1f\" background=\"" + image_src + "\"";
        b.css = "background:#1d1d1f url('" + image_src + "') center / cover no-repeat";
        return b;
    }
    std::string bg;
    if (kind == "tint") {
        bg = mix(base.accent, base.frame, 0.9);
        b.t.card = b.t.card_job = base.frame;
        b.t.frame = bg;
    } else if (kind == "card") {
        bg = base.card;
        b.t.card = b.t.card_job = base.frame;
        b.t.frame = bg;
    } else if (kind == "dark") {
        bg = base.dark ? "#000000" : "#161618";
        dark_neutrals(b.t, bg);
        b.t.accent = b.t.job_bar = base.accent;
    } else if (kind == "accent" || kind == "gradient") {
        bg = base.accent;
        on_colour_neutrals(b.t, bg);
    } else {
        return b; // none, blank, or a value this medium has no reading for
    }
    b.on = true;
    b.attrs = " bgcolor=\"" + bg + "\"";
    b.css = "background:" + bg;
    if (kind == "gradient")
        b.css += ";background-image:linear-gradient(135deg," + base.accent + " 0%," +
                 base.accent2 + " 100%)";
    return b;
}

} // namespace mail
} // namespace hormiga
