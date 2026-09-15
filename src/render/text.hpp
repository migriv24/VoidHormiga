/* render/text.hpp — the pure helpers both output domains share.
 *
 * These were file-level `static` functions inside one 3,100-line translation
 * unit, which is why the newsletter and the website could each have its own
 * idea of how to print a title: sharing was a property of being in the same
 * file rather than of depending on anything. Splitting the two renderers apart
 * is what forced the question, and the answer is that a date is formatted one
 * way, a URL is linkified one way, and a rune's display name is chosen one way
 * — for every output, by construction.
 *
 * All pure: string in, string out, no `HormigaApp`, no ImGui, no I/O. That is
 * what makes them testable on their own and what keeps the Output domain
 * view-free (measured, and now enforceable by tools/check_layering.py).
 *
 * Header-only and `inline` rather than a .cpp, because every one of these is a
 * few lines and the call sites are hot — `human_date` runs per event card, per
 * render, per language.
 */
#pragma once

#include "app/app_internal.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "domain/clock.hpp"      // parse_clock — the one wall-clock parser
#include "domain/scene_value.hpp" // tag_value, field_value


/* ── email-domain helpers (2026-08-19) ───────────────────────────────────────
 *
 * All four exist because an agent shipped a real newsletter and a person read
 * it. The report is `MESSAGE-TO-HORMIGA.md`; the sentence that drove this work
 * is that the emailed issue "contains no clickable link of any kind" — for a
 * newsletter whose entire job is *click this to RSVP*.
 */

/* ── RELATED RUNES, AT THE RENDER SEAM (2026-08-20) ──────────────────────────
 *
 * The renderer ignored `Scene::wires` entirely. A real database had **22
 * `flyer-of` edges** and 2 `translation-of` ones — written by an earlier run,
 * correct, and read by nothing — so the site showed a gallery of fliers in one
 * place and a list of events in another with the association sitting unread in
 * the model. The operator noticed before we did.
 *
 * Deliberately GENERAL rather than a flier special case. The moment a block can
 * ask "what is related to this rune, by this name", the flier grid, the
 * translation pair and whatever the next relation turns out to be are all the
 * same query. A `flier_of()` helper would have had to be joined by
 * `translation_of()` a week later.
 *
 * BOTH DIRECTIONS, because which way an edge points is an authoring accident a
 * reader should not pay for: somebody wired `flier -flyer-of-> event`, somebody
 * else would have wired the reverse, and both mean the same thing to a page.
 *
 * BOTH SPELLINGS, because the same database contains `flyer-of` AND `flier-for`
 * — one written by an import, one typed by hand. Matching a set of names rather
 * than one is the difference between a feature that works on real data and one
 * that works on the fixture. */
inline std::vector<const maiz::SceneNode*> related_runes(
    const maiz::Scene& scene, const std::string& name,
    const std::vector<std::string>& relations) {
    std::vector<const maiz::SceneNode*> out;
    auto wanted = [&](const std::string& r) {
        for (const auto& x : relations)
            if (r == x) return true;
        return relations.empty();
    };
    for (const auto& w : scene.wires) {
        std::string other;
        if (w.from == name && wanted(w.relation)) other = w.to;
        else if (w.to == name && wanted(w.relation)) other = w.from;
        if (other.empty()) continue;
        if (const maiz::SceneNode* n = scene.find(other)) {
            bool dup = false;
            for (const auto* o : out) dup = dup || o->name == n->name;
            if (!dup) out.push_back(n);
        }
    }
    return out;
}

/* The names a flier–event association goes by in real data. */
inline const std::vector<std::string>& kFlierRelations() {
    static const std::vector<std::string> r = {"flyer-of", "flier-of",
                                               "flier-for", "flyer-for"};
    return r;
}

/* ── DOES THIS RUNE BELONG ON A PAGE IN THIS LANGUAGE? ───────────────────────
 *
 * One rule, because there were two and they disagreed.
 *
 * A rune with NO `lang:` tag is language-neutral: a photograph of a dinner is
 * not in a language, and it shows on both pages. A rune tagged `lang:es` shows
 * on the Spanish page. And — the case that was broken — a rune carrying
 * SEVERAL `lang:` tags belongs on every page it names.
 *
 * That last one cost a real site its most useful sheet. A bilingual flier
 * tagged `lang:en`, `lang:es` and `lang:bilingual` was matched by reading the
 * FIRST tag only, so it answered "en" and the Spanish page dropped it — the one
 * flier that told people where to take the backpacks, missing from the half of
 * the site whose readers most needed it.
 *
 * `bilingual` / `both` / `all` are also honoured as explicit ways to say
 * neutral, because somebody will reasonably tag a sheet that way instead of
 * listing the languages. */
