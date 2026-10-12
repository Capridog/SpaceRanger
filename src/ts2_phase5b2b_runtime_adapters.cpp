#include <cstdint>
#include "recomp.h"
#include <ultramodern/ultra64.h>

// The game's unnamed VI helpers still update guest scheduler state. Mirror
// their mode/black requests into the native VI renderer without replacing
// that guest bookkeeping.
extern "C" void ts2_vi_mirror_mode(uint8_t* rdram, uint32_t mode_addr) {
    osViSetMode(rdram, static_cast<PTR(OSViMode)>(static_cast<int32_t>(mode_addr)));
}

extern "C" void ts2_vi_mirror_black(uint32_t active) {
    osViBlack(static_cast<uint8_t>(active));
}

/*
 * TS2 Phase 5B2B compatibility:
 *   0x800078D4 == __osGetSR
 *   0x800078E0 == __osSetSR
 */
extern "C" void __osGetSR_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    ctx->r2 = cop0_status_read(ctx);
}

extern "C" void __osSetSR_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    cop0_status_write(ctx, ctx->r4);
}
/*
 * TS2 PHASE5B2B VR4300 SIGNED DIV COMPAT
 *
 * The current 23-TU regeneration emits TS2_DIV_S32(dividend, divisor, lo, hi)
 * at guest MIPS DIV instructions. Preserve R4x00/VR4300 HI/LO behavior while
 * avoiding host x64 signed-divide faults for divisor zero and INT32_MIN / -1.
 */
extern "C" void ts2_div_s32_compat(
    uint64_t dividend_raw,
    uint64_t divisor_raw,
    uint64_t* lo_out,
    uint64_t* hi_out
) {
    const int32_t dividend = static_cast<int32_t>(
        static_cast<uint32_t>(dividend_raw));
    const int32_t divisor = static_cast<int32_t>(
        static_cast<uint32_t>(divisor_raw));

    int32_t quotient;
    int32_t remainder;

    if (divisor == 0) {
        quotient = (dividend < 0) ? 1 : -1;
        remainder = dividend;
    }
    else if (dividend == (-2147483647 - 1) && divisor == -1) {
        quotient = (-2147483647 - 1);
        remainder = 0;
    }
    else {
        quotient = dividend / divisor;
        remainder = dividend % divisor;
    }

    *lo_out = static_cast<uint64_t>(static_cast<int64_t>(quotient));
    *hi_out = static_cast<uint64_t>(static_cast<int64_t>(remainder));
}
/*
 * TS2 PHASE5B2B VR4300 UNSIGNED DIV COMPAT
 *
 * The 23-TU regeneration emits TS2_DIV_U32(dividend, divisor, lo, hi)
 * at guest DIVU instructions. Every current callsite is proven to be followed
 * by the generated guest divisor-zero branch and BREAK 7 path.
 *
 * The zero case here is therefore only a host-safety placeholder so native
 * C/C++ never performs / 0 before the guest break executes. It is not relied
 * on as architectural guest state.
 */
extern "C" void ts2_div_u32_compat(
    uint64_t dividend_raw,
    uint64_t divisor_raw,
    uint64_t* lo_out,
    uint64_t* hi_out
) {
    const uint32_t dividend = static_cast<uint32_t>(dividend_raw);
    const uint32_t divisor = static_cast<uint32_t>(divisor_raw);

    uint32_t quotient;
    uint32_t remainder;

    if (divisor == 0u) {
        quotient = 0xFFFFFFFFu;
        remainder = dividend;
    }
    else {
        quotient = dividend / divisor;
        remainder = dividend % divisor;
    }

    /*
     * Match the stock N64Recomp DIVU emitter's S32(U32(...)) result shape:
     * 32-bit quotient/remainder are sign-extended into the 64-bit temporaries.
     */
    *lo_out = static_cast<uint64_t>(
        static_cast<int64_t>(static_cast<int32_t>(quotient)));
    *hi_out = static_cast<uint64_t>(
        static_cast<int64_t>(static_cast<int32_t>(remainder)));
}
