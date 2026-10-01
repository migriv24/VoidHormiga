/* domain/chambers.hpp — the .miga's chambers as real mantles (Antfarm v2).
 *
 * okf/concepts/platform/antfarm/v2/mantles.md. The author, 2026-09-28: "i don't
 * see how we can even have a prototype yet when the assets, network, and
 * documents dont even exist as mantles yet ... that's an integral aspect of this
 * whole overhaul." So they exist:
 *
 *   assets     one `asset` rune per file: its content address, size, type and
 *              where it sits beside the database. WHAT a file is; rivers say where.
 *   network    one `profile` rune per device that has opened this database,
 *              written by that device about itself (never by another).
 *   documents  one `chamber_doc` rune per newsletter, website, calendar and map:
 *              the document's IDENTITY and where it goes (kind, title, mount,
 *              last published), beside its CONTENT, which stays where its tab
 *              edits it (a Builder mantle, a calendar view, a map view).
 *
 * Everything here is Scene-in / commands-out, like every other domain file, so
 * the GUI, the CLI and the tests keep the chambers the same way. Names are
 * DETERMINISTIC (from a content address, a key fingerprint, a document's own
 * name), so two devices reconciling the same database mint the same rune and a
 * merge sees one thing, not two (Q74; Void Maiz's worst September bug). */
#pragma once

#include "domain/scene_value.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/scene.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace hormiga::chambers {

inline constexpr const char* kAssets = "assets";
inline constexpr const char* kNetwork = "network";
inline constexpr const char* kDocuments = "documents";

inline bool is_chamber(const std::string& mantle) {
    return mantle == kAssets || mantle == kNetwork || mantle == kDocuments;
}

inline void register_glyphs(maiz::Core& core) {
    core.register_glyph(
        R"({"glyph":"asset","label":"Asset","kind":"entity",)"
        R"("fields":["file","sha256","bytes","media","original","url","hosted_by","added_by"],)"
        R"("hints":{"color":"#7d5bb0","face":{"w":210,"h":56},"category":"Assets",)"
        R"("editors":{"file":"path"},)"
        R"__("labels":{"file":"File (beside the database)","sha256":"Content address (sha256)",)__"
        R"__("bytes":"Size (bytes)","media":"Type","original":"Original name",)__"
        R"__("url":"Public link (when hosted)","hosted_by":"Hosted by","added_by":"Added by"}}})__");
    core.register_glyph(
        R"({"glyph":"profile","label":"Profile","kind":"entity",)"
        R"("fields":["username","color","device","platform","serves_local","always_on",)"
        R"("fingerprint","role","first_seen","last_opened"],)"
        R"("hints":{"color":"#6b7a8f","face":{"w":210,"h":56},"category":"Network",)"
        R"("editors":{"device":"combo:desktop,phone,station","role":"combo:admin,editor,viewer",)"
        R"("serves_local":"combo:yes,no","always_on":"combo:yes,no"},)"
        R"__("labels":{"username":"Username","color":"Colour","device":"Device","platform":"Platform",)__"
        R"__("serves_local":"Can serve a local address","always_on":"Always on",)__"
        R"__("fingerprint":"Key fingerprint","role":"Role","first_seen":"First seen",)__"
        R"__("last_opened":"Last opened this database"}}})__");
    core.register_glyph(
        R"({"glyph":"chamber_doc","label":"Document","kind":"entity",)"
        R"("fields":["kind","of","title","mount","last_published","published_to","gone"],)"
        R"("hints":{"color":"#2e6b4f","face":{"w":210,"h":56},"category":"Documents",)"
        R"("editors":{"kind":"combo:newsletter,website,calendar,map"},)"
        R"__("labels":{"kind":"Kind","of":"Its content (a Builder document, a calendar view or a map view)",)__"
        R"__("title":"Title","mount":"Mount path when published (/ or /events/)",)__"
        R"__("last_published":"Last published","published_to":"Published to",)__"
        R"__("gone":"Its content was removed"}}})__");
}

