// Integration test for CompanionServer: real HTTP requests and a real
// WebSocket handshake + join + input round-trip against a live server.
//
// Deliberately speaks the wire protocol by hand rather than reusing the
// engine's client-side helpers, so the test proves the server's framing.

#include "plyEngine/CompanionServer.hpp"

#include <nlohmann/json.hpp>

#include <arpa/inet.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

int connectTo(int port) {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    struct sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0) {
        close(sock);
        return -1;
    }
    struct timeval timeout {};
    timeout.tv_sec = 3;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return sock;
}

std::string httpGet(int port, const std::string& path) {
    const int sock = connectTo(port);
    if (sock < 0) {
        return {};
    }
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\n" +
                                "Connection: close\r\n\r\n";
    send(sock, request.data(), request.size(), 0);

    std::string response;
    char buffer[4096];
    ssize_t n = 0;
    while ((n = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, static_cast<size_t>(n));
    }
    close(sock);
    return response;
}

bool recvExactly(int sock, void* out, size_t length) {
    auto* cursor = static_cast<unsigned char*>(out);
    size_t got = 0;
    while (got < length) {
        const ssize_t n = recv(sock, cursor + got, length - got, 0);
        if (n <= 0) {
            return false;
        }
        got += static_cast<size_t>(n);
    }
    return true;
}

