#pragma once
#include <ll/api/mod/NativeMod.h>

namespace mif::invalid_nak_fix {

class iInvalidNAKFix {
public:
    struct HandleSocketReceiveHook;

public:
    static iInvalidNAKFix& getInstance();

    iInvalidNAKFix() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace mif::invalid_nak_fix