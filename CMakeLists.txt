# =============================================================================
# SpaceRanger
# Toy Story 2: Buzz Lightyear to the Rescue (N64) - native static recompilation
#
# Adapted from the ToyStory2OldNative build, which is the only configuration of
# these dependencies known to compile, link and boot. What is new here is the
# recompilation itself: 1184 functions across 21 sections, with all nineteen
# low-memory overlays modelled explicitly (see docs/STATUS.md).
#
# Rendering fixes live in source and regeneration hooks. This build does not
# depend on the historical injected bridge DLL or its HFR patches.
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(SpaceRanger LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep optimized native builds useful for startup/crash investigation.
option(TS2_RELEASE_SYMBOLS "Generate debugging symbols for Release builds" OFF)
if(MSVC AND TS2_RELEASE_SYMBOLS)
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>
        $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
endif()

# SpaceRanger builds entirely from trees inside this project. The runtime
# dependencies were copied from ToyStory2OldNative with their local
# modifications intact; see docs/deps-provenance.md and docs/deps-*.diff for
# the exact revisions and the delta from upstream.
#
# RecompiledFuncs is a copy of a generated N64Recomp pass; see
# RecompiledFuncs/SOURCE_PASS.txt. Regenerate with tools/run_overlays_pass10.ps1.
set(TS2_RECOMP_DIR "${CMAKE_CURRENT_LIST_DIR}/RecompiledFuncs")
set(N64MR_DIR "${CMAKE_CURRENT_LIST_DIR}/deps/N64ModernRuntime")
set(RT64_DIR "${CMAKE_CURRENT_LIST_DIR}/deps/rt64")

foreach(required
    "${TS2_RECOMP_DIR}/funcs.h"
    "${TS2_RECOMP_DIR}/lookup.cpp"
    "${N64MR_DIR}/CMakeLists.txt"
    "${RT64_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_LIST_DIR}/src/stage79_first_vi_audio_wakeup.cpp"
    "${TS2_RECOMP_DIR}/recomp_overlays.inl"
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_sections.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_generated_bridge_layer.c"
    src/ts2_phase5b2b_runtime_adapters.cpp
    src/ts2_phase5b2b_missing_lowlevel.c
    src/ts2_phase5b2b_thread_queue_helpers.c
    src/ts2_phase5b2b_timer_pi_compat.cpp
    src/ts2_recovery_internal_aliases_compat.c
)
    if(NOT IS_ABSOLUTE "${required}")
        set(required "${CMAKE_CURRENT_LIST_DIR}/${required}")
    endif()
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required recovery input missing: ${required}")
    endif()
endforeach()

execute_process(
    COMMAND git -C "${N64MR_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE TS2_N64MR_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND git -C "${RT64_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE TS2_RT64_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "N64ModernRuntime source commit: ${TS2_N64MR_COMMIT}")
message(STATUS "RT64 source commit: ${TS2_RT64_COMMIT}")

add_subdirectory(
    "${N64MR_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/N64ModernRuntime"
)

set(RT64_STATIC ON CACHE BOOL "Build RT64 statically" FORCE)
add_subdirectory(
    "${RT64_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/rt64"
    EXCLUDE_FROM_ALL
)
# =============================================================================
# TS2 OLDNATIVE V59 CONSOLIDATED RECOMPFRONTEND COMPAT
#
# Compatibility-only integration for the pinned fresh RecompFrontend.
# This block deliberately changes no Toy Story 2 runtime/source behavior.
# =============================================================================

set(FRONTEND_DIR "${CMAKE_CURRENT_LIST_DIR}/deps/RecompFrontend")
set(RECOMP_FRONTEND_N64MODERNRUNTIME_PATH "${N64MR_DIR}")
set(RECOMP_FRONTEND_RT64_PATH "${RT64_DIR}")

# RT64's shader helper functions are callable by RecompFrontend, but their DXC
# variables must exist in this parent/sibling caller scope. This is the proven
# SpaceRanger/Banjo-style configuration.
set(DXC_COMMON_OPTS
    "-I${CMAKE_CURRENT_LIST_DIR}/src"
)

set(DXC_DXIL_OPTS
    "-Wno-ignored-attributes"
)

set(DXC_SPV_OPTS
    "-spirv"
    "-fspv-target-env=vulkan1.0"
    "-fvk-use-dx-layout"
)

set(DXC_PS_OPTS
    "${DXC_COMMON_OPTS}"
    "-E" "PSMain"
    "-T" "ps_6_0"
    "-D" "DYNAMIC_RENDER_PARAMS"
)

set(DXC_VS_OPTS
    "${DXC_COMMON_OPTS}"
    "-E" "VSMain"
    "-T" "vs_6_0"
    "-D" "DYNAMIC_RENDER_PARAMS"
    "-fvk-invert-y"
)

set(DXC_CS_OPTS
    "${DXC_COMMON_OPTS}"
    "-E" "CSMain"
    "-T" "cs_6_0"
)

set(DXC_GS_OPTS
    "${DXC_COMMON_OPTS}"
    "-E" "GSMain"
    "-T" "gs_6_0"
)

set(DXC_RT_OPTS
    "${DXC_COMMON_OPTS}"
    "-D" "RT_SHADER"
    "-T" "lib_6_3"
    "-fspv-target-env=vulkan1.1spirv1.4"
    "-fspv-extension=SPV_KHR_ray_tracing"
    "-fspv-extension=SPV_EXT_descriptor_indexing"
)

if(WIN32)
    set(DXC
        "${RT64_DIR}/src/contrib/dxc/bin/x64/dxc.exe"
    )
    add_compile_definitions(NOMINMAX)
endif()

add_subdirectory(
    "${FRONTEND_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/RecompFrontend"
    EXCLUDE_FROM_ALL
)

# =============================================================================
# TS2 V143 MSVC RECOMPFRONTEND WARNING-FLAG COMPAT
#
# The pinned N64ModernRuntime/RecompFrontend CMake files add the GCC/Clang
# warning suppression -Wno-unused-parameter. Visual Studio forwards it to
# cl.exe as /Wno-unused-parameter, which MSVC rejects with D8021.
#
# Keep the dependency checkouts pristine: remove only that exact warning flag
# from the already-created targets in this parent project, and only on MSVC.
# =============================================================================
if(MSVC)
    foreach(TS2_MSVC_COMPAT_TARGET
        ultramodern
        librecomp
        recompinput
        recompui
    )
        if(NOT TARGET ${TS2_MSVC_COMPAT_TARGET})
            message(FATAL_ERROR
                "V143 expected target ${TS2_MSVC_COMPAT_TARGET} to exist"
            )
        endif()

        get_target_property(
            TS2_MSVC_COMPILE_OPTIONS
            ${TS2_MSVC_COMPAT_TARGET}
            COMPILE_OPTIONS
        )

        if(TS2_MSVC_COMPILE_OPTIONS
           AND NOT TS2_MSVC_COMPILE_OPTIONS MATCHES "-NOTFOUND$")
            list(REMOVE_ITEM TS2_MSVC_COMPILE_OPTIONS
                "-Wno-unused-parameter"
                "/Wno-unused-parameter"
            )
            set_property(
                TARGET ${TS2_MSVC_COMPAT_TARGET}
                PROPERTY COMPILE_OPTIONS "${TS2_MSVC_COMPILE_OPTIONS}"
            )
        endif()

        get_target_property(
            TS2_MSVC_INTERFACE_COMPILE_OPTIONS
            ${TS2_MSVC_COMPAT_TARGET}
            INTERFACE_COMPILE_OPTIONS
        )

        if(TS2_MSVC_INTERFACE_COMPILE_OPTIONS
           AND NOT TS2_MSVC_INTERFACE_COMPILE_OPTIONS MATCHES "-NOTFOUND$")
            list(REMOVE_ITEM TS2_MSVC_INTERFACE_COMPILE_OPTIONS
                "-Wno-unused-parameter"
                "/Wno-unused-parameter"
            )
            set_property(
                TARGET ${TS2_MSVC_COMPAT_TARGET}
                PROPERTY INTERFACE_COMPILE_OPTIONS
                "${TS2_MSVC_INTERFACE_COMPILE_OPTIONS}"
            )
        endif()

        get_target_property(
            TS2_MSVC_POST_OPTIONS
            ${TS2_MSVC_COMPAT_TARGET}
            COMPILE_OPTIONS
        )
        message(STATUS
            "V143 MSVC options ${TS2_MSVC_COMPAT_TARGET}: "
            "${TS2_MSVC_POST_OPTIONS}"
        )
    endforeach()
endif()
# TS2 V144 LIBRECOMP MSVC FLAG FIX: librecomp added to target filter
# TS2 V143 MSVC RECOMPFRONTEND WARNING-FLAG COMPAT END
# =============================================================================

# V59 renderer-only compatibility boundary.
#
# Fresh RecompFrontend's src/api/*.cpp group is the game/mod UI export layer.
# ui_api_events.cpp intentionally includes a project-owned patches/ui_funcs.h.
# ToyStory2OldNative does not yet restore/activate that mod-UI patch ABI, and
# this milestone does not call register_ui_exports or any src/api symbol.
#
# Historical Toy Story 2 V11.6 used this same renderer/config/core-only split.
# Keep the RecompFrontend checkout pristine: filter the target's source list
# from the parent CMake instead of editing recompui/CMakeLists.txt.
get_target_property(TS2_RECOMPUI_ALL_SOURCES recompui SOURCES)
if(NOT TS2_RECOMPUI_ALL_SOURCES)
    message(FATAL_ERROR "V59 could not read recompui SOURCES")
endif()

set(TS2_RECOMPUI_RENDERER_ONLY_SOURCES "")
set(TS2_RECOMPUI_API_SOURCE_COUNT 0)

foreach(TS2_RECOMPUI_SOURCE IN LISTS TS2_RECOMPUI_ALL_SOURCES)
    string(REPLACE "\\" "/" TS2_RECOMPUI_SOURCE_NORM "${TS2_RECOMPUI_SOURCE}")

    if(TS2_RECOMPUI_SOURCE_NORM MATCHES "(^|/)src/api/[^/]+\\.cpp$")
        math(EXPR TS2_RECOMPUI_API_SOURCE_COUNT
            "${TS2_RECOMPUI_API_SOURCE_COUNT} + 1"
        )
        message(STATUS
            "TS2 V59 renderer-only: pruning RecompFrontend API source ${TS2_RECOMPUI_SOURCE}"
        )
    else()
        list(APPEND TS2_RECOMPUI_RENDERER_ONLY_SOURCES
            "${TS2_RECOMPUI_SOURCE}"
        )
    endif()
endforeach()

# V52 audited this pinned frontend and found exactly three stock API sources:
# ui_api.cpp, ui_api_events.cpp and ui_api_images.cpp.
if(NOT TS2_RECOMPUI_API_SOURCE_COUNT EQUAL 3)
    message(FATAL_ERROR
        "V59 expected exactly 3 RecompFrontend src/api C++ sources; found ${TS2_RECOMPUI_API_SOURCE_COUNT}"
    )
endif()

set_property(TARGET recompui PROPERTY SOURCES
    "${TS2_RECOMPUI_RENDERER_ONLY_SOURCES}"
)

message(STATUS
    "TS2 V59 renderer-only RecompFrontend API prune count: ${TS2_RECOMPUI_API_SOURCE_COUNT}"
)

# Fresh recompinput's public header directly includes <SDL.h>. Make the known
# RT64-bundled SDL2 dependency visible transitively without editing frontend.
if(WIN32)
    set(TS2_FRONTEND_SDL2_INCLUDE
        "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/include"
    )
    set(TS2_FRONTEND_SDL2_LIBDIR
        "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64"
    )

    target_include_directories(recompinput PUBLIC
        "${TS2_FRONTEND_SDL2_INCLUDE}"
    )

    # recompui also consumes SDL APIs internally on Windows. Give it the same
    # existing include source directly to avoid relying on incidental ordering.
    target_include_directories(recompui PRIVATE
        "${TS2_FRONTEND_SDL2_INCLUDE}"
    )

    if(TARGET SDL2)
        target_link_libraries(recompinput PUBLIC SDL2)
        target_link_libraries(recompui PRIVATE SDL2)
    else()
        add_library(ts2_frontend_sdl2 STATIC IMPORTED GLOBAL)
        set_target_properties(ts2_frontend_sdl2 PROPERTIES
            IMPORTED_LOCATION
                "${TS2_FRONTEND_SDL2_LIBDIR}/SDL2.lib"
            INTERFACE_INCLUDE_DIRECTORIES
                "${TS2_FRONTEND_SDL2_INCLUDE}"
        )

        target_link_libraries(recompinput PUBLIC ts2_frontend_sdl2)
        target_link_libraries(recompui PRIVATE ts2_frontend_sdl2)
    endif()
endif()

# Baseline linking is intentionally emitted later, after the target definition.
# TS2 OLDNATIVE V59 CONSOLIDATED RECOMPFRONTEND COMPAT END
# =============================================================================

file(GLOB TS2_RECOMP_C CONFIGURE_DEPENDS "${TS2_RECOMP_DIR}/funcs_*.c")
list(LENGTH TS2_RECOMP_C TS2_RECOMP_COUNT)
message(STATUS "Recovery generated C files: ${TS2_RECOMP_COUNT}")

# The user's current regenerated tree has 23 translation units while the
# historical checkpoint had 26. Translation-unit count itself is not part of
# guest semantics, so this first recovery build accepts the preserved current
# grouping and tests whether the old direct renderer can link against it.
if(TS2_RECOMP_COUNT LESS 20)
    message(FATAL_ERROR "Generated tree looks incomplete: only ${TS2_RECOMP_COUNT} funcs_*.c files")
endif()

add_library(spaceranger_recompiled STATIC
    ${TS2_RECOMP_C}
    "${TS2_RECOMP_DIR}/lookup.cpp"
)

target_include_directories(spaceranger_recompiled PRIVATE
    "${TS2_RECOMP_DIR}"
    "${N64MR_DIR}/N64Recomp/include"
    "${N64MR_DIR}/librecomp/include"
    "${N64MR_DIR}/ultramodern/include"
)
if(MSVC)
    target_compile_options(spaceranger_recompiled PRIVATE /W0)
endif()

add_executable(spaceranger
    src/render_support.cpp
    src/input_replay.cpp
    src/runtime_diagnostics.cpp
    src/actor_submission_trace.cpp
    "${CMAKE_CURRENT_LIST_DIR}/src/stage79_first_vi_audio_wakeup.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/bridge_audio.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/rsp/ts2_aspMain.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_sections.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_generated_bridge_layer.c"
    src/ts2_phase5b2b_runtime_adapters.cpp
    src/ts2_phase5b2b_missing_lowlevel.c
    src/ts2_phase5b2b_thread_queue_helpers.c
    src/ts2_phase5b2b_timer_pi_compat.cpp
    src/ts2_recovery_internal_aliases_compat.c
)

# SpaceRanger is a pure from-source build. No frozen exe, no injected bridge.
set_target_properties(spaceranger PROPERTIES
    OUTPUT_NAME "SpaceRanger"
)
set(TS2_RUNTIME_OUTPUT_DIRECTORY "" CACHE PATH "Optional output folder for a separate test build")
if(TS2_RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(spaceranger PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TS2_RUNTIME_OUTPUT_DIRECTORY}")
endif()
if(MSVC)
    target_link_options(spaceranger PRIVATE /MAP)
endif()

target_include_directories(spaceranger PRIVATE
    "${TS2_RECOMP_DIR}"
    "${N64MR_DIR}/N64Recomp/include"
    "${N64MR_DIR}/librecomp/include"
    "${N64MR_DIR}/ultramodern/include"
    "${RT64_DIR}/src"
    "${RT64_DIR}/include"
    "${RT64_DIR}/src/contrib"
    "${RT64_DIR}/src/contrib/plume"
    "${RT64_DIR}/src/contrib/imgui"
    "${RT64_DIR}/src/contrib/hlslpp/include"
    "${RT64_DIR}/src/contrib/dxc/inc"
    "${RT64_DIR}/src/contrib/mupen64plus-core/src/api"
    "${RT64_DIR}/src/contrib/nativefiledialog-extended/src/include"
    "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/include"
)

target_compile_definitions(spaceranger PRIVATE
    NOMINMAX
    HLSL_CPU
    FFX_GCC
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    __PRFCHWINTRIN_H
    SDL_MAIN_HANDLED
    TS2_N64MR_COMMIT="${TS2_N64MR_COMMIT}"
    TS2_RT64_COMMIT="${TS2_RT64_COMMIT}"
    TS2_ROM_PATH="${CMAKE_CURRENT_LIST_DIR}/rom/baserom.us.rev0.z64"
)

# =============================================================================
# SpaceRanger bridge-layer declarations
#
# src/ts2_generated_bridge_layer.c defines a set of ts2_* symbols the host
# calls. OldNative declared them by hand-editing the GENERATED funcs.h, so
# every regeneration dropped them. Here they live in their own header and are
# force-included, leaving RecompiledFuncs/ regenerable.
# =============================================================================
target_include_directories(spaceranger PRIVATE "${CMAKE_CURRENT_LIST_DIR}/include")
if(MSVC)
    target_compile_options(spaceranger PRIVATE
        "/FI${CMAKE_CURRENT_LIST_DIR}/include/ts2_bridge_layer.h")
else()
    target_compile_options(spaceranger PRIVATE
        -include "${CMAKE_CURRENT_LIST_DIR}/include/ts2_bridge_layer.h")
endif()

if(MSVC)
    target_compile_options(spaceranger PRIVATE /W3 /EHsc)
endif()

if(WIN32)
    target_link_directories(spaceranger PRIVATE
        "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64"
    )
endif()

target_link_libraries(spaceranger PRIVATE
    spaceranger_recompiled
    librecomp
    ultramodern
    rt64
)

if(WIN32)
    target_link_libraries(spaceranger PRIVATE
        Ole32.lib
        Winmm.lib
    )
endif()

add_dependencies(spaceranger rt64)

if(WIN32)
    add_custom_command(TARGET spaceranger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Freetype::Freetype>"
            "$<TARGET_FILE_DIR:spaceranger>/freetype.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RT64_DIR}/src/contrib/mupen64plus-win32-deps/SDL2-2.26.3/lib/x64/SDL2.dll"
            "$<TARGET_FILE_DIR:spaceranger>/SDL2.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RT64_DIR}/src/contrib/dxc/bin/x64/dxcompiler.dll"
            "$<TARGET_FILE_DIR:spaceranger>/dxcompiler.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${RT64_DIR}/src/contrib/dxc/bin/x64/dxil.dll"
            "$<TARGET_FILE_DIR:spaceranger>/dxil.dll"
        VERBATIM
    )
endif()

# TS2 OLDNATIVE FRONTEND RECONCILIATION SAFETY TRAPS
target_sources(spaceranger PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_frontend_unresolved_safety_traps.cpp"
)

# TS2 OLDNATIVE V23 PROJECT-LOCAL BOOTSTRAP SOURCE
target_sources(spaceranger PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_oldnative_bootstrap_handoff.cpp"
)
# =============================================================================
# TS2 OLDNATIVE V59 LATE FRONTEND LINK
# Emitted only after spaceranger has been declared by the root CMake.
# No frontend APIs are called by Stage79 in this milestone.
# =============================================================================
target_link_libraries(spaceranger PRIVATE
    recompui
    recompinput
)
# TS2 OLDNATIVE V59 LATE FRONTEND LINK END
# =============================================================================
# TS2 OLDNATIVE V64 CANONICAL FRONTEND COMPILE PROBE
# Compile/link only. Stage79 ownership remains unchanged.
# =============================================================================
if(NOT TARGET spaceranger)
    message(FATAL_ERROR "V64 requires spaceranger to exist before probe source registration")
endif()

target_sources(spaceranger PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/ts2_canonical_frontend_compile_probe_v64.cpp"
)

# TS2 OLDNATIVE V64 CANONICAL FRONTEND COMPILE PROBE END
# =============================================================================

# TS2 OLDNATIVE V87 STOCK RECOMPUI EVENT CONSUMER
# Restores only ui_api_events.cpp after the V59 renderer/core API prune.
# ui_api.cpp and ui_api_images.cpp remain pruned; register_ui_exports stays inactive.
target_sources(recompui PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/deps/RecompFrontend/recompui/src/api/ui_api_events.cpp"
)
# TS2 OLDNATIVE V87 STOCK RECOMPUI EVENT CONSUMER END

# =============================================================================
# TS2 V152 RECOMPFRONTEND RUNTIME ASSETS
# =============================================================================
if(TARGET spaceranger)
    if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/assets")
        message(FATAL_ERROR
            "V152 expected project runtime assets at "
            "${CMAKE_CURRENT_LIST_DIR}/assets"
        )
    endif()

    add_custom_command(TARGET spaceranger POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:spaceranger>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_LIST_DIR}/assets"
            "$<TARGET_FILE_DIR:spaceranger>/assets"
        COMMENT "Staging Toy Story 2 RecompFrontend runtime assets"
        VERBATIM
    )
endif()
# TS2 V152 RECOMPFRONTEND RUNTIME ASSETS END

# Optional focused integration checks; does not launch the game or need speakers.
add_executable(ts2_projection_check EXCLUDE_FROM_ALL tools/test_ts2_projection.cpp)
target_include_directories(ts2_projection_check PRIVATE "${CMAKE_CURRENT_LIST_DIR}/deps/rt64/src")

option(TS2_BUILD_AUDIO_TESTS "Build RSP/SDL audio integration checks" OFF)
if(TS2_BUILD_AUDIO_TESTS)
    add_executable(ts2_audio_smoke tools/test_audio_rsp.cpp
        src/bridge_audio.cpp rsp/ts2_aspMain.cpp)
    target_include_directories(ts2_audio_smoke PRIVATE src
        "${N64MR_DIR}/N64Recomp/include"
        "${N64MR_DIR}/librecomp/include"
        "${N64MR_DIR}/ultramodern/include"
        "${TS2_FRONTEND_SDL2_INCLUDE}")
    target_compile_definitions(ts2_audio_smoke PRIVATE SDL_MAIN_HANDLED
        TS2_TEST_ROM="${CMAKE_CURRENT_LIST_DIR}/rom/baserom.us.rev0.z64")
    target_link_libraries(ts2_audio_smoke PRIVATE librecomp)
    target_link_directories(ts2_audio_smoke PRIVATE "${TS2_FRONTEND_SDL2_LIBDIR}")
    target_link_libraries(ts2_audio_smoke PRIVATE SDL2)
    add_executable(ts2_audio_pacing tools/test_audio_pacing.cpp)
    target_include_directories(ts2_audio_pacing PRIVATE "${TS2_FRONTEND_SDL2_INCLUDE}")
    target_link_directories(ts2_audio_pacing PRIVATE "${TS2_FRONTEND_SDL2_LIBDIR}")
    target_link_libraries(ts2_audio_pacing PRIVATE SDL2)
    add_executable(ts2_audio_replay tools/test_audio_replay.cpp tools/audio_rsp_scalar.cpp rsp/ts2_aspMain.cpp)
    target_include_directories(ts2_audio_replay PRIVATE "${N64MR_DIR}/librecomp/include"
        "${N64MR_DIR}/N64Recomp/include" "${N64MR_DIR}/ultramodern/include")
    target_link_libraries(ts2_audio_replay PRIVATE librecomp)
endif()

# CPU-only integration coverage of the actual RT64 scene/matrix matcher.
option(TS2_BUILD_RENDER_TESTS "Build frame-matching integration checks" OFF)
if(TS2_BUILD_RENDER_TESTS)
    add_executable(ts2_frame_matching tools/test_frame_matching.cpp)
    target_include_directories(ts2_frame_matching PRIVATE
        $<TARGET_PROPERTY:rt64,INCLUDE_DIRECTORIES>)
    target_compile_definitions(ts2_frame_matching PRIVATE
        $<TARGET_PROPERTY:rt64,COMPILE_DEFINITIONS>)
    target_link_directories(ts2_frame_matching PRIVATE "${TS2_FRONTEND_SDL2_LIBDIR}")
    target_link_libraries(ts2_frame_matching PRIVATE rt64)
endif()
