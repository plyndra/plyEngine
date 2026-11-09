#pragma once

#include "plyEngine/CompanionInput.hpp"
#include "plyEngine/CompanionSession.hpp"
#include "plyEngine/MdnsAdvertiser.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ply {

/// Embedded HTTP + WebSocket server that lets phones act as controllers.
///
/// Serves the companion PWA over HTTP and accepts controller input over a
/// WebSocket. Network threads never touch the simulation: they only call
/// join/setConnected on the session and push() into CompanionInput. The
/// simulation calls update() once per fixed step, which is the only place
/// session state advances and queued input is applied.
class CompanionServer {
public:
    /// Pick the directory to serve the PWA from.
    ///
    /// The shipped release archive lays the built PWA out at `companion/`,
    /// while a source checkout builds it into `companion/dist/` (Vite's
    /// output dir, gitignored). Defaulting to either path breaks the other, so
    /// callers pass a candidate list: the first entry that actually contains
    /// `index.html` wins, and the first entry is returned unchanged when none
    /// match (so the server's own 404 message still names what was asked for).
    static std::string resolveWebRoot(
        const std::vector<std::string>& candidates);

    struct Config {
        /// 0 picks a free ephemeral port.
        int port = 8080;
        int slotCount = 4;
        double reconnectGrace = 20.0;
        /// Directory containing the built PWA (index.html, app.js, style.css).
        std::string webRoot = "companion/dist";
        bool enableMdns = true;
        std::string instanceName = "plyEngine";
        std::string hostLabel = "plyengine";
    };

    CompanionServer();
    ~CompanionServer();

    CompanionServer(const CompanionServer&) = delete;
    CompanionServer& operator=(const CompanionServer&) = delete;

    bool start(const Config& config);
    void stop();

    bool running() const;
    int port() const;
    std::string roomCode() const;
    /// http://<lan-ip>:<port> — what a player types or scans.
    std::string url() const;

    /// Advance session timers and apply queued input. Call once per fixed step.
    void update(double now);

    companion::CompanionInput& input() { return input_; }

    /// Snapshot of the slots (thread-safe copy).
    std::vector<companion::SlotInfo> slots() const;
    int connectedCount() const;

    bool mdnsActive() const;
    const std::string& error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    companion::CompanionInput input_;
};

}  // namespace ply
