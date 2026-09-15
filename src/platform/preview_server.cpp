/* preview_server.cpp — the live-preview server's winsock guts.
 * See preview_server.hpp for the contract. Localhost only, by construction. */
#include "platform/preview_server.hpp"

/* ── ONE SOCKET API, TWO SPELLINGS (2026-09-08) ──────────────────────────────
 *
 * This file was winsock with no alternative, which made it the first real
 * obstacle to a Linux or macOS build once Void Maiz answered that their view
 * compiles on all three (their CI, 2026-09-08). The port is mechanical and the
 * shim below is the whole of it, because **winsock IS BSD sockets** — Berkeley
 * is what Microsoft copied — differing in initialization, teardown, and the
 * name of the failure value. Naming those three things once leaves the body of
 * this file identical on every platform, which is the point: a translation
 * layer that touched every call site would be a second thing to keep right.
 *
 * The one place they genuinely disagree is what happens when the peer hangs up
 * mid-response, and it is not cosmetic. On Linux a `send` to a closed socket
 * raises SIGPIPE and the DEFAULT DISPOSITION IS TO KILL THE PROCESS — so a
 * person closing the preview tab while a page was still being written would
 * take the whole application down with it. Windows has no such signal. See
 * `kSendFlags` and the `SO_NOSIGPIPE` call in `serve()`. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
static constexpr SOCKET INVALID_SOCKET = -1;
inline int closesocket(SOCKET s) { return ::close(s); }
#endif

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace hormiga {
namespace {

/* The reload poller injected into every served .html (NEVER on disk): polls
 * /__version; a change means the app re-rendered — reload. */
const char* kReloadScript =
    "<script>/* hormiga live preview (not part of the deployed site) */"
    "(function(){var v=null;setInterval(function(){"
    "fetch('/__version').then(function(r){return r.text()}).then(function(t){"
    "if(v===null)v=t;else if(t!==v)location.reload();}).catch(function(){});"
    "},700);})()</script>";

const char* content_type(const std::string& path) {
    auto ends = [&](const char* s) {
        size_t n = std::strlen(s);
        return path.size() >= n && path.compare(path.size() - n, n, s) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".css")) return "text/css";
    if (ends(".js")) return "application/javascript";
    if (ends(".png")) return "image/png";
    if (ends(".jpg") || ends(".jpeg")) return "image/jpeg";
    if (ends(".gif")) return "image/gif";
    if (ends(".svg")) return "image/svg+xml";
    if (ends(".woff2")) return "font/woff2";
    if (ends(".woff")) return "font/woff";
    if (ends(".ics")) return "text/calendar";
    if (ends(".json")) return "application/json";
    /* audio, for the `audio` block (2026-09-02). The live preview has to agree
       with the deployed site about what a .mp3 is, or the block plays in one
       and offers a download in the other. */
    if (ends(".mp3")) return "audio/mpeg";
    if (ends(".m4a")) return "audio/mp4";
    if (ends(".aac")) return "audio/aac";
    if (ends(".ogg") || ends(".oga") || ends(".opus")) return "audio/ogg";
    if (ends(".wav")) return "audio/wav";
    if (ends(".flac")) return "audio/flac";
    if (ends(".mp4")) return "video/mp4";
    if (ends(".webm")) return "video/webm";
    return "application/octet-stream";
}

/* Winsock has no signals; Linux raises SIGPIPE on a write to a hung-up peer and
 * kills the process by default. macOS has neither `MSG_NOSIGNAL` nor that
 * default reachable this way, so it is handled per-socket with `SO_NOSIGPIPE`
 * in `serve()` instead. */
#if defined(MSG_NOSIGNAL)
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

/* Winsock needs a one-time library init and BSD sockets do not. Written as a
 * function rather than a faked `WSAStartup` macro so the difference is visible
 * where it is read. */
bool net_startup() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

void send_all(SOCKET c, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        /* `send` returns int on Windows and ssize_t on POSIX; the widest of the
         * two holds either, and a short write is the loop's ordinary case. */
        const long long n = (long long)send(c, data.data() + off,
                                            (int)(data.size() - off), kSendFlags);
        if (n <= 0) return;
        off += (size_t)n;
    }
}

void respond(SOCKET c, const char* status, const char* ctype,
             const std::string& body) {
    std::ostringstream h;
    h << "HTTP/1.0 " << status << "\r\nContent-Type: " << ctype
      << "\r\nContent-Length: " << body.size()
      << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    send_all(c, h.str());
    send_all(c, body);
}

} // namespace

