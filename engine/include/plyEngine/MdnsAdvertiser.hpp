#pragma once

#include <cstdint>
#include <string>

namespace ply {

/// Advertises the companion server over mDNS/DNS-SD so phones on the same LAN
/// can find it without typing an address.
///
/// Advertising is best-effort: multicast may be blocked (locked-down Wi-Fi,
/// container CI), so failure to advertise never stops the server. The room code
/// shown on screen always remains a working fallback.
class MdnsAdvertiser {
public:
    struct Config {
        /// Host label, without the ".local" suffix (e.g. "plyengine-ab12").
        std::string hostLabel = "plyengine";
        /// Service type, always DNS-SD form with a trailing dot.
        std::string serviceType = "_plyengine._tcp.local.";
        /// Human-readable instance name shown in discovery UIs.
        std::string instanceName = "plyEngine";
        uint16_t port = 0;
        /// Advertised as a TXT record so a client can pre-fill the room code.
        std::string roomCode;
    };

    MdnsAdvertiser();
    ~MdnsAdvertiser();

    MdnsAdvertiser(const MdnsAdvertiser&) = delete;
    MdnsAdvertiser& operator=(const MdnsAdvertiser&) = delete;

    /// Starts the responder thread. Returns false when multicast could not be
    /// set up; `error()` then explains why.
    bool start(const Config& config);
    void stop();

    bool running() const;
    const std::string& error() const;

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace ply
