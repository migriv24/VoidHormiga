/* app/lan_radio.cpp — members kept in sync over the phone's radios.
 *
 * The author, 2026-10-05: phones in the field must share with no Wi-Fi network
 * in common, and not through a hotspot ("a big waste of data plans and money
 * ... not everyone can easily do hotspot"). So: Wi-Fi Direct between Android
 * phones, and Bluetooth LE ("substantially slower (so a lot more progress bars
 * and such will be needed), however, it would be compatible with an iOS device
 * and android device in the future"), with Reticulum on top.
 *
 * THE LAYERS, and whose each is:
 *   Void Maiz radio.hpp      bytes between nearby devices (BLE GATT, Wi-Fi Direct)
 *   Void Maiz rnsradio.hpp   those bytes as Reticulum interfaces (HDLC pipes, UDP)
 *   Void Palabra Node        Reticulum: announces, encrypted links, Resources
 *   THIS FILE                who is a member, and handing Network's frames over
 *   app/lan_net.cpp          the same `maiz::Network` a LAN link feeds
 *
 * A RADIO LINK IS A LINK. Its `link_io` entry is keyed by the member's
 * fingerprint exactly as a LAN link is, so `Network` cannot tell them apart and
 * a member is carried by one or the other, never both: whichever is up first
 * carries; a radio link that finds the LAN already carrying waits, proven and
 * idle, and takes over the moment the LAN link goes.
 *
 * WHO IS A MEMBER is the room key's, as on the LAN (lan_net.cpp, point 4):
 *   - discovery: each device advertises `room6 + self4` (12 + 8 hex), where
 *     room6 is a keyed hash of the room key nobody outside can compute or
 *     reverse, and self4 is random per run. A device connects only to a peer
 *     whose room6 matches, and only the side with the lower self4 dials (two
 *     dialers would make two streams);
 *   - announces carry {room, fp, user}; links are opened only to members of
 *     the registry, by the lower fingerprint (net_links' rule);
 *   - a link carries no frame until both sides have proven the room key:
 *     "HZP1", then crypto_auth(room key, "hormiga-rns-member|<link>|<fp>"). The
 *     link id is fresh per link, so a proof cannot be replayed onto another.
 * Reticulum encrypts the link itself (forward secret); the proof is ours.
 *
 * ON A DESKTOP there is no radio, so HORMIGA_SIM_RADIO installs Void Maiz's
 * loopback stand-in ("listen:<port>" or "connect:<port>", throttled like LE),
 * which is how two processes on one computer test everything above the radio.
 */
#include "app/lan_internal.hpp"

#include "voidmaiz/radio.hpp"

#if defined(HORMIGA_HAVE_NET) && defined(VOIDMAIZ_RETICULUM)

#include "voidmaiz/rnsradio.hpp"
#include "voidpalabra/reticulum.hpp"

#include <sodium.h>

#include <cstdlib>
#include <deque>

// stb_image_write's deflate, compiled once in app/app.cpp (STB_IMAGE_WRITE_IMPLEMENTATION)
extern "C" unsigned char* stbi_zlib_compress(unsigned char* data, int data_len, int* out_len, int quality);

namespace fs = std::filesystem;
namespace rns = voidpalabra::reticulum;
using namespace lan_detail;

