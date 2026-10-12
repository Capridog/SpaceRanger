#include "recomp.h"
#include "funcs.h"

/* TS2 PHASE 5B2B COMPAT INTERNAL ENTRIES
 * Generated from the current 23-TU translated parent functions.
 * Do not regenerate by copying stale 26-TU alias bodies.
 */

/* guest 0x80087870 extracted from func_80087820 (funcs_12.c) */
RECOMP_FUNC void ts2_auto_alias_80087870(uint8_t* rdram, recomp_context* ctx) {
    uint64_t hi = 0, lo = 0, result = 0;
    int c1cs = 0;
    // 0x80087870: lw          $t3, 0x10($v0)
    ctx->r11 = MEM_W(ctx->r2, 0X10);
    // 0x80087874: lw          $t4, 0x14($v0)
    ctx->r12 = MEM_W(ctx->r2, 0X14);
    // 0x80087878: lw          $t5, 0x18($v0)
    ctx->r13 = MEM_W(ctx->r2, 0X18);
    // 0x8008787C: lw          $t6, 0x20($v0)
    ctx->r14 = MEM_W(ctx->r2, 0X20);
    // 0x80087880: lw          $t7, 0x24($v0)
    ctx->r15 = MEM_W(ctx->r2, 0X24);
    // 0x80087884: lw          $t8, 0x28($v0)
    ctx->r24 = MEM_W(ctx->r2, 0X28);
    // 0x80087888: lw          $s0, 0x30($v0)
    ctx->r16 = MEM_W(ctx->r2, 0X30);
    // 0x8008788C: lw          $s1, 0x34($v0)
    ctx->r17 = MEM_W(ctx->r2, 0X34);
    // 0x80087890: lw          $s2, 0x38($v0)
    ctx->r18 = MEM_W(ctx->r2, 0X38);
    // 0x80087894: lui         $v0, 0x803F
    ctx->r2 = S32(0X803F << 16);
    // 0x80087898: ori         $v0, $v0, 0xD000
    ctx->r2 = ctx->r2 | 0XD000;
    // 0x8008789C: addi        $a0, $a3, 0x4
    ctx->r4 = ADD32(ctx->r7, 0X4);
    // 0x800878A0: lh          $s4, 0x0($a0)
    ctx->r20 = MEM_H(ctx->r4, 0X0);
    // 0x800878A4: lh          $s5, 0x2($a0)
    ctx->r21 = MEM_H(ctx->r4, 0X2);
    // 0x800878A8: mult        $t0, $s4
    result = S64(S32(ctx->r8)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800878AC: mflo        $s3
    ctx->r19 = lo;
    // 0x800878B0: lh          $s6, 0x4($a0)
    ctx->r22 = MEM_H(ctx->r4, 0X4);
    // 0x800878B4: nop

    // 0x800878B8: mult        $t1, $s5
    result = S64(S32(ctx->r9)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800878BC: mflo        $t9
    ctx->r25 = lo;
    // 0x800878C0: nop

    // 0x800878C4: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x800878C8: mult        $t2, $s6
    result = S64(S32(ctx->r10)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800878CC: mflo        $v1
    ctx->r3 = lo;
    // 0x800878D0: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x800878D4: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x800878D8: add         $a1, $s0, $s3
    ctx->r5 = ADD32(ctx->r16, ctx->r19);
    // 0x800878DC: mult        $t3, $s4
    result = S64(S32(ctx->r11)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800878E0: mflo        $s3
    ctx->r19 = lo;
    // 0x800878E4: sh          $a1, 0x2($v0)
    MEM_H(0X2, ctx->r2) = ctx->r5;
    // 0x800878E8: nop

    // 0x800878EC: mult        $t4, $s5
    result = S64(S32(ctx->r12)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800878F0: mflo        $t9
    ctx->r25 = lo;
    // 0x800878F4: nop

    // 0x800878F8: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x800878FC: mult        $t5, $s6
    result = S64(S32(ctx->r13)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087900: mflo        $v1
    ctx->r3 = lo;
    // 0x80087904: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087908: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x8008790C: add         $s7, $s1, $s3
    ctx->r23 = ADD32(ctx->r17, ctx->r19);
    // 0x80087910: mult        $t6, $s4
    result = S64(S32(ctx->r14)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087914: mflo        $s3
    ctx->r19 = lo;
    // 0x80087918: sh          $s7, 0x4($v0)
    MEM_H(0X4, ctx->r2) = ctx->r23;
    // 0x8008791C: nop

    // 0x80087920: mult        $t7, $s5
    result = S64(S32(ctx->r15)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087924: mflo        $t9
    ctx->r25 = lo;
    // 0x80087928: nop

    // 0x8008792C: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087930: mult        $t8, $s6
    result = S64(S32(ctx->r24)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087934: mflo        $v1
    ctx->r3 = lo;
    // 0x80087938: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x8008793C: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087940: add         $s5, $s2, $s3
    ctx->r21 = ADD32(ctx->r18, ctx->r19);
    // 0x80087944: sh          $s5, 0x6($v0)
    MEM_H(0X6, ctx->r2) = ctx->r21;
    // 0x80087948: add         $at, $zero, $s5
    ctx->r1 = ADD32(0, ctx->r21);
    // 0x8008794C: addi        $s5, $s5, -0x50
    ctx->r21 = ADD32(ctx->r21, -0X50);
    // 0x80087950: bgez        $s5, L_8008795C
    if (SIGNED(ctx->r21) >= 0) {
        // 0x80087954: addiu       $v1, $zero, 0xA0
        ctx->r3 = ADD32(0, 0XA0);
            goto L_8008795C;
    }
    // 0x80087954: addiu       $v1, $zero, 0xA0
    ctx->r3 = ADD32(0, 0XA0);
    // 0x80087958: add         $s5, $zero, $zero
    ctx->r21 = ADD32(0, 0);
L_8008795C:
    // 0x8008795C: addi        $s5, $s5, 0x50
    ctx->r21 = ADD32(ctx->r21, 0X50);
    // 0x80087960: mtc1        $s5, $f12
    ctx->f12.u32l = ctx->r21;
    // 0x80087964: cvt.s.w     $f12, $f12
    CHECK_FR(ctx, 12);
    CHECK_FR(ctx, 12);
    ctx->f12.fl = CVT_S_W(ctx->f12.u32l);
    // 0x80087968: mult        $v1, $a1
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r5)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x8008796C: mflo        $s6
    ctx->r22 = lo;
    // 0x80087970: nop

    // 0x80087974: nop

    // 0x80087978: mult        $v1, $s7
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r23)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x8008797C: mflo        $s7
    ctx->r23 = lo;
    // 0x80087980: nop

    // 0x80087984: nop

    // 0x80087988: div         $zero, $s6, $s5
    TS2_DIV_S32(ctx->r22, ctx->r21, lo, hi);
    // 0x8008798C: mtc1        $s7, $f14
    ctx->f14.u32l = ctx->r23;
    // 0x80087990: cvt.s.w     $f14, $f14
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 14);
    ctx->f14.fl = CVT_S_W(ctx->f14.u32l);
    // 0x80087994: div.s       $f16, $f14, $f12
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 12);
    NAN_CHECK(ctx->f14.fl); NAN_CHECK(ctx->f12.fl); 
    ctx->f16.fl = DIV_S(ctx->f14.fl, ctx->f12.fl);
    // 0x80087998: mflo        $s6
    ctx->r22 = lo;
    // 0x8008799C: nop

    // 0x800879A0: nop

    // 0x800879A4: cvt.w.s     $f16, $f16
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 16);
    ctx->f16.u32l = CVT_W_S(ctx->f16.fl);
    // 0x800879A8: mfc1        $s7, $f16
    ctx->r23 = (int32_t)ctx->f16.u32l;
    // 0x800879AC: addi        $s6, $s6, 0xA0
    ctx->r22 = ADD32(ctx->r22, 0XA0);
    // 0x800879B0: addi        $s7, $s7, 0x78
    ctx->r23 = ADD32(ctx->r23, 0X78);
    // 0x800879B4: sh          $s6, 0x8($v0)
    MEM_H(0X8, ctx->r2) = ctx->r22;
    // 0x800879B8: sh          $s7, 0xA($v0)
    MEM_H(0XA, ctx->r2) = ctx->r23;
    // 0x800879BC: addi        $a0, $a3, 0xC
    ctx->r4 = ADD32(ctx->r7, 0XC);
    // 0x800879C0: lh          $s4, 0x0($a0)
    ctx->r20 = MEM_H(ctx->r4, 0X0);
    // 0x800879C4: lh          $s5, 0x2($a0)
    ctx->r21 = MEM_H(ctx->r4, 0X2);
    // 0x800879C8: mult        $t0, $s4
    result = S64(S32(ctx->r8)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800879CC: mflo        $s3
    ctx->r19 = lo;
    // 0x800879D0: lh          $s6, 0x4($a0)
    ctx->r22 = MEM_H(ctx->r4, 0X4);
    // 0x800879D4: nop

    // 0x800879D8: mult        $t1, $s5
    result = S64(S32(ctx->r9)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800879DC: mflo        $t9
    ctx->r25 = lo;
    // 0x800879E0: nop

    // 0x800879E4: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x800879E8: mult        $t2, $s6
    result = S64(S32(ctx->r10)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x800879EC: mflo        $v1
    ctx->r3 = lo;
    // 0x800879F0: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x800879F4: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x800879F8: add         $a1, $s0, $s3
    ctx->r5 = ADD32(ctx->r16, ctx->r19);
    // 0x800879FC: mult        $t3, $s4
    result = S64(S32(ctx->r11)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A00: mflo        $s3
    ctx->r19 = lo;
    // 0x80087A04: sh          $a1, 0xE($v0)
    MEM_H(0XE, ctx->r2) = ctx->r5;
    // 0x80087A08: nop

    // 0x80087A0C: mult        $t4, $s5
    result = S64(S32(ctx->r12)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A10: mflo        $t9
    ctx->r25 = lo;
    // 0x80087A14: nop

    // 0x80087A18: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087A1C: mult        $t5, $s6
    result = S64(S32(ctx->r13)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A20: mflo        $v1
    ctx->r3 = lo;
    // 0x80087A24: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087A28: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087A2C: add         $s7, $s1, $s3
    ctx->r23 = ADD32(ctx->r17, ctx->r19);
    // 0x80087A30: mult        $t6, $s4
    result = S64(S32(ctx->r14)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A34: mflo        $s3
    ctx->r19 = lo;
    // 0x80087A38: sh          $s7, 0x10($v0)
    MEM_H(0X10, ctx->r2) = ctx->r23;
    // 0x80087A3C: nop

    // 0x80087A40: mult        $t7, $s5
    result = S64(S32(ctx->r15)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A44: mflo        $t9
    ctx->r25 = lo;
    // 0x80087A48: nop

    // 0x80087A4C: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087A50: mult        $t8, $s6
    result = S64(S32(ctx->r24)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A54: mflo        $v1
    ctx->r3 = lo;
    // 0x80087A58: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087A5C: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087A60: add         $s5, $s2, $s3
    ctx->r21 = ADD32(ctx->r18, ctx->r19);
    // 0x80087A64: sh          $s5, 0x12($v0)
    MEM_H(0X12, ctx->r2) = ctx->r21;
    // 0x80087A68: add         $at, $at, $s5
    ctx->r1 = ADD32(ctx->r1, ctx->r21);
    // 0x80087A6C: addi        $s5, $s5, -0x50
    ctx->r21 = ADD32(ctx->r21, -0X50);
    // 0x80087A70: bgez        $s5, L_80087A7C
    if (SIGNED(ctx->r21) >= 0) {
        // 0x80087A74: addiu       $v1, $zero, 0xA0
        ctx->r3 = ADD32(0, 0XA0);
            goto L_80087A7C;
    }
    // 0x80087A74: addiu       $v1, $zero, 0xA0
    ctx->r3 = ADD32(0, 0XA0);
    // 0x80087A78: add         $s5, $zero, $zero
    ctx->r21 = ADD32(0, 0);
L_80087A7C:
    // 0x80087A7C: addi        $s5, $s5, 0x50
    ctx->r21 = ADD32(ctx->r21, 0X50);
    // 0x80087A80: mtc1        $s5, $f12
    ctx->f12.u32l = ctx->r21;
    // 0x80087A84: cvt.s.w     $f12, $f12
    CHECK_FR(ctx, 12);
    CHECK_FR(ctx, 12);
    ctx->f12.fl = CVT_S_W(ctx->f12.u32l);
    // 0x80087A88: mult        $v1, $a1
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r5)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A8C: mflo        $s6
    ctx->r22 = lo;
    // 0x80087A90: nop

    // 0x80087A94: nop

    // 0x80087A98: mult        $v1, $s7
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r23)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087A9C: mflo        $s7
    ctx->r23 = lo;
    // 0x80087AA0: nop

    // 0x80087AA4: nop

    // 0x80087AA8: div         $zero, $s6, $s5
    TS2_DIV_S32(ctx->r22, ctx->r21, lo, hi);
    // 0x80087AAC: mtc1        $s7, $f14
    ctx->f14.u32l = ctx->r23;
    // 0x80087AB0: cvt.s.w     $f14, $f14
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 14);
    ctx->f14.fl = CVT_S_W(ctx->f14.u32l);
    // 0x80087AB4: div.s       $f16, $f14, $f12
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 12);
    NAN_CHECK(ctx->f14.fl); NAN_CHECK(ctx->f12.fl); 
    ctx->f16.fl = DIV_S(ctx->f14.fl, ctx->f12.fl);
    // 0x80087AB8: mflo        $s6
    ctx->r22 = lo;
    // 0x80087ABC: nop

    // 0x80087AC0: nop

    // 0x80087AC4: cvt.w.s     $f16, $f16
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 16);
    ctx->f16.u32l = CVT_W_S(ctx->f16.fl);
    // 0x80087AC8: mfc1        $s7, $f16
    ctx->r23 = (int32_t)ctx->f16.u32l;
    // 0x80087ACC: addi        $s6, $s6, 0xA0
    ctx->r22 = ADD32(ctx->r22, 0XA0);
    // 0x80087AD0: addi        $s7, $s7, 0x78
    ctx->r23 = ADD32(ctx->r23, 0X78);
    // 0x80087AD4: sh          $s6, 0x14($v0)
    MEM_H(0X14, ctx->r2) = ctx->r22;
    // 0x80087AD8: sh          $s7, 0x16($v0)
    MEM_H(0X16, ctx->r2) = ctx->r23;
    // 0x80087ADC: addi        $a0, $a3, 0x1C
    ctx->r4 = ADD32(ctx->r7, 0X1C);
    // 0x80087AE0: lh          $s4, 0x0($a0)
    ctx->r20 = MEM_H(ctx->r4, 0X0);
    // 0x80087AE4: lh          $s5, 0x2($a0)
    ctx->r21 = MEM_H(ctx->r4, 0X2);
    // 0x80087AE8: mult        $t0, $s4
    result = S64(S32(ctx->r8)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087AEC: mflo        $s3
    ctx->r19 = lo;
    // 0x80087AF0: lh          $s6, 0x4($a0)
    ctx->r22 = MEM_H(ctx->r4, 0X4);
    // 0x80087AF4: nop

    // 0x80087AF8: mult        $t1, $s5
    result = S64(S32(ctx->r9)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087AFC: mflo        $t9
    ctx->r25 = lo;
    // 0x80087B00: nop

    // 0x80087B04: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087B08: mult        $t2, $s6
    result = S64(S32(ctx->r10)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B0C: mflo        $v1
    ctx->r3 = lo;
    // 0x80087B10: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087B14: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087B18: add         $a1, $s0, $s3
    ctx->r5 = ADD32(ctx->r16, ctx->r19);
    // 0x80087B1C: mult        $t3, $s4
    result = S64(S32(ctx->r11)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B20: mflo        $s3
    ctx->r19 = lo;
    // 0x80087B24: sh          $a1, 0x1A($v0)
    MEM_H(0X1A, ctx->r2) = ctx->r5;
    // 0x80087B28: nop

    // 0x80087B2C: mult        $t4, $s5
    result = S64(S32(ctx->r12)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B30: mflo        $t9
    ctx->r25 = lo;
    // 0x80087B34: nop

    // 0x80087B38: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087B3C: mult        $t5, $s6
    result = S64(S32(ctx->r13)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B40: mflo        $v1
    ctx->r3 = lo;
    // 0x80087B44: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087B48: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087B4C: add         $s7, $s1, $s3
    ctx->r23 = ADD32(ctx->r17, ctx->r19);
    // 0x80087B50: mult        $t6, $s4
    result = S64(S32(ctx->r14)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B54: mflo        $s3
    ctx->r19 = lo;
    // 0x80087B58: sh          $s7, 0x1C($v0)
    MEM_H(0X1C, ctx->r2) = ctx->r23;
    // 0x80087B5C: nop

    // 0x80087B60: mult        $t7, $s5
    result = S64(S32(ctx->r15)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B64: mflo        $t9
    ctx->r25 = lo;
    // 0x80087B68: nop

    // 0x80087B6C: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087B70: mult        $t8, $s6
    result = S64(S32(ctx->r24)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087B74: mflo        $v1
    ctx->r3 = lo;
    // 0x80087B78: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087B7C: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087B80: add         $s5, $s2, $s3
    ctx->r21 = ADD32(ctx->r18, ctx->r19);
    // 0x80087B84: sh          $s5, 0x1E($v0)
    MEM_H(0X1E, ctx->r2) = ctx->r21;
    // 0x80087B88: add         $at, $at, $s5
    ctx->r1 = ADD32(ctx->r1, ctx->r21);
    // 0x80087B8C: addi        $s5, $s5, -0x50
    ctx->r21 = ADD32(ctx->r21, -0X50);
    // 0x80087B90: bgez        $s5, L_80087B9C
    if (SIGNED(ctx->r21) >= 0) {
        // 0x80087B94: addiu       $v1, $zero, 0xA0
        ctx->r3 = ADD32(0, 0XA0);
            goto L_80087B9C;
    }
    // 0x80087B94: addiu       $v1, $zero, 0xA0
    ctx->r3 = ADD32(0, 0XA0);
    // 0x80087B98: add         $s5, $zero, $zero
    ctx->r21 = ADD32(0, 0);
L_80087B9C:
    // 0x80087B9C: addi        $s5, $s5, 0x50
    ctx->r21 = ADD32(ctx->r21, 0X50);
    // 0x80087BA0: mtc1        $s5, $f12
    ctx->f12.u32l = ctx->r21;
    // 0x80087BA4: cvt.s.w     $f12, $f12
    CHECK_FR(ctx, 12);
    CHECK_FR(ctx, 12);
    ctx->f12.fl = CVT_S_W(ctx->f12.u32l);
    // 0x80087BA8: mult        $v1, $a1
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r5)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087BAC: mflo        $s6
    ctx->r22 = lo;
    // 0x80087BB0: nop

    // 0x80087BB4: nop

    // 0x80087BB8: mult        $v1, $s7
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r23)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087BBC: mflo        $s7
    ctx->r23 = lo;
    // 0x80087BC0: nop

    // 0x80087BC4: nop

    // 0x80087BC8: div         $zero, $s6, $s5
    TS2_DIV_S32(ctx->r22, ctx->r21, lo, hi);
    // 0x80087BCC: mtc1        $s7, $f14
    ctx->f14.u32l = ctx->r23;
    // 0x80087BD0: cvt.s.w     $f14, $f14
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 14);
    ctx->f14.fl = CVT_S_W(ctx->f14.u32l);
    // 0x80087BD4: div.s       $f16, $f14, $f12
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 12);
    NAN_CHECK(ctx->f14.fl); NAN_CHECK(ctx->f12.fl); 
    ctx->f16.fl = DIV_S(ctx->f14.fl, ctx->f12.fl);
    // 0x80087BD8: mflo        $s6
    ctx->r22 = lo;
    // 0x80087BDC: nop

    // 0x80087BE0: nop

    // 0x80087BE4: cvt.w.s     $f16, $f16
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 16);
    ctx->f16.u32l = CVT_W_S(ctx->f16.fl);
    // 0x80087BE8: mfc1        $s7, $f16
    ctx->r23 = (int32_t)ctx->f16.u32l;
    // 0x80087BEC: addi        $s6, $s6, 0xA0
    ctx->r22 = ADD32(ctx->r22, 0XA0);
    // 0x80087BF0: addi        $s7, $s7, 0x78
    ctx->r23 = ADD32(ctx->r23, 0X78);
    // 0x80087BF4: sh          $s6, 0x20($v0)
    MEM_H(0X20, ctx->r2) = ctx->r22;
    // 0x80087BF8: sh          $s7, 0x22($v0)
    MEM_H(0X22, ctx->r2) = ctx->r23;
    // 0x80087BFC: addi        $a0, $a3, 0x14
    ctx->r4 = ADD32(ctx->r7, 0X14);
    // 0x80087C00: lh          $s4, 0x0($a0)
    ctx->r20 = MEM_H(ctx->r4, 0X0);
    // 0x80087C04: lh          $s5, 0x2($a0)
    ctx->r21 = MEM_H(ctx->r4, 0X2);
    // 0x80087C08: mult        $t0, $s4
    result = S64(S32(ctx->r8)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C0C: mflo        $s3
    ctx->r19 = lo;
    // 0x80087C10: lh          $s6, 0x4($a0)
    ctx->r22 = MEM_H(ctx->r4, 0X4);
    // 0x80087C14: nop

    // 0x80087C18: mult        $t1, $s5
    result = S64(S32(ctx->r9)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C1C: mflo        $t9
    ctx->r25 = lo;
    // 0x80087C20: nop

    // 0x80087C24: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087C28: mult        $t2, $s6
    result = S64(S32(ctx->r10)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C2C: mflo        $v1
    ctx->r3 = lo;
    // 0x80087C30: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087C34: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087C38: add         $a1, $s0, $s3
    ctx->r5 = ADD32(ctx->r16, ctx->r19);
    // 0x80087C3C: mult        $t3, $s4
    result = S64(S32(ctx->r11)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C40: mflo        $s3
    ctx->r19 = lo;
    // 0x80087C44: sh          $a1, 0x26($v0)
    MEM_H(0X26, ctx->r2) = ctx->r5;
    // 0x80087C48: nop

    // 0x80087C4C: mult        $t4, $s5
    result = S64(S32(ctx->r12)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C50: mflo        $t9
    ctx->r25 = lo;
    // 0x80087C54: nop

    // 0x80087C58: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087C5C: mult        $t5, $s6
    result = S64(S32(ctx->r13)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C60: mflo        $v1
    ctx->r3 = lo;
    // 0x80087C64: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087C68: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087C6C: add         $s7, $s1, $s3
    ctx->r23 = ADD32(ctx->r17, ctx->r19);
    // 0x80087C70: mult        $t6, $s4
    result = S64(S32(ctx->r14)) * S64(S32(ctx->r20)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C74: mflo        $s3
    ctx->r19 = lo;
    // 0x80087C78: sh          $s7, 0x28($v0)
    MEM_H(0X28, ctx->r2) = ctx->r23;
    // 0x80087C7C: nop

    // 0x80087C80: mult        $t7, $s5
    result = S64(S32(ctx->r15)) * S64(S32(ctx->r21)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C84: mflo        $t9
    ctx->r25 = lo;
    // 0x80087C88: nop

    // 0x80087C8C: add         $s3, $s3, $t9
    ctx->r19 = ADD32(ctx->r19, ctx->r25);
    // 0x80087C90: mult        $t8, $s6
    result = S64(S32(ctx->r24)) * S64(S32(ctx->r22)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087C94: mflo        $v1
    ctx->r3 = lo;
    // 0x80087C98: add         $s3, $s3, $v1
    ctx->r19 = ADD32(ctx->r19, ctx->r3);
    // 0x80087C9C: sra         $s3, $s3, 12
    ctx->r19 = S32(SIGNED(ctx->r19) >> 12);
    // 0x80087CA0: add         $s5, $s2, $s3
    ctx->r21 = ADD32(ctx->r18, ctx->r19);
    // 0x80087CA4: sh          $s5, 0x2A($v0)
    MEM_H(0X2A, ctx->r2) = ctx->r21;
    // 0x80087CA8: add         $at, $at, $s5
    ctx->r1 = ADD32(ctx->r1, ctx->r21);
    // 0x80087CAC: addi        $s5, $s5, -0x50
    ctx->r21 = ADD32(ctx->r21, -0X50);
    // 0x80087CB0: bgez        $s5, L_80087CBC
    if (SIGNED(ctx->r21) >= 0) {
        // 0x80087CB4: addiu       $v1, $zero, 0xA0
        ctx->r3 = ADD32(0, 0XA0);
            goto L_80087CBC;
    }
    // 0x80087CB4: addiu       $v1, $zero, 0xA0
    ctx->r3 = ADD32(0, 0XA0);
    // 0x80087CB8: add         $s5, $zero, $zero
    ctx->r21 = ADD32(0, 0);
L_80087CBC:
    // 0x80087CBC: addi        $s5, $s5, 0x50
    ctx->r21 = ADD32(ctx->r21, 0X50);
    // 0x80087CC0: mtc1        $s5, $f12
    ctx->f12.u32l = ctx->r21;
    // 0x80087CC4: cvt.s.w     $f12, $f12
    CHECK_FR(ctx, 12);
    CHECK_FR(ctx, 12);
    ctx->f12.fl = CVT_S_W(ctx->f12.u32l);
    // 0x80087CC8: mult        $v1, $a1
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r5)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087CCC: mflo        $s6
    ctx->r22 = lo;
    // 0x80087CD0: nop

    // 0x80087CD4: nop

    // 0x80087CD8: mult        $v1, $s7
    result = S64(S32(ctx->r3)) * S64(S32(ctx->r23)); lo = S32(result >> 0); hi = S32(result >> 32);
    // 0x80087CDC: mflo        $s7
    ctx->r23 = lo;
    // 0x80087CE0: nop

    // 0x80087CE4: mtc1        $s7, $f14
    ctx->f14.u32l = ctx->r23;
    // 0x80087CE8: div         $zero, $s6, $s5
    TS2_DIV_S32(ctx->r22, ctx->r21, lo, hi);
    // 0x80087CEC: cvt.s.w     $f14, $f14
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 14);
    ctx->f14.fl = CVT_S_W(ctx->f14.u32l);
    // 0x80087CF0: div.s       $f16, $f14, $f12
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 14);
    CHECK_FR(ctx, 12);
    NAN_CHECK(ctx->f14.fl); NAN_CHECK(ctx->f12.fl); 
    ctx->f16.fl = DIV_S(ctx->f14.fl, ctx->f12.fl);
    // 0x80087CF4: mflo        $s6
    ctx->r22 = lo;
    // 0x80087CF8: sra         $at, $at, 4
    ctx->r1 = S32(SIGNED(ctx->r1) >> 4);
    // 0x80087CFC: cvt.w.s     $f16, $f16
    CHECK_FR(ctx, 16);
    CHECK_FR(ctx, 16);
    ctx->f16.u32l = CVT_W_S(ctx->f16.fl);
    // 0x80087D00: mfc1        $s7, $f16
    ctx->r23 = (int32_t)ctx->f16.u32l;
    // 0x80087D04: addi        $s6, $s6, 0xA0
    ctx->r22 = ADD32(ctx->r22, 0XA0);
    // 0x80087D08: addi        $s7, $s7, 0x78
    ctx->r23 = ADD32(ctx->r23, 0X78);
    // 0x80087D0C: sh          $s6, 0x2C($v0)
    MEM_H(0X2C, ctx->r2) = ctx->r22;
    // 0x80087D10: sh          $s7, 0x2E($v0)
    MEM_H(0X2E, ctx->r2) = ctx->r23;
    // 0x80087D14: add         $v0, $zero, $at
    ctx->r2 = ADD32(0, ctx->r1);
    // 0x80087D18: lw          $s0, 0x0($sp)
    ctx->r16 = MEM_W(ctx->r29, 0X0);
    // 0x80087D1C: lw          $s1, 0x4($sp)
    ctx->r17 = MEM_W(ctx->r29, 0X4);
    // 0x80087D20: lw          $s2, 0x8($sp)
    ctx->r18 = MEM_W(ctx->r29, 0X8);
    // 0x80087D24: lw          $s3, 0xC($sp)
    ctx->r19 = MEM_W(ctx->r29, 0XC);
    // 0x80087D28: lw          $s4, 0x10($sp)
    ctx->r20 = MEM_W(ctx->r29, 0X10);
    // 0x80087D2C: lw          $s5, 0x14($sp)
    ctx->r21 = MEM_W(ctx->r29, 0X14);
    // 0x80087D30: lw          $s6, 0x18($sp)
    ctx->r22 = MEM_W(ctx->r29, 0X18);
    // 0x80087D34: lw          $s7, 0x1C($sp)
    ctx->r23 = MEM_W(ctx->r29, 0X1C);
    // 0x80087D38: lw          $v1, 0x24($sp)
    ctx->r3 = MEM_W(ctx->r29, 0X24);
    // 0x80087D3C: lw          $a1, 0x28($sp)
    ctx->r5 = MEM_W(ctx->r29, 0X28);
    // 0x80087D40: lw          $a2, 0x2C($sp)
    ctx->r6 = MEM_W(ctx->r29, 0X2C);
    // 0x80087D44: lw          $a3, 0x30($sp)
    ctx->r7 = MEM_W(ctx->r29, 0X30);
    // 0x80087D48: lw          $at, 0x34($sp)
    ctx->r1 = MEM_W(ctx->r29, 0X34);
    // 0x80087D4C: addi        $sp, $sp, 0x38
    ctx->r29 = ADD32(ctx->r29, 0X38);
    // 0x80087D50: jr          $ra
    // 0x80087D54: nop

    return;
    // 0x80087D54: nop

    // 0x80087D58: nop

    // 0x80087D5C: nop

;
}

/* guest 0x8009C4AC extracted from func_8009C468 (funcs_17.c) */
RECOMP_FUNC void ts2_auto_alias_8009C4AC(uint8_t* rdram, recomp_context* ctx) {
    uint64_t hi = 0, lo = 0, result = 0;
    int c1cs = 0;
    // 0x8009C4AC: addiu       $a1, $s1, 0xC
    ctx->r5 = ADD32(ctx->r17, 0XC);
    after_1:
    // 0x8009C4B0: j           L_8009C508
    // 0x8009C4B4: addu        $v0, $s2, $zero
    ctx->r2 = ADD32(ctx->r18, 0);
        goto L_8009C508;
    // 0x8009C4B4: addu        $v0, $s2, $zero
    ctx->r2 = ADD32(ctx->r18, 0);
L_8009C4B8:
    // 0x8009C4B8: lw          $s0, 0xC($s1)
    ctx->r16 = MEM_W(ctx->r17, 0XC);
    // 0x8009C4BC: beq         $s0, $zero, L_8009C508
    if (ctx->r16 == 0) {
        // 0x8009C4C0: addu        $v0, $s2, $zero
        ctx->r2 = ADD32(ctx->r18, 0);
            goto L_8009C508;
    }
    // 0x8009C4C0: addu        $v0, $s2, $zero
    ctx->r2 = ADD32(ctx->r18, 0);
L_8009C4C4:
    // 0x8009C4C4: lw          $v0, 0x8($s0)
    ctx->r2 = MEM_W(ctx->r16, 0X8);
    // 0x8009C4C8: lh          $v1, 0x16($v0)
    ctx->r3 = MEM_H(ctx->r2, 0X16);
    // 0x8009C4CC: sll         $v0, $a2, 16
    ctx->r2 = S32(ctx->r6 << 16);
    // 0x8009C4D0: sra         $v0, $v0, 16
    ctx->r2 = S32(SIGNED(ctx->r2) >> 16);
    // 0x8009C4D4: slt         $v0, $v0, $v1
    ctx->r2 = SIGNED(ctx->r2) < SIGNED(ctx->r3) ? 1 : 0;
    // 0x8009C4D8: bne         $v0, $zero, L_8009C4FC
    if (ctx->r2 != 0) {
        // 0x8009C4DC: nop
    
            goto L_8009C4FC;
    }
    // 0x8009C4DC: nop

    // 0x8009C4E0: lw          $v0, 0xD8($s0)
    ctx->r2 = MEM_W(ctx->r16, 0XD8);
    // 0x8009C4E4: bne         $v0, $zero, L_8009C4FC
    if (ctx->r2 != 0) {
        // 0x8009C4E8: nop
    
            goto L_8009C4FC;
    }
    // 0x8009C4E8: nop

    // 0x8009C4EC: sw          $s0, 0x0($a1)
    MEM_W(0X0, ctx->r5) = ctx->r16;
    // 0x8009C4F0: lw          $v0, 0x8($s0)
    ctx->r2 = MEM_W(ctx->r16, 0X8);
    // 0x8009C4F4: addiu       $s2, $zero, 0x1
    ctx->r18 = ADD32(0, 0X1);
    // 0x8009C4F8: lhu         $a2, 0x16($v0)
    ctx->r6 = MEM_HU(ctx->r2, 0X16);
L_8009C4FC:
    // 0x8009C4FC: lw          $s0, 0x0($s0)
    ctx->r16 = MEM_W(ctx->r16, 0X0);
    // 0x8009C500: bne         $s0, $zero, L_8009C4C4
    if (ctx->r16 != 0) {
        // 0x8009C504: addu        $v0, $s2, $zero
        ctx->r2 = ADD32(ctx->r18, 0);
            goto L_8009C4C4;
    }
    // 0x8009C504: addu        $v0, $s2, $zero
    ctx->r2 = ADD32(ctx->r18, 0);
L_8009C508:
    // 0x8009C508: lw          $ra, 0x1C($sp)
    ctx->r31 = MEM_W(ctx->r29, 0X1C);
    // 0x8009C50C: lw          $s2, 0x18($sp)
    ctx->r18 = MEM_W(ctx->r29, 0X18);
    // 0x8009C510: lw          $s1, 0x14($sp)
    ctx->r17 = MEM_W(ctx->r29, 0X14);
    // 0x8009C514: lw          $s0, 0x10($sp)
    ctx->r16 = MEM_W(ctx->r29, 0X10);
    // 0x8009C518: jr          $ra
    // 0x8009C51C: addiu       $sp, $sp, 0x20
    ctx->r29 = ADD32(ctx->r29, 0X20);
    return;
    // 0x8009C51C: addiu       $sp, $sp, 0x20
    ctx->r29 = ADD32(ctx->r29, 0X20);
;
}

