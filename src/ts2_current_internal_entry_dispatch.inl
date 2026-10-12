#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

namespace {
std::mutex g_ts2_current_internal_mutex;
std::unordered_map<recomp_context*, uint32_t> g_ts2_current_internal;

void ts2_current_set_internal_entry(recomp_context* ctx, uint32_t pc) {
    std::lock_guard<std::mutex> lock(g_ts2_current_internal_mutex);
    const auto r = g_ts2_current_internal.emplace(ctx, pc);
    if (!r.second) {
        std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: duplicate internal entry ctx=%p old=%08X new=%08X\n",
            static_cast<void*>(ctx), r.first->second, pc);
        std::fflush(stderr);
        std::abort();
    }
}

void ts2_current_verify_internal_consumed(recomp_context* ctx, uint32_t pc) {
    std::lock_guard<std::mutex> lock(g_ts2_current_internal_mutex);
    const auto it = g_ts2_current_internal.find(ctx);
    if (it != g_ts2_current_internal.end()) {
        std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: internal entry %08X not consumed ctx=%p\n",
            pc, static_cast<void*>(ctx));
        std::fflush(stderr);
        std::abort();
    }
}
} // namespace

extern "C" int ts2_current_take_internal_entry(recomp_context* ctx, uint32_t expected_pc) {
    std::lock_guard<std::mutex> lock(g_ts2_current_internal_mutex);
    const auto it = g_ts2_current_internal.find(ctx);
    if (it == g_ts2_current_internal.end()) return 0;
    if (it->second != expected_pc) {
        std::fprintf(stderr, "TS2 OLDNATIVE SAFETY STOP: internal entry mismatch ctx=%p expected=%08X actual=%08X\n",
            static_cast<void*>(ctx), expected_pc, it->second);
        std::fflush(stderr);
        std::abort();
    }
    g_ts2_current_internal.erase(it);
    return 1;
}

extern "C" void ts2_current_internal_entry_80093630_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80093630u);
    func_80093520(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80093630u);
}

extern "C" void ts2_current_internal_entry_80096DE0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80096DE0u);
    osCreateScheduler(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80096DE0u);
}

extern "C" void ts2_current_internal_entry_80096F28_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80096F28u);
    osScAddClient(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80096F28u);
}

extern "C" void ts2_current_internal_entry_80096F80_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80096F80u);
    osScRemoveClient(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80096F80u);
}

extern "C" void ts2_current_internal_entry_80097010_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80097010u);
    osScGetCmdQ(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80097010u);
}

extern "C" void ts2_current_internal_entry_80097018_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80097018u);
    __scMain(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80097018u);
}

extern "C" void ts2_current_internal_entry_8009711C_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009711Cu);
    __scHandleRetrace(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009711Cu);
}

extern "C" void ts2_current_internal_entry_8009721C_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009721Cu);
    __scHandleRSP(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009721Cu);
}

extern "C" void ts2_current_internal_entry_8009732C_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009732Cu);
    __scHandleRDP(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009732Cu);
}

extern "C" void ts2_current_internal_entry_80097994_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x80097994u);
    func_80097940(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x80097994u);
}

extern "C" void ts2_current_internal_entry_800987B0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x800987B0u);
    func_800986F0(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x800987B0u);
}

extern "C" void ts2_current_internal_entry_800991B0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x800991B0u);
    func_800991A0(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x800991B0u);
}

extern "C" void ts2_current_internal_entry_8009B870_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009B870u);
    func_8009B704(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009B870u);
}

extern "C" void ts2_current_internal_entry_8009B878_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009B878u);
    func_8009B704(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009B878u);
}

extern "C" void ts2_current_internal_entry_8009B950_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009B950u);
    func_8009B880(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009B950u);
}

extern "C" void ts2_current_internal_entry_8009B9B0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009B9B0u);
    func_8009B960(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009B9B0u);
}

extern "C" void ts2_current_internal_entry_8009BBF0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009BBF0u);
    osPfsFreeBlocks_recomp(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009BBF0u);
}

extern "C" void ts2_current_internal_entry_8009BD90_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009BD90u);
    func_8009BC40(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009BD90u);
}

extern "C" void ts2_current_internal_entry_8009BE30_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009BE30u);
    func_8009BDA0(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009BE30u);
}

extern "C" void ts2_current_internal_entry_8009C2B0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009C2B0u);
    func_8009C160(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009C2B0u);
}

extern "C" void ts2_current_internal_entry_8009C5D0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009C5D0u);
    func_8009C59C(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009C5D0u);
}

extern "C" void ts2_current_internal_entry_8009C670_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009C670u);
    func_8009C5E0(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009C670u);
}

extern "C" void ts2_current_internal_entry_8009C6D0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009C6D0u);
    func_8009C680(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009C6D0u);
}

extern "C" void ts2_current_internal_entry_8009D450_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009D450u);
    func_8009D420(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009D450u);
}

extern "C" void ts2_current_internal_entry_8009DE20_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009DE20u);
    func_8009DB20(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009DE20u);
}

extern "C" void ts2_current_internal_entry_8009DF00_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009DF00u);
    func_8009DB20(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009DF00u);
}

extern "C" void ts2_current_internal_entry_8009F910_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x8009F910u);
    func_8009F8C8(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x8009F910u);
}

extern "C" void ts2_current_internal_entry_800A04F0_recomp(uint8_t* rdram, recomp_context* ctx) {
    ts2_current_set_internal_entry(ctx, 0x800A04F0u);
    func_800A036C(rdram, ctx);
    ts2_current_verify_internal_consumed(ctx, 0x800A04F0u);
}

