#pragma once
// =============================================================================
// SpaceRanger - bridge-layer declarations
//
// DEFINED in src/ts2_generated_bridge_layer.c, consumed by the host in
// src/stage79_first_vi_audio_wakeup.cpp.
//
// ToyStory2OldNative hand-added these to the GENERATED RecompiledFuncs/funcs.h,
// so every regeneration silently dropped them - that is what its
// funcs.h.pre_* backup files are. Declaring them here instead keeps the
// generated header pristine and regenerable. Force-included by CMakeLists.
// =============================================================================
// SDL renames main() to SDL_main() unless SDL_MAIN_HANDLED is defined before
// SDL.h is parsed (SDL_main.h line 143). The host deliberately defines BOTH
// main() and extern "C" SDL_main(), so the rename collides them and MSVC
// reports: error C2731: 'main': function cannot be overloaded.
// The build also passes /D SDL_MAIN_HANDLED; defining it here as well
// guarantees it is set before any SDL header can be reached, whatever the
// include order in a given translation unit.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED 1
#endif

#include <stdint.h>
#include "recomp.h"

#ifdef __cplusplus
extern "C" {
#endif

int ts2_wide_visible(uint8_t* rdram, uint32_t address, int count, int stride, int indirect);
int ts2_wide_margin(void);
void ts2_trace_actor_submission(uint8_t* rdram, const recomp_context* ctx, uint32_t pc);
void ts2_note_mesh_allocation(int32_t result);
void ts2_begin_scenery_distance(uint8_t* rdram, int32_t* saved);
void ts2_end_scenery_distance(uint8_t* rdram, const int32_t* saved, int world_pass);
int32_t ts2_wide_positive_extent(int32_t extent);
int ts2_wide_sphere_visible(int32_t outside, int32_t depth, int32_t radius, int32_t slope_fixed);
int ts2_wide_room_sphere_visible(uint8_t* rdram, uint32_t camera, uint32_t entry);
void ts2_wide_full_portal(uint8_t* rdram, uint32_t address);
void ts2_wide_mesh_bounds(uint8_t* rdram);
void ts2_wide_portal_edge(uint8_t* rdram, uint32_t address);
void ts2_trace_emitter(uint8_t* rdram, uint32_t function_vram);
void ts2_trace_mesh(uint8_t* rdram, uint32_t function_vram, uint32_t mesh);
void ts2_write_emitter_history(const char* path);
void ts2_vi_mirror_mode(uint8_t* rdram, uint32_t mode_addr);
void ts2_vi_mirror_black(uint32_t active);

typedef int (*ts2_thread_create_bridge_callback_t)(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri);

typedef int (*ts2_thread_start_bridge_callback_t)(
    uint8_t* rdram,
    uint32_t thread_addr);

typedef int (*ts2_thread_get_priority_bridge_callback_t)(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t* out_priority);

typedef int (*ts2_thread_set_priority_bridge_callback_t)(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t priority);

typedef int (*ts2_mesg_recv_bridge_callback_t)(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t msg_addr,
    int32_t flags,
    int32_t* out_result);

typedef int (*ts2_mesg_send_bridge_callback_t)(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t message,
    int32_t flags,
    int32_t* out_result);

typedef uint32_t (*ts2_vi_mmio_read_callback_t)(uint32_t addr);

typedef void (*ts2_vi_mmio_write_callback_t)(uint32_t addr, uint32_t value);

typedef uint32_t (*ts2_mi_mmio_read_callback_t)(uint32_t addr);

typedef void (*ts2_mi_mmio_write_callback_t)(uint32_t addr, uint32_t value);

typedef uint32_t (*ts2_si_mmio_read_callback_t)(uint32_t addr);

typedef void (*ts2_si_mmio_write_callback_t)(uint32_t addr, uint32_t value);

typedef uint32_t (*ts2_pi_mmio_read_callback_t)(uint32_t addr);

typedef void (*ts2_pi_mmio_write_callback_t)(uint32_t addr, uint32_t value);

typedef int (*ts2_thread_entry_callback_t)(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc);

typedef int (*ts2_boot_checkpoint_callback_t)(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc);

typedef void (*ts2_graphics_task_callback_t)(
    uint8_t* rdram,
    uint32_t task_address);

void ts2_set_thread_bridge_callbacks(
    ts2_thread_create_bridge_callback_t create_callback,
    ts2_thread_start_bridge_callback_t start_callback);

int ts2_try_thread_create_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri);

int ts2_try_thread_start_bridge(
    uint8_t* rdram,
    uint32_t thread_addr);

