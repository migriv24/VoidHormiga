/* app/lan_wire.hpp — what goes on the network when a database is shared, as
 * bytes: the offer on the beacon, presence sealed to the room key, and the rule
 * for a file name that arrives from someone else.
 *
 * okf/concepts/platform/lan-sharing.md §2, §3, §6. Split from `lan_share.cpp`
 * so it can be tested without the application: it depends on `sync/` (the
 * sealing) and the vendored JSON, and on nothing that knows a rune.
 *
 * WHAT IS IN THE CLEAR AND WHAT IS NOT. An OFFER is readable by everyone on the
 * network -- the database's name and description and the host's username -- and
 * is only announced while a person has switched sharing on. PRESENCE says only
 * *a room id* in the clear; who is present and what they are on is sealed with
 * the room key, which only members hold.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hormiga::lan {

inline constexpr int kProtocol = 1;
inline constexpr std::size_t kFileChunk = 1 << 20;     // one file message on the wire
inline constexpr std::size_t kMaxHeader = 4u << 20;    // a join or welcome message
inline constexpr long long kMaxTotal = 16LL << 30;     // refuse a transfer larger than this
inline constexpr int kMaxFiles = 50000;

/* A database someone is offering on this network. */
struct Offer {
    std::string peer_id, db, description, user, color, fingerprint, address;
    int port = 0;
    std::int64_t seen = 0;
};

/* What a member is doing, as the others see it. */
struct Activity {
    std::string fingerprint, user, color, section, mantle;
    std::vector<std::string> selection;
    std::string version;   // what this member's replica shows (lan-sharing.md §3b)
    std::string address;   // filled in by the receiver, from the datagram
    std::int64_t seen = 0;
};

/* 16 random bytes as hex after a prefix: a replica id (unique per device and per
 * database, which is what Void Palabra requires of one). */
std::string random_id(const char* prefix);

std::string b64(const std::string& raw);
std::string unb64(const std::string& text);  // "" on malformed input

/* 32 random bytes, and a short public name for them: members recognise their
 * room's beacons by this id, and learn nothing else from it. */
std::string new_room_key();
std::string room_id(const std::string& room_key);

/* The beacon's opaque `extra`: an optional offer, an optional room id with its
 * sealed presence. Trimmed to fit one datagram (sync::kMaxBeacon). */
std::string beacon_extra(const Offer* offer, const std::string& room,
                         const std::string& sealed_activity);
struct ExtraParts {
    bool has_offer = false;
    Offer offer;
    std::string room, sealed;
};
bool read_extra(const std::string& extra, ExtraParts& out);

std::string seal_activity(const Activity& a, const std::string& room_key);
bool open_activity(const std::string& sealed, const std::string& room_key, Activity& out);

/* A file name that came from the other device. Relative, no `..`, no drive or
 * root, no backslash, no control characters, not absurdly long. The joining
 * device writes only names this accepts: a host that names `../../.bashrc`
 * would otherwise be writing wherever it liked. */
bool safe_rel(const std::string& rel);

/* A folder-safe version of a database's name, for the file it arrives as. */
std::string file_stem(const std::string& name);

}  // namespace hormiga::lan
