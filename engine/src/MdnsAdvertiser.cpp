#include "plyEngine/MdnsAdvertiser.hpp"

#include "SocketCompat.hpp"

#include <mdns.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ply {

namespace {

// Records and their backing strings must outlive the responder thread.
struct ServiceData {
    mdns_record_t recordPtr{};
    mdns_record_t recordSrv{};
    mdns_record_t recordA{};
    mdns_record_t recordTxt{};

    std::string serviceType;      // "_plyengine._tcp.local."
    std::string hostname;         // "plyengine-ab12.local."
    std::string instance;         // "plyEngine-ab12._plyengine._tcp.local."
    std::string txtRoomValue;     // "ROOM=ABCD"

    mdns_string_t serviceTypeStr{};
    mdns_string_t hostnameStr{};
    mdns_string_t instanceStr{};
    mdns_string_t txtValueStr{};

    uint16_t port = 0;
    struct sockaddr_in address {};
};

mdns_string_t toMdnsString(const std::string& value) {
    return mdns_string_t{value.c_str(), value.size()};
}

void bindStrings(ServiceData& data) {
    data.serviceTypeStr = toMdnsString(data.serviceType);
    data.hostnameStr = toMdnsString(data.hostname);
    data.instanceStr = toMdnsString(data.instance);
    data.txtValueStr = toMdnsString(data.txtRoomValue);

    data.recordPtr.name = data.serviceTypeStr;
    data.recordPtr.type = MDNS_RECORDTYPE_PTR;
    data.recordPtr.data.ptr.name = data.instanceStr;

    data.recordSrv.name = data.instanceStr;
    data.recordSrv.type = MDNS_RECORDTYPE_SRV;
    data.recordSrv.data.srv.priority = 0;
    data.recordSrv.data.srv.weight = 0;
    data.recordSrv.data.srv.port = data.port;
    data.recordSrv.data.srv.name = data.hostnameStr;

    data.recordA.name = data.hostnameStr;
    data.recordA.type = MDNS_RECORDTYPE_A;
    data.recordA.data.a.addr = data.address;

    data.recordTxt.name = data.instanceStr;
    data.recordTxt.type = MDNS_RECORDTYPE_TXT;
    data.recordTxt.data.txt.key = mdns_string_t{"room", 4};
    data.recordTxt.data.txt.value = data.txtValueStr;
}

bool nameEquals(mdns_string_t name, const std::string& expected) {
    return name.length == expected.size() &&
           std::memcmp(name.str, expected.data(), name.length) == 0;
}

int responderCallback(int sock, const struct sockaddr* from, size_t addrlen,
                      mdns_entry_type_t entry, uint16_t query_id, uint16_t rtype,
                      uint16_t rclass, uint32_t /*ttl*/, const void* data, size_t size,
                      size_t name_offset, size_t /*name_length*/, size_t /*record_offset*/,
                      size_t /*record_length*/, void* user_data) {
    if (entry != MDNS_ENTRYTYPE_QUESTION) {
        return 0;
    }
    auto* service = static_cast<ServiceData*>(user_data);
    if (!service) {
        return 0;
    }

    char nameBuffer[256];
    size_t offset = name_offset;
    const mdns_string_t name =
        mdns_string_extract(data, size, &offset, nameBuffer, sizeof(nameBuffer));

    const bool unicast = (rclass & MDNS_UNICAST_RESPONSE) != 0;
    alignas(4) char sendBuffer[2048];

    auto answer = [&](mdns_record_t record, const mdns_record_t* additional,
                      size_t additionalCount) {
        if (unicast) {
            mdns_query_answer_unicast(sock, from, addrlen, sendBuffer, sizeof(sendBuffer),
                                      query_id, static_cast<mdns_record_type_t>(rtype),
                                      name.str, name.length, record, nullptr, 0, additional,
                                      additionalCount);
        } else {
            mdns_query_answer_multicast(sock, sendBuffer, sizeof(sendBuffer), record, nullptr,
                                        0, additional, additionalCount);
        }
    };

    const std::string dnsSd = "_services._dns-sd._udp.local.";

    if (nameEquals(name, dnsSd) &&
        (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY)) {
        answer(service->recordPtr, nullptr, 0);
        return 0;
    }

    if (nameEquals(name, service->serviceType) &&
        (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY)) {
        const mdns_record_t additional[2] = {service->recordSrv, service->recordA};
        answer(service->recordPtr, additional, 2);
        return 0;
    }

    if (nameEquals(name, service->instance) &&
        (rtype == MDNS_RECORDTYPE_SRV || rtype == MDNS_RECORDTYPE_ANY)) {
        const mdns_record_t additional[2] = {service->recordA, service->recordTxt};
        answer(service->recordSrv, additional, 2);
        return 0;
    }

    if (nameEquals(name, service->hostname) &&
        (rtype == MDNS_RECORDTYPE_A || rtype == MDNS_RECORDTYPE_ANY)) {
        answer(service->recordA, nullptr, 0);
        return 0;
    }

    return 0;  // not ours
}

// Local IPv4 address used in the A record. A connected UDP socket picks the
// outbound interface without sending any traffic.
bool localIPv4(struct sockaddr_in& out) {
    const int sock = static_cast<int>(socket(AF_INET, SOCK_DGRAM, 0));
    if (sock < 0) {
        return false;
    }
    struct sockaddr_in remote {};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(53);
    remote.sin_addr.s_addr = inet_addr("8.8.8.8");
    const bool connected =
        connect(sock, reinterpret_cast<struct sockaddr*>(&remote), sizeof(remote)) == 0;
    bool ok = false;
    if (connected) {
        net::SockLen len = sizeof(out);
        ok = getsockname(sock, reinterpret_cast<struct sockaddr*>(&out), &len) == 0 &&
             out.sin_family == AF_INET;
    }
    net::closeSocket(sock);
    return ok;
}

}  // namespace

