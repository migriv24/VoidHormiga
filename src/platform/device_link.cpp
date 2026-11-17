/* platform/device_link.cpp — see device_link.hpp. */
#include "platform/device_link.hpp"

namespace hormiga::devices {

bool Sender::queue(const std::string& who, const std::string& address, std::uint16_t port, const std::string& payload,
                   std::string* error) {
    const auto ip = maiz::lan::Ipv4::parse(address);
    if (!ip) {
        if (error) *error = "'" + address + "' is not an IPv4 address (like 192.168.1.50); names are not looked up yet";
        return false;
    }
    if (port == 0) {
        if (error) *error = "no port";
        return false;
    }
    Live l;
    l.who = who;
    l.where = address + ":" + std::to_string(port);
    l.payload = payload;
    l.tcp = std::make_unique<maiz::lan::Tcp>();
    l.started = std::chrono::steady_clock::now();
    std::string e;
    if (!l.tcp->connect(*ip, port, &e)) {
        if (error) *error = "could not start connecting to " + l.where + (e.empty() ? "" : ": " + e);
        return false;
    }
    live_.push_back(std::move(l));
    return true;
}

std::vector<Report> Sender::pump() {
    std::vector<Report> done;
    const auto now = std::chrono::steady_clock::now();
    for (auto it = live_.begin(); it != live_.end();) {
        Live& l = *it;
        const bool alive = l.tcp->pump();
        if (l.tcp->connected() && !l.wrote) { // connected: write once, then listen a little
            l.tcp->write(l.payload);
            l.tcp->pump();
            l.wrote = true;
            l.written = now;
        }
        l.got += l.tcp->take_read();
        Report r;
        r.who = l.who;
        bool finished = false;
        if (!l.wrote && (!alive || now - l.started > connect_timeout)) {
            r.text = "could not reach " + l.where +
                     (l.tcp->error().empty() ? " (no answer within " + std::to_string(connect_timeout.count() / 1000) + " s)"
                                             : ": " + l.tcp->error());
            finished = true;
        } else if (l.wrote && (!alive || now - l.written > answer_window)) {
            r.ok = true; // written; the device closing on us, or saying nothing, is normal
            r.text = "sent " + std::to_string(l.payload.size()) + " bytes to " + l.where;
            r.answered = l.got;
            finished = true;
        }
        if (finished) {
            l.tcp->close();
            done.push_back(std::move(r));
            it = live_.erase(it);
        } else {
            ++it;
        }
    }
    return done;
}

} // namespace hormiga::devices
