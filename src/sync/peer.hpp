/* peer.hpp — finding another Hormiga on the LAN, proving it is the one you
 * meant, and moving an opaque blob to it privately.
 *
 * okf/concepts/platform/collaboration.md §3-4. Void Palabra's second pillar is
 * "Speaking — device-to-device communication, LAN first" and it is not built
 * (`reconciliation` and `peer-and-tier` are still `status:planned` on their
 * side). The author needs multi-device now, so this layer is ours for the
 * meantime, and it is written to be THROWN AWAY:
 *
 *     Nothing here knows what a rune is.
 *
 * That ignorance is the design. It finds an address, proves an identity, and
 * moves bytes; the moment Palabra's Phase 4 lands, this file is deleted and the
 * two call sites move, exactly the way `src/platform/miga.*` is written to be
 * displaced by their container.
 *
 * ── WHAT IS HAND-ROLLED AND WHAT IS NOT ────────────────────────────────────
 *
 * The DISCOVERY BEACON is hand-rolled, on purpose. The job is "find another
 * Hormiga on this subnet"; both ends are ours, there is no Bonjour browser to
 * interoperate with, and vendoring an mDNS implementation would buy a protocol
 * we do not need. Ground rule 5 is "vendor, don't depend", and the cheapest way
 * to obey it is to not need the dependency. If interop ever matters,
 * `mjansson/mdns` is a single-header public-domain implementation and this is a
 * small swap.
 *
 * The CRYPTO is not hand-rolled and never will be. Ground rule 6: libsodium
 * only. X25519 key agreement (`crypto_kx`), XChaCha20-Poly1305 `secretstream`
 * for the channel -- the same primitive `platform/backup.cpp` uses, chosen there
 * because it detects TRUNCATION, which is exactly what a network transfer wants
 * for exactly the same reason a file does. A beacon is a datagram format. That
 * is the whole distinction.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace hormiga::sync {

/* The fixed multicast group and port the beacon uses. Administratively-scoped
 * IPv4 multicast (239.0.0.0/8) -- routers do not forward it off the local
 * network, which is the containment we want: a beacon that escaped the subnet
 * would be advertising the existence of an organization's database to strangers. */
inline constexpr const char* kBeaconGroup = "239.77.73.71";  // "MIGA"
inline constexpr std::uint16_t kBeaconPort = 47731;
inline constexpr std::uint16_t kStreamPortDefault = 47732;

/* What a beacon advertises. Deliberately the minimum, and deliberately NOT the
 * organization's name: a datagram on the local network is readable by everyone
 * on it, so this carries only what a peer needs in order to decide whether to
 * start a handshake.
 *
 * `version` is Palabra's cut name (`v:…`). It is the load-bearing field: two
 * devices that are already in sync see equal versions and do nothing at all,
 * which is the common case and costs no connection. */
struct PeerInfo {
    std::string peer_id;      // stable, per device+database (a mint prefix)
    std::string display;      // what a human calls this device
    std::string version;      // Palabra cut name of the sender's state
    std::string fingerprint;  // short hash of the public key, for recognition
    std::uint16_t port = kStreamPortDefault;
    std::string address;      // filled in by the receiver, never by the sender
    std::int64_t last_seen = 0;
    /* OPAQUE TO THIS LAYER (2026-09-16). The application's own announcement --
     * a share offer, or presence sealed to a room key -- as printable text
     * (base64). This file carries it and never reads it, which is what keeps
     * it replaceable (collaboration.md §3). Kept small: a beacon is one
     * datagram, and the whole thing must stay under `kMaxBeacon`. */
    std::string extra;
};

/* A beacon larger than this is refused on both ends. One Ethernet frame is
 * ~1500 bytes; this allows a sealed presence body without fragmenting on any
 * ordinary network. */
inline constexpr std::size_t kMaxBeacon = 1400;

/* --- the beacon --------------------------------------------------------- */

std::string encode_beacon(const PeerInfo& self);
/* Returns false on anything that is not a well-formed beacon from our own
 * protocol version. A malformed datagram on an open UDP port is the expected
 * case, not an exceptional one -- anything on the network may send here. */
