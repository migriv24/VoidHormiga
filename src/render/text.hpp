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
 * to get behaviour every mail client already gives them. */
inline std::string linkify(const std::string& escaped) {
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
            out += "<a href=\"" + url + "\" style=\"color:inherit\">" + url + "</a>";
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
                out += "<a href=\"mailto:" + addr + "\" style=\"color:inherit\">" +
                       addr + "</a>";
                i = e;
                continue;
            }
        }
        out += escaped[i++];
    }
    return out;
}

/* Escaped, then linkified — the pairing every prose field in the email domain
 * wants, named so a future block cannot accidentally use only half of it. */
inline std::string prose(const std::string& raw) {
    return linkify(html_escape(raw));
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

/* A wall-clock time as a human writes it → 24-hour (h, m). Accepts `9:00 AM`,
 * `09:00`, `9 AM`, `3:00 p.m.`, `15:00`, `noon`, `midnight`. Returns false when
 * there is no time in there at all, which is an ALL-DAY event and not an error.
 *
 * Deliberately permissive on input and strict on output: the model stores what
 * the flier said, and the calendar file is the one place that has to be exact. */
inline bool parse_clock(const std::string& raw, int& hh, int& mm) {
    std::string s;
    for (char c : raw) s += (char)std::tolower((unsigned char)c);
    if (s.find("noon") != std::string::npos) { hh = 12; mm = 0; return true; }
    if (s.find("midnight") != std::string::npos) { hh = 0; mm = 0; return true; }
    size_t i = 0;
    while (i < s.size() && !std::isdigit((unsigned char)s[i])) ++i;
    if (i >= s.size()) return false;
    int h = 0, m = 0;
    while (i < s.size() && std::isdigit((unsigned char)s[i]))
        h = h * 10 + (s[i++] - '0');
    if (i < s.size() && s[i] == ':') {
        ++i;
        int digits = 0;
        while (i < s.size() && std::isdigit((unsigned char)s[i]) && digits < 2) {
            m = m * 10 + (s[i++] - '0');
            ++digits;
        }
    }
    if (h > 23 || m > 59) return false;
    // the meridiem, which the old arithmetic threw away — the difference
    // between a 3 PM meeting and one at three in the morning
    const size_t p = s.find('p', i ? i - 1 : 0);
    const size_t a = s.find('a', i ? i - 1 : 0);
    const bool pm = p != std::string::npos && p + 1 < s.size() &&
                    (s[p + 1] == 'm' || s[p + 1] == '.');
    const bool am = a != std::string::npos && a + 1 < s.size() &&
                    (s[a + 1] == 'm' || s[a + 1] == '.');
    if (pm && h < 12) h += 12;
    if (am && h == 12) h = 0;   // 12:30 AM is 00:30
    hh = h;
    mm = m;
    return true;
}

/* Escape a TEXT value: RFC 5545 §3.3.11 gives `\` `;` `,` and newline meaning
 * inside one, so a venue like "Springfield, OR" silently ends the property and
 * starts a bogus parameter. */
inline std::string ics_text(const std::string& v) {
    std::string o;
    for (char c : v) {
        if (c == '\\' || c == ';' || c == ',') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') continue;
        else o += c;
    }
    return o;
}

/* DTSTAMP is REQUIRED on a VEVENT (RFC 5545 §3.6.1) and was absent, which some
 * importers reject the whole file over. UTC, because a stamp is an instant. */
inline std::string ics_now_utc() {
    const std::time_t t = std::time(nullptr);
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char b[32];
    std::snprintf(b, sizeof b, "%04d%02d%02dT%02d%02d%02dZ", g.tm_year + 1900,
                  g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
    return b;
}

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

