/* app/lan_cli.cpp — `effect profile`, `lan-offers`, `lan-share`, `lan-join`:
 * the same runtime the windows use, driven from a terminal. */
#include "app/lan_internal.hpp"

#include <thread>

namespace fs = std::filesystem;
using namespace lan_detail;

/* ── the CLI ────────────────────────────────────────────────────────────────── */

int LanRuntime::cli(HormigaApp& app, std::string_view op, const std::vector<std::string>& args,
                    const std::string& state_json, std::string& value) {
    LanRuntime& rt = of(app);
    auto arg = [&](std::size_t i) { return i < args.size() ? args[i] : std::string(); };
    auto seconds = [&](std::size_t i, int d) {
        const int v = std::atoi(arg(i).c_str());
        return v > 0 ? v : d;
    };
    auto now = [] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    if (op == "profile") {
        for (std::size_t i = 0; i + 1 < args.size(); i += 2) {
            if (args[i] == "username") rt.me.username = args[i + 1];
            else if (args[i] == "color") rt.me.color = args[i + 1];
        }
        if (!args.empty()) hormiga::profile::save(rt.me);
        std::cerr << "  profile folder: " << u8(hormiga::profile::dir()) << "\n"
                  << "  username: " << hormiga::profile::display_name(rt.me) << "\n"
                  << "  colour: " << rt.me.color << "\n"
                  << "  key fingerprint: " << fingerprint(rt) << "\n";
        for (const auto& f : hormiga::profile::device_facts({{"Void Core", std::string(maiz::Core::core_version())}}))
            std::cerr << "  " << f.label << ": " << f.value << "\n";
        value = rt.me.username;
        return 0;
    }

    /* A throwaway app whose core is nobody's host: the CLI reads the document and
     * shares it; effects are answered by the CLI itself. */
    app.core = maiz::Core(state_json);  // NO HOST SEAMS: see the comment above
    if (app.on_register_glyphs) app.on_register_glyphs(app.core);
    app.reproject();

    if (op == "lan-offers") {
        rt.discovering = true;
        const double end = now() + seconds(0, 5);
        while (now() < end) {
            tick(app, now());
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        const auto found = offers(app);
        if (found.empty()) std::cerr << "  nobody is sharing a database on this network right now\n";
        for (const auto& o : found)
            std::cerr << "  " << o.db << " - shared by " << o.user << " at " << o.address << " port " << o.port
                      << (o.description.empty() ? "" : "\n      " + o.description) << "\n";
        value = std::to_string(found.size());
        return 0;
    }
    if (op == "lan-share") {
        const bool approve = std::find(args.begin(), args.end(), "approve") != args.end();
        std::string err;
        if (!start_sharing(app, err)) {
            std::cerr << "  [error] share: " << err << "\n";
            return 1;
        }
        std::cerr << "  sharing. The plan:\n";
        for (const auto& it : rt.plan.items)
            std::cerr << "    " << (it.send ? "send " : "keep ") << it.kind << "  " << it.rel << "  - " << it.why << "\n";
        for (const auto& n : rt.plan.notes) std::cerr << "    note: " << n << "\n";
        const double end = now() + seconds(0, 120);
        std::string last;
        while (now() < end) {
            tick(app, now());
            std::shared_ptr<Pending> p;
            std::string status;
            {
                std::lock_guard<std::mutex> lk(rt.mu);
                p = rt.pending;
                status = rt.host_status;
                for (const auto& [lvl, msg] : rt.thread_log) std::cerr << "  [" << lvl << "] share: " << msg << "\n";
                rt.thread_log.clear();
            }
            if (status != last) std::cerr << "  " << (last = status) << "\n";
            if (p && p->answer == 0) {
                std::cerr << "  " << p->req.user << " asks to join. Their screen should show " << p->req.sas << ".\n";
                std::cerr << (approve ? "  allowing (approve was given)\n"
                                      : "  refusing - run with `approve` to allow\n");
                answer(app, approve);
            }
            if (status.find(" joined - ") != std::string::npos) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        stop_sharing(app);
        value = last;
        return last.find(" joined - ") != std::string::npos ? 0 : 1;
    }
    if (op == "lan-join") {
        Offer o;
        o.address = arg(0);
        o.port = std::atoi(arg(1).c_str());
        const fs::path dest = from_u8(arg(2));
        if (o.address.empty() || o.port <= 0 || dest.empty()) {
            std::cerr << "  usage: effect lan-join <address> <port> <folder>\n";
            return 1;
        }
        o.user = "the host";
        std::string err;
        if (!start_join(app, o, dest, err)) {
            std::cerr << "  [error] join: " << err << "\n";
            return 1;
        }
        std::string last, sas;
        const double end = now() + 300;
        while (rt.joining && now() < end) {
            {
                std::lock_guard<std::mutex> lk(rt.mu);
                if (rt.join_sas != sas) std::cerr << "  code on this screen: " << (sas = rt.join_sas) << "\n";
                if (rt.join_status != last) std::cerr << "  " << (last = rt.join_status) << "\n";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        std::lock_guard<std::mutex> lk(rt.mu);
        // a fast join can finish between two polls; the code is still worth printing
        if (sas.empty() && !rt.join_sas.empty()) std::cerr << "  code on this screen: " << rt.join_sas << "\n";
        for (const auto& [lvl, msg] : rt.thread_log) std::cerr << "  [" << lvl << "] " << msg << "\n";
        rt.thread_log.clear();
        if (rt.joined_miga.empty()) {
            if (rt.join_error.empty()) std::cerr << "  [error] join: did not finish\n";  // else logged above
            return 1;
        }
        std::cerr << "  the database is at " << u8(rt.joined_miga) << " - open it in Void Hormiga";
        if (!rt.joined_vault.empty())
            std::cerr << " (" << rt.joined_vault.size() << " vault secret(s) arrived; the CLI does not keep them)";
        std::cerr << "\n";
        value = u8(rt.joined_miga);
        return 0;
    }
    std::cerr << "  unknown LAN verb\n";
    return 1;
}
