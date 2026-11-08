// Internal portability shim for the handful of BSD-socket calls plyEngine
// makes outside Crow/asio. Kept private to engine/src: Crow/asio own the
// sockets that matter, these are the throwaway ones used for LAN IP probing,
// free-port discovery and the mDNS receive timeout.
//
// Window's winsock differs in three ways that bite in practice:
//   * sockets are closed with closesocket(), not close()
//   * setsockopt() takes `const char*` for the option value
//   * the sockaddr length parameter is an int, not socklen_t
//   * WSAStartup() must have run before any socket call (Crow/asio usually
//     does it, but these helpers must not depend on call order)

#pragma once

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ply::net {

/// Portable close for a socket owned outside Crow.
inline void closeSocket(int sock) {
    if (sock < 0) {
        return;
    }
#if defined(_WIN32)
    ::closesocket(sock);
#else
    ::close(sock);
#endif
}

/// Type of the sockaddr length argument, which differs between platforms.
#if defined(_WIN32)
using SockLen = int;
#else
using SockLen = socklen_t;
#endif

/// Sets SO_RCVTIMEO from a timeval, with the platform's constness quirk.
inline void setReceiveTimeout(int sock, int seconds, int microseconds) {
    timeval timeout{};
    timeout.tv_sec = seconds;
    timeout.tv_usec = microseconds;
#if defined(_WIN32)
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

/// Ensures winsock is initialised (no-op elsewhere). Safe to call repeatedly.
inline void ensureInit() {
#if defined(_WIN32)
    static const int once = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data);
    }();
    (void)once;
#endif
}

}  // namespace ply::net