bool sendAll(int sock, const void* data, size_t length) {
    const auto* cursor = static_cast<const unsigned char*>(data);
    size_t sent = 0;
    while (sent < length) {
        const ssize_t n = send(sock, cursor + sent, length - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

// Sends a client text frame (client frames must be masked).
bool sendText(int sock, const std::string& text) {
    std::vector<unsigned char> frame;
    frame.push_back(0x81);  // FIN + text
    const size_t length = text.size();
    if (length < 126) {
        frame.push_back(static_cast<unsigned char>(0x80 | length));
    } else if (length < 65536) {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<unsigned char>((length >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(length & 0xFF));
    } else {
        return false;
    }
    const unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame.insert(frame.end(), mask, mask + 4);
    for (size_t i = 0; i < length; ++i) {
        frame.push_back(static_cast<unsigned char>(text[i] ^ mask[i % 4]));
    }
    return sendAll(sock, frame.data(), frame.size());
}

// Reads one server text frame (servers do not mask).
bool readText(int sock, std::string& out) {
    unsigned char header[2];
    if (!recvExactly(sock, header, 2)) {
        return false;
    }
    const bool masked = (header[1] & 0x80) != 0;
    uint64_t length = header[1] & 0x7F;
    if (length == 126) {
        unsigned char extended[2];
        if (!recvExactly(sock, extended, 2)) {
            return false;
        }
        length = (static_cast<uint64_t>(extended[0]) << 8) | extended[1];
    } else if (length == 127) {
        unsigned char extended[8];
        if (!recvExactly(sock, extended, 8)) {
            return false;
        }
        length = 0;
        for (unsigned char byte : extended) {
            length = (length << 8) | byte;
        }
    }
    unsigned char mask[4] = {0, 0, 0, 0};
    if (masked && !recvExactly(sock, mask, 4)) {
        return false;
    }
    std::string payload(length, '\0');
    if (length > 0 && !recvExactly(sock, payload.data(), length)) {
        return false;
    }
    if (masked) {
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
        }
    }
    out = std::move(payload);
    return true;
}

// The test serves its own tiny web root so it never depends on a node build
// having produced companion/dist.
std::string makeWebRoot() {
    const std::string dir = "/tmp/ply_companion_webroot";
    mkdir(dir.c_str(), 0755);
    {
        std::ofstream index(dir + "/index.html");
        index << "<!doctype html><title>plyEngine companion test</title>";
    }
    {
        std::ofstream app(dir + "/app.js");
        app << "console.log('companion');";
    }
    return dir;
}

}  // namespace

int main() {
    ply::CompanionServer server;
    ply::CompanionServer::Config config;
    config.port = 0;  // ephemeral
    config.slotCount = 2;
    config.reconnectGrace = 30.0;
    config.webRoot = makeWebRoot();
    config.enableMdns = false;  // multicast is unreliable in CI containers

    if (!server.start(config)) {
        std::fprintf(stderr, "  FAIL: server did not start\n");
        return 1;
    }
    check(server.running(), "server reports running");

    const int port = server.port();
    check(port > 0, "server picked a port");

    // Give Crow a moment to begin accepting.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // --- HTTP: room endpoint ---------------------------------------------
    std::string roomFromApi;
    {
        const std::string response = httpGet(port, "/api/room");
        check(response.rfind("HTTP/1.1 200", 0) == 0, "GET /api/room is 200");
        const size_t bodyStart = response.find("\r\n\r\n");
        check(bodyStart != std::string::npos, "response has a body");
        if (bodyStart != std::string::npos) {
            const nlohmann::json body =
                nlohmann::json::parse(response.substr(bodyStart + 4), nullptr, false);
            check(!body.is_discarded(), "room endpoint returns JSON");
            if (!body.is_discarded()) {
                check(body.contains("room"), "room field present");
                roomFromApi = body["room"].get<std::string>();
                check(roomFromApi.size() == 4, "room code is 4 characters");
                check(body["protocol"].get<int>() == 1, "protocol version advertised");
                check(body["slots"].get<int>() == 2, "slot count advertised");
            }
        }
        check(roomFromApi == server.roomCode(), "API room matches the session");
    }

    // --- HTTP: serves the PWA --------------------------------------------
    {
        const std::string response = httpGet(port, "/");
        check(response.rfind("HTTP/1.1 200", 0) == 0, "GET / serves index.html");
        check(response.find("plyEngine") != std::string::npos,
              "index.html contents are served");
    }

    // --- HTTP: a second file from the web root ---------------------------
    {
        const std::string response = httpGet(port, "/app.js");
        check(response.rfind("HTTP/1.1 200", 0) == 0, "GET /app.js is 200");
        check(response.find("companion") != std::string::npos, "app.js contents are served");
    }

    // --- HTTP: path traversal is rejected --------------------------------
    {
        const std::string response = httpGet(port, "/../CMakeLists.txt");
        check(response.rfind("HTTP/1.1 200", 0) != 0,
              "path traversal is not served");
    }

    // --- HTTP: unknown asset is a 404 ------------------------------------
    {
        const std::string response = httpGet(port, "/definitely-missing.js");
        check(response.find("404") != std::string::npos, "missing asset is 404");
    }

    // --- WebSocket: handshake, hello, join, input ------------------------
    {
        const int sock = connectTo(port);
        check(sock >= 0, "websocket socket connects");
        if (sock >= 0) {
            const std::string upgrade =
                "GET /ws HTTP/1.1\r\n"
                "Host: 127.0.0.1\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n";
            sendAll(sock, upgrade.data(), upgrade.size());

            // Read the handshake response headers.
            std::string headers;
            char byte = 0;
            while (headers.find("\r\n\r\n") == std::string::npos) {
                if (recv(sock, &byte, 1, 0) != 1) {
                    break;
                }
                headers.push_back(byte);
            }
            check(headers.find("101") != std::string::npos,
                  "websocket upgrade returns 101");

            // Server hello.
            std::string helloText;
            check(readText(sock, helloText), "server sends a hello frame");
            const nlohmann::json hello =
                nlohmann::json::parse(helloText, nullptr, false);
            check(!hello.is_discarded(), "hello is JSON");
            if (!hello.is_discarded()) {
                check(hello["type"] == "hello", "first frame is hello");
                check(hello["v"].get<int>() == 1, "hello carries the version");
            }

            // Join with the real room code.
            nlohmann::json join;
            join["v"] = 1;
            join["type"] = "join";
            join["seq"] = 1;
            join["payload"] = {{"room", roomFromApi}, {"name", "TestPlayer"}};
            check(sendText(sock, join.dump()), "join frame sent");

            std::string joinedText;
            check(readText(sock, joinedText), "server replies to join");
            const nlohmann::json joined =
                nlohmann::json::parse(joinedText, nullptr, false);
            check(!joined.is_discarded(), "join reply is JSON");
            if (!joined.is_discarded()) {
                check(joined["type"] == "joined", "join accepted");
                check(joined["payload"]["slot"].get<int>() == 0, "first player is slot 0");
            }
            check(server.connectedCount() == 1, "session shows one connected player");

            // Send input, then drain it on the simulation side.
            nlohmann::json input;
            input["v"] = 1;
            input["type"] = "input";
            input["seq"] = 2;
            input["payload"] = {{"x", 1.0}, {"y", 0.0}, {"buttons", 1}};
            check(sendText(sock, input.dump()), "input frame sent");

            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            server.update(1.0);

            const ply::companion::ControllerState state = server.input().state(0);
            check(state.x == 1.0f, "input axis reached the simulation");
            check(state.buttons == 1u, "button bit reached the simulation");
            check(server.input().pendingCount() == 0, "queue drained by update()");

            close(sock);
        }
    }

    // --- wrong room code is rejected -------------------------------------
    {
        const int sock = connectTo(port);
        if (sock >= 0) {
            const std::string upgrade =
                "GET /ws HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
                "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n";
            sendAll(sock, upgrade.data(), upgrade.size());
            std::string headers;
            char byte = 0;
            while (headers.find("\r\n\r\n") == std::string::npos) {
                if (recv(sock, &byte, 1, 0) != 1) {
                    break;
                }
                headers.push_back(byte);
            }

            std::string helloText;
            readText(sock, helloText);  // hello

            nlohmann::json join;
            join["v"] = 1;
            join["type"] = "join";
            join["seq"] = 1;
            join["payload"] = {{"room", "ZZZZ"}, {"name", "Intruder"}};
            sendText(sock, join.dump());

            std::string reply;
            check(readText(sock, reply), "server replies to a bad join");
            const nlohmann::json parsed = nlohmann::json::parse(reply, nullptr, false);
            if (!parsed.is_discarded()) {
                check(parsed["type"] == "rejected", "wrong room code rejected");
            }
            close(sock);
        }
    }

    // --- a protocol version mismatch is rejected -------------------------
    {
        const int sock = connectTo(port);
        if (sock >= 0) {
            const std::string upgrade =
                "GET /ws HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\n"
                "Connection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n\r\n";
            sendAll(sock, upgrade.data(), upgrade.size());
            std::string headers;
            char byte = 0;
            while (headers.find("\r\n\r\n") == std::string::npos) {
                if (recv(sock, &byte, 1, 0) != 1) {
                    break;
                }
                headers.push_back(byte);
            }
            std::string helloText;
            readText(sock, helloText);

            sendText(sock, R"({"v":99,"type":"join","seq":1,"payload":{}})");
            std::string reply;
            check(readText(sock, reply), "server replies to a bad version");
            const nlohmann::json parsed = nlohmann::json::parse(reply, nullptr, false);
            if (!parsed.is_discarded()) {
                check(parsed["type"] == "rejected", "version mismatch rejected");
                const std::string reason = parsed["payload"]["reason"].get<std::string>();
                check(reason.find("mismatch") != std::string::npos,
                      "rejection explains the version mismatch");
            }
            close(sock);
        }
    }

    server.stop();
    check(!server.running(), "server stops cleanly");

    if (failures == 0) {
        std::printf("companion server tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "companion server tests: %d failure(s)\n", failures);
    return 1;
}
