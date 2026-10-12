#include <stdint.h>

#include "recomp.h"
#include "funcs.h"

/* TS2 OLDNATIVE PHASE5B1 BRIDGE STORAGE BEGIN */
static ts2_thread_create_bridge_callback_t g_ts2_thread_create_bridge_callback = 0;
static ts2_thread_start_bridge_callback_t g_ts2_thread_start_bridge_callback = 0;
static ts2_thread_get_priority_bridge_callback_t g_ts2_thread_get_priority_bridge_callback = 0;
static ts2_thread_set_priority_bridge_callback_t g_ts2_thread_set_priority_bridge_callback = 0;
static ts2_mesg_recv_bridge_callback_t g_ts2_mesg_recv_bridge_callback = 0;
static ts2_mesg_send_bridge_callback_t g_ts2_mesg_send_bridge_callback = 0;

static ts2_vi_mmio_read_callback_t g_ts2_vi_mmio_read_callback = 0;
static ts2_vi_mmio_write_callback_t g_ts2_vi_mmio_write_callback = 0;
static ts2_mi_mmio_read_callback_t g_ts2_mi_mmio_read_callback = 0;
static ts2_mi_mmio_write_callback_t g_ts2_mi_mmio_write_callback = 0;
static ts2_si_mmio_read_callback_t g_ts2_si_mmio_read_callback = 0;
static ts2_si_mmio_write_callback_t g_ts2_si_mmio_write_callback = 0;
static ts2_pi_mmio_read_callback_t g_ts2_pi_mmio_read_callback = 0;
static ts2_pi_mmio_write_callback_t g_ts2_pi_mmio_write_callback = 0;

static ts2_thread_entry_callback_t g_ts2_thread_entry_callback = 0;
static int g_ts2_thread_stop_requested = 0;

static const uint8_t* g_ts2_rom_image = 0;
static uint32_t g_ts2_rom_size = 0;
static uint32_t g_ts2_rom_read_count = 0;
static uint32_t g_ts2_last_rom_read_offset = 0;
static uint32_t g_ts2_last_rom_read_value = 0;

static uint32_t g_ts2_pif_word_7fc = 0;

static ts2_boot_checkpoint_callback_t g_ts2_boot_checkpoint_callback = 0;
static int g_ts2_boot_stop_requested = 0;

static ts2_graphics_task_callback_t g_ts2_graphics_task_callback = 0;

void ts2_set_thread_bridge_callbacks(
    ts2_thread_create_bridge_callback_t create_callback,
    ts2_thread_start_bridge_callback_t start_callback) {
    g_ts2_thread_create_bridge_callback = create_callback;
    g_ts2_thread_start_bridge_callback = start_callback;
}

int ts2_try_thread_create_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {
    if (g_ts2_thread_create_bridge_callback != 0) {
        return g_ts2_thread_create_bridge_callback(
            rdram, thread_addr, id, entry, arg, sp, pri);
    }
    return 0;
}

int ts2_try_thread_start_bridge(
    uint8_t* rdram,
    uint32_t thread_addr) {
    if (g_ts2_thread_start_bridge_callback != 0) {
        return g_ts2_thread_start_bridge_callback(rdram, thread_addr);
    }
    return 0;
}

void ts2_set_thread_priority_bridge_callbacks(
    ts2_thread_get_priority_bridge_callback_t get_callback,
    ts2_thread_set_priority_bridge_callback_t set_callback) {
    g_ts2_thread_get_priority_bridge_callback = get_callback;
    g_ts2_thread_set_priority_bridge_callback = set_callback;
}

int ts2_try_thread_get_priority_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t* out_priority) {
    if (g_ts2_thread_get_priority_bridge_callback != 0) {
        return g_ts2_thread_get_priority_bridge_callback(
            rdram, thread_addr, out_priority);
    }
    return 0;
}