namespace {

std::string hex(const unsigned char* p, std::size_t n) {
    static const char* d = "0123456789abcdef";
    std::string o;
    for (std::size_t i = 0; i < n; ++i) {
        o += d[p[i] >> 4];
        o += d[p[i] & 15];
    }
    return o;
}

/* The room's discovery tag: 6 bytes nobody without the key can compute. A
 * different context from room_id (the LAN beacon's), so the two never match. */
std::string room6(const std::string& room_key) {
    if (sodium_init() < 0 || room_key.size() != 32) return {};
    unsigned char h[6];
    static const char ctx[] = "hormiga-radio-room";
    crypto_generichash(h, sizeof h, (const unsigned char*)room_key.data(), room_key.size(),
                       (const unsigned char*)ctx, sizeof ctx - 1);
    return hex(h, sizeof h);
}

std::string proof_mac(const std::string& room_key, const std::string& link, const std::string& fp) {
    static_assert(crypto_auth_KEYBYTES == 32, "the room key is the auth key");
    const std::string msg = "hormiga-rns-member|" + link + "|" + fp;
    unsigned char mac[crypto_auth_BYTES];
    crypto_auth(mac, (const unsigned char*)msg.data(), msg.size(), (const unsigned char*)room_key.data());
    return hex(mac, sizeof mac);
}

/* ── A FRAME ON A RADIO IS COMPRESSED (2026-10-05) ─────────────────────────
 * Palabra's session sends a member's shareable state WHOLE on every new link
 * (sync.hpp, Kind::doc): for the Cat Colony a one-field edit is a 600 KB frame.
 * On a LAN nobody notices; on Bluetooth LE at ~6 KB/s it is a hundred seconds.
 * The state is JSON and deflates about 12:1 (599,720 -> 48,079 bytes), so each
 * frame goes as "Z" + zlib when that is smaller, else "R" + the frame. The
 * compressor is stb_image_write's (compiled once, in app/app.cpp), the
 * inflater stb_image's: nothing new is vendored. Deltas instead of whole
 * states are Palabra's to decide (okf/developer_questions.md). */
std::string pack_frame(const std::string& frame) {
    if (frame.size() > 512) {
        int n = 0;
        unsigned char* z = stbi_zlib_compress((unsigned char*)frame.data(), (int)frame.size(), &n, 6);
        if (z) {
            std::string out;
            if ((std::size_t)n + 1 < frame.size()) out = "Z" + std::string((const char*)z, (std::size_t)n);
            std::free(z);
            if (!out.empty()) return out;
        }
    }
    return "R" + frame;
}

bool unpack_frame(const std::string& bytes, std::string& frame) {
    if (bytes.empty()) return false;
    if (bytes[0] == 'R') {
        frame = bytes.substr(1);
        return true;
    }
    if (bytes[0] != 'Z') return false;
    int n = 0;
    char* p = stbi_zlib_decode_malloc(bytes.data() + 1, (int)bytes.size() - 1, &n);
    if (!p) return false;
    const bool ok = n >= 0 && (std::size_t)n <= 64u * 1024 * 1024; // Palabra's own frame ceiling
    if (ok) frame.assign(p, (std::size_t)n);
    std::free(p);
    return ok;
}

std::vector<std::string> split_tabs(const std::string& s) {
    std::vector<std::string> out(1);
    for (char c : s) {
        if (c == '\t') out.emplace_back();
        else out.back() += c;
    }
    return out;
}

/* HORMIGA_SIM_RADIO=listen:<port> | connect:<port>[,<bytes per second>] */
std::unique_ptr<maiz::RadioPlatform> sim_radio(const std::string& name) {
    const char* e = std::getenv("HORMIGA_SIM_RADIO");
    if (!e || !*e) return nullptr;
    const std::string s = e;
    maiz::LoopbackRadioOptions o;
    o.name = name.empty() ? "desktop" : name;
    const auto colon = s.find(':');
    if (colon == std::string::npos) return nullptr;
    const int port = std::atoi(s.c_str() + colon + 1);
    if (s.compare(0, colon, "listen") == 0) o.listen_port = port;
    else o.connect_port = port;
    if (const auto comma = s.find(','); comma != std::string::npos) o.bytes_per_second = std::atof(s.c_str() + comma + 1);
    return maiz::loopback_radio(o);
}

} // namespace

struct LanRuntime::RadioState {
    maiz::RadioPlatform* radio = nullptr;
    std::unique_ptr<maiz::RadioBridge> bridge;
    bool on[2] = {false, false};
    bool asked[2] = {false, false}; // a prompt is up for it: switch it on when it is granted
    double asked_check = 0;
    bool node_up = false;
    std::string status;            // the last thing worth telling a person
    std::string self4, tag, room;  // this run's discovery tag; the room it was made for
    std::string announced;         // the announce data last set
    double next_announce = 0;
    std::size_t pipes_seen = 0;
    bool group_seen = false;

    std::map<std::string, RadioNear> near;           // radio peer -> what we know
    std::map<std::string, double> dialled;           // radio peer -> when we last asked to connect
    std::map<std::string, std::string> dest_fp;      // Reticulum destination -> member fingerprint
    std::map<std::string, double> opened;            // destination -> when we last opened a link
    struct Link {
        std::string fp, user, color;
        bool proven = false;   // they proved the room key
        bool carrying = false; // their link_io entry is ours
        double since = 0, proof_at = 0;
        std::deque<std::string> out; // frames Node::send refused (its queue was full)
    };
    std::map<std::string, Link> links;               // Reticulum link -> member
    std::map<std::string, double> transfer_started;  // link -> when its Resource began
};

