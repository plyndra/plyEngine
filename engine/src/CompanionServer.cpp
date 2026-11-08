#include "plyEngine/CompanionServer.hpp"

#include "SocketCompat.hpp"

#include <crow.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace ply {

namespace {

double monotonicSeconds() {
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point epoch = Clock::now();
    return std::chrono::duration<double>(Clock::now() - epoch).count();
}

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

const char* contentTypeFor(const std::string& path) {
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".html") == 0) {
        return "text/html; charset=utf-8";
    }
    if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".js") == 0) {
        return "text/javascript; charset=utf-8";
    }
    if (path.size() >= 8 && path.compare(path.size() - 8, 8, ".mjs") == 0) {
        return "text/javascript; charset=utf-8";
    }
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".css") == 0) {
        return "text/css; charset=utf-8";
    }
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".svg") == 0) {
        return "image/svg+xml";
    }
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0) {
        return "application/json";
    }
    return "application/octet-stream";
}

// Rejects traversal and anything that is not a simple relative file name.
bool safeRelativePath(const std::string& path) {
    if (path.empty() || path[0] == '/') {
        return false;
    }
    if (path.find("..") != std::string::npos) {
        return false;
    }
    for (char c : path) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                        c == '/';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string localIPv4String() {
    ply::net::ensureInit();
    const auto sock = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
    if (sock < 0) {
        return "127.0.0.1";
    }
    struct sockaddr_in remote {};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    remote.sin_addr.s_addr = inet_addr("8.8.8.8");
    std::string result = "127.0.0.1";
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&remote), sizeof(remote)) == 0) {
        struct sockaddr_in local {};
        ply::net::SockLen len = sizeof(local);
        if (getsockname(sock, reinterpret_cast<struct sockaddr*>(&local), &len) == 0) {
            char buffer[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &local.sin_addr, buffer, sizeof(buffer))) {
                result = buffer;
            }
        }
    }
    ply::net::closeSocket(sock);
    return result;
}

int findFreePort() {
    ply::net::ensureInit();
    const auto sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (sock < 0) {
        return 8080;
    }
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0;
    int port = 8080;
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) == 0) {
        ply::net::SockLen len = sizeof(address);
        if (getsockname(sock, reinterpret_cast<struct sockaddr*>(&address), &len) == 0) {
            port = ntohs(address.sin_port);
        }
    }
    ply::net::closeSocket(sock);
    return port;
}

}  // namespace

struct CompanionServer::Impl {
    Config config;
    crow::SimpleApp app;
    std::thread thread;

    std::mutex sessionMutex;
    companion::CompanionSession* session = nullptr;

    std::mutex connMutex;
    std::unordered_map<crow::websocket::connection*, int> connSlots;

    MdnsAdvertiser mdns;
    std::string ip;
    int port = 0;
    std::atomic<bool> active{false};
    std::string error;

    ~Impl() { delete session; }
};

CompanionServer::CompanionServer() : impl_(std::make_unique<Impl>()) {}

CompanionServer::~CompanionServer() { stop(); }

bool CompanionServer::running() const { return impl_ && impl_->active.load(); }
int CompanionServer::port() const { return impl_ ? impl_->port : 0; }

std::string CompanionServer::roomCode() const {
    if (!impl_ || !impl_->session) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->sessionMutex);
    return impl_->session->room().value();
}

std::string CompanionServer::url() const {
    if (!impl_) {
        return {};
    }
    return "http://" + impl_->ip + ":" + std::to_string(impl_->port);
}

std::vector<companion::SlotInfo> CompanionServer::slots() const {
    if (!impl_ || !impl_->session) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->sessionMutex);
    return impl_->session->slots();
}

int CompanionServer::connectedCount() const {
    if (!impl_ || !impl_->session) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(impl_->sessionMutex);
    return impl_->session->connectedCount();
}

bool CompanionServer::mdnsActive() const { return impl_ && impl_->mdns.running(); }

const std::string& CompanionServer::error() const {
    static const std::string empty;
    return impl_ ? impl_->error : empty;
}

void CompanionServer::update(double now) {
    if (!impl_ || !impl_->session) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(impl_->sessionMutex);
        impl_->session->update(now);
    }
    input_.drain();
}

