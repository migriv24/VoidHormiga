/* merge.cpp — the Palabra call, and the splice back into a Core state document.
 *
 * See merge.hpp for what this increment does and does not buy. The code here is
 * deliberately boring: parse, enrich, join, read the conflicts, flatten, splice.
 * Every interesting decision was made in the library.
 */
#include "sync/merge.hpp"

#include <cJSON.h>

#include <memory>

#include "voidpalabra/canonical.hpp"
#include "voidpalabra/join.hpp"

namespace hormiga::sync {
namespace {

/* cJSON is a C library and every early return below would otherwise leak a
 * tree. The merge path runs on the organization's whole database, so a leak
 * here is measured in megabytes per sync rather than bytes. */
struct JsonDel {
    void operator()(cJSON* p) const { cJSON_Delete(p); }
};
using Json = std::unique_ptr<cJSON, JsonDel>;

std::string print_and_free(cJSON* v) {
    char* s = cJSON_PrintUnformatted(v);
    if (!s) return {};
    std::string out(s);
    cJSON_free(s);
    return out;
}

const cJSON* obj(const cJSON* v, const char* k) {
    return v ? cJSON_GetObjectItemCaseSensitive(v, k) : nullptr;
}

const char* str_of(const cJSON* v, const char* k) {
    const cJSON* f = obj(v, k);
    return (f && cJSON_IsString(f) && f->valuestring) ? f->valuestring : "";
}

/* (mantle, rune id) -> spirit.name, for every rune in a state document.
 * Only used to make the one-sided report legible to a human; nothing depends
 * on it. */
void index_runes(const cJSON* state,
                 std::vector<std::pair<std::pair<std::string, std::string>, std::string>>& out) {
    const cJSON* ms = obj(state, "mantles");
    if (!cJSON_IsArray(ms)) return;
    const cJSON* m = nullptr;
    cJSON_ArrayForEach(m, ms) {
        const std::string mname = str_of(m, "name");
        const cJSON* rs = obj(m, "runes");
        if (!cJSON_IsArray(rs)) continue;
        const cJSON* r = nullptr;
        cJSON_ArrayForEach(r, rs) {
            const cJSON* sp = obj(r, "spirit");
            out.push_back({{mname, str_of(sp, "id")}, str_of(sp, "name")});
        }
    }
}

bool has_rune(const std::vector<std::pair<std::pair<std::string, std::string>, std::string>>& idx,
              const std::string& mantle, const std::string& rune) {
    for (const auto& e : idx)
        if (e.first.first == mantle && e.first.second == rune) return true;
    return false;
}

}  // namespace

std::string version_name(const std::string& state_json, std::string* error) {
    Json state(cJSON_Parse(state_json.c_str()));
    if (!state) {
        if (error) *error = "state document does not parse as JSON";
        return {};
    }
    try {
        return voidpalabra::version_name(state.get());
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return {};
    }
}

MergeResult merge_states(const std::string& local_state, const std::string& remote_state,
                         const std::string& local_peer, const std::string& remote_peer) {
    MergeResult res;

    /* THE MINT PREFIXES MUST DIFFER. Palabra's tags are the source of
     * observed-remove and of conflict identity; two peers minting the same tag
     * for different elements is a corruption that presents as "the merge
     * silently dropped one of them". Refusing is the only safe response --
     * defaulting one of them would hide a caller's bug in the one place a bug
     * is unrecoverable. */
    if (local_peer.empty() || remote_peer.empty() || local_peer == remote_peer) {
        res.error = "peer ids must be non-empty and different (got '" + local_peer +
                    "' and '" + remote_peer + "')";
        return res;
    }

    Json a(cJSON_Parse(local_state.c_str()));
    if (!a) { res.error = "local state document does not parse as JSON"; return res; }
    Json b(cJSON_Parse(remote_state.c_str()));
    if (!b) { res.error = "peer state document does not parse as JSON"; return res; }

    try {
        res.version_local = voidpalabra::version_name(a.get());
        res.version_remote = voidpalabra::version_name(b.get());

        /* One round trip, no data. Two devices already in sync discover it
         * here, which is the common case and the reason `version_name` is what
         * the discovery beacon advertises. */
        if (res.version_local == res.version_remote) {
            res.identical = true;
            res.version_merged = res.version_local;
            res.merged_state = local_state;
            res.ok = true;
            return res;
        }

        voidpalabra::CounterMint mint_a(local_peer), mint_b(remote_peer);
        voidpalabra::Doc da = voidpalabra::enrich(a.get(), mint_a);
        voidpalabra::Doc db = voidpalabra::enrich(b.get(), mint_b);
        voidpalabra::Doc merged = voidpalabra::join(da, db);

        for (const auto& c : voidpalabra::conflicts(merged)) {
            Conflict out;
            out.mantle = c.mantle;
            out.rune = c.rune;
            out.field = c.field;
            out.sides = c.sides;
            out.hash = voidpalabra::to_hex(c.hash());
            res.conflicts.push_back(std::move(out));
        }

        voidpalabra::Doc flat = voidpalabra::flatten(merged);

        /* THE SPLICE. `flatten` returns the versioned slice -- `mantles` and
         * nothing else, because that is all Palabra versions. The merged
         * document is therefore the LOCAL document with its mantles replaced,
         * which is what keeps this device's Antfarm wiring, its domains and its
         * `site.base_url` its own. collaboration.md §2. */
        cJSON* new_mantles = cJSON_DetachItemFromObjectCaseSensitive(flat.root, "mantles");
        if (!new_mantles) { res.error = "merged document carried no mantles"; return res; }
        cJSON_DeleteItemFromObjectCaseSensitive(a.get(), "mantles");
        cJSON_AddItemToObject(a.get(), "mantles", new_mantles);

        res.mantles = cJSON_GetArraySize(new_mantles);
        const cJSON* m = nullptr;
        cJSON_ArrayForEach(m, new_mantles) {
            const cJSON* rs = obj(m, "runes");
            if (cJSON_IsArray(rs)) res.runes += cJSON_GetArraySize(rs);
        }

        /* WHAT CAME FROM WHERE. Computed against the two ORIGINAL documents --
         * `a` has already been spliced, so `b` is compared to the pre-merge
         * local index captured before the splice. */
        std::vector<std::pair<std::pair<std::string, std::string>, std::string>> ia, ib;
        {
            Json a_orig(cJSON_Parse(local_state.c_str()));
            index_runes(a_orig.get(), ia);
        }
        index_runes(b.get(), ib);
        for (const auto& e : ia)
            if (!has_rune(ib, e.first.first, e.first.second))
                res.one_sided.push_back({e.first.first, e.first.second, e.second, true});
        for (const auto& e : ib)
            if (!has_rune(ia, e.first.first, e.first.second))
                res.one_sided.push_back({e.first.first, e.first.second, e.second, false});

        res.merged_state = print_and_free(a.get());
        if (res.merged_state.empty()) { res.error = "could not serialise merged state"; return res; }

        res.version_merged = version_name(res.merged_state, &res.error);
        res.ok = true;
        return res;
    } catch (const std::exception& e) {
        res.error = e.what();
        res.ok = false;
        return res;
    }
}

}  // namespace hormiga::sync
