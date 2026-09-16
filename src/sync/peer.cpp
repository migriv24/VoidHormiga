/* peer.cpp — the LAN transport's guts: a multicast beacon, an X25519
 * handshake with an out-of-band short authentication string, and a sealed
 * length-prefixed stream.
 *
 * See peer.hpp for why the beacon is hand-rolled and the crypto is not.
 */
#include "sync/peer.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define HZ_BAD_SOCKET INVALID_SOCKET
#define hz_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define HZ_BAD_SOCKET (-1)
#define hz_close ::close
#endif

#include <sodium.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

namespace hormiga::sync {
namespace {

constexpr char kMagic[4] = {'H', 'R', 'M', 'G'};
constexpr unsigned char kProtocol = 1;
constexpr std::size_t kChunk = 60 * 1024;   // plaintext bytes per sealed frame
constexpr std::size_t kMaxFrame = 1 << 20;  // refuse an absurd length prefix

std::int64_t now_s() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool sodium_ready() {
    static const bool ok = (sodium_init() >= 0);
    return ok;
}

/* Winsock needs a process-wide startup and the preview server may or may not
 * have done it. WSAStartup is reference-counted, so calling it once here from a
 * function-local static is correct regardless of who else called it. */
bool net_ready() {
#ifdef _WIN32
    static const bool ok = [] {
        WSADATA w;
        return WSAStartup(MAKEWORD(2, 2), &w) == 0;
    }();
    return ok;
#else
    return true;
#endif
}

std::string hex(const unsigned char* p, std::size_t n) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; i++) {
        out.push_back(d[p[i] >> 4]);
        out.push_back(d[p[i] & 15]);
    }
    return out;
}

/* --- a deliberately tiny JSON writer/reader for the beacon ---------------
 *
 * The beacon is a flat map of short strings on an untrusted UDP port. Pulling
 * a JSON library into a datagram parser that anything on the network can feed
 * is a larger attack surface than the format deserves, and the format is five
 * fields. Values are escaped for the two characters that can appear in a
 * device name a person typed. */
std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o.push_back('\\');
        if ((unsigned char)c < 0x20) continue;  // no control bytes in a beacon
        o.push_back(c);
    }
    return o;
}

/* Reads `"key":"value"` out of a flat object. Returns false if absent. */
bool field(const std::string& src, const std::string& key, std::string& out) {
    const std::string pat = "\"" + key + "\":\"";
    auto i = src.find(pat);
    if (i == std::string::npos) return false;
    i += pat.size();
    std::string v;
    while (i < src.size() && src[i] != '"') {
        if (src[i] == '\\' && i + 1 < src.size()) i++;
        v.push_back(src[i++]);
    }
    if (i >= src.size()) return false;  // unterminated: refuse, do not salvage
    out = v;
    return true;
}

}  // namespace

/* --- beacon payload ------------------------------------------------------ */

std::string encode_beacon(const PeerInfo& self) {
    std::string o = "{\"app\":\"voidhormiga\",\"p\":1";
    o += ",\"id\":\"" + esc(self.peer_id) + "\"";
    o += ",\"n\":\"" + esc(self.display) + "\"";
    o += ",\"v\":\"" + esc(self.version) + "\"";
    o += ",\"f\":\"" + esc(self.fingerprint) + "\"";
    if (!self.extra.empty()) o += ",\"x\":\"" + esc(self.extra) + "\"";
    o += ",\"port\":" + std::to_string(self.port) + "}";
    return o;
}

bool decode_beacon(const std::string& d, PeerInfo& out) {
    if (d.size() < 20 || d.size() > kMaxBeacon) return false;
    if (d.find("\"app\":\"voidhormiga\"") == std::string::npos) return false;
    if (d.find("\"p\":1") == std::string::npos) return false;  // our version only

    PeerInfo p;
    if (!field(d, "id", p.peer_id) || p.peer_id.empty()) return false;
    if (!field(d, "v", p.version)) return false;
    field(d, "n", p.display);
    field(d, "f", p.fingerprint);
    field(d, "x", p.extra);

    auto i = d.find("\"port\":");
    if (i == std::string::npos) return false;
    long port = std::strtol(d.c_str() + i + 7, nullptr, 10);
    if (port <= 0 || port > 65535) return false;
    p.port = (std::uint16_t)port;

    p.last_seen = now_s();
    out = p;
    return true;
}

