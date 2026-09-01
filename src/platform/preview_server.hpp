/* preview_server.hpp — the LIVE PREVIEW server (okf/concepts/sections/builder.md B2;
 * QB decided by the author: "tiny local host seems like the best thing").
 *
 * A hand-rolled localhost-only HTTP server — no vendored dependency, ~200
 * lines — serving the app's base_dir (site/ + preview-*.html + exports/).
 * Two jobs:
 *
 *   1. Serve files, injecting a tiny reload-poller into every .html AT SERVE
 *      TIME — files on disk stay deploy-clean; only the live view carries it.
 *   2. Answer GET /__version with a counter the app bumps after each
 *      re-render; the poller reloads the page when it changes.
 *
 * Binds 127.0.0.1 ONLY (never a network interface — nothing here is meant to
 * leave the machine; the deploy holiday is how things leave the machine).
 * HTTP/1.0, close-per-request: dumb on purpose.
 */
#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

namespace hormiga {

class PreviewServer {
  public:
    /* Two modes over the same guts:
     *   PREVIEW (dev): root = base_dir, whitelist ON (serves only render
     *     artifacts — never the org db/keys), reload-poller injected, `/`
     *     maps to the English site index.
     *   HOST (a real-domain stand-in): root = the built site/ folder itself,
     *     served clean (no injection) so it is deploy-faithful; `/` maps to
     *     index-en.html. Safe to serve openly — root IS the public artifact.
     * `base_port` lets the two modes take different ports (preview 8642, host
     * 8780). Idempotent; false = no socket bound. */
    bool start(const std::filesystem::path& root, bool host_mode = false,
               int base_port = 8642);
    void stop();
    void bump() { ++version_; } // a render landed → live pages reload
    int port() const { return port_; }
    bool running() const { return running_.load(); }
    ~PreviewServer() { stop(); }

  private:
    void serve(); // the accept loop (worker thread)
    std::filesystem::path root_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<int> version_{1};
    unsigned long long sock_ = (unsigned long long)-1; // SOCKET, kept opaque
    int port_ = 0;
    bool host_mode_ = false; // clean serve (no inject, no whitelist, site root)
};

} // namespace hormiga