inline bool lang_matches(const maiz::SceneNode& n, std::string_view lang) {
    const std::vector<std::string> ls = hormiga::tag_values(n, "lang");
    if (ls.empty()) return true; // untagged = belongs everywhere
    for (const auto& l : ls) {
        if (l == lang) return true;
        if (l == "bilingual" || l == "both" || l == "all") return true;
    }
    return false;
}

/* ── SOME VALUES HAVE NO TRANSLATION, AND COUNTING THEM IS A LIE ─────────────
 *
 * The portfolio agent, after translating 96% of a site by hand:
 *
 *   > The last two fields standing between us and 100% are
 *   > `label_es "migriv24@gmail.com"` and `label_es "541-913-5781"`. An email
 *   > address has no Spanish. The report's own header warns that writing the
 *   > source value into the `_es` field is "worse than the fallback", so the
 *   > correct move is to leave them — which means **the warning can never go
 *   > away**, and a warning that cannot be cleared is a warning people stop
 *   > reading.
 *
 * That last clause is the real damage. This project has spent a lot of comments
 * on warnings being worth reading, and a permanent one at the bottom of every
 * render undoes that for all of them.
 *
 * Their first suggestion — "not counting a field whose value contains no
 * letters" — does not survive `migriv24@gmail.com`, which is nearly all
 * letters. What actually distinguishes these values is that they are
 * IDENTIFIERS rather than prose: an address, a phone number, a URL, a handle.
 * They are typed the same in every language because they are not in a language.
 *
 * Three shapes, deliberately narrow. Anything that is prose — even one word, even
 * a proper noun somebody may well want to localise — keeps counting, because
 * the failure to avoid is silently excusing text that SHOULD have been
 * translated. An operator with a fourth shape has `lang:none` below.
 */
inline bool untranslatable(const std::string& v) {
    if (v.empty()) return true;
    // a URL: nothing about it is language
    if (v.rfind("http://", 0) == 0 || v.rfind("https://", 0) == 0 ||
        v.rfind("www.", 0) == 0 || v.rfind("mailto:", 0) == 0 ||
        v.rfind("tel:", 0) == 0)
        return true;
    // an email address: exactly one @, no spaces, a dot after it
    const size_t at = v.find('@');
    if (at != std::string::npos && at > 0 && v.find('@', at + 1) == std::string::npos &&
        v.find(' ') == std::string::npos && v.find('.', at) != std::string::npos)
        return true;
    // a number-shaped value: a phone, a year, a price, an extension. Digits and
    // punctuation only -- one letter anywhere makes it prose again.
    bool has_digit = false, only_number_chars = true;
    for (unsigned char c : v) {
        if (std::isdigit(c)) has_digit = true;
        else if (!std::strchr(" +-().,/x#", c)) only_number_chars = false;
    }
    return has_digit && only_number_chars;
}

/* ── A PER-LANGUAGE FIELD ON A DATA RUNE (2026-09-03) ────────────────────────
 *
 * `render_site`'s own `text()` lambda does this for the DOCUMENT — `title_en`
 * falling back to `title_es` — and it stopped there, so the blocks that publish
 * an organization's own CONTENT could not be bilingual at all. The portfolio
 * report is exact about the cost:
 *
 *   > Every one of Miguel's five project descriptions — the longest prose on
 *   > the site, the part a reader actually reads — prints in English on the
 *   > Spanish page, and `translation-report` says the site is 96% done.
 *
 * `organization` declared `bio` and no `bio_es`; `image` declared `description`
 * and `alt` and no `_es` for either. So the one text on the site that could not
 * be translated was the text a visitor came to read.
 *
 * Their framing is the one that decides it, and it is the author's own
 * commitment applied one layer down: *"Given the position you took declining
 * `site.languages` — that Spanish is not a translation of the site, for many
 * readers it IS the site — this is the same commitment applied one layer
 * down."*
 *
 * The rule the guide already states resolves the shape: per-language THINGS get
 * sibling runes and a `lang:` tag, per-language STRINGS on one thing get the
 * `_en`/`_es` suffix. A project description is a string on one thing.
 *
 * THE LEGACY FIELD IS STILL READ, last. Every database written before today has
 * `bio` and no `bio_en`, and a bilingual upgrade that blanked existing prose
 * would be a worse bug than the one it fixes. Same shape as `render_site`'s
 * `text_or`, which does this for `summary`/`title` on events. */
inline std::string lang_text(const maiz::SceneNode& n, const char* base,
                             std::string_view lang, const char* legacy = nullptr) {
    const std::string b(base);
    std::string v = field_value(n, b + "_" + std::string(lang));
    if (!v.empty()) return v;
    for (const std::string& other : hormiga::site_langs()) {
        if (other == lang) continue;
        v = field_value(n, b + "_" + other);
        if (!v.empty()) return v;
    }
    return legacy ? field_value(n, legacy) : field_value(n, base);
}

