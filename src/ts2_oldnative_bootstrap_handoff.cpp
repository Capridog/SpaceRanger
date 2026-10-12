#include <cstdio>
#include <cstdint>

#include "recomp.h"
#include <ultramodern/ultramodern.hpp>

extern "C" void func_80008000(
    uint8_t* rdram,
    recomp_context* ctx);

extern "C" void recomp_entrypoint(
    uint8_t* rdram,
    recomp_context* ctx);

extern "C" void ts2_oldnative_bootstrap_handoff_recomp(
    uint8_t* rdram,
    recomp_context* ctx) noexcept(false) {

    std::fputs(
        "TS2 OLDNATIVE BOOTSTRAP: before func_80008000.\n",
        stdout);
    std::fflush(stdout);

    func_80008000(rdram, ctx);

    std::fputs(
        "TS2 OLDNATIVE BOOTSTRAP: first thread started; "
        "unwinding direct GameEntry caller.\n",
        stdout);
    std::fflush(stdout);

    throw ultramodern::thread_terminated{};
}

extern "C" int ts2_oldnative_call_recomp_entrypoint_catching_bootstrap(
    uint8_t* rdram,
    recomp_context* ctx) noexcept {

    // TS2 OLDNATIVE V26 LOCAL C++ EH SELF-TEST.
    // This occurs before recomp_entrypoint. If this exact type is not caught here,
    // the helper TU/compiler exception model is the failure boundary.
    try {
        throw ultramodern::thread_terminated{};
    }
    catch (ultramodern::thread_terminated&) {
        std::fputs(
            "TS2 OLDNATIVE EH SELFTEST V26: local thread_terminated catch PASS.\n",
            stdout);
        std::fflush(stdout);
    }


    try {
        recomp_entrypoint(rdram, ctx);
        return 0;
    }
    catch (ultramodern::thread_terminated&) {
        std::fputs(
            "TS2 OLDNATIVE BOOTSTRAP: expected thread_terminated "
            "caught at Stage61 direct-entry boundary.\n",
            stdout);
        std::fflush(stdout);
        return 1;
    }
}
