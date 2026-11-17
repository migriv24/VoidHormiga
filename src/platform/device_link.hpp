/* platform/device_link.hpp — sending to a device, without blocking a frame
 * (okf/concepts/platform/shell.md, "Devices").
 *
 * The author (2026-11-15): *"hardware communication should be what we offer ...
 * Drivers, connecting to other devices, and sending data somewhere to do an
 * action (like printing), would be something for the ant farm as well."* So
 * Hormiga never learns what a label or a sign is: an Antfarm `device` node says
 * where a device is, an action says what to send, and this carries the bytes.
 *
 * THE TRANSPORT IS VOID HAMMER'S (`maiz::lan::Tcp` in voidhammer_reach, the
 * family's reaching-devices code): non-blocking, pumped once a frame, so a slow
 * or absent device never freezes the window. A send connects, writes, waits a
 * short while for an answer (some devices reply, most say nothing), closes, and
 * reports: what was sent, where, and anything the device answered. TCP first;
 * UDP, HTTP, Bluetooth LE and serial join the same queue later.
 *
 * Plain C++ and Void Hammer, no ImGui: testable against a loopback listener
 * (tests/device_smoke.cpp). */
#pragma once

#include "voidmaiz/lan.hpp" // Void Hammer's voidhammer_reach, through Void Maiz's forwarding header

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hormiga::devices {

struct Report {
    std::string who;      // the device node's name
    bool ok = false;
    std::string text;     // "sent 42 bytes to 192.168.1.50:9100", or why not
    std::string answered; // what the device sent back, if anything
};

class Sender {
  public:
    /* Queue a send. False with `error` when the address cannot be used at all. */
    bool queue(const std::string& who, const std::string& address, std::uint16_t port, const std::string& payload,
               std::string* error);
    /* Once a frame: move bytes, finish what is done. Returns the sends that ended. */
    std::vector<Report> pump();
    bool busy() const { return !live_.empty(); }

    std::chrono::milliseconds connect_timeout{3000};
    std::chrono::milliseconds answer_window{700}; // how long to listen after writing

  private:
    struct Live {
        std::string who, where, payload, got;
        std::unique_ptr<maiz::lan::Tcp> tcp;
        std::chrono::steady_clock::time_point started, written;
        bool wrote = false;
    };
    std::vector<Live> live_;
};

} // namespace hormiga::devices