/* Pick the member of `runes` whose `lang:` tag matches the page, falling back
 * to one with no language tag, then to the first. A flier exists in two
 * languages as two runes (per the project rule: per-language THINGS get sibling
 * runes and a `lang:` tag), so a bilingual page showing the English scan on the
 * Spanish side is the failure this prevents. */
inline const maiz::SceneNode* pick_for_lang(
    const std::vector<const maiz::SceneNode*>& runes, std::string_view lang) {
    const maiz::SceneNode* neutral = nullptr;
    for (const auto* n : runes) {
        const std::vector<std::string> ls = hormiga::tag_values(*n, "lang");
        for (const auto& l : ls)
            if (l == lang) return n; // an exact match wins outright
        if (ls.empty() && !neutral) neutral = n;
    }
    // then anything that says it serves both, then anything at all
    if (!neutral)
        for (const auto* n : runes)
            if (lang_matches(*n, lang)) { neutral = n; break; }
    return neutral ? neutral : (runes.empty() ? nullptr : runes.front());
}

/* The name of a language code, written in the page's own language.
 *
 * A table rather than a lookup, because the set is `site_langs()` and a third
 * entry is a row here plus a row there — the same shape `ui()` keeps in the
 * renderer, and for the same reason: `tools/lint_i18n.py` can see a table.
 * An unknown code returns itself, which is honest and never blank. */
inline std::string lang_name(std::string_view code, std::string_view in_lang) {
    const bool es = in_lang == "es";
    if (code == "en") return es ? "ingles" : "English";
    if (code == "es") return es ? "espanol" : "Spanish";
    return std::string(code);
}

/* "Showing 1 of 5 - the other 4 are only in English."
 *
 * ── the sentence the field report asked for (2026-08-28) ────────────────────
 *
 * The Archive page gave an English reader five tiles and a Spanish reader one,
 * silently. Three answers were available and only one of them is honest: filter
 * and say nothing (what shipped), do not filter (hand a Spanish speaker four
 * sheets they cannot read), or filter and say so. This is the third.
 *
 * NAMING the language is what makes the line worth printing. A bilingual reader
 * — which, in an outreach organization, is most of the staff and much of the
 * membership — can act on "only in English" and can do nothing at all with
 * "some items are not available". And the plural set is honoured: if the hidden
 * sheets are not all in one language the line says so rather than picking the
 * first and asserting it, which is the same first-tag-only mistake that cost
 * `lang_matches` its bilingual fliers.
 *
 * Assembled by concatenation rather than by one format string per language.
 * The two languages put the count, the noun and the language name in different
 * ORDERS, so a shared `printf` template would need a different argument list
 * for each — which is a mismatched-specifier crash waiting for whoever adds
 * the third language, in a function whose entire job is to be safe to add a
 * third language to.
 *
 * `shown` may legitimately be 0 — an all-English section on the Spanish page —
 * and the sentence still reads correctly, which is the case that matters most:
 * that is the page where the silence was total. */
inline std::string lang_shortfall(int shown, int total,
                                  const std::vector<std::string>& hidden_langs,
                                  std::string_view lang) {
    const int rest = total - shown;
    if (rest <= 0) return {};
    const bool es = lang == "es";
    const std::string n_shown = std::to_string(shown);
    const std::string n_total = std::to_string(total);
    const std::string n_rest = std::to_string(rest);

    /* One language for all of them, or several? `hidden_langs` holds every
     * `lang:` value carried by every hidden rune, so an untagged rune
     * contributes nothing — correctly, since an untagged rune is neutral and
     * was never hidden by this filter in the first place. */
    std::string only;
    bool uniform = !hidden_langs.empty();
    for (const auto& l : hidden_langs) {
        if (only.empty()) only = l;
        else if (only != l) { uniform = false; break; }
    }
    const std::string where =
        uniform ? lang_name(only, lang)
                : (es ? std::string("otros idiomas") : std::string("other languages"));

    if (es)
        return "Mostrando " + n_shown + " de " + n_total + " - " +
               (rest == 1 ? "el otro solo esta" : "los otros " + n_rest + " solo estan") +
               " en " + where + ".";
    return "Showing " + n_shown + " of " + n_total + " - the other " + n_rest +
           (rest == 1 ? " is" : " are") + " only in " + where + ".";
}

/* Turn bare URLs and email addresses in ALREADY-ESCAPED text into anchors.
 *
 * Runs after `html_escape`, never before: escaping first means a `<` in the
 * prose can never become markup, and the only thing this adds is anchors around
 * runs that are already inert. An `&` inside a URL has become `&amp;` by now,
 * which is what an `href` attribute wants anyway.
 *
 * WHY THE RENDERER AND NOT THE AUTHOR. A person writing "complete the survey at
 * https://…" in a summary field has written a link; asking them to also wrap it
 * in a `link` block is asking them to know about our block vocabulary in order
 * to get behaviour every mail client already gives them.
 * WHY THE STYLE IS A PARAMETER (2026-09-02). An email has no stylesheet, so a
 * linkified URL there must carry `style="color:inherit"` inline or the client
 * paints it `#0000EE`. A web page HAS one, and that same inline style would
 * override it — so bringing linkification to the website (field report D4) with
 * the email's attribute attached would have made every bare URL in prose look
 * exactly like the body text around it. Two domains, one transform, one
 * difference, stated once here rather than as two copies of this function. */
