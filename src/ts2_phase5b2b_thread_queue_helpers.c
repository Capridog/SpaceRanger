#include "funcs.h"

/*
 * Project-local implementations for two libultra exceptasm helpers that
 * N64Recomp recognizes but the pinned N64ModernRuntime does not implement.
 *
 * Guest identities:
 *   0x8009600C = __osEnqueueThread (0x48 bytes)
 *   0x80096054 = __osPopThread     (0x10 bytes)
 *
 * OSThread layout used by libultra:
 *   +0x00 next
 *   +0x04 priority (OSPri/s32)
 */

RECOMP_FUNC void __osPopThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint32_t queue = (uint32_t)ctx->r4;
    uint32_t thread = (uint32_t)MEM_W(queue, 0X0);

    ctx->r2 = (int32_t)thread;
    MEM_W(0X0, queue) = MEM_W(thread, 0X0);
}

RECOMP_FUNC void __osEnqueueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    uint32_t pred = (uint32_t)ctx->r4;
    uint32_t thread = (uint32_t)ctx->r5;
    uint32_t next = (uint32_t)MEM_W(pred, 0X0);
    int32_t thread_pri = (int32_t)MEM_W(thread, 0X4);

    while ((int32_t)MEM_W(next, 0X4) >= thread_pri) {
        pred = next;
        next = (uint32_t)MEM_W(next, 0X0);
    }

    MEM_W(0X0, thread) = next;
    MEM_W(0X0, pred) = thread;
}
