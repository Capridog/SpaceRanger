#include <cstdio>

#include "funcs.h"
#include "librecomp/overlays.hpp"
#include "recomp_overlays.inl"

namespace ts2 {

// The generated recomp_overlays.inl is the authoritative section metadata.
// This lightweight validation wrapper existed only to catch obviously empty
// generated output before the host registered the overlay tables.
bool validate_section_layout() {
    const std::size_t section_count =
        sizeof(section_table) / sizeof(section_table[0]);
    const std::size_t overlay_index_count =
        sizeof(overlay_sections_by_index) / sizeof(overlay_sections_by_index[0]);

    if (section_count == 0 || overlay_index_count == 0) {
        std::fputs("Overlay section metadata mismatch.\n", stderr);
        return false;
    }

    return true;
}

void register_sections() {
    recomp::overlays::overlay_section_table_data_t sections{
        .code_sections = section_table,
        .num_code_sections = sizeof(section_table) / sizeof(section_table[0]),
        .total_num_sections = 21,
    };

    recomp::overlays::overlays_by_index_t overlay_ids{
        .table = overlay_sections_by_index,
        .len = sizeof(overlay_sections_by_index) / sizeof(overlay_sections_by_index[0]),
    };

    recomp::overlays::register_overlays(sections, overlay_ids);
}

// Exercise the runtime's real address lookup while overlays reuse the same RAM.
// Run in a separate process through --check-overlays, before starting the game.
int check_overlay_lookup() {
    register_sections();
    recomp::overlays::init_overlays();
    std::size_t checked = 0;
    std::size_t overlays = 0;
    bool level2_checked = false;
    bool attic_checked = false;
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& section : section_table) {
            if (section.ram_addr != 0x80000400u) {
                continue;
            }
            load_overlays(section.rom_addr, static_cast<int32_t>(section.ram_addr), section.size);
            for (std::size_t i = 0; i < section.num_funcs; ++i) {
                const auto& entry = section.funcs[i];
                if (get_function(static_cast<int32_t>(section.ram_addr + entry.offset)) != entry.func) {
                    std::fputs("Overlay callback lookup mismatch.\n", stderr);
                    return 1;
                }
                ++checked;
            }
            if (section.rom_addr == 0x000A9300u) {
                if (get_function(static_cast<int32_t>(0x80000474u)) != ovl01_func_80000474) {
                    std::fputs("Andy's House object callback lookup mismatch.\n", stderr);
                    return 1;
                }
                attic_checked = true;
            }
            if (section.rom_addr == 0x000AC460u) {
                if (get_function(static_cast<int32_t>(0x800009F8u)) != ovl02_func_800009F8) {
                    std::fputs("Level 2 callback lookup mismatch.\n", stderr);
                    return 1;
                }
                level2_checked = true;
            }
            unload_overlays(static_cast<int32_t>(section.ram_addr), section.size);
            ++overlays;
        }
    }
    if (overlays != 38 || !level2_checked || !attic_checked) {
        std::fputs("Incomplete overlay lookup check.\n", stderr);
        return 1;
    }
    std::printf("PASS: 19 overlays loaded twice; %zu callback lookups, including Andy's House 0x80000474 and Level 2 0x800009F8.\n", checked);
    return 0;
}

} // namespace ts2