bool PreviewServer::start(const std::filesystem::path& root, bool host_mode,
                          int base_port) {
    if (running_.load()) return true;
    host_mode_ = host_mode;
    if (!net_startup()) return false;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // localhost ONLY
    int port = base_port;
    for (; port < base_port + 20; ++port) { // first free port in a small window
        addr.sin_port = htons((uint16_t)port); // `u_short` is a winsock spelling
        if (bind(s, (sockaddr*)&addr, sizeof addr) == 0) break;
    }
    if (port >= base_port + 20 || listen(s, 8) != 0) {
        closesocket(s);
        return false;
    }
    root_ = root;
    sock_ = (unsigned long long)s;
    port_ = port;
    running_ = true;
    worker_ = std::thread([this] { serve(); });
    return true;
}

void PreviewServer::stop() {
    if (!running_.exchange(false)) return;
    closesocket((SOCKET)sock_); // unblocks accept()
    if (worker_.joinable()) worker_.join();
}

void PreviewServer::serve() {
    while (running_.load()) {
        SOCKET c = accept((SOCKET)sock_, nullptr, nullptr);
        if (c == INVALID_SOCKET) break; // stop() closed the listener
#ifdef SO_NOSIGPIPE
        /* macOS: the per-socket form of what `MSG_NOSIGNAL` does per-call on
         * Linux. Without one of the two, closing the preview tab mid-response
         * kills the application. */
        const int one = 1;
        setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        char buf[4096];
        const long long n = (long long)recv(c, buf, sizeof buf - 1, 0);
        if (n <= 0) { closesocket(c); continue; }
        buf[n] = 0;
        // "GET /path HTTP/1.1" — the only request shape we serve
        std::string req(buf);
        std::string path;
        if (req.rfind("GET ", 0) == 0) {
            size_t sp = req.find(' ', 4);
            if (sp != std::string::npos) path = req.substr(4, sp - 4);
        }
        if (size_t q = path.find('?'); q != std::string::npos)
            path = path.substr(0, q);
        // ── the WHITELIST: this server exists to show RENDER ARTIFACTS —
        // site/, the email previews, exported images. base_dir also holds the
        // org database and (dev-only) plaintext keys; those must never be one
        // GET away, even on localhost. Everything not rendered is 404. ──────
        auto allowed = [](const std::string& p) {
            auto starts = [&](const char* s) { return p.rfind(s, 0) == 0; };
            auto ends = [&](const char* s) {
                size_t n = std::strlen(s);
                return p.size() >= n && p.compare(p.size() - n, n, s) == 0;
            };
            /* `/assets/` IMAGES ONLY (2026-09-15): a newsletter preview draws an
             * image that is not uploaded yet from its local file, so the author
             * sees the layout they are building. Image extensions and nothing
             * else — assets/ can also hold documents that were never published. */
            const bool image = ends(".png") || ends(".jpg") || ends(".jpeg") ||
                               ends(".gif") || ends(".webp") || ends(".PNG") ||
                               ends(".JPG") || ends(".JPEG");
            return starts("/site/") || starts("/exports/") ||
                   (starts("/assets/") && image) ||
                   (starts("/preview-") && ends(".html"));
        };
        // HOST mode roots at the built site/ folder (a real-domain stand-in),
        // serves clean (no whitelist, no reload injection); PREVIEW mode roots
        // at base_dir and serves only render artifacts with the live poller.
        std::string index = host_mode_ ? "/index-en.html" : "/site/index-en.html";
        if (path == "/__version") {
            respond(c, "200 OK", "text/plain", std::to_string(version_.load()));
        } else if (path.empty() || path.find("..") != std::string::npos) {
            respond(c, "400 Bad Request", "text/plain", "no");
        } else if (!host_mode_ && path != "/" && !allowed(path)) {
            respond(c, "404 Not Found", "text/plain",
                    "the preview serves render artifacts only "
                    "(/site/, /preview-*.html, /exports/, /assets/ images)");
        } else {
            if (path == "/") path = index;
            std::filesystem::path full = root_ / path.substr(1);
            std::ifstream f(full, std::ios::binary);
            if (!f) {
                respond(c, "404 Not Found", "text/plain",
                        "not rendered yet: " + path);
            } else {
                std::ostringstream ss;
                ss << f.rdbuf();
                std::string body = ss.str();
                std::string ps = full.string();
                // inject the live-reload poller ONLY in preview mode — the host
                // serves deploy-faithful bytes (what a real domain would send)
                if (!host_mode_ && ps.size() > 5 &&
                    ps.compare(ps.size() - 5, 5, ".html") == 0) {
                    size_t at = body.rfind("</body>");
                    if (at != std::string::npos) body.insert(at, kReloadScript);
                    else body += kReloadScript;
                }
                respond(c, "200 OK", content_type(ps), body);
            }
        }
        closesocket(c);
    }
}

} // namespace hormiga
