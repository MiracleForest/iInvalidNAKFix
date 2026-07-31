#pragma once
#include <ll/api/mod/NativeMod.h>

namespace mif::invalid_nak_fix {

class iInvalidNAKFix {
public:
    struct HandleSocketReceiveHook;
    struct Config {
        int version{1};
        struct {
            uint64 base{0xFFFFFF};
            uint64 multiplier{10};
        } threshold;
        uint64 maxHistorySize{50};
        uint64 timeWindowMilliseconds{500};
        uint   banDurationMilliseconds{0};
    };

public:
    static iInvalidNAKFix& getInstance();

    iInvalidNAKFix() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }
    [[nodiscard]] Config& getConfig() { return mConfig; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    ll::mod::NativeMod& mSelf;
    Config mConfig;
};

} // namespace mif::invalid_nak_fix