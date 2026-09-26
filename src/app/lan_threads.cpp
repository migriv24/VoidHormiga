/* app/lan_threads.cpp — the host's thread and the joining device's thread.
 * See lan_share.hpp for the order of a join; neither thread touches the core. */
#include "app/lan_internal.hpp"

namespace lan_detail {

void host_loop(HostJob job) {
    LanRuntime& rt = *job.rt;
    while (rt.sharing) {
        hormiga::sync::Session s;
        std::string pk, sas, err;
        if (!s.accept_one((std::uint16_t)job.port, job.keys, pk, sas, 700, &err)) {
            if (!is_timeout(err)) {
                {
                    std::lock_guard<std::mutex> lk(rt.mu);
                    rt.host_status = "cannot accept: " + err;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            continue;
        }
        s.set_timeout_ms(15000);
        std::string msg;
        if (!s.receive(msg, &err, hormiga::lan::kMaxHeader)) {
            say(rt, "warn", "a device connected and said nothing: " + err);
            continue;
        }
        const json j = json::parse(msg, nullptr, false);
        if (jstr(j, "t") != "join") {
            say(rt, "warn", "a device connected with something that is not a join request");
            continue;
        }
        Request req;
        req.user = jstr(j, "user");
        req.color = jstr(j, "color");
        req.public_key = hormiga::lan::unb64(jstr(j, "pk"));
        req.avatar_png = hormiga::lan::unb64(jstr(j, "avatar"));
        req.app = jstr(j, "app");
        req.sas = sas;
        /* The key it CLAIMS must be the key it HANDSHOOK with. Otherwise one
         * device could complete the handshake and enrol somebody else's key. */
        if (req.public_key != pk) {
            s.send(dump({{"t", "denied"}, {"why", "the request's key does not match the connection"}}));
            say(rt, "warn", "refused a join whose key did not match its connection");
            continue;
        }
        req.fingerprint = hormiga::sync::fingerprint_of(pk);
        if (req.user.empty()) req.user = "(no username)";

        auto pend = std::make_shared<LanRuntime::Pending>();
        pend->req = req;
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            rt.pending = pend;
            rt.host_status = req.user + " is asking to join";
        }
        s.send(dump({{"t", "asked"}}));
        for (int waited = 0; rt.sharing && pend->answer == 0 && waited < 180000; waited += 100)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            if (rt.pending == pend) rt.pending.reset();
        }
        if (pend->answer != (int)LanRuntime::Answer::Allow) {
            const bool nobody = pend->answer == 0;
            s.send(dump({{"t", "denied"},
                         {"why", nobody ? "the host did not answer in time" : "the host said no"}}));
            std::lock_guard<std::mutex> lk(rt.mu);
            rt.host_status = nobody ? "no answer for " + req.user + " - they were turned away"
                                    : "turned " + req.user + " away";
            continue;
        }

        std::string merr;
        if (!add_member_doc(job.reg, job.members_file, req, "admin", job.host_user, merr))
            say(rt, "warn", merr);

        // the welcome: every file, with its size NOW (the members file just changed)
        json files = json::array();
        std::vector<PlanItem> sending;
        long long total = 0;
        std::error_code ec;
        for (const auto& it : job.plan.items) {
            if (!it.send || it.src.empty()) continue;
            if (!fs::is_regular_file(it.src, ec)) continue;
            PlanItem x = it;
            x.bytes = (long long)fs::file_size(it.src, ec);
            total += x.bytes;
            files.push_back({{"kind", x.kind}, {"rel", x.rel}, {"bytes", x.bytes}});
            sending.push_back(x);
        }
        json welcome = job.welcome;
        welcome["t"] = "welcome";
        welcome["files"] = files;
        welcome["vault"] = job.plan.vault;
        s.set_timeout_ms(60000);
        if (!s.send(dump(welcome), &err)) {
            say(rt, "error", "sending to " + req.user + " failed: " + err);
            continue;
        }
        bool ok = true;
        long long sent = 0;
        for (const auto& it : sending) {
            std::ifstream in(it.src, std::ios::binary);
            std::string chunk(hormiga::lan::kFileChunk, '\0');
            long long left = it.bytes;
            while (left > 0 && ok) {
                const std::streamsize want =
                    (std::streamsize)std::min<long long>(left, (long long)chunk.size());
                in.read(&chunk[0], want);
                if (in.gcount() != want) ok = false;
                else ok = s.send(chunk.substr(0, (std::size_t)want), &err);
                left -= want;
                sent += want;
            }
            if (!ok) break;
            std::lock_guard<std::mutex> lk(rt.mu);
            rt.host_status = "sending to " + req.user + ": " + std::to_string(sent / 1024) +
                             " of " + std::to_string(total / 1024) + " KB";
            auto& t = rt.transfers["join:" + req.user]; // the Migos screen's bar
            if (t.started == 0.0) t = {"the database, to " + req.user, 0, total, true, now_seconds(), -1.0};
            t.done = sent;
        }
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            auto t = rt.transfers.find("join:" + req.user);
            if (t != rt.transfers.end()) t->second.finished = now_seconds();
        }
        if (ok) ok = s.send(dump({{"t", "done"}}), &err);
        std::string reply;
        s.set_timeout_ms(300000);
        if (ok) ok = s.receive(reply, &err, 4096) && jstr(json::parse(reply, nullptr, false), "t") == "ok";
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.host_status = ok ? req.user + " joined - " + std::to_string(sending.size()) +
                                  " files sent, sealed"
                            : "sending to " + req.user + " failed: " +
                                  (err.empty() ? std::string("they did not confirm") : err);
        rt.thread_log.push_back({ok ? "info" : "error", rt.host_status});
    }
}

