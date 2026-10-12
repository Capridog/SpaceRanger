// SpaceRanger: definitions for symbols the recompilation now genuinely
// provides have been removed from this file. They existed to trap calls
// into gaps in the older, weaker symbol map; SpaceRanger fills them, so
// keeping the traps here produced duplicate definitions at link time.
#include "recomp.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" void ts2_runtime_owned_vi_manager_trap_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: guest VI-manager thread body 0x80099998 was reached, but N64ModernRuntime VI ownership was expected.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80024190(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80024190 -> func_80024190.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8008FF04(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8008FF04 -> func_8008FF04.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80090390(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80090390 -> func_80090390.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80091AB0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80091AB0 -> func_80091AB0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80091DE0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80091DE0 -> func_80091DE0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80092D70(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80092D70 -> func_80092D70.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80093E90(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80093E90 -> func_80093E90.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80094A30(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80094A30 -> func_80094A30.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80094A40(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80094A40 -> func_80094A40.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_800958A0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x800958A0 -> func_800958A0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80095F04(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80095F04 -> func_80095F04.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80096064(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80096064 -> func_80096064.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80096890(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80096890 -> func_80096890.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80097CD0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80097CD0 -> func_80097CD0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80097CF0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80097CF0 -> func_80097CF0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80098420(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80098420 -> func_80098420.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80098B70(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80098B70 -> func_80098B70.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80098CB0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80098CB0 -> func_80098CB0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_80099714(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80099714 -> func_80099714.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_800997A0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x800997A0 -> func_800997A0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009A430(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009A430 -> func_8009A430.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009A490(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009A490 -> func_8009A490.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009AE90(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009AE90 -> func_8009AE90.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009AF70(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009AF70 -> func_8009AF70.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009B150(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009B150 -> func_8009B150.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009BA20(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009BA20 -> func_8009BA20.\n");
    std::fprintf(stderr, "Reason held: old low-level TLB helper; do not restore blindly.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009C330(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009C330 -> func_8009C330.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009C520(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009C520 -> func_8009C520.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009C54C(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009C54C -> func_8009C54C.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009D010(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009D010 -> func_8009D010.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009D150(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009D150 -> func_8009D150.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009D2D0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009D2D0 -> func_8009D2D0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009E230(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009E230 -> func_8009E230.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009F9D4(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009F9D4 -> func_8009F9D4.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_800A05F0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x800A05F0 -> func_800A05F0.\n");
    std::fflush(stderr);
    std::abort();
}


// Restore the historical trap: host lookup still registers this address.
extern "C" void func_80096D00(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80096D00 -> func_80096D00.\n");
    std::fflush(stderr);
    std::abort();
}

// Restored: these three guest addresses touch MMIO (SI / AI / PIF domain).
// Their generated forms index far outside RDRAM and fault the process, so the
// addresses are marked `ignored` in the recomp config and these definitions
// stand in. They are diagnostics, not implementations: reaching one means a
// path the old native build never exercised is now live and needs a real
// host implementation rather than a silent fault.

extern "C" void func_80098240(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x80098240 -> func_80098240.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_8009C2C0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x8009C2C0 -> func_8009C2C0.\n");
    std::fflush(stderr);
    std::abort();
}

extern "C" void func_800A06A0(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: unresolved historical frontend mapping reached: guest 0x800A06A0 -> func_800A06A0.\n");
    std::fflush(stderr);
    std::abort();
}
