#include "funcs.h"

/*
 * SpaceRanger: host-side implementations of the guest's raw SI (Serial
 * Interface) primitives.
 *
 * These two functions MUST NOT be recompiled, and are marked `ignored` in
 * config/toystory2.us.rev0.recomp.toml so no body is generated for them.
 *
 * Recompiled guest code reaches memory as rdram[guest_addr - 0x80000000], which
 * is only meaningful for RDRAM. Both functions access SI device registers at
 * 0xA4800000, so the generated form indexes roughly half a gigabyte past the
 * end of the 8 MB RDRAM buffer and faults the process
 * (0xC0000005, access violation) the first time the game polls the
 * controller -- which happens on entry to level 1.
 *
 * The bodies below are the recompiled guest logic with only the hardware
 * register accesses rerouted through the SI bridge in
 * ts2_generated_bridge_layer.c (ts2_si_mmio_read32 / ts2_si_mmio_write32).
 * No guest logic is skipped or substituted; control flow and register effects
 * are unchanged.
 *
 *   func_800980E0  __osSiRawStartDma   - writes SI_DRAM_ADDR and the
 *                                        SI_PIF_ADDR_{RD,WR}64B trigger
 *   func_800981A0  __osSiDeviceBusy    - polls SI_STATUS for
 *                                        DMA_BUSY | IO_READ_BUSY
 *
 * Note these are NOT the same as the transcribed copies this file previously
 * also carried for func_80091AC0 / func_80091AE0 (__osDisableInt /
 * __osRestoreInt). Those touch only CP0 Status, which the recompiler handles
 * correctly, so the generated versions of those are used instead.
 */

/*
 * Because both are `ignored`, the generated funcs.h no longer declares them, so
 * they are declared here. Without this, func_800980E0's call to func_800981A0
 * below would be implicitly declared as returning int and collide with the
 * definition further down (C2371).
 */
void func_800980E0(uint8_t* rdram, recomp_context* ctx);
void func_800981A0(uint8_t* rdram, recomp_context* ctx);

RECOMP_FUNC void func_800980E0(uint8_t* rdram, recomp_context* ctx) {
    uint64_t hi = 0, lo = 0, result = 0;
    int c1cs = 0;
    (void)hi; (void)lo; (void)result; (void)c1cs;

    ctx->r29 = ADD32(ctx->r29, -0X20);
    MEM_W(0X14, ctx->r29) = ctx->r17;
    ctx->r17 = ADD32(ctx->r4, 0);
    MEM_W(0X10, ctx->r29) = ctx->r16;
    ctx->r16 = ADD32(ctx->r5, 0);
    ctx->r2 = ctx->r16 & 0X3;

    if (ctx->r2 == 0) {
        MEM_W(0X18, ctx->r29) = ctx->r31;
        goto L_80098118;
    }

    MEM_W(0X18, ctx->r29) = ctx->r31;
    ctx->r4 = S32(0X800A << 16);
    ctx->r4 = ADD32(ctx->r4, 0X2360);
    ctx->r5 = S32(0X800A << 16);
    ctx->r5 = ADD32(ctx->r5, 0X2364);
    ctx->r6 = ADD32(0, 0X37);
    func_80097CE0(rdram, ctx);
    ctx->r6 = ADD32(0, 0X37);

L_80098118:
    func_800981A0(rdram, ctx);

    if (ctx->r2 != 0) {
        ctx->r2 = ADD32(0, -0X1);
        goto L_80098184;
    }

    ctx->r2 = ADD32(0, -0X1);
    ctx->r2 = ADD32(0, 0X1);

    if (ctx->r17 != ctx->r2) {
        ctx->r4 = ADD32(ctx->r16, 0);
        goto L_8009813C;
    }

    ctx->r4 = ADD32(ctx->r16, 0);
    ctx->r5 = ADD32(0, 0X40);
    osWritebackDCache_recomp(rdram, ctx);
    ctx->r5 = ADD32(0, 0X40);

L_8009813C:
    ctx->r4 = ADD32(ctx->r16, 0);
    osVirtualToPhysical_recomp(rdram, ctx);
    ctx->r4 = ADD32(ctx->r16, 0);

    ctx->r3 = S32(0XA480 << 16);
    ts2_si_mmio_write32((uint32_t)(ctx->r3 + 0X0), (uint32_t)ctx->r2);

    if (ctx->r17 != 0) {
        ctx->r3 = S32(0XA480 << 16);
        goto L_8009815C;
    }

    ctx->r3 = ctx->r3 | 0X4;
    goto L_80098160;

L_8009815C:
    ctx->r3 = ctx->r3 | 0X10;

L_80098160:
    ctx->r2 = S32(0X1FC0 << 16);
    ctx->r2 = ctx->r2 | 0X7C0;
    ts2_si_mmio_write32((uint32_t)(ctx->r3 + 0X0), (uint32_t)ctx->r2);

    if (ctx->r17 != 0) {
        ctx->r2 = ADD32(0, 0);
        goto L_80098184;
    }

    ctx->r2 = ADD32(0, 0);
    ctx->r4 = ADD32(ctx->r16, 0);
    ctx->r5 = ADD32(0, 0X40);
    osInvalDCache_recomp(rdram, ctx);
    ctx->r5 = ADD32(0, 0X40);
    ctx->r2 = ADD32(0, 0);

L_80098184:
    ctx->r31 = MEM_W(ctx->r29, 0X18);
    ctx->r17 = MEM_W(ctx->r29, 0X14);
    ctx->r16 = MEM_W(ctx->r29, 0X10);
    ctx->r29 = ADD32(ctx->r29, 0X20);
    return;
}

RECOMP_FUNC void func_800981A0(uint8_t* rdram, recomp_context* ctx) {
    uint64_t hi = 0, lo = 0, result = 0;
    int c1cs = 0;
    (void)rdram; (void)hi; (void)lo; (void)result; (void)c1cs;

    // 0x800981A0: lui $v0, 0xA480
    ctx->r2 = S32(0XA480 << 16);
    // 0x800981A4: ori $v0, $v0, 0x18
    ctx->r2 = ctx->r2 | 0X18;
    // 0x800981A8: lw $v0, 0x0($v0)
    // Route the hardware register read through the recovered SI bridge.
    ctx->r2 = ts2_si_mmio_read32((uint32_t)(ctx->r2 + 0X0));
    // 0x800981AC: andi $v0, $v0, 0x3
    ctx->r2 = ctx->r2 & 0X3;
    // 0x800981B0 / 0x800981B4
    ctx->r2 = 0 < ctx->r2 ? 1 : 0;
    return;
}