namespace {

LanRuntime::RadioState& state(LanRuntime& rt) {
    if (!rt.radio) rt.radio = std::make_shared<LanRuntime::RadioState>();
    return *rt.radio;
}

maiz::RadioPlatform* platform(LanRuntime& rt) {
    if (!maiz::radio()) {
        if (auto sim = sim_radio(rt.me.username)) maiz::install_radio(std::move(sim));
    }
    return maiz::radio();
}

bool node_up(LanRuntime& rt, LanRuntime::RadioState& rs) {
    if (rs.node_up) return true;
    rns::Node& node = rns::Node::instance();
    if (!node.started()) {
        rns::Options o;
        o.storage_dir = (hormiga::profile::dir() / "rns").string();
        o.app_name = "hormiga";
        o.aspects = "members";
        std::string why;
        if (!node.start(o, &why)) {
            rs.status = "Reticulum would not start: " + why;
            return false;
        }
    }
    (void)rt;
    rs.node_up = true;
    return true;
}

} // namespace

bool LanRuntime::radio_available() {
    return maiz::radio() != nullptr || std::getenv("HORMIGA_SIM_RADIO") != nullptr;
}

int LanRuntime::radio_access(HormigaApp& app, int kind) {
    maiz::RadioPlatform* r = platform(of(app));
    return (int)(r ? r->access((maiz::RadioKind)kind) : maiz::RadioAccess::Unavailable);
}

bool LanRuntime::radio_on(HormigaApp& app, int kind) {
    LanRuntime& rt = of(app);
    return rt.radio && kind >= 0 && kind < 2 && rt.radio->on[kind];
}

bool LanRuntime::radio_switch(HormigaApp& app, int kind, bool on, std::string* why) {
    LanRuntime& rt = of(app);
    RadioState& rs = state(rt);
    maiz::RadioPlatform* r = platform(rt);
    auto fail = [&](const std::string& s) {
        rs.status = s;
        if (why) *why = s;
        return false;
    };
    if (kind < 0 || kind > 1) return fail("no such radio");
    const auto k = (maiz::RadioKind)kind;
    if (!on) {
        if (r && rs.on[kind]) r->stop(k);
        rs.on[kind] = false;
        rs.asked[kind] = false;
        if (!rs.on[0] && !rs.on[1]) {
            if (rs.bridge) rs.bridge->clear();
            rns::Node& node = rns::Node::instance();
            for (const auto& [link, l] : rs.links) node.close(link);
            rs.near.clear();
        }
        rs.status = kind == kRadioBle ? "Bluetooth is off for Hormiga" : "Wi-Fi Direct is off for Hormiga";
        app.log.push_back({"info", "share", rs.status});
        return true;
    }
    if (!r) return fail("this device has no radio Hormiga can use");
    if (rt.room_key.empty()) return fail("this database is not shared yet: there is no room to find members of");
    const auto access = r->access(k);
    if (access == maiz::RadioAccess::Unavailable) return fail(kind == kRadioBle ? "this device has no Bluetooth LE"
                                                                               : "this device has no Wi-Fi Direct");
    if (access != maiz::RadioAccess::Ready) {
        // the system's prompt, or "turn it on"; radio_tick finishes the switch once it is granted
        rs.asked[kind] = access != maiz::RadioAccess::Denied && r->request(k);
        if (!rs.asked[kind] && access != maiz::RadioAccess::Denied) return fail("the system did not ask; try again");
        return fail(access == maiz::RadioAccess::Denied ? "permission was refused: allow Nearby devices in the "
                                                          "system's settings for Hormiga"
                                                        : "asking for permission");
    }
    if (!node_up(rt, rs)) return fail(rs.status);
    if (rs.self4.empty()) {
        unsigned char b[4];
        randombytes_buf(b, sizeof b);
        rs.self4 = hex(b, sizeof b);
    }
    rs.room = room6(rt.room_key);
    rs.tag = rs.room + rs.self4;
    if (!rs.bridge) rs.bridge = std::make_unique<maiz::RadioBridge>(*r);
    rs.radio = r;
    if (!r->start(k, rs.tag)) return fail("the radio would not start");
    rs.on[kind] = true;
    rs.asked[kind] = false;
    rs.status = kind == kRadioBle ? "Looking for members over Bluetooth" : "Looking for members over Wi-Fi Direct";
    app.log.push_back({"info", "share", rs.status});
    return true;
}

