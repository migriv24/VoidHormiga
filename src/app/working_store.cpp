/* app/working_store.cpp — see working_store.hpp. */
#include "app/working_store.hpp"

#include "rancho/memory.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/rancho.hpp" // the app's Core as Rancho's CoreLink

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace fs = std::filesystem;

namespace hormiga {

/* HORMIGA_MACHINE_MB plans for a machine of that size instead of this one: how
 * a laptop is made to open a database as the smallest promised device would
 * (tools/lowmem, tests/app_store_test.cpp). */
rancho::Budget machine_budget() {
    if (const char* e = std::getenv("HORMIGA_MACHINE_MB"))
        if (const long long mb = std::atoll(e); mb > 0) return rancho::Budget::for_machine((std::uint64_t)mb << 20);
    return rancho::Budget::for_machine(rancho::memory::physical_bytes());
}

bool fits_whole(const fs::path& bundle, const rancho::Budget& budget) {
    if (store::bundle_kind(bundle) != store::BundleKind::V3) return false;
    std::error_code ec;
    const auto bytes = fs::file_size(bundle, ec);
    return !ec && bytes <= budget.text_bytes();
}

void remove_store_files(const fs::path& p) {
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm", "-journal"}) fs::remove(p.string() + s, ec);
}

namespace {

/* A mantle too large to hold whole is clustered once, on its first open
 * (11.8 s for the 5×10^5-rune stress database, at a 79 MB peak). The leaves are
 * derived data kept in the store, so later opens only read them. */
long long cluster_large(rancho::Store& st, const rancho::Budget& budget, const rancho::Progress& progress) {
    long long n = 0;
    for (const auto& m : st.estimates())
        if ((std::uint64_t)m.bytes > budget.text_bytes() / 8 && !st.clustered(m.name)) {
            if (progress) progress(0.0, "Finding the parts of a large mantle");
            const auto c = st.cluster(m.name, 2000, progress);
            if (c.ok) ++n;
        }
    return n;
}

} // namespace

rancho::Plan plan_around_leaf(rancho::Store& st, const rancho::Budget& budget, const std::string& mantle,
                              const std::string& leaf) {
    rancho::Plan p = rancho::plan(st, budget, mantle);
    if (!p.partial || leaf.empty()) return p;
    const auto leaves = st.leaf_estimates(mantle);
    std::size_t start = leaves.size();
    for (std::size_t i = 0; i < leaves.size(); ++i)
        if (leaves[i].path == leaf) start = i;
    if (start == leaves.size()) return p;

    // what the plan gave this mantle goes back into the room, and the leaf's part takes it
    const auto it = p.sel.by_cluster.find(mantle);
    std::uint64_t other = p.text_bytes;
    long long runes = p.runes;
    double scale = 1.0;
    for (const auto& m : st.estimates())
        if (m.name == mantle && m.rune_bytes > 0) scale = (double)m.bytes / (double)m.rune_bytes;
    auto cost = [&](const rancho::ClusterLeaf& l) { return (std::uint64_t)((double)l.bytes * scale); };
    if (it != p.sel.by_cluster.end())
        for (const auto& l : leaves)
            if (std::find(it->second.begin(), it->second.end(), l.path) != it->second.end()) {
                other -= std::min(other, cost(l));
                runes -= l.runes;
            }
    p.sel.by_name.erase(mantle);
    const std::uint64_t cap = budget.text_bytes();
    const std::uint64_t room = cap > other ? cap - other : 0;

    std::vector<std::string> chosen{leaves[start].path};
    std::uint64_t used = cost(leaves[start]);
    runes += leaves[start].runes;
    std::size_t lo = start, hi = start;
    for (bool grew = true; grew;) { // outwards in path order: the nearest leaves share the longest prefix
        grew = false;
        if (hi + 1 < leaves.size() && used + cost(leaves[hi + 1]) <= room) {
            used += cost(leaves[++hi]);
            runes += leaves[hi].runes;
            chosen.push_back(leaves[hi].path);
            grew = true;
        }
        if (lo > 0 && used + cost(leaves[lo - 1]) <= room) {
            used += cost(leaves[--lo]);
            runes += leaves[lo].runes;
            chosen.push_back(leaves[lo].path);
            grew = true;
        }
    }
    p.sel.by_cluster[mantle] = std::move(chosen);
    p.text_bytes = other + used;
    p.runes = runes;
    p.focus = leaf;
    return p;
}

Prepared prepare_store(const fs::path& store_path, const rancho::Budget& budget, const std::string& focus_mantle,
                       const std::string& focus_rune, const std::string& focus_leaf) {
    Prepared out;
    rancho::Store st(store_path);
    if (!st.ok()) {
        out.error = "could not open the database: " + st.error();
        return out;
    }
    out.clustered = cluster_large(st, budget, {});
    out.plan = focus_leaf.empty() ? rancho::plan(st, budget, focus_mantle, focus_rune)
                                  : plan_around_leaf(st, budget, focus_mantle, focus_leaf);
    out.state = st.state(out.plan.sel);
    out.ok = !out.state.empty();
    if (!out.ok) out.error = "the database wrote no document: " + st.error();
    return out;
}

Prepared open_into_store(const fs::path& bundle, const fs::path& store_path, const fs::path& base_dir,
                         const fs::path& assets_dir, const rancho::Budget& budget, const rancho::Progress& progress) {
    Prepared out;
    auto t = std::chrono::steady_clock::now();
    auto lap = [&](const char* what) {
        const auto now = std::chrono::steady_clock::now();
        out.timings += (out.timings.empty() ? "" : ", ") + std::string(what) + " " +
                       std::to_string((long long)std::chrono::duration<double, std::milli>(now - t).count()) + " ms";
        t = now;
    };
    auto part = [&](double lo, double hi) -> rancho::Progress {
        if (!progress) return {};
        return [=](double f, const char* phase) { progress(lo + (hi - lo) * f, phase); };
    };
    out.bundle = store::open_bundle(bundle, store_path, base_dir, assets_dir, part(0.0, 0.7));
    if (!out.bundle.ok) {
        out.error = out.bundle.error;
        return out;
    }
    lap("reading the bundle");
    rancho::Store st(store_path);
    if (!st.ok()) {
        out.error = "could not open the database: " + st.error();
        return out;
    }
    /* A v4 bundle's pictures are now files in the working copy, which is where
     * the app reads them; the copied store would hold every one a second time
     * (Rancho's reply of 2026-11-02). Dropped, and the file compacted. */
    if (!st.blob_paths().empty()) {
        if (progress) progress(0.72, "Making room");
        st.clear_blobs();
        st.compact();
    }
    lap("making room");
    out.clustered = cluster_large(st, budget, part(0.75, 0.95));
    lap("clustering");
    if (progress) progress(0.95, "Choosing what to hold");
    out.plan = rancho::plan(st, budget);
    lap("planning");
    out.state = st.state(out.plan.sel);
    lap("writing the part");
    out.ok = !out.state.empty();
    if (!out.ok) out.error = "the database wrote no document: " + st.error();
    return out;
}

WorkingStore::WorkingStore() = default;
WorkingStore::~WorkingStore() { detach(); }

bool WorkingStore::attach(maiz::Core& core, const fs::path& path, const rancho::Plan& plan) {
    detach();
    store_ = std::make_unique<rancho::Store>(path);
    if (!store_->ok()) {
        error_ = store_->error();
        store_.reset();
        return false;
    }
    link_ = std::make_unique<maiz::RanchoLink>(core);
    ws_ = std::make_unique<rancho::WorkingSet>(*store_, *link_);
    if (!ws_->adopt(plan.sel)) {
        error_ = ws_->error();
        detach();
        return false;
    }
    ws_->set_actor("human:hormiga");
    plan_ = plan;
    path_ = path;
    total_ = -1;
    error_.clear();
    return true;
}

void WorkingStore::detach() {
    ws_.reset();
    link_.reset();
    store_.reset();
    plan_ = {};
    path_.clear();
    total_ = -1;
}

maiz::Result WorkingStore::dispatch(const std::string& command) {
    maiz::Result out;
    const rancho::Reply r = ws_->dispatch(command);
    out.ok = r.ok;
    out.data = r.data;
    std::size_t at = 0;
    while (!r.text.empty() && at <= r.text.size()) {
        const std::size_t nl = r.text.find('\n', at);
        out.lines.push_back(r.text.substr(at, nl == std::string::npos ? std::string::npos : nl - at));
        if (nl == std::string::npos) break;
        at = nl + 1;
    }
    return out;
}

bool WorkingStore::sync() {
    if (!ws_) return true;
    const bool ok = ws_->sync();
    if (!ok) error_ = ws_->error();
    total_ = -1;
    return ok;
}

bool WorkingStore::name_taken(const std::string& mantle, const std::string& name) {
    return ws_ && !mantle.empty() && ws_->name_taken(mantle, name);
}

long long WorkingStore::total_runes() {
    if (!store_) return 0;
    if (total_ < 0) total_ = store_->rune_count();
    return total_;
}

} // namespace hormiga