namespace detail {
inline std::string q(const std::string& s) {
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (c == '\n') { o += "\\n"; continue; }
        o += c;
    }
    return o + "\"";
}
inline std::string safe(std::string s) {
    for (char& c : s)
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) c = '-';
    while (s.find("--") != std::string::npos) s.replace(s.find("--"), 2, "-");
    return s;
}
} // namespace detail

/* The 64-hex content address inside a file name ("flier-<sha256>.jpg"), or "". */
inline std::string sha_in(const std::string& path) {
    int run = 0;
    for (std::size_t i = 0; i < path.size(); ++i) {
        run = std::isxdigit((unsigned char)path[i]) ? run + 1 : 0;
        if (run == 64 && (i + 1 == path.size() || !std::isxdigit((unsigned char)path[i + 1])))
            return path.substr(i + 1 - 64, 64);
    }
    return "";
}

inline std::string media_of(const std::string& path) {
    std::string e = path.substr(path.find_last_of('.') == std::string::npos ? path.size() : path.find_last_of('.'));
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    if (e == ".jpg" || e == ".jpeg") return "image/jpeg";
    if (e == ".png") return "image/png";
    if (e == ".gif") return "image/gif";
    if (e == ".webp") return "image/webp";
    if (e == ".svg") return "image/svg+xml";
    if (e == ".pdf") return "application/pdf";
    if (e == ".mp3") return "audio/mpeg";
    if (e == ".mp4") return "video/mp4";
    return "application/octet-stream";
}

/* An asset's rune name: from its content address when it has one, so the same
 * bytes are the same rune on every device; else from the file name. */
inline std::string asset_name(const std::string& file) {
    const std::string sha = sha_in(file);
    if (!sha.empty()) return "a-" + sha.substr(0, 16);
    std::string base = file.substr(file.find_last_of("/\\") == std::string::npos ? 0 : file.find_last_of("/\\") + 1);
    return "a-" + detail::safe(base);
}

struct FileFact {
    std::string file;  // relative to the database ("assets/cat-01.jpg") or as a rune names it
    long long bytes = -1;
    std::string added_by;
};

/* Commands (inside `assets`) that register every file not yet registered. A
 * file is known by its `file` field or by its content address. */
inline std::vector<std::string> register_assets(const maiz::Scene& assets, const std::vector<FileFact>& files) {
    std::set<std::string> known_files, known_names;
    for (const auto& n : assets.nodes) {
        known_names.insert(n.name);
        known_files.insert(field_value(n, "file"));
    }
    std::vector<std::string> out;
    for (const auto& f : files) {
        if (f.file.empty() || known_files.count(f.file)) continue;
        const std::string name = asset_name(f.file);
        if (known_names.count(name)) continue;
        known_names.insert(name);
        known_files.insert(f.file);
        out.push_back("rune new asset " + name);
        out.push_back("set " + name + " file " + detail::q(f.file));
        const std::string sha = sha_in(f.file);
        if (!sha.empty()) out.push_back("set " + name + " sha256 " + detail::q(sha));
        if (f.bytes >= 0) out.push_back("set " + name + " bytes " + detail::q(std::to_string(f.bytes)));
        out.push_back("set " + name + " media " + detail::q(media_of(f.file)));
        const auto slash = f.file.find_last_of("/\\");
        out.push_back("set " + name + " original " + detail::q(slash == std::string::npos ? f.file : f.file.substr(slash + 1)));
        if (!f.added_by.empty()) out.push_back("set " + name + " added_by " + detail::q(f.added_by));
        out.push_back("tag " + name + " +" + (media_of(f.file).rfind("image/", 0) == 0 ? "image" : "file"));
    }
    return out;
}

/* Every file a Data rune names: image paths and pictures (avatars, logos), the
 * fields whose editor is `image`. Returns rune → the file it names. */
