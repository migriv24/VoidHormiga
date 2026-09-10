/* app/translate.cpp — WHAT IS NOT TRANSLATED YET, AND A FILE THAT FIXES IT.
 *
 * ── WHY THIS EXISTS, AND WHY IT IS NOT THE FEATURE THAT WAS ASKED FOR ────────
 *
 * The Click LaFont field report (2026-09-02) asked for A3. **Their proposal is
 * quoted below and was DECLINED — `site.languages` is not a key, has never been
 * read, and is not coming.** It is quoted because the reasoning matters, not
 * because it describes behaviour:
 *
 *   > **A one-language site is not expressible.** … Ask:
 *   > `config set site.languages 'en'`, default `'en,es'`. It suppresses the
 *   > second build, the switch and the alternates.
 *
 * (A later reader took that blockquote for a contract and set the key, which is
 * a fair reading of an unlabelled quote and the reason for this paragraph.
 * `render_site` now warns when the key is set, so nobody has to read this file
 * to find out.)
 *
 * The author declined it the same day, and the reasoning is why this file
 * exists rather than that switch:
 *
 *   > instead of opting to NOT have multi language, the ask should've been
 *   > "have better and more robust translation tools", so its easier for a site
 *   > to be in english and spanish. … the website itself should still
 *   > prioritize multiple languages. We don't want to be lazy in our
 *   > development. We want MORE features, not less.
 *
 * That is a commitment, not a preference. Void Hormiga was built for an
 * outreach organization whose Spanish half is not a translation of the site —
 * for most of the people it is published for it IS the site, which is the rule
 * `app.hpp`'s `site_langs()` already states unconditionally. A knob that turns
 * the second language off is a knob that gets reached for by every operator who
 * finds translating tedious, on the day they are busy, and the readers who lose
 * the page are the ones with the least recourse.
 *
 * ── SO WHAT WAS ACTUALLY WRONG ───────────────────────────────────────────────
 *
 * The report's evidence is real even though its remedy was not. A site with no
 * Spanish copy got a nav button promising Spanish that leads to an identical
 * English page, and a sitemap of duplicate pairs. Two answers, and neither of
 * them is "publish one language":
 *
 *   1. The duplicate-pair worry is already handled, and the report did not know
 *      it. Every page carries `<link rel="alternate" hreflang="…">` in both
 *      directions plus `x-default` — precisely the mechanism search engines
 *      document for the same content served under two language URLs. It is not
 *      an SEO liability; it is the SEO answer.
 *
 *   2. The real defect is that NOBODY IS TOLD. `text()` falls back from
 *      `title_es` to `title_en` silently, so an operator can publish a site
 *      that is 0% translated and never see a number. Same class of silence as
 *      D6, and it gets the same treatment: measure it, say it, and hand over
 *      the thing that fixes it.
 *
 * ── THE THING THAT FIXES IT ──────────────────────────────────────────────────
 *
 * `effect translation-report [lang]` writes a REPLAYABLE SCRIPT, not a list.
 * Every gap becomes a `set <rune> <field>_es '<the English text>'` line with
 * the source text already in place, grouped under the `use <mantle>` that makes
 * it apply. A translator — human or agent — edits the right-hand sides, and the
 * file replays through the ordinary door:
 *
 *     voidhormiga-cli --script --atomic --actor "maria" \
 *         exports/translate-es.hormiga
 *
 * Founding commitment 1 the whole way: translating is a batch of dispatcher
 * commands, so it is logged, attributed, replayable and diffable like every
 * other change, and `--atomic` means a half-finished pass lands whole or not at
 * all.
 */
#include "app/app_internal.hpp"
#include "render/text.hpp" // untranslatable(): shared with the render coverage count

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

namespace {

/* Is `key` a per-language field, and if so what is its base and language?
 *
 * The convention is an `_en` / `_es` suffix on an otherwise ordinary field name
 * — `title_en`, `summary_es`, `text_en`, `caption_es`. Derived from
 * `site_langs()` rather than from a hardcoded pair, deliberately: the day a
 * third language joins that list this file finds its fields with no edit, which
 * is the whole reason `site_langs()` is a list and nothing downstream names a
 * language. */
bool split_lang_field(const std::string& key, std::string& base, std::string& lang) {
    for (const std::string& lg : hormiga::site_langs()) {
        const std::string suf = "_" + lg;
        if (key.size() > suf.size() &&
            key.compare(key.size() - suf.size(), suf.size(), suf) == 0) {
            base = key.substr(0, key.size() - suf.size());
            lang = lg;
            return true;
        }
    }
    return false;
}

} // namespace

