#include "invalid_nak_fix/SystemAddress.h"
#include <array>

namespace RakNet {

bool SystemAddress::operator==(SystemAddress const& other) const {
    auto& laddr4 = address.addr4;
    auto& laddr6 = address.addr6;
    auto& raddr4 = other.address.addr4;
    auto& raddr6 = other.address.addr6;
    return laddr4.sin_port == raddr4.sin_port
        && ((laddr4.sin_family == AF_INET && laddr4.sin_addr.s_addr == raddr4.sin_addr.s_addr)
            || (laddr4.sin_family == AF_INET6
                && memcmp(laddr6.sin6_addr.s6_addr, raddr6.sin6_addr.s6_addr, sizeof(laddr6.sin6_addr.s6_addr)) == 0));
}

std::optional<std::string> SystemAddress::getIp() const {
    if (*this == UNASSIGNED_SYSTEM_ADDRESS) return std::nullopt;

    auto family = address.sa_stor.ss_family;
    if (family != AF_INET && family != AF_INET6) return std::nullopt;

    std::array<char, INET6_ADDRSTRLEN> dest{};
    if (inet_ntop(
            family,
            family == AF_INET ? static_cast<void const*>(&address.addr4.sin_addr) : &address.addr6.sin6_addr,
            dest.data(),
            INET6_ADDRSTRLEN
        )
        == nullptr) {
        return std::nullopt;
    }

    return std::string{dest.data()};
}

SystemAddress const SystemAddress::UNASSIGNED_SYSTEM_ADDRESS{};

} // namespace RakNet