// clang-format off
#pragma include_alias("mc/deps/raknet/SystemAddress.h", "invalid_nak_fix/SystemAddress.h")
#pragma include_alias(<mc/deps/raknet/SystemAddress.h>, <invalid_nak_fix/SystemAddress.h>)
// clang-format on
#include "invalid_nak_fix/iInvalidNAKFix.h"
#include <ll/api/Config.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/ErrorUtils.h>
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
    static constexpr auto readUint24LE = [](uint8_t const* ptr) constexpr -> uint32_t {
        return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8)
             | (static_cast<uint32_t>(ptr[2]) << 16);
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

    auto   rangeCount = (static_cast<uint16_t>(static_cast<uint8_t>(data[1])) << 8)
                      | static_cast<uint16_t>(static_cast<uint8_t>(data[2]));
    uint64 totalIntervalSize{0};
    bool   directBan{false};

    for (int offset = 3, index = 0; index < rangeCount; ++index) {
        if (offset >= length) return origin();

        auto flags   = static_cast<uint8_t>(data[offset++]);
        auto isRange = (flags == 0);

        if (offset + 3 > length) return origin();
        auto minVal  = readUint24LE(reinterpret_cast<uint8_t const*>(data + offset));
        offset      += 3;

        auto maxVal = minVal;
        if (isRange) {
            if (offset + 3 > length) return origin();
            maxVal  = readUint24LE(reinterpret_cast<uint8_t const*>(data + offset));
            offset += 3;
        }

        if (maxVal < minVal) {
            // getInstance().getSelf().getLogger().warn(
            //     "Invalid reversed NAK range from {0}: min={1}, max={2}; packet dropped",
            //     address.value_or("Unknown Address"),
            //     minVal,
            //     maxVal
            // );
            return;
        }

        if (minVal == 0 && maxVal == 0xFFFFFF) {
            directBan = true;
            break;
        }

        auto intervalSize = static_cast<uint64_t>(maxVal) - static_cast<uint64_t>(minVal) + 1;
        if (totalIntervalSize > std::numeric_limits<uint64>::max() - intervalSize) {
            // getInstance().getSelf().getLogger().warn(
            //     "NAK interval counter overflow from {0}; packet dropped",
            //     address.value_or("Unknown Address")
            // );
            return;
        }
        totalIntervalSize += intervalSize;
    }

    if (directBan) {
        if (address) {
            // getInstance().getSelf().getLogger().warn(
            //     "Dangerous NAK range [0, 0xFFFFFF] received from {0}",
            //     address.value_or("Unknown Address")
            // );
            rakPeer->AddToBanList(address->c_str(), 0);
        }
        return;
    }

    if (!address) return origin();

    {
        // clang-format off
        struct NakEvent {
            uint64                                mIntervalSize{};
            std::chrono::steady_clock::time_point mTime{};
        };
        static ll::DenseMap<std::string, std::deque<NakEvent>> vsIpHistory;

        auto& config = getInstance().getConfig();
        if (vsIpHistory.size() >= config.maxHistorySize && config.maxHistorySize > 0) vsIpHistory.clear();

        auto now = std::chrono::steady_clock::now();
        auto maxUint64 = std::numeric_limits<uint64>::max();
        auto threshold =
            config.threshold.multiplier != 0 && config.threshold.base > maxUint64 / config.threshold.multiplier
                ? maxUint64
                : config.threshold.base * config.threshold.multiplier;
        auto shouldDrop{false};

        auto processDeque = [&](auto& deque) {
            auto window = std::chrono::milliseconds{config.timeWindowMilliseconds};
            while (!deque.empty() && (now - deque.front().mTime) > window) deque.pop_front();

            uint64 accumulated{0};
            for (auto& event : deque) {
                if (accumulated > maxUint64 - event.mIntervalSize) {
                    accumulated = maxUint64;
                    break;
                }
                accumulated += event.mIntervalSize;
            }

            auto currentTotal = accumulated > maxUint64 - totalIntervalSize
                ? maxUint64
                : accumulated + totalIntervalSize;

            if (currentTotal > threshold) {
                // getInstance().getSelf().getLogger().warn(
                //     "NAK flood from {0}: {1} intervals in {2}ms (threshold {3})",
                //     *address,
                //     currentTotal,
                //     config.timeWindowMilliseconds,
                //     threshold
                // );
                rakPeer->AddToBanList(address->c_str(), config.banDurationMilliseconds);
                shouldDrop = true;
                return;
            }

            deque.emplace_back(totalIntervalSize, now);
        };

        vsIpHistory.lazy_emplace_l(
            *address,
            [&](auto& pair) { processDeque(pair.second); },
            [&](auto&& ctor) {
                std::deque<NakEvent> deque;
                processDeque(deque);
                ctor(*address, std::move(deque));
            }
        );

        if (shouldDrop) return;
    }

    return origin();
}

iInvalidNAKFix& iInvalidNAKFix::getInstance() {
    static iInvalidNAKFix instance;
    return instance;
}

bool iInvalidNAKFix::load() {
    {
        auto path = getSelf().getConfigDir() / u8"config.json";
        try {
            ll::config::loadConfig(mConfig, path);
        } catch (...) {
            getSelf().getLogger().error("Failed to load config file");
            ll::error_utils::printCurrentException(getSelf().getLogger());
        }
        ll::config::saveConfig(mConfig, path);
    }
    return true;
}

bool iInvalidNAKFix::enable() {
    ll::memory::HookRegistrar<HandleSocketReceiveHook>::hook();
    return true;
}

bool iInvalidNAKFix::disable() {
    ll::memory::HookRegistrar<HandleSocketReceiveHook>::unhook();
    ll::service::getRakPeer().and_then(&RakNet::RakPeer::ClearBanList);
    return true;
}

bool iInvalidNAKFix::unload() { return true; }

LL_REGISTER_MOD(iInvalidNAKFix, iInvalidNAKFix::getInstance());

} // namespace mif::invalid_nak_fix