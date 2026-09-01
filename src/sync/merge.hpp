/* merge.hpp — the Void Palabra seam: two states in, one state out.
 *
 * okf/concepts/platform/collaboration.md §2. This is a THIN ADAPTER and that is
 * a design constraint rather than a description: it converts, it calls, it
 * reports, and it contains no merge logic of its own. Merge logic here would be
 * a second implementation of the thing we adopted a library for, and it is the
 * one piece in the whole sync story that can be silently wrong.
 *
 * WHY A LIBRARY AT ALL, stated once so nobody is tempted to inline it:
 *
 *     A join over bare Void Core state is impossible. Two peers holding {a} and
 *     {} cannot tell "I never had a" from "I removed a", so any join of bare
 *     state is UNION.
 *
 * That is Palabra's own opening argument (join.hpp), and it is the reason the
 * fifty-line version of this file that everyone writes first is wrong.
 *
 * ── WHAT THIS INCREMENT ACTUALLY DOES, AND WHAT IT DOES NOT ────────────────
 *
 * It enriches both sides FRESH, from bare state, on every merge. That buys the
 * correct answer for edits and for additions, and it does NOT buy propagating
 * deletions -- because a fresh enrich has no memory of what this peer once saw,
 * which is precisely the memory the quote above says the problem requires.
 *
 * So: **a deletion on one side does not survive a merge with a side that still
 * has the rune.** The rune comes back. That is a real limitation, it is stated
 * here rather than discovered later, and `MergeResult` reports the one-sided
 * runes explicitly so a reviewer sees exactly what arrived from where instead of
 * being told a merge "succeeded".
 *
 * Fixing it needs a PERSISTED enriched document (Palabra's container, their
 * `Archive`) so each peer remembers what it has observed. That is deliberately
 * out of scope here: Q40's answer adopted the merge and explicitly left the
 * save path alone, because the save path is the one place a silent loss is
 * unacceptable. Measured and reported upstream 2026-08-27.
 */
#pragma once

#include <string>
#include <vector>

namespace hormiga::sync {

/* A conflict, flattened out of Palabra's `Conflict` so this header stays free
 * of cJSON and of Palabra's types -- everything above `sync/` speaks strings.
 *
 * A conflict is a VALUE, never an error (Palabra okf/concepts/conflict.md): it
 * has an address and a content hash, so two peers who see the same divergence
 * name it identically. */
struct Conflict {
    std::string mantle;              // mantle name
    std::string rune;                // spirit.id; empty for a mantle-level field
    std::string field;               // e.g. "content.phone"
    std::vector<std::string> sides;  // the surviving values, canonically ordered
    std::string hash;                // this conflict's own content address
};

/* A rune that exists on exactly one side.
 *
 * Reported rather than silently unioned, because this is the category the
 * limitation above lives in: "they added it" and "I deleted it" are the same
 * observation to a stateless merge, and only a human knows which happened. */
struct OneSided {
    std::string mantle;
    std::string rune;   // spirit.id
    std::string name;   // spirit.name, for a reviewer who has to recognise it
    bool on_local;      // true: only here. false: only on the peer.
};

struct MergeResult {
    bool ok = false;
    std::string error;

    /* The sayable names of the three cuts (Palabra `v:…`). Two peers who
     * reached the same state by different routes compute the same string. */
    std::string version_local, version_remote, version_merged;

    /* Both sides were already the same cut. Nothing was merged, nothing needs
     * writing, and the exchange should stop here -- this is the common case
     * between two devices that are in sync, and it costs one round trip. */
    bool identical = false;

    /* The merged state document: the LOCAL document with its `mantles` replaced
     * by the join. `config`, `domains` and `bindings` are peer-local resolution
     * and are deliberately NOT merged -- a domain carries real deploy commands,
     * so syncing one would run one device's deploy on another. The practical
     * consequence is that syncing cannot move credentials. */
    std::string merged_state;

    std::vector<Conflict> conflicts;
    std::vector<OneSided> one_sided;

    int mantles = 0, runes = 0;  // what the merged document ended up holding
};

/* The version name of a state document's versioned slice (`mantles` only).
 * Empty on a parse failure, with the reason in `error` when it is non-null. */
std::string version_name(const std::string& state_json, std::string* error = nullptr);

/* Merge two Void Core state documents.
 *
 * NEVER resolves a conflict silently. `flatten` with the default policy picks
 * the lowest-ordered value, so a caller that ignores `conflicts` is the one way
 * to lose data at this seam; callers are expected to refuse, or to show them.
 *
 * `local_peer` / `remote_peer` are tag-mint prefixes. They must DIFFER, and they
 * must be stable for a given peer, or two peers mint the same tag for different
 * elements. Passing the same string for both is rejected rather than
 * accommodated. */
MergeResult merge_states(const std::string& local_state, const std::string& remote_state,
                         const std::string& local_peer, const std::string& remote_peer);

}  // namespace hormiga::sync