/* Every field that says something in one language and nothing in `lang`.
 *
 * Scans the data mantle AND every document mantle, because a website's own
 * prose (`hero`, `narrative`, `section_header`, `page`) lives in the document
 * while the things it displays (`event`, `image`, `contact`) live in the data —
 * and a site is only as translated as the thinner of the two.
 *
 * Returns the number of gaps, or -1 on refusal. Writes the script when there
 * are any. */
int HormigaApp::translation_report(const std::string& state_json,
                                   std::string_view lang) {
    /* Boot a core from the document we were handed, the way `sync_ops.cpp`
     * does and for the same stated reason: every read below takes the state
     * from HERE rather than from a core somebody hopes is loaded. This method
     * walks EVERY mantle, so it cannot ride the active projection.
     *
     * (No `refresh_allo_rules()`: a translation gap is a field being empty, and
     * no derived rule can fill one in. Adding the call would be borrowing a
     * step from `render_from_state` that this does not need.) */
    core = maiz::Core(state_json);
    if (on_register_glyphs) on_register_glyphs(core);
    const size_t log_from = log.size();
    struct Reporter {
        const std::vector<maiz::LogEntry>& log;
        size_t from;
        ~Reporter() {
            for (size_t i = from; i < log.size(); ++i)
                if (log[i].op == "translate")
                    std::cerr << "  [" << log[i].level << "] translate: "
                              << log[i].msg << "\n";
        }
    } _report{log, log_from};

    const std::string target(lang);
    if (std::find(hormiga::site_langs().begin(), hormiga::site_langs().end(),
                  target) == hormiga::site_langs().end()) {
        log.push_back({"error", "translate",
                       "'" + target + "' is not one of this site's languages (" +
                           hormiga::site_langs_str() + ")"});
        return -1;
    }

    /* Which mantles hold translatable prose. The Antfarm, the Allomone rules and
     * the civic record are excluded on purpose: they are configuration, logic
     * and a versioned public record respectively, and no visitor reads any of
     * them in a chosen language. Including them would bury the pages a person
     * actually reads under node labels. */
    std::vector<std::string> mantles{kDataMantle};
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' '))
            line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (line.empty() || line == "(no mantles)" || line == kDataMantle ||
            line == kAntfarmMantle || line == kAlloMantle || line == kCivicMantle)
            continue;
        mantles.push_back(line);
    }

    struct Gap { std::string rune, field, source_lang, source_text; };
    std::map<std::string, std::vector<Gap>> by_mantle;
    int total = 0, translated = 0;

    for (const std::string& mt : mantles) {
        maiz::ProjectOptions po;
        po.mantle = mt;
        const maiz::Scene sc = maiz::project_scene(core, po);
        for (const auto& n : sc.nodes) {
            /* Every base name this rune carries a per-language field for. A set,
             * so `title_en` and `title_es` are one entry and the gap is asked
             * about once rather than once per language present. */
            std::set<std::string> bases;
            for (const auto& f : n.fields) {
                std::string base, lg;
                if (split_lang_field(f.key, base, lg)) bases.insert(base);
            }
            for (const std::string& base : bases) {
                if (!field_value(n, base + "_" + target).empty()) {
                    ++total;
                    ++translated;
                    continue;
                }
                /* Nothing in the target language. Is there anything in any
                 * other? A rune with `title_en` and `title_es` both empty is not
                 * an untranslated rune, it is an unwritten one, and putting it
                 * in a translator's file is how a translator learns to skim the
                 * file. */
                std::string src_lang, src_text;
                for (const std::string& lg : hormiga::site_langs()) {
                    if (lg == target) continue;
                    const std::string v = field_value(n, base + "_" + lg);
                    if (!v.empty()) { src_lang = lg; src_text = v; break; }
                }
                if (src_text.empty()) continue;
                /* AN ADDRESS HAS NO SPANISH (2026-09-03). Counting an email or
                 * a phone number as untranslated makes 100% unreachable, and a
                 * warning that cannot be cleared is one people stop reading —
                 * which costs every other warning this renderer prints. A rune
                 * tagged `lang:none` opts out wholesale, for the shapes this
                 * heuristic does not know. */
                if (untranslatable(src_text) ||
                    maiz::node_matches("lang:none", n))
                    continue;
                ++total;
                by_mantle[mt].push_back({n.name, base, src_lang, src_text});
            }
        }
    }

    if (total == 0) {
        log.push_back({"info", "translate",
                       "nothing in this database carries per-language fields yet"});
        return 0;
    }
    const int gaps = total - translated;
    if (gaps == 0) {
        log.push_back({"info", "translate",
                       "every translatable field has a " + target + " value (" +
                           std::to_string(total) + " of " + std::to_string(total) +
                           ")"});
        return 0;
    }
    const int pct = (int)((translated * 100.0) / total + 0.5);

    /* THE SCRIPT. Written beside the database under `exports/`, the folder for
     * things that are re-derivable — this one is, exactly, by running the effect
     * again. */
    std::error_code ec;
    fs::create_directories(data_dir("exports"), ec);
    const fs::path out = data_dir("exports") / ("translate-" + target + ".hormiga");
    std::ofstream o(out, std::ios::binary | std::ios::trunc);
    if (!o) {
        log.push_back({"error", "translate", "cannot write " + out.string()});
        return -1;
    }
    o << "# Translations into " << target << " - " << gaps
      << " field(s) to write.\n"
      << "#\n"
      << "# Each `set` line already carries the text in the other language.\n"
      << "# REPLACE the quoted value with the " << target << " translation, and\n"
      << "# DELETE any line you do not want to change. A line left as it is\n"
      << "# would write the source language INTO the " << target << " field,\n"
      << "# which is worse than the fallback it replaces - the fallback is at\n"
      << "# least honest about which language it is showing.\n"
      << "#\n"
      << "# Then replay it through the ordinary door:\n"
      << "#\n"
      << "#   voidhormiga-cli --script --atomic --actor \"<your name>\" "
      << out.string() << "\n"
      << "#\n"
      << "# --atomic means a half-finished pass lands whole or not at all.\n";

    for (const auto& [mt, list] : by_mantle) {
        o << "\nuse " << mt << "\n";
        for (const Gap& g : list)
            o << "# " << g.rune << "." << g.field << " (" << g.source_lang << ")\n"
              << "set " << g.rune << " " << g.field << "_" << target << " "
              << json_str(g.source_text) << "\n";
    }
    o.close();

    log.push_back({"warn", "translate",
                   std::to_string(translated) + " of " + std::to_string(total) +
                       " translatable field(s) have a " + target + " value (" +
                       std::to_string(pct) + "%) - the other " +
                       std::to_string(gaps) +
                       " fall back to the language they were written in"});
    for (const auto& [mt, list] : by_mantle)
        log.push_back({"info", "translate",
                       "  " + mt + ": " + std::to_string(list.size()) +
                           " field(s) to translate"});
    log.push_back({"info", "translate",
                   "wrote " + out.string() +
                       " - edit the values, then `--script --atomic` it back in"});
    return gaps;
}