bool CompanionServer::start(const Config& config) {
    if (!impl_) {
        return false;
    }
    if (impl_->active.load()) {
        return false;
    }

    impl_->config = config;
    impl_->ip = localIPv4String();
    impl_->port = config.port > 0 ? config.port : findFreePort();

    const companion::RoomCode room = companion::RoomCode::generate();
    impl_->session = new companion::CompanionSession(
        config.slotCount, config.reconnectGrace, room);

    const std::string webRoot = config.webRoot;

    // --- HTTP surface -----------------------------------------------------
    auto serveFile = [webRoot](crow::response& res, const std::string& relative) {
        if (!safeRelativePath(relative)) {
            res.code = 400;
            res.body = "bad path";
            res.end();
            return;
        }
        const std::string content = readFile(webRoot + "/" + relative);
        if (content.empty()) {
            res.code = 404;
            res.body = "not found: " + relative;
            res.end();
            return;
        }
        res.code = 200;
        res.set_header("Content-Type", contentTypeFor(relative));
        res.set_header("Cache-Control", "no-store");
        res.body = content;
        res.end();
    };

    CROW_ROUTE(impl_->app, "/")
    ([serveFile](const crow::request&, crow::response& res) {
        serveFile(res, "index.html");
    });

    CROW_ROUTE(impl_->app, "/api/room")
    ([this]() {
        nlohmann::json body;
        body["room"] = roomCode();
        body["port"] = impl_->port;
        body["url"] = url();
        body["slots"] = impl_->config.slotCount;
        body["protocol"] = companion::kProtocolVersion;
        return crow::response(body.dump());
    });

    // --- WebSocket surface ------------------------------------------------
    CROW_WEBSOCKET_ROUTE(impl_->app, "/ws")
        .onopen([this](crow::websocket::connection& conn) {
            nlohmann::json payload;
            payload["room"] = impl_->session ? impl_->session->room().value() : "";
            payload["slots"] = impl_->config.slotCount;
            payload["protocol"] = companion::kProtocolVersion;
            conn.send_text(companion::encodeMessage(companion::kTypeHello, 0, payload));
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& data,
                          bool isBinary) {
            if (isBinary) {
                return;
            }
            companion::Message message;
            std::string decodeError;
            if (!companion::decodeMessage(data, message, decodeError)) {
                nlohmann::json payload;
                payload["reason"] = decodeError;
                conn.send_text(
                    companion::encodeMessage(companion::kTypeRejected, 0, payload));
                return;
            }

            if (message.type == companion::kTypeJoin) {
                const std::string roomCodeValue =
                    message.payload.contains("room") && message.payload["room"].is_string()
                        ? message.payload["room"].get<std::string>()
                        : "";
                const std::string name =
                    message.payload.contains("name") && message.payload["name"].is_string()
                        ? message.payload["name"].get<std::string>()
                        : "";

                int slot = -1;
                std::string joinError;
                {
                    std::lock_guard<std::mutex> lock(impl_->sessionMutex);
                    slot = impl_->session->join(roomCodeValue, name, monotonicSeconds(),
                                                joinError);
                }

                if (slot < 0) {
                    nlohmann::json payload;
                    payload["reason"] = joinError;
                    conn.send_text(companion::encodeMessage(
                        companion::kTypeRejected, message.seq, payload));
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(impl_->connMutex);
                    impl_->connSlots[&conn] = slot;
                }

                nlohmann::json payload;
                payload["slot"] = slot;
                if (const companion::SlotInfo* info = impl_->session->slot(slot)) {
                    payload["colour"] = info->colourIndex;
                    payload["name"] = info->name;
                }
                conn.send_text(
                    companion::encodeMessage(companion::kTypeJoined, message.seq, payload));
                return;
            }

            if (message.type == companion::kTypeInput) {
                int slot = -1;
                {
                    std::lock_guard<std::mutex> lock(impl_->connMutex);
                    auto it = impl_->connSlots.find(&conn);
                    if (it != impl_->connSlots.end()) {
                        slot = it->second;
                    }
                }
                if (slot < 0) {
                    return;  // input before join is ignored
                }
                companion::ControllerState state;
                std::string inputError;
                if (!companion::decodeController(message.payload, state, inputError)) {
                    return;
                }
                input_.push(slot, state, message.seq);
                return;
            }

            if (message.type == companion::kTypePing) {
                conn.send_text(companion::encodeMessage(companion::kTypePong,
                                                        message.seq, {}));
                return;
            }
        })
        .onclose([this](crow::websocket::connection& conn, const std::string&,
                        uint16_t) {
            int slot = -1;
            {
                std::lock_guard<std::mutex> lock(impl_->connMutex);
                auto it = impl_->connSlots.find(&conn);
                if (it != impl_->connSlots.end()) {
                    slot = it->second;
                    impl_->connSlots.erase(it);
                }
            }
            if (slot < 0) {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(impl_->sessionMutex);
                impl_->session->setConnected(slot, false, monotonicSeconds());
            }
            input_.clearSlot(slot);
        });

    // The catch-all must be registered LAST: Crow matches in registration
    // order, so a wildcard route declared earlier would swallow /api/room
    // and /ws.
    CROW_ROUTE(impl_->app, "/<path>")
    ([serveFile](const crow::request&, crow::response& res, const std::string& path) {
        serveFile(res, path);
    });

    // --- start ------------------------------------------------------------
    impl_->active = true;
    impl_->thread = std::thread([this] {
        impl_->app.port(static_cast<uint16_t>(impl_->port)).bindaddr("0.0.0.0").run();
    });

    if (config.enableMdns) {
        MdnsAdvertiser::Config mdnsConfig;
        mdnsConfig.hostLabel = config.hostLabel;
        mdnsConfig.instanceName = config.instanceName;
        mdnsConfig.port = static_cast<uint16_t>(impl_->port);
        mdnsConfig.roomCode = room.value();
        if (!impl_->mdns.start(mdnsConfig)) {
            std::fprintf(stderr,
                         "plyEngine: mDNS advertising unavailable (%s); "
                         "players can still share the room code\n",
                         impl_->mdns.error().c_str());
        }
    }

    return true;
}

void CompanionServer::stop() {
    if (!impl_) {
        return;
    }
    impl_->mdns.stop();
    if (impl_->active.exchange(false)) {
        impl_->app.stop();
        if (impl_->thread.joinable()) {
            impl_->thread.join();
        }
    }
}

}  // namespace ply