inline std::string linkify(const std::string& escaped,
                           const char* style = " style=\"color:inherit\"") {
    static const std::string kStop = " \t\n<>\"')";
    std::string out;
    out.reserve(escaped.size() + 32);
    for (size_t i = 0; i < escaped.size();) {
        size_t h = escaped.compare(i, 8, "https://") == 0   ? i
                   : escaped.compare(i, 7, "http://") == 0  ? i
                                                            : std::string::npos;
        if (h == i) {
            size_t e = escaped.find_first_of(kStop, i);
            if (e == std::string::npos) e = escaped.size();
            // Trailing punctuation belongs to the sentence, not the URL.
            while (e > i && std::strchr(".,;:!?", escaped[e - 1])) --e;
            const std::string url = escaped.substr(i, e - i);
            out += "<a href=\"" + url + "\"" + style + ">" + url + "</a>";
            i = e;
            continue;
        }
        if (escaped[i] == '@' && !out.empty()) {
            // Walk back over the local part we already emitted, and forward
            // over the domain. Only rewrite when both halves look real.
            size_t s = out.size();
            while (s > 0 && (std::isalnum((unsigned char)out[s - 1]) ||
                             std::strchr("._%+-", out[s - 1])))
                --s;
            size_t e = i + 1;
            while (e < escaped.size() && (std::isalnum((unsigned char)escaped[e]) ||
                                          std::strchr(".-", escaped[e])))
                ++e;
            while (e > i + 1 && escaped[e - 1] == '.') --e;
            const std::string local = out.substr(s);
            const std::string domain = escaped.substr(i + 1, e - i - 1);
            if (!local.empty() && domain.find('.') != std::string::npos) {
                const std::string addr = local + "@" + domain;
                out.erase(s);
                out += "<a href=\"mailto:" + addr + "\"" + style + ">" +
                       addr + "</a>";
                i = e;
                continue;
            }
        }
        out += escaped[i++];
    }
    return out;
}

/* Escaped, then linkified — the pairing every prose field wants, named so a
 * future block cannot accidentally use only half of it.
 *
 * `web_prose` is the same pairing without the inline colour: a web page has a
 * stylesheet and `.prose a` in it, and an inline `color:inherit` would win over
 * that and make every linkified URL look like the paragraph it sits in. */
inline std::string prose(const std::string& raw) {
    return linkify(html_escape(raw));
}

inline std::string web_prose(const std::string& raw) {
    return linkify(html_escape(raw), "");
}

/* ── LISTS IN PROSE (2026-09-15) ─────────────────────────────────────────────
 *
 * The author: *"lists need to be a supported feature for narrative sections."*
 * The shape people already type: a line starting `- `, `* ` or `• ` is a
 * bullet, and `1. ` or `1) ` is a numbered item. Nothing else is markup - no
 * asterisks for bold, no pound signs for headings - because a volunteer pasting
 * from a word processor must not trip a syntax they never meant, and those two
 * line shapes are ones nobody types at the start of a line by accident.
 *
 * TEXT WITH NO LIST RENDERS EXACTLY AS IT DID, byte for byte, in both domains
 * (the golden render holds both callers to it), so every existing narrative is
 * untouched and a list is purely additive. A blank line ends a list; ordinary
 * text after one starts a new paragraph. Pure, like the rest of this header.
 */
struct ProseChunk {
    int kind = 0; // 0 paragraph text, 1 bullets, 2 numbered
    std::vector<std::string> lines;
};

// 0 = not a list line; 1 = a bullet; 2 = a numbered item. `item` = its text.
inline int prose_list_item(const std::string& line, std::string& item) {
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    auto gap = [&](size_t k) {
        return k < line.size() && (line[k] == ' ' || line[k] == '\t');
    };
    auto rest = [&](size_t k) {
        while (gap(k)) ++k;
        item = k < line.size() ? line.substr(k) : std::string();
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
        return !item.empty();
    };
    if (i < line.size() && (line[i] == '-' || line[i] == '*') && gap(i + 1))
        return rest(i + 2) ? 1 : 0;
    if (line.compare(i, 3, "\xe2\x80\xa2") == 0 && gap(i + 3)) return rest(i + 4) ? 1 : 0;
    size_t d = i;
    while (d < line.size() && d - i < 3 && std::isdigit((unsigned char)line[d])) ++d;
    if (d > i && d < line.size() && (line[d] == '.' || line[d] == ')') && gap(d + 1))
        return rest(d + 2) ? 2 : 0;
    return 0;
}

