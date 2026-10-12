#include <cstdint>
#include <filesystem>

#include "ultramodern/config.hpp"
#include "ultramodern/renderer_context.hpp"
#include "ultramodern/renderer_context.hpp"
#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "recompinput/players.h"
#include "librecomp/game.hpp"

namespace ts2_oldnative_v64_compile_probe {

[[maybe_unused]]
static void initialize_frontend_metadata_definition_only() {
    recompui::programconfig::set_program_name("SpaceRanger");
    recompui::programconfig::set_program_id(u8"spaceranger");

    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");

    recomp::register_config_path(
        std::filesystem::current_path() / "spaceranger-config"
    );

    recompinput::players::set_single_player_mode(true);

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;

    recompui::config::create_general_tab(general_options);
    recompui::config::create_controls_tab();
    recompui::config::create_graphics_tab();
    recompui::config::create_sound_tab();
    recompui::config::finalize();

    // Intentionally NOT calling recompui::register_ui_exports() here.
    // V59 is renderer/config/core-only until patches/ui_funcs.h is restored.
}

[[maybe_unused]]
static ultramodern::renderer::callbacks_t
make_canonical_renderer_callbacks_definition_only() {
    return ultramodern::renderer::callbacks_t{
        .create_render_context =
            [](uint8_t* rdram,
               ultramodern::renderer::WindowHandle window_handle,
               bool developer_mode) {
                auto presentation_mode =
                    ultramodern::renderer::PresentationMode::PresentEarly;

                return recompui::renderer::create_render_context(
                    rdram,
                    window_handle,
                    presentation_mode,
                    developer_mode
                );
            },
    };
}

} // namespace ts2_oldnative_v64_compile_probe