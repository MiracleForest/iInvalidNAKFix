#pragma once
#include <ll/api/base/Macro.h>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace RakNet {
struct SystemAddress {
public:
    union {
        sockaddr_storage sa_stor;
        sockaddr_in6     addr6;
        sockaddr_in      addr4;
    } address;
    ushort debugPort;
    ushort systemIndex;

public:
    bool operator==(SystemAddress const& other) const;

    std::optional<std::string> getIp() const;

public:
    static SystemAddress const UNASSIGNED_SYSTEM_ADDRESS;
};
} // namespace RakNet