inline std::vector<ProseChunk> prose_chunks(const std::string& raw) {
    std::vector<ProseChunk> out;
    size_t start = 0;
    for (;;) {
        const size_t nl = raw.find('\n', start);
        std::string line =
            raw.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string item;
        if (const int k = prose_list_item(line, item)) {
            if (out.empty() || out.back().kind != k) out.push_back({k, {}});
            out.back().lines.push_back(item);
        } else if (!out.empty() && out.back().kind == 0) {
            out.back().lines.push_back(line);
        } else if (line.find_first_not_of(" \t") != std::string::npos) {
            out.push_back({0, {line}}); // a blank line after a list just ends it
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return out;
}

inline bool prose_has_list(const std::string& raw) {
    if (raw.find('-') == std::string::npos && raw.find('*') == std::string::npos &&
        raw.find('.') == std::string::npos && raw.find(')') == std::string::npos &&
        raw.find("\xe2\x80\xa2") == std::string::npos)
        return false; // the common case, without splitting a single line
    for (const auto& c : prose_chunks(raw))
        if (c.kind) return true;
    return false;
}

// a paragraph chunk's text, without the blank lines at either end
inline std::string prose_chunk_text(const ProseChunk& c) {
    auto blank = [](const std::string& s) {
        return s.find_first_not_of(" \t") == std::string::npos;
    };
    size_t a = 0, b = c.lines.size();
    while (a < b && blank(c.lines[a])) ++a;
    while (b > a && blank(c.lines[b - 1])) --b;
    std::string s;
    for (size_t i = a; i < b; ++i) s += (i > a ? "\n" : "") + c.lines[i];
    return s;
}

/* The newsletter's form. `p_style` is the paragraph's inline style, `lead` is
 * markup placed before the first words (a narrative's emoji), and lists are
 * inline-styled `<ul>`/`<ol>`, which every mail client renders. */
inline std::string email_prose_block(const std::string& raw, const std::string& p_style,
                                     const std::string& ink, const std::string& lead,
                                     bool newline) {
    const std::string nl = newline ? "\n" : "";
    if (!prose_has_list(raw))
        return "<p style=\"" + p_style + "\">" + lead + prose(raw) + "</p>" + nl;
    std::string o, first = lead;
    for (const auto& c : prose_chunks(raw)) {
        if (c.kind == 0) {
            const std::string t = prose_chunk_text(c);
            if (t.empty()) continue;
            o += "<p style=\"" + p_style + ";margin:0 0 10px\">" + first + prose(t) + "</p>" + nl;
        } else {
            const std::string tag = c.kind == 2 ? "ol" : "ul";
            o += "<" + tag + " style=\"margin:0 0 12px;padding-left:24px;line-height:1.5;color:" +
                 ink + "\">";
            for (const auto& it : c.lines) {
                o += "<li style=\"margin:0 0 4px\">" + first + prose(it) + "</li>";
                first.clear();
            }
            o += "</" + tag + ">" + nl;
        }
        first.clear();
    }
    return o;
}

/* The website's form. With no list it is the `<p class>` both web callers
 * always wrote; with one it is a `.prose.prose-rich` block of paragraphs and
 * `.prose-list` lists (style.css). `emit_empty` = false returns "" for empty
 * text, which is what `image_text` wants. */
inline std::string web_prose_block(const std::string& raw, const std::string& p_class,
                                   bool emit_empty = true) {
    if (!prose_has_list(raw)) {
        if (raw.empty() && !emit_empty) return {};
        return "<p class=\"" + p_class + "\">" + web_prose(raw) + "</p>";
    }
    const bool reveal = p_class.find("reveal") != std::string::npos;
    std::string o = std::string("<div class=\"prose prose-rich") + (reveal ? " reveal" : "") + "\">";
    for (const auto& c : prose_chunks(raw)) {
        if (c.kind == 0) {
            const std::string t = prose_chunk_text(c);
            if (!t.empty()) o += "<p class=\"pre-line\">" + web_prose(t) + "</p>";
        } else {
            const std::string tag = c.kind == 2 ? "ol" : "ul";
            o += "<" + tag + " class=\"prose-list\">";
            for (const auto& it : c.lines) o += "<li>" + web_prose(it) + "</li>";
            o += "</" + tag + ">";
        }
    }
    return o + "</div>";
}

/* ── ORDER BY WHAT SOMETHING IS, NOT ONLY BY ITS NAME (2026-09-15) ───────────
 *
 * The author, on the website's directory: *"right now its just listing people
 * alphabetically ... the order in which people are placed does kinda matter ...
 * we have alphebetical as a baseline, however, your order in the queue can
 * increase based on 'positive' tags or 'negative' tags can decrease your order
 * ... 'leader' so all leaders show up higher ... or 'volunteer' as a negative
 * tag, so the volunteers are listed last. This sort of thing could probably be
 * applied to other things as well such as image fliers or anything with a
 * list."*
 *
 * `rank_up` and `rank_down` on a block are lists of tags (commas or spaces).
 * The block's own order stays the baseline, and ties keep it: a stable sort.
 * EARLIER TAGS WEIGH MORE, each outweighing every tag after it combined, so
 * `rank_up leader, board` lists leaders, then board members, then everyone —
 * and a leader on the board above a leader who is not. `rank_down` mirrors it:
 * its first tag sinks furthest.
 *
 * A tag with no namespace also matches under any namespace, so `leader` finds
 * `role:leader`, while `role:leader` matches only itself. Pure and shared, so
 * the website and the newsletter list the same people in the same order.
 */
inline std::vector<std::string> rank_tokens(const std::string& raw) {
    std::vector<std::string> toks;
    std::string cur;
    for (size_t i = 0; i <= raw.size(); ++i) {
        const char c = i < raw.size() ? raw[i] : ',';
        if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!cur.empty()) toks.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (toks.size() > 30) toks.resize(30); // the weights are powers of two
    return toks;
}

inline long long tag_rank_score(const std::vector<std::string>& tags,
                                const std::vector<std::string>& up,
                                const std::vector<std::string>& down) {
    auto has = [&](const std::string& want) {
        const bool bare = want.find(':') == std::string::npos;
        for (const auto& t : tags) {
            if (t == want) return true;
            const size_t colon = t.rfind(':');
            if (bare && colon != std::string::npos &&
                t.compare(colon + 1, std::string::npos, want) == 0)
                return true;
        }
        return false;
    };
    long long score = 0;
    for (size_t i = 0; i < up.size(); ++i)
        if (has(up[i])) score += 1LL << (up.size() - 1 - i);
    for (size_t i = 0; i < down.size(); ++i)
        if (has(down[i])) score -= 1LL << (down.size() - 1 - i);
    return score;
}

template <class T, class TagsOf>
inline void rank_by_tags(std::vector<T>& items, const std::string& up_raw,
                         const std::string& down_raw, TagsOf tags_of) {
    const std::vector<std::string> up = rank_tokens(up_raw), down = rank_tokens(down_raw);
    if (up.empty() && down.empty()) return;
    std::vector<std::pair<long long, size_t>> order;
    order.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i)
        order.push_back({tag_rank_score(tags_of(items[i]), up, down), i});
    std::stable_sort(order.begin(), order.end(),
                     [](const auto& x, const auto& y) { return x.first > y.first; });
    std::vector<T> sorted;
    sorted.reserve(items.size());
    for (const auto& o : order) sorted.push_back(std::move(items[o.second]));
    items.swap(sorted);
}

inline void rank_by_tags(std::vector<const maiz::SceneNode*>& items, const std::string& up,
                         const std::string& down) {
    rank_by_tags(items, up, down,
                 [](const maiz::SceneNode* n) -> const std::vector<std::string>& {
                     return n->tags;
                 });
}

/* ── A LIST OF CSS COLOURS, VALIDATED (2026-09-02) ──────────────────────────
 *
 * For `divider_style bar` — the coloured swatch strip the field report asked
 * for (A6: "the brand's strongest repeating device and there is no way to put a
 * coloured bar on a page"). Comma- or space-separated in, safe-to-emit out.
 *
 * VALIDATED RATHER THAN ESCAPED, because there is no such thing as a safely
 * escaped arbitrary CSS value: a field that reaches a `style` attribute is a
 * field that could carry `;background:url(...)`, and the render seam does not
 * get an exception for a swatch. A hex triplet or a bare colour word survives;
 * anything else comes back in `rejected` so the renderer can say what it
 * dropped, because a silently ignored value is the defect this whole feature
 * was reported alongside.
 *
 * Pure, so a test can hand it `#fff; background:url(x)` and check what comes
 * back — which is the reason it lives here and not in the emit lambda. */
inline std::vector<std::string> css_colors(const std::string& raw,
                                           std::vector<std::string>* rejected) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i <= raw.size(); ++i) {
        const char c = i < raw.size() ? raw[i] : ',';
        if (c != ',' && c != ' ' && c != '\t') { cur += c; continue; }
        if (cur.empty()) continue;
        bool good = cur.size() <= 24;
        if (good && cur[0] == '#') {
            good = (cur.size() == 4 || cur.size() == 7 || cur.size() == 9);
            for (size_t k = 1; good && k < cur.size(); ++k)
                good = std::isxdigit((unsigned char)cur[k]) != 0;
        } else {
            for (size_t k = 0; good && k < cur.size(); ++k)
                good = std::isalpha((unsigned char)cur[k]) != 0;
        }
        if (good) out.push_back(cur);
        else if (rejected) rejected->push_back(cur);
        cur.clear();
    }
    return out;
}

