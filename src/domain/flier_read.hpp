/* flier_read.hpp — turning what is printed on a flier into commands a person
 * can read before dispatching.
 *
 * ── why this file exists ─────────────────────────────────────────────────────
 *
 * The operator, through the field agent, 2026-08-28: *"we might also want to
 * consider analyzing the fliers with some sort of tool so we know which dates
 * correspond to them"*, and separately, tagging fliers *"with key words that
 * might be written on the flier, so it's easier to search."*
 *
 * The agent did both by hand that day — read four fliers, pulled the dates and
 * the venue off them, applied 60-odd `kw:` tags — and said the honest thing
 * about it: *"it does not scale past a person who is willing to look at every
 * image."*
 *
 * ── what this is NOT ────────────────────────────────────────────────────────
 *
 * *"I am not asking for OCR in the binary. Something more like: an effect that
 * PROPOSES a transcript of tags from an image, for a person to dispatch. Same
 * seam as everything else, and the same reason."*
 *
 * So there is no OCR here, no model, and no network. The recognizer is whatever
 * the operator has already got — `tesseract`, a shell script, a cloud CLI they
 * are already paying for — named in one config key and invoked exactly the way
 * the deploy command is:
 *
 *     config set tools.image_text "tesseract {path} - -l eng+spa"
 *
 * That is the same decision `deploy_cmd` and `rollback_cmd` made and for the
 * same reason: the vendor call is the operator's to state, not ours to compile
 * into a binary that needs a release to correct. It also means the capability
 * is absent by default rather than pretending to work.
 *
 * ── and the OUTPUT is a transcript, not a change ────────────────────────────
 *
 * Nothing here dispatches. It compiles what a recognizer saw into `tag` and
 * `set` commands and prints them, and a person decides. That is founding
 * commitment 1 read the honest way round: every change is a logged, replayable
 * dispatcher command, and a change proposed by a machine that cannot read is
 * still a change — so it goes through a human and then through the door, rather
 * than around both. A tool that wrote 60 tags by itself would be a tool nobody
 * could review, which is worse than the afternoon it saves.
 *
 * Pure: text in, commands out. The effect that shells out lives in the front
 * end; everything interesting is here, where a test can reach it.
 */
#pragma once

