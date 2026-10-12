#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "funcs.h"
#include "librecomp/addresses.hpp"

namespace recomp {
    void do_rom_pio(
        uint8_t* rdram,
        gpr ram_address,
        uint32_t physical_addr);
}

/*
 * 0x800982E0 = Toy Story 2's osPiRawReadIo.
 * Use the proven native synchronous ROM PIO primitive instead of A460 MMIO.
 */
extern "C" void func_800982E0(uint8_t* rdram, recomp_context* ctx) {
    const uint32_t dev_addr = static_cast<uint32_t>(ctx->r4);
    const uint32_t result_addr32 = static_cast<uint32_t>(ctx->r5);
    const gpr result_addr =
        static_cast<gpr>(static_cast<int64_t>(
            static_cast<int32_t>(result_addr32)));

    const uint32_t physical_addr =
        recomp::rom_base | (dev_addr & 0x0FFFFFFFu);

    recomp::do_rom_pio(rdram, result_addr, physical_addr);
    ctx->r2 = 0;
}

/*
 * Two obsolete generated timer internals still reference __osSetCompare.
 * Normal timer creation is now routed through native osSetTimer above.
 *
 * This is deliberately a hard safety stop rather than a silent no-op.
 */
extern "C" void __osSetCompare_recomp(
    uint8_t* rdram,
    recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(
        stderr,
        "TS2 OLDNATIVE SAFETY STOP: legacy __osSetCompare path reached "
        "(guest 0x80098B60). Native timer ownership was expected.\\n");
    std::fflush(stderr);
    std::abort();
}
