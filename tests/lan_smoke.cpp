/* lan_smoke.cpp — the parts of LAN sharing that must hold without a network.
 *
 * okf/concepts/platform/lan-sharing.md. The two-process run (a host and a joiner
 * on loopback) is the end-to-end proof and is recorded in okf/log.md; this pins
 * the pieces that are easy to break quietly:
 *
 *   - a file name from the other device is refused unless it is plainly relative
 *   - presence opens only with the right room key, and a tampered body is refused
 *   - an offer survives the beacon round trip and fits one datagram
 *   - no two present people are shown in the same colour, whatever they prefer
 */
#include "app/lan_wire.hpp"
#include "domain/collab.hpp"
#include "sync/peer.hpp"

#include <cstdio>
#include <set>

static int failures = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

int main() {
    using namespace hormiga;

    // ── names that arrive from someone else ──────────────────────────────────
    CHECK(lan::safe_rel("LON.miga"));
    CHECK(lan::safe_rel("keys/cloudflare_token.txt"));
    CHECK(lan::safe_rel("Llaves de José.txt"));
    CHECK(!lan::safe_rel(""));
    CHECK(!lan::safe_rel("../outside.txt"));
    CHECK(!lan::safe_rel("a/../../b"));
    CHECK(!lan::safe_rel("/etc/passwd"));
    CHECK(!lan::safe_rel("C:/Windows/x"));
    CHECK(!lan::safe_rel("C:\\Windows\\x"));
    CHECK(!lan::safe_rel("a//b"));
    CHECK(!lan::safe_rel(".bashrc"));
    CHECK(!lan::safe_rel("new\nline"));
    CHECK(lan::file_stem("Cat Colony") == "Cat-Colony");
    CHECK(lan::file_stem("../../x") == "x");

    // ── presence is sealed to the room ───────────────────────────────────────
    const std::string key = lan::new_room_key(), other = lan::new_room_key();
    CHECK(key.size() == 32 && other.size() == 32 && key != other);
    CHECK(lan::room_id(key).size() == 12 && lan::room_id(key) != lan::room_id(other));
    lan::Activity a;
    a.fingerprint = "0123456789abcdef";
    a.user = "cool_username_123";
    a.color = "#3c8fd6";
    a.section = "Data";
    a.mantle = "demo-org";
    a.selection = {"maria", "flyer-taller"};
    const std::string sealed = lan::seal_activity(a, key);
    CHECK(!sealed.empty());
    CHECK(sealed.find("cool_username_123") == std::string::npos);  // not in the clear
    lan::Activity back;
    CHECK(lan::open_activity(sealed, key, back));
    CHECK(back.user == a.user && back.selection == a.selection && back.mantle == a.mantle);
    CHECK(!lan::open_activity(sealed, other, back));
    std::string tampered = sealed;
    tampered[tampered.size() / 2] ^= 0x01;
    CHECK(!lan::open_activity(tampered, key, back));

    // ── the beacon carries an offer and presence in one datagram ─────────────
    lan::Offer o;
    o.db = "Cat Colony";
    o.description = std::string(400, 'd');  // too long on purpose
    o.user = "host_person";
    o.color = "#e0555a";
    o.port = collab::kShareStreamPort;
    sync::PeerInfo pi;
    pi.peer_id = "hz0123456789abcdef";
    pi.display = "host_person";
    pi.fingerprint = "0123456789abcdef";
    pi.extra = lan::beacon_extra(&o, lan::room_id(key), sealed);
    CHECK(!pi.extra.empty());
    const std::string datagram = sync::encode_beacon(pi);
    CHECK(datagram.size() <= sync::kMaxBeacon);
    sync::PeerInfo got;
    CHECK(sync::decode_beacon(datagram, got));
    lan::ExtraParts parts;
    CHECK(lan::read_extra(got.extra, parts));
    CHECK(parts.has_offer && parts.offer.db == "Cat Colony" && parts.offer.port == collab::kShareStreamPort);
    CHECK(parts.room == lan::room_id(key));
    CHECK(lan::open_activity(parts.sealed, key, back) && back.user == a.user);
    CHECK(!lan::read_extra("not base64 at all", parts));

    // ── colours: preferences honoured until they clash ───────────────────────
    std::vector<collab::Present> people = {
        {"aaa", "#3c8fd6", "2026-09-01"},
        {"bbb", "#3c8fd6", "2026-09-02"},  // same preference, joined later
        {"ccc", "#3d8ed7", "2026-09-03"},  // nearly the same
        {"ddd", "", ""},
    };
    auto shown = collab::assign_colors(people);
    CHECK(shown["aaa"] == "#3c8fd6");  // first to join keeps it
    std::set<std::string> distinct;
    for (const auto& [k, c] : shown) distinct.insert(c);
    CHECK(distinct.size() == people.size());
    for (int i = 0; i < 30; ++i) people.push_back({"p" + std::to_string(i), "#e0555a", ""});
    shown = collab::assign_colors(people);
    distinct.clear();
    for (const auto& [k, c] : shown) distinct.insert(c);
    CHECK(distinct.size() == people.size());
    CHECK(collab::assign_colors(people) == shown);  // deterministic

    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lan smoke: ok\n");
    return 0;
}