/* --- the beacon ---------------------------------------------------------- */

struct Beacon::Impl {
    std::thread rx, tx;
    std::atomic<bool> stop{false};
    socket_t listen_sock = HZ_BAD_SOCKET;
    mutable std::mutex mu;
    std::map<std::string, PeerInfo> seen;  // peer_id -> latest
    std::string payload;                   // what we announce
    std::string self_id;
};

Beacon::Beacon() : p_(new Impl) {}
Beacon::~Beacon() {
    stop();
    delete p_;
}

bool Beacon::start(const PeerInfo& self, std::string* error) {
    if (running_) {
        announce(self);
        return true;
    }
    if (!net_ready()) {
        if (error) *error = "winsock unavailable";
        return false;
    }
    p_->payload = self.peer_id.empty() ? std::string() : encode_beacon(self);
    p_->self_id = self.peer_id;
    p_->stop = false;

    socket_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == HZ_BAD_SOCKET) {
        if (error) *error = "could not open a UDP socket";
        return false;
    }
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kBeaconPort);
    if (bind(s, (sockaddr*)&addr, sizeof addr) != 0) {
        hz_close(s);
        if (error) *error = "another process holds the beacon port";
        return false;
    }
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(kBeaconGroup);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof mreq) != 0) {
        hz_close(s);
        if (error) *error = "could not join the multicast group (no LAN interface?)";
        return false;
    }
    p_->listen_sock = s;

    p_->rx = std::thread([this] {
        char buf[kMaxBeacon + 1];
        while (!p_->stop) {
            sockaddr_in from{};
#ifdef _WIN32
            int flen = sizeof from;
#else
            socklen_t flen = sizeof from;
#endif
            int n = recvfrom(p_->listen_sock, buf, sizeof buf, 0, (sockaddr*)&from, &flen);
            if (n <= 0) break;  // closed on stop(), or a hard error
            PeerInfo peer;
            if (!decode_beacon(std::string(buf, (std::size_t)n), peer)) continue;
            /* OUR OWN BEACON COMES BACK TO US -- multicast loopback is on by
             * default and we want it on, because two databases on ONE machine
             * (the two-folder case) must be able to find each other. So the
             * filter is by peer id rather than by address. */
            if (peer.peer_id == p_->self_id) continue;
            char ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
            peer.address = ip;
            std::lock_guard<std::mutex> lk(p_->mu);
            p_->seen[peer.peer_id] = peer;
        }
    });

    p_->tx = std::thread([this] {
        socket_t o = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (o == HZ_BAD_SOCKET) return;
        unsigned char ttl = 1;  // this subnet only; never routed outward
        setsockopt(o, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof ttl);
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = inet_addr(kBeaconGroup);
        to.sin_port = htons(kBeaconPort);
        while (!p_->stop) {
            std::string msg;
            {
                std::lock_guard<std::mutex> lk(p_->mu);
                msg = p_->payload;
            }
            if (!msg.empty() && msg.size() <= kMaxBeacon)  // empty = listening only
                sendto(o, msg.data(), (int)msg.size(), 0, (sockaddr*)&to, sizeof to);
            for (int i = 0; i < 30 && !p_->stop; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        hz_close(o);
    });

    running_ = true;
    return true;
}

void Beacon::stop() {
    if (!running_) return;
    p_->stop = true;
    if (p_->listen_sock != HZ_BAD_SOCKET) {
        hz_close(p_->listen_sock);  // unblocks recvfrom
        p_->listen_sock = HZ_BAD_SOCKET;
    }
    if (p_->rx.joinable()) p_->rx.join();
    if (p_->tx.joinable()) p_->tx.join();
    running_ = false;
}

void Beacon::announce(const PeerInfo& self) {
    std::lock_guard<std::mutex> lk(p_->mu);
    p_->payload = self.peer_id.empty() ? std::string() : encode_beacon(self);
    p_->self_id = self.peer_id;
}

std::vector<PeerInfo> Beacon::peers(int stale_seconds) const {
    std::vector<PeerInfo> out;
    const std::int64_t cut = now_s() - stale_seconds;
    {
        std::lock_guard<std::mutex> lk(p_->mu);
        for (const auto& kv : p_->seen)
            if (kv.second.last_seen >= cut) out.push_back(kv.second);
    }
    std::sort(out.begin(), out.end(), [](const PeerInfo& a, const PeerInfo& b) {
        return a.display == b.display ? a.peer_id < b.peer_id : a.display < b.display;
    });
    return out;
}

/* --- identity ------------------------------------------------------------ */

KeyPair generate_keypair() {
    KeyPair kp;
    if (!sodium_ready()) return kp;
    unsigned char pk[crypto_kx_PUBLICKEYBYTES], sk[crypto_kx_SECRETKEYBYTES];
    crypto_kx_keypair(pk, sk);
    kp.public_key.assign((char*)pk, sizeof pk);
    kp.secret_key.assign((char*)sk, sizeof sk);
    sodium_memzero(sk, sizeof sk);
    return kp;
}

std::string fingerprint_of(const std::string& public_key) {
    if (!sodium_ready() || public_key.size() != crypto_kx_PUBLICKEYBYTES) return {};
    unsigned char h[8];
    crypto_generichash(h, sizeof h, (const unsigned char*)public_key.data(),
                       public_key.size(), nullptr, 0);
    return hex(h, sizeof h);
}

std::string short_auth_string(const std::string& pk_a, const std::string& pk_b) {
    if (!sodium_ready()) return {};
    if (pk_a.size() != crypto_kx_PUBLICKEYBYTES || pk_b.size() != crypto_kx_PUBLICKEYBYTES)
        return {};
    /* SORTED, so both ends compute the same string without negotiating who is
     * "first" -- the property that makes this comparable out of band at all. */
    const std::string& lo = (pk_a <= pk_b) ? pk_a : pk_b;
    const std::string& hi = (pk_a <= pk_b) ? pk_b : pk_a;
    std::string in = "voidhormiga-sas-v1";
    in += lo;
    in += hi;
    unsigned char h[16];
    crypto_generichash(h, sizeof h, (const unsigned char*)in.data(), in.size(), nullptr, 0);

    /* No 0/O/1/I/L: these get READ ALOUD by two people in a room, and a scheme
     * whose failure mode is "they said B and I heard V" is a scheme that
     * teaches users to shrug at mismatches. */
    static const char* abc = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
    const std::size_t n = 31;
    std::string sas;
    for (int i = 0; i < 6; i++) sas.push_back(abc[h[i] % n]);
    return sas;
}

/* --- sealed frames ------------------------------------------------------- */

namespace {

bool send_all(socket_t s, const char* p, std::size_t n) {
    while (n) {
        int w = ::send(s, p, (int)n, 0);
        if (w <= 0) return false;
        p += w;
        n -= (std::size_t)w;
    }
    return true;
}

bool recv_all(socket_t s, char* p, std::size_t n) {
    while (n) {
        int r = ::recv(s, p, (int)n, 0);
        if (r <= 0) return false;
        p += r;
        n -= (std::size_t)r;
    }
    return true;
}

bool send_frame(socket_t s, const std::string& b) {
    unsigned char len[4] = {(unsigned char)(b.size() >> 24), (unsigned char)(b.size() >> 16),
                            (unsigned char)(b.size() >> 8), (unsigned char)b.size()};
    return send_all(s, (const char*)len, 4) && send_all(s, b.data(), b.size());
}

bool recv_frame(socket_t s, std::string& out) {
    unsigned char len[4];
    if (!recv_all(s, (char*)len, 4)) return false;
    std::size_t n = ((std::size_t)len[0] << 24) | ((std::size_t)len[1] << 16) |
                    ((std::size_t)len[2] << 8) | len[3];
    if (n > kMaxFrame) return false;  // a length prefix is attacker-controlled
    out.resize(n);
    return n == 0 || recv_all(s, &out[0], n);
}

}  // namespace

bool seal_blob(const std::string& plain, const std::string& key32, std::string& out) {
    if (!sodium_ready() || key32.size() != crypto_secretstream_xchacha20poly1305_KEYBYTES)
        return false;
    crypto_secretstream_xchacha20poly1305_state st;
    unsigned char hdr[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(&st, hdr,
                                                    (const unsigned char*)key32.data());
    out.assign((char*)hdr, sizeof hdr);

    std::size_t off = 0;
    std::vector<unsigned char> ct(kChunk + crypto_secretstream_xchacha20poly1305_ABYTES);
    do {
        const std::size_t n = std::min(kChunk, plain.size() - off);
        const bool last = (off + n >= plain.size());
        unsigned long long clen = 0;
        crypto_secretstream_xchacha20poly1305_push(
            &st, ct.data(), &clen, (const unsigned char*)plain.data() + off, n, nullptr, 0,
            last ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0);
        /* Each chunk keeps its own 4-byte length, so `open_blob` can tell a
         * truncated stream from a complete one -- the whole reason this is a
         * secretstream and not a single-shot seal. */
        const std::size_t c = (std::size_t)clen;
        unsigned char len[4] = {(unsigned char)(c >> 24), (unsigned char)(c >> 16),
                                (unsigned char)(c >> 8), (unsigned char)c};
        out.append((const char*)len, 4);
        out.append((const char*)ct.data(), c);
        off += n;
    } while (off < plain.size());
    return true;
}

bool open_blob(const std::string& sealed, const std::string& key32, std::string& out,
               std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    if (!sodium_ready() || key32.size() != crypto_secretstream_xchacha20poly1305_KEYBYTES)
        return fail("bad key");
    const std::size_t H = crypto_secretstream_xchacha20poly1305_HEADERBYTES;
    if (sealed.size() < H) return fail("truncated: no header");

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(
            &st, (const unsigned char*)sealed.data(), (const unsigned char*)key32.data()) != 0)
        return fail("bad header");

    out.clear();
    std::size_t off = H;
    bool saw_final = false;
    std::vector<unsigned char> pt(kChunk + crypto_secretstream_xchacha20poly1305_ABYTES);
    while (off < sealed.size()) {
        if (off + 4 > sealed.size()) return fail("truncated: partial length");
        const unsigned char* L = (const unsigned char*)sealed.data() + off;
        const std::size_t c = ((std::size_t)L[0] << 24) | ((std::size_t)L[1] << 16) |
                              ((std::size_t)L[2] << 8) | L[3];
        off += 4;
        if (c > pt.size() || off + c > sealed.size()) return fail("truncated: partial chunk");
        unsigned long long mlen = 0;
        unsigned char tag = 0;
        if (crypto_secretstream_xchacha20poly1305_pull(
                &st, pt.data(), &mlen, &tag, (const unsigned char*)sealed.data() + off, c,
                nullptr, 0) != 0)
            return fail("wrong key or tampered ciphertext");
        out.append((const char*)pt.data(), (std::size_t)mlen);
        off += c;
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
            saw_final = true;
            break;
        }
    }
    /* THE POINT OF ALL THIS. A stream that decrypts cleanly but never reached
     * its FINAL tag was cut short, and returning its prefix as though it were
     * the message is precisely the quiet failure this design exists to refuse. */
    if (!saw_final) return fail("truncated: stream never terminated");
    return true;
}

