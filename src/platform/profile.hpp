/* platform/profile.hpp — the person at THIS computer, outside every database.
 *
 * okf/concepts/platform/lan-sharing.md §1. The author (2026-09-16): *"a 'profile'
 * window. this is INDEPENDENT on a database. this is information that exists for
 * any database. this will automatically check what device the user has, their
 * operating system, the version of hormiga, etc. … This page will also allow the
 * user to set a username. no passwords yet."*
 *
 * WHAT IS STORED: a username, a preferred colour, a picture, and a keypair --
 * in the user's settings folder, never in a database. WHAT IS DETECTED: the
 * device facts, on every call, because a stored copy is wrong after the next
 * update.
 *
 * A PROFILE IS A CREDENTIAL, NOT A PERSON (identity.md). It never becomes a
 * contact and never publishes. The private key is a file with owner-only
 * permissions on POSIX -- the SSH posture, stated rather than dressed up, because
 * the author asked for no password yet. Sealing it to a passphrase is the vault
 * primitive and a later step.
 *
 * No GUI and no Void Core here: the window is ui/profile.cpp, and the Core
 * version is handed in by the caller that has one.
 */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace hormiga::profile {

struct Profile {
    std::string username;    // "" until the person sets one
    std::string color;       // "#rrggbb", the colour this person PREFERS
    std::string avatar;      // file name inside dir(); "" = the drawn default
    std::string public_key;  // 32 raw bytes (X25519, crypto_kx) -- public
    std::string secret_key;  // 32 raw bytes -- NEVER transmitted, never logged
    std::string created;     // ISO date
};

/* `HORMIGA_PROFILE_DIR` when set (two profiles on one machine, for testing);
 * otherwise the per-OS settings folder. Created on demand. */
std::filesystem::path dir();

/* Load the profile, making the keypair and a colour on first run. Never fails
 * outright: a profile that cannot be written still works for this session, and
 * `error` says why it will not be remembered. */
Profile load_or_create(std::string* error = nullptr);
bool save(const Profile& p, std::string* error = nullptr);

/* Copy a picked picture into dir() as the avatar. "" = reset to the default. */
bool set_avatar(Profile& p, const std::filesystem::path& picked, std::string* error = nullptr);
std::filesystem::path avatar_path(const Profile& p);  // "" when default

/* A small square PNG of the avatar (centre-cropped, box-filtered), for sending
 * to other members. "" when there is no picture or it cannot be decoded. */
std::string avatar_png(const Profile& p, int size = 64);

/* The name a person sees when no username is set yet. */
std::string display_name(const Profile& p);

struct Fact {
    std::string label, value;
};

/* Operating system and version, device name, architecture, cores, memory, the
 * Hormiga version, and whatever the caller adds (the Void Core version, the
 * graphics renderer). Detected now. */
std::vector<Fact> device_facts(const std::vector<Fact>& extra = {});

}  // namespace hormiga::profile