#include "domain/date_query.hpp" // days_from_civil, parse_iso_date

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace hormiga {
namespace flier {

/* A date a recognizer found, as ISO plus the text it came from — the second
 * half matters, because "9/16" is ambiguous and a person confirming a proposal
 * needs to see what was actually printed. */
struct FoundDate {
    std::string iso;
    std::string seen; // the substring on the flier
};

namespace detail {

inline std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

/* Month names in both of this organization's languages, plus the three-letter
 * abbreviations people actually print. Accent-free on the Spanish side because
 * a flier scan comes back accent-free about as often as not, and matching the
 * stripped form matches both. */
inline int month_from_word(const std::string& w) {
    static const char* kEn[] = {"january", "february", "march",     "april",
                                "may",     "june",     "july",      "august",
                                "september", "october", "november", "december"};
    static const char* kEs[] = {"enero",      "febrero",   "marzo",
                                "abril",      "mayo",      "junio",
                                "julio",      "agosto",    "septiembre",
                                "octubre",    "noviembre", "diciembre"};
    const std::string s = lower(w);
    if (s.size() < 3) return 0;
    for (int i = 0; i < 12; ++i) {
        const std::string en = kEn[i], es = kEs[i];
        if (s == en || s == es) return i + 1;
        // a leading three (or four, for "sept") letters is how they get printed
        if (s.size() <= 5 &&
            (en.rfind(s, 0) == 0 || es.rfind(s, 0) == 0))
            return i + 1;
    }
    return 0;
}

inline bool is_word_char(char c) {
    return std::isalnum((unsigned char)c) != 0;
}

} // namespace detail

/* Every date the transcript appears to contain.
 *
 * THE YEAR IS THE HARD PART and it is why `hint_year` exists. A flier says
 * "September 16th"; it almost never says 2026, because the person holding it
 * knows what year it is. So the caller passes the year to assume — in practice
 * the linked event's, or the current one — and the proposal shows the full date
 * it built so a human can see the assumption rather than inherit it silently.
 *
 * The shapes that turn up on real sheets:
 *
 *     September 16, 2026     September 16th      16 de septiembre
 *     9/16/2026              9/16/26             2026-09-16
 *
 * A bare `9/16` is deliberately NOT matched: on a flier it is as likely to be a
 * time, a room number or a score as a date, and a wrong date proposed
 * confidently is worse than a date not proposed at all — the whole point of
 * this is to catch the case where a printed date and a stored date disagree. */
inline std::vector<FoundDate> dates_in(const std::string& text, int hint_year) {
    std::vector<FoundDate> out;
    auto add = [&](int y, int m, int d, const std::string& seen) {
        if (m < 1 || m > 12 || d < 1 || d > 31) return;
        char buf[16];
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
        for (const auto& f : out)
            if (f.iso == buf) return; // a flier repeats its date; propose once
        out.push_back({buf, seen});
    };

    // 1. ISO, and numeric M/D/Y — anchored on a separator so a phone number
    //    cannot become a date.
    for (size_t i = 0; i < text.size(); ++i) {
        int y = 0, m = 0, d = 0, n = 0;
        if (std::sscanf(text.c_str() + i, "%4d-%2d-%2d%n", &y, &m, &d, &n) == 3 &&
            n > 0 && y > 1900 && y < 2200 &&
            (i == 0 || !detail::is_word_char(text[i - 1]))) {
            add(y, m, d, text.substr(i, (size_t)n));
            i += (size_t)n - 1;
            continue;
        }
        if (std::sscanf(text.c_str() + i, "%2d/%2d/%4d%n", &m, &d, &y, &n) == 3 &&
            n > 0 && (i == 0 || !detail::is_word_char(text[i - 1]))) {
            if (y < 100) y += 2000; // "9/16/26"
            if (y > 1900 && y < 2200) {
                add(y, m, d, text.substr(i, (size_t)n));
                i += (size_t)n - 1;
            }
        }
    }

    // 2. A month NAME with a day near it, in either order — "September 16" and
    //    "16 de septiembre" are the same sheet in two languages.
    std::vector<std::string> words;
    std::vector<size_t> at;
    for (size_t i = 0; i < text.size();) {
        if (!detail::is_word_char(text[i])) { ++i; continue; }
        const size_t s0 = i;
        while (i < text.size() && detail::is_word_char(text[i])) ++i;
        words.push_back(text.substr(s0, i - s0));
        at.push_back(s0);
    }
    auto day_of = [](const std::string& w) -> int {
        if (w.empty() || w.size() > 4) return 0;
        size_t k = 0;
        while (k < w.size() && std::isdigit((unsigned char)w[k])) ++k;
        if (k == 0) return 0;
        // "16th" / "16th" / "1o" — the ordinal suffix is a word character
        const int v = std::atoi(w.substr(0, k).c_str());
        return (v >= 1 && v <= 31) ? v : 0;
    };
    for (size_t i = 0; i < words.size(); ++i) {
        const int m = detail::month_from_word(words[i]);
        if (!m) continue;
        int d = 0;
        std::string seen = words[i];
        // "September 16" / "September 16, 2026"
        if (i + 1 < words.size() && (d = day_of(words[i + 1])) > 0) {
            seen += " " + words[i + 1];
        } else if (i >= 2 && (d = day_of(words[i - 2])) > 0 &&
                   detail::lower(words[i - 1]) == "de") {
            seen = words[i - 2] + " de " + words[i]; // "16 de septiembre"
        } else if (i >= 1 && (d = day_of(words[i - 1])) > 0) {
            seen = words[i - 1] + " " + words[i];    // "16 September"
        } else {
            continue;
        }
        int y = hint_year;
        // an explicit year right after the day wins over the hint
        if (i + 2 < words.size()) {
            const int maybe = std::atoi(words[i + 2].c_str());
            if (maybe > 1900 && maybe < 2200) { y = maybe; seen += " " + words[i + 2]; }
        }
        add(y, m, d, seen);
    }
    return out;
}

/* Words worth proposing as `kw:` tags.
 *
 * Deliberately dumb, because a smarter version would need to be right. It keeps
 * words that are long enough to be searched for and are not in the stop list,
 * lowercases them, and stops at a modest count. A person reads the list and
 * strikes the ones that are noise, which takes seconds; a person who has to
 * think about what an extractor decided takes longer than doing it by hand,
 * which is the trap this whole feature is walking past.
 *
 * The stop list carries BOTH LANGUAGES for the obvious reason and one less
 * obvious one: an English-only stop list on a Spanish flier proposes `kw:para`,
 * `kw:como`, `kw:todos`, and the operator's first impression of the feature is
 * that it produces garbage. */
inline std::vector<std::string> keywords_in(const std::string& text, size_t limit) {
    static const char* kStop[] = {
        "and", "the", "for", "with", "you", "your", "our", "are", "will", "this",
        "that", "from", "all", "can", "not", "has", "have", "was", "were", "who",
        "what", "when", "where", "how", "why", "more", "info", "please", "join",
        "free", "com", "www", "http", "https", "org", "net", "pm", "am",
        "para", "por", "con", "los", "las", "del", "que", "una", "uno", "todos",
        "todas", "como", "mas", "este", "esta", "nuestro", "nuestra", "sus",
        "gratis", "informacion", "hasta", "desde", "sobre"};
    std::vector<std::string> out;
    std::vector<std::string> seen;
    std::string w;
    auto flush = [&]() {
        if (w.size() < 4 || w.size() > 24) { w.clear(); return; }
        bool digitish = true;
        for (char c : w) if (!std::isdigit((unsigned char)c)) { digitish = false; break; }
        if (digitish) { w.clear(); return; }
        for (const char* st : kStop)
            if (w == st) { w.clear(); return; }
        for (const auto& s : seen)
            if (s == w) { w.clear(); return; }
        seen.push_back(w);
        if (out.size() < limit) out.push_back(w);
        w.clear();
    };
    for (char c : text) {
        if (std::isalpha((unsigned char)c)) w += (char)std::tolower((unsigned char)c);
        else flush();
    }
    flush();
    return out;
}

/* The proposal, as commands.
 *
 * `rune` is the image; `event_iso` is the date of the event it is wired to, or
 * "" when it is wired to none. The commands are COMMENTED where a human has to
 * decide, because a transcript that is safe to paste blind is a transcript that
 * will be pasted blind. */
struct Proposal {
    std::vector<std::string> commands; // safe to dispatch as-is
    std::vector<std::string> notes;    // what a person has to decide
};

inline Proposal propose(const std::string& rune, const std::string& text,
                        const std::string& event_rune,
                        const std::string& event_iso, long long today) {
    Proposal out;
    int hint_year = 0;
    {
        int y = 0, m = 0, d = 0;
        if (parse_iso_date(event_iso, y, m, d)) hint_year = y;
    }
    if (!hint_year) {
        // the current year, since a flier on a desk is about the year it is
        const long long t = today;
        int y = 1970;
        while (days_from_civil(y + 1, 1, 1) <= t) ++y;
        hint_year = y;
    }

    const std::vector<FoundDate> found = dates_in(text, hint_year);
    if (found.empty()) {
        out.notes.push_back("no date recognised on this flier - if it prints "
                            "one, the recognizer did not read it");
    }
    for (const auto& f : found) {
        if (event_iso.empty()) {
            out.notes.push_back(
                "printed date " + f.iso + " (read as \"" + f.seen +
                "\") - this flier is linked to no event, so nothing checks it. "
                "`link " + rune + " <event> --relation flyer-of`");
        } else if (f.iso == event_iso) {
            out.notes.push_back("printed date " + f.iso + " matches " +
                                event_rune + " - nothing to do");
        } else {
            /* THE WHOLE POINT OF THE FEATURE. A flier that says one date and an
             * event that stores another is the failure that goes wrong
             * silently, and it is what put an August 19 sheet on an August 28
             * home page. Reported, never corrected: which of the two is right
             * is not something a recognizer gets to decide. */
            out.notes.push_back(
                "MISMATCH: the flier prints " + f.iso + " (read as \"" + f.seen +
                "\") but " + event_rune + " stores " + event_iso +
                ". One of them is wrong; fix whichever it is with `set " +
                event_rune + " date <YYYY-MM-DD>`.");
        }
    }

    for (const auto& k : keywords_in(text, 12))
        out.commands.push_back("tag " + rune + " +kw:" + k);
    return out;
}

} // namespace flier
} // namespace hormiga