inline std::vector<std::pair<std::string, std::string>> referenced_files(const maiz::Scene& data) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& n : data.nodes)
        for (const auto& f : n.fields) {
            if (f.editor != "image" && !(n.glyph == "image" && f.key == "path")) continue;
            const std::string v = field_value(n, f.key);
            if (v.empty() || v.rfind("http://", 0) == 0 || v.rfind("https://", 0) == 0) continue;
            out.push_back({n.name, v});
        }
    return out;
}

/* ── the Network chamber: this device, about itself ────────────────────── */
struct Self {
    std::string fingerprint; // hex, from the profile's public key
    std::string username, color, device, platform;
    bool serves_local = true;
    std::string today;       // YYYY-MM-DD
};

inline std::string profile_name(const Self& s) {
    return "p-" + (s.fingerprint.empty() ? detail::safe(s.username.empty() ? "device" : s.username)
                                         : s.fingerprint.substr(0, 12));
}

/* Upsert this device's own profile rune. Writes only what changed, so an
 * unchanged boot writes nothing (a boot that always wrote would be a sync and an
 * undo frame every time somebody opened the database). */
inline std::vector<std::string> upsert_self(const maiz::Scene& network, const Self& s) {
    const std::string name = profile_name(s);
    const maiz::SceneNode* n = network.find(name);
    std::vector<std::string> out;
    if (!n) {
        out.push_back("rune new profile " + name);
        out.push_back("set " + name + " first_seen " + detail::q(s.today));
    }
    auto want = [&](const char* key, const std::string& v) {
        if (v.empty() || (n && field_value(*n, key) == v)) return;
        out.push_back("set " + name + " " + key + " " + detail::q(v));
    };
    want("username", s.username);
    want("color", s.color);
    want("device", s.device);
    want("platform", s.platform);
    want("serves_local", s.serves_local ? "yes" : "no");
    want("fingerprint", s.fingerprint);
    if (!out.empty() || !n || field_value(*n, "last_opened") != s.today) want("last_opened", s.today);
    return out;
}

/* ── the Documents chamber: one entry per presentation of the database ── */
struct DocFact {
    std::string of;    // the content: a Builder mantle, a calview rune, a map rune
    std::string kind;  // newsletter | website | calendar | map
    std::string title;
};

inline std::string doc_name(const DocFact& d) { return "doc-" + detail::safe(d.of); }

inline std::vector<std::string> reconcile_documents(const maiz::Scene& docs, const std::vector<DocFact>& live) {
    std::vector<std::string> out;
    std::set<std::string> alive;
    for (const auto& d : live) {
        const std::string name = doc_name(d);
        alive.insert(name);
        const maiz::SceneNode* n = docs.find(name);
        if (!n) {
            out.push_back("rune new chamber_doc " + name);
            out.push_back("set " + name + " of " + detail::q(d.of));
            out.push_back("set " + name + " kind " + detail::q(d.kind));
            if (!d.title.empty()) out.push_back("set " + name + " title " + detail::q(d.title));
            out.push_back("set " + name + " mount " +
                          detail::q(d.kind == "website" ? "/" : "/" + detail::safe(d.of) + "/"));
            continue;
        }
        if (field_value(*n, "kind") != d.kind) out.push_back("set " + name + " kind " + detail::q(d.kind));
        if (!d.title.empty() && field_value(*n, "title") != d.title)
            out.push_back("set " + name + " title " + detail::q(d.title));
        if (!field_value(*n, "gone").empty()) out.push_back("set " + name + " gone " + detail::q(""));
    }
    /* A document whose content is gone is MARKED, not deleted: its publish
     * history and mount are the organization's record, and a device that has
     * not synced the new document yet must not delete another member's work. */
    for (const auto& n : docs.nodes)
        if (n.glyph == "chamber_doc" && !alive.count(n.name) && field_value(n, "gone").empty())
            out.push_back("set " + n.name + " gone " + detail::q("yes"));
    return out;
}

} // namespace hormiga::chambers