bool decode_beacon(const std::string& datagram, PeerInfo& out);

class Beacon {
public:
    Beacon();
    ~Beacon();
    Beacon(const Beacon&) = delete;
    Beacon& operator=(const Beacon&) = delete;

    /* Begin announcing `self` and listening for others. Idempotent. An EMPTY
     * `peer_id` listens without announcing -- "Discover databases" looks around
     * without telling the network it is there. */
    bool start(const PeerInfo& self, std::string* error = nullptr);
    void stop();
    bool running() const { return running_; }

    /* Update what we advertise -- called when the database changes, so peers
     * see a new version name without waiting for a restart. */
    void announce(const PeerInfo& self);

    /* Peers heard from within `stale_seconds`. Sorted by display name so a UI
     * does not reorder under the user. */
    std::vector<PeerInfo> peers(int stale_seconds = 30) const;

private:
    struct Impl;
    Impl* p_ = nullptr;
    bool running_ = false;
};

/* --- identity and the short authentication string ------------------------ */

struct KeyPair {
    std::string public_key;  // 32 raw bytes
    std::string secret_key;  // 32 raw bytes -- NEVER transmitted, never logged
};

KeyPair generate_keypair();
std::string fingerprint_of(const std::string& public_key);

/* The SAS: six characters both ends compute independently from BOTH public
 * keys, compared out of band by two humans in the same room.
 *
 * This is not a pairing code and the difference is the entire security
 * property. A code that device A generates and device B types travels over the
 * channel being attacked, so it cannot authenticate that channel. The SAS is
 * derived from key material after it is exchanged and compared OFF the channel;
 * a machine-in-the-middle who substituted keys produces two different strings
 * and has no way to make them agree.
 *
 * Symmetric by construction: the two keys are sorted before hashing, so both
 * ends get the same answer without agreeing who is "first". Alphabet excludes
 * 0/O/1/I/L because these characters get read aloud. */
std::string short_auth_string(const std::string& pk_a, const std::string& pk_b);

/* --- the sealed stream --------------------------------------------------- */

/* One side of a connection. `Session` owns a socket and two secretstream
 * states; it moves length-prefixed sealed messages and nothing else. */
class Session {
public:
    Session();
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /* Dial a peer and complete the handshake. On success `sas` holds the six
     * characters for the humans to compare -- the caller MUST NOT send anything
     * sensitive before that comparison happens. */
    bool connect(const std::string& host, std::uint16_t port, const KeyPair& self,
                 std::string& peer_public_key, std::string& sas, std::string* error = nullptr);

    /* Accept one inbound connection on `port` and complete the handshake.
     * Blocks up to `timeout_ms`. */
    bool accept_one(std::uint16_t port, const KeyPair& self, std::string& peer_public_key,
                    std::string& sas, int timeout_ms, std::string* error = nullptr);

    bool send(const std::string& payload, std::string* error = nullptr);
    /* `max_bytes` (0 = no limit) refuses a message larger than the caller
     * expects -- a peer decides how much it sends, so a receiver that holds a
     * whole message in memory needs its own ceiling (2026-09-16). */
    bool receive(std::string& payload, std::string* error = nullptr, std::size_t max_bytes = 0);

    /* Receive and send give up after `ms` of silence; 0 = wait forever (the
     * default). A join waits on a person to click Allow, and a thread that can
     * never time out is a thread that can never be cancelled. */
    void set_timeout_ms(int ms);
    void close();
    bool open() const;

private:
    struct Impl;
    Impl* p_ = nullptr;
};

/* Seal / open a payload with an explicit key, for the paths that are not a live
 * socket -- writing a sync bundle to a USB stick or an S3 rendezvous object.
 * Same primitive as the stream, so there is one format and one set of failure
 * modes. Returns false on a wrong key, a tampered byte, or a TRUNCATED input,
 * which is the failure the single-shot seal cannot see. */
bool seal_blob(const std::string& plain, const std::string& key32, std::string& out);
bool open_blob(const std::string& sealed, const std::string& key32, std::string& out,
               std::string* error = nullptr);

}  // namespace hormiga::sync