int ts2_try_thread_set_priority_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t priority) {
    if (g_ts2_thread_set_priority_bridge_callback != 0) {
        return g_ts2_thread_set_priority_bridge_callback(
            rdram, thread_addr, priority);
    }
    return 0;
}

void ts2_set_mesg_recv_bridge_callback(
    ts2_mesg_recv_bridge_callback_t callback) {
    g_ts2_mesg_recv_bridge_callback = callback;
}

int ts2_try_mesg_recv_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t msg_addr,
    int32_t flags,
    int32_t* out_result) {
    if (g_ts2_mesg_recv_bridge_callback != 0) {
        return g_ts2_mesg_recv_bridge_callback(
            rdram, queue_addr, msg_addr, flags, out_result);
    }
    return 0;
}

void ts2_set_mesg_send_bridge_callback(
    ts2_mesg_send_bridge_callback_t callback) {
    g_ts2_mesg_send_bridge_callback = callback;
}

int ts2_try_mesg_send_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t message,
    int32_t flags,
    int32_t* out_result) {
    if (g_ts2_mesg_send_bridge_callback != 0) {
        return g_ts2_mesg_send_bridge_callback(
            rdram, queue_addr, message, flags, out_result);
    }
    return 0;
}

void ts2_set_vi_mmio_bridge_callbacks(
    ts2_vi_mmio_read_callback_t read_callback,
    ts2_vi_mmio_write_callback_t write_callback) {
    g_ts2_vi_mmio_read_callback = read_callback;
    g_ts2_vi_mmio_write_callback = write_callback;
}

uint32_t ts2_vi_mmio_read32(uint32_t addr) {
    if (g_ts2_vi_mmio_read_callback != 0) {
        return g_ts2_vi_mmio_read_callback(addr);
    }
    return 0;
}

void ts2_vi_mmio_write32(uint32_t addr, uint32_t value) {
    if (g_ts2_vi_mmio_write_callback != 0) {
        g_ts2_vi_mmio_write_callback(addr, value);
    }
}

void ts2_set_mi_mmio_bridge_callbacks(
    ts2_mi_mmio_read_callback_t read_callback,
    ts2_mi_mmio_write_callback_t write_callback) {
    g_ts2_mi_mmio_read_callback = read_callback;
    g_ts2_mi_mmio_write_callback = write_callback;
}

uint32_t ts2_mi_mmio_read32(uint32_t addr) {
    if (g_ts2_mi_mmio_read_callback != 0) {
        return g_ts2_mi_mmio_read_callback(addr);
    }
    return 0;
}

void ts2_mi_mmio_write32(uint32_t addr, uint32_t value) {
    if (g_ts2_mi_mmio_write_callback != 0) {
        g_ts2_mi_mmio_write_callback(addr, value);
    }
}

void ts2_set_si_mmio_bridge_callbacks(
    ts2_si_mmio_read_callback_t read_callback,
    ts2_si_mmio_write_callback_t write_callback) {
    g_ts2_si_mmio_read_callback = read_callback;
    g_ts2_si_mmio_write_callback = write_callback;
}

uint32_t ts2_si_mmio_read32(uint32_t addr) {
    if (g_ts2_si_mmio_read_callback != 0) {
        return g_ts2_si_mmio_read_callback(addr);
    }
    return 0;
}

void ts2_si_mmio_write32(uint32_t addr, uint32_t value) {
    if (g_ts2_si_mmio_write_callback != 0) {
        g_ts2_si_mmio_write_callback(addr, value);
    }
}

void ts2_set_pi_mmio_bridge_callbacks(
    ts2_pi_mmio_read_callback_t read_callback,
    ts2_pi_mmio_write_callback_t write_callback) {
    g_ts2_pi_mmio_read_callback = read_callback;
    g_ts2_pi_mmio_write_callback = write_callback;
}

uint32_t ts2_pi_mmio_read32(uint32_t addr) {
    if (g_ts2_pi_mmio_read_callback != 0) {
        return g_ts2_pi_mmio_read_callback(addr);
    }
    return 0;
}