struct MdnsAdvertiser::Impl {
    std::thread thread;
    std::atomic<bool> stop{false};
    bool active = false;
    std::string error;
    int sockets[4] = {-1, -1, -1, -1};
    ServiceData service;
};

MdnsAdvertiser::MdnsAdvertiser() : impl_(new Impl) {}

MdnsAdvertiser::~MdnsAdvertiser() {
    stop();
    delete impl_;
}

bool MdnsAdvertiser::running() const { return impl_ && impl_->active; }

const std::string& MdnsAdvertiser::error() const {
    static const std::string empty;
    return impl_ ? impl_->error : empty;
}

bool MdnsAdvertiser::start(const Config& config) {
    if (!impl_ || impl_->active) {
        return false;
    }

    net::ensureInit();  // winsock must be up before the first socket call

    impl_->stop = false;
    impl_->error.clear();

    ServiceData& service = impl_->service;
    service.serviceType = config.serviceType;
    service.hostname = config.hostLabel + ".local.";
    service.instance = config.instanceName + "." + config.serviceType;
    service.txtRoomValue = config.roomCode;
    service.port = config.port;

    struct sockaddr_in address {};
    if (!localIPv4(address)) {
        impl_->error = "no local IPv4 address available";
        return false;
    }
    service.address = address;
    bindStrings(service);

    // Two sockets: one bound to the mDNS port for queries (plus a random-port
    // socket so we can also send). IPv4 only; phones are fine with that.
    struct sockaddr_in bindAddress {};
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_addr.s_addr = INADDR_ANY;
    bindAddress.sin_port = htons(MDNS_PORT);

    int count = 0;
    int sock = mdns_socket_open_ipv4(&bindAddress);
    if (sock < 0) {
        impl_->error = "could not open mDNS socket (multicast may be blocked)";
        return false;
    }
    impl_->sockets[count++] = sock;

    // Short receive timeout so stop() is responsive.
    net::setReceiveTimeout(sock, 0, 250000);

    impl_->active = true;
    impl_->thread = std::thread([this] {
        alignas(4) char buffer[2048];
        while (!impl_->stop.load()) {
            const int sock = impl_->sockets[0];
            if (sock < 0) {
                break;
            }
            mdns_socket_listen(sock, buffer, sizeof(buffer), responderCallback,
                               &impl_->service);
        }
    });

    return true;
}

void MdnsAdvertiser::stop() {
    if (!impl_) {
        return;
    }
    if (impl_->active) {
        impl_->stop = true;
        if (impl_->thread.joinable()) {
            impl_->thread.join();
        }
        impl_->active = false;
    }
    for (int& sock : impl_->sockets) {
        if (sock >= 0) {
            mdns_socket_close(sock);  // portable: closesocket() on Windows
            sock = -1;
        }
    }
}

}  // namespace ply
