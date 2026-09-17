/* replica.hpp — one device's copy of a shared database, kept BETWEEN exchanges.
 *
 * Void Palabra's answer to our request (MESSAGE_FOR_VOIDHORMIGA_palabra-sharing-
 * answered-2026-09-16, folded into okf/concepts/platform/collaboration.md):
 * `merge_states` enriches both sides afresh on every exchange, and a document
 * rebuilt from bare state has forgotten every removal it made -- so a deleted
 * rune comes back. On a timer that is a sync that resurrects deletions forever.
 * A REPLICA survives between exchanges, so a deletion is a recorded act.
 *
 * Like merge.hpp this is a thin adapter: strings in, strings out, no merge logic,
 * and no knowledge of what a rune MEANS. It knows a state document has `mantles`
 * holding `runes` with a `spirit.id`, which is the one fact the private-rune splice
 * needs, and nothing more.
 *
 * ── THE ORDER, and it is not tidiness (Palabra replica.hpp) ─────────────────
 *
 *     observe(state)      record what the person did since last time
 *     save  to_bytes()    BEFORE anything leaves the device
 *     send  doc_json()    the whole document; a lost delta is repaired by the next
 *     merge(received)     validated, identity-checked, joined
 *     splice_into(state)  mantles + glyphs, the local private runes put back
 *     save  to_bytes()
 *
 * Not thread-safe: one replica, one thread (the GUI's). The network threads only
 * ever carry the strings this produces.
 */
#pragma once

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace hormiga::sync {

/* A conflict as the replica reports it. `kind` is "values" (two edits to one
 * field both survived) or "deleted_while_edited" (one member deleted something
 * another was editing; sides are "deleted" and "kept"). */
struct ReplicaConflict {
    std::string kind, mantle, rune, glyph, field, hash;
    std::vector<std::string> sides;
};

class SharedReplica {
public:
    SharedReplica();
    ~SharedReplica();
    SharedReplica(SharedReplica&&) noexcept;
    SharedReplica& operator=(SharedReplica&&) noexcept;
    SharedReplica(const SharedReplica&) = delete;
    SharedReplica& operator=(const SharedReplica&) = delete;

    /* From saved bytes, or a new empty replica under `new_id` when `bytes` is
     * empty. An id must be random, per device and per database, 16+ characters of
     * [A-Za-z0-9-]. */
    bool open(const std::string& bytes, const std::string& new_id, std::string* why = nullptr);
    bool ready() const;
    std::string id() const;
    std::string to_bytes() const;

    /* The same document under a new identity: a restored backup, a cloned
     * machine, or `identity_collision` from a merge. */
    bool fork(const std::string& new_id, std::string* why = nullptr);

    /* Record `state` (already stripped of private runes) as this device's acts. */
    bool observe(const std::string& state_json, std::size_t* changes, std::string* why = nullptr);

    /* The whole enriched document, for the wire. */
    std::string doc_json() const;

    /* The version name of what this replica SHOWS: equal on two devices means
     * there is nothing to exchange. */
    std::string shown_version() const;

    enum class Merge { ok, invalid, identity_collision };
    Merge merge(const std::string& remote_doc_json, std::string* why = nullptr);

    /* `local_state` with its `mantles` and `glyphs` replaced by what this replica
     * shows -- `config`, `domains` and `bindings` stay this device's -- and every
     * rune in `keep` (mantle name, spirit.id: the private ones) put back from the
     * local document, since the replica never saw them. "" on a parse failure. */
    std::string splice_into(const std::string& local_state,
                            const std::set<std::pair<std::string, std::string>>& keep) const;

    std::vector<ReplicaConflict> conflicts() const;
    /* Choose side `side` of the conflict with this `hash`; false if it no longer
     * exists as given (read `conflicts()` again). */
    bool resolve(const std::string& hash, std::size_t side);

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

}  // namespace hormiga::sync