/* Cut to a whole word near `n`, with an ellipsis. Used by the grid `detail`
 * levels. Returns the input untouched when it already fits, so `compact` costs
 * nothing on a one-line summary. */
inline std::string clip(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    size_t cut = s.rfind(' ', n);
    if (cut == std::string::npos || cut < n / 2) cut = n;
    std::string out = s.substr(0, cut);
    while (!out.empty() && std::strchr(" ,;:.", out.back())) out.pop_back();
    return out + "\xE2\x80\xA6"; // …
}

/* A BULLETPROOF EMAIL BUTTON: a one-cell table wrapping a padded anchor.
 *
 * Not a styled `<a>`, because Outlook's word-processor rendering engine drops
 * padding on inline elements — the button would collapse to bare underlined
 * text in the client a community organization's audience is most likely to be
 * using. The table is the standard workaround and it is why this is a helper
 * rather than three lines at the call site. */
/* `columns` on a card grid (event_grid, job_grid), as the website's class.
 * Blank keeps the auto-fill grid the site always drew; 1-3 fixes the count, and a
 * phone still stacks (style.css, `.cards.cols-N`). The newsletter reads the same
 * field and lays the cards out as table cells (render/email.cpp). */
inline std::string grid_cols_class(const maiz::SceneNode& n) {
    const int c = hormiga::doc_field_int(n, "columns", 0);
    return (c >= 1 && c <= 3) ? " cols-" + std::to_string(c) : std::string();
}

