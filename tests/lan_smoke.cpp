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
#include "sync/replica.hpp"

#include "json.hpp"
#include "voidmaiz/embed.hpp"

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
    a.ids = {"rune_9fa3c1b7e2", "rune_00000000aa"};  // matched on these, not names
    const std::string sealed = lan::seal_activity(a, key);
    CHECK(!sealed.empty());
    CHECK(sealed.find("cool_username_123") == std::string::npos);  // not in the clear
    lan::Activity back;
    CHECK(lan::open_activity(sealed, key, back));
    CHECK(back.user == a.user && back.selection == a.selection && back.mantle == a.mantle);
    CHECK(back.ids == a.ids);
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

    // ── replicas: deletions propagate (Palabra's answer, 2026-09-16) ──────────
    {
        auto spirit_id = [](const std::string& state, const std::string& name) {
            const auto j = nlohmann::json::parse(state);
            for (const auto& m : j["mantles"])
                for (const auto& r : m["runes"])
                    if (r["spirit"]["name"] == name) return r["spirit"]["id"].get<std::string>();
            return std::string();
        };
        auto has = [&](const std::string& state, const std::string& name) { return !spirit_id(state, name).empty(); };
        // a bare Core knows no glyphs; `note` is declared the way the app declares it
        const char* kNote = R"({"glyph":"note","label":"Note","fields":["text"]})";
        auto edit = [kNote](const std::string& state, std::initializer_list<const char*> cmds) {
            maiz::Core c(state);
            c.register_glyph(kNote);
            for (const char* cmd : cmds) c.dispatch(cmd);
            return c.export_state();
        };
        maiz::Core seed;
        seed.register_glyph(kNote);
        for (const char* cmd : {"mantle new m", "rune new note keep-me", "rune new note delete-me"})
            seed.dispatch(cmd);
        const std::string s0 = seed.export_state();

        sync::SharedReplica a, b;
        std::string why;
        CHECK(a.open("", "device-aaaaaaaaaaaaaaaa", &why));
        CHECK(b.open("", "device-bbbbbbbbbbbbbbbb", &why));
        std::size_t n = 0;
        CHECK(a.observe(s0, &n) && n > 0);
        CHECK(b.observe(s0, &n) && n > 0);
        // first contact: each takes the other's document
        CHECK(a.merge(b.doc_json(), &why) == sync::SharedReplica::Merge::ok);
        CHECK(b.merge(a.doc_json(), &why) == sync::SharedReplica::Merge::ok);
        std::string sa = a.splice_into(s0, {}), sb = b.splice_into(s0, {});
        CHECK(a.shown_version() == b.shown_version());
        CHECK(a.observe(sa, &n) && n == 0);  // observing its own output records nothing

        // A deletes a rune; after one exchange it is gone on B too
        sa = edit(sa, {"use m", "rune rm delete-me"});
        CHECK(!has(sa, "delete-me"));
        CHECK(a.observe(sa, &n) && n > 0);
        CHECK(b.merge(a.doc_json(), &why) == sync::SharedReplica::Merge::ok);
        sb = b.splice_into(sb, {});
        CHECK(!has(sb, "delete-me"));  // the whole point: it did not come back
        CHECK(has(sb, "keep-me"));
        CHECK(a.merge(b.doc_json(), &why) == sync::SharedReplica::Merge::ok);
        CHECK(!has(a.splice_into(sa, {}), "delete-me"));
        // does a replica document still carry a deleted rune's content? (decides
        // whether a join may hand the host's replica over: lan-sharing.md §3b)
        std::fprintf(stderr, "PROBE deleted name still in doc: %s\n",
                     a.doc_json().find("delete-me") != std::string::npos ? "yes" : "no");

        // a private rune is never observed, and the splice puts it back
        std::string sp = edit(sa, {"use m", "rune new note my-secret", "tag my-secret +private"});
        const std::string secret_id = spirit_id(sp, "my-secret");
        CHECK(!secret_id.empty());
        CHECK(a.doc_json().find("my-secret") == std::string::npos);
        const std::string back = a.splice_into(sp, {{"m", secret_id}});
        CHECK(has(back, "my-secret") && has(back, "keep-me"));

        // a copied replica is caught, and forking recovers
        sync::SharedReplica copy;
        CHECK(copy.open(a.to_bytes(), "unused-id-cccccccccccc", &why));
        const std::string sc = edit(sa, {"use m", "rune new note from-the-copy"});
        CHECK(copy.observe(sc, &n) && n > 0);
        CHECK(a.merge(copy.doc_json(), &why) == sync::SharedReplica::Merge::identity_collision);
        CHECK(copy.fork("device-dddddddddddddddd", &why));
    }

    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("lan smoke: ok\n");
    return 0;
}
