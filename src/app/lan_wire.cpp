/* app/lan_wire.cpp — see lan_wire.hpp and okf/concepts/platform/lan-sharing.md. */
#include "app/lan_wire.hpp"

#include "sync/peer.hpp"

#include "json.hpp"
#include <sodium.h>

#include <cctype>

namespace hormiga::lan {

namespace {

std::string clip(const std::string& s, std::size_t n) {
    if (s.size() <= n) return s;
    std::string o = s.substr(0, n);
    while (!o.empty() && ((unsigned char)o.back() & 0xC0) == 0x80) o.pop_back();  // whole UTF-8
    if (!o.empty() && ((unsigned char)o.back() & 0x80)) o.pop_back();
    return o;
}

nlohmann::json parse(const std::string& s) {
    return nlohmann::json::parse(s, nullptr, false);
}

std::string str(const nlohmann::json& j, const char* k) {
    return j.is_object() && j.contains(k) && j[k].is_string() ? j[k].get<std::string>()
                                                              : std::string();
}

std::string dump(const nlohmann::json& j) {
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

}  // namespace

std::string b64(const std::string& raw) {
    if (sodium_init() < 0) return {};
    std::string out(sodium_base64_ENCODED_LEN(raw.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
    sodium_bin2base64(&out[0], out.size(), (const unsigned char*)raw.data(), raw.size(),
                      sodium_base64_VARIANT_ORIGINAL);
    out.resize(std::char_traits<char>::length(out.c_str()));
    return out;
}

std::string unb64(const std::string& text) {
    if (sodium_init() < 0 || text.empty()) return {};
    std::string out(text.size(), '\0');
    std::size_t len = 0;
    if (sodium_base642bin((unsigned char*)&out[0], out.size(), text.data(), text.size(), nullptr,
                          &len, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0)
        return {};
    out.resize(len);
    return out;
}

std::string new_room_key() {
    if (sodium_init() < 0) return {};
    std::string k(32, '\0');
    randombytes_buf(&k[0], k.size());
    return k;
}

std::string random_id(const char* prefix) {
    if (sodium_init() < 0) return {};
    unsigned char b[16];
    randombytes_buf(b, sizeof b);
    static const char* d = "0123456789abcdef";
    std::string o = prefix;
    for (unsigned char c : b) {
        o += d[c >> 4];
        o += d[c & 15];
    }
    return o;
}

std::string room_id(const std::string& room_key) {
    if (sodium_init() < 0 || room_key.size() != 32) return {};
    unsigned char h[6];
    static const char ctx[] = "hormiga-room-id";
    crypto_generichash(h, sizeof h, (const unsigned char*)room_key.data(), room_key.size(),
                       (const unsigned char*)ctx, sizeof ctx - 1);
    static const char* d = "0123456789abcdef";
    std::string o;
    for (unsigned char c : h) {
        o += d[c >> 4];
        o += d[c & 15];
    }
    return o;
}

std::string beacon_extra(const Offer* offer, const std::string& room,
                         const std::string& sealed_activity) {
    nlohmann::json j = nlohmann::json::object();
    j["p"] = kProtocol;
    if (offer) {
        j["o"] = {{"db", clip(offer->db, 60)},
                  {"d", clip(offer->description, 160)},
                  {"u", clip(offer->user, 40)},
                  {"c", clip(offer->color, 7)},
                  {"port", offer->port}};
    }
    if (!room.empty() && !sealed_activity.empty()) {
        j["r"] = room;
        j["s"] = b64(sealed_activity);
    }
    std::string e = b64(dump(j));
    /* One datagram. Presence first gives way (it is re-sent in three seconds),
     * then the description; the offer's name and port always fit. */
    const std::size_t budget = hormiga::sync::kMaxBeacon - 260;
    if (e.size() > budget && j.contains("s")) {
        j.erase("s");
        j.erase("r");
        e = b64(dump(j));
    }
    if (e.size() > budget && j.contains("o")) {
        j["o"]["d"] = "";
        e = b64(dump(j));
    }
    return e.size() > budget ? std::string() : e;
}

bool read_extra(const std::string& extra, ExtraParts& out) {
    out = ExtraParts{};
    if (extra.empty() || extra.size() > hormiga::sync::kMaxBeacon) return false;
    const nlohmann::json j = parse(unb64(extra));
    if (!j.is_object() || !j.contains("p") || !j["p"].is_number_integer() ||
        j["p"].get<int>() != kProtocol)
        return false;
    if (j.contains("o") && j["o"].is_object()) {
        const auto& o = j["o"];
        out.has_offer = true;
        out.offer.db = str(o, "db");
        out.offer.description = str(o, "d");
        out.offer.user = str(o, "u");
        out.offer.color = str(o, "c");
        out.offer.port = o.contains("port") && o["port"].is_number_integer() ? o["port"].get<int>() : 0;
        if (out.offer.port <= 0 || out.offer.port > 65535) out.has_offer = false;
    }
    out.room = str(j, "r");
    out.sealed = unb64(str(j, "s"));
    return true;
}

std::string seal_activity(const Activity& a, const std::string& room_key) {
    nlohmann::json j = {{"f", a.fingerprint}, {"u", clip(a.user, 40)}, {"c", clip(a.color, 7)},
                        {"t", clip(a.section, 24)}, {"m", clip(a.mantle, 48)},
                        {"v", clip(a.version, 80)}};
    nlohmann::json sel = nlohmann::json::array();
    for (std::size_t i = 0; i < a.selection.size() && i < 6; ++i) sel.push_back(clip(a.selection[i], 48));
    j["sel"] = sel;
    std::string sealed;
    if (!hormiga::sync::seal_blob(dump(j), room_key, sealed)) return {};
    return sealed;
}

bool open_activity(const std::string& sealed, const std::string& room_key, Activity& out) {
    std::string plain;
    if (sealed.empty() || !hormiga::sync::open_blob(sealed, room_key, plain)) return false;
    const nlohmann::json j = parse(plain);
    if (!j.is_object()) return false;
    Activity a;
    a.fingerprint = str(j, "f");
    a.user = str(j, "u");
    a.color = str(j, "c");
    a.section = str(j, "t");
    a.mantle = str(j, "m");
    a.version = str(j, "v");
    if (j.contains("sel") && j["sel"].is_array())
        for (const auto& s : j["sel"])
            if (s.is_string() && a.selection.size() < 6) a.selection.push_back(s.get<std::string>());
    if (a.fingerprint.empty()) return false;
    out = a;
    return true;
}

bool safe_rel(const std::string& rel) {
    if (rel.empty() || rel.size() > 240) return false;
    if (rel.front() == '/' || rel.front() == '.') return false;  // root, hidden, or a `..` start
    if (rel.find('\\') != std::string::npos || rel.find(':') != std::string::npos) return false;
    for (unsigned char c : rel)
        if (c < 0x20 || c == 0x7f) return false;
    std::size_t start = 0;
    while (start <= rel.size()) {
        const std::size_t end = rel.find('/', start);
        const std::string part = rel.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

std::string file_stem(const std::string& name) {
    std::string o;
    for (unsigned char c : name) {
        if (std::isalnum(c) || c == '-' || c == '_') o += (char)c;
        else if (c == ' ' || c == '.') o += '-';
        else if (c >= 0x80) o += (char)c;  // keep letters with accents
    }
    while (!o.empty() && o.front() == '-') o.erase(o.begin());
    while (!o.empty() && o.back() == '-') o.pop_back();
    return o.empty() ? std::string("shared-database") : clip(o, 60);
}

}  // namespace hormiga::lan