std::vector<LanRuntime::RadioNear> LanRuntime::radio_near(HormigaApp& app) {
    LanRuntime& rt = of(app);
    std::vector<RadioNear> out;
    if (!rt.radio) return out;
    for (const auto& [peer, n] : rt.radio->near) out.push_back(n);
    return out;
}

std::string LanRuntime::radio_status(HormigaApp& app) {
    LanRuntime& rt = of(app);
    return rt.radio ? rt.radio->status : std::string();
}

void LanRuntime::radio_tick(HormigaApp& app, double now) {
    LanRuntime& rt = of(app);
    if (!rt.radio) return;
    if ((rt.radio->asked[0] || rt.radio->asked[1]) && now_seconds() - rt.radio->asked_check > 1.0) {
        rt.radio->asked_check = now_seconds(); // the prompt answered? (once a second: it is a JNI call)
        for (int k = 0; k < 2; ++k) {
            if (!rt.radio->asked[k] || !maiz::radio()) continue;
            const auto a = maiz::radio()->access((maiz::RadioKind)k);
            if (a == maiz::RadioAccess::Ready) radio_switch(app, k, true, nullptr);
            else if (a == maiz::RadioAccess::Denied) {
                rt.radio->asked[k] = false;
                rt.radio->status = "permission was refused: allow Nearby devices in the system's settings for Hormiga";
            }
        }
    }
    if (!rt.radio->radio || !rt.radio->node_up) return;
    RadioState& rs = *rt.radio;
    if (!rs.on[0] && !rs.on[1]) return;
    maiz::RadioPlatform& radio = *rs.radio;
    rns::Node& node = rns::Node::instance();
    const std::string mine = fingerprint(rt);
    const double wall = now_seconds();
    static const bool trace = std::getenv("HORMIGA_SYNC_TRACE") != nullptr; // a harness reads it

    if (room6(rt.room_key) != rs.room) { // another database opened: start over in its room
        for (int k = 0; k < 2; ++k)
            if (rs.on[k]) {
                radio.stop((maiz::RadioKind)k);
                rs.on[k] = false;
                radio_switch(app, k, true, nullptr);
            }
        if (!rs.on[0] && !rs.on[1]) return;
    }

    // ── the radio: who is near, and whom to connect ──────────────────────────
    std::vector<maiz::RadioEvent> rev;
    radio.poll(rev);
    for (const auto& e : rev) {
        auto& n = rs.near[e.peer];
        n.peer = e.peer;
        n.kind = (int)e.kind;
        if (!e.name.empty()) n.name = e.name;
        if (e.rssi) n.rssi = e.rssi;
        n.seen = wall;
        using T = maiz::RadioEvent::Type;
        switch (e.type) {
        case T::found:
            n.member = e.tag.size() >= 12 && e.tag.compare(0, 12, rs.room) == 0;
            if (n.member && !n.linked && e.tag.size() >= 20 && rs.self4 < e.tag.substr(12, 8) &&
                (!rs.dialled.count(e.peer) || wall - rs.dialled[e.peer] > 10.0)) {
                rs.dialled[e.peer] = wall;
                radio.connect(e.kind, e.peer);
            }
            break;
        case T::connected: n.linked = true; break;
        case T::disconnected: n.linked = false; break;
        case T::lost: rs.near.erase(e.peer); break;
        case T::group: rs.status = "A Wi-Fi Direct group formed (" + e.detail + ")"; break;
        case T::group_gone: rs.status = "The Wi-Fi Direct group ended"; break;
        case T::error: rs.status = e.detail; app.log.push_back({"warn", "share", "radio: " + e.detail}); break;
        default: break;
        }
    }
    for (auto it = rs.near.begin(); it != rs.near.end();) // a peer not heard for a minute is gone
        it = !it->second.linked && wall - it->second.seen > 60.0 ? rs.near.erase(it) : std::next(it);
    rs.bridge->handle(rev);

    // ── Reticulum ───────────────────────────────────────────────────────────
    node.loop();
    rs.bridge->pump();
    {
        nlohmann::json a = {{"room", rs.room}, {"fp", mine}, {"user", rt.me.username}};
        const std::string data = a.dump();
        if (data != rs.announced) {
            node.set_announce_data(data);
            rs.announced = data;
        }
    }
    const std::size_t pipes = rs.bridge->pipes().size();
    const bool group = !rs.bridge->group().empty();
    const bool fresh = pipes > rs.pipes_seen || (group && !rs.group_seen);
    rs.pipes_seen = pipes;
    rs.group_seen = group;
    if ((pipes || group) && (fresh || wall >= rs.next_announce)) {
        node.announce();
        rs.next_announce = wall + 5.0;
    }

    std::set<std::string> members;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        members = rt.member_fps;
    }
    auto send_proof = [&](const std::string& link) {
        const std::string msg = "HZP1\t" + mine + "\t" + rt.me.username + "\t" + rt.me.color + "\t" +
                                proof_mac(rt.room_key, link, mine);
        node.send(link, msg);
    };
    std::vector<rns::Event> ev;
    node.poll(ev);
    for (const auto& e : ev) {
        using T = rns::Event::Type;
        if (e.type == T::announce) {
            const auto j = nlohmann::json::parse(e.bytes, nullptr, false);
            if (!j.is_object() || j.value("room", "") != rs.room) continue;
            const std::string fp = j.value("fp", "");
            if (fp.empty() || fp == mine || !members.count(fp)) continue;
            rs.dest_fp[e.destination] = fp;
            bool have = false;
            for (const auto& [link, l] : rs.links) have = have || l.fp == fp;
            if (have || !(mine < fp) || (rs.opened.count(e.destination) && wall - rs.opened[e.destination] < 15.0))
                continue;
            rs.opened[e.destination] = wall;
            node.open(e.destination);
        } else if (e.type == T::link_established) {
            auto& l = rs.links[e.link];
            if (auto d = rs.dest_fp.find(e.destination); d != rs.dest_fp.end()) l.fp = d->second;
            l.since = l.proof_at = wall;
            send_proof(e.link);
        } else if (e.type == T::data) {
            auto it = rs.links.find(e.link);
            if (it == rs.links.end()) continue;
            auto& l = it->second;
            /* A PROOF IS ONE PACKET, and packets are fire-and-forget: each side
             * resends its own until it has the other's, and a proof arriving on a
             * link already proven means ours was lost -- send it again. */
            if (l.proven && e.bytes.rfind("HZP1\t", 0) == 0) {
                send_proof(e.link);
                continue;
            }
            if (!l.proven) {
                const auto f = split_tabs(e.bytes);
                const bool ok = f.size() == 5 && f[0] == "HZP1" && f[1] != mine && members.count(f[1]) &&
                                (l.fp.empty() || l.fp == f[1]) && f[4].size() == crypto_auth_BYTES * 2 &&
                                sodium_memcmp(f[4].data(), proof_mac(rt.room_key, e.link, f[1]).data(),
                                              f[4].size()) == 0;
                if (!ok) {
                    app.log.push_back({"warn", "share", "radio: a device that is not a member tried to sync; closed"});
                    node.close(e.link);
                    continue;
                }
                l.fp = f[1];
                l.user = f[2];
                l.color = f[3];
                l.proven = true;
                app.log.push_back({"info", "share", "radio: " + l.user + " is near, over " +
                                                        (rs.bridge->group().empty() ? "Bluetooth" : "Wi-Fi Direct")});
                continue;
            }
            if (!l.carrying) continue;
            std::string frame;
            if (!unpack_frame(e.bytes, frame)) {
                app.log.push_back({"warn", "share", "radio: a frame from " + l.user + " did not unpack; dropped"});
                continue;
            }
            if (trace)
                std::fprintf(stderr, "[radio] in %zu bytes (%zu on the air) from %s\n", frame.size(), e.bytes.size(),
                             l.user.c_str());
            std::lock_guard<std::mutex> lk(rt.mu);
            if (auto io = rt.link_io.find(l.fp); io != rt.link_io.end()) io->second.in.push_back(std::move(frame));
        } else if (e.type == T::link_closed) {
            auto it = rs.links.find(e.link);
            if (it == rs.links.end()) continue;
            if (it->second.carrying) {
                std::lock_guard<std::mutex> lk(rt.mu);
                rt.link_io.erase(it->second.fp);
            }
            if (it->second.proven) app.log.push_back({"info", "share", "radio: " + it->second.user + " went out of reach"});
            rs.links.erase(it);
        }
    }

    // ── frames: Network's, both ways, on the links we carry ──────────────────
    const std::int64_t epoch = (std::int64_t)std::time(nullptr);
    std::vector<std::string> unproven;
    for (auto& [link, l] : rs.links) {
        if (!l.proven) {
            if (wall - l.since > 30.0) unproven.push_back(link);
            else if (wall - l.proof_at > 4.0) {
                l.proof_at = wall;
                send_proof(link);
            }
            continue;
        }
        std::lock_guard<std::mutex> lk(rt.mu);
        auto io = rt.link_io.find(l.fp);
        if (!l.carrying && io == rt.link_io.end()) { // the LAN is not carrying them: we are
            rt.link_io[l.fp].carrying = true;
            l.carrying = true;
            rt.thread_log.push_back({"info", "sync: reached " + l.user + " over the radio, carrying frames"});
            io = rt.link_io.find(l.fp);
        } else if (l.carrying && io == rt.link_io.end()) {
            rt.link_io[l.fp].carrying = true; // closed under us (a new database): carry again
            io = rt.link_io.find(l.fp);
        }
        if (!l.carrying) continue;
        for (auto& f : io->second.out) l.out.push_back(pack_frame(f));
        io->second.out.clear();
        while (!l.out.empty()) {
            std::string why;
            if (!node.send(link, l.out.front(), &why)) {
                if (trace) std::fprintf(stderr, "[radio] held %zu bytes: %s\n", l.out.front().size(), why.c_str());
                break;
            }
            if (trace) std::fprintf(stderr, "[radio] out %zu bytes to %s\n", l.out.front().size(), l.user.c_str());
            l.out.pop_front();
        }
        // here, for the roster and the Migos screen, unless the LAN already says so
        auto& a = rt.present[l.fp];
        if (a.address.empty()) {
            a.fingerprint = l.fp;
            a.user = l.user;
            a.color = l.color;
            a.seen = epoch;
        }
    }

    for (const auto& link : unproven) node.close(link); // its link_closed tidies up

    // ── progress: Reticulum's Resources are the transfers worth a bar ───────
    std::set<std::string> moving;
    for (const auto& t : node.transfers()) {
        auto l = rs.links.find(t.link);
        if (l == rs.links.end() || !l->second.carrying) continue;
        moving.insert(t.link);
        if (!rs.transfer_started.count(t.link)) rs.transfer_started[t.link] = wall;
        std::lock_guard<std::mutex> lk(rt.mu);
        auto& x = rt.transfers[l->second.fp];
        x.what = rs.bridge->group().empty() ? "changes, over Bluetooth" : "changes, over Wi-Fi Direct";
        x.done = (long long)t.done;
        x.total = (long long)t.total;
        x.sending = !t.incoming;
        x.started = rs.transfer_started[t.link];
        x.finished = -1.0;
    }
    for (auto it = rs.transfer_started.begin(); it != rs.transfer_started.end();) {
        if (moving.count(it->first)) { ++it; continue; }
        if (auto l = rs.links.find(it->first); l != rs.links.end()) {
            std::lock_guard<std::mutex> lk(rt.mu);
            if (auto x = rt.transfers.find(l->second.fp); x != rt.transfers.end() && x->second.finished < 0) {
                x->second.done = x->second.total;
                x->second.finished = wall;
            }
        }
        it = rs.transfer_started.erase(it);
    }
    // the radio's strength, where the LAN's beacon count does not say it
    int rssi = 0; // the best linked member's; LE reports one per peer, not per Reticulum link
    for (const auto& [peer, n] : rs.near)
        if (n.linked && n.member && n.rssi && (!rssi || n.rssi > rssi)) rssi = n.rssi;
    for (const auto& [link, l] : rs.links)
        if (l.carrying && rssi && !rt.heard_mark.count(l.fp))
            rt.strength[l.fp] = std::clamp((float)(rssi + 100) / 50.0f, 0.0f, 1.0f); // -100..-50 dBm
    (void)now;
}

#else // no Reticulum in this build: the radios are absent, and say so

struct LanRuntime::RadioState {};
bool LanRuntime::radio_available() { return false; }
int LanRuntime::radio_access(HormigaApp&, int) { return (int)maiz::RadioAccess::Unavailable; }
bool LanRuntime::radio_on(HormigaApp&, int) { return false; }
bool LanRuntime::radio_switch(HormigaApp&, int, bool, std::string* why) {
    if (why) *why = "this build of Hormiga has no Reticulum";
    return false;
}
void LanRuntime::radio_tick(HormigaApp&, double) {}
std::vector<LanRuntime::RadioNear> LanRuntime::radio_near(HormigaApp&) { return {}; }
std::string LanRuntime::radio_status(HormigaApp&) { return {}; }

#endif
