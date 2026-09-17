/* replica.cpp — see replica.hpp. */
#include "sync/replica.hpp"

#include <cJSON.h>

#include "voidpalabra/canonical.hpp"
#include "voidpalabra/replica.hpp"

namespace hormiga::sync {
namespace {

struct JsonDel {
    void operator()(cJSON* p) const { cJSON_Delete(p); }
};
using Json = std::unique_ptr<cJSON, JsonDel>;

std::string print(const cJSON* v) {
    if (!v) return {};
    char* s = cJSON_PrintUnformatted(v);
    if (!s) return {};
    std::string out(s);
    cJSON_free(s);
    return out;
}

const char* str_of(const cJSON* v, const char* k) {
    const cJSON* f = v ? cJSON_GetObjectItemCaseSensitive(v, k) : nullptr;
    return (f && cJSON_IsString(f) && f->valuestring) ? f->valuestring : "";
}

}  // namespace

struct SharedReplica::Impl {
    voidpalabra::Replica r;
    bool ready = false;
};

SharedReplica::SharedReplica() : p_(std::make_unique<Impl>()) {}
SharedReplica::~SharedReplica() = default;
SharedReplica::SharedReplica(SharedReplica&&) noexcept = default;
SharedReplica& SharedReplica::operator=(SharedReplica&&) noexcept = default;

bool SharedReplica::open(const std::string& bytes, const std::string& new_id, std::string* why) {
    p_->ready = bytes.empty() ? voidpalabra::Replica::create(new_id, p_->r, why)
                              : voidpalabra::Replica::from_bytes(bytes, p_->r, why);
    return p_->ready;
}

bool SharedReplica::ready() const { return p_->ready; }
std::string SharedReplica::id() const { return p_->ready ? p_->r.id() : std::string(); }
std::string SharedReplica::to_bytes() const { return p_->ready ? p_->r.to_bytes() : std::string(); }

bool SharedReplica::fork(const std::string& new_id, std::string* why) {
    if (!p_->ready) return false;
    voidpalabra::Replica forked;
    if (!p_->r.fork(new_id, forked, why)) return false;
    p_->r = std::move(forked);
    return true;
}

bool SharedReplica::observe(const std::string& state_json, std::size_t* changes, std::string* why) {
    if (changes) *changes = 0;
    if (!p_->ready) return false;
    Json state(cJSON_Parse(state_json.c_str()));
    if (!state) {
        if (why) *why = "the state document does not parse";
        return false;
    }
    const auto o = p_->r.observe(state.get());
    if (!o.ok) {
        if (why) *why = o.error;
        return false;
    }
    if (changes) *changes = o.changes;
    return true;
}

std::string SharedReplica::doc_json() const {
    return p_->ready ? print(p_->r.doc().root) : std::string();
}

std::string SharedReplica::shown_version() const {
    if (!p_->ready) return {};
    try {
        const voidpalabra::Doc flat = p_->r.flatten();
        return voidpalabra::version_name(flat.root);
    } catch (...) {
        return {};
    }
}

SharedReplica::Merge SharedReplica::merge(const std::string& remote_doc_json, std::string* why) {
    Json remote(cJSON_Parse(remote_doc_json.c_str()));
    if (!p_->ready || !remote) {
        if (why) *why = "the peer's document does not parse";
        return Merge::invalid;
    }
    switch (p_->r.merge(remote.get(), why)) {
        case voidpalabra::MergeResult::ok: return Merge::ok;
        case voidpalabra::MergeResult::identity_collision: return Merge::identity_collision;
        default: return Merge::invalid;
    }
}

std::string SharedReplica::splice_into(const std::string& local_state,
                                       const std::set<std::pair<std::string, std::string>>& keep) const {
    if (!p_->ready) return {};
    Json local(cJSON_Parse(local_state.c_str()));
    if (!local) return {};
    voidpalabra::Doc flat = p_->r.flatten();
    cJSON* mantles = cJSON_DetachItemFromObjectCaseSensitive(flat.root, "mantles");
    cJSON* glyphs = cJSON_DetachItemFromObjectCaseSensitive(flat.root, "glyphs");
    if (!mantles) mantles = cJSON_CreateArray();

    /* THE PRIVATE RUNES GO BACK IN. The replica never observed them
     * (lan-sharing.md §7), so what it shows lacks them; splicing without this
     * would delete this person's private notes from their own device. A private
     * rune whose mantle another member deleted keeps its mantle here -- losing
     * somebody's own note is worse than a mantle that exists on one device. */
    if (!keep.empty()) {
        const cJSON* old_mantles = cJSON_GetObjectItemCaseSensitive(local.get(), "mantles");
        const cJSON* om = nullptr;
        cJSON_ArrayForEach(om, old_mantles) {
            const std::string mname = str_of(om, "name");
            cJSON* target = nullptr;
            cJSON* nm = nullptr;
            cJSON_ArrayForEach(nm, mantles)
                if (mname == str_of(nm, "name")) target = nm;
            const cJSON* oruns = cJSON_GetObjectItemCaseSensitive(om, "runes");
            const cJSON* rune = nullptr;
            cJSON_ArrayForEach(rune, oruns) {
                const std::string id = str_of(cJSON_GetObjectItemCaseSensitive(rune, "spirit"), "id");
                if (!keep.count({mname, id})) continue;
                if (!target) {
                    target = cJSON_Duplicate(om, 1);
                    cJSON_DeleteItemFromObjectCaseSensitive(target, "runes");
                    cJSON_AddItemToObject(target, "runes", cJSON_CreateArray());
                    cJSON_AddItemToArray(mantles, target);
                }
                cJSON* runes = cJSON_GetObjectItemCaseSensitive(target, "runes");
                if (!runes) runes = cJSON_AddArrayToObject(target, "runes");
                cJSON_AddItemToArray(runes, cJSON_Duplicate(rune, 1));
            }
        }
    }
    cJSON_DeleteItemFromObjectCaseSensitive(local.get(), "mantles");
    cJSON_AddItemToObject(local.get(), "mantles", mantles);
    if (glyphs) {
        cJSON_DeleteItemFromObjectCaseSensitive(local.get(), "glyphs");
        cJSON_AddItemToObject(local.get(), "glyphs", glyphs);
    }
    return print(local.get());
}

std::vector<ReplicaConflict> SharedReplica::conflicts() const {
    std::vector<ReplicaConflict> out;
    if (!p_->ready) return out;
    for (const auto& c : p_->r.conflicts()) {
        ReplicaConflict rc;
        rc.kind = c.kind == voidpalabra::ConflictKind::deleted_while_edited ? "deleted_while_edited" : "values";
        rc.mantle = c.mantle;
        rc.rune = c.rune;
        rc.glyph = c.glyph;
        rc.field = c.field;
        /* Sides arrive as canonical value bytes; a person reads the value. A
         * plain string shows without its quotes. */
        for (const auto& bytes : c.sides) {
            Json v(voidpalabra::decode(bytes));
            std::string shown = v ? (cJSON_IsString(v.get()) ? std::string(v->valuestring) : print(v.get()))
                                  : std::string("(unreadable)");
            rc.sides.push_back(std::move(shown));
        }
        rc.hash = voidpalabra::to_hex(c.hash());
        out.push_back(std::move(rc));
    }
    return out;
}

bool SharedReplica::resolve(const std::string& hash, std::size_t side) {
    if (!p_->ready) return false;
    for (const auto& c : p_->r.conflicts())
        if (voidpalabra::to_hex(c.hash()) == hash) return p_->r.resolve(c, side);
    return false;
}

}  // namespace hormiga::sync