/* ── `site.languages` IS NOT A KEY, AND SAYING SO IS THE FIX ─────────────────
 *
 * Reported by the portfolio agent, 2026-09-02, and they are right that it is a
 * trust bug rather than a missing feature:
 *
 *   > A config key that accepts, stores and returns a value it does not act on
 *   > is worse than a missing one, because the read-back confirms it.
 *
 * `config set site.languages 'en'` succeeded, `config get` returned `en`, and
 * `site_langs()` above is a hardcoded pair that nothing reads it into. Void
 * Core's `config` is a free-form key-value store by design — it cannot know
 * which keys this application honours — so the check belongs at the seam that
 * would have honoured it, which is this file.
 *
 * THE FEATURE IS NOT COMING, and that is why this says so rather than
 * implementing it. The author declined a one-language switch in terms that make
 * it a commitment (see the header above). An operator who typed this key
 * deserves to be told that once, with the thing that does exist named — not to
 * discover it from ten rendered files.
 *
 * Warns rather than refuses: the site is fine, the key is inert, and a render
 * is not the place to fail over a stale config line.
 */
void HormigaApp::warn_unread_language_key() {
    std::string v = core.dispatch("config get site.languages").data;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
        v = v.substr(1, v.size() - 2);
    if (v.empty() || v == "null") return;
    log.push_back(
        {"warn", "render",
         "`site.languages` is set to '" + v +
             "' and nothing reads it - Hormiga publishes every language it "
             "knows (" + hormiga::site_langs_str() +
             ") and there is deliberately no way to turn one off. If translating "
             "is the problem, `effect translation-report` writes a script with "
             "the source text already in it. `config rm site.languages` clears "
             "this line."});
}
