/* import.hpp — the CSV import compiler (phase C, headless).
 *
 * The import philosophy (okf/concepts/platform/antfarm/index.md, Import interface): a file
 * comes in ONCE, but what lands in the org is COMMANDS — `rune new` / `set` /
 * `tag` lines the caller wraps in one `compile_commit` batch. The import is
 * thereby a single undo frame, fully logged, and replayable forever without
 * the source file. Nothing here touches a core or a filesystem: text in,
 * commands out — testable headless, usable by app and future CLI alike.
 *
 * CSV contract (deliberately dumb, spreadsheet-first):
 *   - first row = headers, matched case-insensitively;
 *   - a "name" column names the rune (else the first column does);
 *   - a "tags" column holds space-separated tags (`month:july status:active`);
 *   - every other header matching a declared glyph field maps to `set`;
 *     unmatched headers are reported, not fatal;
 *   - quoted values, embedded commas, doubled quotes, CRLF all handled;
 *   - every rune gets `+type:<glyph>`; names are sanitized to command-safe
 *     slugs and deduplicated against the org and the file itself.
 */
#pragma once

#include <cctype>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace hormiga {

struct CsvImport {
    std::vector<std::string> commands; // ready for maiz::compile_commit
    int rows = 0;                      // data rows that produced a rune
    std::vector<std::string> notes;    // per-row/column issues, human-readable
    std::string error;                 // fatal ("" = ok)
};

namespace detail {

/* One CSV record (RFC-4180-ish): quotes, embedded commas/newlines, "" escape. */
inline std::vector<std::vector<std::string>> parse_csv(const std::string& text) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cell;
    bool quoted = false, any = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quoted) {
            if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
                cell += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                cell += c;
            }
            continue;
        }
        if (c == '"') quoted = true;
        else if (c == ',') { row.push_back(cell); cell.clear(); any = true; }
        else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            if (any || !cell.empty()) { row.push_back(cell); rows.push_back(row); }
            row.clear();
            cell.clear();
            any = false;
        } else cell += c;
    }
    if (any || !cell.empty()) { row.push_back(cell); rows.push_back(row); }
    return rows;
}

inline std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t");
    size_t b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

/* Command-safe slug: lowercase, [a-z0-9_-], runs of anything else → '-'. */
inline std::string slug(const std::string& s) {
    std::string out;
    bool dash = false;
    for (char c : lower(trim(s))) {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-') {
            out += c;
            dash = false;
        } else if (!dash && !out.empty()) {
            out += '-';
            dash = true;
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

inline std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

} // namespace detail

inline CsvImport compile_csv_import(
    const std::string& csv_text, const std::string& glyph,
    const std::vector<std::string>& glyph_fields,
    const std::function<bool(const std::string&)>& name_taken) {
    using namespace detail;
    CsvImport out;
    auto rows = parse_csv(csv_text);
    if (rows.size() < 2) {
        out.error = rows.empty() ? "empty file" : "no data rows under the header";
        return out;
    }

    // header → role: name column, tags column, field columns
    const auto& header = rows[0];
    int name_col = -1, tags_col = -1;
    std::vector<int> field_col(header.size(), -1); // index into glyph_fields
    for (int i = 0; i < (int)header.size(); ++i) {
        std::string h = lower(trim(header[i]));
        if (h == "name") name_col = i;
        else if (h == "tags") tags_col = i;
        else {
            bool matched = false;
            for (int f = 0; f < (int)glyph_fields.size(); ++f)
                if (lower(glyph_fields[f]) == h) {
                    field_col[i] = f;
                    matched = true;
                }
            if (!matched && !h.empty())
                out.notes.push_back("column \"" + trim(header[i]) + "\" ignored (no such " +
                                    glyph + " field)");
        }
    }
    if (name_col < 0) name_col = 0; // convention: first column names the rune

    std::set<std::string> minted;
    for (size_t r = 1; r < rows.size(); ++r) {
        const auto& row = rows[r];
        std::string base = slug(name_col < (int)row.size() ? row[name_col] : "");
        if (base.empty()) base = glyph + "-row" + std::to_string(r);
        std::string name = base;
        for (int n = 2; minted.count(name) || name_taken(name); ++n)
            name = base + "-" + std::to_string(n);
        minted.insert(name);

        out.commands.push_back("rune new " + glyph + " " + name);
        for (int i = 0; i < (int)row.size() && i < (int)field_col.size(); ++i) {
            if (field_col[i] < 0) continue;
            std::string v = trim(row[i]);
            if (v.empty()) continue;
            out.commands.push_back("set " + name + " " + glyph_fields[field_col[i]] +
                                   " " + quote(v));
        }
        std::string tag_cmd = "tag " + name + " +type:" + glyph;
        if (tags_col >= 0 && tags_col < (int)row.size()) {
            std::string t;
            for (char c : row[tags_col] + " ")
                if (std::isspace((unsigned char)c)) {
                    if (!t.empty()) tag_cmd += " +" + (t[0] == '+' ? t.substr(1) : t);
                    t.clear();
                } else t += c;
        }
        out.commands.push_back(tag_cmd);
        ++out.rows;
    }
    return out;
}

} // namespace hormiga
