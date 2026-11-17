/* device_smoke.cpp — sending to a device (platform/device_link.hpp), against a
 * loopback listener standing in for one: it receives exactly what was sent and
 * answers; a port with nothing behind it fails within its timeout and says so;
 * an address that is not one is refused before anything is attempted. */
#include "platform/device_link.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
            ++failures;                                                               \
        }                                                                             \
    } while (0)

int main() {
    using namespace std::chrono_literals;
    // a device: listens, reads what arrives, answers "OK"
    maiz::lan::TcpListener device;
    std::string err;
    CHECK(device.open(0, &err));
    const std::uint16_t port = device.port();

    hormiga::devices::Sender s;
    s.answer_window = 400ms;
    const std::string label = "^XA^FO50,50^FDAna O'Brien^FS^XZ\r\n"; // any bytes: Hormiga never reads them
    CHECK(s.queue("printer", "127.0.0.1", port, label, &err));
    std::unique_ptr<maiz::lan::Tcp> peer;
    std::string got;
    std::vector<hormiga::devices::Report> reports;
    bool answered = false;
    const auto t0 = std::chrono::steady_clock::now();
    while (reports.empty() && std::chrono::steady_clock::now() - t0 < 5s) {
        if (!peer) peer = device.accept();
        if (peer) {
            peer->pump();
            got += peer->take_read();
            if (got.size() >= label.size() && !answered) {
                peer->write("OK");
                peer->pump();
                answered = true;
            }
        }
        for (auto& r : s.pump()) reports.push_back(r);
        std::this_thread::sleep_for(5ms);
    }
    CHECK(got == label); // exactly the bytes, apostrophe and line ending included
    CHECK(reports.size() == 1 && reports[0].ok && reports[0].who == "printer");
    CHECK(!reports.empty() && reports[0].text.find("sent " + std::to_string(label.size()) + " bytes") != std::string::npos);
    CHECK(!reports.empty() && reports[0].answered == "OK"); // what the device said back
    CHECK(!s.busy());

    // nothing behind the port: a failure, said plainly, within the timeout
    device.close();
    s.connect_timeout = 800ms;
    reports.clear();
    if (s.queue("gone", "127.0.0.1", port, "x", &err)) {
        const auto t1 = std::chrono::steady_clock::now();
        while (reports.empty() && std::chrono::steady_clock::now() - t1 < 3s) {
            for (auto& r : s.pump()) reports.push_back(r);
            std::this_thread::sleep_for(5ms);
        }
        CHECK(reports.size() == 1 && !reports[0].ok && reports[0].text.find("could not reach") != std::string::npos);
    } // (some systems refuse at once: queue then fails, which is also a plain answer)

    CHECK(!s.queue("x", "printer.local", 9100, "x", &err) && err.find("IPv4") != std::string::npos);
    CHECK(!s.queue("x", "127.0.0.1", 0, "x", &err));

    if (failures) std::printf("%d failure(s)\n", failures);
    else std::printf("device_smoke: ok\n");
    return failures ? 1 : 0;
}
