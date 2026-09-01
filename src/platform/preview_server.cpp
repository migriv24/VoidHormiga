/* preview_server.cpp — the live-preview server's winsock guts.
 * See preview_server.hpp for the contract. Localhost only, by construction. */
#include "platform/preview_server.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

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
    return "application/octet-stream";
}

void send_all(SOCKET c, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        int n = send(c, data.data() + off, (int)(data.size() - off), 0);
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
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // localhost ONLY
    int port = base_port;
    for (; port < base_port + 20; ++port) { // first free port in a small window
        addr.sin_port = htons((u_short)port);
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
        char buf[4096];
        int n = recv(c, buf, sizeof buf - 1, 0);
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
            return starts("/site/") || starts("/exports/") ||
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
                    "(/site/, /preview-*.html, /exports/)");
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
