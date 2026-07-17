#include "invalid_nak_fix/iInvalidNAKFix.h"
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/RegisterHelper.h>
#include <mc/deps/raknet/ReliabilityLayer.h>
// #include <mc/deps/raknet/RakNet.h>
// #include <mc/deps/raknet/SystemAddress.h>

namespace mif::invalid_nak_fix {

using namespace ll::memory_literals;

#if defined(VERSION_1_21_50_10) || defined(VERSION_1_21_60_10)
#  define FUNC_IDENTIFIER &RakNet::ReliabilityLayer::HandleSocketReceiveFromConnectedPlayer
#elif defined(VERSION_1_21_7004) || defined(VERSION_1_21_80) || defined(VERSION_1_21_93) || defined(VERSION_1_21_102)
#  define FUNC_IDENTIFIER                                                                                              \
      "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 ?? ?? ?? ?? 4D 8B E1"_sig
#elif defined(VERSION_1_21_111) || defined(VERSION_1_21_120) || defined(VERSION_1_21_124)
#  define FUNC_IDENTIFIER                                                                                              \
      "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 ?? ?? ?? ?? 4D 8B E1 45 8B F8"_sig
#elif defined(VERSION_1_21_132) || defined(VERSION_26_10_4)
#  define FUNC_IDENTIFIER                                                                                              \
      "48 8B C4 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 0F 29 70 ?? 0F 29 78 ?? 44 0F 29 40 ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 ?? ?? ?? ?? 4D 8B E1 41 8B F8"_sig
#elif defined(VERSION_26_20_5)
#  define FUNC_IDENTIFIER                                                                                              \
      "55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ?? ?? ?? ?? 48 8D AC 24 ?? ?? ?? ?? 48 C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 4C 89 8D ?? ?? ?? ?? 45 89 C5"_sig
#else
#  error "Unsupported version"
#endif

LL_TYPE_INSTANCE_HOOK(
    iInvalidNAKFix::HandleSocketReceiveHook,
    HookPriority::Normal,
    RakNet::ReliabilityLayer,
    FUNC_IDENTIFIER,
    bool,
    char const*                                      buffer,
    uint                                             length,
    RakNet::SystemAddress&                           systemAddress,
    DataStructures::List<RakNet::PluginInterface2*>& messageHandlerList,
    int                                              MTUSize,
    RakNet::RakNetSocket2*                           s,
    RakNet::RakNetRandom*                            rnr,
    uint64                                           timeRead,
    RakNet::BitStream&                               updateBitStream
) {
    // clang-format on
    static std::array<std::byte, 10> invalidNAKPacket{
        std::byte{0xA0},                                     // 数据报头部: bit[7]=isValid, bit[5]=isNAK -> 0b10100000
        std::byte{0x00}, std::byte{0x01},                    // 区间个数: 1（大端无符号 16 位）
        std::byte{0x00},                                     // 区间标志: 0 = 范围（需读取 min 和 max）
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00},   // min: 0
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}    // max: 0xFFFFFF (16777215)
    };
    // clang-format on

    if (length < 10 || std::memcmp(buffer, invalidNAKPacket.data(), 10) != 0) {
        return origin(buffer, length, systemAddress, messageHandlerList, MTUSize, s, rnr, timeRead, updateBitStream);
    }

    // getInstance().getSelf().getLogger().warn("Invalid NAK Packet received from {0}", [&]() {
    //     if (systemAddress != RakNet::UNASSIGNED_SYSTEM_ADDRESS()) {
    //         std::array<char, 71> buffer{};
    //         systemAddress.ToString(false, buffer.data(), '|');
    //         return strlen(buffer.data()) > 0 ? std::string{buffer.data()} : std::string{"Unknown Address"};
    //     }
    //     return std::string{"Unknown Address"};
    // }());
    return false;
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