/* --- session ------------------------------------------------------------- */

struct Session::Impl {
    socket_t sock = HZ_BAD_SOCKET;
    crypto_secretstream_xchacha20poly1305_state tx{}, rx{};
    bool ready = false;
};

Session::Session() : p_(new Impl) {}
Session::~Session() {
    close();
    delete p_;
}
bool Session::open() const { return p_ && p_->sock != HZ_BAD_SOCKET && p_->ready; }

void Session::close() {
    if (p_ && p_->sock != HZ_BAD_SOCKET) {
        hz_close(p_->sock);
        p_->sock = HZ_BAD_SOCKET;
    }
    if (p_) p_->ready = false;
}

namespace {

/* Exchange the plaintext greeting (magic, protocol byte, public key), derive
 * the two session keys, and start both secretstreams.
 *
 * `is_client` picks which crypto_kx side we are -- the two sides derive
 * MIRRORED rx/tx keys, and getting this backwards produces a channel that
 * encrypts fine and never decrypts, so it is stated rather than inferred. */
bool do_handshake(socket_t s, const KeyPair& self, bool is_client, std::string& peer_pk,
                  crypto_secretstream_xchacha20poly1305_state& tx,
                  crypto_secretstream_xchacha20poly1305_state& rx, std::string* error) {
    auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    if (self.public_key.size() != crypto_kx_PUBLICKEYBYTES ||
        self.secret_key.size() != crypto_kx_SECRETKEYBYTES)
        return fail("this peer has no usable keypair");

    std::string hello(kMagic, 4);
    hello.push_back((char)kProtocol);
    hello += self.public_key;
    if (!send_all(s, hello.data(), hello.size())) return fail("could not send the greeting");

    char in[5 + crypto_kx_PUBLICKEYBYTES];
    if (!recv_all(s, in, sizeof in)) return fail("peer closed during the greeting");
    if (std::memcmp(in, kMagic, 4) != 0) return fail("not a Hormiga peer");
    if ((unsigned char)in[4] != kProtocol) return fail("peer speaks a different sync protocol");
    peer_pk.assign(in + 5, crypto_kx_PUBLICKEYBYTES);

    unsigned char rxk[crypto_kx_SESSIONKEYBYTES], txk[crypto_kx_SESSIONKEYBYTES];
    const int r = is_client ? crypto_kx_client_session_keys(
                                  rxk, txk, (const unsigned char*)self.public_key.data(),
                                  (const unsigned char*)self.secret_key.data(),
                                  (const unsigned char*)peer_pk.data())
                            : crypto_kx_server_session_keys(
                                  rxk, txk, (const unsigned char*)self.public_key.data(),
                                  (const unsigned char*)self.secret_key.data(),
                                  (const unsigned char*)peer_pk.data());
    if (r != 0) return fail("key agreement refused the peer's public key");

    /* Headers cross before any payload does. */
    unsigned char myhdr[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(&tx, myhdr, txk);
    if (!send_all(s, (const char*)myhdr, sizeof myhdr)) return fail("could not send the header");
    unsigned char theirhdr[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    if (!recv_all(s, (char*)theirhdr, sizeof theirhdr)) return fail("no header from the peer");
    if (crypto_secretstream_xchacha20poly1305_init_pull(&rx, theirhdr, rxk) != 0)
        return fail("the peer's stream header was rejected");

    sodium_memzero(rxk, sizeof rxk);
    sodium_memzero(txk, sizeof txk);
    return true;
}

}  // namespace

bool Session::connect(const std::string& host, std::uint16_t port, const KeyPair& self,
                      std::string& peer_public_key, std::string& sas, std::string* error) {
    if (!sodium_ready() || !net_ready()) {
        if (error) *error = "crypto or winsock unavailable";
        return false;
    }
    close();
    socket_t s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == HZ_BAD_SOCKET) {
        if (error) *error = "could not open a socket";
        return false;
    }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) != 1) {
        hz_close(s);
        if (error) *error = "not an IPv4 address: " + host;
        return false;
    }
    if (::connect(s, (sockaddr*)&a, sizeof a) != 0) {
        hz_close(s);
        if (error) *error = "no answer at " + host + ":" + std::to_string(port);
        return false;
    }
    if (!do_handshake(s, self, true, peer_public_key, p_->tx, p_->rx, error)) {
        hz_close(s);
        return false;
    }
    p_->sock = s;
    p_->ready = true;
    sas = short_auth_string(self.public_key, peer_public_key);
    return true;
}