void join_loop(JoinJob job) {
    LanRuntime& rt = *job.rt;
    auto status = [&](const std::string& m) {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.join_status = m;
    };
    auto fail = [&](const std::string& why) {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.join_error = why;
        rt.join_status = "did not join";
        rt.thread_log.push_back({"error", "join: " + why});
        rt.joining = false;
    };
    hormiga::sync::Session s;
    std::string pk, sas, err;
    status("connecting to " + job.offer.user + " at " + job.offer.address + "...");
    /* A FEW TRIES: the host re-opens its listening socket between waits
     * (`accept_one` per 700 ms), so a connect can land in the gap. */
    bool connected = false;
    for (int attempt = 0; attempt < 6 && !connected; ++attempt) {
        connected = s.connect(job.offer.address, (std::uint16_t)job.offer.port, job.keys, pk, sas, &err);
        if (!connected) std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    if (!connected) return fail("could not connect: " + err);
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.join_sas = sas;
    }
    s.set_timeout_ms(20000);
    const json ask = {{"t", "join"},
                      {"v", hormiga::lan::kProtocol},
                      {"user", job.me.user},
                      {"color", job.me.color},
                      {"pk", hormiga::lan::b64(job.me.public_key)},
                      {"app", job.me.app},
                      {"avatar", hormiga::lan::b64(job.me.avatar_png)}};
    if (!s.send(dump(ask), &err)) return fail("could not ask: " + err);

    status("waiting for " + job.offer.user + " to allow you in - check that their screen shows " + sas);
    s.set_timeout_ms(200000);
    json welcome;
    for (;;) {
        std::string msg;
        if (!s.receive(msg, &err, hormiga::lan::kMaxHeader)) return fail("no answer: " + err);
        welcome = json::parse(msg, nullptr, false);
        const std::string t = jstr(welcome, "t");
        if (t == "asked") continue;
        if (t == "denied") return fail(jstr(welcome, "why").empty() ? "not allowed" : jstr(welcome, "why"));
        if (t == "welcome") break;
        return fail("the host sent something unexpected");
    }

    // check every name and size BEFORE writing anything
    if (!welcome.contains("files") || !welcome["files"].is_array()) return fail("the welcome listed no files");
    struct F { std::string kind, rel; long long bytes; };
    std::vector<F> files;
    long long total = 0;
    std::string miga;
    for (const auto& f : welcome["files"]) {
        F x{jstr(f, "kind"), jstr(f, "rel"),
            f.contains("bytes") && f["bytes"].is_number_integer() ? f["bytes"].get<long long>() : -1};
        if (!hormiga::lan::safe_rel(x.rel)) return fail("refused a file name from the host: " + x.rel);
        if (x.bytes < 0) return fail("refused a file with no size: " + x.rel);
        total += x.bytes;
        if (x.kind == "database") miga = x.rel;
        files.push_back(x);
    }
    if ((int)files.size() > hormiga::lan::kMaxFiles || total > hormiga::lan::kMaxTotal)
        return fail("the transfer is larger than this version accepts");
    if (miga.empty() || fs::path(miga).extension() != ".miga") return fail("the host sent no database");

    std::error_code ec;
    fs::create_directories(job.dest, ec);
    s.set_timeout_ms(60000);
    long long got = 0;
    for (const auto& f : files) {
        const fs::path out = job.dest / from_u8(f.rel);
        fs::create_directories(out.parent_path(), ec);
        fs::path part = out;
        part += ".part";
        std::ofstream o(part, std::ios::binary | std::ios::trunc);
        if (!o) return fail("cannot write " + u8(part));
        long long left = f.bytes;
        while (left > 0) {
            std::string chunk;
            if (!s.receive(chunk, &err, hormiga::lan::kFileChunk)) return fail("the transfer stopped: " + err);
            if ((long long)chunk.size() > left) return fail("the host sent more than it announced");
            o.write(chunk.data(), (std::streamsize)chunk.size());
            left -= (long long)chunk.size();
            got += (long long)chunk.size();
            if ((got & 0xFFFFF) < (long long)chunk.size())
                status("receiving: " + std::to_string(got / 1024) + " of " + std::to_string(total / 1024) + " KB");
            std::lock_guard<std::mutex> lk(rt.mu); // the Migos screen's bar
            auto& t = rt.transfers["join"];
            if (t.started == 0.0) t = {"the database, from " + job.offer.user, 0, total, false, now_seconds(), -1.0};
            t.done = got;
        }
        o.close();
        fs::remove(out, ec);
        fs::rename(part, out, ec);
        if (ec) return fail("cannot place " + u8(out) + ": " + ec.message());
        if (f.kind == "key" || f.kind == "room-key") owner_only(out);
    }
    std::string done;
    if (!s.receive(done, &err, 4096) || jstr(json::parse(done, nullptr, false), "t") != "done")
        return fail("the host did not finish: " + err);
    s.send(dump({{"t", "ok"}}));

    std::lock_guard<std::mutex> lk(rt.mu);
    rt.joined_miga = job.dest / from_u8(miga);
    rt.joined_vault.clear();
    if (welcome.contains("vault") && welcome["vault"].is_object())
        for (auto it = welcome["vault"].begin(); it != welcome["vault"].end(); ++it)
            if (it.value().is_string()) rt.joined_vault[it.key()] = it.value().get<std::string>();
    rt.join_status = "received " + std::to_string(files.size()) + " files from " + job.offer.user;
    if (auto t = rt.transfers.find("join"); t != rt.transfers.end()) t->second.finished = now_seconds();
    rt.thread_log.push_back({"info", "join: " + rt.join_status + " into " + u8(job.dest)});
    rt.joining = false;
}

}  // namespace lan_detail