inline std::string email_button(const std::string& href, const std::string& label,
                                const std::string& accent) {
    return "<table role=\"presentation\" cellpadding=\"0\" cellspacing=\"0\" "
           "style=\"margin:12px 0\"><tr><td align=\"center\" bgcolor=\"" + accent +
           "\" style=\"background:" + accent + ";padding:11px 22px\">"
           "<a href=\"" + href +
           "\" style=\"color:#ffffff;text-decoration:none;font-weight:bold;"
           "font-size:15px;display:inline-block\">" +
           html_escape(label) + "</a></td></tr></table>\n";
}

/* `target` as an href. A bare `name@host` is a mailto; a page slug is left
 * alone (the site render resolves it, and in an email it is at least visible
 * rather than a dead `#`). */
/* ── RFC 5545 helpers (2026-08-19) ───────────────────────────────────────────
 *
 * The `.ics` twin the website writes was malformed for every time in a real
 * community database, because it sliced fixed offsets out of a string that is
 * whatever a person typed off a flier. These three are the whole fix.
 */

/* `parse_clock` MOVED to `domain/clock.hpp` (2026-09-10) and is reachable from
 * here through that include, so every existing call site is unchanged.
 *
 * The move was forced by a good question: this header's own docstring says its
 * helpers are "pure — string in, string out, no HormigaApp, no ImGui, no I/O",
 * and every function in it is. But the HEADER is not: line 21 includes
 * `app/app_internal.hpp`, which reaches ImGui and the whole application. So a
 * function here is pure and still un-shareable — the calendar could not use the
 * one time parser without dragging a window in behind it, which is precisely
 * why the grid grew a second, worse one and disagreed with this file for three
 * weeks about what time a 3 PM meeting is.
 *
 * `domain/clock.hpp` includes nothing but `<cctype>` and `<string>`, which is
 * what makes "one time parser" a property of the build rather than a promise. */

/* `ics_text` and `ics_now_utc` MOVED to `domain/ical.hpp` (2026-09-10) as
 * `ical::escape_text` and `ical::now_utc`, with the rest of the RFC 5545
 * writer. They lived here because the `.ics` twin was written inline in the
 * site renderer's calendar block; it is a lens now, in `domain/`, because every
 * transport in the calendar's X-track needs it and none of them renders a page.
 * Nothing outside that header calls them any more. */

/* A DATA RUNE'S NAME AS A PERSON READS IT.
 *
 * A rune name is a command-safe slug, so it is lowercase, hyphenated, and
 * ASCII — and for an organization whose people write their names with diacritics that is the fatal
 * one: `jose-garcia` is not how José García writes their name. `humanize()`
 * title-cases a slug, which also gives "Acme" for ACME and "Mcdonald" for
 * McDonald.
 *
 * `display_name` is the declared field that says what to print. Empty falls
 * back to the humanized slug, so nothing that already exists changes and a
 * database can be corrected one rune at a time — the same shape as
 * `title_en` on an event, for the same reason. */
inline std::string display_name(const maiz::SceneNode& n) {
    const std::string v = field_value(n, "display_name");
    return v.empty() ? humanize(n.name) : v;
}