void ts2_pi_mmio_write32(uint32_t addr, uint32_t value) {
    if (g_ts2_pi_mmio_write_callback != 0) {
        g_ts2_pi_mmio_write_callback(addr, value);
    }
}

void ts2_set_thread_entry_callback(
    ts2_thread_entry_callback_t callback) {
    g_ts2_thread_entry_callback = callback;
    g_ts2_thread_stop_requested = 0;
}

int ts2_thread_entry_checkpoint(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc) {
    if (g_ts2_thread_entry_callback != 0) {
        int stop = g_ts2_thread_entry_callback(rdram, ctx, guest_pc);
        if (stop) {
            g_ts2_thread_stop_requested = 1;
        }
        return stop;
    }
    return 0;
}

int ts2_thread_stop_requested(void) {
    return g_ts2_thread_stop_requested;
}

void ts2_bind_rom_image(const uint8_t* rom, uint32_t size) {
    g_ts2_rom_image = rom;
    g_ts2_rom_size = size;

    if (rom != 0) {
        g_ts2_rom_read_count = 0;
        g_ts2_last_rom_read_offset = 0;
        g_ts2_last_rom_read_value = 0;
    }
}

int ts2_host_rom_read_u32(uint32_t offset, uint32_t* value) {
    uint32_t v;

    if (g_ts2_rom_image == 0 ||
        value == 0 ||
        offset > g_ts2_rom_size ||
        (g_ts2_rom_size - offset) < 4u) {
        return 0;
    }

    v = ((uint32_t)g_ts2_rom_image[offset + 0u] << 24) |
        ((uint32_t)g_ts2_rom_image[offset + 1u] << 16) |
        ((uint32_t)g_ts2_rom_image[offset + 2u] << 8) |
        ((uint32_t)g_ts2_rom_image[offset + 3u]);

    *value = v;
    g_ts2_rom_read_count++;
    g_ts2_last_rom_read_offset = offset;
    g_ts2_last_rom_read_value = v;
    return 1;
}

uint32_t ts2_get_rom_read_count(void) {
    return g_ts2_rom_read_count;
}

uint32_t ts2_get_last_rom_read_offset(void) {
    return g_ts2_last_rom_read_offset;
}

uint32_t ts2_get_last_rom_read_value(void) {
    return g_ts2_last_rom_read_value;
}

void ts2_reset_pif_word_7fc(void) {
    g_ts2_pif_word_7fc = 0;
}

uint32_t ts2_get_pif_word_7fc(void) {
    return g_ts2_pif_word_7fc;
}

void ts2_set_pif_word_7fc(uint32_t value) {
    g_ts2_pif_word_7fc = value;
}

void ts2_set_boot_checkpoint_callback(
    ts2_boot_checkpoint_callback_t callback) {
    g_ts2_boot_checkpoint_callback = callback;
    g_ts2_boot_stop_requested = 0;
}

int ts2_boot_checkpoint(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc) {
    if (g_ts2_boot_checkpoint_callback != 0) {
        int stop = g_ts2_boot_checkpoint_callback(rdram, ctx, guest_pc);
        if (stop) {
            g_ts2_boot_stop_requested = 1;
        }
        return stop;
    }
    return 0;
}

int ts2_boot_guard_active(void) {
    return g_ts2_boot_checkpoint_callback != 0;
}

int ts2_boot_stop_requested(void) {
    return g_ts2_boot_stop_requested;
}

void ts2_set_graphics_task_callback(
    ts2_graphics_task_callback_t callback) {
    g_ts2_graphics_task_callback = callback;
}

int ts2_try_graphics_task_callback(
    uint8_t* rdram,
    uint32_t task_address) {
    if (g_ts2_graphics_task_callback != 0) {
        g_ts2_graphics_task_callback(rdram, task_address);
        return 1;
    }
    return 0;
}
/* TS2 OLDNATIVE PHASE5B1 BRIDGE STORAGE END */