// clang-format off
#pragma include_alias("mc/deps/raknet/SystemAddress.h", "invalid_nak_fix/SystemAddress.h")
#pragma include_alias(<mc/deps/raknet/SystemAddress.h>, <invalid_nak_fix/SystemAddress.h>)
// clang-format on

#include "invalid_nak_fix/iInvalidNAKFix.h"
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/RegisterHelper.h>
#include <mc/deps/raknet/RakNet.h>
#include <mc/deps/raknet/RakPeer.h>
#include <mc/deps/raknet/SystemAddress.h>

namespace mif::invalid_nak_fix {

using namespace ll::memory_literals;

#if defined(VERSION_1_21_50_10) || defined(VERSION_1_21_60_10)
#  define FUNC_IDENTIFIER                                                                                              \
      "?ProcessNetworkPacket@RakNet@@YAXUSystemAddress@1@PEBDHPEAVRakPeer@1@PEAVRakNetSocket2@1@_KAEAVBitStream@1@@Z"_sym
#elif defined(VERSION_1_21_7004) || defined(VERSION_1_21_80) || defined(VERSION_1_21_93) || defined(VERSION_1_21_102)
#  define FUNC_IDENTIFIER                                                                                              \
      "48 89 5C 24 ?? 48 89 74 24 ?? 55 41 54 41 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 0F 28 01"_sig
#elif defined(VERSION_1_21_111) || defined(VERSION_1_21_120) || defined(VERSION_1_21_124) || defined(VERSION_1_21_132) \
    || defined(VERSION_26_10_4)
#  define FUNC_IDENTIFIER "48 89 5C 24 ?? 55 41 54 41 55 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 0F 28 01"_sig
#elif defined(VERSION_26_20_5)
#  define FUNC_IDENTIFIER "41 57 41 56 41 55 41 54 56 57 53 48 81 EC ?? ?? ?? ?? 4D 89 CE 44 89 C6"_sig
#else
#  error "Unsupported version"
#endif

LL_STATIC_HOOK(
    iInvalidNAKFix::HandleSocketReceiveHook,
    HookPriority::Normal,
    FUNC_IDENTIFIER,
    void,
    RakNet::SystemAddress  systemAddress,
    char const*            data,
    int                    length,
    RakNet::RakPeer*       rakPeer,
    RakNet::RakNetSocket2* rakNetSocket,
    uint64                 timeRead,
    RakNet::BitStream&     updateBitStream
) {
    auto origin = [&]() {
        return HandleSocketReceiveHook::origin(
            systemAddress,
            data,
            length,
            rakPeer,
            rakNetSocket,
            timeRead,
            updateBitStream
        );
    };
    static constexpr auto readUint24BE = [](uint8_t const* ptr) constexpr {
        return (static_cast<uint32_t>(ptr[0]) << 16) | (static_cast<uint32_t>(ptr[1]) << 8)
             | (static_cast<uint32_t>(ptr[2]));
    };
    if (length < 1) return origin();


    auto header  = static_cast<uint8_t>(data[0]);
    auto isValid = (header >> 7) & 1;
    auto isACK   = (header >> 6) & 1;
    auto isNAK   = (header >> 5) & 1;
    if (!isValid || !isNAK || isACK) return origin();
    if (length < 3) return origin();

    auto address = systemAddress.getIp();
    if (address && rakPeer->IsBanned(address->c_str())) return;

    auto rangeCount = (static_cast<uint16_t>(data[1]) << 8) | static_cast<uint16_t>(data[2]);

    for (int offset = 3, index = 0; index < rangeCount; ++index) {
        if (offset >= length) return origin();

        auto flags   = static_cast<uint8_t>(data[offset++]);
        auto isRange = (flags == 0);

        if (offset + 3 > length) return origin();
        auto minVal  = readUint24BE(reinterpret_cast<uint8_t const*>(data + offset));
        offset      += 3;

        auto maxVal = minVal;
        if (isRange) {
            if (offset + 3 > length) return origin();
            maxVal  = readUint24BE(reinterpret_cast<uint8_t const*>(data + offset));
            offset += 3;
        }

        if (maxVal == 0xFFFFFF) {
            // getInstance().getSelf().getLogger().warn(
            //     "Dangerous NAK range [{0}, 0xFFFFFF] received from {1}",
            //     minVal,
            //     address.value_or("Unknown Address")
            // );
            if (address) rakPeer->AddToBanList(address->c_str(), 0);
            return;
        }
    }

    return origin();
}

iInvalidNAKFix& iInvalidNAKFix::getInstance() {
    static iInvalidNAKFix instance;
    return instance;
}

bool iInvalidNAKFix::load() { return true; }

bool iInvalidNAKFix::enable() {
    ll::memory::HookRegistrar<HandleSocketReceiveHook>::hook();
    return true;
}

bool iInvalidNAKFix::disable() {
    ll::memory::HookRegistrar<HandleSocketReceiveHook>::unhook();
    return true;
}

bool iInvalidNAKFix::unload() { return true; }

LL_REGISTER_MOD(iInvalidNAKFix, iInvalidNAKFix::getInstance());

} // namespace mif::invalid_nak_fix