/* An ISO date, as a person reads it (2026-08-20).
 *
 * `2026-08-19` is a machine's spelling. It is correct, it sorts, and it is the
 * right thing to STORE — and it is not what a flier says or what a visitor
 * scanning a page wants to see. This is the same argument as `title_en` over a
 * rune slug, one field along: the stored form and the printed form are
 * different jobs.
 *
 * The year is shown only when it is not the current one, because "2026" on
 * every card of a page read in 2026 is noise that pushes the venue down a line.
 * Anything that is not an ISO date (a free-text `days` like "Last Friday of the
 * Month") is passed through untouched — a recurrence is not an instant, and
 * mangling it would lose the only information it carries. */
inline std::string human_date(const std::string& iso, std::string_view lang) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12)
        return iso;
    static const char* kMonEn[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    static const char* kMonEs[] = {"ene", "feb", "mar", "abr", "may", "jun",
                                   "jul", "ago", "sep", "oct", "nov", "dic"};
    static const char* kDayEn[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* kDayEs[] = {"dom", "lun", "mar", "mie", "jue", "vie", "sab"};
    const bool es = lang == "es";
    // Sakamoto's day-of-week: no <ctime> round trip, no locale, no surprises
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int yy = y - (m < 3 ? 1 : 0);
    const int dow = (yy + yy / 4 - yy / 100 + yy / 400 + t[m - 1] + d) % 7;
    char out[64];
    std::tm now{};
    const std::time_t nt = std::time(nullptr);
#ifdef _WIN32
    localtime_s(&now, &nt);
#else
    localtime_r(&nt, &now);
#endif
    const bool this_year = (now.tm_year + 1900) == y;
    if (es)
        std::snprintf(out, sizeof out, this_year ? "%s, %d de %s" : "%s, %d de %s %d",
                      kDayEs[dow], d, kMonEs[m - 1], y);
    else
        std::snprintf(out, sizeof out, this_year ? "%s, %s %d" : "%s, %s %d, %d",
                      kDayEn[dow], kMonEn[m - 1], d, y);
    return out;
}

/* An "add to Google Calendar" URL, built HOST-SIDE (2026-08-20).
 *
 * `site_js()`'s `glink()` did `e.s.replace(':','')` on the start time, which is
 * the identical defect already fixed in the `.ics` writer — living on in a
 * SECOND parser, in another language, one file away. Every time in a real
 * community database is 12-hour with a meridiem, because that is what a flier
 * prints, so it produced `dates=20260819T800 AM00/...`: a space and "AM" inside
 * a value that must be basic-format digits. Google opens on the wrong date
 * rather than erroring, so it failed silently on every event on the site.
 *
 * The fix is not to port the parser to JavaScript. It is to stop having two:
 * `parse_clock` already exists and is already correct, so the renderer computes
 * the whole URL and hands it to the page as data. The same reasoning that gave
 * `deploy_site` and `rollback_site` one `host_said_ok` — a second
 * implementation of a rule is a second thing to fix and a guarantee that one of
 * them will be missed. */
inline std::string gcal_url(const std::string& title, const std::string& iso_date,
                            const std::string& start, const std::string& end,
                            const std::string& venue) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso_date.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return {};
    auto enc = [](const std::string& v) { // percent-encode for a query value
        static const char* hex = "0123456789ABCDEF";
        std::string o;
        for (unsigned char c : v) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                o += (char)c;
            else {
                o += '%';
                o += hex[c >> 4];
                o += hex[c & 0xF];
            }
        }
        return o;
    };
    char day[16];
    std::snprintf(day, sizeof day, "%04d%02d%02d", y, m, d);
    std::string dates;
    int sh = 0, sm = 0, eh = 0, em = 0;
    if (parse_clock(start, sh, sm)) {
        if (!parse_clock(end, eh, em)) { eh = sh + 1; em = sm; }
        char a[24], b[24];
        std::snprintf(a, sizeof a, "%sT%02d%02d00", day, sh, sm);
        std::snprintf(b, sizeof b, "%sT%02d%02d00", day, eh % 24, em);
        dates = std::string(a) + "/" + b;
    } else {
        // all-day: Google's end is EXCLUSIVE, same as RFC 5545's
        std::tm t{};
        t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d + 1; t.tm_hour = 12;
        char nxt[16];
        if (std::mktime(&t) != (std::time_t)-1)
            std::snprintf(nxt, sizeof nxt, "%04d%02d%02d", t.tm_year + 1900,
                          t.tm_mon + 1, t.tm_mday);
        else
            std::snprintf(nxt, sizeof nxt, "%s", day);
        dates = std::string(day) + "/" + nxt;
    }
    std::string u = "https://calendar.google.com/calendar/render?action=TEMPLATE&text=" +
                    enc(title) + "&dates=" + dates;
    if (!venue.empty()) u += "&location=" + enc(venue);
    return u;
}

inline std::string href_of(const std::string& target) {
    if (target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0 ||
        target.rfind("mailto:", 0) == 0)
        return target;
    if (target.find('@') != std::string::npos &&
        target.find(' ') == std::string::npos)
        return "mailto:" + target;
    return target;
}