bool Session::accept_one(std::uint16_t port, const KeyPair& self, std::string& peer_public_key,
                         std::string& sas, int timeout_ms, std::string* error) {
    if (!sodium_ready() || !net_ready()) {
        if (error) *error = "crypto or winsock unavailable";
        return false;
    }
    close();
    socket_t l = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (l == HZ_BAD_SOCKET) {
        if (error) *error = "could not open a socket";
        return false;
    }
    int yes = 1;
    setsockopt(l, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(port);
    if (bind(l, (sockaddr*)&a, sizeof a) != 0 || listen(l, 1) != 0) {
        hz_close(l);
        if (error) *error = "could not listen on port " + std::to_string(port);
        return false;
    }
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(l, &fds);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select((int)l + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        hz_close(l);
        if (error) *error = "no peer connected before the timeout";
        return false;
    }
    socket_t c = accept(l, nullptr, nullptr);
    hz_close(l);
    if (c == HZ_BAD_SOCKET) {
        if (error) *error = "accept failed";
        return false;
    }
    if (!do_handshake(c, self, false, peer_public_key, p_->tx, p_->rx, error)) {
        hz_close(c);
        return false;
    }
    p_->sock = c;
    p_->ready = true;
    sas = short_auth_string(self.public_key, peer_public_key);
    return true;
}

bool Session::send(const std::string& payload, std::string* error) {
    if (!open()) {
        if (error) *error = "session is not open";
        return false;
    }
    std::size_t off = 0;
    std::vector<unsigned char> ct(kChunk + crypto_secretstream_xchacha20poly1305_ABYTES);
    do {
        const std::size_t n = std::min(kChunk, payload.size() - off);
        const bool last = (off + n >= payload.size());
        unsigned long long clen = 0;
        crypto_secretstream_xchacha20poly1305_push(
            &p_->tx, ct.data(), &clen, (const unsigned char*)payload.data() + off, n, nullptr,
            0, last ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0);
        if (!send_frame(p_->sock, std::string((const char*)ct.data(), (std::size_t)clen))) {
            if (error) *error = "the connection dropped mid-send";
            return false;
        }
        off += n;
    } while (off < payload.size());
    return true;
}

void Session::set_timeout_ms(int ms) {
    if (!p_ || p_->sock == HZ_BAD_SOCKET) return;
#ifdef _WIN32
    DWORD tv = (DWORD)(ms < 0 ? 0 : ms);
#else
    timeval tv{ms / 1000, (ms % 1000) * 1000};
#endif
    setsockopt(p_->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(p_->sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
}

bool Session::receive(std::string& payload, std::string* error, std::size_t max_bytes) {
    if (!open()) {
        if (error) *error = "session is not open";
        return false;
    }
    payload.clear();
    std::vector<unsigned char> pt(kChunk + crypto_secretstream_xchacha20poly1305_ABYTES);
    for (;;) {
        std::string frame;
        if (!recv_frame(p_->sock, frame)) {
            if (error) *error = "the connection dropped mid-receive";
            return false;
        }
        unsigned long long mlen = 0;
        unsigned char tag = 0;
        if (crypto_secretstream_xchacha20poly1305_pull(
                &p_->rx, pt.data(), &mlen, &tag, (const unsigned char*)frame.data(),
                frame.size(), nullptr, 0) != 0) {
            if (error) *error = "a frame failed authentication";
            return false;
        }
        payload.append((const char*)pt.data(), (std::size_t)mlen);
        if (max_bytes && payload.size() > max_bytes) {
            if (error) *error = "the peer sent a message larger than this step allows";
            return false;
        }
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) return true;
    }
}

}  // namespace hormiga::sync