void ts2_set_thread_priority_bridge_callbacks(
    ts2_thread_get_priority_bridge_callback_t get_callback,
    ts2_thread_set_priority_bridge_callback_t set_callback);

int ts2_try_thread_get_priority_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t* out_priority);

int ts2_try_thread_set_priority_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t priority);

void ts2_set_mesg_recv_bridge_callback(
    ts2_mesg_recv_bridge_callback_t callback);

int ts2_try_mesg_recv_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t msg_addr,
    int32_t flags,
    int32_t* out_result);

void ts2_set_mesg_send_bridge_callback(
    ts2_mesg_send_bridge_callback_t callback);

int ts2_try_mesg_send_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t message,
    int32_t flags,
    int32_t* out_result);

void ts2_set_vi_mmio_bridge_callbacks(
    ts2_vi_mmio_read_callback_t read_callback,
    ts2_vi_mmio_write_callback_t write_callback);

uint32_t ts2_vi_mmio_read32(uint32_t addr);

void ts2_vi_mmio_write32(uint32_t addr, uint32_t value);

void ts2_set_mi_mmio_bridge_callbacks(
    ts2_mi_mmio_read_callback_t read_callback,
    ts2_mi_mmio_write_callback_t write_callback);

uint32_t ts2_mi_mmio_read32(uint32_t addr);

void ts2_mi_mmio_write32(uint32_t addr, uint32_t value);

void ts2_set_si_mmio_bridge_callbacks(
    ts2_si_mmio_read_callback_t read_callback,
    ts2_si_mmio_write_callback_t write_callback);

uint32_t ts2_si_mmio_read32(uint32_t addr);

void ts2_si_mmio_write32(uint32_t addr, uint32_t value);

void ts2_set_pi_mmio_bridge_callbacks(
    ts2_pi_mmio_read_callback_t read_callback,
    ts2_pi_mmio_write_callback_t write_callback);

uint32_t ts2_pi_mmio_read32(uint32_t addr);

void ts2_pi_mmio_write32(uint32_t addr, uint32_t value);

void ts2_set_thread_entry_callback(
    ts2_thread_entry_callback_t callback);

int ts2_thread_entry_checkpoint(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc);

int ts2_thread_stop_requested(void);

void ts2_bind_rom_image(const uint8_t* rom, uint32_t size);

int ts2_host_rom_read_u32(uint32_t offset, uint32_t* value);

uint32_t ts2_get_rom_read_count(void);

uint32_t ts2_get_last_rom_read_offset(void);

uint32_t ts2_get_last_rom_read_value(void);

void ts2_reset_pif_word_7fc(void);

uint32_t ts2_get_pif_word_7fc(void);

void ts2_set_pif_word_7fc(uint32_t value);

void ts2_set_boot_checkpoint_callback(
    ts2_boot_checkpoint_callback_t callback);

int ts2_boot_checkpoint(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc);

int ts2_boot_guard_active(void);

int ts2_boot_stop_requested(void);

void ts2_set_graphics_task_callback(
    ts2_graphics_task_callback_t callback);

int ts2_try_graphics_task_callback(
    uint8_t* rdram,
    uint32_t task_address);

void ts2_div_s32_compat(
    uint64_t dividend_raw,
    uint64_t divisor_raw,
    uint64_t* lo_out,
    uint64_t* hi_out
);

void ts2_div_u32_compat(
    uint64_t dividend_raw,
    uint64_t divisor_raw,
    uint64_t* lo_out,
    uint64_t* hi_out
);

/*
 * VR4300 signed/unsigned divide compatibility.
 *
 * The carried-over compat sources call TS2_DIV_S32 / TS2_DIV_U32. In the old
 * native tree these macros lived in the *generated* funcs.h, which is why that
 * tree carries funcs.h.pre_* backups. SpaceRanger keeps funcs.h regenerable, so
 * the macros live here instead and reach every translation unit through the
 * /FI force-include. Without them a C source calling TS2_DIV_S32 compiles under
 * implicit declaration and fails at link as an unresolved external.
 */
#define TS2_DIV_S32(dividend, divisor, lo_value, hi_value) \
    ts2_div_s32_compat(                                    \
        (uint64_t)(dividend),                              \
        (uint64_t)(divisor),                               \
        &(lo_value),                                       \
        &(hi_value)                                        \
    )

#define TS2_DIV_U32(dividend, divisor, lo_value, hi_value) \
    ts2_div_u32_compat(                                    \
        (uint64_t)(dividend),                              \
        (uint64_t)(divisor),                               \
        &(lo_value),                                       \
        &(hi_value)                                        \
    )

#ifdef __cplusplus
}
#endif
