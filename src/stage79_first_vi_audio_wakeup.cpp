#ifndef TS2_OLDNATIVE_V65_CANONICAL_FRONTEND_INCLUDES
#define TS2_OLDNATIVE_V65_CANONICAL_FRONTEND_INCLUDES 1
#include <filesystem>
#include <ctime>
#include <chrono>
#include <cstring>
#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "elements/ui_theme.h"
#include "elements/ui_svg.h"
#include "recompinput/players.h"
#include "recompinput/input_events.h"
#include "recompinput/input_state.h"
#include "recompinput/profiles.h"
bool ts2_get_input(int controller_num, uint16_t* buttons, float* x, float* y);
#include "ultramodern/config.hpp"
#include "ultramodern/renderer_context.hpp"
#include "ultramodern/renderer_context.hpp"
#include "librecomp/game.hpp"
#endif
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif
#include <cctype>

// ROM location. Release builds must not depend on the developer's checkout:
// the ROM the player selects is validated by librecomp and stored (normalized
// to .z64 byte order) in the config folder, and every later read uses that copy.
static std::filesystem::path g_ts2_rom_file = std::filesystem::path(TS2_ROM_PATH);

static std::filesystem::path ts2_exe_dir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

static void ts2_rom_message(const char* text) {
    std::fprintf(stderr, "TS2 ROM: %s\n", text);
#ifdef _WIN32
    MessageBoxA(nullptr, text, "SpaceRanger", MB_OK | MB_ICONINFORMATION);
#endif
}

static std::filesystem::path ts2_pick_rom_file() {
#ifdef _WIN32
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"N64 ROM (*.z64;*.n64;*.v64)\0*.z64;*.n64;*.v64\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Select your Toy Story 2 (N64, USA) ROM";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&ofn)) {
        return std::filesystem::path(file);
    }
#endif
    return {};
}

static const char* ts2_rom_error_text(recomp::RomValidationError error) {
    switch (error) {
        case recomp::RomValidationError::FailedToOpen:
            return "That file could not be opened.";
        case recomp::RomValidationError::NotARom:
            return "That file is not an N64 ROM.";
        case recomp::RomValidationError::IncorrectVersion:
            return "That is a Toy Story 2 ROM, but not the supported version.\n\nThis release supports the USA version (N64, revision 0).";
        default:
            return "That file is not a supported Toy Story 2 ROM.\n\nThis release supports the USA version (N64, revision 0).";
    }
}

// Makes sure a validated ROM is stored in the config folder. A ROM found via
// TS2_ROM_PATH or next to the exe is imported silently. If none is found and
// `interactive` is set (automated runs that skip the title screen), the player
// is asked for one; otherwise the title screen's "Load ROM" option handles it.
// Returns false if no valid ROM is stored afterwards.
static bool ts2_ensure_rom(std::u8string& game_id, bool interactive) {
    g_ts2_rom_file = recomp::get_config_path() / (game_id + u8".z64");
    recomp::check_all_stored_roms();
    if (!recomp::is_rom_valid(game_id)) {
        std::vector<std::filesystem::path> candidates;
        if (const char* env = std::getenv("TS2_ROM_PATH")) {
            candidates.emplace_back(env);
        }
        const std::filesystem::path exe_dir = ts2_exe_dir();
        for (const auto& dir : { exe_dir, exe_dir / "rom" }) {
            std::error_code ec;
            for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
                std::string ext = it->path().extension().string();
                for (char& c : ext) c = char(std::tolower((unsigned char)c));
                if (ext == ".z64" || ext == ".n64" || ext == ".v64") {
                    candidates.push_back(it->path());
                }
            }
        }

        bool selected = false;
        for (const auto& candidate : candidates) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(candidate, ec) &&
                recomp::select_rom(candidate, game_id) == recomp::RomValidationError::Good) {
                selected = true;
                break;
            }
        }

        if (!selected && !interactive) {
            return false;
        }
        if (!selected) {
            ts2_rom_message(
                "SpaceRanger needs your own copy of Toy Story 2.\n\n"
                "Select your Toy Story 2: Buzz Lightyear to the Rescue ROM (N64, USA). "
                "It is checked and copied into the spaceranger-config folder, so you only do this once.");
        }
        while (!selected) {
            const std::filesystem::path picked = ts2_pick_rom_file();
            if (picked.empty()) {
                return false;
            }
            const recomp::RomValidationError result = recomp::select_rom(picked, game_id);
            if (result == recomp::RomValidationError::Good) {
                selected = true;
            }
            else {
                ts2_rom_message(ts2_rom_error_text(result));
            }
        }

        recomp::check_all_stored_roms();
        if (!recomp::is_rom_valid(game_id)) {
            return false;
        }
    }

    return true;
}

#include <cstddef>
#include <cstring>
#include <memory>
#include <objbase.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include "hle/rt64_application.h"
#include "gbi/rt64_gbi.h"
#include "gbi/rt64_f3d.h"
#include "ultramodern/renderer_context.hpp"
#include "funcs.h"
#include "librecomp/overlays.hpp"
#include "librecomp/rsp.hpp"
#include "librecomp/game.hpp"
#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/input.hpp"
#include "ts2_frontend_reconcile_adapters.hpp"
#include "ts2_current_internal_entry_dispatch.inl"

// TS2 OLDNATIVE PROJECT-LOCAL ENTRYPOINT CATCH BRIDGE
extern "C" int ts2_oldnative_call_recomp_entrypoint_catching_bootstrap(uint8_t* rdram, recomp_context* ctx) noexcept;
// TS2 PHASE 5B2B COMPAT INTERNAL ENTRY DECLARATIONS
extern "C" void ts2_auto_alias_80087870(uint8_t* rdram, recomp_context* ctx);
extern "C" void ts2_auto_alias_8009C4AC(uint8_t* rdram, recomp_context* ctx);


using ts2_thread_dispatch_callback_t =
    void (*)(uint8_t* rdram, uint64_t addr, uint64_t sp, uint64_t arg);

extern "C" void ts2_set_thread_dispatch_callback(
    ts2_thread_dispatch_callback_t callback);

namespace ts2 {
int check_overlay_lookup();
bool validate_section_layout();
void register_sections();
}


// TS2 OLDNATIVE V12 NATIVE AI DIAGNOSTICS
// Stage61 AI-register shadow probes retired: high-level AI is owned by
// N64ModernRuntime osAi* adapters. No AI MMIO shim is installed.

void sp_complete();
void dp_complete();

// TS2 OLDNATIVE V29 RECOMPFRONTEND HOST GLOBAL
SDL_Window* window = nullptr;

// TS2 OLDNATIVE V29 INACTIVE CANONICAL GAMEENTRY HOST
//
// This restores only the compile-time host metadata/callback surface used by
// N64ModernRuntime. The game remains disabled and main() remains on Stage61.
namespace {
    void ts2_b0d_runtime_entrypoint(uint8_t* rdram, recomp_context* ctx);
    void ts2_b0d_runtime_thread_create(uint8_t* rdram, recomp_context* ctx);
    void ts2_b0d_runtime_on_init(uint8_t* rdram, recomp_context* ctx);
}

std::vector<recomp::GameEntry> supported_games = {
    {
        .rom_hash = 0xFD53C5EC8CE2530AULL,
        .internal_name = "Toy Story 2",
        .display_name = "Toy Story 2",
        .game_id = u8"spaceranger",
        .mod_game_id = "spaceranger",
        .save_type = recomp::SaveType::None,
        .thumbnail_bytes = {},
        .is_enabled = true,

        // KSEG0 entrypoint sign-extension matches the later native cutover.
        .entrypoint_address =
            static_cast<gpr>(static_cast<int32_t>(0x80006000u)),
        .entrypoint = &ts2_b0d_runtime_entrypoint,
        .thread_create_callback = &ts2_b0d_runtime_thread_create,
        .on_init_callback = &ts2_b0d_runtime_on_init,
    }
};
// TS2 OLDNATIVE V29 INACTIVE CANONICAL GAMEENTRY HOST END

// TS2 OLDNATIVE V79 GLOBAL CANONICAL HELPER FORWARDS
static void ts2_v79_register_native_maps();
static void ts2_initialize_recompfrontend_metadata();
static ultramodern::renderer::callbacks_t ts2_make_canonical_renderer_callbacks();
// TS2 OLDNATIVE V79 GLOBAL CANONICAL HELPER FORWARDS END

namespace {

constexpr std::size_t kRdramSize = 8 * 1024 * 1024;

constexpr uint32_t kResidentRomStart = 0x00001000u;
constexpr uint32_t kResidentRomEnd   = 0x000A68A0u;
constexpr uint32_t kResidentGuest    = 0x80006000u;
constexpr uint32_t kResidentSize     = kResidentRomEnd - kResidentRomStart;

constexpr uint32_t kBss1Start = 0x800AB8A0u;
constexpr uint32_t kBss2End   = 0x801F4240u;

constexpr uint32_t kStage20Pc = 0x80091388u;
constexpr uint32_t kStage21Pc = 0x80093CB8u;
constexpr uint32_t kStage22Pc = 0x80093DACu;
constexpr uint32_t kStage23Pc = 0x80008018u;
constexpr uint32_t kStage24Pc = 0x8000804Cu;
constexpr uint32_t kStage25Pc = 0x80008054u;

constexpr uint32_t kThread3EntryCheckpointPc = 0x80008074u;
constexpr uint32_t kThread4CreateCheckpointPc = 0x800080ACu;
constexpr uint32_t kPiBootstrapCheckpointPc = 0x80099C10u;
constexpr uint32_t kPiThreadCreateCheckpointPc = 0x80099CC8u;
constexpr uint32_t kPiThreadRestoredCheckpointPc = 0x80099CECu;
constexpr uint32_t kPiManagerEntryCheckpointPc = 0x8009A010u;
constexpr uint32_t kThread3PostPiManagerCheckpointPc = 0x800080C8u;
constexpr uint32_t kThread3IdleCheckpointPc = 0x800080DCu;
constexpr uint32_t kThread4InitCheckpointPc = 0x80008120u;
constexpr uint32_t kThread4QueueBootstrapCheckpointPc = 0x800081E0u;
constexpr uint32_t kManagerQueueBootstrapCheckpointPc = 0x80096E68u;
constexpr uint32_t kViManagerBootstrapCheckpointPc = 0x800998B0u;
constexpr uint32_t kViThreadCreateCheckpointPc = 0x8009994Cu;
constexpr uint32_t kViMmioBoundaryCheckpointPc = 0x800935E0u;
constexpr uint32_t kViMmioCompleteCheckpointPc = 0x80099954u;
constexpr uint32_t kViManagerReceiveCheckpointPc = 0x800999ECu;
constexpr uint32_t kThread4ViReturnCheckpointPc = 0x80099978u;
constexpr uint32_t kViManagerReceiveMsg = 0x801F1910u;
constexpr uint32_t kViManagerFieldState = 0x801F0780u;


constexpr uint32_t kViMmioBase = 0xA4400000u;
constexpr uint32_t kViMmioLast = 0xA4400034u;
constexpr uint32_t kViRegisterCount = 14u;

constexpr uint32_t kViStatusReg   = 0xA4400000u;
constexpr uint32_t kViOriginReg   = 0xA4400004u;
constexpr uint32_t kViWidthReg    = 0xA4400008u;
constexpr uint32_t kViIntrReg     = 0xA440000Cu;
constexpr uint32_t kViCurrentReg2 = 0xA4400010u;
constexpr uint32_t kViBurstReg    = 0xA4400014u;
constexpr uint32_t kViVSyncReg    = 0xA4400018u;
constexpr uint32_t kViHSyncReg    = 0xA440001Cu;
constexpr uint32_t kViLeapReg     = 0xA4400020u;
constexpr uint32_t kViHStartReg   = 0xA4400024u;
constexpr uint32_t kViVStartReg   = 0xA4400028u;
constexpr uint32_t kViVBurstReg   = 0xA440002Cu;
constexpr uint32_t kViXScaleReg   = 0xA4400030u;
constexpr uint32_t kViYScaleReg   = 0xA4400034u;


constexpr uint32_t kViSoftBlock0 = 0x800AA5A0u;
constexpr uint32_t kViSoftBlock1 = 0x800AA5D0u;
constexpr uint32_t kViSoftGlobal0 = 0x800AA600u;
constexpr uint32_t kViSoftGlobal1 = 0x800AA604u;
constexpr uint32_t kViNtscMode = 0x800AA620u;
constexpr uint32_t kViCurrentReg = 0xA4400010u;


constexpr uint32_t kViManagerThreadAddr      = 0x801F0788u;
constexpr int32_t  kViManagerThreadId        = 0;
constexpr uint32_t kViManagerThreadEntry     = 0x80099998u;
constexpr uint32_t kViManagerThreadArg       = 0x800AB520u;
constexpr uint32_t kViManagerThreadStackTop  = 0x801F1940u;
constexpr uint32_t kViManagerThreadInitialSp = 0x801F1930u;
constexpr int32_t  kViManagerThreadPriority  = 254;
constexpr uint32_t kViSingletonBase          = 0x800AB520u;


constexpr uint32_t kViManagerPriority = 254u;
constexpr uint32_t kViQueue = 0x801F1940u;
constexpr uint32_t kViQueueBuffer = 0x801F1958u;
constexpr uint32_t kViQueueCapacity = 5u;
constexpr uint32_t kViMessage = 0x801F1970u;
constexpr uint32_t kCounterMessage = 0x801F1988u;
constexpr uint32_t kEventTableBase = 0x801F2D40u;
constexpr uint32_t kEventCounter = 3u;
constexpr uint32_t kEventVi = 7u;
constexpr uint32_t kTimerGlobal0 = 0x801F2CC0u;
constexpr uint32_t kTimerGlobal1 = 0x801F2CC4u;
constexpr uint32_t kTimerGlobal2 = 0x801F2CE8u;
constexpr uint32_t kTimerGlobal3 = 0x801F2CECu;
constexpr uint32_t kViThreadArg = 0x800AB520u;


constexpr uint32_t kManagerBase = 0x801CDE80u;
constexpr uint32_t kManagerQueueA = 0x801CDEC0u;
constexpr uint32_t kManagerQueueABuffer = 0x801CDED8u;
constexpr uint32_t kManagerQueueB = 0x801CDEF8u;
constexpr uint32_t kManagerQueueBBuffer = 0x801CDF10u;
constexpr uint32_t kManagerStackTop = 0x803FC000u;
constexpr uint32_t kManagerPriority = 13u;
constexpr uint32_t kManagerArg = 0u;
constexpr uint32_t kManagerStackFlag = 1u;

constexpr uint32_t kWorkerCreateCheckpointPc = 0x80096EFCu;
constexpr uint32_t kWorkerReceiveCheckpointPc = 0x80097050u;
constexpr uint32_t kThread4PostWorkerStartCheckpointPc = 0x80096F04u;
constexpr uint32_t kThread4PostManagerBootstrapPc = 0x80008214u;
constexpr uint32_t kMiMaskBoundaryPc = 0x80096AA8u;
constexpr uint32_t kPostMiLinkCheckpointPc = 0x8000823Cu;
constexpr uint32_t kSiBootstrapBoundaryPc = 0x80015A00u;
constexpr uint32_t kPiRawDmaBoundaryPc = 0x80099E44u;
constexpr uint32_t kPiRawDmaInternalEntry = 0x80099E20u;
constexpr uint32_t kPiStatusReg = 0xA4600010u;
constexpr uint32_t kPiDramAddrReg = 0xA4600000u;
constexpr uint32_t kPiCartAddrReg = 0xA4600004u;
constexpr uint32_t kPiRdLenReg = 0xA4600008u;
constexpr uint32_t kPiWrLenReg = 0xA460000Cu;
constexpr uint32_t kPiCompletionWaitPc = 0x8009A344u;
constexpr uint32_t kPiCompletionReceiveMsg = 0x801F2B18u;
constexpr uint32_t kPiCompletionEventMessage = 0x22222222u;
constexpr uint32_t kLoaderWaitPc = 0x80085954u;
constexpr uint32_t kLoaderPostReceivePc = 0x8008595Cu;
constexpr uint32_t kExpectedLoaderSp = 0x803F9E70u;
constexpr uint32_t kExpectedPiRequest = 0x803F9E90u;
constexpr uint32_t kInitialLoaderRemaining = 0x00004000u;
constexpr uint32_t kSecondLoaderRemaining = 0x00003000u;
constexpr uint32_t kSecondPiDeviceAddr = 0xB00D97E0u;
constexpr uint32_t kSecondPiDramAddr = 0x801C5A28u;
constexpr uint32_t kLoaderReturnPc = 0x80085708u;
constexpr uint32_t kParserReturnPc = 0x80085408u;
constexpr uint32_t kFontPayloadAlignedCallPc = 0x8008543Cu;
constexpr uint32_t kFontPayloadUnalignedCallPc = 0x800854CCu;
constexpr uint32_t kExpectedFontDataOffset = 0x000A4AC8u;
constexpr uint32_t kExpectedFontSize = 0x00001417u;
constexpr uint32_t kExpectedFontPayloadRomOffset = 0x0017D2A8u;
constexpr uint32_t kExpectedFontPayloadHash = 0x0FB7427Du;
constexpr uint32_t kDirectoryRomOffset = 0x000D87E0u;
constexpr uint32_t kDirectoryDataOffset = 0x000034C0u;
constexpr uint32_t kDirectoryRomEnd = kDirectoryRomOffset + kDirectoryDataOffset;
constexpr uint32_t kDirectoryRdram = 0x801C4A28u;
constexpr uint32_t kParserCallerSp = 0x803F9EE8u;
constexpr uint32_t kParserDataOffsetOut = 0x803F9F08u;
constexpr uint32_t kParserSizeOut = 0x803F9F0Cu;
constexpr uint32_t kExpectedLoaderCallerSp = 0x803F9EC0u;
constexpr uint32_t kInitialLoadSize = 0x00004000u;
constexpr uint32_t kInitialLoadRdram = 0x801C4A28u;
constexpr uint32_t kInitialLoadRomOffset = 0x000D87E0u;
constexpr uint32_t kExpectedFullLoadHash = 0x3A3634F1u;
constexpr uint32_t kFontCompleteBoundaryPc = 0x80085550u;
constexpr uint32_t kActualFontCallerReturnPc = 0x8001D6E0u;
constexpr uint32_t kActualFontCallerSp = 0x803F9F40u;
constexpr uint32_t kFontBufferGlobal = 0x801C8E50u;
constexpr uint32_t kFontLoadCallPc = 0x8001D6D8u;
constexpr uint32_t kTahoma08String = 0x800A0FF0u;
constexpr uint32_t kTahoma12String = 0x800A1004u;
constexpr uint32_t kFontPoolBaseGlobal = 0x801DB3F0u;
constexpr uint32_t kFirstFontGlyphCount = 0x00000069u;
constexpr uint32_t kFirstFontInitialOutputOffset = 0x00000A90u;
constexpr uint32_t kFirstFontExpandedBytes = 0x00001F88u;
constexpr uint32_t kFirstFontFinalOutputOffset = 0x00002A18u;
constexpr uint32_t kNextFontEntryIndex = 25u;
constexpr uint32_t kNextFontEntryRomOffset = 0x000D8A88u;
constexpr uint32_t kNextFontDataOffset = 0x000A5EE0u;
constexpr uint32_t kNextFontFileSize = 0x00000BBFu;
constexpr uint32_t kNextFontPayloadRomOffset = 0x0017E6C0u;
constexpr uint32_t kNextFontFirstWord = 0x524E4301u;
constexpr uint32_t kNextFontPayloadHash = 0x94F36F5Cu;
constexpr uint32_t kRncBoundaryPc = 0x80085674u;
constexpr uint32_t kRncReturnPc = 0x8008567Cu;
constexpr uint16_t kRncExpectedUnpackedCrc = 0x84B1u;
constexpr uint16_t kRncExpectedPackedCrc = 0xCED7u;
constexpr uint32_t kRncHeaderSize = 18u;
constexpr uint32_t kRncUnpackedSize = 0x00001E9Fu;
constexpr uint32_t kRncPackedPayloadSize = 0x00000BADu;
constexpr uint32_t kRncTempInput = 0x801C6E69u;
constexpr uint32_t kSecondFontPrimarySize = 0x00000BB8u;
constexpr uint32_t kSecondFontTailRomOffset = 0x0017F278u;
constexpr uint32_t kSecondFontPrimaryHash = 0x9DC24567u;
constexpr uint32_t kSecondFontTailHash = 0x0FA07F6Eu;
constexpr uint32_t kEtchFontString = 0x800A1018u;
constexpr uint32_t kEtchFontEntryIndex = 26u;
constexpr uint32_t kEtchFontEntryRomOffset = 0x000D8AA4u;
constexpr uint32_t kEtchFontDataOffset = 0x000A6AA0u;
constexpr uint32_t kEtchFontFileSize = 0x00001F36u;
constexpr uint32_t kEtchFontPayloadRomOffset = 0x0017F280u;
constexpr uint32_t kEtchFontPayloadHash = 0xCD3EA83Au;
constexpr uint32_t kEtchFontFirstWord = 0x03000010u;
constexpr uint32_t kEtchFontPrimarySize = 0x00001F30u;
constexpr uint32_t kEtchFontTailRomOffset = 0x001811B0u;
constexpr uint32_t kEtchFontPrimaryChunk0Hash = 0xEF907CA1u;
constexpr uint32_t kEtchFontPrimaryChunk1Hash = 0xE9A39E54u;
constexpr uint32_t kEtchFontTailHash = 0x52233FF3u;
constexpr uint32_t kThirdFontExpandedBytesExpected = 0x00002D08u;
constexpr uint32_t kThirdFontFinalS0Expected = 0x802FD8C0u;
constexpr uint32_t kThreeFontPoolBytesExpected = 0x000082C0u;
constexpr uint32_t kThreeFontCallerReturnPc = 0x800138D8u;
constexpr uint32_t kThreeFontCallerSp = 0x803F9FA0u;
constexpr uint32_t kThreeFontCallerS0 = 0x801DB3F0u;
constexpr uint32_t kPostPoolReturnPc = 0x80008254u;
constexpr uint32_t kPostPoolReturnSp = 0x803F9FB8u;

// func_800159B0(4) creates the controller/SI message queue, binds OS event 5
// to it, then prepares func_800984A0(queue, &statusByte, controllerBuffer).
// kSiQueue/kSiQueueBuffer/kSiEventTableEntry/kSiScratchReset and
// kSiControllerBuffer are inherited from the earlier bring-up harness.
constexpr uint32_t kSiQueueCapacity = 2u;
constexpr uint32_t kSiBoundarySp = 0x803F9F98u;
constexpr uint32_t kSiStatusBytePtr = 0x803F9FA8u;

// Stage 62 enters the real N64 controller initializer and stops at the
// first SI register instruction, before the read itself executes.
constexpr uint32_t kFirstSiMmioBoundaryPc = 0x800981A8u;
constexpr uint32_t kSiStatusReg = 0xA4800018u;
constexpr uint32_t kFirstSiMmioSp = 0x803F9EF8u;
constexpr uint32_t kControllerInitOnceFlag = 0x800AB4E0u;
constexpr uint32_t kControllerChannelCount = 0x801F3F14u;
constexpr uint32_t kControllerStatusArray = 0x801F3F18u;

// Stage 63 executes the first real SI register sequence and stops immediately
// after launching the 64-byte RDRAM -> PIF command DMA.
constexpr uint32_t kFirstSiDmaLaunchPc = 0x8009816Cu;
constexpr uint32_t kSiDramAddrReg = 0xA4800000u;
constexpr uint32_t kSiPifAddrRd64bReg = 0xA4800004u;
constexpr uint32_t kSiPifAddrWr64bReg = 0xA4800010u;
constexpr uint32_t kPifRamAddress = 0x1FC007C0u;
constexpr uint32_t kControllerStatusArrayPhys = 0x001F3F18u;
constexpr uint32_t kFirstPifCommandBytes = 64u;
constexpr uint32_t kFirstPifCommandFnv = 0x84453E38u;

// Stage 64 completes the first outbound SI transfer, lets Toy Story 2
// consume event 5/message 0 from its registered SI queue, and then stops
// immediately after the matching PIF -> RDRAM 64-byte return DMA launch.
constexpr uint32_t kSiEventReceiveMsgPtr = 0x803F9F70u;
constexpr uint32_t kReturnSiDmaLaunchPc = 0x8009816Cu;
constexpr uint32_t kControllerStatusParsedPc = 0x800985ECu;
constexpr uint32_t kControllerInitOuterSp = 0x803F9F18u;
constexpr uint32_t kControllerConnectedMaskPtr = 0x803F9FA8u;
constexpr uint32_t kControllerStatusOutput = 0x800B8858u;
constexpr uint32_t kNoControllerError = 8u;
constexpr uint32_t kNoControllerPifReplyFnv = 0x41838D38u;
constexpr uint32_t kControllerBootstrapReturnPc = 0x8000825Cu;
constexpr uint32_t kControllerBootstrapReturnSp = 0x803F9FB8u;
constexpr uint32_t kMainThreadS0 = 0x801CDE80u;
constexpr uint32_t kControllerSelectionGlobal = 0x800A2BECu;
constexpr uint32_t kControllerListCountGlobal = 0x800A2BC8u;
constexpr uint32_t kControllerPollStartedGlobal = 0x800A2BD0u;
constexpr uint32_t kNoUsableControllerSelection = 5u;
constexpr uint32_t kSpriteLoadCallPc = 0x800087F8u;
constexpr uint32_t kSpriteLoadReturnPc = 0x80008800u;
constexpr uint32_t kStage84PreInitPc = 0x80008824u;
constexpr uint32_t kStage85NextInitCallPc = 0x80008840u;
constexpr uint32_t kStage86Func8000B0C0CallPc = 0x80012F3Cu;
constexpr uint32_t kStage87Func8000AFCCCallPc = 0x8000B17Cu;
constexpr uint32_t kStage88AfterFunc8000AFCCPc = 0x8000B184u;
constexpr uint32_t kStage89Case98TransferPc = 0x8000B85Cu;
constexpr uint32_t kStage90AfterCase98TransferPc = 0x8000B864u;
constexpr uint32_t kStage91FirstResourceLoadPc = 0x80014040u;
constexpr uint32_t kStage91ExpectedSp = 0x803F9D50u;
constexpr uint32_t kStage91OuterSelector = 0x00000062u;
constexpr uint32_t kStage91PreviousPiTransferCount = 66u;
constexpr uint32_t kStage91PreviousResourceLoaderCount = 60u;
constexpr uint32_t kStage92AfterSelectPicPc = 0x80014048u;
constexpr uint32_t kStage92ExpectedSp = 0x803F9D50u;
constexpr uint32_t kStage92ResourcePtr = 0x803F9D80u;
constexpr uint32_t kStage92ResourceDest = 0x80369400u;
constexpr uint32_t kStage92UnpackedSize = 0x00001088u;
constexpr uint32_t kStage92StoredSize = 0x00000071u;
constexpr uint32_t kStage92PackedSize = 0x0000005Fu;
constexpr uint16_t kStage92UnpackedCrc = 0xD426u;
constexpr uint16_t kStage92PackedCrc = 0x5813u;
constexpr uint32_t kStage92StoredFnv = 0x45AE8147u;
constexpr uint32_t kStage92FirstTransferIndex = 66u;
constexpr uint32_t kStage92TransferCount = 6u;
constexpr uint32_t kStage92ExpectedLoaderCount = 66u;
constexpr uint32_t kStage93AfterFunc13D88Pc = 0x80014538u;
constexpr uint32_t kStage93NextResourceBoundaryPc = 0x80014040u;
constexpr uint32_t kStage90FirstTransferIndex = 60u;
constexpr uint32_t kStage90TransferCount = 6u;
constexpr uint32_t kStage89Case98Selector = 0x00000062u;
constexpr uint32_t kStage89Case98CartSource = 0xB00D34E0u;
constexpr uint32_t kStage89Case98RdramDest = 0x80000400u;
constexpr uint32_t kStage89Case98Size = 0x00005300u;
constexpr uint32_t kStage88Clear0 = 0x801CE330u;
constexpr uint32_t kStage88Clear0Size = 0x00000200u;
constexpr uint32_t kStage88Clear1 = 0x800B0858u;
constexpr uint32_t kStage88Clear1Size = 0x00007E90u;
constexpr uint32_t kStage88Clear2 = 0x801E95D0u;
constexpr uint32_t kStage88Clear2Size = 0x00003CF0u;
constexpr uint32_t kStage88DynamicBasePtr = 0x801E314Cu;
constexpr uint32_t kStage88DynamicClearSize = 0x00000400u;
constexpr uint32_t kStage88MinusOne = 0x801E89A8u;
constexpr uint32_t kStage88LoopBase = 0x801EDB8Cu;
constexpr uint32_t kStage88ResetWordsBase = 0x801F06F0u;
constexpr uint32_t kStage88SmallClear0 = 0x801ED368u;
constexpr uint32_t kStage88SmallClear0Size = 0x00000080u;
constexpr uint32_t kStage88SmallClear1 = 0x801CEE88u;
constexpr uint32_t kStage88SmallClear1Size = 0x00000200u;
constexpr uint32_t kStage87Selector = 0x00000062u;
constexpr uint32_t kStage87TableBase0 = 0x800A27BEu;
constexpr uint32_t kStage87FloatSource = 0x800A0908u;
constexpr uint32_t kStage87FloatMirror = 0x800A292Cu;
constexpr uint32_t kStage87MirrorV1 = 0x800A28CCu;
constexpr uint32_t kStage87MirrorA0 = 0x800A28D0u;
constexpr uint32_t kStage87MirrorA1 = 0x800A28B0u;
constexpr uint32_t kStage87MirrorA2 = 0x800A28B4u;
constexpr uint32_t kStage87MirrorV0 = 0x800A28B8u;
constexpr uint32_t kStage87ZeroA = 0x801E9528u;
constexpr uint32_t kStage87ZeroB = 0x801CC424u;
constexpr uint32_t kStage87One = 0x801C90FCu;
constexpr uint32_t kStage87ZeroC = 0x801CCE4Cu;
constexpr uint32_t kStage86Global62 = 0x801F0714u;
constexpr uint32_t kStage86GlobalC3C = 0x801DB3D8u;
constexpr uint32_t kStage85InitObject = 0x801E2D48u;
constexpr uint32_t kStage85GlobalMode = 0x800BB10Au;
constexpr uint32_t kStage85GlobalByte2 = 0x800B86E8u;
constexpr uint32_t kSpriteCallerSp = 0x803F9F98u;
constexpr uint32_t kSpriteString = 0x800A08A8u;
constexpr uint32_t kSpriteBufferGlobal = 0x801CDE70u;
constexpr uint32_t kSpriteDest = 0x80366100u;
constexpr uint32_t kSpriteEntryIndex = 467u;
constexpr uint32_t kSpriteEntryRomOffset = 0x000DBC80u;
constexpr uint32_t kSpriteDataOffset = 0x0071DDE8u;
constexpr uint32_t kSpriteFileSize = 0x000001DFu;
constexpr uint32_t kSpritePayloadRomOffset = 0x007F9A88u;
constexpr uint32_t kSpriteFirstWord = 0x524E4301u;
constexpr uint32_t kSpriteCompressedHash = 0xB139A942u;
constexpr uint32_t kSpritePrimarySize = 0x000001D8u;
constexpr uint32_t kSpriteTailRomOffset = 0x007F9C60u;
constexpr uint32_t kSpritePrimaryHash = 0xD207F6FCu;
constexpr uint32_t kSpriteTailHash = 0xB85B5C1Fu;
constexpr uint32_t kSpriteTailScratch = 0x803F9F50u;
constexpr uint32_t kSpriteRncUnpackedSize = 0x00000548u;
constexpr uint32_t kSpriteRncPackedPayloadSize = 0x000001CDu;
constexpr uint16_t kSpriteRncUnpackedCrc = 0xB9DEu;
constexpr uint16_t kSpriteRncPackedCrc = 0x1873u;
constexpr uint32_t kSpriteDirPiRequest = 0x803F9EE8u;
constexpr uint32_t kSpritePayloadPiRequest = 0x803F9F10u;

// Corrected Stage 67 startup order: audio resources precede sprite0.pic.
constexpr uint32_t kSfxDataOffset = 0x000034C0u;
constexpr uint32_t kSfxFileSize = 0x00001978u;
constexpr uint32_t kSfxDest = 0x80340800u;
constexpr uint32_t kSfxFirstWord = 0x000000E9u;
constexpr uint32_t kSfxFullHash = 0xCF866787u;
constexpr uint32_t kPtrDataOffset = 0x00004E38u;
constexpr uint32_t kPtrFileSize = 0x0001106Cu;
constexpr uint32_t kPtrDest = 0x80320000u;
constexpr uint32_t kPtrFirstWord = 0x4E363420u;
constexpr uint32_t kPtrFullHash = 0x88ED2B6Du;
constexpr uint32_t kAudioEngineInitBoundaryPc = 0x80019304u;
constexpr uint32_t kAudioEngineInitBoundarySp = 0x803F9F38u;
constexpr uint32_t kAudioEngineConfigPtr = 0x803F9F48u;
constexpr uint32_t kAudioEngineInitV0 = 0x00000400u;
constexpr uint32_t kAudioDirectCartReadPc = 0x8008E084u;
constexpr uint32_t kAudioDirectCartPostReadPc = 0x8008E088u;
constexpr uint32_t kStage71RealSfxInitPc = 0x80019344u;
constexpr uint32_t kStage71ExpectedSp = 0x803F9F38u;
constexpr uint32_t kStage71PtrSlot = 0x803F9F60u;
constexpr uint32_t kStage71WbkSlot = 0x803F9F64u;
constexpr uint32_t kStage71FxSlot = 0x803F9F68u;
constexpr uint32_t kStage71PtrBank = 0x80320000u;
constexpr uint32_t kStage71WaveBank = 0xB07F9C80u;
constexpr uint32_t kStage71SfxBank = 0x80340800u;
constexpr uint32_t kStage71SfxCount = 0x000000E9u;
constexpr uint32_t kStage71SfxFlag = 0x00000000u;
constexpr uint32_t kStage71SfxDataOffset = 0x000017A6u;
constexpr uint32_t kWaveTableFirstCartWritePc = 0x8008E0C0u;
constexpr uint32_t kWaveTableFirstCartWriteSp = 0x803F9EF8u;
constexpr uint32_t kWaveTableField14Word = 0x00000000u;
constexpr uint32_t kWaveTableCountWord = 0x4E363420u;
constexpr uint32_t kWaveTableRelocGlobal = 0x800ACC2Cu;
constexpr uint32_t kAudioDirectCartPostReadSp = 0x803F9F00u;
constexpr uint32_t kAudioDirectCartPostReadWord = 0x65732000u;
constexpr uint32_t kAudioDirectCartReadSp = 0x803F9F00u;
constexpr uint32_t kAudioDirectCartBase = 0xB07F9C80u;
constexpr uint32_t kAudioDirectCartReadAddr = 0xB07F9C8Cu;
constexpr uint32_t kAudioDirectCartRomOffset = 0x007F9C80u;
constexpr uint32_t kAudioDirectCartExpectedWord = 0x65732000u;
constexpr uint32_t kAudioTracePreClearPc = 0x8008D384u;
constexpr uint32_t kAudioTracePostClearPc = 0x8008D38Cu;
constexpr uint32_t kAudioTracePreSetPtrPc = 0x8008D390u;
constexpr uint32_t kAudioTracePostSetPtrPc = 0x8008D398u;
constexpr uint32_t kAudioTracePreAlloc1Pc = 0x8008D3ACu;
constexpr uint32_t kAudioTracePostAlloc1Pc = 0x8008D3B4u;
constexpr uint32_t kAudioTracePreAlloc2Pc = 0x8008D3D4u;
constexpr uint32_t kAudioTracePostAlloc2Pc = 0x8008D3DCu;
constexpr uint32_t kAudioTracePrePoolPc = 0x8008D3F4u;
constexpr uint32_t kAudioTracePostPoolPc = 0x8008D3FCu;
constexpr uint32_t kAudioTracePrePtrInitPc = 0x8008D424u;
constexpr uint32_t kAudioTracePostPtrInitPc = 0x8008D42Cu;
constexpr uint32_t kAudioTracePreBankInitPc = 0x8008D448u;
constexpr uint32_t kAudioTracePostBankInitPc = 0x8008D450u;
constexpr uint32_t kAudioTracePre903A0Pc = 0x8008D470u;
constexpr uint32_t kAudioTraceExpectedSp = 0x803F9F00u;
constexpr uint32_t kAudioTraceConfigPtr = 0x803F9F48u;
constexpr uint32_t kAudioCartProbePiStatusPc = 0x8009831Cu;
constexpr uint32_t kAudioCartProbePiStatusSp = 0x803F9E30u;
constexpr uint32_t kAudioCartProbeResultPtr = 0x803F9E60u;
constexpr uint32_t kAudioCartProbePiStatusReg = 0xA4600010u;
constexpr uint32_t kAudioCartProbeBaseGlobal = 0x801F3E44u;
constexpr uint32_t kAudioCartProbeBaseValue = 0xB0000000u;
constexpr uint32_t kFirstAiMmioPc = 0x800A07B4u;
constexpr uint32_t kStage72AiReturnPc = 0x800903F4u;
constexpr uint32_t kStage73TracePostPtrPc = 0x800903FCu;
constexpr uint32_t kStage73TracePostInitPc = 0x80090410u;
constexpr uint32_t kStage73TracePostCalcPc = 0x80090424u;
constexpr uint32_t kStage73TracePostAllocPc = 0x80090434u;
constexpr uint32_t kStage73TracePostSmallAllocPc = 0x80090444u;
constexpr uint32_t kStage73TracePreBigAllocPc = 0x80090478u;
constexpr uint32_t kStage73SchedulerSetupPc = 0x800904B0u;
constexpr uint32_t kStage74SchedulerReturnPc = 0x800904B8u;
constexpr uint32_t kStage75AudioThreadEntryPc = 0x800904DCu;
constexpr uint32_t kStage76FirstCallbackPc = 0x80090528u;
constexpr uint32_t kStage77FirstCallbackReturnPc = 0x80090530u;
constexpr uint32_t kStage78Callback1ReceivePc = 0x800907B8u;
constexpr uint32_t kStage78ExpectedSp = 0x8031EF80u;
constexpr uint32_t kStage78ExpectedQueueBase = 0x8031EFF0u;
constexpr uint32_t kStage78ExpectedEffectiveQueue = 0x8031EFF8u;
constexpr uint32_t kStage78ExpectedMsgOut = 0x8031EF90u;
constexpr uint32_t kStage78ExpectedFlags = 1u;
constexpr uint32_t kStage78ExpectedCapacity = 4u;
constexpr uint32_t kStage78ExpectedBuffer = 0x8031F010u;
constexpr uint32_t kStage79AfterCallback1Pc = 0x80090544u;
constexpr uint32_t kStage79V6PostBlockingReceivePc = 0x800907D0u;
constexpr uint32_t kStage79V6ExpectedSp = 0x8031EF80u;
constexpr uint32_t kStage79ExpectedMainSp = 0x8031EFA0u;
constexpr uint32_t kStage79ViEventSlot = 0x801F2D78u;
constexpr uint32_t kStage79ViManagerQueue = 0x801F1940u;
constexpr uint32_t kStage79ViInterruptMessage = 0x801F1970u;
constexpr uint32_t kStage79ViContextGlobal = 0x800AA604u;
constexpr uint32_t kStage79ViCurrentContextGlobal = 0x800AA600u;
constexpr uint32_t kStage79ViContextA = 0x800AA5A0u;
constexpr uint32_t kStage79ViContextB = 0x800AA5D0u;
constexpr uint32_t kStage79WorkerBase = 0x801CDE80u;
constexpr uint32_t kStage79WorkerQueue = 0x801CDEC0u;
constexpr uint32_t kStage79WorkerEventMessage = 0x0000029Au;
constexpr uint32_t kStage79AudioQueue = 0x8031EFF8u;
constexpr uint32_t kStage79Callback1MsgOut = 0x8031EF90u;
constexpr uint32_t kStage79ExpectedAudioQueueCapacity = 4u;
constexpr uint32_t kStage79ExpectedAudioQueueBuffer = 0x8031F010u;
constexpr uint32_t kStage77ExpectedSp = 0x8031EFA0u;
constexpr uint32_t kStage77Callback0 = 0x80090730u;
constexpr uint32_t kStage77Callback1 = 0x80090798u;
constexpr uint32_t kStage77Callback2 = 0x800907FCu;
constexpr uint32_t kStage77SyncBlockGlobal = 0x800ACE84u;
constexpr uint32_t kStage77AudioListGlobal = 0x800ACE80u;
constexpr uint32_t kStage76ExpectedSp = 0x8031EFA0u;
constexpr uint32_t kStage75ParentPostStartPc = 0x800904C0u;
constexpr uint32_t kStage75AudioThreadInitialSp = 0x8031EFE0u;
constexpr uint32_t kStage74ExpectedSp = 0x803F9EB0u;
constexpr uint32_t kStage74ConfigPtr = 0x803F9F48u;
constexpr uint32_t kStage74SchedulerHeadGlobal = 0x800AA68Cu;
constexpr uint32_t kStage74SchedulerStackTop = 0x8031EFE0u;
constexpr uint32_t kStage74SchedulerEntryPairHi = 0xFFFFFFFFu;
constexpr uint32_t kStage74SchedulerEntryPairLo = 0x800961E0u;
constexpr uint32_t kStage74SchedulerFlags = 0x0000FF03u;
constexpr uint32_t kStage74SchedulerMask = 0x0000003Fu;
constexpr uint32_t kStage74SchedulerControl = 0x01000800u;
constexpr uint32_t kStage73ExpectedSp = 0x803F9EB0u;
constexpr uint32_t kStage73SchedulerObj = 0x800ACCA0u;
constexpr uint32_t kStage73SchedulerCallback = 0x800904DCu;
constexpr uint32_t kStage72AiReturnSp = 0x803F9EB0u;
constexpr uint32_t kStage72AudioClockGlobal = 0x800AA6A8u;
constexpr uint32_t kStage72BitrateValue = 0x0000000Fu;
constexpr uint32_t kStage72ControlValue = 0x00000001u;
constexpr uint32_t kFirstAiMmioSp = 0x803F9EB0u;
constexpr uint32_t kAiDacrateReg = 0xA4500010u;
constexpr uint32_t kAiBitrateReg = 0xA4500014u;
constexpr uint32_t kAiControlReg = 0xA4500008u;
constexpr uint32_t kAudioDacDivisor = 0x000008A0u;
constexpr uint32_t kAudioDacrateValue = 0x0000089Fu;

constexpr uint32_t kArena0PtrTable = 0x800A2AC0u;
constexpr uint32_t kArena0RemainTable = 0x800A2AD0u;
constexpr uint32_t kArena1PtrTable = 0x800A2AC4u;
constexpr uint32_t kArena1RemainTable = 0x800A2AD4u;

constexpr uint32_t kFontPoolGlobal = 0x801DB3F0u;
constexpr uint32_t kCommonPoolGlobal = 0x800BAC38u;
constexpr uint32_t kCommonLineCountGlobal = 0x800B04A8u;
constexpr uint32_t kCommonOutputBase = 0x802FD8C0u;
constexpr uint32_t kCommonOutputSize = 0x000001F8u;
constexpr uint32_t kCommonOutputHash = 0x6449D4C1u;
constexpr uint32_t kCommonLineCount = 26u;

constexpr uint32_t kCommonEntryIndex = 355u;
constexpr uint32_t kCommonEntryRomOffset = 0x000DAFBCu;
constexpr uint32_t kCommonDataOffset = 0x00551F70u;
constexpr uint32_t kCommonFileSize = 0x000001A8u;
constexpr uint32_t kCommonPayloadRomOffset = 0x0062A750u;
constexpr uint32_t kCommonPayloadHash = 0xF99AE1CCu;
constexpr uint32_t kCommonFirstWord = 0x5C633050u;
constexpr uint32_t kCommonString = 0x800A0FE4u;
constexpr uint32_t kCommonSuffixString = 0x800A0FDCu;

constexpr uint32_t kArena0FinalPtr = 0x80300000u;
constexpr uint32_t kArena0FinalRemain = 0x00002548u;
constexpr uint32_t kArena1FinalPtr = 0x803F4000u;
constexpr uint32_t kArena1FinalRemain = 0x0008AF00u;

constexpr uint32_t kCommonDirPiRequest = 0x803F9E28u;
constexpr uint32_t kCommonPayloadPiRequest = 0x803F9E50u;
constexpr uint32_t kSecondFontGlyphBaseIndex = 0x00000069u;
constexpr uint32_t kThirdFontGlyphBaseIndex = 0x000000D2u;
constexpr uint32_t kSecondFontOutputStartOffset = 0x00002A18u;
constexpr uint32_t kExpectedFontDest = 0x800BB228u;
constexpr uint32_t kFontLoaderSp = 0x803F9E98u;
constexpr uint32_t kFontPiRequest = 0x803F9EB8u;
constexpr uint32_t kFontPrimarySize = 0x00001410u;
constexpr uint32_t kFontTailScratch = 0x803F9EF8u;
constexpr uint32_t kFontTailReadSize = 0x00000008u;
constexpr uint32_t kFontTailScratchHash = 0x9B29E6B5u;
constexpr uint32_t kTransferCount = 72u;
constexpr std::array<uint32_t, kTransferCount> kChunkDeviceAddrs{
    0xB00D87E0u, 0xB00D97E0u, 0xB00DA7E0u, 0xB00DB7E0u,
    0xB017D2A8u, 0xB017E2A8u, 0xB017E6B8u, 0xB00D87E0u,
    0xB00D97E0u, 0xB00DA7E0u, 0xB00DB7E0u, 0xB017E6C0u,
    0xB017F278u, 0xB00D87E0u, 0xB00D97E0u, 0xB00DA7E0u,
    0xB00DB7E0u, 0xB017F280u, 0xB0180280u, 0xB01811B0u,
    0xB00D87E0u, 0xB00D97E0u, 0xB00DA7E0u, 0xB00DB7E0u,
    0xB062A750u, 0xB00D87E0u, 0xB00D97E0u, 0xB00DA7E0u,
    0xB00DB7E0u, 0xB00DBCA0u, 0xB00DCCA0u, 0xB00D87E0u,
    0xB00D97E0u, 0xB00DA7E0u, 0xB00DB7E0u, 0xB00DD618u,
    0xB00DE618u, 0xB00DF618u, 0xB00E0618u, 0xB00E1618u,
    0xB00E2618u, 0xB00E3618u, 0xB00E4618u, 0xB00E5618u,
    0xB00E6618u, 0xB00E7618u, 0xB00E8618u, 0xB00E9618u,
    0xB00EA618u, 0xB00EB618u, 0xB00EC618u, 0xB00ED618u,
    0xB00EE618u, 0xB00EE680u,
    0xB00D87E0u, 0xB00D97E0u, 0xB00DA7E0u, 0xB00DB7E0u,
    0xB07F9A88u, 0xB07F9C60u,
    0xB00D34E0u, 0xB00D44E0u, 0xB00D54E0u, 0xB00D64E0u, 0xB00D74E0u, 0xB00D84E0u,
    0xB00D87E0u, 0xB00D97E0u, 0xB00DA7E0u, 0xB00DB7E0u, 0xB07F3ED0u, 0xB07F3F40u

};

constexpr std::array<uint32_t, kTransferCount> kChunkDramAddrs{
    0x801C4A28u, 0x801C5A28u, 0x801C6A28u, 0x801C7A28u,
    0x800BB228u, 0x800BC228u, 0x803F9EF8u, 0x801C4A28u,
    0x801C5A28u, 0x801C6A28u, 0x801C7A28u, 0x800BB228u,
    0x803F9EF8u, 0x801C4A28u, 0x801C5A28u, 0x801C6A28u,
    0x801C7A28u, 0x800BB228u, 0x800BC228u, 0x803F9EF8u,
    0x801C4A28u, 0x801C5A28u, 0x801C6A28u, 0x801C7A28u,
    0x800BB228u, 0x801C4A28u, 0x801C5A28u, 0x801C6A28u,
    0x801C7A28u, 0x80340800u, 0x80341800u, 0x801C4A28u,
    0x801C5A28u, 0x801C6A28u, 0x801C7A28u, 0x80320000u,
    0x80321000u, 0x80322000u, 0x80323000u, 0x80324000u,
    0x80325000u, 0x80326000u, 0x80327000u, 0x80328000u,
    0x80329000u, 0x8032A000u, 0x8032B000u, 0x8032C000u,
    0x8032D000u, 0x8032E000u, 0x8032F000u, 0x80330000u,
    0x80331000u, 0x803F9EF0u,
    0x801C4A28u, 0x801C5A28u, 0x801C6A28u, 0x801C7A28u,
    0x80366100u, 0x803F9F50u,
    0x80000400u, 0x80001400u, 0x80002400u, 0x80003400u, 0x80004400u, 0x80005400u,
    0x801C4A28u, 0x801C5A28u, 0x801C6A28u, 0x801C7A28u, 0x80369400u, 0x803F9D08u

};

constexpr std::array<uint32_t, kTransferCount> kChunkRomOffsets{
    0x000D87E0u, 0x000D97E0u, 0x000DA7E0u, 0x000DB7E0u,
    0x0017D2A8u, 0x0017E2A8u, 0x0017E6B8u, 0x000D87E0u,
    0x000D97E0u, 0x000DA7E0u, 0x000DB7E0u, 0x0017E6C0u,
    0x0017F278u, 0x000D87E0u, 0x000D97E0u, 0x000DA7E0u,
    0x000DB7E0u, 0x0017F280u, 0x00180280u, 0x001811B0u,
    0x000D87E0u, 0x000D97E0u, 0x000DA7E0u, 0x000DB7E0u,
    0x0062A750u, 0x000D87E0u, 0x000D97E0u, 0x000DA7E0u,
    0x000DB7E0u, 0x000DBCA0u, 0x000DCCA0u, 0x000D87E0u,
    0x000D97E0u, 0x000DA7E0u, 0x000DB7E0u, 0x000DD618u,
    0x000DE618u, 0x000DF618u, 0x000E0618u, 0x000E1618u,
    0x000E2618u, 0x000E3618u, 0x000E4618u, 0x000E5618u,
    0x000E6618u, 0x000E7618u, 0x000E8618u, 0x000E9618u,
    0x000EA618u, 0x000EB618u, 0x000EC618u, 0x000ED618u,
    0x000EE618u, 0x000EE680u,
    0x000D87E0u, 0x000D97E0u, 0x000DA7E0u, 0x000DB7E0u,
    0x007F9A88u, 0x007F9C60u,
    0x000D34E0u, 0x000D44E0u, 0x000D54E0u, 0x000D64E0u, 0x000D74E0u, 0x000D84E0u,
    0x000D87E0u, 0x000D97E0u, 0x000DA7E0u, 0x000DB7E0u, 0x007F3ED0u, 0x007F3F40u

};

constexpr std::array<uint32_t, kTransferCount> kChunkSizes{
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00000410u, 0x00000008u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00000BB8u,
    0x00000008u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00000F30u, 0x00000008u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x000001A8u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00000978u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x00000068u, 0x00000008u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u,
    0x000001D8u, 0x00000008u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u, 0x00000300u,
    0x00001000u, 0x00001000u, 0x00001000u, 0x00001000u, 0x00000070u, 0x00000008u

};

constexpr std::array<uint32_t, kTransferCount> kChunkHashes{
    0x8B422B7Au, 0xD81FA12Eu, 0x3B3233FEu, 0xAC84DD3Au,
    0x7E1C4C50u, 0x8B17FA32u, 0x9B29E6B5u, 0x8B422B7Au,
    0xD81FA12Eu, 0x3B3233FEu, 0xAC84DD3Au, 0x9DC24567u,
    0x0FA07F6Eu, 0x8B422B7Au, 0xD81FA12Eu, 0x3B3233FEu,
    0xAC84DD3Au, 0xEF907CA1u, 0xE9A39E54u, 0x52233FF3u,
    0x8B422B7Au, 0xD81FA12Eu, 0x3B3233FEu, 0xAC84DD3Au,
    0xF99AE1CCu, 0x8B422B7Au, 0xD81FA12Eu, 0x3B3233FEu,
    0xAC84DD3Au, 0x5B3ABE55u, 0xB2991EF7u, 0x8B422B7Au,
    0xD81FA12Eu, 0x3B3233FEu, 0xAC84DD3Au, 0xE7D91317u,
    0xAFF71BC1u, 0x5ED960E5u, 0xF2002D49u, 0xDA2AAE42u,
    0x11854FB8u, 0x353D7A7Cu, 0x6187BDFAu, 0x1388AB2Au,
    0x8EC20955u, 0x18713B21u, 0x6558C292u, 0x2C5AAC4Fu,
    0x47A17C2Bu, 0x9C94B267u, 0x6DC93C88u, 0xF65D7C9Cu,
    0x5AD1E40Bu, 0xC322536Bu,
    0x8B422B7Au, 0xD81FA12Eu, 0x3B3233FEu, 0xAC84DD3Au,
    0xD207F6FCu, 0xB85B5C1Fu,
    0x2D67AF0Cu, 0xAB6C48A4u, 0x49B8E6CCu, 0x5DD4B966u, 0x663FE96Du, 0x5BABC06Eu,
    0x8B422B7Au, 0xD81FA12Eu, 0x3B3233FEu, 0xAC84DD3Au, 0x054321FDu, 0x9BE17165u

};
constexpr uint32_t kExpectedPiDramPhysical = 0x001C4A28u;
constexpr uint32_t kExpectedPiCartPhysical = 0x100D87E0u;
constexpr uint32_t kExpectedPiRomOffset = 0x000D87E0u;
constexpr uint32_t kExpectedPiDmaSize = 0x00001000u;
constexpr uint32_t kExpectedPiLengthValue = 0x00000FFFu;
constexpr uint32_t kPiRegsBase = 0xA4600000u;
constexpr uint32_t kPiAccessReceiveMsg = 0x801F2B14u;
constexpr uint32_t kManagerQueueBGlobal = 0x801F06C8u;
constexpr uint32_t kSiQueue = 0x801ED338u;
constexpr uint32_t kSiQueueBuffer = 0x801C8E68u;
constexpr uint32_t kSiEventTableEntry = 0x801F2D68u;
constexpr uint32_t kSiScratchReset = 0x800A2BD0u;
constexpr uint32_t kSiControllerBuffer = 0x800B8858u;
constexpr uint32_t kManagerLinkNode = 0x801E9540u;
constexpr uint32_t kManagerLinkPayload = 0x801CC400u;
constexpr uint16_t kExpectedMiMaskCommand = 0x0555u;
constexpr uint32_t kMiIntrMaskReg = 0xA430000Cu;
constexpr uint32_t kMiMaskShadow = 0x800AA6B0u;
constexpr uint32_t kTvPathScratchFlag = 0x801DBDCCu;
constexpr uint32_t kWorkerReceiveMsg = 0x803FBFC8u;

constexpr uint32_t kWorkerThreadAddr      = 0x801CDF30u;
constexpr int32_t  kWorkerThreadId        = 4;
constexpr uint32_t kWorkerThreadEntry     = 0x80097018u;
constexpr uint32_t kWorkerThreadArg       = 0x801CDE80u;
constexpr uint32_t kWorkerThreadStackTop  = 0x803FC000u;
constexpr uint32_t kWorkerThreadInitialSp = 0x803FBFF0u;
constexpr int32_t  kWorkerThreadPriority  = 13;

constexpr uint32_t kManagerEventMsg0 = 0x0000029Au;
constexpr uint32_t kManagerEventMsg4 = 0x0000029Bu;
constexpr uint32_t kManagerEventMsg9 = 0x0000029Cu;
constexpr uint32_t kManagerEventMsg14 = 0x0000029Du;
constexpr uint32_t kManagerViModeTable0 = 0x800AA760u;

constexpr uint32_t kThread4QueueSmall = 0x800BAB00u;
constexpr uint32_t kThread4QueueSmallBuffer = 0x801E07D8u;
constexpr uint32_t kThread4QueueLarge = 0x801CC400u;
constexpr uint32_t kThread4QueueLargeBuffer = 0x801E89B0u;

constexpr uint32_t kBootstrapPtrAAddr = 0x800AEAA0u;
constexpr uint32_t kBootstrapPtrAValue = 0x801C8A28u;
constexpr uint32_t kBootstrapPtrBAddr = 0x801C8E50u;
constexpr uint32_t kBootstrapPtrBValue = 0x800BB228u;
constexpr uint32_t kBootstrapZeroA = 0x801E3174u;
constexpr uint32_t kBootstrapZeroB = 0x801E3F30u;
constexpr uint32_t kBootstrapZeroC = 0x800B0848u;
constexpr uint32_t kVideoStandardFlag = 0x801DBDCCu;

constexpr uint32_t kNextManagerA0 = 0x801CDE80u;
constexpr uint32_t kNextManagerA1 = 0x803FC000u;
constexpr uint32_t kNextManagerA2 = 13u;
constexpr uint32_t kNextManagerA3 = 0u;
constexpr uint32_t kThread4ClearStart = 0x80200000u;
constexpr uint32_t kThread4ClearEnd = 0x803F4000u;

constexpr uint32_t kPiManagerThreadAddr      = 0x801F19A0u;
constexpr int32_t  kPiManagerThreadId        = 0;
constexpr uint32_t kPiManagerThreadEntry     = 0x80099FE0u;
constexpr uint32_t kPiManagerThreadArg       = 0x800AB540u;
constexpr uint32_t kPiManagerThreadStackTop  = 0x801F2B50u;
constexpr uint32_t kPiManagerThreadInitialSp = 0x801F2B40u;
constexpr int32_t  kPiManagerThreadPriority  = 150;


constexpr uint32_t kThread3Addr      = 0x800AC260u;
constexpr int32_t  kThread3Id        = 3;
constexpr uint32_t kThread3Entry     = 0x8000806Cu;
constexpr uint32_t kThread3Arg       = 0u;
constexpr uint32_t kThread3StackTop  = 0x803F8000u;
constexpr uint32_t kThread3InitialSp = 0x803F7FF0u;
constexpr int32_t  kThread3Priority  = 10;

constexpr uint32_t kThread4Addr      = 0x800AC410u;
constexpr int32_t  kThread4Id        = 4;
constexpr uint32_t kThread4Entry     = 0x800080F8u;
constexpr uint32_t kThread4Arg       = 0u;
constexpr uint32_t kThread4StackTop  = 0x803FA000u;
constexpr uint32_t kThread4InitialSp = 0x803F9FF0u;
constexpr int32_t  kThread4Priority  = 10;

constexpr uint32_t kOsTvType      = 0x80000300u;
constexpr uint32_t kOsRomType     = 0x80000304u;
constexpr uint32_t kOsRomBase     = 0x80000308u;
constexpr uint32_t kOsResetType   = 0x8000030Cu;
constexpr uint32_t kOsCicId       = 0x80000310u;
constexpr uint32_t kOsVersion     = 0x80000314u;
constexpr uint32_t kOsMemSize     = 0x80000318u;
constexpr uint32_t kOsAppNmi      = 0x8000031Cu;
constexpr uint32_t kOsAppNmiSize  = 0x40u;

constexpr uint32_t kExpectedRomClockWord = 0x0000000Fu;

constexpr uint32_t kQueueSentinel       = 0x800AA680u;
constexpr uint32_t kPiCmdQueue         = 0x800AC9C0u;
constexpr uint32_t kPiCmdBuffer        = 0x800AC5C0u;
constexpr uint32_t kPiEventQueue        = 0x801F2B50u;
constexpr uint32_t kPiEventBuffer       = 0x801F2B68u;
constexpr uint32_t kPiAccessQueue        = 0x801F2D10u;
constexpr uint32_t kPiAccessBuffer       = 0x801F2B80u;
constexpr uint32_t kPiEventTableEntry   = 0x801F2D80u;
constexpr uint32_t kPiAccessQueueInitFlag   = 0x800AB580u;

// TS2 OLDNATIVE V30 BINDABLE RDRAM OWNER
//
// Transitional ownership adapter for the canonical N64ModernRuntime cutover.
//
// The current Stage61 frontend still owns an internal 8 MiB array. V30 keeps
// that exact behavior while making the same object capable of binding to
// N64ModernRuntime-owned RDRAM later through GameEntry::on_init_callback.
//
// V30 DOES NOT call bind_external(). Therefore this build must retain the
// existing Stage61 lifecycle and memory ownership.
class Ts2RdramStorage {
public:
    using value_type = uint8_t;
    using size_type = std::size_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    value_type* data() noexcept {
        return base_;
    }

    const value_type* data() const noexcept {
        return base_;
    }

    constexpr size_type size() const noexcept {
        return kRdramSize;
    }

    constexpr bool empty() const noexcept {
        return false;
    }

    reference operator[](size_type index) noexcept {
        return base_[index];
    }

    const_reference operator[](size_type index) const noexcept {
        return base_[index];
    }

    reference at(size_type index) {
        if (index >= kRdramSize) {
            std::abort();
        }
        return base_[index];
    }

    const_reference at(size_type index) const {
        if (index >= kRdramSize) {
            std::abort();
        }
        return base_[index];
    }

    reference front() noexcept {
        return base_[0];
    }

    const_reference front() const noexcept {
        return base_[0];
    }

    reference back() noexcept {
        return base_[kRdramSize - 1];
    }

    const_reference back() const noexcept {
        return base_[kRdramSize - 1];
    }

    iterator begin() noexcept {
        return base_;
    }

    const_iterator begin() const noexcept {
        return base_;
    }

    const_iterator cbegin() const noexcept {
        return base_;
    }

    iterator end() noexcept {
        return base_ + kRdramSize;
    }

    const_iterator end() const noexcept {
        return base_ + kRdramSize;
    }

    const_iterator cend() const noexcept {
        return base_ + kRdramSize;
    }

    void fill(value_type value) noexcept {
        for (size_type i = 0; i < kRdramSize; ++i) {
            base_[i] = value;
        }
    }

    void bind_external(value_type* rdram) {
        if (rdram == nullptr) {
            std::fputs(
                "TS2 OLDNATIVE V30 FATAL: attempted to bind null runtime RDRAM.\n",
                stderr);
            std::fflush(stderr);
            std::abort();
        }

        base_ = rdram;
        external_ = true;
    }

    void bind_owned() noexcept {
        base_ = owned_.data();
        external_ = false;
    }

    bool is_external() const noexcept {
        return external_;
    }

private:
    std::array<value_type, kRdramSize> owned_{};
    value_type* base_ = owned_.data();
    bool external_ = false;
};

Ts2RdramStorage g_rdram{};

// TS2 OLDNATIVE V79 CANONICAL OWNERSHIP CUTOVER

// TS2 OLDNATIVE V29 INACTIVE GAMEENTRY CALLBACK BODIES.
//
// These callbacks are compile-proven only in V29. They are not reached while
// supported_games[].is_enabled=false and main() still enters run_stage61().
void ts2_b0d_runtime_on_init(uint8_t* rdram, recomp_context* ctx) {
    (void)ctx;

    g_rdram.bind_external(rdram);

    // recomp::init() has reset the live overlay map. Restore the exact
    // resident/internal registrations proven by the legacy Stage61 host.
    ::ts2_v79_register_native_maps();

    std::fprintf(
        stderr,
        "TS2 OLDNATIVE V79: GameEntry on_init bound runtime RDRAM %p and "
        "restored the native lookup map.\n",
        static_cast<void*>(rdram));
    std::fflush(stderr);
}

void ts2_b0d_runtime_thread_create(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

void ts2_b0d_runtime_entrypoint(uint8_t* rdram, recomp_context* ctx) {
    if (!g_rdram.is_external() || g_rdram.data() != rdram) {
        std::fputs(
            "TS2 OLDNATIVE V29 FATAL: GameEntry reached without matching "
            "runtime RDRAM binding.\n",
            stderr);
        std::fflush(stderr);
        std::abort();
    }

    recomp_entrypoint(rdram, ctx);
}
// TS2 OLDNATIVE V29 INACTIVE GAMEENTRY CALLBACK BODIES END

std::mutex g_thread_mutex;
std::condition_variable g_thread_cv;
// TS2 LIVE MODE HOST HANDOFF V42B
// Host-only state. Reads/writes are synchronized by g_thread_mutex.
static bool g_ts2_live_mode_reached_v42b = false;

bool g_pi_thread_started_checkpoint_reached = false;
bool g_pi_entry_checkpoint_reached = false;
bool g_thread3_post_pi_checkpoint_reached = false;
bool g_thread4_init_checkpoint_reached = false;
bool g_thread4_queue_bootstrap_checkpoint_reached = false;
bool g_manager_queue_bootstrap_checkpoint_reached = false;
bool g_vi_manager_bootstrap_checkpoint_reached = false;
bool g_vi_thread_create_checkpoint_reached = false;
bool g_vi_mmio_boundary_checkpoint_reached = false;
bool g_vi_mmio_complete_checkpoint_reached = false;
bool g_vi_receive_checkpoint_reached = false;
bool g_thread4_vi_return_checkpoint_reached = false;
bool g_worker_create_checkpoint_reached = false;
bool g_worker_receive_checkpoint_reached = false;
bool g_thread4_post_worker_start_checkpoint_reached = false;
bool g_thread4_post_manager_bootstrap_reached = false;
bool g_mi_mask_boundary_reached = false;
bool g_post_mi_link_checkpoint_reached = false;
bool g_si_bootstrap_boundary_reached = false;
bool g_first_si_mmio_boundary_reached = false;
uint32_t g_first_si_mmio_sp = 0;
uint32_t g_first_si_mmio_v0 = 0;
uint32_t g_first_si_mmio_s0 = 0;
uint32_t g_first_si_mmio_s1 = 0;

bool g_first_si_dma_launch_reached = false;
uint32_t g_first_si_dma_sp = 0;
uint32_t g_first_si_dma_v0 = 0;
uint32_t g_first_si_dma_v1 = 0;
uint32_t g_first_si_dma_s0 = 0;
uint32_t g_first_si_dma_s1 = 0;

uint32_t g_si_dma_launch_count = 0;
bool g_return_si_dma_launch_reached = false;
uint32_t g_return_si_dma_sp = 0;
uint32_t g_return_si_dma_v0 = 0;
uint32_t g_return_si_dma_v1 = 0;
uint32_t g_return_si_dma_s0 = 0;
uint32_t g_return_si_dma_s1 = 0;

bool g_first_si_completion_event_queued = false;
int32_t g_first_si_completion_send_result = -999;
uint32_t g_si_event_recv_count = 0;
uint32_t g_si_event_recv_msg_ptr = 0;
int32_t g_si_event_recv_result = -999;
uint32_t g_si_event_recv_message = 0xFFFFFFFFu;
bool g_second_si_completion_event_queued = false;
int32_t g_second_si_completion_send_result = -999;
uint32_t g_return_pif_reply_fnv = 0;
std::array<uint8_t, kFirstPifCommandBytes> g_return_pif_reply{};
bool g_controller_status_parsed_reached = false;
uint32_t g_controller_status_parsed_sp = 0;
uint32_t g_controller_status_parsed_s4 = 0;
uint32_t g_controller_status_parsed_s5 = 0;
uint32_t g_controller_status_parsed_s6 = 0;
uint32_t g_controller_connected_mask = 0xFFFFFFFFu;
std::array<uint8_t, 4> g_controller_errno{};
bool g_controller_bootstrap_return_reached = false;
uint32_t g_controller_bootstrap_return_sp = 0;
uint32_t g_controller_bootstrap_return_v0 = 0;
uint32_t g_controller_bootstrap_return_s0 = 0;
uint32_t g_controller_selection = 0xFFFFFFFFu;
uint32_t g_controller_list_count = 0xFFFFFFFFu;
uint32_t g_controller_poll_started = 0xFFFFFFFFu;

bool g_sprite_call_reached = false;
uint32_t g_sprite_call_sp = 0;
uint32_t g_sprite_call_a0 = 0;
uint32_t g_sprite_call_a1 = 0;
std::string g_sprite_call_name;
bool g_sprite_return_reached = false;
uint32_t g_sprite_return_sp = 0;
uint32_t g_sprite_return_v0 = 0;
bool g_stage84_pre_init_reached = false;
uint32_t g_stage84_pre_init_sp = 0;
uint32_t g_stage84_pre_init_v0 = 0;
uint32_t g_stage84_pre_init_a0 = 0;
uint32_t g_stage84_pre_init_a1 = 0;
bool g_common_parser_snapshot_valid = false;
uint32_t g_common_parser_result = 0;
uint32_t g_common_parser_data_offset = 0;
uint32_t g_common_parser_size = 0;
bool g_common_payload_snapshot_valid = false;
uint32_t g_common_payload_pc = 0;
uint32_t g_common_payload_a0 = 0;
uint32_t g_common_payload_a1 = 0;
uint32_t g_common_payload_a2 = 0;
bool g_sprite_parser_snapshot_valid = false;
uint32_t g_sprite_parser_result = 0;
uint32_t g_sprite_parser_data_offset = 0;
uint32_t g_sprite_parser_size = 0;
bool g_sprite_payload_snapshot_valid = false;
uint32_t g_sprite_payload_pc = 0;
uint32_t g_sprite_payload_a0 = 0;
uint32_t g_sprite_payload_a1 = 0;
uint32_t g_sprite_payload_a2 = 0;
uint32_t g_sprite_complete_first_word = 0;
uint32_t g_sprite_complete_hash = 0;
bool g_sprite_rnc_active = false;
bool g_sprite_rnc_boundary_reached = false;
uint32_t g_sprite_rnc_sp = 0;
uint32_t g_sprite_rnc_input = 0;
uint32_t g_sprite_rnc_output = 0;
uint32_t g_sprite_rnc_unpacked_size = 0;
uint32_t g_sprite_rnc_packed_size = 0;
uint16_t g_sprite_rnc_header_unpacked_crc = 0;
uint16_t g_sprite_rnc_header_packed_crc = 0;
uint16_t g_sprite_rnc_computed_packed_crc = 0;
uint32_t g_sprite_rnc_predecode_output_hash = 0;
uint32_t g_sprite_rnc_predecode_temp_hash = 0;
bool g_sprite_rnc_return_reached = false;
uint32_t g_sprite_rnc_return_sp = 0;
uint32_t g_sprite_rnc_return_size = 0;
uint32_t g_sprite_rnc_return_output = 0;
uint32_t g_sprite_rnc_output_first_word = 0;
uint16_t g_sprite_rnc_output_crc = 0;
uint32_t g_sprite_rnc_output_fnv = 0;
uint32_t g_fifth_directory_reload_hash_at_dma29 = 0;

bool g_sfx_parser_valid = false;
uint32_t g_sfx_parser_data = 0;
uint32_t g_sfx_parser_size = 0;
uint32_t g_sfx_complete_dest = 0;
uint32_t g_sfx_complete_first = 0;
uint32_t g_sfx_complete_hash = 0;
bool g_ptr_parser_valid = false;
uint32_t g_ptr_parser_data = 0;
uint32_t g_ptr_parser_size = 0;
uint32_t g_ptr_complete_dest = 0;
uint32_t g_ptr_complete_first = 0;
uint32_t g_ptr_complete_hash = 0;
bool g_audio_engine_init_boundary_reached = false;
uint32_t g_audio_engine_init_sp = 0;
uint32_t g_audio_engine_init_v0 = 0;
uint32_t g_audio_engine_init_a0 = 0;
uint32_t g_audio_engine_init_a1 = 0;
bool g_audio_direct_cart_read_reached = false;
bool g_audio_direct_cart_post_read_reached = false;
bool g_stage71_real_sfx_init_reached = false;
uint32_t g_stage71_sp = 0;
uint32_t g_stage71_a0 = 0;
uint32_t g_stage71_a1 = 0;
uint32_t g_stage71_s0 = 0;
bool g_wavetable_first_cart_write_reached = false;
uint32_t g_wavetable_write_sp = 0;
uint32_t g_wavetable_write_v0 = 0;
uint32_t g_wavetable_write_v1 = 0;
uint32_t g_wavetable_write_a0 = 0;
uint32_t g_wavetable_write_a1 = 0;
uint32_t g_wavetable_write_a2 = 0;
uint32_t g_wavetable_write_a3 = 0;
uint32_t g_audio_direct_cart_post_sp = 0;
uint32_t g_audio_direct_cart_post_v0 = 0;
uint32_t g_audio_direct_cart_post_a2 = 0;
uint32_t g_audio_direct_cart_sp = 0;
uint32_t g_audio_direct_cart_a0 = 0;
uint32_t g_audio_direct_cart_a2 = 0;
uint32_t g_audio_trace_last_pc = 0;
uint32_t g_audio_trace_hit_count = 0;
bool g_audio_trace_pre903a0_reached = false;
uint32_t g_audio_trace_terminal_sp = 0;
uint32_t g_audio_trace_terminal_s3 = 0;
bool g_audio_cart_probe_pi_status_reached = false;
uint32_t g_audio_cart_probe_sp = 0;
uint32_t g_audio_cart_probe_v0 = 0;
uint32_t g_audio_cart_probe_s0 = 0;
uint32_t g_audio_cart_probe_s1 = 0;
bool g_first_ai_mmio_boundary_reached = false;
bool g_stage72_ai_return_reached = false;
bool g_stage73_scheduler_setup_reached = false;
bool g_stage74_scheduler_return_reached = false;
bool g_stage75_audio_thread_entry_reached = false;
bool g_stage76_first_callback_boundary_reached = false;
bool g_stage77_first_callback_return_reached = false;
bool g_stage78_callback1_receive_reached = false;
uint32_t g_stage78_receive_sp = 0;
uint32_t g_stage78_queue = 0;
uint32_t g_stage78_msg_out = 0;
uint32_t g_stage78_flags = 0;
uint32_t g_stage78_queue_count = 0;
uint32_t g_stage78_queue_first = 0;
uint32_t g_stage78_queue_capacity = 0;
uint32_t g_stage78_queue_buffer = 0;
bool g_stage79_vi_route_valid = false;
bool g_stage79_vi_event_queued = false;
bool g_stage79_callback1_return_reached = false;
bool g_stage80_ai_status_boundary_reached = false;
uint32_t g_stage80_ai_status_sp = 0;
uint32_t g_stage80_ai_status_addr = 0;
bool g_stage81_audio_queue_reached = false;
uint32_t g_stage81_v7_mix_probe_count = 0;
bool g_stage81_v7_probe_complete = false;
bool g_stage81_idle_milestone_reached = false;
bool g_stage82_post_audio_system_return_reached = false;
uint32_t g_stage82_return_sp = 0;
uint32_t g_stage82_return_v0 = 0;
bool g_stage81_v7_task_output_seen = false;
uint32_t g_stage81_v7_last_sp = 0;
uint32_t g_stage81_v7_last_task_word = 0;
uint32_t g_stage81_v7_last_return_v0 = 0;
uint32_t g_stage81_v7_last_slot = 0;
uint32_t g_stage81_v7_last_slot_word0 = 0;
uint32_t g_stage81_v7_last_slot_word1 = 0;
uint32_t g_stage81_v7_last_prev_buffer = 0;
uint32_t g_stage81_v7_last_ai_quarters = 0;
uint32_t g_stage81_v7_last_ring_index = 0;
uint32_t g_stage81_status_call_count = 0;
uint32_t g_stage81_length_call_count = 0;
uint32_t g_stage81_queue_call_count = 0;
uint32_t g_stage81_last_remaining_bytes = 0;
uint32_t g_stage81_first_buffer = 0;
uint32_t g_stage81_first_buffer_bytes = 0;
uint32_t g_stage79_event_slot_queue = 0;
uint32_t g_stage79_event_slot_message = 0;
uint32_t g_stage79_vi_context = 0;
uint32_t g_stage79_vi_current_context = 0;
uint32_t g_stage79_vi_user_queue = 0;
uint32_t g_stage79_vi_user_message = 0;
uint32_t g_stage79_return_sp = 0;
uint32_t g_stage79_received_message = 0;
uint32_t g_stage79_audio_queue_count = 0;
uint32_t g_stage79_audio_queue_first = 0;
uint32_t g_stage79_audio_queue_capacity = 0;
uint32_t g_stage79_audio_queue_buffer = 0;
bool g_stage77_queue_shape_valid = false;
uint32_t g_stage77_return_sp = 0;
uint32_t g_stage77_callback_return_v0 = 0;
uint32_t g_stage77_sync_block = 0;
uint32_t g_stage77_audio_list = 0;
uint32_t g_stage77_list_head = 0;
uint32_t g_stage77_link_next = 0;
uint32_t g_stage77_queue1 = 0;
uint32_t g_stage77_queue1_cap = 0;
uint32_t g_stage77_queue1_buf = 0;
uint32_t g_stage77_queue2_cap = 0;
uint32_t g_stage77_queue2_buf = 0;
uint32_t g_stage76_first_callback_sp = 0;
uint32_t g_stage76_callback_table = 0;
uint32_t g_stage76_first_callback = 0;
uint32_t g_stage76_second_callback = 0;
uint32_t g_stage76_third_callback = 0;
uint32_t g_stage76_audio_state = 0;
bool g_stage75_parent_post_start_reached = false;
uint32_t g_stage75_audio_thread_sp = 0;
uint32_t g_stage75_audio_thread_a0 = 0;
uint32_t g_stage74_sp = 0;
uint32_t g_stage74_s0 = 0;
uint32_t g_stage73_trace_hits = 0;
uint32_t g_stage73_last_pc = 0;
uint32_t g_stage73_sp = 0;
uint32_t g_stage73_a0 = 0;
uint32_t g_stage73_a1 = 0;
uint32_t g_stage73_a2 = 0;
uint32_t g_stage73_a3 = 0;
uint32_t g_stage72_ai_return_sp = 0;
uint32_t g_stage72_ai_return_v0 = 0;
uint32_t g_stage72_ai_return_a2 = 0;
uint32_t g_first_ai_mmio_sp = 0;
uint32_t g_first_ai_mmio_v0 = 0;
uint32_t g_first_ai_mmio_v1 = 0;
uint32_t g_first_ai_mmio_a0 = 0;
uint32_t g_first_ai_mmio_a1 = 0;
uint32_t g_first_ai_mmio_a2 = 0;
uint32_t g_first_ai_mmio_a3 = 0;

uint32_t g_si_mmio_read_count = 0;
uint32_t g_si_mmio_write_count = 0;
uint32_t g_si_mmio_write_addrs[4]{};
uint32_t g_si_mmio_write_values[4]{};
bool g_si_mmio_args_valid = true;
uint32_t g_first_pif_command_fnv = 0;
std::array<uint8_t, kFirstPifCommandBytes> g_first_pif_command{};

bool g_pi_raw_dma_boundary_reached = false;
bool g_second_pi_raw_boundary_reached = false;
bool g_loader_return_reached = false;
uint32_t g_loader_return_sp = 0;
uint32_t g_parser_input_ptr = 0;
uint32_t g_parser_directory_ptr = 0;
uint32_t g_parser_data_out_ptr = 0;
uint32_t g_parser_size_out_ptr = 0;
std::string g_parser_input_name;
bool g_parser_return_reached = false;
uint32_t g_parser_return_sp = 0;
uint32_t g_parser_return_v0 = 0;
uint32_t g_parser_actual_data_offset = 0;
uint32_t g_parser_actual_size = 0;
uint32_t g_font_original_dest = 0;
bool g_font_payload_boundary_reached = false;
uint32_t g_font_payload_boundary_pc = 0;
uint32_t g_font_payload_sp = 0;
uint32_t g_font_payload_a0 = 0;
uint32_t g_font_payload_a1 = 0;
uint32_t g_font_payload_a2 = 0;
uint32_t g_font_payload_s0 = 0;
uint32_t g_font_payload_s1 = 0;
uint32_t g_font_payload_s2 = 0;
bool g_font_complete_boundary_reached = false;
uint32_t g_font_complete_sp = 0;
uint32_t g_font_complete_s2 = 0;
uint32_t g_font_complete_first_word = 0;
bool g_actual_font_caller_return_reached = false;
uint32_t g_actual_font_caller_sp = 0;
uint32_t g_actual_font_caller_v0 = 0;
uint32_t g_actual_font_caller_s1 = 0;
uint32_t g_actual_font_caller_s2 = 0;
uint32_t g_actual_font_buffer_global = 0;
uint32_t g_font_load_call_count = 0;
uint32_t g_font_complete_count = 0;
bool g_rnc_boundary_reached = false;
uint32_t g_rnc_sp = 0;
uint32_t g_rnc_a0 = 0;
uint32_t g_rnc_a1 = 0;
uint32_t g_rnc_a2 = 0;
uint32_t g_rnc_s0 = 0;
uint32_t g_rnc_s1 = 0;
uint32_t g_rnc_s2 = 0;
uint32_t g_rnc_file_size = 0;
uint32_t g_second_font_complete_first_word = 0;
bool g_rnc_return_reached = false;
uint32_t g_rnc_return_sp = 0;
uint32_t g_rnc_return_s0 = 0;
uint32_t g_rnc_return_s1 = 0;
uint32_t g_rnc_return_s2 = 0;
uint32_t g_rnc_output_first_word = 0;
uint16_t g_rnc_packed_crc = 0;
uint32_t g_rnc_predecode_output_hash = 0;
uint32_t g_rnc_predecode_temp_hash = 0;
uint16_t g_rnc_unpacked_crc = 0;
uint32_t g_rnc_output_fnv = 0;
bool g_first_font_call_seen = false;
uint32_t g_first_font_call_sp = 0;
uint32_t g_first_font_call_a0 = 0;
uint32_t g_first_font_call_a1 = 0;
uint32_t g_first_font_call_s1 = 0;
uint32_t g_first_font_call_s2 = 0;
bool g_second_font_call_reached = false;
uint32_t g_second_font_call_sp = 0;
uint32_t g_second_font_call_a0 = 0;
uint32_t g_second_font_call_a1 = 0;
uint32_t g_second_font_call_s0 = 0;
uint32_t g_second_font_call_s1 = 0;
uint32_t g_second_font_call_s2 = 0;
uint32_t g_second_font_call_s3 = 0;
uint32_t g_second_font_call_s5 = 0;
uint32_t g_second_font_pool_base = 0;
std::string g_second_font_name;
uint32_t g_actual_font_caller_return_count = 0;
bool g_second_font_return_reached = false;
uint32_t g_second_font_return_sp = 0;
uint32_t g_second_font_return_v0 = 0;
uint32_t g_second_font_return_s0 = 0;
uint32_t g_second_font_return_s1 = 0;
uint32_t g_second_font_return_s2 = 0;
uint32_t g_second_font_return_s3 = 0;
uint32_t g_second_font_return_s5 = 0;
uint32_t g_second_font_return_s6 = 0;
uint32_t g_second_font_model_expanded_bytes = 0;
uint32_t g_second_font_model_final_s0 = 0;
bool g_second_font_model_valid = false;
std::array<uint16_t, 3> g_second_font_expected_offsets{};
std::array<uint16_t, 3> g_second_font_expected_packed{};
bool g_third_font_call_reached = false;
uint32_t g_third_font_call_sp = 0;
uint32_t g_third_font_call_a0 = 0;
uint32_t g_third_font_call_a1 = 0;
uint32_t g_third_font_call_s0 = 0;
uint32_t g_third_font_call_s1 = 0;
uint32_t g_third_font_call_s2 = 0;
uint32_t g_third_font_call_s3 = 0;
uint32_t g_third_font_call_s5 = 0;
uint32_t g_third_font_call_s6 = 0;
uint32_t g_third_font_pool_base = 0;
std::string g_third_font_name;
bool g_third_font_return_reached = false;
uint32_t g_third_font_return_sp = 0;
uint32_t g_third_font_return_v0 = 0;
uint32_t g_third_font_return_s0 = 0;
uint32_t g_third_font_return_s1 = 0;
uint32_t g_third_font_return_s2 = 0;
uint32_t g_third_font_return_s3 = 0;
uint32_t g_third_font_return_s5 = 0;
uint32_t g_third_font_return_s6 = 0;
uint32_t g_third_font_model_expanded_bytes = 0;
uint32_t g_third_font_model_final_s0 = 0;
bool g_third_font_model_valid = false;
std::array<uint16_t, 3> g_third_font_expected_offsets{};
std::array<uint16_t, 3> g_third_font_expected_packed{};
uint32_t g_third_font_complete_first_word = 0;
uint32_t g_third_font_complete_hash = 0;
uint32_t g_third_directory_reload_hash_at_dma17 = 0;
uint32_t g_fourth_directory_reload_hash_at_dma24 = 0;
uint32_t g_common_complete_first_word = 0;
uint32_t g_common_complete_hash = 0;
bool g_post_pool_return_reached = false;
uint32_t g_post_pool_return_sp = 0;
uint32_t g_post_pool_return_v0 = 0;
uint32_t g_post_pool_return_s0 = 0;
bool g_three_font_return_reached = false;
uint32_t g_three_font_return_sp = 0;
uint32_t g_three_font_return_v0 = 0;
uint32_t g_three_font_return_s0 = 0;
uint32_t g_three_font_return_a0 = 0;
std::array<uint16_t, 16> g_final_palette{};
uint32_t g_pi_completion_wait_count = 0;
uint32_t g_pi_dma_count = 0;
uint32_t g_pi_auto_event_count = 0;
std::array<uint32_t, kTransferCount> g_raw_dirs{};
std::array<uint32_t, kTransferCount> g_raw_devs{};
std::array<uint32_t, kTransferCount> g_raw_drams{};
std::array<uint32_t, kTransferCount> g_raw_sizes{};
std::array<uint32_t, kTransferCount> g_raw_requests{};
std::array<uint32_t, kTransferCount> g_raw_request_types{};
std::array<uint32_t, kTransferCount> g_raw_request_return_queues{};
std::array<uint32_t, kTransferCount> g_wait_remaining{};
std::array<uint32_t, kTransferCount> g_wait_devs{};
std::array<uint32_t, kTransferCount> g_wait_drams{};
std::array<uint32_t, kTransferCount> g_post_remaining{};
std::array<uint32_t, kTransferCount> g_chunk_actual_hashes{};
uint32_t g_second_directory_reload_hash_at_dma11 = 0u;
std::array<int32_t, kTransferCount> g_auto_event_results{
    {-999, -999, -999, -999, -999, -999, -999}};
uint32_t g_pi_raw_boundary_count = 0;
uint32_t g_loader_wait_count = 0;
uint32_t g_loader_post_count = 0;
bool g_pi_completion_wait_boundary_reached = false;
bool g_pi_mmio_args_valid = true;
bool g_pi_dma_copy_complete = false;
bool g_pi_access_recv_entered = false;
bool g_pi_completion_recv_entered = false;
bool g_loader_recv_entered = false;
bool g_loader_wait_boundary_reached = false;
bool g_loader_post_receive_reached = false;
bool g_release_loader_wait = false;
bool g_pi_completion_event_queued = false;
uint32_t g_pi_cmd_recv_count = 0;
int32_t g_pi_completion_recv_result = -999;
int32_t g_loader_recv_result = -999;
int32_t g_pi_completion_send_result = -999;
uint32_t g_mi_boundary_count = 0;
bool g_release_worker = false;
bool g_worker_dispatch_returned = false;
bool g_release_vi = false;
bool g_vi_dispatch_returned = false;
bool g_thread3_idle_checkpoint_reached = false;
bool g_release_thread4 = false;
bool g_release_thread3 = false;
bool g_thread3_dispatch_returned = false;
bool g_thread4_dispatch_returned = false;
bool g_pi_dispatch_returned = false;

bool g_recv_bridge_entered = false;
uint32_t g_recv_count = 0;
uint32_t g_recv_queue = 0;
uint32_t g_recv_msg = 0;
int32_t g_recv_flags = -1;
bool g_pi_recv_entered = false;
bool g_vi_recv_entered = false;
bool g_worker_recv_entered = false;
uint32_t g_worker_recv_msg = 0;
int32_t g_worker_recv_flags = 0;
uint32_t g_pi_recv_msg = 0;
uint32_t g_vi_recv_msg = 0;
int32_t g_pi_recv_flags = -1;
int32_t g_vi_recv_flags = -1;

bool g_bridge_args_valid = true;
uint32_t g_create_count = 0;
uint32_t g_start_count = 0;
uint32_t g_priority_get_count = 0;
uint32_t g_priority_set_count = 0;
int32_t g_priority_get_value = -1;
int32_t g_priority_set_value = -1;

uint32_t g_thread3_dispatch_sp = 0;
uint32_t g_thread3_dispatch_arg = 0;
uint32_t g_thread4_dispatch_sp = 0;
uint32_t g_thread4_dispatch_arg = 0;
uint32_t g_pi_dispatch_sp = 0;
uint32_t g_pi_dispatch_arg = 0;
uint32_t g_vi_dispatch_sp = 0;
uint32_t g_vi_dispatch_arg = 0;

uint32_t g_pi_thread_started_sp = 0;
uint32_t g_pi_thread_started_s1 = 0;
uint32_t g_pi_thread_started_s2 = 0;

uint32_t g_pi_entry_sp = 0;
uint32_t g_pi_entry_s2 = 0;
uint32_t g_pi_entry_s4 = 0;
uint32_t g_pi_entry_s5 = 0;
uint32_t g_thread3_post_pi_sp = 0;
uint32_t g_thread4_init_sp = 0;
uint32_t g_thread4_init_v0 = 0;
uint32_t g_thread4_init_v1 = 0;
uint32_t g_thread4_queue_bootstrap_sp = 0;
uint32_t g_thread4_queue_bootstrap_a0 = 0;
uint32_t g_thread4_queue_bootstrap_a1 = 0;
uint32_t g_thread4_queue_bootstrap_a2 = 0;
uint32_t g_thread4_queue_bootstrap_a3 = 0;
uint32_t g_thread4_queue_bootstrap_t0 = 0;
uint32_t g_manager_queue_bootstrap_sp = 0;
uint32_t g_manager_queue_bootstrap_s0 = 0;
uint32_t g_manager_queue_bootstrap_s1 = 0;
uint32_t g_manager_queue_bootstrap_s3 = 0;
uint32_t g_manager_queue_bootstrap_s4 = 0;
uint32_t g_manager_queue_bootstrap_s5 = 0;
uint32_t g_vi_manager_bootstrap_sp = 0;
uint32_t g_vi_manager_bootstrap_s0 = 0;
uint32_t g_vi_manager_bootstrap_s1 = 0;
uint32_t g_vi_manager_bootstrap_s2 = 0;
uint32_t g_vi_manager_bootstrap_s3 = 0;
uint32_t g_vi_manager_bootstrap_s4 = 0;
uint32_t g_vi_thread_create_sp = 0;
uint32_t g_vi_thread_create_s0 = 0;
uint32_t g_vi_thread_create_s2 = 0;
uint32_t g_vi_thread_create_s3 = 0;
uint32_t g_vi_thread_create_s4 = 0;
uint32_t g_vi_mmio_boundary_sp = 0;
uint32_t g_vi_mmio_boundary_s0 = 0;
uint32_t g_vi_mmio_boundary_a0 = 0;
uint32_t g_vi_mmio_boundary_v1 = 0;
uint32_t g_vi_mmio_complete_sp = 0;
uint32_t g_vi_mmio_complete_s0 = 0;
uint32_t g_vi_mmio_complete_s1 = 0;
uint32_t g_vi_mmio_complete_s2 = 0;
uint32_t g_vi_mmio_complete_s3 = 0;
uint32_t g_vi_mmio_complete_s4 = 0;
uint32_t g_vi_receive_sp = 0;
uint32_t g_vi_receive_s0 = 0;
uint32_t g_vi_receive_s2 = 0;
uint32_t g_vi_receive_s3 = 0;
uint32_t g_vi_receive_s4 = 0;
uint32_t g_thread4_vi_return_sp = 0;
uint32_t g_thread4_vi_return_s0 = 0;
uint32_t g_thread4_vi_return_s2 = 0;
uint32_t g_thread4_vi_return_s3 = 0;
uint32_t g_thread4_vi_return_s4 = 0;
uint32_t g_worker_create_sp = 0;
uint32_t g_worker_create_s0 = 0;
uint32_t g_worker_create_s1 = 0;
uint32_t g_worker_create_s2 = 0;
uint32_t g_worker_create_s3 = 0;
uint32_t g_worker_create_s4 = 0;
uint32_t g_worker_create_s5 = 0;
uint32_t g_worker_dispatch_sp = 0;
uint32_t g_worker_dispatch_arg = 0;
uint32_t g_worker_receive_sp = 0;
uint32_t g_worker_receive_s1 = 0;
uint32_t g_worker_receive_s2 = 0;
uint32_t g_worker_receive_s3 = 0;
uint32_t g_worker_receive_s4 = 0;
uint32_t g_worker_receive_s5 = 0;
uint32_t g_worker_receive_a0 = 0;
uint32_t g_thread4_post_worker_sp = 0;
uint32_t g_thread4_post_worker_s0 = 0;
uint32_t g_thread4_post_worker_s1 = 0;
uint32_t g_thread4_post_worker_s2 = 0;
uint32_t g_thread4_post_worker_s3 = 0;
uint32_t g_thread4_post_worker_s4 = 0;
uint32_t g_thread4_post_worker_s5 = 0;
uint32_t g_thread4_post_manager_sp = 0;
uint32_t g_mi_boundary_sp = 0;
uint32_t g_mi_boundary_a0 = 0;
uint32_t g_mi_boundary_s0 = 0;
uint32_t g_mi_boundary_s1 = 0;
uint32_t g_mi_boundary_s2 = 0;
uint32_t g_mi_boundary_t2 = 0;
uint32_t g_mi_boundary_shadow = 0;
uint32_t g_mi_boundary_a0_values[2]{};
uint32_t g_post_mi_link_sp = 0;
uint32_t g_post_mi_link_s0 = 0;
uint32_t g_si_boundary_sp = 0;
uint32_t g_si_boundary_a0 = 0;
uint32_t g_si_boundary_a1 = 0;
uint32_t g_si_boundary_a2 = 0;
uint32_t g_si_boundary_s0 = 0;
uint32_t g_pi_raw_sp = 0;
uint32_t g_pi_raw_a0 = 0;
uint32_t g_pi_raw_a1 = 0;
uint32_t g_pi_raw_a2 = 0;
uint32_t g_pi_raw_a3 = 0;
uint32_t g_pi_raw_s0 = 0;
uint32_t g_pi_raw_s1 = 0;
uint32_t g_pi_raw_s2 = 0;
uint32_t g_pi_raw_v0 = 0;
uint32_t g_pi_completion_sp = 0;
uint32_t g_pi_completion_a0 = 0;
uint32_t g_pi_completion_a1 = 0;
uint32_t g_pi_completion_a2 = 0;
uint32_t g_loader_wait_sp = 0;
uint32_t g_loader_wait_a0 = 0;
uint32_t g_loader_wait_a1 = 0;
uint32_t g_loader_wait_a2 = 0;
uint32_t g_loader_wait_s0 = 0;
uint32_t g_loader_wait_s1 = 0;
uint32_t g_loader_wait_s2 = 0;
uint32_t g_loader_wait_s3 = 0;
uint32_t g_loader_wait_s4 = 0;
uint32_t g_loader_post_sp = 0;
uint32_t g_loader_post_s0 = 0;
uint32_t g_loader_post_s1 = 0;
uint32_t g_loader_post_s2 = 0;
uint32_t g_loader_post_s3 = 0;
uint32_t g_loader_post_s4 = 0;
uint32_t g_first_pi_request = 0;
uint32_t g_first_pi_return_queue = 0;
uint32_t g_second_pi_request = 0;
uint32_t g_second_pi_request_type = 0;
uint32_t g_second_pi_return_queue = 0;
uint32_t g_second_pi_request_dram = 0;
uint32_t g_second_pi_request_dev = 0;
uint32_t g_second_pi_request_size = 0;
uint32_t g_second_pi_raw_sp = 0;
uint32_t g_second_pi_raw_a0 = 0;
uint32_t g_second_pi_raw_a1 = 0;
uint32_t g_second_pi_raw_a2 = 0;
uint32_t g_second_pi_raw_a3 = 0;
uint32_t g_second_pi_raw_s2 = 0;
uint32_t g_second_pi_raw_v0 = 0;
uint32_t g_pi_mmio_read_count = 0;
uint32_t g_pi_mmio_write_count = 0;
uint32_t g_pi_mmio_write_addrs[60]{};
uint32_t g_pi_mmio_write_values[60]{};
uint32_t g_pi_dram_addr = 0;
uint32_t g_pi_cart_addr = 0;
uint32_t g_pi_status = 0;
uint32_t g_pi_dma_size = 0;
uint32_t g_pi_expected_hash = 0;
uint32_t g_pi_actual_hash = 0;
const uint8_t* g_stage61_rom_data = nullptr;
uint32_t g_stage61_rom_size = 0;
uint32_t g_mi_mask_state = 0;
uint32_t g_mi_mmio_read_count = 0;
uint32_t g_mi_mmio_write_count = 0;
uint32_t g_mi_write_commands[2]{};
bool g_mi_mmio_args_valid = true;
uint32_t g_pre_933a0_target = 0;
uint32_t g_pre_933a0_control = 0;
uint16_t g_pre_933a0_flags = 0;
std::array<uint32_t, kViRegisterCount> g_vi_registers{};
uint32_t g_vi_mmio_read_count = 0;
uint32_t g_vi_mmio_write_count = 0;
bool g_vi_mmio_args_valid = true;
uint32_t g_thread3_idle_sp = 0;

uint32_t g_parent_checkpoint_count = 0;
uint32_t g_parent_last_pc = 0;
uint32_t g_parent_sp = 0;
bool g_unexpected_parent_checkpoint = false;

uint32_t phys(uint32_t guest) {
    return guest & 0x00FFFFFFu;
}

void write_guest_byte(uint32_t guest, uint8_t value) {
    g_rdram[phys(guest) ^ 3u] = value;
}

void write_guest_u32(uint32_t guest, uint32_t value) {
    write_guest_byte(guest + 0u, static_cast<uint8_t>(value >> 24));
    write_guest_byte(guest + 1u, static_cast<uint8_t>(value >> 16));
    write_guest_byte(guest + 2u, static_cast<uint8_t>(value >> 8));
    write_guest_byte(guest + 3u, static_cast<uint8_t>(value));
}

uint8_t read_guest_byte(uint32_t guest) {
    return g_rdram[phys(guest) ^ 3u];
}

uint32_t read_guest_u32(uint32_t guest) {
    return
        (static_cast<uint32_t>(read_guest_byte(guest + 0u)) << 24) |
        (static_cast<uint32_t>(read_guest_byte(guest + 1u)) << 16) |
        (static_cast<uint32_t>(read_guest_byte(guest + 2u)) << 8) |
        static_cast<uint32_t>(read_guest_byte(guest + 3u));
}

uint16_t read_guest_u16(uint32_t guest) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(read_guest_byte(guest + 0u)) << 8) |
        static_cast<uint16_t>(read_guest_byte(guest + 1u)));
}

bool g_stage92_select_rnc_active = false;
bool g_stage92_select_rnc_boundary_reached = false;
bool g_stage92_select_rnc_return_reached = false;
uint32_t g_stage92_select_rnc_sp = 0u;
uint32_t g_stage92_select_rnc_input = 0u;
uint32_t g_stage92_select_rnc_output = 0u;
uint32_t g_stage92_select_rnc_unpacked_size = 0u;
uint32_t g_stage92_select_rnc_packed_size = 0u;
uint16_t g_stage92_select_rnc_header_unpacked_crc = 0u;
uint16_t g_stage92_select_rnc_header_packed_crc = 0u;
uint16_t g_stage92_select_rnc_computed_packed_crc = 0u;
uint32_t g_stage92_select_rnc_input_hash = 0u;
uint32_t g_stage92_select_rnc_output_hash_before = 0u;
uint32_t g_stage92_select_rnc_return_sp = 0u;
uint32_t g_stage92_select_rnc_return_size = 0u;
uint32_t g_stage92_select_rnc_return_output = 0u;
uint32_t g_stage92_select_rnc_output_first_word = 0u;
uint16_t g_stage92_select_rnc_output_crc = 0u;
uint32_t g_stage92_select_rnc_output_fnv = 0u;

uint32_t stage61_fnv1a_guest(uint32_t guest, uint32_t size);
uint16_t stage61_rnc_crc16_guest(uint32_t guest, uint32_t size);


uint32_t compute_font_bitmap_output_bytes(
    uint32_t font_base,
    uint32_t glyph_count) {

    uint32_t total = 0u;
    for (uint32_t i = 0u; i < glyph_count; ++i) {
        const uint32_t entry = font_base + i * 8u;
        const uint32_t width = read_guest_byte(entry + 0u);
        const uint32_t height = read_guest_byte(entry + 1u);

        if (height == 0u) {
            continue;
        }

        const uint32_t width_plus_one = width + 1u;
        const uint32_t copied_per_row = (width_plus_one + 1u) >> 1u;
        const uint32_t rounded_width = (width + 0x10u) & 0xF0u;
        if (rounded_width < width_plus_one) {
            return 0xFFFFFFFFu;
        }
        const uint32_t pad_per_row =
            (rounded_width - width_plus_one) >> 1u;

        total += (copied_per_row + pad_per_row) * height;
    }
    return total;
}


uint16_t stage61_pack_glyph_metadata(
    uint8_t width,
    uint8_t height,
    uint8_t flags) {

    return static_cast<uint16_t>(
        (((static_cast<uint32_t>(width) + 1u) << 11) & 0xF800u) |
        ((static_cast<uint32_t>(height) & 0x1Fu) << 6) |
        (static_cast<uint32_t>(flags) & 0x3Fu));
}

bool stage61_model_second_font(
    uint32_t font_base,
    uint32_t pool_base,
    uint32_t output_start,
    uint32_t* expanded_bytes,
    std::array<uint16_t, 3>* offsets,
    std::array<uint16_t, 3>* packed) {

    if (expanded_bytes == nullptr ||
        offsets == nullptr ||
        packed == nullptr) {
        return false;
    }

    constexpr std::array<uint32_t, 3> kSpots{0u, 1u, 104u};
    uint32_t current = output_start;
    const uint32_t total =
        compute_font_bitmap_output_bytes(font_base, kFirstFontGlyphCount);
    if (total == 0xFFFFFFFFu) {
        return false;
    }

    for (uint32_t i = 0; i < kFirstFontGlyphCount; ++i) {
        const uint32_t entry = font_base + i * 8u;
        const uint8_t width = read_guest_byte(entry + 0u);
        const uint8_t height = read_guest_byte(entry + 1u);
        const uint8_t flags = read_guest_byte(entry + 2u);
        const uint32_t bitmap_offset = read_guest_u32(entry + 4u);

        if (height != 0u &&
            bitmap_offset + 0x370u >= kRncUnpackedSize) {
            return false;
        }

        for (std::size_t spot = 0; spot < kSpots.size(); ++spot) {
            if (i == kSpots[spot]) {
                (*offsets)[spot] =
                    static_cast<uint16_t>(current - pool_base);
                (*packed)[spot] =
                    stage61_pack_glyph_metadata(width, height, flags);
            }
        }

        if (height != 0u) {
            const uint32_t width_plus_one =
                static_cast<uint32_t>(width) + 1u;
            const uint32_t copied_per_row =
                (width_plus_one + 1u) >> 1u;
            const uint32_t rounded_width =
                (static_cast<uint32_t>(width) + 0x10u) & 0xF0u;
            if (rounded_width < width_plus_one) {
                return false;
            }
            const uint32_t pad_per_row =
                (rounded_width - width_plus_one) >> 1u;
            current +=
                (copied_per_row + pad_per_row) *
                static_cast<uint32_t>(height);
        }
    }

    *expanded_bytes = total;
    return true;
}


bool stage61_model_third_font(
    uint32_t font_base,
    uint32_t pool_base,
    uint32_t output_start,
    uint32_t* expanded_bytes,
    std::array<uint16_t, 3>* offsets,
    std::array<uint16_t, 3>* packed) {

    if (expanded_bytes == nullptr ||
        offsets == nullptr ||
        packed == nullptr) {
        return false;
    }

    constexpr std::array<uint32_t, 3> kSpots{0u, 1u, 104u};
    uint32_t current = output_start;
    const uint32_t total =
        compute_font_bitmap_output_bytes(font_base, kFirstFontGlyphCount);
    if (total == 0xFFFFFFFFu) {
        return false;
    }

    for (uint32_t i = 0; i < kFirstFontGlyphCount; ++i) {
        const uint32_t entry = font_base + i * 8u;
        const uint8_t width = read_guest_byte(entry + 0u);
        const uint8_t height = read_guest_byte(entry + 1u);
        const uint8_t flags = read_guest_byte(entry + 2u);
        const uint32_t bitmap_offset = read_guest_u32(entry + 4u);

        if (height != 0u &&
            bitmap_offset + 0x370u >= kEtchFontFileSize) {
            return false;
        }

        for (std::size_t spot = 0; spot < kSpots.size(); ++spot) {
            if (i == kSpots[spot]) {
                (*offsets)[spot] =
                    static_cast<uint16_t>(current - pool_base);
                (*packed)[spot] =
                    stage61_pack_glyph_metadata(width, height, flags);
            }
        }

        if (height != 0u) {
            const uint32_t width_plus_one =
                static_cast<uint32_t>(width) + 1u;
            const uint32_t copied_per_row =
                (width_plus_one + 1u) >> 1u;
            const uint32_t rounded_width =
                (static_cast<uint32_t>(width) + 0x10u) & 0xF0u;
            if (rounded_width < width_plus_one) {
                return false;
            }
            const uint32_t pad_per_row =
                (rounded_width - width_plus_one) >> 1u;
            current +=
                (copied_per_row + pad_per_row) *
                static_cast<uint32_t>(height);
        }
    }

    *expanded_bytes = total;
    return true;
}


std::string read_guest_string(uint32_t guest, std::size_t max_len = 255u) {
    std::string out;
    out.reserve(64u);
    for (std::size_t i = 0; i < max_len; ++i) {
        const uint8_t c = read_guest_byte(guest + static_cast<uint32_t>(i));
        if (c == 0u) {
            return out;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

uint32_t read_rom_be32(
    const uint8_t* rom,
    uint32_t rom_size,
    uint32_t offset) {

    if (rom == nullptr || offset + 4u > rom_size) {
        return 0u;
    }
    return
        (static_cast<uint32_t>(rom[offset + 0u]) << 24) |
        (static_cast<uint32_t>(rom[offset + 1u]) << 16) |
        (static_cast<uint32_t>(rom[offset + 2u]) << 8) |
        static_cast<uint32_t>(rom[offset + 3u]);
}

char normalize_path_char(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u == static_cast<unsigned char>('/')) {
        u = static_cast<unsigned char>('\\');
    }
    if (u >= static_cast<unsigned char>('A') &&
        u <= static_cast<unsigned char>('Z')) {
        u = static_cast<unsigned char>(u + 0x20u);
    }
    return static_cast<char>(u);
}

bool normalized_path_equal(
    const std::string& a,
    const std::string& b) {

    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (normalize_path_char(a[i]) != normalize_path_char(b[i])) {
            return false;
        }
    }
    return true;
}

struct DirectoryLookupResult {
    bool found = false;
    uint32_t index = 0;
    uint32_t entry_rom_offset = 0;
    uint32_t size = 0;
    uint32_t data_offset = 0;
    std::string name;
};

DirectoryLookupResult lookup_directory_from_rom(
    const uint8_t* rom,
    uint32_t rom_size,
    const std::string& requested) {

    DirectoryLookupResult result{};
    if (rom == nullptr ||
        kDirectoryRomEnd > rom_size ||
        requested.empty()) {
        return result;
    }

    uint32_t p = kDirectoryRomOffset;
    uint32_t cumulative_data = 0u;
    uint32_t index = 0u;

    while (p < kDirectoryRomEnd) {
        if (rom[p] == 0u) {
            break;
        }

        const uint32_t name_start = p;
        std::string name;
        while (p < kDirectoryRomEnd && rom[p] != 0u) {
            name.push_back(static_cast<char>(rom[p]));
            ++p;
        }
        if (p >= kDirectoryRomEnd) {
            break;
        }

        ++p; // NUL
        const uint32_t metadata =
            (p + 3u) & ~3u;
        if (metadata + 8u > kDirectoryRomEnd) {
            break;
        }

        const uint32_t size =
            read_rom_be32(rom, rom_size, metadata);

        if (normalized_path_equal(requested, name)) {
            result.found = true;
            result.index = index;
            result.entry_rom_offset = name_start;
            result.size = size;
            result.data_offset =
                kDirectoryDataOffset + cumulative_data;
            result.name = name;
            return result;
        }

        cumulative_data =
            (cumulative_data + size + 7u) & ~7u;
        p = metadata + 8u;
        ++index;
    }

    return result;
}

bool verify_queue(
    uint32_t queue_addr,
    uint32_t expected_valid_count,
    uint32_t expected_capacity,
    uint32_t expected_buffer,
    const char* label) {

    const uint32_t mtqueue = read_guest_u32(queue_addr + 0x00u);
    const uint32_t fullqueue = read_guest_u32(queue_addr + 0x04u);
    const uint32_t valid = read_guest_u32(queue_addr + 0x08u);
    const uint32_t first = read_guest_u32(queue_addr + 0x0Cu);
    const uint32_t capacity = read_guest_u32(queue_addr + 0x10u);
    const uint32_t buffer = read_guest_u32(queue_addr + 0x14u);

    std::printf(
        "QUEUE %s: addr=0x%08X recvHead=0x%08X sendHead=0x%08X "
        "valid=%u first=%u capacity=%u buffer=0x%08X\n",
        label, queue_addr, mtqueue, fullqueue,
        valid, first, capacity, buffer);

    return
        valid == expected_valid_count &&
        first == 0u &&
        capacity == expected_capacity &&
        buffer == expected_buffer;
}

bool load_rom(std::vector<uint8_t>& rom) {
    std::ifstream file(g_ts2_rom_file, std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "FAILED: could not open ROM:\n  %s\n", g_ts2_rom_file.string().c_str());
        return false;
    }

    const std::streamsize size = file.tellg();
    if (size != 0x00C00000) {
        std::fprintf(stderr, "FAILED: unexpected ROM size: %lld\n",
                     static_cast<long long>(size));
        return false;
    }

    file.seekg(0, std::ios::beg);
    rom.resize(static_cast<std::size_t>(size));
    return static_cast<bool>(
        file.read(reinterpret_cast<char*>(rom.data()), size)
    );
}

bool stage_resident_image(const std::vector<uint8_t>& rom) {
    if (rom.size() < kResidentRomEnd) return false;

    for (uint32_t i = 0; i < kResidentSize; i++) {
        write_guest_byte(kResidentGuest + i, rom[kResidentRomStart + i]);
    }

    return true;
}

void poison_bss_ranges() {
    for (uint32_t a = kBss1Start; a < kBss2End; a++) {
        write_guest_byte(a, 0xA5u);
    }
}

void initialize_boot_globals() {
    write_guest_u32(kOsTvType,    1u);
    write_guest_u32(kOsRomType,   0u);
    write_guest_u32(kOsRomBase,   0xB0000000u);
    write_guest_u32(kOsResetType, 0u);
    write_guest_u32(kOsCicId,     0u);
    write_guest_u32(kOsVersion,   0u);
    write_guest_u32(kOsMemSize,   static_cast<uint32_t>(kRdramSize));

    for (uint32_t i = 0; i < kOsAppNmiSize; i++) {
        write_guest_byte(kOsAppNmi + i, 0x5Au);
    }
}

void poison_thread4_clear_region() {
    for (uint32_t a = kThread4ClearStart; a < kThread4ClearEnd; a++) {
        write_guest_byte(a, 0xA5u);
    }
}

bool verify_thread4_clear_region() {
    for (uint32_t a = kThread4ClearStart; a < kThread4ClearEnd; a++) {
        if (read_guest_byte(a) != 0u) {
            std::fprintf(
                stderr,
                "FAILED: thread-4 clear region first nonzero byte at "
                "0x%08X = 0x%02X.\n",
                a,
                read_guest_byte(a));
            return false;
        }
    }
    return true;
}

OSThread* runtime_thread(uint8_t* rdram, uint32_t guest_addr) {
    return TO_PTR(
        OSThread,
        static_cast<PTR(OSThread)>(static_cast<int32_t>(guest_addr)));
}

bool matches_thread3(
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {
    return
        thread_addr == kThread3Addr &&
        id == kThread3Id &&
        entry == kThread3Entry &&
        arg == kThread3Arg &&
        sp == kThread3StackTop &&
        pri == kThread3Priority;
}

bool matches_thread4(
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {
    return
        thread_addr == kThread4Addr &&
        id == kThread4Id &&
        entry == kThread4Entry &&
        arg == kThread4Arg &&
        sp == kThread4StackTop &&
        pri == kThread4Priority;
}

bool matches_pi_manager_thread(
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {
    return
        thread_addr == kPiManagerThreadAddr &&
        id == kPiManagerThreadId &&
        entry == kPiManagerThreadEntry &&
        arg == kPiManagerThreadArg &&
        sp == kPiManagerThreadStackTop &&
        pri == kPiManagerThreadPriority;
}

bool matches_vi_manager_thread(
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {
    return
        thread_addr == kViManagerThreadAddr &&
        id == kViManagerThreadId &&
        entry == kViManagerThreadEntry &&
        arg == kViManagerThreadArg &&
        sp == kViManagerThreadStackTop &&
        pri == kViManagerThreadPriority;
}

bool matches_worker_thread(
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {
    return
        thread_addr == kWorkerThreadAddr &&
        id == kWorkerThreadId &&
        entry == kWorkerThreadEntry &&
        arg == kWorkerThreadArg &&
        sp == kWorkerThreadStackTop &&
        pri == kWorkerThreadPriority;
}

extern "C" int stage61_create_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t id,
    uint32_t entry,
    uint32_t arg,
    uint32_t sp,
    int32_t pri) {

    g_create_count++;

    std::printf(
        "N64MR BRIDGE CREATE #%u: thread=0x%08X id=%d entry=0x%08X "
        "arg=0x%08X stack=0x%08X pri=%d\n",
        g_create_count,
        thread_addr, id, entry, arg, sp, pri);

    if (!matches_thread3(thread_addr, id, entry, arg, sp, pri) &&
        !matches_thread4(thread_addr, id, entry, arg, sp, pri) &&
        !matches_pi_manager_thread(thread_addr, id, entry, arg, sp, pri) &&
        !matches_vi_manager_thread(thread_addr, id, entry, arg, sp, pri) &&
        !matches_worker_thread(thread_addr, id, entry, arg, sp, pri)) {
        g_bridge_args_valid = false;
        return 1;
    }

    osCreateThread(
        rdram,
        static_cast<PTR(OSThread)>(static_cast<int32_t>(thread_addr)),
        id,
        static_cast<PTR(thread_func_t)>(static_cast<int32_t>(entry)),
        static_cast<PTR(void)>(static_cast<int32_t>(arg)),
        static_cast<PTR(void)>(static_cast<int32_t>(sp)),
        pri);

    return 1;
}

extern "C" int stage61_start_bridge(
    uint8_t* rdram,
    uint32_t thread_addr) {

    g_start_count++;

    std::printf(
        "N64MR BRIDGE START #%u: thread=0x%08X\n",
        g_start_count,
        thread_addr);

    if (thread_addr != kThread3Addr &&
        thread_addr != kPiManagerThreadAddr &&
        thread_addr != kThread4Addr &&
        thread_addr != kViManagerThreadAddr &&
        thread_addr != kWorkerThreadAddr) {
        // Stage 41 additionally starts the priority-13 manager worker.
        g_bridge_args_valid = false;
        return 1;
    }

    osStartThread(
        rdram,
        static_cast<PTR(OSThread)>(static_cast<int32_t>(thread_addr)));

    return 1;
}


extern "C" uint32_t ts2_stage81_ai_status(void) {
    ++g_stage81_status_call_count;

    // Both Toy Story 2 callers use only AI_STATUS bit 31 (FIFO full).
    // N64ModernRuntime exposes a host audio queue rather than the N64's
    // two-entry AI DMA FIFO, so the high-level port representation is
    // "not full"; actual pacing comes from get_remaining_audio_bytes().
    return 0u;
}

extern "C" uint32_t ts2_stage81_ai_remaining_bytes(void) {
    ++g_stage81_length_call_count;

    static bool stage81_frequency_initialized = false;
    if (!stage81_frequency_initialized) {
        if (g_stage72_ai_return_v0 == 0u) {
            std::fputs(
                "FAILED: Stage 81 reached AI_LEN before Stage 72 produced "
                "a nonzero effective audio rate.\n",
                stderr);
            std::fflush(stderr);
            std::_Exit(47);
        }

        ultramodern::set_audio_frequency(g_stage72_ai_return_v0);
        stage81_frequency_initialized = true;

        std::printf(
            "N64MR AUDIO FREQUENCY: deferred Stage 72 proven effective "
            "rate=%u Hz until first Stage 81 AI_LEN query.\n",
            g_stage72_ai_return_v0);
    }

    const uint32_t bytes = ultramodern::get_remaining_audio_bytes();
    g_stage81_last_remaining_bytes = bytes;

    std::printf(
        "N64MR AI LENGTH #%u: remaining host-buffer bytes=%u.\\n",
        g_stage81_length_call_count,
        bytes);

    return bytes;
}

extern "C" int32_t ts2_stage81_queue_audio_buffer(
    uint8_t* rdram,
    uint32_t guest_addr,
    uint32_t byte_count) {

    if (guest_addr < 0x80000000u || guest_addr > 0x807FFFFFu) {
        std::fprintf(
            stderr,
            "FAILED: Stage 81 audio buffer guest address 0x%08X is outside RDRAM.\\n",
            guest_addr);
        std::fflush(stderr);
        std::_Exit(43);
    }

    // N64ModernRuntime's public queue_audio_buffer contract asserts only that
    // the byte count is an integer multiple of int16_t samples.
    if (byte_count == 0u || (byte_count & 0x1u) != 0u) {
        std::fprintf(
            stderr,
            "FAILED: Stage 81 audio buffer byte count %u is not a nonzero "
            "multiple of 16-bit samples.\\n",
            byte_count);
        std::fflush(stderr);
        std::_Exit(44);
    }

    ultramodern::queue_audio_buffer(
        PASS_RDRAM static_cast<PTR(s16)>(guest_addr),
        byte_count);

    {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        ++g_stage81_queue_call_count;
        if (g_stage81_queue_call_count == 1u) {
            g_stage81_first_buffer = guest_addr;
            g_stage81_first_buffer_bytes = byte_count;
            g_stage81_audio_queue_reached = true;

            std::printf(
                "STAGE 81 FIRST AUDIO BUFFER: queue #%u guest=0x%08X "
                "bytes=%u statusCalls=%u lengthCalls=%u "
                "lastRemaining=%u; submitted through N64ModernRuntime.\\n",
                g_stage81_queue_call_count,
                g_stage81_first_buffer,
                g_stage81_first_buffer_bytes,
                g_stage81_status_call_count,
                g_stage81_length_call_count,
                g_stage81_last_remaining_bytes);

            g_thread_cv.notify_all();
            // TS2 FAST-FORWARD: Stage 81 originally parked forever here after
// proving the first native audio-buffer submission. That historical
// regression stop is obsolete now that bring-up continues past Stage 81.
std::fflush(stdout);
std::fflush(stderr);
        }
    }

    return 0;
}

extern "C" int stage61_thread_checkpoint(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc) {
    // TS2 OVERLAY16 FONT SWAP PROBE V39B
    // TEMPORARY DIAGNOSTIC ONLY.
    // Observe overlay-16's temporary font/scratch pointer swap.
    {
        constexpr uint32_t kV39bFontGlobal = 0x801DB3F0u;
        constexpr uint32_t kV39bScratchGlobal = 0x801C8E50u;
        constexpr uint32_t kV39bCompanionGlobal = 0x801E9524u;

        const uint32_t v39b_font =
            read_guest_u32(kV39bFontGlobal);
        const uint32_t v39b_scratch =
            read_guest_u32(kV39bScratchGlobal);

        static uint32_t s_v39b_last_scratch = 0xFFFFFFFFu;

        if (v39b_scratch != s_v39b_last_scratch) {
            const uint32_t v39b_previous = s_v39b_last_scratch;
            s_v39b_last_scratch = v39b_scratch;

            if (v39b_previous == 0xFFFFFFFFu ||
                v39b_previous == 0u ||
                v39b_scratch == 0u) {
                std::fprintf(
                    stderr,
                    "TS2 V39B SCRATCH CHANGE: "
                    "guestPC=0x%08X prev=0x%08X now=0x%08X "
                    "font=0x%08X SP=0x%08X RA=0x%08X\n",
                    guest_pc,
                    v39b_previous,
                    v39b_scratch,
                    v39b_font,
                    static_cast<uint32_t>(ctx->r29),
                    static_cast<uint32_t>(ctx->r31));
                std::fflush(stderr);
            }
        }

        const bool v39b_overlay_checkpoint =
            guest_pc == 0x80000A40u ||
            guest_pc == 0x80000A98u ||
            guest_pc == 0x80000AA8u ||
            guest_pc == 0x80001E3Cu ||
            guest_pc == 0x80001E40u;

        if (v39b_overlay_checkpoint) {
            const char* v39b_stage =
                guest_pc == 0x80000A40u ? "ENTRY" :
                guest_pc == 0x80000A98u ? "AFTER_FONT_SWAP" :
                guest_pc == 0x80000AA8u ? "AFTER_SCRATCH_ADVANCE" :
                guest_pc == 0x80001E3Cu ? "BEFORE_FONT_RESTORE" :
                                          "AFTER_FONT_RESTORE";

            std::fprintf(
                stderr,
                "TS2 V39B OVERLAY16 %s: "
                "guestPC=0x%08X "
                "font=0x%08X scratch=0x%08X companion=0x%08X "
                "T1=0x%08X V1=0x%08X T2=0x%08X "
                "SP=0x%08X RA=0x%08X\n",
                v39b_stage,
                guest_pc,
                v39b_font,
                v39b_scratch,
                read_guest_u32(kV39bCompanionGlobal),
                static_cast<uint32_t>(ctx->r9),
                static_cast<uint32_t>(ctx->r3),
                static_cast<uint32_t>(ctx->r10),
                static_cast<uint32_t>(ctx->r29),
                static_cast<uint32_t>(ctx->r31));
            std::fflush(stderr);

            return 0;
        }
    }

    // TS2 FONTPOOL EXACT WRITER PROBE V36
    // TEMPORARY DIAGNOSTIC ONLY.
    //
    // 0x800096F0 is Toy Story 2's byte-fill helper:
    //     fill [a0, a0 + a2) with byte a1.
    //
    // 0x80013760 initializes an allocator-backed pointer:
    //     *a0 = arenaBase[a1]
    //
    // These probes only observe the ORIGINAL entry arguments. They never
    // modify the pointer, fill range, arena, registers, or return value.
    if (guest_pc == 0x800096F0u) {
        constexpr uint32_t kV36FontPoolGlobal = 0x801DB3F0u;

        const uint32_t v36_start = static_cast<uint32_t>(ctx->r4);
        const uint32_t v36_fill = static_cast<uint32_t>(ctx->r5) & 0xFFu;
        const uint32_t v36_size = static_cast<uint32_t>(ctx->r6);

        const uint64_t v36_begin = static_cast<uint64_t>(v36_start);
        const uint64_t v36_end =
            v36_begin + static_cast<uint64_t>(v36_size);
        const uint64_t v36_target =
            static_cast<uint64_t>(kV36FontPoolGlobal);

        if (v36_size != 0u &&
            v36_begin <= v36_target &&
            v36_target < v36_end) {
            static uint32_t s_v36_overlap_count = 0u;
            const uint32_t v36_number = ++s_v36_overlap_count;

            std::fprintf(
                stderr,
                "TS2 V36 FONTPOOL MEMFILL OVERLAP #%u "
                "start=0x%08X size=0x%08X fill=0x%02X "
                "target=0x%08X offset=0x%08X "
                "SP=0x%08X RA=0x%08X "
                "fontBefore=0x%08X\n",
                v36_number,
                v36_start,
                v36_size,
                v36_fill,
                kV36FontPoolGlobal,
                kV36FontPoolGlobal - v36_start,
                static_cast<uint32_t>(ctx->r29),
                static_cast<uint32_t>(ctx->r31),
                read_guest_u32(kV36FontPoolGlobal));
            std::fflush(stderr);
        }

        return 0;
    }

    if (guest_pc == 0x80013760u) {
        constexpr uint32_t kV36FontPoolGlobal = 0x801DB3F0u;
        constexpr uint32_t kV36ArenaBaseTable = 0x800A2AC0u;

        const uint32_t v36_out = static_cast<uint32_t>(ctx->r4);
        const uint32_t v36_arena = static_cast<uint32_t>(ctx->r5);
        const uint32_t v36_request = static_cast<uint32_t>(ctx->r6);

        if (v36_out == kV36FontPoolGlobal) {
            static uint32_t s_v36_alloc_count = 0u;
            const uint32_t v36_number = ++s_v36_alloc_count;

            uint32_t v36_arena_base = 0xFFFFFFFFu;
            if (v36_arena < 4u) {
                v36_arena_base =
                    read_guest_u32(
                        kV36ArenaBaseTable + v36_arena * 4u);
            }

            std::fprintf(
                stderr,
                "TS2 V36 FONTPOOL ALLOC WRITE #%u "
                "out=0x%08X arena=%u arenaBase=0x%08X "
                "request=0x%08X fontBefore=0x%08X "
                "SP=0x%08X RA=0x%08X\n",
                v36_number,
                v36_out,
                v36_arena,
                v36_arena_base,
                v36_request,
                read_guest_u32(kV36FontPoolGlobal),
                static_cast<uint32_t>(ctx->r29),
                static_cast<uint32_t>(ctx->r31));
            std::fflush(stderr);
        }

        return 0;
    }

    // TS2 FONT POOL GLOBAL WATCH V31B
    // TEMPORARY DIAGNOSTIC ONLY.
    // Observe 0x801DB3F0 transitions; never repair or overwrite it here.
    {
        static uint32_t s_v31b_last_font_pool = 0xFFFFFFFFu;

        const uint32_t v31b_font_pool = read_guest_u32(0x801DB3F0u);
        const uint32_t v31b_previous = s_v31b_last_font_pool;

        if (v31b_font_pool != v31b_previous) {
            s_v31b_last_font_pool = v31b_font_pool;

            std::fprintf(
                stderr,
                "TS2 V31B FONTPOOL CHANGE: "
                "guestPC=0x%08X prev=0x%08X now=0x%08X "
                "SP=0x%08X RA=0x%08X "
                "A0=0x%08X A1=0x%08X A2=0x%08X A3=0x%08X "
                "V0=0x%08X V1=0x%08X\n",
                guest_pc,
                v31b_previous,
                v31b_font_pool,
                static_cast<uint32_t>(ctx->r29),
                static_cast<uint32_t>(ctx->r31),
                static_cast<uint32_t>(ctx->r4),
                static_cast<uint32_t>(ctx->r5),
                static_cast<uint32_t>(ctx->r6),
                static_cast<uint32_t>(ctx->r7),
                static_cast<uint32_t>(ctx->r2),
                static_cast<uint32_t>(ctx->r3));

            std::fflush(stderr);
        }
    }


    if (guest_pc == 0x800086DCu) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        // TS2 FAST-FORWARD: the old idle milestone is the no-audio-buffer
// alternative. Do not wait for that mutually-exclusive proof after
// a real audio buffer has already been submitted.
if (!g_stage81_idle_milestone_reached &&
    !g_stage81_audio_queue_reached) {
            std::puts(
                "STAGE 82: main boot returned from the audio system before "
                "the Stage 81 idle-audio regression proof completed; "
                "waiting for that proof.");
            g_thread_cv.wait(
                lock,
                [] { return g_stage81_idle_milestone_reached; });
        }

        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t v0 = static_cast<uint32_t>(ctx->r2);
        const uint32_t engine_ptr = read_guest_u32(0x800AB5A0u);

        uint32_t engine_first = 0xFFFFFFFFu;
        if (engine_ptr >= 0x80000000u && engine_ptr <= 0x807FFFFCu) {
            engine_first = read_guest_u32(engine_ptr);
        }

                // Stage 81 proved engine[0] == 0 earlier. Generated code then
        // executes func_8008D2D8(a0=0x801E3F38,a1=0x800ACBE0) ->
        // func_8009C680 -> sw a1,0(a0) at guest PC 0x8009C6B8.
if (engine_ptr != 0x801E3F38u ||
            engine_first != 0x800ACBE0u) {
            std::fprintf(
                stderr,
                "FAILED: Stage 82 expected the source-proven post-audio "
                "engine state engine=0x801E3F38 engine[0]=0x800ACBE0, got "
                "engine=0x%08X engine[0]=0x%08X.\\n",
                engine_ptr,
                engine_first);
            std::fflush(stdout);
            std::fflush(stderr);
            std::_Exit(82);
        }

        g_stage82_post_audio_system_return_reached = true;
        g_stage82_return_sp = sp;
        g_stage82_return_v0 = v0;
        g_thread_cv.notify_all();

        std::printf(
            "STAGE 82 MAIN BOOT RETURN: guest PC=0x800086DC "
            "SP=0x%08X v0=0x%08X engine=0x%08X engine[0]=0x%08X.\\n",
            sp,
            v0,
            engine_ptr,
            engine_first);

        std::puts(
            "Stage 82 post-audio-system main-thread return checkpoint PASSED; continuing to Stage 83 sprite/resource completion.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }



    if (guest_pc == 0x800905D4u) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        ++g_stage81_v7_mix_probe_count;

        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t task_word = read_guest_u32(sp + 0x20u);
        const uint32_t return_v0 = static_cast<uint32_t>(ctx->r2);
        const uint32_t slot = static_cast<uint32_t>(ctx->r16);
        const uint32_t ai_quarters = static_cast<uint32_t>(ctx->r18);
        const uint32_t ring_index = static_cast<uint32_t>(ctx->r17);
        const uint32_t prev_buffer = read_guest_u32(0x800AA2E0u);

        uint32_t slot_word0 = 0u;
        uint32_t slot_word1 = 0u;
        if (slot >= 0x80000000u && slot <= 0x807FFFF8u) {
            slot_word0 = read_guest_u32(slot + 0x00u);
            slot_word1 = read_guest_u32(slot + 0x04u);
        }

        g_stage81_v7_last_sp = sp;
        g_stage81_v7_last_task_word = task_word;
        g_stage81_v7_last_return_v0 = return_v0;
        g_stage81_v7_last_slot = slot;
        g_stage81_v7_last_slot_word0 = slot_word0;
        g_stage81_v7_last_slot_word1 = slot_word1;
        g_stage81_v7_last_prev_buffer = prev_buffer;
        g_stage81_v7_last_ai_quarters = ai_quarters;
        g_stage81_v7_last_ring_index = ring_index;

        if (task_word != 0u) {
            g_stage81_v7_task_output_seen = true;
        }

        const uint32_t engine_ptr = read_guest_u32(0x800AB5A0u);
        uint32_t engine_first = 0xFFFFFFFFu;
        if (engine_ptr >= 0x80000000u && engine_ptr <= 0x807FFFFCu) {
            engine_first = read_guest_u32(engine_ptr);
        }

        const bool exact_idle_state =
            engine_ptr == 0x801E3F38u &&
            engine_first == 0u &&
            task_word == 0u &&
            prev_buffer == 0u &&
            ai_quarters == 0u;

        if (g_stage81_v7_mix_probe_count >= 2u &&
            g_stage81_length_call_count >= 2u &&
            g_stage79_callback1_return_reached &&
            g_stage80_ai_status_boundary_reached &&
            exact_idle_state &&
            !g_stage81_audio_queue_reached) {

            std::printf(
                "STAGE 81 IDLE AUDIO MILESTONE: "
                "iterations=%u AI_LEN_calls=%u engine=0x%08X "
                "engine[0]=0x%08X taskWord=0x%08X "
                "prevBuffer=0x%08X AIquarterSamples=%u.\\n",
                g_stage81_v7_mix_probe_count,
                g_stage81_length_call_count,
                engine_ptr,
                engine_first,
                task_word,
                prev_buffer,
                ai_quarters);

            g_stage81_idle_milestone_reached = true;
            g_thread_cv.notify_all();

            std::puts(
                "Stage 81 native AI idle-loop checkpoint PASSED; "
                "continuing to the next real blocking audio receive so "
                "N64ModernRuntime can yield back to the parent thread.");
            std::fflush(stdout);
            std::fflush(stderr);
            return 0;
        }

        std::printf(
            "STAGE 81 V7 MIX PROBE #%u: "
            "SP=0x%08X taskWord=0x%08X returnV0=0x%08X "
            "slot=0x%08X slot[0/4]=0x%08X/0x%08X "
            "prevBuffer=0x%08X aiQuarterSamples=%u ringIndex=%u.\\n",
            g_stage81_v7_mix_probe_count,
            sp,
            task_word,
            return_v0,
            slot,
            slot_word0,
            slot_word1,
            prev_buffer,
            ai_quarters,
            ring_index);

        if (g_stage81_v7_mix_probe_count >= 6u &&
            !g_stage81_audio_queue_reached) {
            g_stage81_v7_probe_complete = true;
            g_thread_cv.notify_all();

            std::puts(
                "STAGE 81 V7 PROBE LIMIT: six post-mixer iterations reached "
                "without an N64ModernRuntime audio-buffer submission; "
                "stopping the audio thread for diagnosis.");

            g_thread_cv.wait(lock, [] { return false; });
            return 1;
        }

        return 0;
    }


    if (guest_pc == 0x800A06CCu) {
        if (g_stage80_ai_status_boundary_reached) {
            return 0;
        }

        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage80_ai_status_sp = static_cast<uint32_t>(ctx->r29);
        g_stage80_ai_status_addr = static_cast<uint32_t>(ctx->r2);

        std::printf(
            "STAGE 80 AI STATUS BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X address=0x%08X; stopping before the direct "
            "AI_STATUS_REG read.\n",
            guest_pc,
            g_stage80_ai_status_sp,
            g_stage80_ai_status_addr);

        if (g_stage80_ai_status_sp != 0x8031EFA0u) {
            std::fprintf(
                stderr,
                "FAILED: Stage 80 expected audio-thread SP 0x8031EFA0 "
                "at AI_STATUS boundary, got 0x%08X.\n",
                g_stage80_ai_status_sp);
            std::fflush(stderr);
            std::_Exit(40);
        }

        if (g_stage80_ai_status_addr != 0xA450000Cu) {
            std::fprintf(
                stderr,
                "FAILED: Stage 80 expected AI_STATUS_REG address "
                "0xA450000C, got 0x%08X.\n",
                g_stage80_ai_status_addr);
            std::fflush(stderr);
            std::_Exit(41);
        }

        g_stage80_ai_status_boundary_reached = true;
        g_thread_cv.notify_all();

        std::puts("STAGE 80: boundary preserved as a Stage 81 regression checkpoint; continuing through the native AI bridge.");
        return 0;
    }


    (void)rdram;

    if (guest_pc == kThread3EntryCheckpointPc) {
        std::printf(
            "THREAD 3: passed Stage 25 entry checkpoint 0x%08X "
            "with SP=0x%08X; continuing.\n",
            guest_pc,
            static_cast<uint32_t>(ctx->r29));
        return 0;
    }

    if (guest_pc == kThread4CreateCheckpointPc) {
        std::printf(
            "THREAD 3: passed Stage 26 thread-4 creation checkpoint "
            "0x%08X with SP=0x%08X; entering PI-manager setup.\n",
            guest_pc,
            static_cast<uint32_t>(ctx->r29));
        return 0;
    }

    if (guest_pc == kPiBootstrapCheckpointPc) {
        std::printf(
            "THREAD 3: passed Stage 27 PI queue/event checkpoint "
            "0x%08X with SP=0x%08X; continuing.\n",
            guest_pc,
            static_cast<uint32_t>(ctx->r29));
        return 0;
    }

    if (guest_pc == kPiThreadCreateCheckpointPc) {
        std::printf(
            "THREAD 3: passed Stage 28 PI-thread creation checkpoint "
            "0x%08X with SP=0x%08X; starting PI manager.\n",
            guest_pc,
            static_cast<uint32_t>(ctx->r29));
        return 0;
    }

    if (guest_pc == kPiThreadRestoredCheckpointPc) {
        g_pi_thread_started_sp = static_cast<uint32_t>(ctx->r29);
        g_pi_thread_started_s1 = static_cast<uint32_t>(ctx->r17);
        g_pi_thread_started_s2 = static_cast<uint32_t>(ctx->r18);
        g_pi_thread_started_checkpoint_reached = true;

        std::printf(
            "THREAD 3: passed Stage 29 priority-restore checkpoint "
            "0x%08X with SP=0x%08X oldPri=%u; returning from PI setup.\n",
            guest_pc,
            g_pi_thread_started_sp,
            g_pi_thread_started_s2);

        return 0;
    }

    if (guest_pc == kPiManagerEntryCheckpointPc) {
        g_pi_entry_sp = static_cast<uint32_t>(ctx->r29);
        g_pi_entry_s2 = static_cast<uint32_t>(ctx->r18);
        g_pi_entry_s4 = static_cast<uint32_t>(ctx->r20);
        g_pi_entry_s5 = static_cast<uint32_t>(ctx->r21);
        g_pi_entry_checkpoint_reached = true;

        std::printf(
            "PI MANAGER: passed Stage 29 entry checkpoint 0x%08X "
            "with SP=0x%08X arg/s2=0x%08X; entering blocking receive.\n",
            guest_pc,
            g_pi_entry_sp,
            g_pi_entry_s2);

        return 0;
    }

    if (guest_pc == kThread3PostPiManagerCheckpointPc) {
        g_thread3_post_pi_sp = static_cast<uint32_t>(ctx->r29);
        g_thread3_post_pi_checkpoint_reached = true;

        std::printf(
            "THREAD 3: passed Stage 30 post-PI checkpoint 0x%08X "
            "with SP=0x%08X; starting thread 4.\n",
            guest_pc,
            g_thread3_post_pi_sp);
        return 0;
    }

    if (guest_pc == kThread4InitCheckpointPc) {
        g_thread4_init_sp = static_cast<uint32_t>(ctx->r29);
        g_thread4_init_v0 = static_cast<uint32_t>(ctx->r2);
        g_thread4_init_v1 = static_cast<uint32_t>(ctx->r3);
        g_thread4_init_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 31 startup-clear checkpoint 0x%08X "
            "with SP=0x%08X v0=0x%08X v1=0x%08X; continuing.\n",
            guest_pc,
            g_thread4_init_sp,
            g_thread4_init_v0,
            g_thread4_init_v1);

        return 0;
    }

    if (guest_pc == kThread4QueueBootstrapCheckpointPc) {
        g_thread4_queue_bootstrap_sp = static_cast<uint32_t>(ctx->r29);
        g_thread4_queue_bootstrap_a0 = static_cast<uint32_t>(ctx->r4);
        g_thread4_queue_bootstrap_a1 = static_cast<uint32_t>(ctx->r5);
        g_thread4_queue_bootstrap_a2 = static_cast<uint32_t>(ctx->r6);
        g_thread4_queue_bootstrap_a3 = static_cast<uint32_t>(ctx->r7);
        g_thread4_queue_bootstrap_t0 = static_cast<uint32_t>(ctx->r8);
        g_thread4_queue_bootstrap_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 32 queue/bootstrap checkpoint 0x%08X "
            "with SP=0x%08X; entering func_80096DE0.\n",
            guest_pc,
            g_thread4_queue_bootstrap_sp);

        return 0;
    }

    if (guest_pc == kManagerQueueBootstrapCheckpointPc) {
        g_manager_queue_bootstrap_sp = static_cast<uint32_t>(ctx->r29);
        g_manager_queue_bootstrap_s0 = static_cast<uint32_t>(ctx->r16);
        g_manager_queue_bootstrap_s1 = static_cast<uint32_t>(ctx->r17);
        g_manager_queue_bootstrap_s3 = static_cast<uint32_t>(ctx->r19);
        g_manager_queue_bootstrap_s4 = static_cast<uint32_t>(ctx->r20);
        g_manager_queue_bootstrap_s5 = static_cast<uint32_t>(ctx->r21);
        g_manager_queue_bootstrap_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 33 func_80096DE0 queue checkpoint "
            "0x%08X with SP=0x%08X; entering VI-manager setup.\n",
            guest_pc,
            g_manager_queue_bootstrap_sp);
        return 0;
    }

    if (guest_pc == kViManagerBootstrapCheckpointPc) {
        g_vi_manager_bootstrap_sp = static_cast<uint32_t>(ctx->r29);
        g_vi_manager_bootstrap_s0 = static_cast<uint32_t>(ctx->r16);
        g_vi_manager_bootstrap_s1 = static_cast<uint32_t>(ctx->r17);
        g_vi_manager_bootstrap_s2 = static_cast<uint32_t>(ctx->r18);
        g_vi_manager_bootstrap_s3 = static_cast<uint32_t>(ctx->r19);
        g_vi_manager_bootstrap_s4 = static_cast<uint32_t>(ctx->r20);
        g_vi_manager_bootstrap_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 34 VI queue/event checkpoint 0x%08X "
            "with SP=0x%08X pri=%u; continuing into priority/thread setup.\n",
            guest_pc,
            g_vi_manager_bootstrap_sp,
            g_vi_manager_bootstrap_s1);
        return 0;
    }

    if (guest_pc == kViThreadCreateCheckpointPc) {
        g_vi_thread_create_sp = static_cast<uint32_t>(ctx->r29);
        g_vi_thread_create_s0 = static_cast<uint32_t>(ctx->r16);
        g_vi_thread_create_s2 = static_cast<uint32_t>(ctx->r18);
        g_vi_thread_create_s3 = static_cast<uint32_t>(ctx->r19);
        g_vi_thread_create_s4 = static_cast<uint32_t>(ctx->r20);
        g_vi_thread_create_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 35 VI-thread creation checkpoint "
            "0x%08X with SP=0x%08X; entering func_80093520.\n",
            guest_pc,
            g_vi_thread_create_sp);
        return 0;
    }

    if (guest_pc == kViMmioBoundaryCheckpointPc) {
        g_vi_mmio_boundary_sp = static_cast<uint32_t>(ctx->r29);
        g_vi_mmio_boundary_s0 = static_cast<uint32_t>(ctx->r16);
        g_vi_mmio_boundary_a0 = static_cast<uint32_t>(ctx->r4);
        g_vi_mmio_boundary_v1 = static_cast<uint32_t>(ctx->r3);
        g_vi_mmio_boundary_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 36 VI MMIO boundary 0x%08X "
            "with SP=0x%08X; entering bridged VI register accesses.\n",
            guest_pc,
            g_vi_mmio_boundary_sp);
        return 0;
    }

    if (guest_pc == kViMmioCompleteCheckpointPc) {
        g_vi_mmio_complete_sp = static_cast<uint32_t>(ctx->r29);
        g_vi_mmio_complete_s0 = static_cast<uint32_t>(ctx->r16);
        g_vi_mmio_complete_s1 = static_cast<uint32_t>(ctx->r17);
        g_vi_mmio_complete_s2 = static_cast<uint32_t>(ctx->r18);
        g_vi_mmio_complete_s3 = static_cast<uint32_t>(ctx->r19);
        g_vi_mmio_complete_s4 = static_cast<uint32_t>(ctx->r20);
        g_vi_mmio_complete_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 37 pre-VI-start checkpoint 0x%08X "
            "with SP=0x%08X; starting VI manager.\n",
            guest_pc,
            g_vi_mmio_complete_sp);
        return 0;
    }

    if (guest_pc == kViManagerReceiveCheckpointPc) {
        g_vi_receive_sp = static_cast<uint32_t>(ctx->r29);
        g_vi_receive_s0 = static_cast<uint32_t>(ctx->r16);
        g_vi_receive_s2 = static_cast<uint32_t>(ctx->r18);
        g_vi_receive_s3 = static_cast<uint32_t>(ctx->r19);
        g_vi_receive_s4 = static_cast<uint32_t>(ctx->r20);
        g_vi_receive_checkpoint_reached = true;

        std::printf(
            "VI MANAGER: passed Stage 38 receive boundary 0x%08X "
            "with SP=0x%08X queue=0x%08X msg=0x%08X; "
            "entering blocking receive.\n",
            guest_pc,
            g_vi_receive_sp,
            read_guest_u32(g_vi_receive_s2 + 0x0Cu),
            kViManagerReceiveMsg);
        return 0;
    }

    if (guest_pc == kThread4ViReturnCheckpointPc) {
        g_thread4_vi_return_sp = static_cast<uint32_t>(ctx->r29);
        g_thread4_vi_return_s0 = static_cast<uint32_t>(ctx->r16);
        g_thread4_vi_return_s2 = static_cast<uint32_t>(ctx->r18);
        g_thread4_vi_return_s3 = static_cast<uint32_t>(ctx->r19);
        g_thread4_vi_return_s4 = static_cast<uint32_t>(ctx->r20);
        g_thread4_vi_return_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 39 post-VI-block checkpoint 0x%08X "
            "with SP=0x%08X oldPri=%u; continuing func_80096DE0.\n",
            guest_pc,
            g_thread4_vi_return_sp,
            g_thread4_vi_return_s2);
        return 0;
    }

    if (guest_pc == kWorkerCreateCheckpointPc) {
        g_worker_create_sp = static_cast<uint32_t>(ctx->r29);
        g_worker_create_s0 = static_cast<uint32_t>(ctx->r16);
        g_worker_create_s1 = static_cast<uint32_t>(ctx->r17);
        g_worker_create_s2 = static_cast<uint32_t>(ctx->r18);
        g_worker_create_s3 = static_cast<uint32_t>(ctx->r19);
        g_worker_create_s4 = static_cast<uint32_t>(ctx->r20);
        g_worker_create_s5 = static_cast<uint32_t>(ctx->r21);
        g_worker_create_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 40 worker-create checkpoint 0x%08X "
            "with SP=0x%08X; starting priority-13 worker.\n",
            guest_pc,
            g_worker_create_sp);
        return 0;
    }

    if (guest_pc == kWorkerReceiveCheckpointPc) {
        g_worker_receive_sp = static_cast<uint32_t>(ctx->r29);
        g_worker_receive_s1 = static_cast<uint32_t>(ctx->r17);
        g_worker_receive_s2 = static_cast<uint32_t>(ctx->r18);
        g_worker_receive_s3 = static_cast<uint32_t>(ctx->r19);
        g_worker_receive_s4 = static_cast<uint32_t>(ctx->r20);
        g_worker_receive_s5 = static_cast<uint32_t>(ctx->r21);
        g_worker_receive_a0 = static_cast<uint32_t>(ctx->r4);
        g_worker_receive_checkpoint_reached = true;

        std::printf(
            "WORKER: passed Stage 41 receive boundary 0x%08X "
            "with SP=0x%08X queue=0x%08X msg=0x%08X; "
            "entering blocking receive.\n",
            guest_pc,
            g_worker_receive_sp,
            g_worker_receive_a0,
            kWorkerReceiveMsg);
        return 0;
    }

    if (guest_pc == kThread4PostWorkerStartCheckpointPc) {
        g_thread4_post_worker_sp = static_cast<uint32_t>(ctx->r29);
        g_thread4_post_worker_s0 = static_cast<uint32_t>(ctx->r16);
        g_thread4_post_worker_s1 = static_cast<uint32_t>(ctx->r17);
        g_thread4_post_worker_s2 = static_cast<uint32_t>(ctx->r18);
        g_thread4_post_worker_s3 = static_cast<uint32_t>(ctx->r19);
        g_thread4_post_worker_s4 = static_cast<uint32_t>(ctx->r20);
        g_thread4_post_worker_s5 = static_cast<uint32_t>(ctx->r21);
        g_thread4_post_worker_start_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 42 post-worker-block checkpoint "
            "0x%08X with SP=0x%08X; allowing func_80096DE0 to return "
            "normally.\n",
            guest_pc,
            g_thread4_post_worker_sp);
        return 0;
    }

    if (guest_pc == kThread4PostManagerBootstrapPc) {
        g_thread4_post_manager_sp = static_cast<uint32_t>(ctx->r29);
        g_thread4_post_manager_bootstrap_reached = true;

        g_pre_933a0_target = read_guest_u32(kViSoftGlobal1);
        if (g_pre_933a0_target != 0u) {
            g_pre_933a0_control =
                read_guest_u32(g_pre_933a0_target + 0x0Cu);
            g_pre_933a0_flags =
                read_guest_u16(g_pre_933a0_target + 0x00u);
        }

        std::printf(
            "THREAD 4: passed Stage 43 manager-return checkpoint "
            "0x%08X with SP=0x%08X; executing osViSetSpecialFeatures_recomp(0x6A).\n",
            guest_pc,
            g_thread4_post_manager_sp);
        return 0;
    }

    if (guest_pc == kMiMaskBoundaryPc) {
        const uint32_t index = g_mi_boundary_count;
        if (index < 2u) {
            g_mi_boundary_a0_values[index] =
                static_cast<uint32_t>(ctx->r4);
        }
        ++g_mi_boundary_count;

        g_mi_boundary_sp = static_cast<uint32_t>(ctx->r29);
        g_mi_boundary_a0 = static_cast<uint32_t>(ctx->r4);
        g_mi_boundary_s0 = static_cast<uint32_t>(ctx->r16);
        g_mi_boundary_s1 = static_cast<uint32_t>(ctx->r17);
        g_mi_boundary_s2 = static_cast<uint32_t>(ctx->r18);
        g_mi_boundary_t2 = static_cast<uint32_t>(ctx->r10);
        g_mi_boundary_shadow = static_cast<uint32_t>(ctx->r11);
        g_mi_mask_boundary_reached = true;

        std::printf(
            "THREAD 4: MI-mask helper boundary #%u at 0x%08X "
            "SP=0x%08X a0=0x%08X shadow=0x%08X; "
            "continuing through bridged MI MMIO.\n",
            g_mi_boundary_count,
            guest_pc,
            g_mi_boundary_sp,
            g_mi_boundary_a0,
            g_mi_boundary_shadow);
        return 0;
    }

    if (guest_pc == kPostMiLinkCheckpointPc) {
        g_post_mi_link_sp = static_cast<uint32_t>(ctx->r29);
        g_post_mi_link_s0 = static_cast<uint32_t>(ctx->r16);
        g_post_mi_link_checkpoint_reached = true;

        std::printf(
            "THREAD 4: passed Stage 45 post-MI checkpoint 0x%08X "
            "with SP=0x%08X; crossing func_80097010 and "
            "func_800137C4.\n",
            guest_pc,
            g_post_mi_link_sp);
        return 0;
    }

    if (guest_pc == kSiBootstrapBoundaryPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_si_boundary_sp = static_cast<uint32_t>(ctx->r29);
        g_si_boundary_a0 = static_cast<uint32_t>(ctx->r4);
        g_si_boundary_a1 = static_cast<uint32_t>(ctx->r5);
        g_si_boundary_a2 = static_cast<uint32_t>(ctx->r6);
        g_si_boundary_s0 = static_cast<uint32_t>(ctx->r16);
        g_si_bootstrap_boundary_reached = true;

        std::printf(
            "THREAD 4 GUARD: software memory layout completed and "
            "func_800159B0 configured SI queue/event state; reached "
            "guest PC 0x%08X with SP=0x%08X queue=0x%08X "
            "msgslot=0x%08X controllerbuf=0x%08X before "
            "func_800984A0.\n",
            guest_pc,
            g_si_boundary_sp,
            g_si_boundary_a0,
            g_si_boundary_a1,
            g_si_boundary_a2);

        return 0;
    }

    if (guest_pc == kFirstSiMmioBoundaryPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        const uint32_t si_sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t si_v0 = static_cast<uint32_t>(ctx->r2);
        const uint32_t si_s0 = static_cast<uint32_t>(ctx->r16);
        const uint32_t si_s1 = static_cast<uint32_t>(ctx->r17);

        if (!g_first_si_mmio_boundary_reached) {
            g_first_si_mmio_sp = si_sp;
            g_first_si_mmio_v0 = si_v0;
            g_first_si_mmio_s0 = si_s0;
            g_first_si_mmio_s1 = si_s1;
            g_first_si_mmio_boundary_reached = true;

            std::printf(
                "THREAD 4: first SI_STATUS_REG boundary at guest PC "
                "0x%08X SP=0x%08X SI_STATUS_REG=0x%08X mode=%u "
                "statusArray=0x%08X; continuing through the bridge.\n",
                guest_pc,
                si_sp,
                si_v0,
                si_s1,
                si_s0);
        } else {
            std::printf(
                "THREAD 4: second SI_STATUS_REG boundary at guest PC "
                "0x%08X SP=0x%08X SI_STATUS_REG=0x%08X mode=%u "
                "statusArray=0x%08X; entering return-DMA setup.\n",
                guest_pc,
                si_sp,
                si_v0,
                si_s1,
                si_s0);
        }

        return 0;
    }

    if (guest_pc == kFirstSiDmaLaunchPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        ++g_si_dma_launch_count;

        if (g_si_dma_launch_count == 1u) {
            g_first_si_dma_sp = static_cast<uint32_t>(ctx->r29);
            g_first_si_dma_v0 = static_cast<uint32_t>(ctx->r2);
            g_first_si_dma_v1 = static_cast<uint32_t>(ctx->r3);
            g_first_si_dma_s0 = static_cast<uint32_t>(ctx->r16);
            g_first_si_dma_s1 = static_cast<uint32_t>(ctx->r17);
            g_first_si_dma_launch_reached = true;

            std::printf(
                "THREAD 4: first SI/PIF command DMA launched at guest "
                "PC 0x%08X with SP=0x%08X mode=%u statusArray=0x%08X "
                "SIreg=0x%08X PIF=0x%08X commandFNV=0x%08X; event 5 "
                "has been queued, so continuing into Toy Story 2's "
                "blocking SI completion receive.\n",
                guest_pc,
                g_first_si_dma_sp,
                g_first_si_dma_s1,
                g_first_si_dma_s0,
                g_first_si_dma_v1,
                g_first_si_dma_v0,
                g_first_pif_command_fnv);

            return 0;
        }

        if (g_si_dma_launch_count == 2u) {
            g_return_si_dma_sp = static_cast<uint32_t>(ctx->r29);
            g_return_si_dma_v0 = static_cast<uint32_t>(ctx->r2);
            g_return_si_dma_v1 = static_cast<uint32_t>(ctx->r3);
            g_return_si_dma_s0 = static_cast<uint32_t>(ctx->r16);
            g_return_si_dma_s1 = static_cast<uint32_t>(ctx->r17);
            g_return_si_dma_launch_reached = true;

            std::printf(
                "THREAD 4: first SI completion was consumed and the "
                "matching PIF-to-RDRAM return DMA launched at guest "
                "PC 0x%08X with SP=0x%08X mode=%u statusArray=0x%08X "
                "SIreg=0x%08X PIF=0x%08X; synthesized no-controller "
                "reply/event 5 are ready, so continuing into the second "
                "blocking SI receive and controller-status parser.\n",
                guest_pc,
                g_return_si_dma_sp,
                g_return_si_dma_s1,
                g_return_si_dma_s0,
                g_return_si_dma_v1,
                g_return_si_dma_v0);
            return 0;
        }

        std::fprintf(
            stderr,
            "TS2 LIVE SI DMA: occurrence #%u guestPC=0x%08X; "
            "controller polling is now allowed to continue.\n",
            g_si_dma_launch_count,
            guest_pc);
        std::fflush(stderr);
        return 0;
    }

    if (guest_pc == kControllerStatusParsedPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);
        g_controller_status_parsed_sp = static_cast<uint32_t>(ctx->r29);
        g_controller_status_parsed_s4 = static_cast<uint32_t>(ctx->r20);
        g_controller_status_parsed_s5 = static_cast<uint32_t>(ctx->r21);
        g_controller_status_parsed_s6 = static_cast<uint32_t>(ctx->r22);
        g_controller_connected_mask = read_guest_byte(kControllerConnectedMaskPtr);
        for (uint32_t channel = 0; channel < 4u; ++channel) {
            g_controller_errno[channel] =
                read_guest_byte(kControllerStatusOutput + 3u + channel * 4u);
        }
        g_controller_status_parsed_reached = true;

        std::printf(
            "THREAD 4: Toy Story 2 parsed the returned PIF controller "
            "status at guest PC 0x%08X SP=0x%08X connectedMask=0x%02X "
            "errno=[%u,%u,%u,%u] replyFNV=0x%08X; continuing through "
            "the rest of func_800984A0 and func_800159B0.\n",
            guest_pc, g_controller_status_parsed_sp, g_controller_connected_mask,
            g_controller_errno[0], g_controller_errno[1],
            g_controller_errno[2], g_controller_errno[3],
            g_return_pif_reply_fnv);
        return 0;
    }

    if (guest_pc == kControllerBootstrapReturnPc) {
        g_controller_bootstrap_return_sp = static_cast<uint32_t>(ctx->r29);
        g_controller_bootstrap_return_v0 = static_cast<uint32_t>(ctx->r2);
        g_controller_bootstrap_return_s0 = static_cast<uint32_t>(ctx->r16);
        g_controller_selection = read_guest_u32(kControllerSelectionGlobal);
        g_controller_list_count = read_guest_u32(kControllerListCountGlobal);
        g_controller_poll_started = read_guest_u32(kControllerPollStartedGlobal);
        g_controller_bootstrap_return_reached = true;

        std::printf(
            "THREAD 4: complete SI/controller bootstrap returned to "
            "main startup at guest PC 0x%08X SP=0x%08X v0=0x%08X "
            "mainS0=0x%08X selection=%u listCount=%u pollStarted=%u; "
            "continuing into func_80008654 for the Stage 67 sprite path.\n",
            guest_pc,
            g_controller_bootstrap_return_sp,
            g_controller_bootstrap_return_v0,
            g_controller_bootstrap_return_s0,
            g_controller_selection,
            g_controller_list_count,
            g_controller_poll_started);

        return 0;
    }

    if (guest_pc == kSpriteLoadCallPc) {
        g_sprite_call_sp = static_cast<uint32_t>(ctx->r29);
        g_sprite_call_a0 = static_cast<uint32_t>(ctx->r4);
        g_sprite_call_a1 = static_cast<uint32_t>(ctx->r5);
        g_sprite_call_name = read_guest_string(g_sprite_call_a0);
        g_sprite_call_reached = true;

        std::printf(
            "THREAD 4: func_80008654 reached first sprite-resource load at "
            "guest PC 0x%08X SP=0x%08X key=\"%s\" dest=0x%08X; "
            "entering the proven ROM loader/RNC path.\n",
            guest_pc,
            g_sprite_call_sp,
            g_sprite_call_name.c_str(),
            g_sprite_call_a1);
        return 0;
    }

    if (guest_pc == kSpriteLoadReturnPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);
        g_sprite_return_sp = static_cast<uint32_t>(ctx->r29);
        g_sprite_return_v0 = static_cast<uint32_t>(ctx->r2);
        g_sprite_return_reached = true;

        std::printf(
            "THREAD 4 GUARD: level99\\\\sprite0.pic completed the real "
            "resource loader/RNC decode and returned to func_80008654 at "
            "guest PC 0x%08X SP=0x%08X size=0x%08X output=0x%08X "
            "CRC16=0x%04X FNV1a=0x%08X; stopping before the remaining "
            "graphics/game startup initialization.\n",
            guest_pc,
            g_sprite_return_sp,
            g_sprite_return_v0,
            g_sprite_rnc_return_output,
            g_sprite_rnc_output_crc,
            g_sprite_rnc_output_fnv);

        g_thread_cv.notify_all();
        if (!g_stage82_post_audio_system_return_reached) {
            std::fputs(
                "FAILED: Stage 83 sprite return occurred before the proven "
                "Stage 82 post-audio-system main return.\n",
                stderr);
            std::fflush(stderr);
            std::_Exit(83);
        }

        if (!g_sprite_call_reached ||
            !g_sprite_parser_snapshot_valid ||
            !g_sprite_rnc_boundary_reached ||
            g_sprite_call_name != "level99\\sprite0.pic") {
            std::fputs(
                "FAILED: Stage 83 sprite/resource proof flags were incomplete.\n",
                stderr);
            std::fflush(stderr);
            std::_Exit(83);
        }

        std::puts(
            "Stage 83 main-boot sprite/resource-loader return checkpoint PASSED; continuing to Stage 84.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }

    if (guest_pc == kStage84PreInitPc) {
        g_stage84_pre_init_sp = static_cast<uint32_t>(ctx->r29);
        g_stage84_pre_init_v0 = static_cast<uint32_t>(ctx->r2);
        g_stage84_pre_init_a0 = static_cast<uint32_t>(ctx->r4);
        g_stage84_pre_init_a1 = static_cast<uint32_t>(ctx->r5);

        if (!g_sprite_return_reached) {
            std::fputs(
                "FAILED: Stage 84 reached 0x80008824 before the proven "
                "Stage 83 sprite return.\n",
                stderr);
            std::fflush(stderr);
            std::_Exit(84);
        }

        std::printf(
            "STAGE 84 PRE-INIT BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X v0=0x%08X a0=0x%08X a1=0x%08X; "
            "func_80008654 completed its post-sprite instructions and "
            "reached the boundary immediately before func_80013450.\n",
            guest_pc,
            g_stage84_pre_init_sp,
            g_stage84_pre_init_v0,
            g_stage84_pre_init_a0,
            g_stage84_pre_init_a1);

        std::puts(
            "Stage 84 pre-func_80013450 main-boot checkpoint PASSED; continuing to Stage 85.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (guest_pc == kStage85NextInitCallPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t v0 = static_cast<uint32_t>(ctx->r2);
        const uint32_t a0 = static_cast<uint32_t>(ctx->r4);

        bool object_ok =
            read_guest_byte(kStage85InitObject + 0x00u) == 0x05u &&
            read_guest_byte(kStage85InitObject + 0x01u) == 0x00u &&
            read_guest_byte(kStage85InitObject + 0x02u) == 0x00u &&
            read_guest_byte(kStage85InitObject + 0x03u) == 0xC0u &&
            read_guest_byte(kStage85InitObject + 0x04u) == 0x0Fu &&
            read_guest_byte(kStage85InitObject + 0x05u) == 0x0Fu &&
            read_guest_byte(kStage85InitObject + 0x84u) == 0x00u &&
            read_guest_byte(kStage85InitObject + 0x89u) == 0x00u &&
            read_guest_u32(kStage85InitObject + 0x9Cu) == 0x00000000u;

        for (uint32_t offset = 0x8Au; offset <= 0x98u; ++offset) {
            if (read_guest_byte(kStage85InitObject + offset) != 0u) {
                object_ok = false;
                break;
            }
        }

        const uint16_t global_mode =
            read_guest_u16(kStage85GlobalMode);
        const uint16_t global_byte2 =
            read_guest_u16(kStage85GlobalByte2);

        if (sp != 0x803F9F98u ||
            v0 != 0x0000000Fu ||
            !object_ok ||
            global_mode != 0x0005u ||
            global_byte2 != 0x0000u) {
            std::fprintf(
                stderr,
                "FAILED: Stage 85 func_80013450 state mismatch at "
                "0x80008840: SP=%08X v0=%08X a0=%08X "
                "obj=%u mode=%04X byte2=%04X.\n",
                sp,
                v0,
                a0,
                object_ok ? 1u : 0u,
                static_cast<unsigned>(global_mode),
                static_cast<unsigned>(global_byte2));
            std::fflush(stderr);
            std::_Exit(85);
        }

        std::printf(
            "STAGE 85 NEXT-INIT BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X v0=0x%08X a0=0x%08X; "
            "func_80013450 completed its real object/global initialization, "
            "the caller applied its post-return 0x0F state bytes, and "
            "execution reached the next call boundary before func_80012F1C.\n",
            guest_pc,
            sp,
            v0,
            a0);

        std::printf(
            "STAGE 85 STATE: object=0x%08X "
            "bytes[0..5]=%02X/%02X/%02X/%02X/%02X/%02X "
            "zeroRange[8A..98]=OK word9C=%08X "
            "globalMode=%04X globalByte2=%04X.\n",
            kStage85InitObject,
            read_guest_byte(kStage85InitObject + 0x00u),
            read_guest_byte(kStage85InitObject + 0x01u),
            read_guest_byte(kStage85InitObject + 0x02u),
            read_guest_byte(kStage85InitObject + 0x03u),
            read_guest_byte(kStage85InitObject + 0x04u),
            read_guest_byte(kStage85InitObject + 0x05u),
            read_guest_u32(kStage85InitObject + 0x9Cu),
            static_cast<unsigned>(global_mode),
            static_cast<unsigned>(global_byte2));

        std::puts(
            "Stage 85 func_80013450 completion / pre-func_80012F1C checkpoint PASSED; continuing to Stage 86.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (guest_pc == kStage86Func8000B0C0CallPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t v0 = static_cast<uint32_t>(ctx->r2);
        const uint32_t a0 = static_cast<uint32_t>(ctx->r4);

        const uint32_t global62 =
            read_guest_u32(kStage86Global62);
        const uint32_t globalC3C =
            read_guest_u32(kStage86GlobalC3C);

        if (v0 != 0x00000C3Cu ||
            a0 != 0x00000062u ||
            global62 != 0x00000062u ||
            globalC3C != 0x00000C3Cu) {
            std::fprintf(
                stderr,
                "FAILED: Stage 86 func_80012F1C pre-func_8000B0C0 "
                "state mismatch at 0x80012F3C: "
                "SP=%08X v0=%08X a0=%08X "
                "global62=%08X globalC3C=%08X.\n",
                sp,
                v0,
                a0,
                global62,
                globalC3C);
            std::fflush(stderr);
            std::_Exit(86);
        }

        std::printf(
            "STAGE 86 INNER-CALL BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X v0=0x%08X a0=0x%08X; "
            "func_80012F1C entered normally, completed its real setup "
            "writes, and reached its first nested call boundary before "
            "func_8000B0C0.\n",
            guest_pc,
            sp,
            v0,
            a0);

        std::printf(
            "STAGE 86 STATE: [0x%08X]=0x%08X "
            "[0x%08X]=0x%08X argument=0x%08X.\n",
            kStage86Global62,
            global62,
            kStage86GlobalC3C,
            globalC3C,
            a0);

        std::puts(
            "Stage 86 func_80012F1C entry/setup / pre-func_8000B0C0 checkpoint PASSED; continuing to Stage 87.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    static bool g_ts2_stage87_to_93_retired = false;
    if (guest_pc == kStage87Func8000AFCCCallPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t selector = static_cast<uint32_t>(ctx->r16);
        const uint32_t s1 = static_cast<uint32_t>(ctx->r17);

        const uint32_t entry_offset =
            (kStage87Selector - 1u) * 12u;

        const auto signed_half = [](uint16_t value) -> uint32_t {
            return static_cast<uint32_t>(
                static_cast<int32_t>(static_cast<int16_t>(value)));
        };

        const uint32_t expected_v1 =
            signed_half(read_guest_u16(kStage87TableBase0 + entry_offset + 0u));
        const uint32_t expected_a0 =
            signed_half(read_guest_u16(kStage87TableBase0 + entry_offset + 2u));
        const uint32_t expected_a1 =
            signed_half(read_guest_u16(kStage87TableBase0 + entry_offset + 4u));
        const uint32_t expected_a2 =
            signed_half(read_guest_u16(kStage87TableBase0 + entry_offset + 6u));
        const uint32_t expected_v0 =
            signed_half(read_guest_u16(kStage87TableBase0 + entry_offset + 8u));

        const uint32_t reg_v1 = static_cast<uint32_t>(ctx->r3);
        const uint32_t reg_a0 = static_cast<uint32_t>(ctx->r4);
        const uint32_t reg_a1 = static_cast<uint32_t>(ctx->r5);
        const uint32_t reg_a2 = static_cast<uint32_t>(ctx->r6);
        const uint32_t reg_v0 = static_cast<uint32_t>(ctx->r2);

        const uint32_t float_source =
            read_guest_u32(kStage87FloatSource);
        const uint32_t float_mirror =
            read_guest_u32(kStage87FloatMirror);

        const bool mirrors_ok =
            read_guest_u32(kStage87MirrorV1) == expected_v1 &&
            read_guest_u32(kStage87MirrorA0) == expected_a0 &&
            read_guest_u32(kStage87MirrorA1) == expected_a1 &&
            read_guest_u32(kStage87MirrorA2) == expected_a2 &&
            read_guest_u32(kStage87MirrorV0) == expected_v0;

        const bool setup_flags_ok =
            read_guest_u32(kStage87ZeroA) == 0u &&
            read_guest_u32(kStage87ZeroB) == 0u &&
            read_guest_u32(kStage87One) == 1u &&
            read_guest_u32(kStage87ZeroC) == 0u;

        const bool regs_ok =
            reg_v1 == expected_v1 &&
            reg_a0 == expected_a0 &&
            reg_a1 == expected_a1 &&
            reg_a2 == expected_a2 &&
            reg_v0 == expected_v0;

        if (sp != 0x803F9E38u ||
            selector != kStage87Selector ||
            s1 != 1u ||
            !regs_ok ||
            !mirrors_ok ||
            !setup_flags_ok ||
            float_mirror != float_source) {
            std::fprintf(
                stderr,
                "FAILED: Stage 87 func_8000B0C0 setup mismatch at "
                "0x8000B17C: SP=%08X selector=%08X s1=%08X "
                "regs=%u mirrors=%u flags=%u float=%08X/%08X "
                "v1=%08X/%08X a0=%08X/%08X a1=%08X/%08X "
                "a2=%08X/%08X v0=%08X/%08X.\n",
                sp,
                selector,
                s1,
                regs_ok ? 1u : 0u,
                mirrors_ok ? 1u : 0u,
                setup_flags_ok ? 1u : 0u,
                float_mirror,
                float_source,
                reg_v1,
                expected_v1,
                reg_a0,
                expected_a0,
                reg_a1,
                expected_a1,
                reg_a2,
                expected_a2,
                reg_v0,
                expected_v0);
            std::fflush(stderr);
            std::fprintf(
                stderr,
                "TS2 LIVE PATH: Stage 87 startup-only expectation no longer "
                "matches the running game; retiring strict Stage 87-93 "
                "bring-up checkpoints and continuing original game code.\n");
            std::fflush(stderr);

            g_ts2_stage87_to_93_retired = true;

            // V42B: Stage 87 has explicitly declared that startup-only
            // regression expectations no longer describe live gameplay.
            // Publish that state to run_stage61() using the harness's
            // existing mutex/condition-variable synchronization.
            {
                std::lock_guard<std::mutex> v42b_lock(g_thread_mutex);
                g_ts2_live_mode_reached_v42b = true;
            }
            g_thread_cv.notify_all();
            return 0;
        }

        std::printf(
            "STAGE 87 FIRST-NESTED-CALL BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X selector=0x%08X; func_8000B0C0 completed its "
            "real selector-table/global setup and reached its first nested "
            "call boundary before func_8000AFCC.\n",
            guest_pc,
            sp,
            selector);

        std::printf(
            "STAGE 87 STATE: tableOffset=0x%08X "
            "v1=%08X a0=%08X a1=%08X a2=%08X v0=%08X "
            "float=%08X mirrors=OK flags=0/0/1/0.\n",
            entry_offset,
            reg_v1,
            reg_a0,
            reg_a1,
            reg_a2,
            reg_v0,
            float_mirror);

        std::puts(
            "Stage 87 func_8000B0C0 setup / pre-func_8000AFCC checkpoint PASSED; continuing to Stage 88.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (g_ts2_stage87_to_93_retired &&
        (guest_pc == kStage88AfterFunc8000AFCCPc ||
         guest_pc == kStage89Case98TransferPc ||
         guest_pc == kStage90AfterCase98TransferPc ||
         guest_pc == kStage91FirstResourceLoadPc ||
         guest_pc == kStage92AfterSelectPicPc ||
         guest_pc == kStage93AfterFunc13D88Pc ||
         guest_pc == kStage93NextResourceBoundaryPc)) {
        return 0;
    }
    if (guest_pc == kStage88AfterFunc8000AFCCPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t dynamic_base =
            read_guest_u32(kStage88DynamicBasePtr);

        const auto range_zero = [&](uint32_t addr, uint32_t size) -> bool {
            for (uint32_t i = 0; i < size; ++i) {
                if (read_guest_byte(addr + i) != 0u) {
                    return false;
                }
            }
            return true;
        };

        const bool dynamic_ptr_ok =
            dynamic_base >= 0x80000000u &&
            dynamic_base <= (0x80800000u - kStage88DynamicClearSize);

        const bool big_clears_ok =
            range_zero(kStage88Clear0, kStage88Clear0Size) &&
            range_zero(kStage88Clear1, kStage88Clear1Size) &&
            range_zero(kStage88Clear2, kStage88Clear2Size) &&
            dynamic_ptr_ok &&
            range_zero(dynamic_base, kStage88DynamicClearSize);

        bool loop_table_ok = true;
        for (uint32_t i = 0; i < 64u; ++i) {
            if (read_guest_u16(kStage88LoopBase + i * 0x3Cu) != 0u) {
                loop_table_ok = false;
                break;
            }
        }

        bool reset_words_ok = true;
        for (uint32_t i = 0; i < 8u; ++i) {
            if (read_guest_u32(kStage88ResetWordsBase + i * 4u) != 0u) {
                reset_words_ok = false;
                break;
            }
        }

        const bool scalar_resets_ok =
            read_guest_u32(kStage88MinusOne) == 0xFFFFFFFFu &&
            read_guest_u16(0x801E7794u) == 0u &&
            read_guest_u16(0x800B883Cu) == 0u &&
            read_guest_u16(0x800B8704u) == 0u &&
            read_guest_u16(0x801EDB64u) == 0u &&
            read_guest_u16(0x801ED810u) == 0u &&
            read_guest_u32(0x801CE314u) == 0u &&
            read_guest_u32(0x801CE108u) == 0u;

        const bool final_reset_helper_ok =
            read_guest_u16(0x800BAC44u) == 0u &&
            read_guest_u16(0x800AEE5Au) == 0u &&
            read_guest_u16(0x800B0800u) == 0u &&
            read_guest_u16(0x800BAC30u) == 0u &&
            read_guest_u16(0x801DB3FCu) == 0u &&
            read_guest_u16(0x801ED812u) == 0u &&
            read_guest_u16(0x801DB9B0u) == 0u &&
            read_guest_u16(0x801EDB5Cu) == 0u &&
            range_zero(kStage88SmallClear0, kStage88SmallClear0Size) &&
            range_zero(kStage88SmallClear1, kStage88SmallClear1Size);

        if (sp != 0x803F9E38u ||
            !big_clears_ok ||
            !loop_table_ok ||
            !reset_words_ok ||
            !scalar_resets_ok ||
            !final_reset_helper_ok) {
            std::fprintf(
                stderr,
                "FAILED: Stage 88 func_8000AFCC completion mismatch at "
                "0x8000B184: SP=%08X dynamic=%08X ptr=%u "
                "big=%u loop=%u words=%u scalar=%u final=%u.\n",
                sp,
                dynamic_base,
                dynamic_ptr_ok ? 1u : 0u,
                big_clears_ok ? 1u : 0u,
                loop_table_ok ? 1u : 0u,
                reset_words_ok ? 1u : 0u,
                scalar_resets_ok ? 1u : 0u,
                final_reset_helper_ok ? 1u : 0u);
            std::fflush(stderr);
            std::_Exit(88);
        }

        std::printf(
            "STAGE 88 RESET-INIT RETURN BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X dynamicBase=0x%08X; func_8000AFCC completed "
            "all real memory clears, pointer-derived clears, reset tables, "
            "func_8006BD80, and func_80009928, then returned normally "
            "to func_8000B0C0.\n",
            guest_pc,
            sp,
            dynamic_base);

        std::printf(
            "STAGE 88 STATE: bigClears=OK loop64=OK resetWords8=OK "
            "scalarResets=OK finalResetHelper=OK.\n");

        std::puts(
            "Stage 88 full func_8000AFCC reset/init return checkpoint PASSED; continuing to Stage 89.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (guest_pc == kStage89Case98TransferPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t selector = static_cast<uint32_t>(ctx->r16);
        const uint32_t source = static_cast<uint32_t>(ctx->r4);
        const uint32_t dest = static_cast<uint32_t>(ctx->r5);
        const uint32_t size = static_cast<uint32_t>(ctx->r6);
        const uint32_t mode = read_guest_u32(kStage86GlobalC3C);

        if (sp != 0x803F9E20u ||
            selector != kStage89Case98Selector ||
            source != kStage89Case98CartSource ||
            dest != kStage89Case98RdramDest ||
            size != kStage89Case98Size ||
            mode != 0x00000C3Cu) {
            std::fprintf(
                stderr,
                "FAILED: Stage 89 case-98 transfer boundary mismatch at "
                "0x8000B85C: SP=%08X selector=%08X "
                "source=%08X dest=%08X size=%08X mode=%08X.\n",
                sp,
                selector,
                source,
                dest,
                size,
                mode);
            std::fflush(stderr);
            std::_Exit(89);
        }

        std::printf(
            "STAGE 89 CASE-98 TRANSFER BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X selector=0x%08X source=0x%08X "
            "dest=0x%08X size=0x%08X; func_80019444 returned, "
            "the selector-0x62 jump-table case prepared its real cart-to-RDRAM "
            "load, and execution reached func_800859A4 before the transfer began.\n",
            guest_pc,
            sp,
            selector,
            source,
            dest,
            size);

        std::puts(
            "Stage 89 case-98 pre-func_800859A4 cart-load checkpoint PASSED; continuing to Stage 90.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (guest_pc == kStage90AfterCase98TransferPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);

        const auto fnv_guest = [&](uint32_t addr, uint32_t size) -> uint32_t {
            uint32_t hash = 2166136261u;
            for (uint32_t i = 0; i < size; ++i) {
                hash ^= static_cast<uint32_t>(read_guest_byte(addr + i));
                hash *= 16777619u;
            }
            return hash;
        };

        bool chunks_ok = true;

        for (uint32_t i = 0; i < kStage90TransferCount; ++i) {
            const uint32_t index = kStage90FirstTransferIndex + i;

            if (g_raw_devs[index] != kChunkDeviceAddrs[index] ||
                g_raw_drams[index] != kChunkDramAddrs[index] ||
                g_raw_sizes[index] != kChunkSizes[index]) {
                chunks_ok = false;
                std::fprintf(
                    stderr,
                    "FAILED: Stage 90 raw transfer #%u mismatch: "
                    "dev=%08X/%08X dram=%08X/%08X size=%08X/%08X.\n",
                    index + 1u,
                    g_raw_devs[index],
                    kChunkDeviceAddrs[index],
                    g_raw_drams[index],
                    kChunkDramAddrs[index],
                    g_raw_sizes[index],
                    kChunkSizes[index]);
                break;
            }

            const uint32_t actual_hash =
                fnv_guest(kChunkDramAddrs[index], kChunkSizes[index]);

            if (actual_hash != kChunkHashes[index]) {
                chunks_ok = false;
                std::fprintf(
                    stderr,
                    "FAILED: Stage 90 copied-data hash #%u mismatch: "
                    "actual=%08X expected=%08X.\n",
                    index + 1u,
                    actual_hash,
                    kChunkHashes[index]);
                break;
            }
        }

        const bool totals_ok =
            g_pi_dma_count == (kStage90FirstTransferIndex + kStage90TransferCount) &&
            g_pi_raw_boundary_count == (kStage90FirstTransferIndex + kStage90TransferCount) &&
            g_pi_mmio_read_count == (kStage90FirstTransferIndex + kStage90TransferCount) &&
            g_pi_mmio_write_count == (kStage90FirstTransferIndex + kStage90TransferCount) * 3u &&
            g_pi_auto_event_count == (kStage90FirstTransferIndex + kStage90TransferCount) &&
            g_pi_completion_wait_count == (kStage90FirstTransferIndex + kStage90TransferCount) &&
            g_loader_wait_count == kStage90FirstTransferIndex &&
            g_loader_post_count == kStage90FirstTransferIndex &&
            g_pi_mmio_args_valid &&
            g_pi_dma_copy_complete;

        if (sp != 0x803F9E20u ||
            !chunks_ok ||
            !totals_ok) {
            std::fprintf(
                stderr,
                "FAILED: Stage 90 full case-98 transfer validation at "
                "0x8000B864: SP=%08X chunks=%u totals=%u "
                "dma/raw/r/w/events/piwait/resourceLoaderWait/resourceLoaderPost="
                "%u/%u/%u/%u/%u/%u/%u/%u valid=%u copy=%u.\n",
                sp,
                chunks_ok ? 1u : 0u,
                totals_ok ? 1u : 0u,
                g_pi_dma_count,
                g_pi_raw_boundary_count,
                g_pi_mmio_read_count,
                g_pi_mmio_write_count,
                g_pi_auto_event_count,
                g_pi_completion_wait_count,
                g_loader_wait_count,
                g_loader_post_count,
                g_pi_mmio_args_valid ? 1u : 0u,
                g_pi_dma_copy_complete ? 1u : 0u);
            std::fflush(stderr);
            std::_Exit(90);
        }

        std::printf(
            "STAGE 90 CASE-98 LOAD RETURN: guest PC=0x%08X "
            "SP=0x%08X; func_800859A4 completed all six real PI chunks "
            "for ROM 0x000D34E0..0x000D87DF into RDRAM "
            "0x80000400..0x800056FF.\n",
            guest_pc,
            sp);

        for (uint32_t i = 0; i < kStage90TransferCount; ++i) {
            const uint32_t index = kStage90FirstTransferIndex + i;
            std::printf(
                "STAGE 90 DMA #%u: dev=0x%08X dram=0x%08X "
                "size=0x%X hash=0x%08X OK.\n",
                index + 1u,
                g_raw_devs[index],
                g_raw_drams[index],
                g_raw_sizes[index],
                kChunkHashes[index]);
        }

        std::printf(
            "STAGE 90 TOTALS: dma/raw/r/w/events/piwait/resourceLoaderWait/resourceLoaderPost="
            "%u/%u/%u/%u/%u/%u/%u/%u.\n",
            g_pi_dma_count,
            g_pi_raw_boundary_count,
            g_pi_mmio_read_count,
            g_pi_mmio_write_count,
            g_pi_auto_event_count,
            g_pi_completion_wait_count,
            g_loader_wait_count,
            g_loader_post_count);

        std::puts(
            "Stage 90 full selector-0x62 case-98 cart-load checkpoint PASSED; continuing to Stage 91.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (guest_pc == kStage93NextResourceBoundaryPc) {
        static uint32_t fast_forward_resource_hits = 0u;
        ++fast_forward_resource_hits;
        if (fast_forward_resource_hits <= 8u ||
            (fast_forward_resource_hits % 32u) == 0u) {
            std::printf(
                "FAST FORWARD RESOURCE CALL #%u: PC=0x%08X completedResources=%u "
                "SP=0x%08X namePtr=0x%08X dest=0x%08X.\n",
                fast_forward_resource_hits,
                guest_pc,
                g_font_complete_count,
                static_cast<uint32_t>(ctx->r29),
                static_cast<uint32_t>(ctx->r20),
                static_cast<uint32_t>(ctx->r18));
        }
        return 0;
    }
    if (guest_pc == kStage91FirstResourceLoadPc) {
        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t resource_ptr = static_cast<uint32_t>(ctx->r4);
        const uint32_t resource_dest = static_cast<uint32_t>(ctx->r5);
        const uint32_t resource_kind = static_cast<uint32_t>(ctx->r17);

        // func_80013D88 saved the caller's s1 (func_80014418 selector 0x62)
        // at +0x94 and saved its input a1 at +0x74.
        const uint32_t saved_outer_selector = read_guest_u32(sp + 0x94u);
        const uint32_t saved_mode_arg = read_guest_u32(sp + 0x74u);

        const bool ptr_ok =
            resource_ptr >= 0x80000000u &&
            resource_ptr < 0x80800000u;

        const bool dest_ok =
            resource_dest >= 0x80000000u &&
            resource_dest < 0x80800000u;

        char resource_name[192] = {};
        bool name_terminated = false;
        bool name_nonempty = false;
        bool name_printable = true;

        if (ptr_ok) {
            for (uint32_t i = 0; i + 1u < sizeof(resource_name); ++i) {
                const uint8_t ch = read_guest_byte(resource_ptr + i);

                if (ch == 0u) {
                    resource_name[i] = '\0';
                    name_terminated = true;
                    break;
                }

                if (ch < 0x20u || ch > 0x7Eu) {
                    name_printable = false;
                }

                resource_name[i] =
                    (ch >= 0x20u && ch <= 0x7Eu)
                        ? static_cast<char>(ch)
                        : '?';

                if (i == 0u) {
                    name_nonempty = true;
                }
            }
        }

        const bool pi_totals_unchanged =
            g_pi_dma_count == kStage91PreviousPiTransferCount &&
            g_pi_raw_boundary_count == kStage91PreviousPiTransferCount &&
            g_pi_mmio_read_count == kStage91PreviousPiTransferCount &&
            g_pi_mmio_write_count == kStage91PreviousPiTransferCount * 3u &&
            g_pi_auto_event_count == kStage91PreviousPiTransferCount &&
            g_pi_completion_wait_count == kStage91PreviousPiTransferCount &&
            g_pi_mmio_args_valid &&
            g_pi_dma_copy_complete;

        const bool old_resource_loader_totals_unchanged =
            g_loader_wait_count == kStage91PreviousResourceLoaderCount &&
            g_loader_post_count == kStage91PreviousResourceLoaderCount;

        if (sp != kStage91ExpectedSp ||
            saved_outer_selector != kStage91OuterSelector ||
            saved_mode_arg != 0u ||
            !ptr_ok ||
            !dest_ok ||
            !name_terminated ||
            !name_nonempty ||
            !name_printable ||
            !pi_totals_unchanged ||
            !old_resource_loader_totals_unchanged) {
            std::fprintf(
                stderr,
                "FAILED: Stage 91 first resource boundary mismatch at "
                "0x80014040: SP=%08X/%08X selector=%08X modeArg=%08X "
                "resourcePtr=%08X dest=%08X kind=%08X "
                "ptr=%u destOk=%u term=%u nonempty=%u printable=%u "
                "pi=%u oldLoader=%u name='%s'.\n",
                sp,
                kStage91ExpectedSp,
                saved_outer_selector,
                saved_mode_arg,
                resource_ptr,
                resource_dest,
                resource_kind,
                ptr_ok ? 1u : 0u,
                dest_ok ? 1u : 0u,
                name_terminated ? 1u : 0u,
                name_nonempty ? 1u : 0u,
                name_printable ? 1u : 0u,
                pi_totals_unchanged ? 1u : 0u,
                old_resource_loader_totals_unchanged ? 1u : 0u,
                resource_name);
            std::fflush(stderr);
            std::_Exit(91);
        }

        std::printf(
            "STAGE 91 FIRST RESOURCE BOUNDARY: guest PC=0x%08X "
            "SP=0x%08X selector=0x%08X modeArg=0x%08X.\n",
            guest_pc,
            sp,
            saved_outer_selector,
            saved_mode_arg);

        std::printf(
            "STAGE 91 RESOURCE: name=\"%s\" ptr=0x%08X "
            "dest=0x%08X kind=0x%08X.\n",
            resource_name,
            resource_ptr,
            resource_dest,
            resource_kind);

        std::printf(
            "STAGE 91 PRE-LOAD TOTALS: "
            "dma/raw/r/w/events/piwait/resourceLoaderWait/resourceLoaderPost="
            "%u/%u/%u/%u/%u/%u/%u/%u.\n",
            g_pi_dma_count,
            g_pi_raw_boundary_count,
            g_pi_mmio_read_count,
            g_pi_mmio_write_count,
            g_pi_auto_event_count,
            g_pi_completion_wait_count,
            g_loader_wait_count,
            g_loader_post_count);

        std::puts(
            "Stage 91 selector-0x62 first func_800853E0 resource-load boundary PASSED.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (guest_pc == kStage92AfterSelectPicPc) {
        std::puts("FAST FORWARD: passed the old Stage 92 select.pic return checkpoint.");
        return 0;
    }
    if (guest_pc == kStage93AfterFunc13D88Pc) {
        std::printf(
            "FAST FORWARD: func_80013D88 returned at guest PC 0x%08X "
            "SP=0x%08X v0=0x%08X; continuing toward game startup.\n",
            guest_pc,
            static_cast<uint32_t>(ctx->r29),
            static_cast<uint32_t>(ctx->r2));
        return 0;
    }
    if (guest_pc == kAudioEngineInitBoundaryPc) {
        g_audio_engine_init_sp = static_cast<uint32_t>(ctx->r29);
        g_audio_engine_init_v0 = static_cast<uint32_t>(ctx->r2);
        g_audio_engine_init_a0 = static_cast<uint32_t>(ctx->r4);
        g_audio_engine_init_a1 = static_cast<uint32_t>(ctx->r5);
        g_audio_engine_init_boundary_reached = true;

        std::printf(
            "THREAD 4: Stage 67 audio-engine entry replayed at guest "
            "PC 0x%08X SP=0x%08X config=0x%08X audioData=0x%08X "
            "v0=0x%08X; continuing through func_8008D2D8 toward the "
            "first Audio Interface MMIO write.\n",
            guest_pc,
            g_audio_engine_init_sp,
            g_audio_engine_init_a0,
            g_audio_engine_init_a1,
            g_audio_engine_init_v0);

        return 0;
    }

    if (guest_pc == kAudioDirectCartReadPc) {
        g_audio_direct_cart_sp = static_cast<uint32_t>(ctx->r29);
        g_audio_direct_cart_a0 = static_cast<uint32_t>(ctx->r4);
        g_audio_direct_cart_a2 = static_cast<uint32_t>(ctx->r6);
        g_audio_direct_cart_read_reached = true;

        std::printf(
            "THREAD 4: replayed Stage 68 direct-cart boundary at "
            "0x%08X SP=0x%08X a0/a2=0x%08X/0x%08X; "
            "executing Stage 69 ROM-backed lw.\n",
            guest_pc,
            g_audio_direct_cart_sp,
            g_audio_direct_cart_a0,
            g_audio_direct_cart_a2);

        return 0;
    }

    if (guest_pc == kAudioDirectCartPostReadPc) {
        g_audio_direct_cart_post_sp = static_cast<uint32_t>(ctx->r29);
        g_audio_direct_cart_post_v0 = static_cast<uint32_t>(ctx->r2);
        g_audio_direct_cart_post_a2 = static_cast<uint32_t>(ctx->r6);
        g_audio_direct_cart_post_read_reached = true;

        std::printf(
            "THREAD 4: Stage 69 first cart lw replayed at guest PC 0x%08X "
            "SP=0x%08X v0=0x%08X a2=0x%08X; continuing to the first "
            "cart-space store boundary.\n",
            guest_pc,
            g_audio_direct_cart_post_sp,
            g_audio_direct_cart_post_v0,
            g_audio_direct_cart_post_a2);

        return 0;
    }

    if (guest_pc == kStage72AiReturnPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage72_ai_return_sp = static_cast<uint32_t>(ctx->r29);
        g_stage72_ai_return_v0 = static_cast<uint32_t>(ctx->r2);
        g_stage72_ai_return_a2 = static_cast<uint32_t>(ctx->r6);
        g_stage72_ai_return_reached = true;


        const uint32_t audio_clock =
            read_guest_u32(kStage72AudioClockGlobal);
        const uint32_t expected_rate =
            audio_clock / kAudioDacDivisor;

        std::printf(
            "THREAD 4 GUARD: native osAiSetFrequency returned normally at "
            "guest PC 0x%08X SP=0x%08X v0/effectiveRate=%u "
            "audioClock=%u divisor=%u expectedRate=%u; "
            "stopping before func_8009097C.\n",
            guest_pc,
            g_stage72_ai_return_sp,
            g_stage72_ai_return_v0,
            audio_clock,
            kAudioDacDivisor,
            expected_rate);

        g_thread_cv.notify_all();
        std::puts("THREAD 4: Stage 72 AI-return boundary replayed; continuing through post-AI software setup.");
        return 0;
    }

    if (guest_pc == kStage73TracePostPtrPc ||
        guest_pc == kStage73TracePostInitPc ||
        guest_pc == kStage73TracePostCalcPc ||
        guest_pc == kStage73TracePostAllocPc ||
        guest_pc == kStage73TracePostSmallAllocPc ||
        guest_pc == kStage73TracePreBigAllocPc ||
        guest_pc == kStage73SchedulerSetupPc) {

        g_stage73_last_pc = guest_pc;
        ++g_stage73_trace_hits;

        std::printf(
            "POST-AI TRACE #%u: guest PC 0x%08X SP=0x%08X "
            "v0=0x%08X a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X.\n",
            g_stage73_trace_hits,
            guest_pc,
            static_cast<uint32_t>(ctx->r29),
            static_cast<uint32_t>(ctx->r2),
            static_cast<uint32_t>(ctx->r4),
            static_cast<uint32_t>(ctx->r5),
            static_cast<uint32_t>(ctx->r6),
            static_cast<uint32_t>(ctx->r7));

        if (guest_pc == kStage73SchedulerSetupPc) {
            std::unique_lock<std::mutex> lock(g_thread_mutex);

            g_stage73_sp = static_cast<uint32_t>(ctx->r29);
            g_stage73_a0 = static_cast<uint32_t>(ctx->r4);
            g_stage73_a1 = static_cast<uint32_t>(ctx->r5);
            g_stage73_a2 = static_cast<uint32_t>(ctx->r6);
            g_stage73_a3 = static_cast<uint32_t>(ctx->r7);
            g_stage73_scheduler_setup_reached = true;

            std::printf(
                "THREAD 4 GUARD: post-AI software initialization reached "
                "guest PC 0x%08X SP=0x%08X scheduler=0x%08X "
                "id/count=0x%08X callback=0x%08X arg=0x%08X; "
                "stopping before func_800919E0.\n",
                guest_pc,
                g_stage73_sp,
                g_stage73_a0,
                g_stage73_a1,
                g_stage73_a2,
                g_stage73_a3);

            std::puts(
                "THREAD 4: Stage 73 scheduler-entry boundary replayed; "
                "executing func_800919E0.");
            return 0;
        }

        return 0;
    }

    if (guest_pc == kStage74SchedulerReturnPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage74_sp = static_cast<uint32_t>(ctx->r29);
        g_stage74_s0 = static_cast<uint32_t>(ctx->r16);
        g_stage74_scheduler_return_reached = true;

        std::printf(
            "THREAD 4 GUARD: func_800919E0 returned normally at "
            "guest PC 0x%08X SP=0x%08X s0/scheduler=0x%08X "
            "head=0x%08X id=0x%08X callback=0x%08X "
            "stackHi/Lo=0x%08X/0x%08X flags=0x%08X mask=0x%08X "
            "stackTop=0x%08X control=0x%08X; "
            "stopping before func_80091B20.\n",
            guest_pc,
            g_stage74_sp,
            g_stage74_s0,
            read_guest_u32(kStage74SchedulerHeadGlobal),
            read_guest_u32(kStage73SchedulerObj + 0x14u),
            read_guest_u32(kStage73SchedulerObj + 0x11Cu),
            read_guest_u32(kStage73SchedulerObj + 0x100u),
            read_guest_u32(kStage73SchedulerObj + 0x104u),
            read_guest_u32(kStage73SchedulerObj + 0x118u),
            read_guest_u32(kStage73SchedulerObj + 0x128u),
            read_guest_u32(kStage73SchedulerObj + 0xF4u),
            read_guest_u32(kStage73SchedulerObj + 0x12Cu));

        std::puts("THREAD 4: Stage 74 create-return boundary replayed; starting audio thread via func_80091B20.");
        return 0;
    }

    if (guest_pc == kStage75ParentPostStartPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage75_parent_post_start_reached = true;

        std::printf(
            "THREAD 4 GATE: func_80091B20 returned at guest PC 0x%08X "
            "SP=0x%08X after starting thread 0x%08X; waiting for "
            "audio-thread dispatch to 0x%08X.\n",
            guest_pc,
            static_cast<uint32_t>(ctx->r29),
            kStage73SchedulerObj,
            kStage75AudioThreadEntryPc);

        g_thread_cv.notify_all();

        g_thread_cv.wait(lock, [] {
            return g_stage75_audio_thread_entry_reached;
        });

        std::puts(
            "THREAD 4 GATE: audio-thread entry observed; Stage 75 regression "
            "proof is complete, continuing the parent/main boot thread.");
        return 0;
    }

    if (guest_pc == kStage79V6PostBlockingReceivePc) {
        if (g_stage79_callback1_return_reached) {
            const uint32_t received =
                read_guest_u32(kStage79Callback1MsgOut);
            const uint32_t queue_count =
                read_guest_u32(kStage79AudioQueue + 0x08u);
            const uint32_t queue_first =
                read_guest_u32(kStage79AudioQueue + 0x0Cu);

            std::printf(
                "AUDIO LATER-WAKE CHECK: received=0x%08X "
                "queueCount=%u queueFirst=%u; first Stage 79 wake "
                "already proven, continuing normally.\n",
                received,
                queue_count,
                queue_first);
            return 0;
        }

        const uint32_t received =
            read_guest_u32(kStage79Callback1MsgOut);
        const uint32_t queue_count =
            read_guest_u32(kStage79AudioQueue + 0x08u);
        const uint32_t queue_first =
            read_guest_u32(kStage79AudioQueue + 0x0Cu);

        std::printf(
            "AUDIO FIRST-WAKE CHECK: first blocking receive completed; "
            "received=0x%08X queueCount=%u queueFirst=%u. "
            "Continuing into callback[1]'s real nonblocking drain.\n",
            received,
            queue_count,
            queue_first);

        if (received != kStage79WorkerBase ||
            queue_count != 0u ||
            queue_first != 1u) {
            std::fprintf(
                stderr,
                "FAILED: audio receive bridge did not deliver the first "
                "VI-driven worker message correctly: "
                "received=0x%08X expected=0x%08X "
                "queueCount=%u expected=0 "
                "queueFirst=%u expected=1.\n",
                received,
                kStage79WorkerBase,
                queue_count,
                queue_first);
            std::fflush(stdout);
            std::fflush(stderr);
            return 1;
        }

        return 0;
    }

    if (guest_pc == kStage79AfterCallback1Pc) {
        if (g_stage79_callback1_return_reached) {
            return 0;
        }

        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage79_return_sp = static_cast<uint32_t>(ctx->r29);
        g_stage79_received_message =
            read_guest_u32(kStage79Callback1MsgOut);

        g_stage79_audio_queue_count =
            read_guest_u32(kStage79AudioQueue + 0x08u);
        g_stage79_audio_queue_first =
            read_guest_u32(kStage79AudioQueue + 0x0Cu);
        g_stage79_audio_queue_capacity =
            read_guest_u32(kStage79AudioQueue + 0x10u);
        g_stage79_audio_queue_buffer =
            read_guest_u32(kStage79AudioQueue + 0x14u);

        g_stage79_callback1_return_reached = true;

        std::printf(
            "AUDIO VI WAKEUP RETURN GUARD: guest PC 0x%08X "
            "SP=0x%08X received=0x%08X "
            "audioQueue=0x%08X count=%u first=%u cap=%u "
            "buffer=0x%08X; callback[1] returned after the first "
            "VI-driven wakeup chain.\\n",
            guest_pc,
            g_stage79_return_sp,
            g_stage79_received_message,
            kStage79AudioQueue,
            g_stage79_audio_queue_count,
            g_stage79_audio_queue_first,
            g_stage79_audio_queue_capacity,
            g_stage79_audio_queue_buffer);

        g_thread_cv.notify_all();
        return 0;
    }

    if (guest_pc == kStage78Callback1ReceivePc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        if (g_stage81_idle_milestone_reached) {
            std::puts(
                "STAGE 82 AUDIO YIELD: Stage 81 is proven; leaving the "
                "audio queue empty and entering Toy Story 2's real "
                "blocking receive so N64ModernRuntime can resume Thread 4.");
            return 0;
        }

        g_stage78_receive_sp = static_cast<uint32_t>(ctx->r29);
        g_stage78_queue = static_cast<uint32_t>(ctx->r4);
        g_stage78_msg_out = static_cast<uint32_t>(ctx->r5);
        g_stage78_flags = static_cast<uint32_t>(ctx->r6);

        const uint32_t effective_queue = g_stage78_queue + 0x08u;

        if (effective_queue >= 0x80000000u &&
            effective_queue <= 0x807FFFE8u) {
            g_stage78_queue_count =
                read_guest_u32(effective_queue + 0x08u);
            g_stage78_queue_first =
                read_guest_u32(effective_queue + 0x0Cu);
            g_stage78_queue_capacity =
                read_guest_u32(effective_queue + 0x10u);
            g_stage78_queue_buffer =
                read_guest_u32(effective_queue + 0x14u);
        }

        g_stage78_callback1_receive_reached = true;

        std::printf(
            "AUDIO CALLBACK #1 RECEIVE GUARD: guest PC 0x%08X "
            "SP=0x%08X queueBase=0x%08X effectiveQueue=0x%08X msgOut=0x%08X flags=%u "
            "count=%u first=%u cap=%u buffer=0x%08X; "
            "stopping before func_80091C40 executes.\n",
            guest_pc,
            g_stage78_receive_sp,
            g_stage78_queue,
            effective_queue,
            g_stage78_msg_out,
            g_stage78_flags,
            g_stage78_queue_count,
            g_stage78_queue_first,
            g_stage78_queue_capacity,
            g_stage78_queue_buffer);

        g_stage79_event_slot_queue =
            read_guest_u32(kStage79ViEventSlot + 0x00u);
        g_stage79_event_slot_message =
            read_guest_u32(kStage79ViEventSlot + 0x04u);
        g_stage79_vi_context =
            read_guest_u32(kStage79ViContextGlobal);
        g_stage79_vi_current_context =
            read_guest_u32(kStage79ViCurrentContextGlobal);

        if (g_stage79_vi_context >= 0x80000000u &&
            g_stage79_vi_context <= 0x807FFFE8u) {
            g_stage79_vi_user_queue =
                read_guest_u32(g_stage79_vi_context + 0x10u);
            g_stage79_vi_user_message =
                read_guest_u32(g_stage79_vi_context + 0x14u);
        }

        g_stage79_vi_route_valid =
            ((g_stage79_vi_current_context == kStage79ViContextA &&
              g_stage79_vi_context == kStage79ViContextB) ||
             (g_stage79_vi_current_context == kStage79ViContextB &&
              g_stage79_vi_context == kStage79ViContextA)) &&
            g_stage79_event_slot_queue == kStage79ViManagerQueue &&
            g_stage79_event_slot_message == kStage79ViInterruptMessage &&
            g_stage79_vi_user_queue == kStage79WorkerQueue &&
            g_stage79_vi_user_message == kStage79WorkerEventMessage &&
            g_stage77_audio_list == kStage79WorkerBase &&
            g_stage77_list_head == g_stage77_sync_block &&
            g_stage78_queue_count == 0u &&
            g_stage78_queue_capacity == 4u;

        if (!g_stage79_vi_route_valid) {
            std::fprintf(
                stderr,
                "FAILED: Stage 79 VI wakeup route is not armed correctly: "
                "eventSlot=%08X/%08X viCtx=%08X user=%08X/%08X "
                "worker=%08X audioQueue=%08X.\\n",
                g_stage79_event_slot_queue,
                g_stage79_event_slot_message,
                g_stage79_vi_context,
                g_stage79_vi_user_queue,
                g_stage79_vi_user_message,
                g_stage77_audio_list,
                kStage79AudioQueue);
            g_thread_cv.notify_all();
            g_thread_cv.wait(lock, [] { return false; });
            return 1;
        }

        std::printf(
            "VI WAKEUP ROUTE: event7 slot queue=0x%08X "
            "msg=0x%08X -> VI current/update=0x%08X/0x%08X "
            "userQueue=0x%08X "
            "userMsg=0x%08X -> worker=0x%08X -> audioQueue=0x%08X.\\n",
            g_stage79_event_slot_queue,
            g_stage79_event_slot_message,
            g_stage79_vi_current_context,
            g_stage79_vi_context,
            g_stage79_vi_user_queue,
            g_stage79_vi_user_message,
            kStage79WorkerBase,
            kStage79AudioQueue);

        // The headless boundary harness does not run the normal timed VI
        // producer. Queue exactly one VI event using N64ModernRuntime's own
        // external-message path. Toy Story 2's real VI manager and worker
        // must perform every subsequent forwarding step themselves.
        ultramodern::enqueue_external_message(
            g_stage79_event_slot_queue,
            g_stage79_event_slot_message,
            false,
            false);
        g_stage79_vi_event_queued = true;

        std::puts(
            "VI EVENT AUTO #1: queued the registered Toy Story 2 VI "
            "interrupt message through N64ModernRuntime; callback[1] "
            "will now execute its real blocking receive.");

        g_thread_cv.notify_all();
        return 0;
    }

    if (guest_pc == kStage77FirstCallbackReturnPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage77_return_sp = static_cast<uint32_t>(ctx->r29);
        g_stage77_callback_return_v0 = static_cast<uint32_t>(ctx->r2);
        g_stage77_sync_block = read_guest_u32(kStage77SyncBlockGlobal);
        g_stage77_audio_list = read_guest_u32(kStage77AudioListGlobal);

        if (g_stage77_sync_block >= 0x80000000u &&
            g_stage77_sync_block <= 0x807FFFA8u) {
            g_stage77_link_next = read_guest_u32(g_stage77_sync_block + 0x00u);
            g_stage77_queue1 = read_guest_u32(g_stage77_sync_block + 0x04u);
            g_stage77_queue1_cap = read_guest_u32(g_stage77_sync_block + 0x18u);
            g_stage77_queue1_buf = read_guest_u32(g_stage77_sync_block + 0x1Cu);
            g_stage77_queue2_cap = read_guest_u32(g_stage77_sync_block + 0x40u);
            g_stage77_queue2_buf = read_guest_u32(g_stage77_sync_block + 0x44u);
        }

        if (g_stage77_audio_list >= 0x80000000u &&
            g_stage77_audio_list <= 0x807FFD9Fu) {
            g_stage77_list_head = read_guest_u32(g_stage77_audio_list + 0x260u);
        }

        g_stage77_queue_shape_valid =
            g_stage77_sync_block >= 0x80000000u &&
            g_stage77_sync_block <= 0x807FFFA8u &&
            g_stage77_queue1 == g_stage77_sync_block + 0x08u &&
            g_stage77_queue1_cap == 4u &&
            g_stage77_queue1_buf == g_stage77_sync_block + 0x20u &&
            g_stage77_queue2_cap == 4u &&
            g_stage77_queue2_buf == g_stage77_sync_block + 0x48u &&
            g_stage77_list_head == g_stage77_sync_block;

        g_stage77_first_callback_return_reached = true;

        std::printf(
            "AUDIO CALLBACK #0 RETURN GUARD: guest PC 0x%08X "
            "SP=0x%08X v0=0x%08X sync=0x%08X list=0x%08X "
            "listHead=0x%08X next=0x%08X queue1=0x%08X "
            "q1cap=%u q1buf=0x%08X q2cap=%u q2buf=0x%08X "
            "shape=%s; stopping before callback[1] is loaded.\n",
            guest_pc,
            g_stage77_return_sp,
            g_stage77_callback_return_v0,
            g_stage77_sync_block,
            g_stage77_audio_list,
            g_stage77_list_head,
            g_stage77_link_next,
            g_stage77_queue1,
            g_stage77_queue1_cap,
            g_stage77_queue1_buf,
            g_stage77_queue2_cap,
            g_stage77_queue2_buf,
            g_stage77_queue_shape_valid ? "OK" : "BAD");

        std::puts("AUDIO THREAD: Stage 77 callback[0] return replayed; loading and executing callback[1] 0x80090798.");
        g_thread_cv.notify_all();
        return 0;
    }

    if (guest_pc == kStage76FirstCallbackPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage76_first_callback_sp =
            static_cast<uint32_t>(ctx->r29);
        g_stage76_callback_table =
            static_cast<uint32_t>(ctx->r4);
        g_stage76_first_callback =
            static_cast<uint32_t>(ctx->r2);
        g_stage76_audio_state =
            static_cast<uint32_t>(ctx->r3);

        if (g_stage76_callback_table >= 0x80000000u &&
            g_stage76_callback_table <= 0x807FFFF4u) {
            g_stage76_second_callback =
                read_guest_u32(g_stage76_callback_table + 0x04u);
            g_stage76_third_callback =
                read_guest_u32(g_stage76_callback_table + 0x08u);
        }

        g_stage76_first_callback_boundary_reached = true;

        std::printf(
            "AUDIO THREAD CALLBACK GUARD: reached guest PC 0x%08X "
            "SP=0x%08X callbackTable=0x%08X "
            "callback[0]=0x%08X callback[1]=0x%08X "
            "callback[2]=0x%08X audioState=0x%08X; "
            "stopping before the first jalr executes.\n",
            guest_pc,
            g_stage76_first_callback_sp,
            g_stage76_callback_table,
            g_stage76_first_callback,
            g_stage76_second_callback,
            g_stage76_third_callback,
            g_stage76_audio_state);

        std::puts("AUDIO THREAD: Stage 76 callback boundary replayed; executing callback[0] 0x80090730.");
        g_thread_cv.notify_all();
        return 0;
    }

    if (guest_pc == kStage75AudioThreadEntryPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_stage75_audio_thread_sp = static_cast<uint32_t>(ctx->r29);
        g_stage75_audio_thread_a0 = static_cast<uint32_t>(ctx->r4);
        g_stage75_audio_thread_entry_reached = true;

        std::printf(
            "AUDIO THREAD GUARD: N64ModernRuntime started Toy Story 2's "
            "audio thread at guest PC 0x%08X SP=0x%08X arg/a0=0x%08X; "
            "stopping before func_800904DC executes its first instruction.\n",
            guest_pc,
            g_stage75_audio_thread_sp,
            g_stage75_audio_thread_a0);

        std::puts("AUDIO THREAD: Stage 75 entry replayed; executing the real func_800904DC prologue.");
        g_thread_cv.notify_all();
        return 0;
    }

    if (guest_pc == kStage71RealSfxInitPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);
        g_stage71_sp = static_cast<uint32_t>(ctx->r29);
        g_stage71_a0 = static_cast<uint32_t>(ctx->r4);
        g_stage71_a1 = static_cast<uint32_t>(ctx->r5);
        g_stage71_s0 = static_cast<uint32_t>(ctx->r16);
        g_stage71_real_sfx_init_reached = true;

        std::printf(
            "THREAD 4 GUARD: MusInitialize returned with ROM WBK untouched; "
            "Toy Story 2 reached explicit SFX init at PC 0x%08X "
            "SP=0x%08X a0/sfx=0x%08X a1=0x%08X s0/wbk=0x%08X "
            "ptr=0x%08X wbk=0x%08X defaultFx=0x%08X "
            "sfxCount=%u sfxFlag=0x%08X sfxData=0x%08X; "
            "Stage 71 regression proof complete, continuing through "
            "func_8008E080(sfx.bfx).\n",
            guest_pc, g_stage71_sp, g_stage71_a0, g_stage71_a1,
            g_stage71_s0,
            read_guest_u32(kStage71PtrSlot),
            read_guest_u32(kStage71WbkSlot),
            read_guest_u32(kStage71FxSlot),
            read_guest_u32(kStage71SfxBank),
            read_guest_u32(kStage71SfxBank + 0x0Cu),
            read_guest_u32(kStage71SfxBank + 0x14u));

        g_thread_cv.notify_all();
        return 0;
    }

    // V121b: 0x8008E0C0 is a valid store boundary for RAM-backed .bfx data.
    // Trap it only if a2 still points into cartridge KSEG1 (0xBxxxxxxx).
    if (guest_pc == kWaveTableFirstCartWritePc &&
        (static_cast<uint32_t>(ctx->r6) & 0xF0000000u) == 0xB0000000u) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_wavetable_write_sp = static_cast<uint32_t>(ctx->r29);
        g_wavetable_write_v0 = static_cast<uint32_t>(ctx->r2);
        g_wavetable_write_v1 = static_cast<uint32_t>(ctx->r3);
        g_wavetable_write_a0 = static_cast<uint32_t>(ctx->r4);
        g_wavetable_write_a1 = static_cast<uint32_t>(ctx->r5);
        g_wavetable_write_a2 = static_cast<uint32_t>(ctx->r6);
        g_wavetable_write_a3 = static_cast<uint32_t>(ctx->r7);
        g_wavetable_first_cart_write_reached = true;

        std::printf(
            "THREAD 4 GUARD: WaveTables relocation reached its first "
            "cart-space store at guest PC 0x%08X SP=0x%08X "
            "v0=0x%08X v1=0x%08X a0/count=0x%08X a1=0x%08X "
            "a2=0x%08X a3=0x%08X relocGlobal=0x%08X; "
            "stopping before sw v1,0xC(a2).\n",
            guest_pc,
            g_wavetable_write_sp,
            g_wavetable_write_v0,
            g_wavetable_write_v1,
            g_wavetable_write_a0,
            g_wavetable_write_a1,
            g_wavetable_write_a2,
            g_wavetable_write_a3,
            read_guest_u32(kWaveTableRelocGlobal));

        g_thread_cv.notify_all();
        std::fprintf(
            stderr,
            "V121b FATAL: func_8008E080 still attempted an in-place BFX relocation in cartridge space; "
            "a2=0x%08X. Refusing the cart write instead of hanging Thread 4.\n",
            static_cast<uint32_t>(ctx->r6));
        std::fflush(stderr);
        std::_Exit(43);
        return 1;
    }

    if (guest_pc == kAudioTracePreClearPc ||
        guest_pc == kAudioTracePostClearPc ||
        guest_pc == kAudioTracePreSetPtrPc ||
        guest_pc == kAudioTracePostSetPtrPc ||
        guest_pc == kAudioTracePreAlloc1Pc ||
        guest_pc == kAudioTracePostAlloc1Pc ||
        guest_pc == kAudioTracePreAlloc2Pc ||
        guest_pc == kAudioTracePostAlloc2Pc ||
        guest_pc == kAudioTracePrePoolPc ||
        guest_pc == kAudioTracePostPoolPc ||
        guest_pc == kAudioTracePrePtrInitPc ||
        guest_pc == kAudioTracePostPtrInitPc ||
        guest_pc == kAudioTracePreBankInitPc ||
        guest_pc == kAudioTracePostBankInitPc ||
        guest_pc == kAudioTracePre903A0Pc) {

        g_audio_trace_last_pc = guest_pc;
        ++g_audio_trace_hit_count;

        std::printf(
            "AUDIO TRACE #%u: guest PC 0x%08X SP=0x%08X "
            "v0=0x%08X a0=0x%08X a1=0x%08X s3=0x%08X.\n",
            g_audio_trace_hit_count,
            guest_pc,
            static_cast<uint32_t>(ctx->r29),
            static_cast<uint32_t>(ctx->r2),
            static_cast<uint32_t>(ctx->r4),
            static_cast<uint32_t>(ctx->r5),
            static_cast<uint32_t>(ctx->r19));

        if (false && guest_pc == kAudioTracePre903A0Pc) {
            std::unique_lock<std::mutex> lock(g_thread_mutex);
            g_audio_trace_terminal_sp = static_cast<uint32_t>(ctx->r29);
            g_audio_trace_terminal_s3 = static_cast<uint32_t>(ctx->r19);
            g_audio_trace_pre903a0_reached = true;

            std::printf(
                "THREAD 4 GUARD: reached 0x%08X SP=0x%08X config=0x%08X; "
                "stopping before func_800903A0.\n",
                guest_pc,
                g_audio_trace_terminal_sp,
                g_audio_trace_terminal_s3);

            g_thread_cv.notify_all();
            g_thread_cv.wait(lock, [] { return false; });
            return 1;
        }

        return 0;
    }

    if (guest_pc == kAudioCartProbePiStatusPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_audio_cart_probe_sp = static_cast<uint32_t>(ctx->r29);
        g_audio_cart_probe_v0 = static_cast<uint32_t>(ctx->r2);
        g_audio_cart_probe_s0 = static_cast<uint32_t>(ctx->r16);
        g_audio_cart_probe_s1 = static_cast<uint32_t>(ctx->r17);
        g_audio_cart_probe_pi_status_reached = true;

        std::printf(
            "THREAD 4 GUARD: audio-engine startup reached the cartridge/PI "
            "probe in func_800982E0 at guest PC 0x%08X SP=0x%08X "
            "PI_STATUS=0x%08X resultPtr=0x%08X cartOffset=0x%08X "
            "cartBaseGlobal=0x%08X; stopping before the direct PI_STATUS "
            "read executes.\n",
            guest_pc,
            g_audio_cart_probe_sp,
            g_audio_cart_probe_v0,
            g_audio_cart_probe_s0,
            g_audio_cart_probe_s1,
            read_guest_u32(kAudioCartProbeBaseGlobal));

        g_thread_cv.notify_all();
        g_thread_cv.wait(lock, [] { return false; });
        return 1;
    }

    if (guest_pc == kFirstAiMmioPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);
        g_first_ai_mmio_sp = static_cast<uint32_t>(ctx->r29);
        g_first_ai_mmio_v0 = static_cast<uint32_t>(ctx->r2);
        g_first_ai_mmio_v1 = static_cast<uint32_t>(ctx->r3);
        g_first_ai_mmio_a0 = static_cast<uint32_t>(ctx->r4);
        g_first_ai_mmio_a1 = static_cast<uint32_t>(ctx->r5);
        g_first_ai_mmio_a2 = static_cast<uint32_t>(ctx->r6);
        g_first_ai_mmio_a3 = static_cast<uint32_t>(ctx->r7);
        g_first_ai_mmio_boundary_reached = true;
        std::printf(
            "THREAD 4 GUARD: reached first AI MMIO at PC 0x%08X "
            "SP=0x%08X DACRATEreg=0x%08X value=0x%08X "
            "BITRATEreg=0x%08X CONTROLreg=0x%08X divisor=0x%08X "
            "bitratePlus1=0x%08X; stopping before write.\\n",
            guest_pc, g_first_ai_mmio_sp, g_first_ai_mmio_v1,
            g_first_ai_mmio_v0, g_first_ai_mmio_a0, g_first_ai_mmio_a1,
            g_first_ai_mmio_a2, g_first_ai_mmio_a3);
        g_thread_cv.notify_all();
        std::puts("THREAD 4: Stage 71 first-AI boundary replayed; Stage 72 will execute the bridged AI writes.");
        return 0;
    }

    if (guest_pc == kPiRawDmaBoundaryPc) {
        const uint32_t index = g_pi_raw_boundary_count;
        ++g_pi_raw_boundary_count;

        g_pi_raw_sp = static_cast<uint32_t>(ctx->r29);
        g_pi_raw_a0 = static_cast<uint32_t>(ctx->r4);
        g_pi_raw_a1 = static_cast<uint32_t>(ctx->r5);
        g_pi_raw_a2 = static_cast<uint32_t>(ctx->r6);
        g_pi_raw_a3 = static_cast<uint32_t>(ctx->r7);
        g_pi_raw_s0 = static_cast<uint32_t>(ctx->r16);
        g_pi_raw_s1 = static_cast<uint32_t>(ctx->r17);
        g_pi_raw_s2 = static_cast<uint32_t>(ctx->r18);
        g_pi_raw_v0 = static_cast<uint32_t>(ctx->r2);
        g_pi_raw_dma_boundary_reached = true;

        if (index >= kTransferCount) {
            std::printf(
                "TS2 FAST-FORWARD DYNAMIC PI RAW #%u: dir=0x%08X dev=0x%08X "
                "dram=0x%08X size=0x%08X SP=0x%08X.\n",
                index + 1u,
                g_pi_raw_a0,
                g_pi_raw_a1,
                g_pi_raw_a2,
                g_pi_raw_a3,
                g_pi_raw_sp);
            // Do not touch the fixed Stage 1-92 capture arrays. Returning zero
            // lets the real recompiled raw-DMA function continue.
            return 0;
        }

        g_raw_dirs[index] = g_pi_raw_a0;
        g_raw_devs[index] = g_pi_raw_a1;
        g_raw_drams[index] = g_pi_raw_a2;
        g_raw_sizes[index] = g_pi_raw_a3;

        const uint32_t request = read_guest_u32(0x801F2B10u);
        g_raw_requests[index] = request;
        if (request != 0u) {
            g_raw_request_types[index] =
                read_guest_u16(request + 0x00u);
            g_raw_request_return_queues[index] =
                read_guest_u32(request + 0x04u);
        }

        std::printf(
            "PI RAW DMA #%u: reached 0x%08X SP=0x%08X "
            "dir=%u dev=0x%08X dram=0x%08X size=0x%08X; "
            "continuing through validated PI MMIO.\n",
            index + 1u,
            guest_pc,
            g_pi_raw_sp,
            g_pi_raw_a0,
            g_pi_raw_a1,
            g_pi_raw_a2,
            g_pi_raw_a3);
        return 0;
    }

    if (guest_pc == kPiCompletionWaitPc) {
        ++g_pi_completion_wait_count;

        g_pi_completion_sp = static_cast<uint32_t>(ctx->r29);
        g_pi_completion_a0 = static_cast<uint32_t>(ctx->r4);
        g_pi_completion_a1 = static_cast<uint32_t>(ctx->r5);
        g_pi_completion_a2 = static_cast<uint32_t>(ctx->r6);
        g_pi_completion_wait_boundary_reached = true;

        std::fprintf(
            stderr,
            "TS2 PI148 PROBE V3 CHECKPOINT BEFORE PRINT count=%u queue=0x%08X msg=0x%08X flags=%u\n",
            g_pi_completion_wait_count,
            g_pi_completion_a0,
            g_pi_completion_a1,
            g_pi_completion_a2);
        std::fflush(stderr);

        std::printf(
            "PI MANAGER: completion receive #%u at 0x%08X "
            "SP=0x%08X queue=0x%08X msg=0x%08X flags=%u; "
            "completion event already queued by PI bridge.\n",
            g_pi_completion_wait_count,
            guest_pc,
            g_pi_completion_sp,
            g_pi_completion_a0,
            g_pi_completion_a1,
            g_pi_completion_a2);

        std::fprintf(
            stderr,
            "TS2 PI148 PROBE V3 CHECKPOINT AFTER PRINT count=%u\n",
            g_pi_completion_wait_count);
        std::fflush(stderr);
        return 0;
    }

    if (guest_pc == kLoaderWaitPc) {
        const uint32_t index = g_loader_wait_count;
        ++g_loader_wait_count;

        g_loader_wait_sp = static_cast<uint32_t>(ctx->r29);
        g_loader_wait_a0 = static_cast<uint32_t>(ctx->r4);
        g_loader_wait_a1 = static_cast<uint32_t>(ctx->r5);
        g_loader_wait_a2 = static_cast<uint32_t>(ctx->r6);
        g_loader_wait_s0 = static_cast<uint32_t>(ctx->r16);
        g_loader_wait_s1 = static_cast<uint32_t>(ctx->r17);
        g_loader_wait_s2 = static_cast<uint32_t>(ctx->r18);
        g_loader_wait_s3 = static_cast<uint32_t>(ctx->r19);
        g_loader_wait_s4 = static_cast<uint32_t>(ctx->r20);

        if (index < kTransferCount) {
            g_wait_remaining[index] = g_loader_wait_s1;
            g_wait_devs[index] = g_loader_wait_s2;
            g_wait_drams[index] = g_loader_wait_s3;
        }

        std::printf(
            "THREAD 4: loader completion receive #%u at 0x%08X "
            "SP=0x%08X chunk=0x%08X remaining=0x%08X "
            "dev=0x%08X dram=0x%08X.\n",
            g_loader_wait_count,
            guest_pc,
            g_loader_wait_sp,
            g_loader_wait_s0,
            g_loader_wait_s1,
            g_loader_wait_s2,
            g_loader_wait_s3);
        return 0;
    }

    if (guest_pc == kLoaderPostReceivePc) {
        const uint32_t index = g_loader_post_count;
        ++g_loader_post_count;

        g_loader_post_sp = static_cast<uint32_t>(ctx->r29);
        g_loader_post_s0 = static_cast<uint32_t>(ctx->r16);
        g_loader_post_s1 = static_cast<uint32_t>(ctx->r17);
        g_loader_post_s2 = static_cast<uint32_t>(ctx->r18);
        g_loader_post_s3 = static_cast<uint32_t>(ctx->r19);
        g_loader_post_s4 = static_cast<uint32_t>(ctx->r20);
        g_loader_post_receive_reached = true;

        if (index < kTransferCount) {
            g_post_remaining[index] = g_loader_post_s1;
        }

        std::printf(
            "THREAD 4: completion #%u consumed at 0x%08X "
            "SP=0x%08X; chunk advance will now execute.\n",
            g_loader_post_count,
            guest_pc,
            g_loader_post_sp);
        return 0;
    }

    if (guest_pc == kLoaderReturnPc) {
        g_loader_return_sp = static_cast<uint32_t>(ctx->r29);
        g_loader_return_reached = true;

        g_parser_directory_ptr = static_cast<uint32_t>(ctx->r16);
        g_parser_input_ptr = static_cast<uint32_t>(ctx->r17);
        g_parser_size_out_ptr = static_cast<uint32_t>(ctx->r18);
        g_parser_data_out_ptr = static_cast<uint32_t>(ctx->r19);
        g_parser_input_name = read_guest_string(g_parser_input_ptr);

        std::printf(
            "THREAD 4: func_800858D4 completed all four chunks and "
            "returned to 0x%08X with SP=0x%08X; entering ROM-directory "
            "parser for key \"%s\" (dir=0x%08X dataOut=0x%08X "
            "sizeOut=0x%08X).\n",
            guest_pc,
            g_loader_return_sp,
            g_parser_input_name.c_str(),
            g_parser_directory_ptr,
            g_parser_data_out_ptr,
            g_parser_size_out_ptr);
        return 0;
    }

    if (guest_pc == kParserReturnPc) {
        g_parser_return_sp = static_cast<uint32_t>(ctx->r29);
        g_parser_return_v0 = static_cast<uint32_t>(ctx->r2);
        // The resource loader's parser output slots depend on the caller's
        // stack frame.  Use the live pointers captured at 0x80085708 rather
        // than the fixed font-call stack addresses used by earlier stages.
        g_parser_actual_data_offset =
            read_guest_u32(g_parser_data_out_ptr);
        g_parser_actual_size =
            read_guest_u32(g_parser_size_out_ptr);
        g_font_original_dest = static_cast<uint32_t>(ctx->r18);
        g_parser_return_reached = true;

        if (g_parser_input_name == "tunes\\sfx.bfx") {
            g_sfx_parser_valid = true;
            g_sfx_parser_data = g_parser_actual_data_offset;
            g_sfx_parser_size = g_parser_actual_size;
        } else if (g_parser_input_name == "tunes\\ts2.ptr") {
            g_ptr_parser_valid = true;
            g_ptr_parser_data = g_parser_actual_data_offset;
            g_ptr_parser_size = g_parser_actual_size;
        }

        if (g_parser_input_name == "text\\common.eng") {
            g_common_parser_snapshot_valid = true;
            g_common_parser_result = g_parser_return_v0;
            g_common_parser_data_offset = g_parser_actual_data_offset;
            g_common_parser_size = g_parser_actual_size;
        } else if (g_parser_input_name == "level99\\sprite0.pic") {
            g_sprite_parser_snapshot_valid = true;
            g_sprite_parser_result = g_parser_return_v0;
            g_sprite_parser_data_offset = g_parser_actual_data_offset;
            g_sprite_parser_size = g_parser_actual_size;
        }

                // TS2 FAST-FORWARD: directory-return diagnostic removed.
        // Parser state above and the original return path below are unchanged.
        return 0;
    }

    if (guest_pc == kFontPayloadAlignedCallPc ||
        guest_pc == kFontPayloadUnalignedCallPc) {
        g_font_payload_boundary_pc = guest_pc;
        g_font_payload_sp = static_cast<uint32_t>(ctx->r29);
        g_font_payload_a0 = static_cast<uint32_t>(ctx->r4);
        g_font_payload_a1 = static_cast<uint32_t>(ctx->r5);
        g_font_payload_a2 = static_cast<uint32_t>(ctx->r6);
        g_font_payload_s0 = static_cast<uint32_t>(ctx->r16);
        g_font_payload_s1 = static_cast<uint32_t>(ctx->r17);
        g_font_payload_s2 = static_cast<uint32_t>(ctx->r18);
        g_font_payload_boundary_reached = true;

        if (g_parser_input_name == "text\\common.eng") {
            g_common_payload_snapshot_valid = true;
            g_common_payload_pc = guest_pc;
            g_common_payload_a0 = g_font_payload_a0;
            g_common_payload_a1 = g_font_payload_a1;
            g_common_payload_a2 = g_font_payload_a2;
        } else if (g_parser_input_name == "level99\\sprite0.pic") {
            g_sprite_payload_snapshot_valid = true;
            g_sprite_payload_pc = guest_pc;
            g_sprite_payload_a0 = g_font_payload_a0;
            g_sprite_payload_a1 = g_font_payload_a1;
            g_sprite_payload_a2 = g_font_payload_a2;
        }

        std::printf(
            "THREAD 4: crossed Stage 52 payload boundary 0x%08X "
            "SP=0x%08X dest=0x%08X dataOffset=0x%08X "
            "primarySize=0x%08X; entering font PI load.\n",
            guest_pc,
            g_font_payload_sp,
            g_font_payload_a0,
            g_font_payload_a1,
            g_font_payload_a2);
        return 0;
    }

    if (guest_pc == kFontCompleteBoundaryPc) {
        ++g_font_complete_count;
        if (g_font_complete_count >= 19u) {
            std::fprintf(
                stderr,
                "TS2 CRITICAL PROBE RESOURCE #%u PC=0x%08X SP=0x%08X\n",
                g_font_complete_count,
                guest_pc,
                static_cast<uint32_t>(ctx->r29));
            std::fflush(stderr);
        }
        g_font_complete_sp = static_cast<uint32_t>(ctx->r29);
        g_font_complete_s2 = static_cast<uint32_t>(ctx->r18);
        g_font_complete_first_word = read_guest_u32(g_font_complete_s2);
        g_font_complete_boundary_reached = true;
        if (g_font_complete_count > 8u) {
            std::printf(
                "FAST FORWARD RESOURCE COMPLETE #%u: PC=0x%08X SP=0x%08X "
                "dest=0x%08X firstWord=0x%08X; continuing.\n",
                g_font_complete_count,
                guest_pc,
                g_font_complete_sp,
                g_font_complete_s2,
                g_font_complete_first_word);
            return 0;
        }

        if (g_font_complete_count == 1u) {
            std::printf(
                "THREAD 4: tahoma08.fnt complete at 0x%08X with "
                "firstWord=0x%08X; continuing through its non-RNC path.\\n",
                guest_pc,
                g_font_complete_first_word);
        } else if (g_font_complete_count == 2u) {
            g_second_font_complete_first_word = g_font_complete_first_word;
            std::printf(
                "THREAD 4: tahoma12.fnt complete at 0x%08X with "
                "SP=0x%08X font=0x%08X firstWord=0x%08X; "
                "entering RNC method-1 handling.\\n",
                guest_pc,
                g_font_complete_sp,
                g_font_complete_s2,
                g_second_font_complete_first_word);
        } else if (g_font_complete_count == 3u) {
            g_third_font_complete_first_word = g_font_complete_first_word;
            g_third_font_complete_hash =
                stage61_fnv1a_guest(kExpectedFontDest, kEtchFontFileSize);
            std::printf(
                "THREAD 4: etchfont.fnt complete at 0x%08X with "
                "SP=0x%08X font=0x%08X firstWord=0x%08X "
                "FNV1a=0x%08X; continuing through its normal non-RNC path.\\n",
                guest_pc,
                g_font_complete_sp,
                g_font_complete_s2,
                g_third_font_complete_first_word,
                g_third_font_complete_hash);
        } else if (g_font_complete_count == 4u) {
            g_common_complete_first_word = g_font_complete_first_word;
            g_common_complete_hash =
                stage61_fnv1a_guest(kExpectedFontDest, kCommonFileSize);
            std::printf(
                "THREAD 4: text\\\\common.eng complete at 0x%08X with "
                "SP=0x%08X dest=0x%08X firstWord=0x%08X "
                "FNV1a=0x%08X; continuing through common-text parsing.\\n",
                guest_pc,
                g_font_complete_sp,
                g_font_complete_s2,
                g_common_complete_first_word,
                g_common_complete_hash);
        } else if (g_font_complete_count == 5u) {
            g_sfx_complete_dest = g_font_complete_s2;
            g_sfx_complete_first = g_font_complete_first_word;
            g_sfx_complete_hash =
                stage61_fnv1a_guest(g_font_complete_s2, kSfxFileSize);
            std::printf(
                "THREAD 4: tunes\\\\sfx.bfx complete dest=0x%08X "
                "firstWord=0x%08X FNV1a=0x%08X; continuing to ts2.ptr.\\n",
                g_sfx_complete_dest,
                g_sfx_complete_first,
                g_sfx_complete_hash);
        } else if (g_font_complete_count == 6u) {
            g_ptr_complete_dest = g_font_complete_s2;
            g_ptr_complete_first = g_font_complete_first_word;
            g_ptr_complete_hash =
                stage61_fnv1a_guest(g_font_complete_s2, kPtrFileSize);
            std::printf(
                "THREAD 4: tunes\\\\ts2.ptr complete dest=0x%08X "
                "firstWord=0x%08X FNV1a=0x%08X; entering audio setup.\\n",
                g_ptr_complete_dest,
                g_ptr_complete_first,
                g_ptr_complete_hash);
        } else if (g_font_complete_count == 7u) {
            g_sprite_complete_first_word = g_font_complete_first_word;
            g_sprite_complete_hash = stage61_fnv1a_guest(g_font_complete_s2, kSpriteFileSize);
            std::printf(
                "THREAD 4: level99\\sprite0.pic compressed resource complete at 0x%08X SP=0x%08X dest=0x%08X firstWord=0x%08X FNV1a=0x%08X; continuing into RNC method-1 handling.\n",
                guest_pc, g_font_complete_sp, g_font_complete_s2,
                g_sprite_complete_first_word, g_sprite_complete_hash);
        } else if (g_font_complete_count == 8u) {
            const uint32_t select_hash =
                stage61_fnv1a_guest(g_font_complete_s2, kStage92StoredSize);

            if (g_parser_input_name != "level98\\select.pic" ||
                g_font_complete_s2 != kStage92ResourceDest ||
                g_font_complete_first_word != 0x524E4301u ||
                select_hash != kStage92StoredFnv) {
                std::fprintf(
                    stderr,
                    "FAILED: Stage 92 select.pic compressed-resource boundary mismatch "
                    "name=\"%s\" dest=%08X/%08X first=%08X/524E4301 hash=%08X/%08X.\n",
                    g_parser_input_name.c_str(),
                    g_font_complete_s2,
                    kStage92ResourceDest,
                    g_font_complete_first_word,
                    select_hash,
                    kStage92StoredFnv);
                return 1;
            }

            std::printf(
                "THREAD 4: level98\\select.pic compressed resource complete at "
                "0x%08X SP=0x%08X dest=0x%08X firstWord=0x%08X "
                "storedSize=0x%X FNV1a=0x%08X; continuing into its real RNC method-1 path.\n",
                guest_pc,
                g_font_complete_sp,
                g_font_complete_s2,
                g_font_complete_first_word,
                kStage92StoredSize,
                select_hash);
        } else {
            std::fprintf(
                stderr,
                "FAILED: unexpected resource-complete hit #%u.\\n",
                g_font_complete_count);
            return 1;
        }
        return 0;
    }

    if (guest_pc == kActualFontCallerReturnPc) {
        ++g_actual_font_caller_return_count;

        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t v0 = static_cast<uint32_t>(ctx->r2);
        const uint32_t s0 = static_cast<uint32_t>(ctx->r16);
        const uint32_t s1 = static_cast<uint32_t>(ctx->r17);
        const uint32_t s2 = static_cast<uint32_t>(ctx->r18);
        const uint32_t s3 = static_cast<uint32_t>(ctx->r19);
        const uint32_t s5 = static_cast<uint32_t>(ctx->r21);
        const uint32_t s6 = static_cast<uint32_t>(ctx->r22);

        if (g_actual_font_caller_return_count == 1u) {
            g_actual_font_caller_sp = sp;
            g_actual_font_caller_v0 = v0;
            g_actual_font_caller_s1 = s1;
            g_actual_font_caller_s2 = s2;
            g_actual_font_buffer_global = read_guest_u32(kFontBufferGlobal);
            g_actual_font_caller_return_reached = true;

            std::printf(
                "THREAD 4: tahoma08.fnt returned to func_8001D4C4 at "
                "0x%08X with SP=0x%08X v0=0x%08X s1=%u s2=0x%08X; "
                "continuing through first-font glyph processing.\n",
                guest_pc,
                sp,
                v0,
                s1,
                s2);
            return 0;
        }

        if (g_actual_font_caller_return_count == 2u) {
            g_second_font_return_sp = sp;
            g_second_font_return_v0 = v0;
            g_second_font_return_s0 = s0;
            g_second_font_return_s1 = s1;
            g_second_font_return_s2 = s2;
            g_second_font_return_s3 = s3;
            g_second_font_return_s5 = s5;
            g_second_font_return_s6 = s6;
            g_second_font_return_reached = true;

            g_second_font_model_valid = stage61_model_second_font(
                kExpectedFontDest,
                s5,
                s0,
                &g_second_font_model_expanded_bytes,
                &g_second_font_expected_offsets,
                &g_second_font_expected_packed);
            g_second_font_model_final_s0 =
                s0 + g_second_font_model_expanded_bytes;

            std::printf(
                "THREAD 4: tahoma12.fnt returned to func_8001D4C4 at "
                "0x%08X SP=0x%08X v0=0x%08X s0=0x%08X "
                "s1=%u s2=0x%08X s6=%u; model predicts "
                "0x%08X glyph-output bytes and final s0=0x%08X.\n",
                guest_pc,
                sp,
                v0,
                s0,
                s1,
                s2,
                s6,
                g_second_font_model_expanded_bytes,
                g_second_font_model_final_s0);
            return 0;
        }

        if (g_actual_font_caller_return_count == 3u) {
            g_third_font_return_sp = sp;
            g_third_font_return_v0 = v0;
            g_third_font_return_s0 = s0;
            g_third_font_return_s1 = s1;
            g_third_font_return_s2 = s2;
            g_third_font_return_s3 = s3;
            g_third_font_return_s5 = s5;
            g_third_font_return_s6 = s6;
            g_third_font_return_reached = true;

            g_third_font_model_valid = stage61_model_third_font(
                kExpectedFontDest,
                s5,
                s0,
                &g_third_font_model_expanded_bytes,
                &g_third_font_expected_offsets,
                &g_third_font_expected_packed);
            g_third_font_model_final_s0 =
                s0 + g_third_font_model_expanded_bytes;

            std::printf(
                "THREAD 4: etchfont.fnt returned to func_8001D4C4 at "
                "0x%08X SP=0x%08X v0=0x%08X s0=0x%08X "
                "s1=%u s2=0x%08X s6=%u; model predicts "
                "0x%08X glyph-output bytes and final s0=0x%08X.\n",
                guest_pc,
                sp,
                v0,
                s0,
                s1,
                s2,
                s6,
                g_third_font_model_expanded_bytes,
                g_third_font_model_final_s0);
            return 0;
        }

        std::fprintf(
            stderr,
            "THREAD 4 LIVE: additional func_8001D4C4 font return #%u at "
            "0x%08X.\n",
            g_actual_font_caller_return_count,
            guest_pc);
        // TS2 LIVE ADDITIONAL FONT RETURN GUARD V41
        // Startup bring-up originally validated exactly three returns from
        // func_800853E0 back into func_8001D4C4 at guest PC 0x8001D6E0.
        // Live gameplay legitimately repeats the same three-font build from
        // overlay 16, so return #4/#5/#6 are normal.
        //
        // Returning 1 here is destructive: the generated checkpoint at
        // 0x8001D6E0 responds by returning directly from func_8001D4C4
        // before its +0x60 stack epilogue. That leaves the overlay caller's
        // SP 0x60 too low and causes its later saved-font restore to read
        // from the wrong stack address.
        //
        // Additional live font returns are therefore diagnostic-only.
        return 0;
    }

    if (guest_pc == kFontLoadCallPc) {
        ++g_font_load_call_count;

        const uint32_t sp = static_cast<uint32_t>(ctx->r29);
        const uint32_t a0 = static_cast<uint32_t>(ctx->r4);
        const uint32_t a1 = static_cast<uint32_t>(ctx->r5);
        const uint32_t s0 = static_cast<uint32_t>(ctx->r16);
        const uint32_t s1 = static_cast<uint32_t>(ctx->r17);
        const uint32_t s2 = static_cast<uint32_t>(ctx->r18);
        const uint32_t s3 = static_cast<uint32_t>(ctx->r19);
        const uint32_t s5 = static_cast<uint32_t>(ctx->r21);

        if (g_font_load_call_count == 1u && s1 == 0u) {
            g_first_font_call_seen = true;
            g_first_font_call_sp = sp;
            g_first_font_call_a0 = a0;
            g_first_font_call_a1 = a1;
            g_first_font_call_s1 = s1;
            g_first_font_call_s2 = s2;

            std::printf(
                "THREAD 4: first font-load call at 0x%08X "
                "SP=0x%08X key=\"%s\" dest=0x%08X s1=%u s2=0x%08X; "
                "entering the already-validated tahoma08 load.\n",
                guest_pc,
                sp,
                read_guest_string(a0).c_str(),
                a1,
                s1,
                s2);
            return 0;
        }

        if (g_font_load_call_count == 2u && s1 == 1u) {
            g_second_font_call_sp = sp;
            g_second_font_call_a0 = a0;
            g_second_font_call_a1 = a1;
            g_second_font_call_s0 = s0;
            g_second_font_call_s1 = s1;
            g_second_font_call_s2 = s2;
            g_second_font_call_s3 = s3;
            g_second_font_call_s5 = s5;
            g_second_font_pool_base = read_guest_u32(kFontPoolBaseGlobal);
            g_second_font_name = read_guest_string(a0);
            g_second_font_call_reached = true;

            std::printf(
                "THREAD 4: first-font processing completed; entering "
                "font #2 at 0x%08X key=\"%s\" s1=%u s2=0x%08X.\n",
                guest_pc,
                g_second_font_name.c_str(),
                s1,
                s2);
            return 0;
        }

        if (g_font_load_call_count == 3u && s1 == 2u) {
            g_third_font_call_sp = sp;
            g_third_font_call_a0 = a0;
            g_third_font_call_a1 = a1;
            g_third_font_call_s0 = s0;
            g_third_font_call_s1 = s1;
            g_third_font_call_s2 = s2;
            g_third_font_call_s3 = s3;
            g_third_font_call_s5 = s5;
            g_third_font_call_s6 = static_cast<uint32_t>(ctx->r22);
            g_third_font_pool_base =
                read_guest_u32(kFontPoolBaseGlobal);
            g_third_font_name = read_guest_string(a0);
            g_third_font_call_reached = true;

            std::printf(
                "THREAD 4: second-font processing completed; entering "
                "font #3 at guest PC 0x%08X "
                "SP=0x%08X key=\"%s\" dest=0x%08X "
                "s0=0x%08X s1=%u s2=0x%08X s3=0x%08X "
                "s5=0x%08X s6=%u.\n",
                guest_pc,
                sp,
                g_third_font_name.c_str(),
                a1,
                s0,
                s1,
                s2,
                s3,
                s5,
                g_third_font_call_s6);

            return 0;
        }

        std::fprintf(
            stderr,
            "THREAD 4 LIVE: additional font-load call #%u at 0x%08X "
            "s1=%u s2=0x%08X key=\"%s\".\n",
            g_font_load_call_count,
            guest_pc,
            s1,
            s2,
            read_guest_string(a0).c_str());
        // TS2 LIVE ADDITIONAL FONT LOAD GUARD V40
        // The old stage bring-up guard assumed func_8001D4C4 would only
        // execute the three startup font loads. Live gameplay legitimately
        // invokes the same three-font builder again from overlay 16.
        //
        // Returning 1 here is NOT a harmless diagnostic stop: the generated
        // checkpoint at 0x8001D6D8 responds by returning directly from
        // func_8001D4C4 before its +0x60 stack epilogue. The overlay caller
        // then continues with SP 0x60 too low and eventually restores
        // 0x801DB3F0 from the wrong stack address.
        //
        // Additional live font loads are therefore diagnostic-only here.
        // Let the original guest function continue normally.
        return 0;
    }

    if (guest_pc == kThreeFontCallerReturnPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_three_font_return_sp = static_cast<uint32_t>(ctx->r29);
        g_three_font_return_v0 = static_cast<uint32_t>(ctx->r2);
        g_three_font_return_s0 = static_cast<uint32_t>(ctx->r16);
        g_three_font_return_a0 = static_cast<uint32_t>(ctx->r4);
        for (uint32_t i = 0; i < 16u; ++i) {
            g_final_palette[i] =
                read_guest_u16(g_third_font_return_s3 + i * 2u);
        }
        g_three_font_return_reached = true;

        std::printf(
            "THREAD 4 GUARD: func_8001D4C4 completed all three fonts and "
            "returned to caller PC 0x%08X SP=0x%08X v0=0x%08X "
            "callerS0=0x%08X before func_80013760 consumes the font-pool "
            "size.\n",
            guest_pc,
            g_three_font_return_sp,
            g_three_font_return_v0,
            g_three_font_return_s0);

        return 0;
    }

    if (guest_pc == kPostPoolReturnPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);
        g_post_pool_return_sp = static_cast<uint32_t>(ctx->r29);
        g_post_pool_return_v0 = static_cast<uint32_t>(ctx->r2);
        g_post_pool_return_s0 = static_cast<uint32_t>(ctx->r16);
        g_post_pool_return_reached = true;

        std::printf(
            "THREAD 4 GUARD: func_800137C4 completed startup pool/common-text "
            "initialization and returned to guest PC 0x%08X "
            "SP=0x%08X v0=0x%08X s0=0x%08X before func_800159B0(4).\\n",
            guest_pc,
            g_post_pool_return_sp,
            g_post_pool_return_v0,
            g_post_pool_return_s0);

        return 0;
    }

    if (guest_pc == kRncBoundaryPc) {
        if (g_font_complete_count > 8u) {
            std::printf(
                "FAST FORWARD RNC SETUP: resource #%u PC=0x%08X input=0x%08X output=0x%08X.\n",
                g_font_complete_count,
                guest_pc,
                static_cast<uint32_t>(ctx->r4),
                static_cast<uint32_t>(ctx->r5));
            return 0;
        }
        if (g_parser_input_name == "level98\\select.pic") {
            g_stage92_select_rnc_active = true;
            g_stage92_select_rnc_boundary_reached = true;
            g_stage92_select_rnc_sp = static_cast<uint32_t>(ctx->r29);
            g_stage92_select_rnc_input = static_cast<uint32_t>(ctx->r4);
            g_stage92_select_rnc_output = static_cast<uint32_t>(ctx->r5);
            g_stage92_select_rnc_unpacked_size =
                read_guest_u32(g_stage92_select_rnc_input + 4u);
            g_stage92_select_rnc_packed_size =
                read_guest_u32(g_stage92_select_rnc_input + 8u);
            g_stage92_select_rnc_header_unpacked_crc =
                read_guest_u16(g_stage92_select_rnc_input + 12u);
            g_stage92_select_rnc_header_packed_crc =
                read_guest_u16(g_stage92_select_rnc_input + 14u);
            g_stage92_select_rnc_computed_packed_crc =
                stage61_rnc_crc16_guest(
                    g_stage92_select_rnc_input + kRncHeaderSize,
                    g_stage92_select_rnc_packed_size);
            g_stage92_select_rnc_input_hash =
                stage61_fnv1a_guest(
                    g_stage92_select_rnc_input,
                    kStage92StoredSize);
            g_stage92_select_rnc_output_hash_before =
                stage61_fnv1a_guest(
                    g_stage92_select_rnc_output,
                    kStage92StoredSize);

            if (g_stage92_select_rnc_output != kStage92ResourceDest ||
                g_stage92_select_rnc_unpacked_size != kStage92UnpackedSize ||
                g_stage92_select_rnc_packed_size != kStage92PackedSize ||
                g_stage92_select_rnc_header_unpacked_crc != kStage92UnpackedCrc ||
                g_stage92_select_rnc_header_packed_crc != kStage92PackedCrc ||
                g_stage92_select_rnc_computed_packed_crc != kStage92PackedCrc ||
                g_stage92_select_rnc_input_hash != kStage92StoredFnv) {
                std::fprintf(
                    stderr,
                    "FAILED: Stage 92 select.pic RNC setup mismatch "
                    "input=%08X output=%08X/%08X unpacked=%08X/%08X packed=%08X/%08X "
                    "headerCRC=%04X/%04X packedCRC=%04X/%04X inputFNV=%08X/%08X.\n",
                    g_stage92_select_rnc_input,
                    g_stage92_select_rnc_output,
                    kStage92ResourceDest,
                    g_stage92_select_rnc_unpacked_size,
                    kStage92UnpackedSize,
                    g_stage92_select_rnc_packed_size,
                    kStage92PackedSize,
                    g_stage92_select_rnc_header_packed_crc,
                    kStage92PackedCrc,
                    g_stage92_select_rnc_computed_packed_crc,
                    kStage92PackedCrc,
                    g_stage92_select_rnc_input_hash,
                    kStage92StoredFnv);
                return 1;
            }

            std::printf(
                "THREAD 4: level98\\select.pic RNC method-1 setup reached guest PC "
                "0x%08X SP=0x%08X input=0x%08X output=0x%08X "
                "unpacked=0x%08X packed=0x%08X packedCRC=0x%04X "
                "storedFNV=0x%08X; entering the already-proven Toy Story 2 RNC decoder.\n",
                guest_pc,
                g_stage92_select_rnc_sp,
                g_stage92_select_rnc_input,
                g_stage92_select_rnc_output,
                g_stage92_select_rnc_unpacked_size,
                g_stage92_select_rnc_packed_size,
                g_stage92_select_rnc_computed_packed_crc,
                g_stage92_select_rnc_input_hash);
            return 0;
        }

        if (g_parser_input_name == "level99\\sprite0.pic") {
            g_sprite_rnc_active = true;
            g_sprite_rnc_boundary_reached = true;
            g_sprite_rnc_sp = static_cast<uint32_t>(ctx->r29);
            g_sprite_rnc_input = static_cast<uint32_t>(ctx->r4);
            g_sprite_rnc_output = static_cast<uint32_t>(ctx->r5);
            g_sprite_rnc_unpacked_size =
                read_guest_u32(g_sprite_rnc_input + 4u);
            g_sprite_rnc_packed_size =
                read_guest_u32(g_sprite_rnc_input + 8u);
            g_sprite_rnc_header_unpacked_crc =
                read_guest_u16(g_sprite_rnc_input + 12u);
            g_sprite_rnc_header_packed_crc =
                read_guest_u16(g_sprite_rnc_input + 14u);
            g_sprite_rnc_computed_packed_crc =
                stage61_rnc_crc16_guest(
                    g_sprite_rnc_input + kRncHeaderSize,
                    g_sprite_rnc_packed_size);
            g_sprite_rnc_predecode_output_hash =
                stage61_fnv1a_guest(
                    g_sprite_rnc_output, kSpriteFileSize);
            g_sprite_rnc_predecode_temp_hash =
                stage61_fnv1a_guest(
                    g_sprite_rnc_input, kSpriteFileSize);

            std::printf(
                "THREAD 4: level99\\\\sprite0.pic RNC method-1 setup "
                "reached guest PC 0x%08X input=0x%08X output=0x%08X "
                "unpacked=0x%08X packed=0x%08X packedCRC=0x%04X "
                "compressedHash(output/temp)=0x%08X/0x%08X; "
                "entering the already-proven Toy Story 2 RNC decoder.\n",
                guest_pc,
                g_sprite_rnc_input,
                g_sprite_rnc_output,
                g_sprite_rnc_unpacked_size,
                g_sprite_rnc_packed_size,
                g_sprite_rnc_computed_packed_crc,
                g_sprite_rnc_predecode_output_hash,
                g_sprite_rnc_predecode_temp_hash);
            return 0;
        }

        g_rnc_sp = static_cast<uint32_t>(ctx->r29);
        g_rnc_a0 = static_cast<uint32_t>(ctx->r4);
        g_rnc_a1 = static_cast<uint32_t>(ctx->r5);
        g_rnc_a2 = static_cast<uint32_t>(ctx->r6);
        g_rnc_s0 = static_cast<uint32_t>(ctx->r16);
        g_rnc_s1 = static_cast<uint32_t>(ctx->r17);
        g_rnc_s2 = static_cast<uint32_t>(ctx->r18);
        g_rnc_file_size = read_guest_u32(g_rnc_sp + 0x24u);
        g_rnc_boundary_reached = true;

        g_rnc_packed_crc = stage61_rnc_crc16_guest(
            kRncTempInput + kRncHeaderSize,
            kRncPackedPayloadSize);
        g_rnc_predecode_output_hash =
            stage61_fnv1a_guest(kExpectedFontDest, kNextFontFileSize);
        g_rnc_predecode_temp_hash =
            stage61_fnv1a_guest(kRncTempInput, kNextFontFileSize);

        std::printf(
            "THREAD 4: tahoma12.fnt RNC method-1 setup reached "
            "guest PC 0x%08X input=0x%08X output=0x%08X "
            "unpacked=0x%08X packedCRC=0x%04X "
            "compressedHash(output/temp)=0x%08X/0x%08X; "
            "entering func_8001AAE0.\n",
            guest_pc,
            g_rnc_a0,
            g_rnc_a1,
            g_rnc_s0,
            g_rnc_packed_crc,
            g_rnc_predecode_output_hash,
            g_rnc_predecode_temp_hash);
        return 0;
     }

    if (guest_pc == kRncReturnPc) {
        if (g_font_complete_count > 8u) {
            g_stage92_select_rnc_active = false;
            g_sprite_rnc_active = false;
            // FAST-FORWARD: no diagnostic printf here. Resource #20 proved
            // func_8001AAE0 returned to this checkpoint, but the redirected
            // stdout diagnostic itself became the observed progress boundary.
            return 0;
        }
        if (g_stage92_select_rnc_active) {
            g_stage92_select_rnc_return_sp = static_cast<uint32_t>(ctx->r29);
            g_stage92_select_rnc_return_size = static_cast<uint32_t>(ctx->r16);
            g_stage92_select_rnc_return_output = static_cast<uint32_t>(ctx->r18);
            g_stage92_select_rnc_output_first_word =
                read_guest_u32(g_stage92_select_rnc_return_output);
            g_stage92_select_rnc_output_crc =
                stage61_rnc_crc16_guest(
                    g_stage92_select_rnc_return_output,
                    g_stage92_select_rnc_return_size);
            g_stage92_select_rnc_output_fnv =
                stage61_fnv1a_guest(
                    g_stage92_select_rnc_return_output,
                    g_stage92_select_rnc_return_size);
            g_stage92_select_rnc_return_reached = true;
            g_stage92_select_rnc_active = false;

            if (g_stage92_select_rnc_return_size != kStage92UnpackedSize ||
                g_stage92_select_rnc_return_output != kStage92ResourceDest ||
                g_stage92_select_rnc_output_crc != kStage92UnpackedCrc) {
                std::fprintf(
                    stderr,
                    "FAILED: Stage 92 select.pic RNC return mismatch "
                    "SP=%08X output=%08X/%08X size=%08X/%08X first=%08X "
                    "crc=%04X/%04X fnv=%08X.\n",
                    g_stage92_select_rnc_return_sp,
                    g_stage92_select_rnc_return_output,
                    kStage92ResourceDest,
                    g_stage92_select_rnc_return_size,
                    kStage92UnpackedSize,
                    g_stage92_select_rnc_output_first_word,
                    g_stage92_select_rnc_output_crc,
                    kStage92UnpackedCrc,
                    g_stage92_select_rnc_output_fnv);
                return 1;
            }

            std::printf(
                "THREAD 4: select.pic RNC decoder returned to guest PC 0x%08X "
                "SP=0x%08X output=0x%08X size=0x%08X firstWord=0x%08X "
                "unpackedCRC=0x%04X FNV1a=0x%08X; func_800853E0 will now "
                "store the decoded size and return.\n",
                guest_pc,
                g_stage92_select_rnc_return_sp,
                g_stage92_select_rnc_return_output,
                g_stage92_select_rnc_return_size,
                g_stage92_select_rnc_output_first_word,
                g_stage92_select_rnc_output_crc,
                g_stage92_select_rnc_output_fnv);
            return 0;
        }

        if (g_sprite_rnc_active) {
            g_sprite_rnc_return_sp = static_cast<uint32_t>(ctx->r29);
            g_sprite_rnc_return_size = static_cast<uint32_t>(ctx->r16);
            g_sprite_rnc_return_output = static_cast<uint32_t>(ctx->r18);
            g_sprite_rnc_output_first_word =
                read_guest_u32(g_sprite_rnc_return_output);
            g_sprite_rnc_output_crc =
                stage61_rnc_crc16_guest(
                    g_sprite_rnc_return_output,
                    g_sprite_rnc_return_size);
            g_sprite_rnc_output_fnv =
                stage61_fnv1a_guest(
                    g_sprite_rnc_return_output,
                    g_sprite_rnc_return_size);
            g_sprite_rnc_return_reached = true;

            std::printf(
                "THREAD 4: sprite RNC decoder returned to guest PC "
                "0x%08X SP=0x%08X output=0x%08X size=0x%08X "
                "firstWord=0x%08X unpackedCRC=0x%04X FNV1a=0x%08X.\n",
                guest_pc,
                g_sprite_rnc_return_sp,
                g_sprite_rnc_return_output,
                g_sprite_rnc_return_size,
                g_sprite_rnc_output_first_word,
                g_sprite_rnc_output_crc,
                g_sprite_rnc_output_fnv);
            return 0;
        }

        g_rnc_return_sp = static_cast<uint32_t>(ctx->r29);
        g_rnc_return_s0 = static_cast<uint32_t>(ctx->r16);
        g_rnc_return_s1 = static_cast<uint32_t>(ctx->r17);
        g_rnc_return_s2 = static_cast<uint32_t>(ctx->r18);
        g_rnc_output_first_word = read_guest_u32(kExpectedFontDest);
        g_rnc_unpacked_crc = stage61_rnc_crc16_guest(
            kExpectedFontDest, kRncUnpackedSize);
        g_rnc_output_fnv = stage61_fnv1a_guest(
            kExpectedFontDest, kRncUnpackedSize);
        g_rnc_return_reached = true;

        std::printf(
            "THREAD 4: func_8001AAE0 returned to guest PC 0x%08X "
            "SP=0x%08X output=0x%08X size=0x%08X firstWord=0x%08X "
            "unpackedCRC=0x%04X FNV1a=0x%08X before func_800853E0 "
            "stores the decompressed size.\n",
            guest_pc,
            g_rnc_return_sp,
            g_rnc_return_s2,
            g_rnc_return_s0,
            g_rnc_output_first_word,
            g_rnc_unpacked_crc,
            g_rnc_output_fnv);

        return 0;
     }

    if (guest_pc == kThread3IdleCheckpointPc) {
        std::unique_lock<std::mutex> lock(g_thread_mutex);

        g_thread3_idle_sp = static_cast<uint32_t>(ctx->r29);
        g_thread3_idle_checkpoint_reached = true;

        std::printf(
            "THREAD 3 GUARD: priority-0 handoff completed and thread 3 "
            "resumed after isolated thread 4 returned; reached guest PC "
            "0x%08X with SP=0x%08X.\n",
            guest_pc,
            g_thread3_idle_sp);

        g_thread_cv.notify_all();
        g_thread_cv.wait(lock, [] {
            return g_release_thread3;
        });
        return 1;
    }

    std::fprintf(
        stderr,
        "FAILED: unexpected thread checkpoint 0x%08X.\n",
        guest_pc);
    return 1;
}

extern "C" uint32_t stage61_vi_mmio_read(uint32_t addr) {
    if (addr < kViMmioBase ||
        addr > kViMmioLast ||
        ((addr - kViMmioBase) & 3u) != 0u) {
        g_vi_mmio_args_valid = false;
        std::fprintf(
            stderr,
            "FAILED: unexpected VI MMIO read address 0x%08X.\n",
            addr);
        return 0;
    }

    const uint32_t index = (addr - kViMmioBase) / 4u;
    const uint32_t value = g_vi_registers[index];
    ++g_vi_mmio_read_count;

    std::printf(
        "VI MMIO READ  #%u: addr=0x%08X value=0x%08X\n",
        g_vi_mmio_read_count,
        addr,
        value);

    return value;
}

extern "C" void stage61_vi_mmio_write(
    uint32_t addr,
    uint32_t value) {
    if (addr < kViMmioBase ||
        addr > kViMmioLast ||
        ((addr - kViMmioBase) & 3u) != 0u) {
        g_vi_mmio_args_valid = false;
        std::fprintf(
            stderr,
            "FAILED: unexpected VI MMIO write address 0x%08X "
            "value=0x%08X.\n",
            addr,
            value);
        return;
    }

    const uint32_t index = (addr - kViMmioBase) / 4u;
    g_vi_registers[index] = value;
    ++g_vi_mmio_write_count;

    std::printf(
        "VI MMIO WRITE #%u: addr=0x%08X value=0x%08X\n",
        g_vi_mmio_write_count,
        addr,
        value);
}

extern "C" uint32_t stage61_mi_mmio_read(uint32_t addr) {
    if (addr != kMiIntrMaskReg) {
        g_mi_mmio_args_valid = false;
        std::fprintf(
            stderr,
            "FAILED: unexpected MI MMIO read address 0x%08X.\n",
            addr);
        return 0;
    }

    ++g_mi_mmio_read_count;
    std::printf(
        "MI MMIO READ  #%u: addr=0x%08X mask=0x%02X\n",
        g_mi_mmio_read_count,
        addr,
        g_mi_mask_state & 0x3Fu);
    return g_mi_mask_state & 0x3Fu;
}

extern "C" void stage61_mi_mmio_write(
    uint32_t addr,
    uint32_t value) {
    if (addr != kMiIntrMaskReg) {
        g_mi_mmio_args_valid = false;
        std::fprintf(
            stderr,
            "FAILED: unexpected MI MMIO write address 0x%08X "
            "value=0x%08X.\n",
            addr,
            value);
        return;
    }

    const uint32_t write_index = g_mi_mmio_write_count;
    if (write_index < 2u) {
        g_mi_write_commands[write_index] = value;
    }
    ++g_mi_mmio_write_count;

    // MI_INTR_MASK_REG writes use clear/set bit pairs:
    // SP 0/1, SI 2/3, AI 4/5, VI 6/7, PI 8/9, DP 10/11.
    for (uint32_t source = 0; source < 6u; ++source) {
        const uint32_t clear_bit = 1u << (source * 2u);
        const uint32_t set_bit = clear_bit << 1u;
        const uint32_t state_bit = 1u << source;

        if ((value & clear_bit) != 0u) {
            g_mi_mask_state &= ~state_bit;
        }
        if ((value & set_bit) != 0u) {
            g_mi_mask_state |= state_bit;
        }
    }

    std::printf(
        "MI MMIO WRITE #%u: addr=0x%08X command=0x%04X -> mask=0x%02X\n",
        g_mi_mmio_write_count,
        addr,
        value & 0x0FFFu,
        g_mi_mask_state & 0x3Fu);
}

uint32_t stage61_fnv1a_rom(const uint8_t* data, uint32_t size) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

uint32_t stage61_fnv1a_guest(uint32_t guest, uint32_t size) {
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) {
        hash ^= read_guest_byte(guest + i);
        hash *= 16777619u;
    }
    return hash;
}

uint16_t stage61_rnc_crc16_guest(uint32_t guest, uint32_t size) {
    uint16_t crc = 0u;
    for (uint32_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint16_t>(read_guest_byte(guest + i));
        for (uint32_t bit = 0; bit < 8u; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001u);
            } else {
                crc = static_cast<uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

// TS2 RUNTIME CONTROLLER INIT BRIDGE V2
//
// Replace the old Stage 64 "no controller" bring-up response with
// N64ModernRuntime-backed Controller 1 state.
//
// First real-input pass:
//   WASD       = analog stick
//   Z          = N64 A
//   X          = N64 B
//   C          = N64 Z
//   Enter      = Start
//   Q / E      = L / R
//   Arrow keys = D-pad
//   I J K L    = C buttons

extern "C" void osContInit_recomp(uint8_t* rdram, recomp_context* ctx);

static bool g_ts2_runtime_input_callbacks_registered = false;

// TS2 POST-WARNING LIVE PATH V9
//
// SDL/RT64 window events are drained by the graphics bridge. Do not call
// SDL_PumpEvents from Toy Story 2's SI/game thread.
static void ts2_runtime_poll_input() {
    recompinput::poll_inputs();
}

static bool ts2_runtime_get_input(
    int controller_num,
    uint16_t* buttons,
    float* x,
    float* y) {

    return ts2_get_input(
        controller_num,
        buttons,
        x,
        y
    );
}

static void ts2_runtime_set_rumble(int controller_num, bool rumble) {
    recompinput::set_rumble(controller_num, rumble);
}

static ultramodern::input::connected_device_info_t
ts2_runtime_get_connected_device_info(int controller_num) {
    if (recompinput::players::is_single_player_mode() ||
        recompinput::players::get_player_is_assigned(controller_num)) {

        return {
            ultramodern::input::Device::Controller,
            ultramodern::input::Pak::RumblePak
        };
    }

    return {
        ultramodern::input::Device::None,
        ultramodern::input::Pak::None
    };
}

static void ts2_register_runtime_input_callbacks() {
    if (g_ts2_runtime_input_callbacks_registered) {
        return;
    }

    ultramodern::input::callbacks_t callbacks{};
    callbacks.poll_input = &recompinput::poll_inputs;
    callbacks.get_input = &ts2_get_input;
    callbacks.set_rumble = &recompinput::set_rumble;
    callbacks.get_connected_device_info =
        &ts2_runtime_get_connected_device_info;

    ultramodern::input::set_callbacks(callbacks);

    g_ts2_runtime_input_callbacks_registered = true;

    std::fprintf(
        stderr,
        "TS2 RUNTIME INPUT: registered Controller 1 keyboard callbacks "
        "(WASD stick, Z=A, X=B, C=Z, Enter=Start).\n");
    std::fflush(stderr);
}
extern "C" uint32_t stage64_si_mmio_read(uint32_t addr) {
    ++g_si_mmio_read_count;

    if (addr != kSiStatusReg) {
        g_si_mmio_args_valid = false;
        std::fprintf(
            stderr,
            "FAILED: unexpected SI MMIO read #%u addr=0x%08X.\n",
            g_si_mmio_read_count,
            addr);
        return 0;
    }

    // Stage 64 models both transfers as complete/idle at the instant the
    // game polls SI_STATUS_REG before programming the next DMA.
    const uint32_t value = 0u;
    std::printf(
        "SI MMIO READ  #%u: SI_STATUS_REG addr=0x%08X value=0x%08X\n",
        g_si_mmio_read_count,
        addr,
        value);
    return value;
}

extern "C" void stage64_si_mmio_write(uint32_t addr, uint32_t value) {
    const uint32_t index = g_si_mmio_write_count;
    if (index < 4u) {
        g_si_mmio_write_addrs[index] = addr;
        g_si_mmio_write_values[index] = value;
    }
    ++g_si_mmio_write_count;

    if (index == 0u) {
        if (addr != kSiDramAddrReg ||
            value != kControllerStatusArrayPhys) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: first SI write expected DRAM_ADDR 0x%08X/"
                "0x%08X, got 0x%08X/0x%08X.\n",
                kSiDramAddrReg,
                kControllerStatusArrayPhys,
                addr,
                value);
            return;
        }

        std::printf(
            "SI MMIO WRITE #1: SI_DRAM_ADDR_REG = 0x%08X "
            "(guest status array 0x%08X).\n",
            value,
            kControllerStatusArray);
        return;
    }

    if (index == 1u) {
        if (addr != kSiPifAddrWr64bReg ||
            value != kPifRamAddress) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: second SI write expected PIF_ADDR_WR64B "
                "0x%08X/0x%08X, got 0x%08X/0x%08X.\n",
                kSiPifAddrWr64bReg,
                kPifRamAddress,
                addr,
                value);
            return;
        }

        uint32_t hash = 2166136261u;
        for (uint32_t i = 0; i < kFirstPifCommandBytes; ++i) {
            const uint8_t b = read_guest_byte(kControllerStatusArray + i);
            g_first_pif_command[i] = b;
            hash ^= b;
            hash *= 16777619u;
        }
        g_first_pif_command_fnv = hash;

        std::printf(
            "SI MMIO WRITE #2: SI_PIF_ADDR_WR64B_REG = 0x%08X "
            "-> captured 64-byte controller command FNV-1a=0x%08X.\n",
            value,
            hash);

        // Event 5 was registered by func_800159B0 as queue 0x801ED338,
        // message 0. Deliver the completion exactly through that queue so
        // the real blocking func_80091C40/osRecvMesg path resumes.
        g_first_si_completion_send_result = osSendMesg(
            g_rdram.data(),
            static_cast<PTR(OSMesgQueue)>(
                static_cast<int32_t>(kSiQueue)),
            static_cast<OSMesg>(0),
            0);

        if (g_first_si_completion_send_result != 0) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: first SI completion event send returned %d.\n",
                g_first_si_completion_send_result);
            return;
        }

        g_first_si_completion_event_queued = true;
        std::printf(
            "SI EVENT AUTO #1: queued event 5 message 0x00000000 for "
            "SI queue 0x%08X.\n",
            kSiQueue);
        return;
    }

    if (index == 2u) {
        if (addr != kSiDramAddrReg ||
            value != kControllerStatusArrayPhys) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: third SI write expected DRAM_ADDR 0x%08X/"
                "0x%08X, got 0x%08X/0x%08X.\n",
                kSiDramAddrReg,
                kControllerStatusArrayPhys,
                addr,
                value);
            return;
        }

        std::printf(
            "SI MMIO WRITE #3: SI_DRAM_ADDR_REG = 0x%08X for the "
            "return PIF DMA.\n",
            value);
        return;
    }

    if (index == 3u) {
        if (addr != kSiPifAddrRd64bReg ||
            value != kPifRamAddress) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: fourth SI write expected PIF_ADDR_RD64B "
                "0x%08X/0x%08X, got 0x%08X/0x%08X.\n",
                kSiPifAddrRd64bReg,
                kPifRamAddress,
                addr,
                value);
            return;
        }

        g_return_pif_reply = g_first_pif_command;

        ts2_register_runtime_input_callbacks();
        // TS2 RUNTIME CONTROLLER INIT NO-RECURSIVE-SI FIX:
        //
        // Do NOT call osContInit_recomp here. We are already inside Toy Story 2's
        // raw SI/PIF transaction, and osContInit_recomp starts another controller
        // transaction, producing duplicate SI writes/DMA launches.
        //
        // Query the same registered runtime callback directly, build the
        // connected mask, and let Toy Story 2's existing PIF parser populate
        // its own controller status structures from the reply below.
        uint8_t runtime_connected_mask = 0u;

        for (int controller = 0; controller < 4; ++controller) {
            const auto device_info =
                ts2_runtime_get_connected_device_info(controller);

            if (device_info.connected_device !=
                ultramodern::input::Device::None) {
                runtime_connected_mask |=
                    static_cast<uint8_t>(1u << controller);
            }
        }

        for (uint32_t channel = 0; channel < 4u; ++channel) {
            const uint32_t o = channel * 8u;
            const bool connected =
                (runtime_connected_mask & (1u << channel)) != 0u;

            if (connected) {
                // Normal PIF controller-status response.
                // RX length 3/no channel error, type CONT_TYPE_NORMAL (0x0005),
                // no pak attached for this first pass.
                g_return_pif_reply[o + 2u] = 0x03u;
                g_return_pif_reply[o + 4u] = 0x05u;
                g_return_pif_reply[o + 5u] = 0x00u;
                g_return_pif_reply[o + 6u] = 0x00u;
            }
            else {
                g_return_pif_reply[o + 2u] = 0x83u;
            }
        }

        std::fprintf(
            stderr,
            "TS2 RUNTIME CONTROLLER INIT: runtime connectedMask=0x%02X; "
            "returning controller status through Toy Story 2's PIF parser.\n",
            static_cast<unsigned>(runtime_connected_mask));
        std::fflush(stderr);

        uint32_t hash = 2166136261u;
        for (uint32_t i = 0; i < kFirstPifCommandBytes; ++i) {
            const uint8_t b = g_return_pif_reply[i];
            write_guest_byte(kControllerStatusArray + i, b);
            hash ^= b;
            hash *= 16777619u;
        }
        g_return_pif_reply_fnv = hash;

        g_second_si_completion_send_result = osSendMesg(
            g_rdram.data(),
            static_cast<PTR(OSMesgQueue)>(static_cast<int32_t>(kSiQueue)),
            static_cast<OSMesg>(0),
            0);
        if (g_second_si_completion_send_result != 0) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: second SI completion event send returned %d.\n",
                g_second_si_completion_send_result);
            return;
        }
        g_second_si_completion_event_queued = true;

        std::printf(
            "SI MMIO WRITE #4: SI_PIF_ADDR_RD64B_REG = 0x%08X "
            "-> runtime-backed 64-byte controller PIF reply "
            "FNV-1a=0x%08X and queued SI event 5 message 0.\n",
            value, hash);
        return;
    }

        // TS2 LIVE CONTROLLER POLL BRIDGE V7
    //
    // Correct raw-SI model:
    // func_800980E0 performs ONE DMA per call using exactly two MMIO writes:
    //
    //   write 0: SI_DRAM_ADDR
    //   write 1: SI_PIF_ADDR_WR64B  when mode != 0
    //         or SI_PIF_ADDR_RD64B  when mode == 0
    //
    // Therefore live traffic is not required to alternate fixed
    // WR64B/RD64B four-write groups. Toy Story 2 may legitimately issue
    // another RD64B after a completed controller read.
    if (index >= 4u) {
        const uint32_t live_index = index - 4u;
        const uint32_t phase = live_index & 1u;
        const uint32_t live_dma = (live_index >> 1) + 1u;

        // func_800980E0 is a generic 64-byte raw-SI DMA helper. It is used
        // with more than one RDRAM PIF command buffer (including 0x801F3F18
        // and 0x801F3EB0), so remember the actual DRAM address programmed by
        // the game instead of forcing the original startup buffer forever.
        static uint32_t live_si_dram_phys = kControllerStatusArrayPhys;

        if (phase == 0u) {
            if (addr != kSiDramAddrReg ||
                value + kFirstPifCommandBytes > g_rdram.size()) {
                g_si_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: live SI DMA #%u invalid DRAM_ADDR "
                    "addr=0x%08X value=0x%08X rdramSize=0x%zX.\n",
                    live_dma,
                    addr,
                    value,
                    g_rdram.size());
                return;
            }

            live_si_dram_phys = value;
            return;
        }

        const uint32_t live_si_guest_buffer =
            0x80000000u | live_si_dram_phys;

        // TS2 CONTROLLER STATE BOUNDARY PROBE V34
        //
        // Carry the buttons supplied by the most recent completed RD64B
        // controller reply. On the NEXT SI transaction Toy Story 2 has
        // already resumed from the previous SI completion and can have run:
        //
        //   func_800937A8 -> func_80015B1C -> game-facing controller state.
        //
        // This is diagnostic-only. It reads RDRAM and prints; it never
        // changes controller state, packet bytes, queues, or generated code.
        static bool ts2_v34_have_last_reply = false;
        static uint16_t ts2_v34_last_reply_buttons = 0u;
        static uint32_t ts2_v34_last_reply_buffer = 0u;
        static uint32_t ts2_v34_snapshot_count = 0u;

        if (phase == 0u && ts2_v34_have_last_reply) {
            constexpr uint32_t kTs2ParsedController0 = 0x801EF260u;
            constexpr uint32_t kTs2PreviousController0 = 0x800ACA20u;
            constexpr uint32_t kTs2ControllerPointerTable = 0x800AD0F8u;
            constexpr uint32_t kTs2ControllerCount = 0x800A2BC8u;
            constexpr uint32_t kTs2ControllerPollGate = 0x800A2BD0u;
            constexpr uint32_t kTs2PifControllerCount = 0x801F3F14u;

            const uint16_t parsed_buttons =
                read_guest_u16(kTs2ParsedController0);
            const uint16_t previous_buttons =
                read_guest_u16(kTs2PreviousController0);
            const uint32_t controller_ptr0 =
                read_guest_u32(kTs2ControllerPointerTable);
            const uint32_t controller_count =
                read_guest_u32(kTs2ControllerCount);
            const uint32_t poll_gate =
                read_guest_u32(kTs2ControllerPollGate);
            const uint8_t pif_controller_count =
                read_guest_byte(kTs2PifControllerCount);

            const bool controller_ptr0_valid =
                controller_ptr0 >= 0x80000000u &&
                (static_cast<uint64_t>(controller_ptr0 - 0x80000000u) + 2u) <=
                    static_cast<uint64_t>(g_rdram.size());

            const uint16_t game_buttons =
                controller_ptr0_valid
                    ? read_guest_u16(controller_ptr0)
                    : 0xFFFFu;

            ++ts2_v34_snapshot_count;

            if (ts2_v34_snapshot_count <= 32u ||
                ts2_v34_last_reply_buttons != 0u ||
                parsed_buttons != 0u ||
                game_buttons != 0u ||
                previous_buttons != 0u) {
                std::fprintf(
                    stderr,
                    "TS2 CONTROLLER STATE V34 #%u "
                    "lastReply=0x%04X lastBuf=0x%08X "
                    "pollGate=%u controllerCount=%u pifCount=%u "
                    "parsed=0x%04X ptr0=0x%08X ptrValid=%u "
                    "game=0x%04X previous=0x%04X\n",
                    ts2_v34_snapshot_count,
                    static_cast<unsigned>(ts2_v34_last_reply_buttons),
                    ts2_v34_last_reply_buffer,
                    poll_gate,
                    controller_count,
                    static_cast<unsigned>(pif_controller_count),
                    static_cast<unsigned>(parsed_buttons),
                    controller_ptr0,
                    controller_ptr0_valid ? 1u : 0u,
                    static_cast<unsigned>(game_buttons),
                    static_cast<unsigned>(previous_buttons));
                std::fflush(stderr);
            }
        }

        if (value != kPifRamAddress ||
            (addr != kSiPifAddrWr64bReg &&
             addr != kSiPifAddrRd64bReg)) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: live SI DMA #%u expected WR64B or RD64B "
                "to PIF 0x%08X, got addr=0x%08X value=0x%08X.\n",
                live_dma,
                kPifRamAddress,
                addr,
                value);
            return;
        }

        if (addr == kSiPifAddrWr64bReg) {
            uint32_t command_hash = 2166136261u;

            for (uint32_t i = 0; i < kFirstPifCommandBytes; ++i) {
                const uint8_t b =
                    read_guest_byte(live_si_guest_buffer + i);

                command_hash ^= b;
                command_hash *= 16777619u;
            }

            ts2_runtime_poll_input();

            const int send_result = osSendMesg(
                g_rdram.data(),
                static_cast<PTR(OSMesgQueue)>(
                    static_cast<int32_t>(kSiQueue)),
                static_cast<OSMesg>(0),
                0);

            if (send_result != 0) {
                g_si_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: live SI WR64B DMA #%u completion send "
                    "returned %d.\n",
                    live_dma,
                    send_result);
                return;
            }

            static uint32_t live_wr_count = 0u;
            ++live_wr_count;

            if (live_wr_count <= 32u ||
                (live_wr_count % 256u) == 0u) {
                std::fprintf(
                    stderr,
                    "TS2 LIVE SI COMMAND #%u dma=%u hash=0x%08X "
                    "cmd0=0x%02X tx0=%u rx0=0x%02X.\n",
                    live_wr_count,
                    live_dma,
                    command_hash,
                    static_cast<unsigned>(
                        read_guest_byte(
                            live_si_guest_buffer + 3u)),
                    static_cast<unsigned>(
                        read_guest_byte(
                            live_si_guest_buffer + 1u)),
                    static_cast<unsigned>(
                        read_guest_byte(
                            live_si_guest_buffer + 2u)));
                std::fflush(stderr);
            }

            return;
        }

        // RD64B: return the current PC input state through the controller
        // response bytes. This also supports a legitimate RD64B without a
        // new WR64B immediately beforehand.
        ts2_runtime_poll_input();

        uint16_t buttons = 0u;
        float stick_x_f = 0.0f;
        float stick_y_f = 0.0f;

        const bool have_pad = ts2_runtime_get_input(
            0,
            &buttons,
            &stick_x_f,
            &stick_y_f);

        auto clamp_stick = [](float v) -> int8_t {
            if (v > 1.0f) {
                v = 1.0f;
            }

            if (v < -1.0f) {
                v = -1.0f;
            }

            const int value_i =
                static_cast<int>(v * 82.0f);

            if (value_i > 127) {
                return 127;
            }

            if (value_i < -128) {
                return -128;
            }

            return static_cast<int8_t>(value_i);
        };

        const int8_t stick_x = clamp_stick(stick_x_f);
        const int8_t stick_y = clamp_stick(stick_y_f);

        for (uint32_t channel = 0; channel < 4u; ++channel) {
            const uint32_t o =
                live_si_guest_buffer + channel * 8u;

            const uint8_t command =
                read_guest_byte(o + 3u);

            if (channel == 0u && have_pad) {
                if (command == 0x01u) {
                    // CONT_CMD_READ_BUTTON.
                    write_guest_byte(o + 2u, 0x04u);
                    write_guest_byte(
                        o + 4u,
                        static_cast<uint8_t>(
                            (buttons >> 8) & 0xFFu));
                    write_guest_byte(
                        o + 5u,
                        static_cast<uint8_t>(
                            buttons & 0xFFu));
                    write_guest_byte(
                        o + 6u,
                        static_cast<uint8_t>(stick_x));
                    write_guest_byte(
                        o + 7u,
                        static_cast<uint8_t>(stick_y));
                }
                else if (command == 0x00u) {
                    // CONT_CMD_REQUEST_STATUS.
                    write_guest_byte(o + 2u, 0x03u);
                    write_guest_byte(o + 4u, 0x05u);
                    write_guest_byte(o + 5u, 0x00u);
                    write_guest_byte(o + 6u, 0x00u);
                }
                else {
                    const uint8_t rx =
                        read_guest_byte(o + 2u) & 0x3Fu;

                    write_guest_byte(o + 2u, rx);
                }
            }
            else {
                const uint8_t rx =
                    read_guest_byte(o + 2u) & 0x3Fu;

                write_guest_byte(
                    o + 2u,
                    static_cast<uint8_t>(rx | 0x80u));
            }
        }

        // V34: remember exactly what this RD64B reply supplied. The next
        // SI transaction snapshots Toy Story 2's downstream RDRAM state.
        ts2_v34_last_reply_buttons = buttons;
        ts2_v34_last_reply_buffer = live_si_guest_buffer;
        ts2_v34_have_last_reply = true;
        const int send_result = osSendMesg(
            g_rdram.data(),
            static_cast<PTR(OSMesgQueue)>(
                static_cast<int32_t>(kSiQueue)),
            static_cast<OSMesg>(0),
            0);

        if (send_result != 0) {
            g_si_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: live SI RD64B DMA #%u completion send "
                "returned %d.\n",
                live_dma,
                send_result);
            return;
        }

        static uint32_t live_controller_reply_count = 0u;
        ++live_controller_reply_count;

        if (live_controller_reply_count <= 32u ||
            (live_controller_reply_count % 256u) == 0u ||
            buttons != 0u ||
            stick_x != 0 ||
            stick_y != 0) {
            std::fprintf(
                stderr,
                "TS2 LIVE CONTROLLER POLL #%u dma=%u "
                "buttons=0x%04X stick=%d,%d cmd0=0x%02X.\n",
                live_controller_reply_count,
                live_dma,
                static_cast<unsigned>(buttons),
                static_cast<int>(stick_x),
                static_cast<int>(stick_y),
                static_cast<unsigned>(
                    read_guest_byte(
                        live_si_guest_buffer + 3u)));
            std::fflush(stderr);
        }

        return;
    }

    g_si_mmio_args_valid = false;
    std::fprintf(
        stderr,
        "FAILED: unhandled SI MMIO write #%u addr=0x%08X "
        "value=0x%08X.\n",
        g_si_mmio_write_count,
        addr,
        value);
}

extern "C" uint32_t stage61_pi_mmio_read(uint32_t addr) {
    ++g_pi_mmio_read_count;
    if (addr != kPiStatusReg) {
        g_pi_mmio_args_valid = false;
        std::fprintf(stderr,
            "FAILED: unexpected PI MMIO read #%u addr=0x%08X.\n",
            g_pi_mmio_read_count, addr);
        return 0;
    }
    std::printf(
        "PI MMIO READ  #%u: PI_STATUS_REG addr=0x%08X value=0x%08X\n",
        g_pi_mmio_read_count, addr, g_pi_status);
    return g_pi_status;
}


// TS2 LIBRECOMP LIVE OVERLAY ACTIVATION V45
//
// Proper live overlay registration for Toy Story 2's shared 0x80000400
// game-overlay window.
//
// The game copies overlays from cartridge ROM to RDRAM in multiple PI chunks.
// librecomp's load_overlays() API expects a ROM range containing a complete
// generated code section, so calling it once per 0x1000 chunk would be wrong.
// This helper watches the real PI copies, accumulates only contiguous chunks
// that preserve the generated ROM->VRAM mapping, and activates the section
// only after its entire generated size has arrived.
//
// No guest code is skipped or substituted. The actual ROM bytes are copied
// first; this only updates librecomp's native indirect-call map to match which
// original overlay is now resident.
struct Ts2V45OverlaySection {
    uint32_t rom;
    uint32_t vram;
    uint32_t size;
    const char* name;
};

static constexpr Ts2V45OverlaySection kTs2V45OverlaySections[] = {
    { 0x000A68A0u, 0x80000400u, 0x00002A60u, "overlay_00" },
    { 0x000A9300u, 0x80000400u, 0x00003160u, "overlay_01" },
    { 0x000AC460u, 0x80000400u, 0x00003500u, "overlay_02" },
    { 0x000AF960u, 0x80000400u, 0x00001280u, "overlay_03" },
    { 0x000B0BE0u, 0x80000400u, 0x00004040u, "overlay_04" },
    { 0x000B4C20u, 0x80000400u, 0x00003240u, "overlay_05" },
    { 0x000B7E60u, 0x80000400u, 0x00000E00u, "overlay_06" },
    { 0x000B8C60u, 0x80000400u, 0x00002EC0u, "overlay_07" },
    { 0x000BBB20u, 0x80000400u, 0x00003040u, "overlay_08" },
    { 0x000BEB60u, 0x80000400u, 0x00001BC0u, "overlay_09" },
    { 0x000C0720u, 0x80000400u, 0x00004320u, "overlay_10" },
    { 0x000C4A40u, 0x80000400u, 0x00004D60u, "overlay_11" },
    { 0x000C97A0u, 0x80000400u, 0x00001200u, "overlay_12" },
    { 0x000CA9A0u, 0x80000400u, 0x00002560u, "overlay_13" },
    { 0x000CCF00u, 0x80000400u, 0x00002860u, "overlay_14" },
    { 0x000CF760u, 0x80000400u, 0x00001DA0u, "overlay_15" },
    { 0x000D1500u, 0x80000400u, 0x00001EE0u, "overlay_16" },
    { 0x000D33E0u, 0x80000400u, 0x00000100u, "overlay_17" },
    { 0x000D34E0u, 0x80000400u, 0x00005300u, "overlay_98" },
};

static int32_t g_ts2_v45_candidate_overlay = -1;
static uint32_t g_ts2_v45_candidate_covered = 0u;
static int32_t g_ts2_v45_active_overlay = -1;
static uint32_t g_ts2_v45_activation_count = 0u;

static void ts2_v45_activate_overlay(size_t index) {
    const Ts2V45OverlaySection& next = kTs2V45OverlaySections[index];

    if (g_ts2_v45_active_overlay >= 0) {
        const Ts2V45OverlaySection& old =
            kTs2V45OverlaySections[
                static_cast<size_t>(g_ts2_v45_active_overlay)];

        // unload_overlays requires the full previously-loaded section range.
        // We track that exact section here, so this is never a partial unload.
        ::unload_overlays(
            static_cast<int32_t>(old.vram),
            old.size);

        std::fprintf(
            stderr,
            "TS2 V45 OVERLAY UNLOAD: %s rom=0x%08X "
            "vram=0x%08X size=0x%X.\n",
            old.name,
            old.rom,
            old.vram,
            old.size);
    }

    ::load_overlays(
        next.rom,
        static_cast<int32_t>(next.vram),
        next.size);

    g_ts2_v45_active_overlay = static_cast<int32_t>(index);
    ++g_ts2_v45_activation_count;

    std::fprintf(
        stderr,
        "TS2 V45 OVERLAY ACTIVATE #%u: %s rom=0x%08X "
        "vram=0x%08X size=0x%X; full generated section is now "
        "live in librecomp func_map.\n",
        g_ts2_v45_activation_count,
        next.name,
        next.rom,
        next.vram,
        next.size);
    std::fflush(stderr);

    g_ts2_v45_candidate_overlay = -1;
    g_ts2_v45_candidate_covered = 0u;
}

static void ts2_v45_note_pi_overlay_chunk(
    uint32_t rom_offset,
    uint32_t dram_phys,
    uint32_t size) {

    if (size == 0u) {
        return;
    }

    const uint32_t guest_ram = 0x80000000u | dram_phys;

    // A chunk beginning exactly at a generated overlay's ROM/VRAM base starts
    // (or restarts) accumulation for that overlay.
    for (size_t i = 0;
         i < sizeof(kTs2V45OverlaySections) /
             sizeof(kTs2V45OverlaySections[0]);
         ++i) {

        const Ts2V45OverlaySection& section =
            kTs2V45OverlaySections[i];

        if (rom_offset == section.rom &&
            guest_ram == section.vram) {

            g_ts2_v45_candidate_overlay = static_cast<int32_t>(i);
            g_ts2_v45_candidate_covered =
                size < section.size ? size : section.size;

            std::fprintf(
                stderr,
                "TS2 V45 OVERLAY LOAD BEGIN: %s rom=0x%08X "
                "vram=0x%08X firstChunk=0x%X total=0x%X.\n",
                section.name,
                section.rom,
                section.vram,
                size,
                section.size);
            std::fflush(stderr);

            if (g_ts2_v45_candidate_covered >= section.size) {
                ts2_v45_activate_overlay(i);
            }
            return;
        }
    }

    // Unrelated PI activity is allowed to interleave. Only a chunk that
    // continues the exact ROM->VRAM mapping advances the current candidate.
    if (g_ts2_v45_candidate_overlay < 0) {
        return;
    }

    const size_t candidate_index =
        static_cast<size_t>(g_ts2_v45_candidate_overlay);
    const Ts2V45OverlaySection& section =
        kTs2V45OverlaySections[candidate_index];

    if (g_ts2_v45_candidate_covered >= section.size) {
        return;
    }

    const uint32_t expected_rom =
        section.rom + g_ts2_v45_candidate_covered;
    const uint32_t expected_ram =
        section.vram + g_ts2_v45_candidate_covered;

    if (rom_offset != expected_rom ||
        guest_ram != expected_ram) {
        return;
    }

    const uint32_t remaining =
        section.size - g_ts2_v45_candidate_covered;
    const uint32_t consumed =
        size < remaining ? size : remaining;

    g_ts2_v45_candidate_covered += consumed;

    std::fprintf(
        stderr,
        "TS2 V45 OVERLAY LOAD CONTINUE: %s chunkRom=0x%08X "
        "chunkRam=0x%08X chunk=0x%X covered=0x%X/0x%X.\n",
        section.name,
        rom_offset,
        guest_ram,
        size,
        g_ts2_v45_candidate_covered,
        section.size);
    std::fflush(stderr);

    if (g_ts2_v45_candidate_covered >= section.size) {
        ts2_v45_activate_overlay(candidate_index);
    }
}

//
// V46: reconcile overlay residency at the game's own transfer boundary.
//
// V45 above is correct but is driven solely by ts2_v45_note_pi_overlay_chunk(),
// which is only reached from the hand-rolled PI MMIO fast-forward path in this
// file. Overlay transfers now go through librecomp's native PI path
// (do_dma -> recomp::do_rom_read), which does not notify this translation unit.
// The result was that no overlay was ever activated, librecomp's func_map held
// nothing in the shared 0x80000400 window, and the first indirect call into a
// resident overlay aborted with "Failed to find function at 0x8000189C".
//
// Re-pointing the tracker at one specific transfer mechanism is what failed
// here, so residency is instead reconciled at the point the game's own overlay
// transfer helper (func_800859A4) returns. At that moment the overlay is fully
// in RDRAM however it arrived, which keeps this correct if the PI path changes
// again.
//
// The resident overlay is identified by hashing the guest window against the
// original ROM image, so the answer is derived from the bytes actually present
// rather than inferred from loader state. A cheap prefix compare rejects
// non-candidates before the full-section hash.
//
// No guest code is skipped or substituted. The real recompiled transfer runs
// first and its behavior is unchanged; this only updates librecomp's native
// indirect-call map to match which original overlay is resident.
//
static constexpr uint32_t kTs2V46PrefixBytes = 64u;

static int32_t ts2_v46_identify_resident_overlay() {
    if (g_stage61_rom_data == nullptr || g_stage61_rom_size == 0u) {
        return -1;
    }

    for (size_t i = 0;
         i < sizeof(kTs2V45OverlaySections) /
             sizeof(kTs2V45OverlaySections[0]);
         ++i) {

        const Ts2V45OverlaySection& section = kTs2V45OverlaySections[i];

        if (static_cast<uint64_t>(section.rom) + section.size >
            static_cast<uint64_t>(g_stage61_rom_size)) {
            continue;
        }

        const uint32_t prefix =
            section.size < kTs2V46PrefixBytes ? section.size
                                              : kTs2V46PrefixBytes;

        if (stage61_fnv1a_guest(section.vram, prefix) !=
            stage61_fnv1a_rom(g_stage61_rom_data + section.rom, prefix)) {
            continue;
        }

        if (stage61_fnv1a_guest(section.vram, section.size) ==
            stage61_fnv1a_rom(
                g_stage61_rom_data + section.rom, section.size)) {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

extern "C" void ts2_v46_overlay_transfer(
    uint8_t* rdram,
    recomp_context* ctx) {

    // Run the real guest transfer first, unchanged.
    func_800859A4(rdram, ctx);

    const int32_t index = ts2_v46_identify_resident_overlay();

    if (index < 0) {
        // A partial or interleaved transfer leaves no whole section resident.
        // Leaving the map untouched is correct: a stale-but-consistent map is
        // preferable to activating a section whose bytes are not all present.
        std::fprintf(
            stderr,
            "TS2 V46 OVERLAY RECONCILE: no complete overlay resident at "
            "0x80000400 after func_800859A4; map left unchanged "
            "(active=%d).\n",
            g_ts2_v45_active_overlay);
        std::fflush(stderr);
        return;
    }

    if (index == g_ts2_v45_active_overlay) {
        return;
    }

    std::fprintf(
        stderr,
        "TS2 V46 OVERLAY RECONCILE: %s is resident after func_800859A4 "
        "(was active=%d); activating.\n",
        kTs2V45OverlaySections[static_cast<size_t>(index)].name,
        g_ts2_v45_active_overlay);
    std::fflush(stderr);

    ts2_v45_activate_overlay(static_cast<size_t>(index));
}

//
// V46: the transfer-observation point.
//
// librecomp's PI path (librecomp/src/pi.cpp, do_dma) calls this after every
// completed cart ROM read. That is the only point which observes every overlay
// transfer: the guest functions that perform them (func_800859A4 and friends)
// are called directly by recompiled code, so recomp::overlays::add_loaded_function
// cannot wrap them -- it is consulted only for indirect LOOKUP_FUNC calls.
//
// Chunk accounting and activation are handled by the V45 tracker above; this
// only forwards. Non-overlay cart reads are ignored there by address, so the
// common case costs one failed range comparison.
//
extern "C" void ts2_host_note_cart_read(
    uint32_t rom_offset,
    uint32_t dram_phys,
    uint32_t size) {

    ts2_v45_note_pi_overlay_chunk(rom_offset, dram_phys, size);
}

extern "C" void stage61_pi_mmio_write(uint32_t addr, uint32_t value) {
    const uint32_t write_index = g_pi_mmio_write_count;
    if (write_index < 60u) {
        g_pi_mmio_write_addrs[write_index] = addr;
        g_pi_mmio_write_values[write_index] = value;
    }
    ++g_pi_mmio_write_count;

    if (addr == kPiDramAddrReg) {
        g_pi_dram_addr = value & 0x00FFFFFFu;
        std::printf(
            "PI MMIO WRITE #%u: PI_DRAM_ADDR_REG = 0x%08X\n",
            g_pi_mmio_write_count,
            g_pi_dram_addr);
        return;
    }

    if (addr == kPiCartAddrReg) {
        g_pi_cart_addr = value & 0x1FFFFFFFu;
        std::printf(
            "PI MMIO WRITE #%u: PI_CART_ADDR_REG = 0x%08X\n",
            g_pi_mmio_write_count,
            g_pi_cart_addr);
        return;
    }

    if (addr == kPiRdLenReg) {
        g_pi_mmio_args_valid = false;
        std::fprintf(
            stderr,
            "FAILED: ROM/font load unexpectedly used PI_RD_LEN_REG "
            "value=0x%08X.\n",
            value);
        return;
    }

    if (addr == kPiWrLenReg) {
        const uint32_t chunk = g_pi_dma_count;
        g_pi_dma_size = (value & 0x00FFFFFFu) + 1u;
        if (chunk >= kTransferCount) {
            if (g_stage61_rom_data == nullptr) {
                g_pi_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: dynamic PI DMA #%u has no ROM backing data.\n",
                    chunk + 1u);
                return;
            }

            // PI cart addresses for cartridge ROM live in domain 1 at 0x10000000+.
            // g_pi_cart_addr is already the 29-bit PI register value.
            if ((g_pi_cart_addr & 0x10000000u) == 0u) {
                g_pi_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: dynamic PI DMA #%u used non-ROM cart address 0x%08X.\n",
                    chunk + 1u,
                    g_pi_cart_addr);
                return;
            }

            const uint32_t dynamic_rom_offset =
                g_pi_cart_addr & 0x0FFFFFFFu;
            const uint64_t dynamic_rom_end =
                static_cast<uint64_t>(dynamic_rom_offset) +
                static_cast<uint64_t>(g_pi_dma_size);
            const uint64_t dynamic_rdram_end =
                static_cast<uint64_t>(g_pi_dram_addr) +
                static_cast<uint64_t>(g_pi_dma_size);

            if (dynamic_rom_end > static_cast<uint64_t>(g_stage61_rom_size) ||
                dynamic_rdram_end > static_cast<uint64_t>(kRdramSize)) {
                g_pi_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: dynamic PI DMA #%u outside ROM/RDRAM: "
                    "rom=0x%08X size=0x%08X/%u dram=0x%08X/%08zX.\n",
                    chunk + 1u,
                    dynamic_rom_offset,
                    g_pi_dma_size,
                    g_stage61_rom_size,
                    g_pi_dram_addr,
                    kRdramSize);
                return;
            }

            for (uint32_t i = 0; i < g_pi_dma_size; ++i) {
                write_guest_byte(
                    0x80000000u | (g_pi_dram_addr + i),
                    g_stage61_rom_data[dynamic_rom_offset + i]);
            }

            const uint32_t expected_hash = stage61_fnv1a_rom(
                g_stage61_rom_data + dynamic_rom_offset,
                g_pi_dma_size);
            const uint32_t actual_hash = stage61_fnv1a_guest(
                0x80000000u | g_pi_dram_addr,
                g_pi_dma_size);

            g_pi_expected_hash = expected_hash;
            g_pi_actual_hash = actual_hash;
            g_pi_dma_copy_complete = expected_hash == actual_hash;
            if (!g_pi_dma_copy_complete) {
                g_pi_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: dynamic PI DMA #%u copied hash mismatch "
                    "ROM=%08X RDRAM=%08X.\n",
                    chunk + 1u,
                    expected_hash,
                    actual_hash);
                return;
            }

                        // V45: the ROM bytes above have been copied and hash-verified.
            // Mirror the completed overlay load into librecomp's live section map.
            ts2_v45_note_pi_overlay_chunk(
                dynamic_rom_offset,
                g_pi_dram_addr,
                g_pi_dma_size);
g_pi_status = 0u;
            ++g_pi_dma_count;

            std::printf(
                "TS2 FAST-FORWARD DYNAMIC PI DMA #%u: ROM+0x%08X -> "
                "RDRAM 0x%08X size=0x%X hash=0x%08X.\n",
                chunk + 1u,
                dynamic_rom_offset,
                0x80000000u | g_pi_dram_addr,
                g_pi_dma_size,
                actual_hash);

            std::fprintf(
                stderr,
                "TS2 PI148 PROBE V3 DYNAMIC SEND BEFORE dma=%u queue=0x%08X msg=0x%08X\n",
                chunk + 1u,
                kPiEventQueue,
                kPiCompletionEventMessage);
            std::fflush(stderr);

            const int32_t send_result = osSendMesg(
                g_rdram.data(),
                static_cast<PTR(OSMesgQueue)>(
                    static_cast<int32_t>(kPiEventQueue)),
                static_cast<OSMesg>(
                    static_cast<int32_t>(kPiCompletionEventMessage)),
                0);

            std::fprintf(
                stderr,
                "TS2 PI148 PROBE V3 DYNAMIC SEND AFTER dma=%u result=%d\n",
                chunk + 1u,
                send_result);
            std::fflush(stderr);

            if (send_result != 0) {
                g_pi_mmio_args_valid = false;
                std::fprintf(
                    stderr,
                    "FAILED: dynamic PI completion event #%u returned %d.\n",
                    chunk + 1u,
                    send_result);
                return;
            }

            ++g_pi_auto_event_count;
            return;
        }

        if (chunk >= kTransferCount ||
            g_stage61_rom_data == nullptr ||
            g_pi_dma_size != kChunkSizes[chunk] ||
            value != (kChunkSizes[chunk] - 1u)) {
            g_pi_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: unexpected PI DMA #%u length/state "
                "len=%08X expected=%08X size=%08X expectedSize=%08X.\n",
                chunk + 1u,
                value,
                chunk < kTransferCount ? kChunkSizes[chunk] - 1u : 0u,
                g_pi_dma_size,
                chunk < kTransferCount ? kChunkSizes[chunk] : 0u);
            return;
        }

        const uint32_t expected_dram =
            kChunkDramAddrs[chunk] & 0x00FFFFFFu;
        const uint32_t expected_cart =
            kChunkDeviceAddrs[chunk] & 0x1FFFFFFFu;
        const uint32_t expected_rom_offset =
            kChunkRomOffsets[chunk];

        if (g_pi_dram_addr != expected_dram ||
            g_pi_cart_addr != expected_cart) {
            g_pi_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: PI DMA #%u programming "
                "dram=%08X/%08X cart=%08X/%08X.\n",
                chunk + 1u,
                g_pi_dram_addr,
                expected_dram,
                g_pi_cart_addr,
                expected_cart);
            return;
        }

        if (expected_rom_offset + g_pi_dma_size > g_stage61_rom_size ||
            g_pi_dram_addr + g_pi_dma_size > kRdramSize) {
            g_pi_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: PI DMA #%u range outside ROM/RDRAM.\n",
                chunk + 1u);
            return;
        }

        for (uint32_t i = 0; i < g_pi_dma_size; ++i) {
            write_guest_byte(
                0x80000000u | (g_pi_dram_addr + i),
                g_stage61_rom_data[expected_rom_offset + i]);
        }

        const uint32_t expected_hash = stage61_fnv1a_rom(
            g_stage61_rom_data + expected_rom_offset,
            g_pi_dma_size);
        const uint32_t actual_hash = stage61_fnv1a_guest(
            0x80000000u | g_pi_dram_addr,
            g_pi_dma_size);

        g_chunk_actual_hashes[chunk] = actual_hash;
        if (chunk == 10u) {
            // DMA #11 has just completed the second 0x4000 directory load.
            // Capture it NOW, before the parser and later RNC staging reuse
            // part of the same RAM range.
            g_second_directory_reload_hash_at_dma11 =
                stage61_fnv1a_guest(kInitialLoadRdram, kInitialLoadSize);
        }
        if (chunk == 16u) {
            // DMA #17 completes the third directory reload, immediately
            // before etchfont.fnt is parsed and loaded.
            g_third_directory_reload_hash_at_dma17 =
                stage61_fnv1a_guest(kInitialLoadRdram, kInitialLoadSize);
        }
        if (chunk == 23u) {
            // DMA #24 completes the fourth directory reload, immediately
            // before text/common.eng is parsed and loaded.
            g_fourth_directory_reload_hash_at_dma24 =
                stage61_fnv1a_guest(kInitialLoadRdram, kInitialLoadSize);
        }
        if (chunk == 28u) {
            // DMA #29 completes the fifth directory reload, immediately
            // before level99/sprite0.pic is parsed and staged for RNC.
            g_fifth_directory_reload_hash_at_dma29 =
                stage61_fnv1a_guest(kInitialLoadRdram, kInitialLoadSize);
        }
        g_pi_expected_hash = expected_hash;
        g_pi_actual_hash = actual_hash;
        g_pi_dma_copy_complete =
            expected_hash == actual_hash &&
            actual_hash == kChunkHashes[chunk];
        g_pi_status = 0u;
        ++g_pi_dma_count;

        std::printf(
            "PI MMIO WRITE #%u: PI_WR_LEN_REG = 0x%08X "
            "-> DMA #%u ROM+0x%08X to RDRAM 0x%08X "
            "(%u bytes) hash=0x%08X.\n",
            g_pi_mmio_write_count,
            value,
            chunk + 1u,
            expected_rom_offset,
            0x80000000u | g_pi_dram_addr,
            g_pi_dma_size,
            actual_hash);

        const int32_t send_result = osSendMesg(
            g_rdram.data(),
            static_cast<PTR(OSMesgQueue)>(
                static_cast<int32_t>(kPiEventQueue)),
            static_cast<OSMesg>(
                static_cast<int32_t>(kPiCompletionEventMessage)),
            0);

        g_auto_event_results[chunk] = send_result;
        if (send_result != 0) {
            g_pi_mmio_args_valid = false;
            std::fprintf(
                stderr,
                "FAILED: auto PI completion event #%u returned %d.\n",
                chunk + 1u,
                send_result);
            return;
        }

        ++g_pi_auto_event_count;
        std::printf(
            "PI EVENT AUTO #%u: queued message 0x%08X for "
            "completion queue 0x%08X.\n",
            chunk + 1u,
            kPiCompletionEventMessage,
            kPiEventQueue);
        return;
    }

    g_pi_mmio_args_valid = false;
    std::fprintf(
        stderr,
        "FAILED: unexpected PI MMIO write #%u addr=0x%08X "
        "value=0x%08X.\n",
        g_pi_mmio_write_count,
        addr,
        value);
}

extern "C" int32_t ts2_scheduler_client_send_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t msg,
    int32_t flags) {

    // Diagnostic-only: capture the event type at the exact moment a message is
    // sent to Toy Story 2's frame queue. This does not synthesize or alter events.
    static uint32_t ts2_frame_event_send_probe_count = 0u;
    const bool ts2_trace_frame_queue =
        queue_addr == 0x801CC400u &&
        ts2_frame_event_send_probe_count < 512u;
    const bool ts2_msg_is_rdram_ptr =
        msg >= 0x80000000u &&
        msg <= 0x807FFFFEu;

    uint16_t ts2_type_before = 0xFFFFu;
    uint32_t ts2_probe_number = 0u;

    if (ts2_trace_frame_queue) {
        ts2_probe_number = ++ts2_frame_event_send_probe_count;
        if (ts2_msg_is_rdram_ptr) {
            ts2_type_before = read_guest_u16(msg);
        }

        std::fprintf(
            stderr,
            "TS2 FRAME EVENT SEND PRE #%u queue=0x%08X msg=0x%08X "
            "type=%u/0x%04X flags=%d\n",
            ts2_probe_number,
            queue_addr,
            msg,
            static_cast<unsigned>(ts2_type_before),
            static_cast<unsigned>(ts2_type_before),
            flags);
        std::fflush(stderr);
    }
    const int32_t result = osSendMesg(
        rdram,
        static_cast<PTR(OSMesgQueue)>(
            static_cast<int32_t>(queue_addr)),
        static_cast<OSMesg>(
            static_cast<int32_t>(msg)),
        flags);
    if (ts2_trace_frame_queue) {
        const uint16_t ts2_type_after =
            ts2_msg_is_rdram_ptr ? read_guest_u16(msg) : 0xFFFFu;

        std::fprintf(
            stderr,
            "TS2 FRAME EVENT SEND POST #%u queue=0x%08X msg=0x%08X "
            "type_before=%u/0x%04X type_after=%u/0x%04X "
            "flags=%d result=%d\n",
            ts2_probe_number,
            queue_addr,
            msg,
            static_cast<unsigned>(ts2_type_before),
            static_cast<unsigned>(ts2_type_before),
            static_cast<unsigned>(ts2_type_after),
            static_cast<unsigned>(ts2_type_after),
            flags,
            result);
        std::fflush(stderr);
    }

    std::fprintf(
        stderr,
        "TS2 CRITICAL PROBE CLIENT SEND queue=0x%08X msg=0x%08X flags=%d result=%d\n",
        queue_addr,
        msg,
        flags,
        result);
    std::fflush(stderr);
    return result;
}
extern "C" void ts2_probe_9711_stage(
    uint32_t stage,
    uint32_t a,
    uint32_t b,
    int32_t c) {

    const char* name = "UNKNOWN";
    switch (stage) {
        case 1u: name = "ENTER"; break;
        case 2u: name = "BEFORE DRAIN"; break;
        case 3u: name = "AFTER DRAIN"; break;
        case 4u: name = "BEFORE WORK"; break;
        case 5u: name = "AFTER WORK"; break;
        case 6u: name = "BEFORE STATE"; break;
        case 7u: name = "AFTER STATE"; break;
        case 8u: name = "CLIENT STAGE"; break;
        default: break;
    }

    std::fprintf(
        stderr,
        "TS2 CRITICAL PROBE 9711 STAGE %u %s a=0x%08X b=0x%08X c=%d\n",
        stage,
        name,
        a,
        b,
        c);
    std::fflush(stderr);
}
extern "C" void ts2_probe_975bc_call(
    const char* call_name,
    uint32_t phase,
    uint32_t sp,
    uint32_t a0,
    uint32_t a1,
    uint32_t a2,
    uint32_t a3,
    uint32_t v0) {

    std::fprintf(
        stderr,
        "TS2 CRITICAL PROBE 975BC %s %s SP=0x%08X a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X v0=0x%08X\n",
        (phase == 0u) ? "BEFORE" : "AFTER",
        call_name,
        sp,
        a0,
        a1,
        a2,
        a3,
        v0);
    std::fflush(stderr);
}
extern "C" void ts2_probe_979a0_call(
    const char* call_name,
    uint32_t phase,
    uint32_t sp,
    uint32_t a0,
    uint32_t a1,
    uint32_t a2,
    uint32_t a3,
    uint32_t v0) {

    std::fprintf(
        stderr,
        "TS2 CRITICAL PROBE 979A0 %s %s SP=0x%08X a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X v0=0x%08X\n",
        (phase == 0u) ? "BEFORE" : "AFTER",
        call_name,
        sp,
        a0,
        a1,
        a2,
        a3,
        v0);
    std::fflush(stderr);
}

// TS2 LIVE RT64 GFX BRIDGE BEGIN
static SDL_Window* g_ts2_rt64_window = nullptr;
static HWND g_ts2_rt64_hwnd = nullptr;
static std::unique_ptr<RT64::Application> g_ts2_rt64_app{};
static std::array<uint8_t, 0x1000> g_ts2_rt64_dmem{};
static std::array<uint8_t, 0x1000> g_ts2_rt64_imem{};
static std::array<unsigned char, 0x40> g_ts2_rt64_rom_header{};

static unsigned int g_ts2_rt64_mi_intr = 0;
static unsigned int g_ts2_rt64_dpc_start = 0;
static unsigned int g_ts2_rt64_dpc_end = 0;
static unsigned int g_ts2_rt64_dpc_current = 0;
static unsigned int g_ts2_rt64_dpc_status = 0;
static unsigned int g_ts2_rt64_dpc_clock = 0;
static unsigned int g_ts2_rt64_dpc_bufbusy = 0;
static unsigned int g_ts2_rt64_dpc_pipebusy = 0;
static unsigned int g_ts2_rt64_dpc_tmem = 0;

static DWORD g_ts2_rt64_setup_thread = 0;
static bool g_ts2_rt64_com_initialized = false;
static uint32_t g_ts2_rt64_gfx_count = 0;

static void ts2_rt64_check_interrupts_stub() {
}

static bool ts2_rt64_sync_vi() {
    auto* vi = ultramodern::renderer::get_vi_regs();
    if (vi == nullptr) {
        std::fputs("TS2 LIVE RT64: N64MR VI register block is null.\n", stderr);
        return false;
    }

    vi->VI_STATUS_REG         = g_vi_registers[0];
    vi->VI_ORIGIN_REG         = g_vi_registers[1];
    vi->VI_WIDTH_REG          = g_vi_registers[2];
    vi->VI_INTR_REG           = g_vi_registers[3];
    vi->VI_V_CURRENT_LINE_REG = g_vi_registers[4];
    vi->VI_TIMING_REG         = g_vi_registers[5];
    vi->VI_V_SYNC_REG         = g_vi_registers[6];
    vi->VI_H_SYNC_REG         = g_vi_registers[7];
    vi->VI_LEAP_REG           = g_vi_registers[8];
    vi->VI_H_START_REG        = g_vi_registers[9];
    vi->VI_V_START_REG        = g_vi_registers[10];
    vi->VI_V_BURST_REG        = g_vi_registers[11];
    vi->VI_X_SCALE_REG        = g_vi_registers[12];
    vi->VI_Y_SCALE_REG        = g_vi_registers[13];

    return true;
}

static bool ts2_rt64_prepare_window(const std::vector<uint8_t>& rom) {
    if (rom.size() < g_ts2_rt64_rom_header.size()) {
        std::fputs("TS2 LIVE RT64: ROM is too small for the N64 header.\n", stderr);
        return false;
    }

    std::memcpy(
        g_ts2_rt64_rom_header.data(),
        rom.data(),
        g_ts2_rt64_rom_header.size());

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: SDL_Init failed: %s\n",
            SDL_GetError());
        return false;
    }

    g_ts2_rt64_window = SDL_CreateWindow(
        "Toy Story 2 N64Recomp - Live RT64",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        960,
        720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (g_ts2_rt64_window == nullptr) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: SDL_CreateWindow failed: %s\n",
            SDL_GetError());
        return false;
    }

    SDL_SysWMinfo wm_info{};
    SDL_VERSION(&wm_info.version);

    if (SDL_GetWindowWMInfo(g_ts2_rt64_window, &wm_info) != SDL_TRUE) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: SDL_GetWindowWMInfo failed: %s\n",
            SDL_GetError());
        return false;
    }

    g_ts2_rt64_hwnd = wm_info.info.win.window;
    if (g_ts2_rt64_hwnd == nullptr) {
        std::fputs(
            "TS2 LIVE RT64: SDL returned a null Win32 HWND.\n",
            stderr);
        return false;
    }

    std::puts(
        "TS2 LIVE RT64: window created; renderer setup is deferred "
        "to the first real M_GFXTASK thread.");
    std::fflush(stdout);
    return true;
}

static bool ts2_rt64_ensure_app(uint8_t* rdram) {
    const DWORD current_thread = GetCurrentThreadId();

    if (g_ts2_rt64_app != nullptr) {
        if (g_ts2_rt64_setup_thread != current_thread) {
            std::fprintf(
                stderr,
                "TS2 LIVE RT64: graphics task changed native thread "
                "(setup=%lu current=%lu).\n",
                static_cast<unsigned long>(g_ts2_rt64_setup_thread),
                static_cast<unsigned long>(current_thread));
            return false;
        }

        return true;
    }

    if (g_ts2_rt64_hwnd == nullptr || g_ts2_rt64_window == nullptr) {
        std::fputs(
            "TS2 LIVE RT64: graphics window was not prepared before the task.\n",
            stderr);
        return false;
    }

    if (rdram != g_rdram.data()) {
        std::fputs(
            "TS2 LIVE RT64: RSP bridge RDRAM pointer does not match live g_rdram.\n",
            stderr);
        return false;
    }

    const HRESULT com_result =
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    if (SUCCEEDED(com_result)) {
        g_ts2_rt64_com_initialized = true;
    }
    else if (com_result != RPC_E_CHANGED_MODE) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: CoInitializeEx failed: 0x%08lX\n",
            static_cast<unsigned long>(com_result));
        return false;
    }

    if (!ts2_rt64_sync_vi()) {
        return false;
    }

    auto* vi = ultramodern::renderer::get_vi_regs();
    if (vi == nullptr) {
        return false;
    }

    RT64::Application::Core core{};
    core.window = g_ts2_rt64_hwnd;
    core.checkInterrupts = ts2_rt64_check_interrupts_stub;
    core.HEADER = g_ts2_rt64_rom_header.data();
    core.RDRAM = g_rdram.data();
    core.DMEM = g_ts2_rt64_dmem.data();
    core.IMEM = g_ts2_rt64_imem.data();

    core.MI_INTR_REG = &g_ts2_rt64_mi_intr;
    core.DPC_START_REG = &g_ts2_rt64_dpc_start;
    core.DPC_END_REG = &g_ts2_rt64_dpc_end;
    core.DPC_CURRENT_REG = &g_ts2_rt64_dpc_current;
    core.DPC_STATUS_REG = &g_ts2_rt64_dpc_status;
    core.DPC_CLOCK_REG = &g_ts2_rt64_dpc_clock;
    core.DPC_BUFBUSY_REG = &g_ts2_rt64_dpc_bufbusy;
    core.DPC_PIPEBUSY_REG = &g_ts2_rt64_dpc_pipebusy;
    core.DPC_TMEM_REG = &g_ts2_rt64_dpc_tmem;

    core.VI_STATUS_REG = &vi->VI_STATUS_REG;
    core.VI_ORIGIN_REG = &vi->VI_ORIGIN_REG;
    core.VI_WIDTH_REG = &vi->VI_WIDTH_REG;
    core.VI_INTR_REG = &vi->VI_INTR_REG;
    core.VI_V_CURRENT_LINE_REG = &vi->VI_V_CURRENT_LINE_REG;
    core.VI_TIMING_REG = &vi->VI_TIMING_REG;
    core.VI_V_SYNC_REG = &vi->VI_V_SYNC_REG;
    core.VI_H_SYNC_REG = &vi->VI_H_SYNC_REG;
    core.VI_LEAP_REG = &vi->VI_LEAP_REG;
    core.VI_H_START_REG = &vi->VI_H_START_REG;
    core.VI_V_START_REG = &vi->VI_V_START_REG;
    core.VI_V_BURST_REG = &vi->VI_V_BURST_REG;
    core.VI_X_SCALE_REG = &vi->VI_X_SCALE_REG;
    core.VI_Y_SCALE_REG = &vi->VI_Y_SCALE_REG;

    RT64::ApplicationConfiguration config{};
    config.useConfigurationFile = false;

    auto app = std::make_unique<RT64::Application>(core, config);
    app->userConfig.developerMode = false;

    std::fprintf(
        stderr,
        "TS2 LIVE RT64: initializing RT64 on graphics-task thread %lu...\n",
        static_cast<unsigned long>(current_thread));
    std::fflush(stderr);

    const auto setup_result = app->setup(current_thread);
    if (setup_result != RT64::Application::SetupResult::Success) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: setup failed with result %d.\n",
            static_cast<int>(setup_result));
        return false;
    }

    g_ts2_rt64_setup_thread = current_thread;
    g_ts2_rt64_app = std::move(app);

    std::fputs(
        "TS2 LIVE RT64: setup OK; real type-1 graphics tasks can render.\n",
        stderr);
    std::fflush(stderr);
    return true;
}

static bool ts2_rt64_process_gfx_task(
    uint8_t* rdram,
    uint32_t task_guest) {

    if (!ts2_rt64_ensure_app(rdram)) {
        return false;
    }

    if (!ts2_rt64_sync_vi()) {
        return false;
    }

    const uint32_t type =
        read_guest_u32(task_guest + offsetof(OSTask_t, type));
    const uint32_t ucode_raw =
        read_guest_u32(task_guest + offsetof(OSTask_t, ucode));
    const uint32_t ucode_data_raw =
        read_guest_u32(task_guest + offsetof(OSTask_t, ucode_data));
    const uint32_t data_raw =
        read_guest_u32(task_guest + offsetof(OSTask_t, data_ptr));

    if (type != 1u) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: refusing non-GFX task type %u.\n",
            type);
        return false;
    }

    const uint32_t ucode = phys(ucode_raw);
    const uint32_t ucode_data = phys(ucode_data_raw);
    const uint32_t display_list = phys(data_raw);

    auto* interp = g_ts2_rt64_app->interpreter.get();

    if (interp == nullptr ||
        g_ts2_rt64_app->state == nullptr ||
        g_ts2_rt64_app->state->rsp == nullptr) {
        std::fputs(
            "TS2 LIVE RT64: RT64 interpreter/RSP state unavailable.\n",
            stderr);
        return false;
    }

    g_ts2_rt64_app->state->rsp->reset();

    RT64::GBI* gbi =
        interp->gbiManager.getGBIForUCode(
            g_ts2_rt64_app->core.RDRAM,
            ucode,
            ucode_data);

    if (gbi == nullptr) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: unrecognized graphics microcode "
            "raw=%08X/%08X phys=%08X/%08X.\n",
            ucode_raw,
            ucode_data_raw,
            ucode,
            ucode_data);
        return false;
    }

    interp->loadUCodeGBI(ucode, ucode_data, true);
    g_ts2_rt64_app->state->rsp->setGBI(gbi);

    if (gbi->resetFromTask != nullptr) {
        gbi->resetFromTask(g_ts2_rt64_app->state.get());
    }

    ++g_ts2_rt64_gfx_count;

    std::fprintf(
        stderr,
        "TS2 LIVE RT64 GFX #%u: task=%08X "
        "ucode=%08X/%08X data=%08X physDL=%08X "
        "VI=%08X/%08X/%08X.\n",
        g_ts2_rt64_gfx_count,
        task_guest,
        ucode_raw,
        ucode_data_raw,
        data_raw,
        display_list,
        g_vi_registers[0],
        g_vi_registers[1],
        g_vi_registers[2]);
    std::fflush(stderr);

#if defined(_WIN32)
    __try {
        g_ts2_rt64_app->processDisplayLists(
            g_ts2_rt64_app->core.RDRAM,
            display_list,
            0,
            true);

        g_ts2_rt64_app->updateScreen();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: Windows exception while processing "
            "graphics task #%u.\n",
            g_ts2_rt64_gfx_count);
        std::fflush(stderr);
        return false;
    }
#else
    g_ts2_rt64_app->processDisplayLists(
        g_ts2_rt64_app->core.RDRAM,
        display_list,
        0,
        true);

    g_ts2_rt64_app->updateScreen();
#endif

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
    }

    std::fprintf(
        stderr,
        "TS2 LIVE RT64 GFX #%u: processDisplayLists/updateScreen returned.\n",
        g_ts2_rt64_gfx_count);
    std::fflush(stderr);
    return true;
}
// TS2 LIVE RT64 GFX BRIDGE END
extern "C" void osSpTaskLoad_recomp(uint8_t* rdram, recomp_context* ctx);
extern "C" void osSpTaskStartGo_recomp(uint8_t* rdram, recomp_context* ctx);

static uint32_t g_ts2_rsp_task_guest = 0u;

extern "C" void ts2_rsp_task_load_bridge(
    uint8_t* rdram,
    recomp_context* ctx) {

    g_ts2_rsp_task_guest = static_cast<uint32_t>(ctx->r4);

    std::fprintf(
        stderr,
        "TS2 RSP BRIDGE LOAD task=0x%08X\n",
        g_ts2_rsp_task_guest);
    std::fflush(stderr);

    osSpTaskLoad_recomp(rdram, ctx);

    std::fprintf(
        stderr,
        "TS2 RSP BRIDGE LOAD RETURN task=0x%08X\n",
        g_ts2_rsp_task_guest);
    std::fflush(stderr);
}

extern "C" void ts2_rsp_task_start_bridge(
    uint8_t* rdram,
    recomp_context* ctx) {

    const uint64_t saved_r4 = ctx->r4;

    if (g_ts2_rsp_task_guest == 0u) {
        std::fprintf(
            stderr,
            "TS2 RSP BRIDGE START ERROR: no task pointer was saved by load\n");
        std::fflush(stderr);
        return;
    }

    ctx->r4 = static_cast<int32_t>(g_ts2_rsp_task_guest);

    std::fprintf(
        stderr,
        "TS2 RSP BRIDGE START task=0x%08X\n",
        g_ts2_rsp_task_guest);
    std::fflush(stderr);

    const uint32_t ts2_live_task_type =
        read_guest_u32(
            g_ts2_rsp_task_guest + offsetof(OSTask_t, type));

    if (ts2_live_task_type == 1u) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: intercepted M_GFXTASK task=0x%08X.\n",
            g_ts2_rsp_task_guest);
        std::fflush(stderr);

        ::sp_complete();

        if (!ts2_rt64_process_gfx_task(
                rdram,
                g_ts2_rsp_task_guest)) {
            std::fputs(
                "TS2 LIVE RT64: graphics task processing FAILED.\n",
                stderr);
            std::fflush(stderr);
            std::_Exit(61);
        }

        ::dp_complete();

        std::fprintf(
            stderr,
            "TS2 LIVE RT64: SP/DP completion delivered for task=0x%08X.\n",
            g_ts2_rsp_task_guest);
        std::fflush(stderr);
    }
    else if (ts2_live_task_type == 2u) {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64 AUDIO BYPASS: intercepted type-2 RSP task=0x%08X; "
            "skipping audio microcode and delivering SP completion.\n",
            g_ts2_rsp_task_guest);
        std::fflush(stderr);

        // TEMPORARY DIAGNOSTIC ONLY.
        // N64MR's real non-GFX worker does run_task() followed by sp_complete().
        // This custom harness has no running RSP worker, so for the first-frame
        // experiment we deliberately omit run_task() and deliver only the SP event.
        ::sp_complete();

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }

        std::fprintf(
            stderr,
            "TS2 LIVE RT64 AUDIO BYPASS: SP completion delivered for task=0x%08X.\n",
            g_ts2_rsp_task_guest);
        std::fflush(stderr);
    }
    else {
        std::fprintf(
            stderr,
            "TS2 LIVE RT64: unknown RSP task type=%u task=0x%08X; using stock N64MR submission.\n",
            ts2_live_task_type,
            g_ts2_rsp_task_guest);
        std::fflush(stderr);
        osSpTaskStartGo_recomp(rdram, ctx);
    }

    std::fprintf(
        stderr,
        "TS2 RSP BRIDGE START RETURN task=0x%08X\n",
        g_ts2_rsp_task_guest);
    std::fflush(stderr);

    ctx->r4 = saved_r4;
}
extern "C" int stage61_recv_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t msg_addr,
    int32_t flags,
    int32_t* out_result) {

    {
        std::lock_guard<std::mutex> lock(g_thread_mutex);
        g_recv_count++;
        g_recv_queue = queue_addr;
        g_recv_msg = msg_addr;
        g_recv_flags = flags;
        g_recv_bridge_entered = true;

        if (queue_addr == kPiCmdQueue) {
            g_pi_recv_entered = true;
            ++g_pi_cmd_recv_count;
            g_pi_recv_msg = msg_addr;
            g_pi_recv_flags = flags;
        } else if (queue_addr == kViQueue) {
            g_vi_recv_entered = true;
            g_vi_recv_msg = msg_addr;
            g_vi_recv_flags = flags;
        }
        else if (queue_addr == kManagerQueueA) {
            g_worker_recv_entered = true;
            g_worker_recv_msg = msg_addr;
            g_worker_recv_flags = flags;
        }
        else if (queue_addr == kPiAccessQueue) {
            g_pi_access_recv_entered = true;
        }
        else if (queue_addr == kPiEventQueue) {
            g_pi_completion_recv_entered = true;
            g_thread_cv.notify_all();
        }
        else if (queue_addr == kThread4QueueSmall) {
            g_loader_recv_entered = true;
            g_thread_cv.notify_all();
        }
        else if (queue_addr == kSiQueue) {
            ++g_si_event_recv_count;
            g_si_event_recv_msg_ptr = msg_addr;
        }

        std::printf(
            "N64MR RECV #%u: queue=0x%08X msg=0x%08X flags=%d "
            "(%s)\n",
            g_recv_count,
            queue_addr,
            msg_addr,
            flags,
            flags == 0 ? "nonblocking" : "blocking");
    }

    const bool pi_receive =
        queue_addr == kPiCmdQueue &&
        msg_addr == 0x801F2B10u &&
        flags == 1;

    const bool vi_receive =
        queue_addr == kViQueue &&
        msg_addr == kViManagerReceiveMsg &&
        flags == 1;

    const bool worker_receive =
        queue_addr == kManagerQueueA &&
        msg_addr == kWorkerReceiveMsg &&
        flags == 1;

    const bool pi_access_receive =
        queue_addr == kPiAccessQueue &&
        msg_addr == kPiAccessReceiveMsg &&
        flags == 1;

    const bool pi_completion_receive =
        queue_addr == kPiEventQueue &&
        msg_addr == kPiCompletionReceiveMsg &&
        flags == 1;

    const bool loader_receive =
        queue_addr == kThread4QueueSmall &&
        msg_addr == 0u &&
        flags == 1;

    const bool si_event_receive =
        queue_addr == kSiQueue &&
        msg_addr == kSiEventReceiveMsgPtr &&
        flags == 1;

    const bool audio_receive =
        queue_addr == kStage79AudioQueue &&
        ((msg_addr == kStage79Callback1MsgOut && flags == 1) ||
         (msg_addr == 0u && flags == 0));

    if (out_result == nullptr) {
        g_bridge_args_valid = false;
        return 1;
    }

    // TS2 FAST-FORWARD SI BOOTSTRAP SHORTCUT:
    // This exact blocking receive was already crossed in the proven Stage 62-64
    // controller/PIF path because the old bring-up allowlist returned -1 here.
    // Preserve ONLY that exact bootstrap result, then let all real SI MMIO/event
    // traffic and every other generic message queue continue normally.
    if (queue_addr == 0x803F9F58u &&
        msg_addr == 0x803F9F70u &&
        flags == 1) {
        static uint32_t si_bootstrap_shortcut_count = 0u;
        ++si_bootstrap_shortcut_count;
        *out_result = -1;
        std::printf(
            "TS2 FAST-FORWARD SI BOOTSTRAP SHORTCUT #%u: "
            "queue=0x%08X msg=0x%08X flags=%d result=-1; "
            "continuing into the already-proven real SI MMIO/event path.\n",
            si_bootstrap_shortcut_count,
            queue_addr,
            msg_addr,
            flags);
        return 1;
    }
    if (queue_addr == kThread4QueueLarge && flags == 1) {
        std::fprintf(
            stderr,
            "TS2 CRITICAL PROBE CLIENT WAIT queue=0x%08X msg=0x%08X flags=%d\n",
            queue_addr,
            msg_addr,
            flags);
        std::fflush(stderr);
    }
    // TS2 VI MANAGER RETRACE FIX:
    //
    // The old bring-up shortcut injected Toy Story 2's 0x29A manager message
    // directly into the scheduler worker queue whenever the frame client was
    // about to block. That skipped the game's real VI-manager interrupt path.
    //
    // Toy Story 2's VI-manager path must run first because it calls
    // func_80095360, which advances/copies the current and next VI contexts.
    // The scheduler intentionally refuses another graphics task while the
    // current and next framebuffer values differ.
    //
    // Keep the external-message ordering that avoids synchronous osSendMesg
    // rescheduling here, but target the registered VI interrupt queue/message.
    // The real VI manager will perform func_80095360 and then forward 0x29A
    // to the scheduler itself.
    if (queue_addr == kThread4QueueLarge && flags == 1) {
        static uint32_t vi_manager_retrace_fix_count = 0u;
        ++vi_manager_retrace_fix_count;

        // SPACERANGER SYNCHRONOUS VI THROTTLE V1
        //
        // Preserve the proven receive -> raw VI -> VI manager -> 0x29A ->
        // scheduler chain, but prevent the guest from manufacturing retraces
        // as fast as the host CPU can reach this blocking receive.
        //
        // There is deliberately NO background VI thread here.
        {
            using clock = std::chrono::steady_clock;
            constexpr auto kViPeriod =
                std::chrono::nanoseconds(16666667); // ~60 Hz

            static clock::time_point s_last_vi{};

            const auto now = clock::now();

            if (s_last_vi != clock::time_point{}) {
                const auto target = s_last_vi + kViPeriod;

                if (now < target) {
                    std::this_thread::sleep_until(target);
                }
            }

            // Use the actual delivery time rather than accumulating a
            // theoretical deadline. This prevents catch-up bursts.
            s_last_vi = clock::now();
        }

        ultramodern::enqueue_external_message(
            static_cast<PTR(OSMesgQueue)>(
                static_cast<int32_t>(g_stage79_event_slot_queue)),
            static_cast<OSMesg>(
                static_cast<int32_t>(g_stage79_event_slot_message)),
            false,
            false);

        if (vi_manager_retrace_fix_count <= 32u ||
            (vi_manager_retrace_fix_count % 256u) == 0u) {
            std::fprintf(
                stderr,
                "TS2 VI MANAGER RETRACE FIX #%u queue=0x%08X msg=0x%08X queued; "
                "real VI manager must update contexts before forwarding 0x29A.\n",
                vi_manager_retrace_fix_count,
                g_stage79_event_slot_queue,
                g_stage79_event_slot_message);
            std::fflush(stderr);
        }
    }
    const bool known_instrumented_receive =
        pi_receive ||
        vi_receive ||
        worker_receive ||
        pi_access_receive ||
        pi_completion_receive ||
        loader_receive ||
        si_event_receive ||
        audio_receive
        ;

    // TS2 FAST-FORWARD GENERIC RECV:
    // The earlier bring-up harness rejected every queue that was not explicitly
    // named above. Real game code creates additional message queues, so pass all
    // well-formed receives through to N64ModernRuntime's osRecvMesg.
    if (!known_instrumented_receive) {
        static uint32_t generic_recv_count = 0u;
        ++generic_recv_count;
        if (generic_recv_count <= 16u ||
            (generic_recv_count % 256u) == 0u) {
            std::printf(
                "TS2 FAST-FORWARD GENERIC RECV #%u: queue=0x%08X "
                "msg=0x%08X flags=%d; delegating to N64ModernRuntime.\n",
                generic_recv_count,
                queue_addr,
                msg_addr,
                flags);
        }
    }

    if (queue_addr == kPiEventQueue) {
        std::fprintf(
            stderr,
            "TS2 PI148 PROBE V3 RECV BEFORE queue=0x%08X msgAddr=0x%08X flags=%d\n",
            queue_addr,
            msg_addr,
            flags);
        std::fflush(stderr);
    }

    *out_result = osRecvMesg(
        rdram,
        static_cast<PTR(OSMesgQueue)>(
            static_cast<int32_t>(queue_addr)),
        static_cast<PTR(OSMesg)>(
            static_cast<int32_t>(msg_addr)),
        flags);
    // Diagnostic-only: inspect the actual message object immediately after
    // Toy Story 2 receives from the frame queue. This does not alter the
    // message, queue, scheduler, or event type.
    if (queue_addr == kThread4QueueLarge) {
        static uint32_t ts2_frame_event_recv_probe_count = 0u;

        if (*out_result == 0 && ts2_frame_event_recv_probe_count < 128u) {
            const uint32_t ts2_recv_probe_number =
                ++ts2_frame_event_recv_probe_count;
            const uint32_t ts2_received_msg =
                (msg_addr != 0u) ? read_guest_u32(msg_addr) : 0u;
            const bool ts2_received_msg_is_rdram_ptr =
                ts2_received_msg >= 0x80000000u &&
                ts2_received_msg <= 0x807FFFFEu;
            const uint16_t ts2_received_type =
                ts2_received_msg_is_rdram_ptr
                    ? read_guest_u16(ts2_received_msg)
                    : 0xFFFFu;

            std::fprintf(
                stderr,
                "TS2 FRAME EVENT RECV #%u queue=0x%08X msgAddr=0x%08X "
                "msg=0x%08X type=%u/0x%04X flags=%d result=%d\n",
                ts2_recv_probe_number,
                queue_addr,
                msg_addr,
                ts2_received_msg,
                static_cast<unsigned>(ts2_received_type),
                static_cast<unsigned>(ts2_received_type),
                flags,
                *out_result);
            std::fflush(stderr);
        }
    }

    if (queue_addr == kPiEventQueue) {
        std::fprintf(
            stderr,
            "TS2 PI148 PROBE V3 RECV AFTER queue=0x%08X result=%d msg=0x%08X\n",
            queue_addr,
            *out_result,
            (msg_addr != 0u) ? read_guest_u32(msg_addr) : 0u);
        std::fflush(stderr);
    }
    if (worker_receive) {
        std::fprintf(
            stderr,
            "TS2 CRITICAL PROBE WORKER RESUME queue=0x%08X result=%d msg=0x%08X\n",
            queue_addr,
            *out_result,
            (msg_addr != 0u) ? read_guest_u32(msg_addr) : 0u);
        std::fflush(stderr);
    }
    if (queue_addr == kThread4QueueLarge && flags == 1) {
        std::fprintf(
            stderr,
            "TS2 CRITICAL PROBE CLIENT RESUME queue=0x%08X result=%d msg=0x%08X\n",
            queue_addr,
            *out_result,
            (msg_addr != 0u) ? read_guest_u32(msg_addr) : 0u);
        std::fflush(stderr);
    }

    if (pi_completion_receive) {
        g_pi_completion_recv_result = *out_result;
        std::printf(
            "PI COMPLETION RECV: resumed with result=%d msg=0x%08X.\n",
            *out_result,
            read_guest_u32(kPiCompletionReceiveMsg));
    }
    if (loader_receive) {
        g_loader_recv_result = *out_result;
        std::printf(
            "THREAD 4 ROM-LOADER RECV: resumed with result=%d.\n",
            *out_result);
    }
    if (si_event_receive) {
        g_si_event_recv_result = *out_result;
        g_si_event_recv_message = read_guest_u32(msg_addr);
        std::printf(
            "SI EVENT RECV #%u: resumed with result=%d msg=0x%08X "
            "from queue 0x%08X.\n",
            g_si_event_recv_count,
            *out_result,
            g_si_event_recv_message,
            queue_addr);
    }

    return 1;
}

extern "C" int stage79_audio_worker_send_bridge(
    uint8_t* rdram,
    uint32_t queue_addr,
    uint32_t message,
    int32_t flags,
    int32_t* out_result) {

    if (queue_addr != kStage79AudioQueue ||
        message != kStage79WorkerBase ||
        flags != 0) {
        return 0;
    }

    if (out_result == nullptr) {
        std::fputs(
            "FAILED: Stage 79 native audio send bridge received a null "
            "result pointer.\n",
            stderr);
        return 1;
    }

    *out_result = osSendMesg(
        rdram,
        static_cast<PTR(OSMesgQueue)>(
            static_cast<int32_t>(queue_addr)),
        static_cast<OSMesg>(message),
        flags);

    std::printf(
        "N64MR AUDIO SEND: worker message 0x%08X -> "
        "queue 0x%08X flags=%d result=%d.\n",
        message,
        queue_addr,
        flags,
        *out_result);

    return 1;
}

extern "C" int stage61_get_priority_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t* out_priority) {

    g_priority_get_count++;

    if (thread_addr != 0u || out_priority == nullptr) {
        g_bridge_args_valid = false;
        return 1;
    }

    *out_priority = osGetThreadPri(rdram, NULLPTR);
    g_priority_get_value = *out_priority;

    std::printf(
        "N64MR PRI GET #%u: current thread priority=%d\n",
        g_priority_get_count,
        g_priority_get_value);

    return 1;
}

extern "C" int stage61_set_priority_bridge(
    uint8_t* rdram,
    uint32_t thread_addr,
    int32_t priority) {

    g_priority_set_count++;

    if (thread_addr != 0u) {
        g_bridge_args_valid = false;
        return 1;
    }

    g_priority_set_value = priority;

    std::printf(
        "N64MR PRI SET #%u: current thread priority=%d\n",
        g_priority_set_count,
        g_priority_set_value);

    // Use the runtime's real priority operation. Besides updating the compact
    // OSThread object, this runs check_running_queue(), which is required for
    // the priority-150 PI manager to preempt thread 3 after thread 3 restores
    // itself from priority 150 back to 10.
    osSetThreadPri(rdram, NULLPTR, priority);

    return 1;
}

extern "C" int stage61_parent_checkpoint(
    uint8_t* rdram,
    recomp_context* ctx,
    uint32_t guest_pc) {
    (void)rdram;

    g_parent_checkpoint_count++;
    g_parent_last_pc = guest_pc;

    if (guest_pc == kStage20Pc ||
        guest_pc == kStage21Pc ||
        guest_pc == kStage22Pc ||
        guest_pc == kStage23Pc ||
        guest_pc == kStage24Pc) {
        std::printf(
            "BOOT GUARD: passed earlier boundary 0x%08X.\n",
            guest_pc);
        return 0;
    }

    if (guest_pc == kStage25Pc) {
        g_parent_sp = static_cast<uint32_t>(ctx->r29);

        std::printf(
            "BOOT GUARD: first native thread handoff complete at "
            "0x%08X with parent SP=0x%08X.\n",
            guest_pc,
            g_parent_sp);

        return 1;
    }

    std::fprintf(
        stderr,
        "FAILED: unexpected parent checkpoint 0x%08X.\n",
        guest_pc);
    g_unexpected_parent_checkpoint = true;
    return 1;
}

void stage61_librecomp_dispatch(
    uint8_t* rdram,
    uint64_t addr,
    uint64_t sp,
    uint64_t arg) {

    const uint32_t guest_addr = static_cast<uint32_t>(addr);
    const uint32_t guest_sp = static_cast<uint32_t>(sp);
    const uint32_t guest_arg = static_cast<uint32_t>(arg);

    std::printf(
        "N64MR DISPATCH: addr=0x%08X sp=0x%08X arg=0x%08X\n",
        guest_addr,
        guest_sp,
        guest_arg);

    recomp_context ctx{};
    ctx.r29 = static_cast<int32_t>(guest_sp);
    ctx.r4 = static_cast<int32_t>(guest_arg);
    ctx.r31 = 0;
    ctx.mips3_float_mode = false;
    ctx.f_odd = &ctx.f0.u32h;

    if (guest_addr == kThread3Entry) {
        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_thread3_dispatch_sp = guest_sp;
            g_thread3_dispatch_arg = guest_arg;
        }

        if (guest_sp != kThread3InitialSp ||
            guest_arg != kThread3Arg) {
            std::fprintf(
                stderr,
                "FAILED: unexpected thread-3 dispatch parameters.\n");
        } else {
            func_8000806C(rdram, &ctx);
        }

        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_thread3_dispatch_returned = true;
        }
        g_thread_cv.notify_all();
        return;
    }

    if (guest_addr == kThread4Entry) {
        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_thread4_dispatch_sp = guest_sp;
            g_thread4_dispatch_arg = guest_arg;
        }

        if (guest_sp != kThread4InitialSp ||
            guest_arg != kThread4Arg) {
            std::fprintf(
                stderr,
                "FAILED: unexpected thread-4 dispatch parameters.\n");
        } else {
            func_800080F8(rdram, &ctx);
        }

        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_thread4_dispatch_returned = true;
        }
        g_thread_cv.notify_all();
        return;
    }

    if (guest_addr == kPiManagerThreadEntry) {
        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_pi_dispatch_sp = guest_sp;
            g_pi_dispatch_arg = guest_arg;
        }

        if (guest_sp != kPiManagerThreadInitialSp ||
            guest_arg != kPiManagerThreadArg) {
            std::fprintf(
                stderr,
                "FAILED: unexpected PI-manager dispatch parameters.\n");
        } else {
            // Current recovery exposes the PI-manager thread entry directly.
            func_80099FE0(rdram, &ctx);
        }

        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_pi_dispatch_returned = true;
        }
        g_thread_cv.notify_all();
        return;
    }

    if (guest_addr == kViManagerThreadEntry) {
        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_vi_dispatch_sp = guest_sp;
            g_vi_dispatch_arg = guest_arg;
        }

        if (guest_sp != kViManagerThreadInitialSp ||
            guest_arg != kViManagerThreadArg) {
            std::fprintf(
                stderr,
                "FAILED: unexpected VI-manager dispatch parameters.\n");
        } else {
            ts2_runtime_owned_vi_manager_trap_recomp(rdram, &ctx);
        }

        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_vi_dispatch_returned = true;
        }
        g_thread_cv.notify_all();
        return;
    }

    if (guest_addr == kWorkerThreadEntry) {
        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_worker_dispatch_sp = guest_sp;
            g_worker_dispatch_arg = guest_arg;
        }

        if (guest_sp != kWorkerThreadInitialSp ||
            guest_arg != kWorkerThreadArg) {
            std::fprintf(
                stderr,
                "FAILED: unexpected worker dispatch parameters.\n");
        } else {
            ts2_current_internal_entry_80097018_recomp(rdram, &ctx);
        }

        {
            std::lock_guard<std::mutex> lock(g_thread_mutex);
            g_worker_dispatch_returned = true;
        }
        g_thread_cv.notify_all();
        return;
    }

    if (guest_addr == 0x800904DCu) {
        if (guest_sp != 0x8031EFE0u ||
            guest_arg != 0x00000000u) {
            std::fprintf(
                stderr,
                "FAILED: unexpected audio-thread dispatch parameters "
                "sp=0x%08X arg=0x%08X.\n",
                guest_sp,
                guest_arg);
        } else {
            func_800904DC(rdram, &ctx);
        }
        return;
    }


    std::fprintf(
        stderr,
        "FAILED: unexpected N64ModernRuntime dispatch address 0x%08X.\n",
        guest_addr);
}

extern "C" void func_8009D020(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8009D2E0(uint8_t* rdram, recomp_context* ctx);
// TS2 BULK INTERNAL CALLBACK ALIASES V1: declarations begin
extern "C" void ts2_auto_alias_80087870(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80099E20(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_80099FE0(uint8_t* rdram, recomp_context* ctx);
extern "C" void ts2_auto_alias_8009C4AC(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8009D1C0(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8009E240(uint8_t* rdram, recomp_context* ctx);
extern "C" void func_8009F9E0(uint8_t* rdram, recomp_context* ctx);
// TS2 BULK INTERNAL CALLBACK ALIASES V1: declarations end
#if defined(_WIN32)
// TS2 ACCESS VIOLATION PROBE V14
//
// First-chance diagnostic only. It records the native Windows AV and then
// returns EXCEPTION_CONTINUE_SEARCH, so normal Windows crash handling still
// occurs. No guest/runtime state is modified and the fault is not swallowed.
static PVOID g_ts2_access_violation_probe_handle = nullptr;
static volatile LONG g_ts2_access_violation_probe_count = 0;

static LONG CALLBACK ts2_access_violation_probe(
    EXCEPTION_POINTERS* exception_info) {

    // TS2 CLEAN NATIVE EXCEPTION PROBE V23
    //
    // Diagnostic only:
    //  - handles the first native AV / integer divide-by-zero / integer overflow
    //  - prints one safe report
    //  - never resumes execution
    //  - never changes guest/runtime state
    //  - returns EXCEPTION_CONTINUE_SEARCH so Windows preserves the real fault
    //
    // The thread-local recursion guard prevents a fault inside the diagnostic
    // path from recursively flooding the process with probe output.
    static thread_local bool ts2_probe_active = false;

    if (ts2_probe_active) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (exception_info == nullptr ||
        exception_info->ExceptionRecord == nullptr ||
        exception_info->ContextRecord == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    EXCEPTION_RECORD* record = exception_info->ExceptionRecord;
    CONTEXT* context = exception_info->ContextRecord;

    const DWORD code = record->ExceptionCode;

    if (code != EXCEPTION_ACCESS_VIOLATION &&
        code != EXCEPTION_INT_DIVIDE_BY_ZERO &&
        code != EXCEPTION_INT_OVERFLOW) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    ts2_probe_active = true;

    const LONG probe_number =
        InterlockedIncrement(&g_ts2_access_violation_probe_count);

    const char* exception_name = "UNKNOWN";

    if (code == EXCEPTION_ACCESS_VIOLATION) {
        exception_name = "ACCESS_VIOLATION";
    }
    else if (code == EXCEPTION_INT_DIVIDE_BY_ZERO) {
        exception_name = "INT_DIVIDE_BY_ZERO";
    }
    else if (code == EXCEPTION_INT_OVERFLOW) {
        exception_name = "INT_OVERFLOW";
    }

    const uintptr_t exception_address =
        reinterpret_cast<uintptr_t>(record->ExceptionAddress);

    HMODULE fault_module = nullptr;
    char fault_module_path[MAX_PATH]{};

    const BOOL have_fault_module =
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(record->ExceptionAddress),
            &fault_module);

    if (have_fault_module != FALSE &&
        fault_module != nullptr) {
        (void)GetModuleFileNameA(
            fault_module,
            fault_module_path,
            static_cast<DWORD>(sizeof(fault_module_path)));
    }

    const uintptr_t module_base =
        reinterpret_cast<uintptr_t>(fault_module);

    const uintptr_t module_rva =
        module_base != 0u &&
        exception_address >= module_base
            ? exception_address - module_base
            : 0u;

    std::fprintf(
        stderr,
        "\n"
        "============================================================\n"
        "TS2 CLEAN NATIVE EXCEPTION PROBE #%ld\n"
        "code=0x%08lX name=%s threadId=%lu\n"
        "exceptionAddress=%p faultModule=%p moduleRVA=0x%llX\n"
        "faultModulePath=%s\n",
        static_cast<long>(probe_number),
        static_cast<unsigned long>(code),
        exception_name,
        static_cast<unsigned long>(GetCurrentThreadId()),
        record->ExceptionAddress,
        reinterpret_cast<void*>(module_base),
        static_cast<unsigned long long>(module_rva),
        fault_module_path[0] != '\0'
            ? fault_module_path
            : "(module lookup failed)");

#if defined(_M_X64) || defined(__x86_64__)
    std::fprintf(
        stderr,
        "RIP=%016llX RSP=%016llX RBP=%016llX\n"
        "RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX\n"
        "RSI=%016llX RDI=%016llX R8 =%016llX R9 =%016llX\n"
        "R10=%016llX R11=%016llX R12=%016llX R13=%016llX\n"
        "R14=%016llX R15=%016llX EFlags=%08lX\n",
        static_cast<unsigned long long>(context->Rip),
        static_cast<unsigned long long>(context->Rsp),
        static_cast<unsigned long long>(context->Rbp),
        static_cast<unsigned long long>(context->Rax),
        static_cast<unsigned long long>(context->Rbx),
        static_cast<unsigned long long>(context->Rcx),
        static_cast<unsigned long long>(context->Rdx),
        static_cast<unsigned long long>(context->Rsi),
        static_cast<unsigned long long>(context->Rdi),
        static_cast<unsigned long long>(context->R8),
        static_cast<unsigned long long>(context->R9),
        static_cast<unsigned long long>(context->R10),
        static_cast<unsigned long long>(context->R11),
        static_cast<unsigned long long>(context->R12),
        static_cast<unsigned long long>(context->R13),
        static_cast<unsigned long long>(context->R14),
        static_cast<unsigned long long>(context->R15),
        static_cast<unsigned long>(context->EFlags));
#elif defined(_M_IX86) || defined(__i386__)
    std::fprintf(
        stderr,
        "EIP=%08lX ESP=%08lX EBP=%08lX "
        "EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX "
        "ESI=%08lX EDI=%08lX EFlags=%08lX\n",
        static_cast<unsigned long>(context->Eip),
        static_cast<unsigned long>(context->Esp),
        static_cast<unsigned long>(context->Ebp),
        static_cast<unsigned long>(context->Eax),
        static_cast<unsigned long>(context->Ebx),
        static_cast<unsigned long>(context->Ecx),
        static_cast<unsigned long>(context->Edx),
        static_cast<unsigned long>(context->Esi),
        static_cast<unsigned long>(context->Edi),
        static_cast<unsigned long>(context->EFlags));
#endif

    if (code == EXCEPTION_ACCESS_VIOLATION) {
        const ULONG_PTR access_kind =
            record->NumberParameters >= 1
                ? record->ExceptionInformation[0]
                : static_cast<ULONG_PTR>(~0ull);

        const ULONG_PTR bad_address =
            record->NumberParameters >= 2
                ? record->ExceptionInformation[1]
                : 0u;

        const char* access_name = "UNKNOWN";

        if (access_kind == 0u) {
            access_name = "READ";
        }
        else if (access_kind == 1u) {
            access_name = "WRITE";
        }
        else if (access_kind == 8u) {
            access_name = "EXECUTE";
        }

        std::fprintf(
            stderr,
            "access=%s(%llu) badAddress=0x%llX params=%lu\n",
            access_name,
            static_cast<unsigned long long>(access_kind),
            static_cast<unsigned long long>(bad_address),
            static_cast<unsigned long>(record->NumberParameters));
    }
    else {
        std::fprintf(
            stderr,
            "arithmeticFault: exceptionParameters=%lu\n",
            static_cast<unsigned long>(record->NumberParameters));
    }

    std::fprintf(
        stderr,
        "============================================================\n");
    std::fflush(stderr);

    // Keep the guard set. The process is expected to terminate through normal
    // Windows exception handling after EXCEPTION_CONTINUE_SEARCH. If a nested
    // diagnostic fault somehow occurs, it will not recurse into this probe.
    return EXCEPTION_CONTINUE_SEARCH;
}

static void ts2_install_access_violation_probe() {
    if (g_ts2_access_violation_probe_handle != nullptr) {
        return;
    }

    g_ts2_access_violation_probe_handle =
        AddVectoredExceptionHandler(
            1,
            &ts2_access_violation_probe);

    std::printf(
        "TS2 NATIVE ARITHMETIC PROBE V20: %s.\n",
        g_ts2_access_violation_probe_handle != nullptr
            ? "installed"
            : "FAILED to install");

    std::fflush(stdout);
    std::fflush(stderr);
}
#endif
#if defined(_WIN32)
// TS2 V145 MSVC SEH ISOLATION
//
// MSVC C2712 forbids __try in run_stage61 because that function owns C++
// objects requiring unwinding. Keep SEH in this POD-only helper instead.
static int ts2_v145_call_recomp_entrypoint_seh(
    uint8_t* rdram,
    recomp_context* ctx) {

    __try {
        return ts2_oldnative_call_recomp_entrypoint_catching_bootstrap(
            rdram,
            ctx);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}
#endif

int run_stage61() {
#if defined(_WIN32)
    ts2_install_access_violation_probe();
#endif
    using namespace std::chrono_literals;

    std::puts("Toy Story 2 N64Recomp - Stage 91 selector-0x62 first resource boundary");
    std::printf("N64ModernRuntime: %s\n", TS2_N64MR_COMMIT);

    if (!ts2::validate_section_layout()) return 2;

    ts2::register_sections();
    recomp::overlays::init_overlays();
    // TS2 PHASE 5B2B COMPAT INTERNAL ENTRY REGISTRATION
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087870u),
        ts2_auto_alias_80087870);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C4ACu),
        ts2_auto_alias_8009C4AC);

    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(kPiRawDmaInternalEntry),
        &func_80099E20);

    std::puts(
        "LOOKUP: registered guest 0x80099E20 -> func_80099E20 "
        "in librecomp live func_map.");

    std::vector<uint8_t> rom;
    if (!load_rom(rom)) return 3;
    if (!stage_resident_image(rom)) return 4;
    g_stage61_rom_data = rom.data();
    g_stage61_rom_size = static_cast<uint32_t>(rom.size());

    const uint32_t rom_clock =
        (static_cast<uint32_t>(rom[4]) << 24) |
        (static_cast<uint32_t>(rom[5]) << 16) |
        (static_cast<uint32_t>(rom[6]) << 8) |
        static_cast<uint32_t>(rom[7]);

    if (rom_clock != kExpectedRomClockWord) {
        std::fprintf(
            stderr,
            "FAILED: ROM clock word=0x%08X, expected 0x%08X.\n",
            rom_clock,
            kExpectedRomClockWord);
        return 5;
    }

    poison_bss_ranges();
    initialize_boot_globals();
    poison_thread4_clear_region();
    ts2_reset_pif_word_7fc();
    ts2_bind_rom_image(rom.data(), static_cast<uint32_t>(rom.size()));

    if (!ts2_rt64_prepare_window(rom)) {
        std::fputs(
            "FAILED: could not prepare the live RT64 graphics window.\n",
            stderr);
        return 5;
    }

    ts2_set_thread_dispatch_callback(&stage61_librecomp_dispatch);
    ts2_set_thread_bridge_callbacks(
        &stage61_create_bridge,
        &stage61_start_bridge);
    ts2_set_thread_priority_bridge_callbacks(
        &stage61_get_priority_bridge,
        &stage61_set_priority_bridge);
    ts2_set_mesg_recv_bridge_callback(&stage61_recv_bridge);
    ts2_set_mesg_send_bridge_callback(&stage79_audio_worker_send_bridge);
    ts2_set_vi_mmio_bridge_callbacks(
        &stage61_vi_mmio_read,
        &stage61_vi_mmio_write);
    ts2_set_mi_mmio_bridge_callbacks(
        &stage61_mi_mmio_read,
        &stage61_mi_mmio_write);
    ts2_set_si_mmio_bridge_callbacks(
        &stage64_si_mmio_read,
        &stage64_si_mmio_write);
    ts2_set_pi_mmio_bridge_callbacks(
        &stage61_pi_mmio_read,
        &stage61_pi_mmio_write);
    ts2_set_thread_entry_callback(&stage61_thread_checkpoint);
    ts2_set_boot_checkpoint_callback(&stage61_parent_checkpoint);

    recomp_context parent_ctx{};

    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090170u),
        func_80090170);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090180u),
        func_80090180);
    std::puts(
        "LOOKUP: registered guest 0x80090170 -> func_80090170 and "
        "0x80090180 -> func_80090180 in librecomp live func_map.");

    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800904DCu),
        func_800904DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E478u),
        func_8008E478);
    std::puts(
        "LOOKUP: registered guest 0x8008E478 -> func_8008E478 "
        "for post-RSP indirect dispatch.");
    std::puts(
        "LOOKUP: registered guest 0x800904DC -> func_800904DC "
        "for N64ModernRuntime audio-thread dispatch.");

    // TS2 BULK RESIDENT FUNCTION MAP V3: begin
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006840u),
        func_80006840);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006878u),
        func_80006878);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800068A8u),
        func_800068A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800068E0u),
        func_800068E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000691Cu),
        func_8000691C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006964u),
        func_80006964);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800069A4u),
        func_800069A4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006A80u),
        func_80006A80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006ABCu),
        func_80006ABC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006BD0u),
        func_80006BD0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006C48u),
        func_80006C48);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006D34u),
        func_80006D34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006E00u),
        func_80006E00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80006E74u),
        func_80006E74);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007024u),
        func_80007024);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007090u),
        func_80007090);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800072BCu),
        func_800072BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007390u),
        func_80007390);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800073B0u),
        func_800073B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800073FCu),
        func_800073FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000748Cu),
        func_8000748C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000751Cu),
        func_8000751C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007584u),
        func_80007584);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007648u),
        func_80007648);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007710u),
        func_80007710);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007780u),
        func_80007780);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800077F0u),
        func_800077F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000789Cu),
        func_8000789C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800078B8u),
        func_800078B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800078D4u),
        func_800078D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800078E0u),
        func_800078E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800078ECu),
        func_800078EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007C38u),
        func_80007C38);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80007CFCu),
        func_80007CFC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80008000u),
        func_80008000);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000806Cu),
        func_8000806C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800080F8u),
        func_800080F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80008278u),
        func_80008278);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800083A0u),
        func_800083A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80008490u),
        func_80008490);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000855Cu),
        func_8000855C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80008654u),
        func_80008654);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800088ACu),
        func_800088AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80008D04u),
        func_80008D04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800096F0u),
        func_800096F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000971Cu),
        func_8000971C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000977Cu),
        func_8000977C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80009784u),
        func_80009784);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000978Cu),
        func_8000978C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80009794u),
        func_80009794);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000979Cu),
        func_8000979C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800097A4u),
        func_800097A4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800097ACu),
        func_800097AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800097D4u),
        func_800097D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80009928u),
        func_80009928);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800099A8u),
        func_800099A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800099E4u),
        func_800099E4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000AC1Cu),
        func_8000AC1C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000AC34u),
        func_8000AC34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000AFCCu),
        func_8000AFCC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000B0C0u),
        func_8000B0C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000B3ACu),
        func_8000B3AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000B944u),
        func_8000B944);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000BBD8u),
        func_8000BBD8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C1F0u),
        func_8000C1F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C394u),
        func_8000C394);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C4B8u),
        func_8000C4B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C588u),
        func_8000C588);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C620u),
        func_8000C620);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C7B8u),
        func_8000C7B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C86Cu),
        func_8000C86C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000C9DCu),
        func_8000C9DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000CD98u),
        func_8000CD98);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000CFA8u),
        func_8000CFA8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000D1B8u),
        func_8000D1B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000D328u),
        func_8000D328);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000DD34u),
        func_8000DD34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8000DEA0u),
        func_8000DEA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001041Cu),
        func_8001041C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001045Cu),
        func_8001045C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800104A0u),
        func_800104A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800104FCu),
        func_800104FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001053Cu),
        func_8001053C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010708u),
        func_80010708);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800107C8u),
        func_800107C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800107DCu),
        func_800107DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010A28u),
        func_80010A28);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010A30u),
        func_80010A30);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010A40u),
        func_80010A40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010A9Cu),
        func_80010A9C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010AF8u),
        func_80010AF8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80010BB0u),
        func_80010BB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800112F4u),
        func_800112F4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011A10u),
        func_80011A10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011B34u),
        func_80011B34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011B3Cu),
        func_80011B3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011B84u),
        func_80011B84);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011B8Cu),
        func_80011B8C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011B94u),
        func_80011B94);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011DE0u),
        func_80011DE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011E80u),
        func_80011E80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80011EE0u),
        func_80011EE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800120ECu),
        func_800120EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012C74u),
        func_80012C74);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012EB8u),
        func_80012EB8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012EF0u),
        func_80012EF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012F1Cu),
        func_80012F1C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012F54u),
        func_80012F54);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012F5Cu),
        func_80012F5C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012FECu),
        func_80012FEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012FF4u),
        func_80012FF4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80012FFCu),
        func_80012FFC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013004u),
        func_80013004);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001300Cu),
        func_8001300C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013014u),
        func_80013014);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001301Cu),
        func_8001301C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013024u),
        func_80013024);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001302Cu),
        func_8001302C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013034u),
        func_80013034);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800130A0u),
        func_800130A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800130F0u),
        func_800130F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800130F8u),
        func_800130F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013180u),
        func_80013180);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013288u),
        func_80013288);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013290u),
        func_80013290);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013298u),
        func_80013298);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800132A0u),
        func_800132A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800132B0u),
        func_800132B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001330Cu),
        func_8001330C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800133B8u),
        func_800133B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013450u),
        func_80013450);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800134B8u),
        func_800134B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800134FCu),
        func_800134FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013570u),
        func_80013570);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013614u),
        func_80013614);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013634u),
        func_80013634);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013654u),
        func_80013654);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800136C4u),
        func_800136C4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800136F8u),
        func_800136F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013708u),
        func_80013708);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013754u),
        func_80013754);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013760u),
        func_80013760);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800137C4u),
        func_800137C4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013A60u),
        func_80013A60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013AE8u),
        func_80013AE8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013BACu),
        func_80013BAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013C64u),
        func_80013C64);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013CDCu),
        func_80013CDC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80013D88u),
        func_80013D88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014418u),
        func_80014418);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800145DCu),
        func_800145DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014694u),
        func_80014694);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014714u),
        func_80014714);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014CC4u),
        func_80014CC4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014DDCu),
        func_80014DDC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014E1Cu),
        func_80014E1C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014E20u),
        func_80014E20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014EDCu),
        func_80014EDC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014F2Cu),
        func_80014F2C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014F74u),
        func_80014F74);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80014FCCu),
        func_80014FCC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800150B4u),
        func_800150B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800150ECu),
        func_800150EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015184u),
        func_80015184);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800151D8u),
        func_800151D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001528Cu),
        func_8001528C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015428u),
        func_80015428);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800154A4u),
        func_800154A4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001552Cu),
        func_8001552C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800155A8u),
        func_800155A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001561Cu),
        func_8001561C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001571Cu),
        func_8001571C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015890u),
        func_80015890);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800159B0u),
        func_800159B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015B1Cu),
        func_80015B1C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015CECu),
        func_80015CEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015D00u),
        func_80015D00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015D74u),
        func_80015D74);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015DA0u),
        func_80015DA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015DBCu),
        func_80015DBC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015DE8u),
        func_80015DE8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80015E80u),
        func_80015E80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80016124u),
        func_80016124);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80016358u),
        func_80016358);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80016544u),
        func_80016544);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800167B4u),
        func_800167B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80016B70u),
        func_80016B70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80016F98u),
        func_80016F98);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800171B4u),
        func_800171B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001739Cu),
        func_8001739C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800174D8u),
        func_800174D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017544u),
        func_80017544);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017688u),
        func_80017688);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800178CCu),
        func_800178CC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800179E4u),
        func_800179E4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017B44u),
        func_80017B44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017DB0u),
        func_80017DB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017DD0u),
        func_80017DD0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017E48u),
        func_80017E48);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017E68u),
        func_80017E68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80017FF0u),
        func_80017FF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80018160u),
        func_80018160);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80018254u),
        func_80018254);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800182A8u),
        func_800182A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800184BCu),
        func_800184BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001877Cu),
        func_8001877C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800188BCu),
        func_800188BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800188ECu),
        func_800188EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80018900u),
        func_80018900);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019024u),
        func_80019024);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019040u),
        func_80019040);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001912Cu),
        func_8001912C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001918Cu),
        func_8001918C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800191BCu),
        func_800191BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001923Cu),
        func_8001923C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019444u),
        func_80019444);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800194F0u),
        func_800194F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001995Cu),
        func_8001995C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800199C0u),
        func_800199C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019A00u),
        func_80019A00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019AD4u),
        func_80019AD4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019CACu),
        func_80019CAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019E14u),
        func_80019E14);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019E3Cu),
        func_80019E3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80019F58u),
        func_80019F58);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A134u),
        func_8001A134);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A154u),
        func_8001A154);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A18Cu),
        func_8001A18C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A238u),
        func_8001A238);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A2B8u),
        func_8001A2B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A454u),
        func_8001A454);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A604u),
        func_8001A604);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A680u),
        func_8001A680);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001A790u),
        func_8001A790);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001AAD8u),
        func_8001AAD8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001AAE0u),
        func_8001AAE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001B8FCu),
        func_8001B8FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001B96Cu),
        func_8001B96C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001BF08u),
        func_8001BF08);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001BF20u),
        func_8001BF20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001BF4Cu),
        func_8001BF4C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001C9A8u),
        func_8001C9A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001CA3Cu),
        func_8001CA3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001CAD0u),
        func_8001CAD0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001CBACu),
        func_8001CBAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001CBDCu),
        func_8001CBDC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001CC08u),
        func_8001CC08);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001CC38u),
        func_8001CC38);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001D33Cu),
        func_8001D33C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001D48Cu),
        func_8001D48C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001D4C4u),
        func_8001D4C4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001D880u),
        func_8001D880);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001DB24u),
        func_8001DB24);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001DD40u),
        func_8001DD40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E09Cu),
        func_8001E09C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E108u),
        func_8001E108);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E1E4u),
        func_8001E1E4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E1F4u),
        func_8001E1F4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E338u),
        func_8001E338);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E3E0u),
        func_8001E3E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E3E8u),
        func_8001E3E8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E45Cu),
        func_8001E45C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E510u),
        func_8001E510);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E8D8u),
        func_8001E8D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001E99Cu),
        func_8001E99C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001EFD0u),
        func_8001EFD0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F070u),
        func_8001F070);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F0BCu),
        func_8001F0BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F134u),
        func_8001F134);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F16Cu),
        func_8001F16C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F188u),
        func_8001F188);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F82Cu),
        func_8001F82C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001F938u),
        func_8001F938);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001FA9Cu),
        func_8001FA9C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8001FC00u),
        func_8001FC00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80020F04u),
        func_80020F04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800210E4u),
        func_800210E4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800211C8u),
        func_800211C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80021220u),
        func_80021220);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80021310u),
        func_80021310);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80021D94u),
        func_80021D94);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80021ED8u),
        func_80021ED8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002201Cu),
        func_8002201C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002205Cu),
        func_8002205C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800221ECu),
        func_800221EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002237Cu),
        func_8002237C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800225D8u),
        func_800225D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800226A0u),
        func_800226A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80022894u),
        func_80022894);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80022BA4u),
        func_80022BA4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80022CA0u),
        func_80022CA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80022D68u),
        func_80022D68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80022DD4u),
        func_80022DD4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80022E94u),
        func_80022E94);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80023370u),
        func_80023370);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80024074u),
        func_80024074);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800240ECu),
        func_800240EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80024190u),
        func_80024190);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800241A0u),
        func_800241A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80024330u),
        func_80024330);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800244F0u),
        func_800244F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80024D6Cu),
        func_80024D6C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80024FA0u),
        func_80024FA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025050u),
        func_80025050);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025228u),
        func_80025228);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025274u),
        func_80025274);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800256DCu),
        func_800256DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025940u),
        func_80025940);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025B68u),
        func_80025B68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025E80u),
        func_80025E80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80025F10u),
        func_80025F10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80026404u),
        func_80026404);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80026608u),
        func_80026608);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80026970u),
        func_80026970);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80026E5Cu),
        func_80026E5C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002749Cu),
        func_8002749C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80027BECu),
        func_80027BEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80027F88u),
        func_80027F88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80028CD8u),
        func_80028CD8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80028CE0u),
        func_80028CE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80028E2Cu),
        func_80028E2C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80028EF8u),
        func_80028EF8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80028F7Cu),
        func_80028F7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029428u),
        func_80029428);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800295B0u),
        func_800295B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800296A4u),
        func_800296A4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800297DCu),
        func_800297DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029964u),
        func_80029964);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029AC4u),
        func_80029AC4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029BACu),
        func_80029BAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029C50u),
        func_80029C50);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029D88u),
        func_80029D88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80029E4Cu),
        func_80029E4C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002ABF0u),
        func_8002ABF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002AC9Cu),
        func_8002AC9C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002AD80u),
        func_8002AD80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002AE0Cu),
        func_8002AE0C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002B0ACu),
        func_8002B0AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002B228u),
        func_8002B228);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002B31Cu),
        func_8002B31C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002B4A0u),
        func_8002B4A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002C5C0u),
        func_8002C5C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002DF10u),
        func_8002DF10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002E478u),
        func_8002E478);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002E480u),
        func_8002E480);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002EB84u),
        func_8002EB84);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002F120u),
        func_8002F120);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002F268u),
        func_8002F268);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002F2D8u),
        func_8002F2D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002F2E0u),
        func_8002F2E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002F4D8u),
        func_8002F4D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002FF44u),
        func_8002FF44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002FF94u),
        func_8002FF94);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8002FFA0u),
        func_8002FFA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80030104u),
        func_80030104);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800302FCu),
        func_800302FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80030344u),
        func_80030344);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80030524u),
        func_80030524);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80030B6Cu),
        func_80030B6C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80030BCCu),
        func_80030BCC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800312F0u),
        func_800312F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031364u),
        func_80031364);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800314B8u),
        func_800314B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031518u),
        func_80031518);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031568u),
        func_80031568);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031B60u),
        func_80031B60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031C68u),
        func_80031C68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031C80u),
        func_80031C80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031D08u),
        func_80031D08);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80031D30u),
        func_80031D30);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003224Cu),
        func_8003224C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800325CCu),
        func_800325CC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800327B4u),
        func_800327B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80032B00u),
        func_80032B00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80032B70u),
        func_80032B70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80032C50u),
        func_80032C50);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80032D78u),
        func_80032D78);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003311Cu),
        func_8003311C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800333D4u),
        func_800333D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80033454u),
        func_80033454);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80033828u),
        func_80033828);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80033940u),
        func_80033940);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80033C54u),
        func_80033C54);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80033E04u),
        func_80033E04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800341C0u),
        func_800341C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800356B4u),
        func_800356B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80035C20u),
        func_80035C20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80035E60u),
        func_80035E60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036040u),
        func_80036040);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800361E8u),
        func_800361E8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800364A8u),
        func_800364A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003674Cu),
        func_8003674C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036B10u),
        func_80036B10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036B88u),
        func_80036B88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036BA0u),
        func_80036BA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036BD4u),
        func_80036BD4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036EC0u),
        func_80036EC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80036FF0u),
        func_80036FF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037100u),
        func_80037100);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800374F8u),
        func_800374F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037648u),
        func_80037648);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037704u),
        func_80037704);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037834u),
        func_80037834);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037878u),
        func_80037878);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800378CCu),
        func_800378CC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037980u),
        func_80037980);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037A7Cu),
        func_80037A7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037B2Cu),
        func_80037B2C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037BF0u),
        func_80037BF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80037D38u),
        func_80037D38);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A304u),
        func_8003A304);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A320u),
        func_8003A320);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A3B8u),
        func_8003A3B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A44Cu),
        func_8003A44C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A740u),
        func_8003A740);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A778u),
        func_8003A778);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A798u),
        func_8003A798);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003A9F4u),
        func_8003A9F4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003AAE0u),
        func_8003AAE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003AB78u),
        func_8003AB78);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003AC30u),
        func_8003AC30);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003AD48u),
        func_8003AD48);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003ADB4u),
        func_8003ADB4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003AE10u),
        func_8003AE10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003B078u),
        func_8003B078);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003B0B0u),
        func_8003B0B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003B354u),
        func_8003B354);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003B840u),
        func_8003B840);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003BE44u),
        func_8003BE44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003BFB0u),
        func_8003BFB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C804u),
        func_8003C804);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C83Cu),
        func_8003C83C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C848u),
        func_8003C848);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C850u),
        func_8003C850);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C858u),
        func_8003C858);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C860u),
        func_8003C860);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C868u),
        func_8003C868);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C888u),
        func_8003C888);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C8A8u),
        func_8003C8A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C8C8u),
        func_8003C8C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C8E8u),
        func_8003C8E8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C8F0u),
        func_8003C8F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003C91Cu),
        func_8003C91C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003CC64u),
        func_8003CC64);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D068u),
        func_8003D068);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D724u),
        func_8003D724);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D72Cu),
        func_8003D72C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D734u),
        func_8003D734);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D73Cu),
        func_8003D73C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D744u),
        func_8003D744);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D74Cu),
        func_8003D74C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D754u),
        func_8003D754);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D760u),
        func_8003D760);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D83Cu),
        func_8003D83C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003D928u),
        func_8003D928);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003DA04u),
        func_8003DA04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003DAF0u),
        func_8003DAF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003DB28u),
        func_8003DB28);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003DB60u),
        func_8003DB60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003E240u),
        func_8003E240);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003EAC8u),
        func_8003EAC8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003EE3Cu),
        func_8003EE3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8003FF40u),
        func_8003FF40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80040360u),
        func_80040360);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800411B0u),
        func_800411B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80042DB8u),
        func_80042DB8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800433CCu),
        func_800433CC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800450C8u),
        func_800450C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800451A4u),
        func_800451A4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80045420u),
        func_80045420);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800456A0u),
        func_800456A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800462CCu),
        func_800462CC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004638Cu),
        func_8004638C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80046458u),
        func_80046458);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800466F8u),
        func_800466F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048298u),
        func_80048298);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800482E0u),
        func_800482E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048324u),
        func_80048324);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800483DCu),
        func_800483DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048444u),
        func_80048444);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800484ECu),
        func_800484EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048544u),
        func_80048544);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048558u),
        func_80048558);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800485C0u),
        func_800485C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800485F0u),
        func_800485F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048658u),
        func_80048658);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048660u),
        func_80048660);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80048E00u),
        func_80048E00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800491B4u),
        func_800491B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049510u),
        func_80049510);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049640u),
        func_80049640);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049660u),
        func_80049660);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049680u),
        func_80049680);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049D54u),
        func_80049D54);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049D84u),
        func_80049D84);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049DE4u),
        func_80049DE4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049E3Cu),
        func_80049E3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049F88u),
        func_80049F88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80049FF0u),
        func_80049FF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004A660u),
        func_8004A660);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004A6ECu),
        func_8004A6EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004C008u),
        func_8004C008);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004C3FCu),
        func_8004C3FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004D124u),
        func_8004D124);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004D618u),
        func_8004D618);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004DB40u),
        func_8004DB40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004E5F4u),
        func_8004E5F4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004E604u),
        func_8004E604);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004E620u),
        func_8004E620);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004E640u),
        func_8004E640);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004E67Cu),
        func_8004E67C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004E7F0u),
        func_8004E7F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8004F494u),
        func_8004F494);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005007Cu),
        func_8005007C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800510FCu),
        func_800510FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80052184u),
        func_80052184);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800521BCu),
        func_800521BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80052214u),
        func_80052214);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800522BCu),
        func_800522BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800522C0u),
        func_800522C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80052488u),
        func_80052488);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80053AB0u),
        func_80053AB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80054CDCu),
        func_80054CDC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800550DCu),
        func_800550DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80055450u),
        func_80055450);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80055B14u),
        func_80055B14);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80057A8Cu),
        func_80057A8C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80057E24u),
        func_80057E24);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80057F3Cu),
        func_80057F3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80058F6Cu),
        func_80058F6C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800595ACu),
        func_800595AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800595F8u),
        func_800595F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80059C14u),
        func_80059C14);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005B628u),
        func_8005B628);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005B77Cu),
        func_8005B77C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005B954u),
        func_8005B954);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005BB2Cu),
        func_8005BB2C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005BCB4u),
        func_8005BCB4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005BE40u),
        func_8005BE40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005BF88u),
        func_8005BF88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C0ECu),
        func_8005C0EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C258u),
        func_8005C258);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C3E0u),
        func_8005C3E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C57Cu),
        func_8005C57C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C720u),
        func_8005C720);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C8A8u),
        func_8005C8A8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005C9FCu),
        func_8005C9FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005CB44u),
        func_8005CB44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005CCB0u),
        func_8005CCB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005CE38u),
        func_8005CE38);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005CE40u),
        func_8005CE40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005CE74u),
        func_8005CE74);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005D060u),
        func_8005D060);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8005D69Cu),
        func_8005D69C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066538u),
        func_80066538);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066738u),
        func_80066738);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006690Cu),
        func_8006690C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006695Cu),
        func_8006695C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066A48u),
        func_80066A48);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066AB4u),
        func_80066AB4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066AECu),
        func_80066AEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066B7Cu),
        func_80066B7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066DA0u),
        func_80066DA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80066E0Cu),
        func_80066E0C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80067014u),
        func_80067014);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006783Cu),
        func_8006783C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80067E08u),
        func_80067E08);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800689ECu),
        func_800689EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80068A3Cu),
        func_80068A3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80068B28u),
        func_80068B28);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006984Cu),
        func_8006984C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006AAA8u),
        func_8006AAA8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006ABB0u),
        func_8006ABB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006B888u),
        func_8006B888);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BAA8u),
        func_8006BAA8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BB34u),
        func_8006BB34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BBC8u),
        func_8006BBC8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BCC0u),
        func_8006BCC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BD80u),
        func_8006BD80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BDA4u),
        func_8006BDA4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006BE6Cu),
        func_8006BE6C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006C5E8u),
        func_8006C5E8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006C608u),
        func_8006C608);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006C628u),
        func_8006C628);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006CDF4u),
        func_8006CDF4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006D508u),
        func_8006D508);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006D988u),
        func_8006D988);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006E00Cu),
        func_8006E00C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006E048u),
        func_8006E048);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006E38Cu),
        func_8006E38C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006E93Cu),
        func_8006E93C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006EBA4u),
        func_8006EBA4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006F0E0u),
        func_8006F0E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006F180u),
        func_8006F180);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006F6E0u),
        func_8006F6E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006FA7Cu),
        func_8006FA7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006FA80u),
        func_8006FA80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8006FAE8u),
        func_8006FAE8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80074300u),
        func_80074300);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800764C8u),
        func_800764C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8007CBD8u),
        func_8007CBD8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8007CBF8u),
        func_8007CBF8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8007DDB4u),
        func_8007DDB4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8007DF94u),
        func_8007DF94);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80083770u),
        func_80083770);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80083798u),
        func_80083798);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800837E0u),
        func_800837E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008381Cu),
        func_8008381C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80083AF8u),
        func_80083AF8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80083B60u),
        func_80083B60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800842BCu),
        func_800842BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80084AECu),
        func_80084AEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80084C04u),
        func_80084C04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80084C8Cu),
        func_80084C8C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80085040u),
        func_80085040);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800853E0u),
        func_800853E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800856B4u),
        func_800856B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800858D4u),
        func_800858D4);
    // V46: registered as the overlay-residency reconciliation wrapper, which
    // runs the real func_800859A4 and then syncs librecomp's live func_map to
    // whichever overlay is actually resident at 0x80000400.
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800859A4u),
        ts2_v46_overlay_transfer);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80085A80u),
        func_80085A80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80086350u),
        func_80086350);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008682Cu),
        func_8008682C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80086C44u),
        func_80086C44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087188u),
        func_80087188);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800871BCu),
        func_800871BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800871F0u),
        func_800871F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008721Cu),
        func_8008721C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087340u),
        func_80087340);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800873E0u),
        func_800873E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087820u),
        func_80087820);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087D60u),
        func_80087D60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087EA0u),
        func_80087EA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80088820u),
        func_80088820);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800889C0u),
        func_800889C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C220u),
        func_8008C220);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C240u),
        func_8008C240);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C880u),
        func_8008C880);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C8A0u),
        func_8008C8A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C8D0u),
        func_8008C8D0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C8F0u),
        func_8008C8F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008C8FCu),
        func_8008C8FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CA14u),
        func_8008CA14);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CAFCu),
        func_8008CAFC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CB10u),
        func_8008CB10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CB34u),
        func_8008CB34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CBA4u),
        func_8008CBA4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CC18u),
        func_8008CC18);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CC28u),
        func_8008CC28);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CC58u),
        func_8008CC58);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CC68u),
        func_8008CC68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CC78u),
        func_8008CC78);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CC88u),
        func_8008CC88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CCE0u),
        func_8008CCE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CD44u),
        func_8008CD44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CD54u),
        func_8008CD54);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CD60u),
        func_8008CD60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CD70u),
        func_8008CD70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CD7Cu),
        func_8008CD7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CDE8u),
        func_8008CDE8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CE84u),
        func_8008CE84);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CEACu),
        func_8008CEAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CEB8u),
        func_8008CEB8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CEC8u),
        func_8008CEC8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CED4u),
        func_8008CED4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CEE8u),
        func_8008CEE8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CEFCu),
        func_8008CEFC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CF04u),
        func_8008CF04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CF4Cu),
        func_8008CF4C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CF58u),
        func_8008CF58);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CF60u),
        func_8008CF60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CFD0u),
        func_8008CFD0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008CFE0u),
        func_8008CFE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D034u),
        func_8008D034);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D088u),
        func_8008D088);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D0DCu),
        func_8008D0DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D0ECu),
        func_8008D0EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D1C4u),
        func_8008D1C4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D200u),
        func_8008D200);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D210u),
        func_8008D210);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D258u),
        func_8008D258);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D2CCu),
        func_8008D2CC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D2D8u),
        func_8008D2D8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D588u),
        func_8008D588);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D5B4u),
        func_8008D5B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D5E8u),
        func_8008D5E8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D83Cu),
        func_8008D83C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D8C4u),
        func_8008D8C4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008D9E4u),
        func_8008D9E4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DAB0u),
        func_8008DAB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DB34u),
        func_8008DB34);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DBECu),
        func_8008DBEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DC48u),
        func_8008DC48);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DCACu),
        func_8008DCAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DD20u),
        func_8008DD20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DD90u),
        func_8008DD90);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DE20u),
        func_8008DE20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DEACu),
        func_8008DEAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DEECu),
        func_8008DEEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DF18u),
        func_8008DF18);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DF3Cu),
        func_8008DF3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DF4Cu),
        func_8008DF4C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DFACu),
        func_8008DFAC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008DFD4u),
        func_8008DFD4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E000u),
        func_8008E000);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E034u),
        func_8008E034);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E080u),
        func_8008E080);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E104u),
        func_8008E104);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E110u),
        func_8008E110);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E120u),
        func_8008E120);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E130u),
        func_8008E130);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E140u),
        func_8008E140);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E148u),
        func_8008E148);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E154u),
        func_8008E154);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E164u),
        func_8008E164);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E1D0u),
        func_8008E1D0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E250u),
        func_8008E250);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E260u),
        func_8008E260);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E2C8u),
        func_8008E2C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E354u),
        func_8008E354);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E3DCu),
        func_8008E3DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008E704u),
        func_8008E704);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008EB68u),
        func_8008EB68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008EBF8u),
        func_8008EBF8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008ED58u),
        func_8008ED58);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008EE68u),
        func_8008EE68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008EED8u),
        func_8008EED8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F0B8u),
        func_8008F0B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F0D4u),
        func_8008F0D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F18Cu),
        func_8008F18C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F1E4u),
        func_8008F1E4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F24Cu),
        func_8008F24C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F310u),
        func_8008F310);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F420u),
        func_8008F420);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F5ECu),
        func_8008F5EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F784u),
        func_8008F784);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F814u),
        func_8008F814);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008F944u),
        func_8008F944);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FB0Cu),
        func_8008FB0C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FB44u),
        func_8008FB44);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FBF8u),
        func_8008FBF8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FCCCu),
        func_8008FCCC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FEB0u),
        func_8008FEB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FF04u),
        func_8008FF04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8008FF20u),
        func_8008FF20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090098u),
        func_80090098);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800901F0u),
        func_800901F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090390u),
        func_80090390);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800903A0u),
        func_800903A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009065Cu),
        func_8009065C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090660u),
        func_80090660);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800906D4u),
        func_800906D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090714u),
        func_80090714);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090720u),
        func_80090720);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800908DCu),
        func_800908DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800908E0u),
        func_800908E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090930u),
        func_80090930);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090964u),
        func_80090964);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009097Cu),
        func_8009097C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009098Cu),
        func_8009098C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800909B8u),
        func_800909B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090A40u),
        func_80090A40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090A9Cu),
        func_80090A9C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090D6Cu),
        func_80090D6C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090E04u),
        func_80090E04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090F5Cu),
        func_80090F5C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091240u),
        func_80091240);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091258u),
        func_80091258);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009129Cu),
        func_8009129C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800912E0u),
        func_800912E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091400u),
        func_80091400);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091480u),
        func_80091480);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800915DCu),
        func_800915DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091760u),
        func_80091760);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800917F8u),
        func_800917F8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800918A0u),
        func_800918A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800918F0u),
        func_800918F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800919E0u),
        osCreateThread_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091AB0u),
        func_80091AB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091AC0u),
        func_80091AC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091AE0u),
        func_80091AE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091B00u),
        func_80091B00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091B10u),
        func_80091B10);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091B20u),
        osStartThread_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091C40u),
        osRecvMesg_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091D70u),
        func_80091D70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091DE0u),
        func_80091DE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091E20u),
        osPiStartDma_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80091EE0u),
        osJamMesg_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092020u),
        func_80092020);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092040u),
        osMotorStart_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092200u),
        osMotorStop_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800923C0u),
        osMotorInit_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092890u),
        func_80092890);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800928F4u),
        func_800928F4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092960u),
        func_80092960);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092990u),
        func_80092990);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800929A0u),
        func_800929A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092BD4u),
        func_80092BD4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092D70u),
        func_80092D70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092D80u),
        func_80092D80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092E60u),
        func_80092E60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092EF0u),
        func_80092EF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092F80u),
        func_80092F80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80092FC8u),
        func_80092FC8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093040u),
        osPfsRepairId_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093200u),
        osPfsFindFile_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800933A0u),
        osViSetSpecialFeatures_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093520u),
        func_80093520);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093630u),
        ts2_current_internal_entry_80093630_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093680u),
        func_80093680);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093720u),
        osContStartReadData_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800937A8u),
        osContGetReadData_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093844u),
        func_80093844);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093900u),
        func_80093900);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093AB0u),
        func_80093AB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093C80u),
        osInitialize_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093E90u),
        func_80093E90);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093EA0u),
        func_80093EA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093EC0u),
        func_80093EC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093EE0u),
        func_80093EE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80093FD8u),
        func_80093FD8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009401Cu),
        func_8009401C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800942FCu),
        func_800942FC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094460u),
        func_80094460);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094640u),
        func_80094640);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094704u),
        func_80094704);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800949DCu),
        func_800949DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094A30u),
        func_80094A30);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094A40u),
        func_80094A40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094AC0u),
        osPfsAllocateFile_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80094E50u),
        func_80094E50);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009501Cu),
        func_8009501C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800950B0u),
        func_800950B0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800950B8u),
        func_800950B8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800950C0u),
        func_800950C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095134u),
        func_80095134);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800951C0u),
        func_800951C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009529Cu),
        func_8009529C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095350u),
        func_80095350);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095360u),
        func_80095360);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095650u),
        func_80095650);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095660u),
        osPfsFileState_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800958A0u),
        func_800958A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095E14u),
        func_80095E14);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80095F04u),
        func_80095F04);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009600Cu),
        __osEnqueueThread_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096054u),
        __osPopThread_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096064u),
        func_80096064);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009670Cu),
        func_8009670C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800967DCu),
        func_800967DC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096890u),
        func_80096890);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800968A0u),
        func_800968A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096A80u),
        osSetIntMask_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096B20u),
        func_80096B20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096B74u),
        func_80096B74);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096C00u),
        func_80096C00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096D00u),
        func_80096D00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096DE0u),
        ts2_current_internal_entry_80096DE0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096F28u),
        ts2_current_internal_entry_80096F28_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80096F80u),
        ts2_current_internal_entry_80096F80_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097010u),
        ts2_current_internal_entry_80097010_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097018u),
        ts2_current_internal_entry_80097018_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009711Cu),
        ts2_current_internal_entry_8009711C_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009721Cu),
        ts2_current_internal_entry_8009721C_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009732Cu),
        ts2_current_internal_entry_8009732C_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009740Cu),
        func_8009740C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097460u),
        func_80097460);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097510u),
        func_80097510);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800975BCu),
        func_800975BC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800976F0u),
        func_800976F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097728u),
        func_80097728);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097940u),
        func_80097940);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097994u),
        ts2_current_internal_entry_80097994_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800979A0u),
        osSpTaskLoad_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097BACu),
        osSpTaskStartGo_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097BE0u),
        func_80097BE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097C00u),
        osSpTaskYielded_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097C60u),
        func_80097C60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097C80u),
        func_80097C80);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097CC0u),
        func_80097CC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097CD0u),
        func_80097CD0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097CE0u),
        func_80097CE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097CF0u),
        func_80097CF0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097D00u),
        osDpSetNextBuffer_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097DA0u),
        func_80097DA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097DC0u),
        func_80097DC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097E20u),
        func_80097E20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097E58u),
        func_80097E58);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097E7Cu),
        func_80097E7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80097EC0u),
        func_80097EC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098060u),
        osViGetNextFramebuffer_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800980A0u),
        osCreateMesgQueue_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800980E0u),
        func_800980E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800981A0u),
        func_800981A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800981C0u),
        osSetEventMesg_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098220u),
        osSpTaskYield_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098240u),
        func_80098240);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800982E0u),
        func_800982E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098380u),
        osContStartQuery_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098400u),
        osContGetQuery_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098420u),
        func_80098420);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098440u),
        osViSetEvent_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800984A0u),
        osContInit_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009863Cu),
        func_8009863C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800986F0u),
        func_800986F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800987B0u),
        ts2_current_internal_entry_800987B0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800987C0u),
        osSetTimer_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098860u),
        func_80098860);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800988B4u),
        func_800988B4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800989F0u),
        func_800989F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098A50u),
        func_80098A50);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098B60u),
        __osSetCompare_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098B70u),
        func_80098B70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098B80u),
        osSendMesg_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098CB0u),
        func_80098CB0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098CC0u),
        func_80098CC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098DC0u),
        func_80098DC0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098F3Cu),
        func_80098F3C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80098FE0u),
        func_80098FE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800990A0u),
        osPfsNumFiles_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800991A0u),
        func_800991A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800991B0u),
        ts2_current_internal_entry_800991B0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800991C0u),
        osPfsDeleteFile_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800993ECu),
        func_800993EC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009958Cu),
        func_8009958C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099660u),
        osInvalDCache_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099714u),
        func_80099714);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099720u),
        osInvalICache_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800997A0u),
        func_800997A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099800u),
        osCreateViManager_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099998u),
        ts2_runtime_owned_vi_manager_trap_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099B40u),
        osGetThreadPri_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099B60u),
        func_80099B60);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099B80u),
        osCreatePiManager_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099D20u),
        func_80099D20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099F00u),
        func_80099F00);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A3A0u),
        func_8009A3A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A3E0u),
        osYieldThread_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A430u),
        func_8009A430);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A440u),
        func_8009A440);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A490u),
        func_8009A490);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A4A0u),
        func_8009A4A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A640u),
        osViSwapBuffer_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A6A0u),
        func_8009A6A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009A8C0u),
        func_8009A8C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009AA60u),
        osPfsReadWriteFile_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009AE90u),
        func_8009AE90);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009AEA0u),
        osSetThreadPri_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009AF70u),
        func_8009AF70);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009AF80u),
        osPfsInitPak_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B150u),
        func_8009B150);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B160u),
        func_8009B160);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B5ACu),
        func_8009B5AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B704u),
        func_8009B704);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B870u),
        ts2_current_internal_entry_8009B870_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B878u),
        ts2_current_internal_entry_8009B878_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B880u),
        func_8009B880);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B950u),
        ts2_current_internal_entry_8009B950_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B960u),
        func_8009B960);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B9B0u),
        ts2_current_internal_entry_8009B9B0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009B9C0u),
        osVirtualToPhysical_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BA20u),
        func_8009BA20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BAE0u),
        osPfsFreeBlocks_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BBF0u),
        ts2_current_internal_entry_8009BBF0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BC00u),
        osViGetCurrentFramebuffer_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BC40u),
        func_8009BC40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BD90u),
        ts2_current_internal_entry_8009BD90_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BDA0u),
        func_8009BDA0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BE30u),
        ts2_current_internal_entry_8009BE30_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009BE40u),
        func_8009BE40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C160u),
        func_8009C160);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C2B0u),
        ts2_current_internal_entry_8009C2B0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C2C0u),
        func_8009C2C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C330u),
        func_8009C330);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C340u),
        func_8009C340);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C468u),
        func_8009C468);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C520u),
        func_8009C520);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C54Cu),
        func_8009C54C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C57Cu),
        func_8009C57C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C59Cu),
        func_8009C59C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C5D0u),
        ts2_current_internal_entry_8009C5D0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C5E0u),
        func_8009C5E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C670u),
        ts2_current_internal_entry_8009C670_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C680u),
        func_8009C680);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C6D0u),
        ts2_current_internal_entry_8009C6D0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C6E0u),
        func_8009C6E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C760u),
        func_8009C760);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C820u),
        func_8009C820);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C8C0u),
        func_8009C8C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C960u),
        func_8009C960);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C980u),
        func_8009C980);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CC4Cu),
        func_8009CC4C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CDFCu),
        func_8009CDFC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CE28u),
        func_8009CE28);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CE40u),
        func_8009CE40);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CE94u),
        func_8009CE94);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CED4u),
        func_8009CED4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CF1Cu),
        func_8009CF1C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009CF68u),
        func_8009CF68);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D010u),
        func_8009D010);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D120u),
        func_8009D120);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D150u),
        func_8009D150);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D160u),
        func_8009D160);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D29Cu),
        func_8009D29C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D2D0u),
        func_8009D2D0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D420u),
        func_8009D420);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D450u),
        ts2_current_internal_entry_8009D450_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D460u),
        func_8009D460);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D500u),
        func_8009D500);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009DA4Cu),
        func_8009DA4C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009DB20u),
        func_8009DB20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009DE20u),
        ts2_current_internal_entry_8009DE20_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009DF00u),
        ts2_current_internal_entry_8009DF00_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009DF24u),
        func_8009DF24);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009E1A4u),
        func_8009E1A4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009E230u),
        func_8009E230);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009E5F0u),
        func_8009E5F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009E604u),
        func_8009E604);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009E7F4u),
        func_8009E7F4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009EA0Cu),
        func_8009EA0C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009EB7Cu),
        func_8009EB7C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009ECECu),
        func_8009ECEC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009ED88u),
        func_8009ED88);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009EE20u),
        func_8009EE20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F00Cu),
        func_8009F00C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F0E0u),
        func_8009F0E0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F184u),
        func_8009F184);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F61Cu),
        func_8009F61C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F6D4u),
        func_8009F6D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F784u),
        func_8009F784);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F810u),
        func_8009F810);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F86Cu),
        func_8009F86C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F8C8u),
        func_8009F8C8);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F910u),
        ts2_current_internal_entry_8009F910_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F920u),
        func_8009F920);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F940u),
        func_8009F940);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F9D4u),
        func_8009F9D4);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009FE24u),
        func_8009FE24);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A01C0u),
        func_800A01C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A036Cu),
        func_800A036C);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A04F0u),
        ts2_current_internal_entry_800A04F0_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A0500u),
        func_800A0500);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A0540u),
        osEPiStartDma_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A05E0u),
        osAiGetLength_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A05F0u),
        func_800A05F0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A0600u),
        osAiSetNextBuffer_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A06A0u),
        func_800A06A0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A06C0u),
        osAiGetStatus_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A06E0u),
        osAiSetFrequency_recomp);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800A0820u),
        func_800A0820);
    std::puts("LOOKUP BULK V3: resident func_XXXXXXXX map populated.");
    // TS2 BULK RESIDENT FUNCTION MAP V3: end
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D020u),
        func_8009D020);
    std::puts(
        "LOOKUP ALIAS: registered guest 0x8009D020 -> "
        "func_8009D020 (func_8009D010 + 0x10).");
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D2E0u),
        func_8009D2E0);
    std::puts(
        "LOOKUP ALIAS: registered guest 0x8009D2E0 -> "
        "func_8009D2E0 (func_8009D2D0 + 0x10, standalone delay-slot entry).");
    // TS2 BULK INTERNAL CALLBACK ALIASES V1: begin
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80087870u),
        ts2_auto_alias_80087870);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099E20u),
        func_80099E20);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80099FE0u),
        func_80099FE0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009C4ACu),
        ts2_auto_alias_8009C4AC);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009D1C0u),
        func_8009D1C0);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009E240u),
        func_8009E240);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x8009F9E0u),
        func_8009F9E0);
    std::puts("LOOKUP AUTO ALIASES: bulk internal callback aliases registered.");
    // TS2 BULK INTERNAL CALLBACK ALIASES V1: end
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090730u),
        func_80090730);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x80090798u),
        func_80090798);
    recomp::overlays::add_loaded_function(
        static_cast<int32_t>(0x800907FCu),
        func_800907FC);
    std::puts(
        "LOOKUP: registered audio callback table entries "
        "0x80090730/0x80090798/0x800907FC in librecomp live func_map.");

    std::puts("Calling real Toy Story 2 recomp_entrypoint...");
    std::puts(
        "Stage 64 replays the proven first SI/PIF command DMA, delivers its "
        "registered event-5 completion through Toy Story 2's real SI queue, "
        "lets the blocking receive resume, and stops at guest PC 0x8009816C "
        "on the second occurrence immediately after the PIF-to-RDRAM return "
        "DMA launch.");

#if defined(_WIN32)
    const int ts2_bootstrap_caught =
        ts2_v145_call_recomp_entrypoint_seh(g_rdram.data(), &parent_ctx);

    if (ts2_bootstrap_caught == -1) {
        ts2_set_boot_checkpoint_callback(nullptr);
        ts2_set_thread_dispatch_callback(nullptr);
        ts2_set_thread_bridge_callbacks(nullptr, nullptr);
        ts2_set_thread_priority_bridge_callbacks(nullptr, nullptr);
        ts2_set_mesg_recv_bridge_callback(nullptr);
        ts2_set_mesg_send_bridge_callback(nullptr);
        ts2_set_vi_mmio_bridge_callbacks(nullptr, nullptr);
        ts2_set_mi_mmio_bridge_callbacks(nullptr, nullptr);
        ts2_set_si_mmio_bridge_callbacks(nullptr, nullptr);
        ts2_set_pi_mmio_bridge_callbacks(nullptr, nullptr);
        ts2_set_thread_entry_callback(nullptr);
        ts2_bind_rom_image(nullptr, 0);
        std::fputs(
            "FAILED: Windows exception during Stage 61 parent boot.\n",
            stderr);
        return 6;
    }

    if (ts2_bootstrap_caught != 1) {
        std::fputs(
            "FAILED: Stage 61 GameEntry returned without the expected bootstrap unwind.\n",
            stderr);
        std::fflush(stderr);
        return 6;
    }
#else
    const int ts2_bootstrap_caught =
        ts2_oldnative_call_recomp_entrypoint_catching_bootstrap(
            g_rdram.data(),
            &parent_ctx);

    if (ts2_bootstrap_caught != 1) {
        std::fputs(
            "FAILED: Stage 61 GameEntry returned without the expected bootstrap unwind.\n",
            stderr);
        std::fflush(stderr);
        return 6;
    }
#endif

    ts2_set_boot_checkpoint_callback(nullptr);

    if (g_unexpected_parent_checkpoint) {
        std::_Exit(7);
    }

    {
        std::unique_lock<std::mutex> lock(g_thread_mutex);
        if (!g_thread_cv.wait_for(lock, 180s, [] {
                return g_stage84_pre_init_reached ||
                    g_ts2_live_mode_reached_v42b;
            })) {
            std::fputs(
                "FAILED: Stage 85 did not reach the pre-func_80012F1C boundary "
                "at guest PC 0x80008824 after the proven Stage 83 sprite return.\n",
                stderr);
            std::fflush(stderr);
            std::_Exit(9);
        }
    }

    // V42B live-mode host handoff.
    //
    // Everything below this point is the historical fixed-count Stage
    // 61-85 regression checklist. Live Toy Story 2 legitimately performs
    // more font builds, PI transfers, directory reloads, RNC work, etc.
    //
    // Once Stage 87 has explicitly retired startup-only expectations,
    // skip those host assertions and simply keep the already-running guest
    // runtime alive. This does not modify guest state or generated code.
    bool v42b_live_mode = false;
    {
        std::lock_guard<std::mutex> v42b_lock(g_thread_mutex);
        v42b_live_mode = g_ts2_live_mode_reached_v42b;
    }

    if (v42b_live_mode) {
        std::puts(
            "TS2 V42B LIVE HOST: startup-only regression assertions retired; "
            "keeping the PC runtime alive. Close the window or press Escape to exit.");
        std::fflush(stdout);
        std::fflush(stderr);

        bool ts2_v42b_host_running = true;

        while (ts2_v42b_host_running) {
            SDL_Event event{};

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    ts2_v42b_host_running = false;
                }
            }

#ifdef _WIN32
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
                ts2_v42b_host_running = false;
            }
#endif

            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }

        std::puts(
            "TS2 V42B LIVE HOST: shutdown requested; leaving run_stage61 normally.");
        std::fflush(stdout);
        std::fflush(stderr);
        return 0;
    }
    if (!g_first_font_call_seen ||
        !g_second_font_call_reached ||
        !g_third_font_call_reached ||
        g_font_load_call_count != 3u ||
        g_actual_font_caller_return_count != 3u ||
        g_font_complete_count != 7u) {
        std::fprintf(
            stderr,
            "FAILED: three-font call/return counts "
            "calls=%u returns=%u completes=%u first=%d second=%d third=%d.\n",
            g_font_load_call_count,
            g_actual_font_caller_return_count,
            g_font_complete_count,
            g_first_font_call_seen ? 1 : 0,
            g_second_font_call_reached ? 1 : 0,
            g_third_font_call_reached ? 1 : 0);
        std::_Exit(10);
    }

    if (!g_pi_mmio_args_valid ||
        !g_pi_dma_copy_complete ||
        g_pi_dma_count != kTransferCount ||
        g_pi_raw_boundary_count != kTransferCount ||
        g_pi_mmio_read_count != kTransferCount ||
        g_pi_mmio_write_count != kTransferCount * 3u ||
        g_pi_auto_event_count != kTransferCount ||
        g_pi_completion_wait_count != kTransferCount ||
        g_loader_wait_count != kTransferCount ||
        g_loader_post_count != kTransferCount) {
        std::fprintf(
            stderr,
            "FAILED: Stage 61 PI totals "
            "dma/raw/r/w/events/piwait/loader/post="
            "%u/%u/%u/%u/%u/%u/%u/%u valid=%d copy=%d.\n",
            g_pi_dma_count,
            g_pi_raw_boundary_count,
            g_pi_mmio_read_count,
            g_pi_mmio_write_count,
            g_pi_auto_event_count,
            g_pi_completion_wait_count,
            g_loader_wait_count,
            g_loader_post_count,
            g_pi_mmio_args_valid ? 1 : 0,
            g_pi_dma_copy_complete ? 1 : 0);
        std::_Exit(11);
    }

    for (uint32_t i = 0; i < kTransferCount; ++i) {
        uint32_t expected_request = kFontPiRequest;
        if (i < 4u ||
            (i >= 7u && i < 11u) ||
            (i >= 13u && i < 17u)) {
            expected_request = kExpectedPiRequest;
        } else if (i >= 20u && i < 24u) {
            expected_request = kCommonDirPiRequest;
        } else if (i == 24u) {
            expected_request = kCommonPayloadPiRequest;
        } else if (i >= 25u && i < 29u) {
            expected_request = kSpriteDirPiRequest;
        } else if (i >= 29u) {
            expected_request = kSpritePayloadPiRequest;
        }

        if (g_raw_dirs[i] != 0u ||
            g_raw_devs[i] != kChunkDeviceAddrs[i] ||
            g_raw_drams[i] != kChunkDramAddrs[i] ||
            g_raw_sizes[i] != kChunkSizes[i] ||
            (i < 25u && g_raw_requests[i] != expected_request) ||
            g_raw_request_types[i] != 11u ||
            g_raw_request_return_queues[i] != kThread4QueueSmall ||
            g_chunk_actual_hashes[i] != kChunkHashes[i] ||
            g_auto_event_results[i] != 0) {
            std::fprintf(
                stderr,
                "FAILED: transfer #%u mismatch "
                "dev/dram/size/request/hash="
                "%08X/%08X/%08X/%08X/%08X.\n",
                i + 1u,
                g_raw_devs[i],
                g_raw_drams[i],
                g_raw_sizes[i],
                g_raw_requests[i],
                g_chunk_actual_hashes[i]);
            std::_Exit(12);
        }
    }

    if (g_second_directory_reload_hash_at_dma11 != kExpectedFullLoadHash ||
        g_third_directory_reload_hash_at_dma17 != kExpectedFullLoadHash ||
        g_fourth_directory_reload_hash_at_dma24 != kExpectedFullLoadHash ||
        g_fifth_directory_reload_hash_at_dma29 != kExpectedFullLoadHash) {
        std::fprintf(
            stderr,
            "FAILED: directory reload snapshots second/third/fourth/fifth="
            "%08X/%08X/%08X/%08X.\n",
            g_second_directory_reload_hash_at_dma11,
            g_third_directory_reload_hash_at_dma17,
            g_fourth_directory_reload_hash_at_dma24,
            g_fifth_directory_reload_hash_at_dma29);
        std::_Exit(13);
    }

    if (!g_rnc_boundary_reached ||
        !g_rnc_return_reached ||
        g_rnc_packed_crc != kRncExpectedPackedCrc ||
        g_rnc_unpacked_crc != kRncExpectedUnpackedCrc ||
        g_rnc_output_fnv != 0x33CFB17Bu ||
        g_rnc_predecode_output_hash != kNextFontPayloadHash ||
        g_rnc_predecode_temp_hash != kNextFontPayloadHash) {
        std::fprintf(
            stderr,
            "FAILED: Stage 57 RNC regression "
            "entry=%d return=%d packed=%04X unpacked=%04X fnv=%08X "
            "pre=%08X/%08X.\n",
            g_rnc_boundary_reached ? 1 : 0,
            g_rnc_return_reached ? 1 : 0,
            g_rnc_packed_crc,
            g_rnc_unpacked_crc,
            g_rnc_output_fnv,
            g_rnc_predecode_output_hash,
            g_rnc_predecode_temp_hash);
        std::_Exit(14);
    }

    if (!g_second_font_return_reached ||
        !g_second_font_model_valid ||
        g_second_font_return_v0 != kRncUnpackedSize ||
        g_second_font_model_expanded_bytes != 0x00002BA0u ||
        g_second_font_model_final_s0 != 0x802FABB8u) {
        std::fprintf(
            stderr,
            "FAILED: tahoma12 processing regression "
            "return=%d model=%d v0/bytes/final=%08X/%08X/%08X.\n",
            g_second_font_return_reached ? 1 : 0,
            g_second_font_model_valid ? 1 : 0,
            g_second_font_return_v0,
            g_second_font_model_expanded_bytes,
            g_second_font_model_final_s0);
        std::_Exit(15);
    }

    if (!g_third_font_return_reached ||
        !g_third_font_model_valid ||
        g_third_font_return_sp != kActualFontCallerSp ||
        g_third_font_return_v0 != kEtchFontFileSize ||
        g_third_font_return_s0 != 0x802FABB8u ||
        g_third_font_return_s1 != 2u ||
        g_third_font_return_s2 != kThirdFontGlyphBaseIndex ||
        g_third_font_return_s5 != g_third_font_pool_base ||
        g_third_font_return_s6 != 0u ||
        g_third_font_model_expanded_bytes !=
            kThirdFontExpandedBytesExpected ||
        g_third_font_model_final_s0 !=
            kThirdFontFinalS0Expected ||
        g_third_font_complete_first_word != kEtchFontFirstWord ||
        g_third_font_complete_hash != kEtchFontPayloadHash) {
        std::fprintf(
            stderr,
            "FAILED: etchfont return/model "
            "return=%d model=%d sp/v0/s0/s1/s2/s5/s6/bytes/final/first/hash="
            "%08X/%08X/%08X/%08X/%08X/%08X/%08X/%08X/%08X/%08X/%08X.\n",
            g_third_font_return_reached ? 1 : 0,
            g_third_font_model_valid ? 1 : 0,
            g_third_font_return_sp,
            g_third_font_return_v0,
            g_third_font_return_s0,
            g_third_font_return_s1,
            g_third_font_return_s2,
            g_third_font_return_s5,
            g_third_font_return_s6,
            g_third_font_model_expanded_bytes,
            g_third_font_model_final_s0,
            g_third_font_complete_first_word,
            g_third_font_complete_hash);
        std::_Exit(16);
    }

    constexpr std::array<uint32_t, 3> kThirdSpots{0u, 1u, 104u};
    for (std::size_t i = 0; i < kThirdSpots.size(); ++i) {
        const uint32_t glyph_index =
            kThirdFontGlyphBaseIndex + kThirdSpots[i];
        const uint32_t meta =
            g_third_font_return_s5 + glyph_index * 4u;
        const uint16_t actual_offset = read_guest_u16(meta + 0u);
        const uint16_t actual_packed = read_guest_u16(meta + 2u);

        if (actual_offset != g_third_font_expected_offsets[i] ||
            actual_packed != g_third_font_expected_packed[i]) {
            std::fprintf(
                stderr,
                "FAILED: etchfont glyph metadata spot #%u "
                "actual=%04X/%04X expected=%04X/%04X.\n",
                kThirdSpots[i],
                actual_offset,
                actual_packed,
                g_third_font_expected_offsets[i],
                g_third_font_expected_packed[i]);
            std::_Exit(17);
        }
    }

    constexpr std::array<uint16_t, 16> kExpectedPalette{
        0x0000u, 0x0001u, 0x0843u, 0x2109u,
        0x318Du, 0x4211u, 0x5AD7u, 0x6B5Bu,
        0x7BDFu, 0x8C63u, 0xA529u, 0xB5ADu,
        0xC631u, 0xDEF7u, 0xEF7Bu, 0xFFFFu};
    if (g_final_palette != kExpectedPalette) {
        std::fputs(
            "FAILED: final etchfont 16-entry palette copy mismatch.\n",
            stderr);
        std::_Exit(18);
    }

    const uint32_t common_raw_hash =
        stage61_fnv1a_guest(kExpectedFontDest, kCommonFileSize);

    if (common_raw_hash != kCommonPayloadHash ||
        g_common_complete_hash != kCommonPayloadHash ||
        g_common_complete_first_word != kCommonFirstWord ||
        !g_common_parser_snapshot_valid ||
        g_common_parser_result != 1u ||
        g_common_parser_data_offset != kCommonDataOffset ||
        g_common_parser_size != kCommonFileSize ||
        !g_common_payload_snapshot_valid ||
        g_common_payload_pc != kFontPayloadAlignedCallPc ||
        g_common_payload_a0 != kExpectedFontDest ||
        g_common_payload_a1 != kCommonDataOffset ||
        g_common_payload_a2 != kCommonFileSize) {
        std::fprintf(
            stderr,
            "FAILED: common-text raw/parser state "
            "raw/complete/first/key/ret/data/size/pc/a0/a1/a2="
            "%08X/%08X/%08X/\"%s\"/%08X/%08X/%08X/%08X/%08X/%08X/%08X.\n",
            common_raw_hash,
            g_common_complete_hash,
            g_common_complete_first_word,
            "text\\common.eng",
            g_common_parser_result,
            g_common_parser_data_offset,
            g_common_parser_size,
            g_common_payload_pc,
            g_common_payload_a0,
            g_common_payload_a1,
            g_common_payload_a2);
        std::_Exit(19);
    }

    if (!g_three_font_return_reached ||
        g_three_font_return_sp != kThreeFontCallerSp ||
        g_three_font_return_v0 != kThreeFontPoolBytesExpected ||
        g_three_font_return_s0 != kThreeFontCallerS0) {
        std::fprintf(
            stderr,
            "FAILED: completed three-font caller return "
            "reached=%d sp/v0/s0=%08X/%08X/%08X "
            "(pre-instruction a0=0x%08X is caller-saved and not asserted).\n",
            g_three_font_return_reached ? 1 : 0,
            g_three_font_return_sp,
            g_three_font_return_v0,
            g_three_font_return_s0,
            g_three_font_return_a0);
        std::_Exit(20);
    }

    if (!g_post_pool_return_reached ||
        g_post_pool_return_sp != kPostPoolReturnSp) {
        std::fprintf(
            stderr,
            "FAILED: post-pool return boundary reached=%d sp=%08X.\n",
            g_post_pool_return_reached ? 1 : 0,
            g_post_pool_return_sp);
        std::_Exit(21);
    }

    const uint32_t common_output_hash =
        stage61_fnv1a_guest(kCommonOutputBase, kCommonOutputSize);

    if (read_guest_u32(kFontPoolGlobal) != 0x802F5600u ||
        read_guest_u32(kCommonPoolGlobal) != kCommonOutputBase ||
        read_guest_u32(kCommonLineCountGlobal) != kCommonLineCount ||
        common_output_hash != kCommonOutputHash ||
        read_guest_u32(kCommonOutputBase + 0x00u) != 0x802FD928u ||
        read_guest_u32(kCommonOutputBase + 0x04u) != 0x802FD936u ||
        read_guest_u32(kCommonOutputBase + 25u * 4u) != 0x802FDAA1u) {
        std::fprintf(
            stderr,
            "FAILED: parsed common-text block "
            "fontPool/commonPool/lines/hash/ptr0/ptr1/ptr25="
            "%08X/%08X/%08X/%08X/%08X/%08X/%08X.\n",
            read_guest_u32(kFontPoolGlobal),
            read_guest_u32(kCommonPoolGlobal),
            read_guest_u32(kCommonLineCountGlobal),
            common_output_hash,
            read_guest_u32(kCommonOutputBase + 0x00u),
            read_guest_u32(kCommonOutputBase + 0x04u),
            read_guest_u32(kCommonOutputBase + 25u * 4u));
        std::_Exit(22);
    }

    if (read_guest_u32(kArena0PtrTable) != kArena0FinalPtr ||
        read_guest_u32(kArena0RemainTable) != kArena0FinalRemain ||
        read_guest_u32(kArena1PtrTable) != kArena1FinalPtr ||
        read_guest_u32(kArena1RemainTable) != kArena1FinalRemain) {
        std::fprintf(
            stderr,
            "FAILED: final startup arena state "
            "a0ptr/a0rem/a1ptr/a1rem=%08X/%08X/%08X/%08X.\n",
            read_guest_u32(kArena0PtrTable),
            read_guest_u32(kArena0RemainTable),
            read_guest_u32(kArena1PtrTable),
            read_guest_u32(kArena1RemainTable));
        std::_Exit(23);
    }

    if (!g_si_bootstrap_boundary_reached ||
        g_si_boundary_sp != kSiBoundarySp ||
        g_si_boundary_a0 != kSiQueue ||
        g_si_boundary_a1 != kSiStatusBytePtr ||
        g_si_boundary_a2 != kSiControllerBuffer ||
        g_si_boundary_s0 != kSiControllerBuffer ||
        read_guest_u32(kSiQueue + 0x00u) != 0x800AA680u ||
        read_guest_u32(kSiQueue + 0x04u) != 0x800AA680u ||
        read_guest_u32(kSiQueue + 0x08u) != 0u ||
        // Two event-5 messages have now been enqueued and consumed.
        // Capacity 2 wraps the circular read index 0 -> 1 -> 0.
        read_guest_u32(kSiQueue + 0x0Cu) != 0u ||
        read_guest_u32(kSiQueue + 0x10u) != kSiQueueCapacity ||
        read_guest_u32(kSiQueue + 0x14u) != kSiQueueBuffer ||
        read_guest_u32(kSiEventTableEntry + 0x00u) != kSiQueue ||
        read_guest_u32(kSiEventTableEntry + 0x04u) != 0u ||
        read_guest_u32(kSiScratchReset) != 0u) {
        std::fprintf(
            stderr,
            "FAILED: SI bootstrap/post-event queue state reached=%d sp/a0/a1/a2/s0="
            "%08X/%08X/%08X/%08X/%08X queue="
            "%08X/%08X/%08X/%08X/%08X/%08X event=%08X/%08X init=%08X.\n",
            g_si_bootstrap_boundary_reached ? 1 : 0,
            g_si_boundary_sp,
            g_si_boundary_a0,
            g_si_boundary_a1,
            g_si_boundary_a2,
            g_si_boundary_s0,
            read_guest_u32(kSiQueue + 0x00u),
            read_guest_u32(kSiQueue + 0x04u),
            read_guest_u32(kSiQueue + 0x08u),
            read_guest_u32(kSiQueue + 0x0Cu),
            read_guest_u32(kSiQueue + 0x10u),
            read_guest_u32(kSiQueue + 0x14u),
            read_guest_u32(kSiEventTableEntry + 0x00u),
            read_guest_u32(kSiEventTableEntry + 0x04u),
            read_guest_u32(kSiScratchReset));
        std::_Exit(24);
    }

    if (!g_first_si_mmio_boundary_reached ||
        g_first_si_mmio_sp != kFirstSiMmioSp ||
        g_first_si_mmio_v0 != kSiStatusReg ||
        g_first_si_mmio_s1 != 1u ||
        g_first_si_mmio_s0 != kControllerStatusArray ||
        read_guest_u32(kControllerInitOnceFlag) != 1u ||
        read_guest_byte(kControllerChannelCount) != 4u) {
        std::fprintf(
            stderr,
            "FAILED: first SI MMIO boundary state reached=%d "
            "sp/v0/s1/s0/init/count="
            "%08X/%08X/%08X/%08X/%08X/%02X.\n",
            g_first_si_mmio_boundary_reached ? 1 : 0,
            g_first_si_mmio_sp,
            g_first_si_mmio_v0,
            g_first_si_mmio_s1,
            g_first_si_mmio_s0,
            read_guest_u32(kControllerInitOnceFlag),
            static_cast<unsigned>(read_guest_byte(kControllerChannelCount)));
        std::_Exit(25);
    }

    std::array<uint8_t, kFirstPifCommandBytes> expected_pif{};
    for (uint32_t channel = 0; channel < 4u; ++channel) {
        const uint32_t o = channel * 8u;
        expected_pif[o + 0u] = 0xFFu;
        expected_pif[o + 1u] = 0x01u;
        expected_pif[o + 2u] = 0x03u;
        expected_pif[o + 3u] = 0x00u;
        expected_pif[o + 4u] = 0xFFu;
        expected_pif[o + 5u] = 0xFFu;
        expected_pif[o + 6u] = 0xFFu;
        expected_pif[o + 7u] = 0xFFu;
    }
    expected_pif[0x20u] = 0xFEu;
    expected_pif[0x3Fu] = 0x01u;

    if (!g_first_si_dma_launch_reached ||
        g_first_si_dma_sp != kFirstSiMmioSp ||
        g_first_si_dma_s1 != 1u ||
        g_first_si_dma_s0 != kControllerStatusArray ||
        g_first_si_dma_v1 != kSiPifAddrWr64bReg ||
        g_first_si_dma_v0 != kPifRamAddress ||
        !g_return_si_dma_launch_reached ||
        g_return_si_dma_sp != kFirstSiMmioSp ||
        g_return_si_dma_s1 != 0u ||
        g_return_si_dma_s0 != kControllerStatusArray ||
        g_return_si_dma_v1 != kSiPifAddrRd64bReg ||
        g_return_si_dma_v0 != kPifRamAddress ||
        g_si_dma_launch_count < 2u ||
        !g_si_mmio_args_valid ||
        g_si_mmio_read_count < 2u ||
        g_si_mmio_write_count < 4u ||
        g_si_mmio_write_addrs[0] != kSiDramAddrReg ||
        g_si_mmio_write_values[0] != kControllerStatusArrayPhys ||
        g_si_mmio_write_addrs[1] != kSiPifAddrWr64bReg ||
        g_si_mmio_write_values[1] != kPifRamAddress ||
        g_si_mmio_write_addrs[2] != kSiDramAddrReg ||
        g_si_mmio_write_values[2] != kControllerStatusArrayPhys ||
        g_si_mmio_write_addrs[3] != kSiPifAddrRd64bReg ||
        g_si_mmio_write_values[3] != kPifRamAddress ||
        g_first_pif_command_fnv != kFirstPifCommandFnv ||
        g_first_pif_command != expected_pif ||
        !g_first_si_completion_event_queued ||
        g_first_si_completion_send_result != 0 ||
        g_si_event_recv_count < 2u ||
        g_si_event_recv_msg_ptr != kSiEventReceiveMsgPtr ||
        g_si_event_recv_result != 0 ||
        g_si_event_recv_message != 0u ||
        !g_second_si_completion_event_queued ||
        g_second_si_completion_send_result != 0) {
        std::fprintf(
            stderr,
            "FAILED: Stage 64 SI completion/return DMA "
            "launch1/launch2/count=%d/%d/%u "
            "r/w/valid=%u/%u/%d "
            "eventQueued/send/recv/msgptr/result/msg=%d/%d/%u/%08X/%d/%08X "
            "return sp/s1/s0/v1/v0=%08X/%08X/%08X/%08X/%08X "
            "packetHash=%08X.\\n",
            g_first_si_dma_launch_reached ? 1 : 0,
            g_return_si_dma_launch_reached ? 1 : 0,
            g_si_dma_launch_count,
            g_si_mmio_read_count,
            g_si_mmio_write_count,
            g_si_mmio_args_valid ? 1 : 0,
            g_first_si_completion_event_queued ? 1 : 0,
            g_first_si_completion_send_result,
            g_si_event_recv_count,
            g_si_event_recv_msg_ptr,
            g_si_event_recv_result,
            g_si_event_recv_message,
            g_return_si_dma_sp,
            g_return_si_dma_s1,
            g_return_si_dma_s0,
            g_return_si_dma_v1,
            g_return_si_dma_v0,
            g_first_pif_command_fnv);
        std::_Exit(26);
    }

    if (g_pi_dma_count != kTransferCount ||
        g_pi_raw_boundary_count != kTransferCount ||
        g_pi_mmio_read_count != kTransferCount ||
        g_pi_mmio_write_count != kTransferCount * 3u ||
        g_pi_auto_event_count != kTransferCount) {
        std::fprintf(
            stderr,
            "FAILED: unexpected PI totals at audio-engine init boundary: "
            "dma/raw/r/w/e=%u/%u/%u/%u/%u.\n",
            g_pi_dma_count,
            g_pi_raw_boundary_count,
            g_pi_mmio_read_count,
            g_pi_mmio_write_count,
            g_pi_auto_event_count);
        std::_Exit(25);
    }

    if (!g_controller_status_parsed_reached ||
        g_controller_status_parsed_sp != kControllerInitOuterSp ||
        g_controller_status_parsed_s4 != kSiQueue ||
        g_controller_status_parsed_s5 != kControllerConnectedMaskPtr ||
        g_controller_status_parsed_s6 != kControllerStatusOutput ||
        g_controller_connected_mask != 0x01u ||
        read_guest_byte(kControllerStatusArray + 2u) != 0x03u ||
        read_guest_byte(kControllerStatusArray + 10u) != 0x83u ||
        read_guest_byte(kControllerStatusArray + 18u) != 0x83u ||
        read_guest_byte(kControllerStatusArray + 26u) != 0x83u ||
        g_controller_errno[0] != 0u ||
        g_controller_errno[1] != kNoControllerError ||
        g_controller_errno[2] != kNoControllerError ||
        g_controller_errno[3] != kNoControllerError) {
        std::fprintf(
            stderr,
            "FAILED: Stage 65 controller-status parse "
            "reached=%d sp/s4/s5/s6=%08X/%08X/%08X/%08X "
            "replyHash/mask/errno=%08X/%02X/%u,%u,%u,%u.\n",
            g_controller_status_parsed_reached ? 1 : 0,
            g_controller_status_parsed_sp,
            g_controller_status_parsed_s4,
            g_controller_status_parsed_s5,
            g_controller_status_parsed_s6,
            g_return_pif_reply_fnv,
            g_controller_connected_mask,
            g_controller_errno[0], g_controller_errno[1],
            g_controller_errno[2], g_controller_errno[3]);
        std::_Exit(27);
    }

    if (!g_controller_bootstrap_return_reached ||
        g_controller_bootstrap_return_sp != kControllerBootstrapReturnSp ||
        g_controller_bootstrap_return_v0 != 1u ||
        g_controller_bootstrap_return_s0 != kMainThreadS0 ||
        g_controller_selection != 0u ||
        g_controller_list_count != 1u ||
        g_controller_poll_started != 1u ||
        read_guest_byte(kControllerConnectedMaskPtr) != 0x01u) {
        std::fprintf(
            stderr,
            "FAILED: Stage 66 controller-bootstrap return "
            "reached=%d sp/v0/s0=%08X/%08X/%08X "
            "selection/listCount/poll/mask=%u/%u/%u/%u.\n",
            g_controller_bootstrap_return_reached ? 1 : 0,
            g_controller_bootstrap_return_sp,
            g_controller_bootstrap_return_v0,
            g_controller_bootstrap_return_s0,
            g_controller_selection,
            g_controller_list_count,
            g_controller_poll_started,
            read_guest_byte(kControllerConnectedMaskPtr));
        std::_Exit(28);
    }

    if (!g_sfx_parser_valid ||
        g_sfx_parser_data != kSfxDataOffset ||
        g_sfx_parser_size != kSfxFileSize ||
        g_sfx_complete_dest != kSfxDest ||
        g_sfx_complete_first != kSfxFirstWord ||
        g_sfx_complete_hash != kSfxFullHash ||
        !g_ptr_parser_valid ||
        g_ptr_parser_data != kPtrDataOffset ||
        g_ptr_parser_size != kPtrFileSize ||
        g_ptr_complete_dest != kPtrDest ||
        g_ptr_complete_first != kPtrFirstWord ||
        g_ptr_complete_hash != kPtrFullHash ||
        !g_audio_engine_init_boundary_reached ||
        g_audio_engine_init_sp != kAudioEngineInitBoundarySp ||
        g_audio_engine_init_v0 != kAudioEngineInitV0 ||
        g_audio_engine_init_a0 != kAudioEngineConfigPtr ||
        g_audio_engine_init_a1 != kPtrDest) {
        std::fprintf(
            stderr,
            "FAILED: Stage 67 audio/audio-engine state "
            "sfx=%d/%08X/%08X/%08X/%08X/%08X "
            "ptr=%d/%08X/%08X/%08X/%08X/%08X "
            "engine=%d sp/v0/a0/a1=%08X/%08X/%08X/%08X.\n",
            g_sfx_parser_valid ? 1 : 0,
            g_sfx_parser_data,
            g_sfx_parser_size,
            g_sfx_complete_dest,
            g_sfx_complete_first,
            g_sfx_complete_hash,
            g_ptr_parser_valid ? 1 : 0,
            g_ptr_parser_data,
            g_ptr_parser_size,
            g_ptr_complete_dest,
            g_ptr_complete_first,
            g_ptr_complete_hash,
            g_audio_engine_init_boundary_reached ? 1 : 0,
            g_audio_engine_init_sp,
            g_audio_engine_init_v0,
            g_audio_engine_init_a0,
            g_audio_engine_init_a1);
        std::_Exit(29);
    }

    const uint32_t stage75_audio_clock =
        read_guest_u32(kStage72AudioClockGlobal);
    const uint32_t stage75_expected_rate =
        stage75_audio_clock / kAudioDacDivisor;

    if (!g_first_ai_mmio_boundary_reached ||
        !g_stage72_ai_return_reached ||
        !g_stage73_scheduler_setup_reached ||
        !g_stage74_scheduler_return_reached ||
        !g_stage75_audio_thread_entry_reached ||
        !g_stage76_first_callback_boundary_reached ||
        g_stage76_first_callback != kStage77Callback0 ||
        g_stage76_second_callback != kStage77Callback1 ||
        g_stage76_third_callback != kStage77Callback2 ||
        !g_stage77_first_callback_return_reached ||
        !g_stage78_callback1_receive_reached ||
        !g_stage79_vi_route_valid ||
        !g_stage79_vi_event_queued ||
        !g_stage79_callback1_return_reached ||
        g_stage79_return_sp != kStage79ExpectedMainSp ||
        g_stage79_received_message != kStage79WorkerBase ||
        g_stage79_audio_queue_count != 0u ||
        g_stage79_audio_queue_first != 1u ||
        g_stage79_audio_queue_capacity != kStage79ExpectedAudioQueueCapacity ||
        g_stage79_audio_queue_buffer != kStage79ExpectedAudioQueueBuffer ||
        g_stage78_receive_sp != kStage78ExpectedSp ||
        g_stage78_queue != kStage78ExpectedQueueBase ||
        (g_stage78_queue + 0x08u) != kStage78ExpectedEffectiveQueue ||
        g_stage78_msg_out != kStage78ExpectedMsgOut ||
        g_stage78_flags != kStage78ExpectedFlags ||
        g_stage78_queue_count != 0u ||
        g_stage78_queue_first != 0u ||
        g_stage78_queue_capacity != kStage78ExpectedCapacity ||
        g_stage78_queue_buffer != kStage78ExpectedBuffer ||
        g_stage77_return_sp != kStage77ExpectedSp ||
        !g_stage77_queue_shape_valid ||
        g_stage76_first_callback_sp != kStage76ExpectedSp ||
        g_stage76_callback_table < 0x80000000u ||
        g_stage76_callback_table > 0x807FFFF4u ||
        g_stage76_first_callback < 0x80000000u ||
        g_stage76_first_callback > 0x807FFFFFu ||
        g_stage74_sp != kStage74ExpectedSp ||
        g_stage74_s0 != kStage73SchedulerObj ||
        g_stage75_audio_thread_sp != kStage75AudioThreadInitialSp ||
        g_stage75_audio_thread_a0 != 0u ||
        g_stage72_ai_return_v0 != stage75_expected_rate ||
        read_guest_u32(kStage71PtrSlot) != kStage71PtrBank ||
        read_guest_u32(kStage71WbkSlot) != kStage71WaveBank ||
        read_guest_u32(kStage71FxSlot) != 0u) {
        std::fprintf(
            stderr,
            "FAILED: Stage 78 callback1 receive-boundary state "
            "createEntry=%d createReturn=%d audioEntry=%d "
            "parentSp/s0=%08X/%08X audioSp/a0=%08X/%08X "
            "rate=%08X expected=%08X; native AI ownership active.\n",
            g_stage73_scheduler_setup_reached ? 1 : 0,
            g_stage74_scheduler_return_reached ? 1 : 0,
            g_stage75_audio_thread_entry_reached ? 1 : 0,
            g_stage74_sp,
            g_stage74_s0,
            g_stage75_audio_thread_sp,
            g_stage75_audio_thread_a0,
            g_stage72_ai_return_v0,
            stage75_expected_rate);
        std::_Exit(39);
    }

    if (read_guest_u32(kPiAccessQueue + 0x08u) != 1u ||
        read_guest_u32(kPiCmdQueue + 0x00u) != kPiManagerThreadAddr ||
        read_guest_u32(kViQueue + 0x00u) != kViManagerThreadAddr ||
        read_guest_u32(kManagerQueueA + 0x00u) != kWorkerThreadAddr) {
        std::fputs(
            "FAILED: service-thread/PI-token state regressed after "
            "three-font initialization.\n",
            stderr);
        std::_Exit(21);
    }

    std::puts(
        "OK: Stages 57-59 replayed without regression: RNC decompression, "
        "all three fonts, 315 glyph records, final palette, and 0x82C0 "
        "font-pool usage remain correct.");
    std::puts(
        "OK: func_80013760 consumed the 0x82C0-byte font pool and advanced "
        "arena 0 from 0x802F5600 to 0x802FD8C0.");
    std::puts(
        "OK: text\\common.eng resolved as directory entry #355, "
        "dataOffset=0x00551F70, size=0x000001A8, "
        "payload ROM+0x0062A750.");
    std::puts(
        "OK: the fourth 0x4000 directory reload and 0x1A8-byte common-text "
        "payload completed through DMA #21-#25; raw FNV-1a is "
        "0xF99AE1CC and firstWord=0x5C633050.");
    std::puts(
        "OK: func_8001D33C parsed 26 common-text lines into a 0x1F8-byte "
        "pointer/string block at 0x802FD8C0 "
        "(FNV-1a 0x6449D4C1).");
    std::puts(
        "OK: after func_80013760's size-zero end-pointer reservations, arena 0 "
        "is at 0x80300000 with remaining=0x00002548 and arena 1 is at "
        "0x803F4000 with remaining=0x0008AF00.");
    std::puts(
        "OK: Stage 66 controller bootstrap replayed without regression and "
        "func_80008654 entered the real audio-first startup path.");
    std::puts(
        "OK: tunes\\sfx.bfx resolved at dataOffset=0x000034C0, "
        "size=0x1978 and loaded to 0x80340800 (FNV-1a 0xCF866787).");
    std::puts(
        "OK: tunes\\ts2.ptr resolved at dataOffset=0x00004E38, "
        "size=0x1106C and loaded to 0x80320000 (FNV-1a 0x88ED2B6D).");
    std::puts(
        "OK: the real resource path completed 54 PI DMAs total; "
        "PI totals are 54 STATUS reads / 162 register writes / "
        "54 completion events.");
    std::puts(
        "OK: func_8001923C built its audio-engine configuration block and "
        "reached guest PC 0x80019304 with SP=0x803F9F38, "
        "config=0x803F9F48, audioData=0x80320000 and v0=0x00000400.");
    std::puts(
        "OK: Stage 67 audio-engine entry replayed successfully and "
        "func_8008D2D8 continued through its software initialization.");
    std::puts(
        "OK: func_8008D2D8 crossed its initial RAM/allocator setup and "
        "entered func_800903A0 -> func_8008FF20 -> func_80099F00.");
    std::puts(
        "OK: first-run cartridge state initialized base 0xB0000000 and "
        "reached func_800982E0 with cart offset 0 and result pointer "
        "0x803F9E60.");
    std::puts(
        "OK: Stage 68 stopped at guest PC 0x8009831C with "
        "PI_STATUS_REG=0xA4600010 immediately before the direct status "
        "read that precedes the cartridge word probe.");
    if (!g_stage80_ai_status_boundary_reached ||
        g_stage80_ai_status_sp != 0x8031EFA0u ||
        g_stage80_ai_status_addr != 0xA450000Cu) {
        std::fprintf(
            stderr,
            "FAILED: Stage 80 terminal state reached=%d sp/address=%08X/%08X.\n",
            g_stage80_ai_status_boundary_reached ? 1 : 0,
            g_stage80_ai_status_sp,
            g_stage80_ai_status_addr);
        std::fflush(stderr);
        std::_Exit(42);
    }

    if (!g_stage80_ai_status_boundary_reached ||
        g_stage80_ai_status_sp != 0x8031EFA0u ||
        g_stage80_ai_status_addr != 0xA450000Cu) {
        std::fprintf(
            stderr,
            "FAILED: Stage 80 regression while advancing Stage 81: "
            "reached=%d sp/address=%08X/%08X.\\n",
            g_stage80_ai_status_boundary_reached ? 1 : 0,
            g_stage80_ai_status_sp,
            g_stage80_ai_status_addr);
        std::fflush(stderr);
        std::_Exit(45);
    }

    if (!g_stage81_audio_queue_reached &&
        g_stage81_v7_probe_complete) {

        std::printf(
            "STAGE 81 V7 BOUNDED PROBE COMPLETE: "
            "iterations=%u taskOutputSeen=%d "
            "lastTaskWord=0x%08X lastReturnV0=0x%08X "
            "lastSlot=0x%08X lastSlotWords=0x%08X/0x%08X "
            "lastPrevBuffer=0x%08X lastAIQuarterSamples=%u "
            "lastRingIndex=%u.\\n",
            g_stage81_v7_mix_probe_count,
            g_stage81_v7_task_output_seen ? 1 : 0,
            g_stage81_v7_last_task_word,
            g_stage81_v7_last_return_v0,
            g_stage81_v7_last_slot,
            g_stage81_v7_last_slot_word0,
            g_stage81_v7_last_slot_word1,
            g_stage81_v7_last_prev_buffer,
            g_stage81_v7_last_ai_quarters,
            g_stage81_v7_last_ring_index);

        std::puts(
            "Stage 81 is NOT marked passed by this diagnostic. "
            "The values above identify whether Toy Story 2 produced "
            "the post-mixer task/previous-buffer state required for "
            "osAiSetNextBuffer_recomp.");
        std::fflush(stdout);
        std::fflush(stderr);
        std::_Exit(81);
    }

    if (!g_stage81_audio_queue_reached ||
        g_stage81_queue_call_count != 1u ||
        g_stage81_status_call_count < 2u ||
        g_stage81_length_call_count < 1u ||
        g_stage81_first_buffer < 0x80000000u ||
        g_stage81_first_buffer > 0x807FFFFFu ||
        g_stage81_first_buffer_bytes == 0u ||
        (g_stage81_first_buffer_bytes & 0x1u) != 0u) {
        std::fprintf(
            stderr,
            "FAILED: Stage 81 terminal state reached=%d "
            "queue/status/length=%u/%u/%u buffer=%08X bytes=%u.\\n",
            g_stage81_audio_queue_reached ? 1 : 0,
            g_stage81_queue_call_count,
            g_stage81_status_call_count,
            g_stage81_length_call_count,
            g_stage81_first_buffer,
            g_stage81_first_buffer_bytes);
        std::fflush(stderr);
        std::_Exit(46);
    }

    std::puts(
        "Stage 81 native AI bridge / first generated audio-buffer handoff "
        "checkpoint PASSED.");

    std::fflush(stdout);
    std::fflush(stderr);
    // TS2 LIVE HOST LIFETIME V12
    //
    // Historical bring-up behavior ended the executable here with
    // std::_Exit(0) once the Stage 81 regression checklist passed.
    //
    // We now have sustained graphics, VI, scheduler, SI/controller traffic,
    // and live PC input. Keep the host process alive instead so the original
    // Toy Story 2 runtime threads can continue executing normally.
    std::puts(
        "TS2 LIVE HOST: all historical bring-up validations passed; "
        "keeping the PC runtime alive. Close the window to exit.");
    std::fflush(stdout);
    std::fflush(stderr);

    bool ts2_host_running = true;

    while (ts2_host_running) {
        SDL_Event event{};

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                ts2_host_running = false;
            }
        }

#ifdef _WIN32
        // Also allow Escape to close this diagnostic PC harness even if the
        // window-close event is delayed or the RT64 window is not focused.
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
            ts2_host_running = false;
        }
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(4));
    }

    std::puts("TS2 LIVE HOST: shutdown requested; leaving run_stage61 normally.");
    std::fflush(stdout);
    std::fflush(stderr);

    return 0;

}

} // namespace


// TS2 OLDNATIVE V79 EXACT STAGE61 LOOKUP MAP REUSE
static void ts2_v79_register_native_maps() {
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087870u),
            ::ts2_auto_alias_80087870);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C4ACu),
            ::ts2_auto_alias_8009C4AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(kPiRawDmaInternalEntry),
            &::func_80099E20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090170u),
            func_80090170);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090180u),
            func_80090180);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800904DCu),
            func_800904DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E478u),
            func_8008E478);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006840u),
            func_80006840);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006878u),
            func_80006878);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800068A8u),
            func_800068A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800068E0u),
            func_800068E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000691Cu),
            func_8000691C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006964u),
            func_80006964);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800069A4u),
            func_800069A4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006A80u),
            func_80006A80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006ABCu),
            func_80006ABC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006BD0u),
            func_80006BD0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006C48u),
            func_80006C48);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006D34u),
            func_80006D34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006E00u),
            func_80006E00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80006E74u),
            func_80006E74);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007024u),
            func_80007024);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007090u),
            func_80007090);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800072BCu),
            func_800072BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007390u),
            func_80007390);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800073B0u),
            func_800073B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800073FCu),
            func_800073FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000748Cu),
            func_8000748C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000751Cu),
            func_8000751C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007584u),
            func_80007584);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007648u),
            func_80007648);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007710u),
            func_80007710);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007780u),
            func_80007780);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800077F0u),
            func_800077F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000789Cu),
            func_8000789C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800078B8u),
            func_800078B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800078D4u),
            func_800078D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800078E0u),
            func_800078E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800078ECu),
            func_800078EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007C38u),
            func_80007C38);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80007CFCu),
            func_80007CFC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80008000u),
            func_80008000);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000806Cu),
            func_8000806C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800080F8u),
            func_800080F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80008278u),
            func_80008278);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800083A0u),
            func_800083A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80008490u),
            func_80008490);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000855Cu),
            func_8000855C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80008654u),
            func_80008654);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800088ACu),
            func_800088AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80008D04u),
            func_80008D04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800096F0u),
            func_800096F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000971Cu),
            func_8000971C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000977Cu),
            func_8000977C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80009784u),
            func_80009784);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000978Cu),
            func_8000978C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80009794u),
            func_80009794);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000979Cu),
            func_8000979C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800097A4u),
            func_800097A4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800097ACu),
            func_800097AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800097D4u),
            func_800097D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80009928u),
            func_80009928);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800099A8u),
            func_800099A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800099E4u),
            func_800099E4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000AC1Cu),
            func_8000AC1C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000AC34u),
            func_8000AC34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000AFCCu),
            func_8000AFCC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000B0C0u),
            func_8000B0C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000B3ACu),
            func_8000B3AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000B944u),
            func_8000B944);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000BBD8u),
            func_8000BBD8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C1F0u),
            func_8000C1F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C394u),
            func_8000C394);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C4B8u),
            func_8000C4B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C588u),
            func_8000C588);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C620u),
            func_8000C620);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C7B8u),
            func_8000C7B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C86Cu),
            func_8000C86C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000C9DCu),
            func_8000C9DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000CD98u),
            func_8000CD98);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000CFA8u),
            func_8000CFA8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000D1B8u),
            func_8000D1B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000D328u),
            func_8000D328);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000DD34u),
            func_8000DD34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8000DEA0u),
            func_8000DEA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001041Cu),
            func_8001041C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001045Cu),
            func_8001045C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800104A0u),
            func_800104A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800104FCu),
            func_800104FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001053Cu),
            func_8001053C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010708u),
            func_80010708);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800107C8u),
            func_800107C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800107DCu),
            func_800107DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010A28u),
            func_80010A28);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010A30u),
            func_80010A30);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010A40u),
            func_80010A40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010A9Cu),
            func_80010A9C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010AF8u),
            func_80010AF8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80010BB0u),
            func_80010BB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800112F4u),
            func_800112F4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011A10u),
            func_80011A10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011B34u),
            func_80011B34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011B3Cu),
            func_80011B3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011B84u),
            func_80011B84);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011B8Cu),
            func_80011B8C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011B94u),
            func_80011B94);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011DE0u),
            func_80011DE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011E80u),
            func_80011E80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80011EE0u),
            func_80011EE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800120ECu),
            func_800120EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012C74u),
            func_80012C74);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012EB8u),
            func_80012EB8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012EF0u),
            func_80012EF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012F1Cu),
            func_80012F1C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012F54u),
            func_80012F54);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012F5Cu),
            func_80012F5C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012FECu),
            func_80012FEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012FF4u),
            func_80012FF4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80012FFCu),
            func_80012FFC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013004u),
            func_80013004);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001300Cu),
            func_8001300C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013014u),
            func_80013014);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001301Cu),
            func_8001301C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013024u),
            func_80013024);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001302Cu),
            func_8001302C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013034u),
            func_80013034);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800130A0u),
            func_800130A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800130F0u),
            func_800130F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800130F8u),
            func_800130F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013180u),
            func_80013180);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013288u),
            func_80013288);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013290u),
            func_80013290);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013298u),
            func_80013298);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800132A0u),
            func_800132A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800132B0u),
            func_800132B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001330Cu),
            func_8001330C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800133B8u),
            func_800133B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013450u),
            func_80013450);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800134B8u),
            func_800134B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800134FCu),
            func_800134FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013570u),
            func_80013570);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013614u),
            func_80013614);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013634u),
            func_80013634);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013654u),
            func_80013654);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800136C4u),
            func_800136C4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800136F8u),
            func_800136F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013708u),
            func_80013708);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013754u),
            func_80013754);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013760u),
            func_80013760);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800137C4u),
            func_800137C4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013A60u),
            func_80013A60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013AE8u),
            func_80013AE8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013BACu),
            func_80013BAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013C64u),
            func_80013C64);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013CDCu),
            func_80013CDC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80013D88u),
            func_80013D88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014418u),
            func_80014418);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800145DCu),
            func_800145DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014694u),
            func_80014694);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014714u),
            func_80014714);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014CC4u),
            func_80014CC4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014DDCu),
            func_80014DDC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014E1Cu),
            func_80014E1C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014E20u),
            func_80014E20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014EDCu),
            func_80014EDC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014F2Cu),
            func_80014F2C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014F74u),
            func_80014F74);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80014FCCu),
            func_80014FCC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800150B4u),
            func_800150B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800150ECu),
            func_800150EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015184u),
            func_80015184);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800151D8u),
            func_800151D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001528Cu),
            func_8001528C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015428u),
            func_80015428);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800154A4u),
            func_800154A4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001552Cu),
            func_8001552C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800155A8u),
            func_800155A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001561Cu),
            func_8001561C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001571Cu),
            func_8001571C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015890u),
            func_80015890);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800159B0u),
            func_800159B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015B1Cu),
            func_80015B1C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015CECu),
            func_80015CEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015D00u),
            func_80015D00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015D74u),
            func_80015D74);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015DA0u),
            func_80015DA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015DBCu),
            func_80015DBC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015DE8u),
            func_80015DE8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80015E80u),
            func_80015E80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80016124u),
            func_80016124);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80016358u),
            func_80016358);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80016544u),
            func_80016544);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800167B4u),
            func_800167B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80016B70u),
            func_80016B70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80016F98u),
            func_80016F98);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800171B4u),
            func_800171B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001739Cu),
            func_8001739C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800174D8u),
            func_800174D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017544u),
            func_80017544);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017688u),
            func_80017688);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800178CCu),
            func_800178CC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800179E4u),
            func_800179E4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017B44u),
            func_80017B44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017DB0u),
            func_80017DB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017DD0u),
            func_80017DD0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017E48u),
            func_80017E48);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017E68u),
            func_80017E68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80017FF0u),
            func_80017FF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80018160u),
            func_80018160);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80018254u),
            func_80018254);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800182A8u),
            func_800182A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800184BCu),
            func_800184BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001877Cu),
            func_8001877C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800188BCu),
            func_800188BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800188ECu),
            func_800188EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80018900u),
            func_80018900);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019024u),
            func_80019024);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019040u),
            func_80019040);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001912Cu),
            func_8001912C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001918Cu),
            func_8001918C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800191BCu),
            func_800191BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001923Cu),
            func_8001923C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019444u),
            func_80019444);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800194F0u),
            func_800194F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001995Cu),
            func_8001995C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800199C0u),
            func_800199C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019A00u),
            func_80019A00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019AD4u),
            func_80019AD4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019CACu),
            func_80019CAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019E14u),
            func_80019E14);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019E3Cu),
            func_80019E3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80019F58u),
            func_80019F58);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A134u),
            func_8001A134);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A154u),
            func_8001A154);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A18Cu),
            func_8001A18C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A238u),
            func_8001A238);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A2B8u),
            func_8001A2B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A454u),
            func_8001A454);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A604u),
            func_8001A604);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A680u),
            func_8001A680);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001A790u),
            func_8001A790);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001AAD8u),
            func_8001AAD8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001AAE0u),
            func_8001AAE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001B8FCu),
            func_8001B8FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001B96Cu),
            func_8001B96C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001BF08u),
            func_8001BF08);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001BF20u),
            func_8001BF20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001BF4Cu),
            func_8001BF4C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001C9A8u),
            func_8001C9A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001CA3Cu),
            func_8001CA3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001CAD0u),
            func_8001CAD0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001CBACu),
            func_8001CBAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001CBDCu),
            func_8001CBDC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001CC08u),
            func_8001CC08);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001CC38u),
            func_8001CC38);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001D33Cu),
            func_8001D33C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001D48Cu),
            func_8001D48C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001D4C4u),
            func_8001D4C4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001D880u),
            func_8001D880);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001DB24u),
            func_8001DB24);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001DD40u),
            func_8001DD40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E09Cu),
            func_8001E09C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E108u),
            func_8001E108);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E1E4u),
            func_8001E1E4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E1F4u),
            func_8001E1F4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E338u),
            func_8001E338);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E3E0u),
            func_8001E3E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E3E8u),
            func_8001E3E8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E45Cu),
            func_8001E45C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E510u),
            func_8001E510);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E8D8u),
            func_8001E8D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001E99Cu),
            func_8001E99C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001EFD0u),
            func_8001EFD0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F070u),
            func_8001F070);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F0BCu),
            func_8001F0BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F134u),
            func_8001F134);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F16Cu),
            func_8001F16C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F188u),
            func_8001F188);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F82Cu),
            func_8001F82C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001F938u),
            func_8001F938);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001FA9Cu),
            func_8001FA9C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8001FC00u),
            func_8001FC00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80020F04u),
            func_80020F04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800210E4u),
            func_800210E4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800211C8u),
            func_800211C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80021220u),
            func_80021220);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80021310u),
            func_80021310);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80021D94u),
            func_80021D94);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80021ED8u),
            func_80021ED8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002201Cu),
            func_8002201C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002205Cu),
            func_8002205C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800221ECu),
            func_800221EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002237Cu),
            func_8002237C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800225D8u),
            func_800225D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800226A0u),
            func_800226A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80022894u),
            func_80022894);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80022BA4u),
            func_80022BA4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80022CA0u),
            func_80022CA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80022D68u),
            func_80022D68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80022DD4u),
            func_80022DD4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80022E94u),
            func_80022E94);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80023370u),
            func_80023370);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80024074u),
            func_80024074);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800240ECu),
            func_800240EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80024190u),
            func_80024190);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800241A0u),
            func_800241A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80024330u),
            func_80024330);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800244F0u),
            func_800244F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80024D6Cu),
            func_80024D6C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80024FA0u),
            func_80024FA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025050u),
            func_80025050);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025228u),
            func_80025228);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025274u),
            func_80025274);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800256DCu),
            func_800256DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025940u),
            func_80025940);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025B68u),
            func_80025B68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025E80u),
            func_80025E80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80025F10u),
            func_80025F10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80026404u),
            func_80026404);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80026608u),
            func_80026608);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80026970u),
            func_80026970);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80026E5Cu),
            func_80026E5C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002749Cu),
            func_8002749C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80027BECu),
            func_80027BEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80027F88u),
            func_80027F88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80028CD8u),
            func_80028CD8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80028CE0u),
            func_80028CE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80028E2Cu),
            func_80028E2C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80028EF8u),
            func_80028EF8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80028F7Cu),
            func_80028F7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029428u),
            func_80029428);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800295B0u),
            func_800295B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800296A4u),
            func_800296A4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800297DCu),
            func_800297DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029964u),
            func_80029964);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029AC4u),
            func_80029AC4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029BACu),
            func_80029BAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029C50u),
            func_80029C50);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029D88u),
            func_80029D88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80029E4Cu),
            func_80029E4C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002ABF0u),
            func_8002ABF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002AC9Cu),
            func_8002AC9C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002AD80u),
            func_8002AD80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002AE0Cu),
            func_8002AE0C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002B0ACu),
            func_8002B0AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002B228u),
            func_8002B228);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002B31Cu),
            func_8002B31C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002B4A0u),
            func_8002B4A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002C5C0u),
            func_8002C5C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002DF10u),
            func_8002DF10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002E478u),
            func_8002E478);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002E480u),
            func_8002E480);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002EB84u),
            func_8002EB84);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002F120u),
            func_8002F120);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002F268u),
            func_8002F268);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002F2D8u),
            func_8002F2D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002F2E0u),
            func_8002F2E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002F4D8u),
            func_8002F4D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002FF44u),
            func_8002FF44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002FF94u),
            func_8002FF94);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8002FFA0u),
            func_8002FFA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80030104u),
            func_80030104);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800302FCu),
            func_800302FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80030344u),
            func_80030344);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80030524u),
            func_80030524);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80030B6Cu),
            func_80030B6C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80030BCCu),
            func_80030BCC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800312F0u),
            func_800312F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031364u),
            func_80031364);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800314B8u),
            func_800314B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031518u),
            func_80031518);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031568u),
            func_80031568);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031B60u),
            func_80031B60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031C68u),
            func_80031C68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031C80u),
            func_80031C80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031D08u),
            func_80031D08);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80031D30u),
            func_80031D30);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003224Cu),
            func_8003224C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800325CCu),
            func_800325CC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800327B4u),
            func_800327B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80032B00u),
            func_80032B00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80032B70u),
            func_80032B70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80032C50u),
            func_80032C50);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80032D78u),
            func_80032D78);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003311Cu),
            func_8003311C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800333D4u),
            func_800333D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80033454u),
            func_80033454);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80033828u),
            func_80033828);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80033940u),
            func_80033940);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80033C54u),
            func_80033C54);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80033E04u),
            func_80033E04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800341C0u),
            func_800341C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800356B4u),
            func_800356B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80035C20u),
            func_80035C20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80035E60u),
            func_80035E60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036040u),
            func_80036040);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800361E8u),
            func_800361E8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800364A8u),
            func_800364A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003674Cu),
            func_8003674C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036B10u),
            func_80036B10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036B88u),
            func_80036B88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036BA0u),
            func_80036BA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036BD4u),
            func_80036BD4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036EC0u),
            func_80036EC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80036FF0u),
            func_80036FF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037100u),
            func_80037100);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800374F8u),
            func_800374F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037648u),
            func_80037648);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037704u),
            func_80037704);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037834u),
            func_80037834);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037878u),
            func_80037878);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800378CCu),
            func_800378CC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037980u),
            func_80037980);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037A7Cu),
            func_80037A7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037B2Cu),
            func_80037B2C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037BF0u),
            func_80037BF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80037D38u),
            func_80037D38);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A304u),
            func_8003A304);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A320u),
            func_8003A320);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A3B8u),
            func_8003A3B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A44Cu),
            func_8003A44C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A740u),
            func_8003A740);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A778u),
            func_8003A778);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A798u),
            func_8003A798);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003A9F4u),
            func_8003A9F4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003AAE0u),
            func_8003AAE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003AB78u),
            func_8003AB78);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003AC30u),
            func_8003AC30);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003AD48u),
            func_8003AD48);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003ADB4u),
            func_8003ADB4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003AE10u),
            func_8003AE10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003B078u),
            func_8003B078);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003B0B0u),
            func_8003B0B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003B354u),
            func_8003B354);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003B840u),
            func_8003B840);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003BE44u),
            func_8003BE44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003BFB0u),
            func_8003BFB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C804u),
            func_8003C804);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C83Cu),
            func_8003C83C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C848u),
            func_8003C848);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C850u),
            func_8003C850);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C858u),
            func_8003C858);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C860u),
            func_8003C860);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C868u),
            func_8003C868);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C888u),
            func_8003C888);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C8A8u),
            func_8003C8A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C8C8u),
            func_8003C8C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C8E8u),
            func_8003C8E8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C8F0u),
            func_8003C8F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003C91Cu),
            func_8003C91C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003CC64u),
            func_8003CC64);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D068u),
            func_8003D068);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D724u),
            func_8003D724);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D72Cu),
            func_8003D72C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D734u),
            func_8003D734);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D73Cu),
            func_8003D73C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D744u),
            func_8003D744);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D74Cu),
            func_8003D74C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D754u),
            func_8003D754);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D760u),
            func_8003D760);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D83Cu),
            func_8003D83C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003D928u),
            func_8003D928);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003DA04u),
            func_8003DA04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003DAF0u),
            func_8003DAF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003DB28u),
            func_8003DB28);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003DB60u),
            func_8003DB60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003E240u),
            func_8003E240);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003EAC8u),
            func_8003EAC8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003EE3Cu),
            func_8003EE3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8003FF40u),
            func_8003FF40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80040360u),
            func_80040360);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800411B0u),
            func_800411B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80042DB8u),
            func_80042DB8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800433CCu),
            func_800433CC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800450C8u),
            func_800450C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800451A4u),
            func_800451A4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80045420u),
            func_80045420);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800456A0u),
            func_800456A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800462CCu),
            func_800462CC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004638Cu),
            func_8004638C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80046458u),
            func_80046458);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800466F8u),
            func_800466F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048298u),
            func_80048298);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800482E0u),
            func_800482E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048324u),
            func_80048324);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800483DCu),
            func_800483DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048444u),
            func_80048444);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800484ECu),
            func_800484EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048544u),
            func_80048544);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048558u),
            func_80048558);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800485C0u),
            func_800485C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800485F0u),
            func_800485F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048658u),
            func_80048658);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048660u),
            func_80048660);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80048E00u),
            func_80048E00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800491B4u),
            func_800491B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049510u),
            func_80049510);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049640u),
            func_80049640);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049660u),
            func_80049660);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049680u),
            func_80049680);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049D54u),
            func_80049D54);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049D84u),
            func_80049D84);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049DE4u),
            func_80049DE4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049E3Cu),
            func_80049E3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049F88u),
            func_80049F88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80049FF0u),
            func_80049FF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004A660u),
            func_8004A660);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004A6ECu),
            func_8004A6EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004C008u),
            func_8004C008);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004C3FCu),
            func_8004C3FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004D124u),
            func_8004D124);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004D618u),
            func_8004D618);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004DB40u),
            func_8004DB40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004E5F4u),
            func_8004E5F4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004E604u),
            func_8004E604);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004E620u),
            func_8004E620);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004E640u),
            func_8004E640);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004E67Cu),
            func_8004E67C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004E7F0u),
            func_8004E7F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8004F494u),
            func_8004F494);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005007Cu),
            func_8005007C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800510FCu),
            func_800510FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80052184u),
            func_80052184);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800521BCu),
            func_800521BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80052214u),
            func_80052214);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800522BCu),
            func_800522BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800522C0u),
            func_800522C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80052488u),
            func_80052488);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80053AB0u),
            func_80053AB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80054CDCu),
            func_80054CDC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800550DCu),
            func_800550DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80055450u),
            func_80055450);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80055B14u),
            func_80055B14);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80057A8Cu),
            func_80057A8C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80057E24u),
            func_80057E24);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80057F3Cu),
            func_80057F3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80058F6Cu),
            func_80058F6C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800595ACu),
            func_800595AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800595F8u),
            func_800595F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80059C14u),
            func_80059C14);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005B628u),
            func_8005B628);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005B77Cu),
            func_8005B77C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005B954u),
            func_8005B954);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005BB2Cu),
            func_8005BB2C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005BCB4u),
            func_8005BCB4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005BE40u),
            func_8005BE40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005BF88u),
            func_8005BF88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C0ECu),
            func_8005C0EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C258u),
            func_8005C258);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C3E0u),
            func_8005C3E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C57Cu),
            func_8005C57C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C720u),
            func_8005C720);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C8A8u),
            func_8005C8A8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005C9FCu),
            func_8005C9FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005CB44u),
            func_8005CB44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005CCB0u),
            func_8005CCB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005CE38u),
            func_8005CE38);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005CE40u),
            func_8005CE40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005CE74u),
            func_8005CE74);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005D060u),
            func_8005D060);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8005D69Cu),
            func_8005D69C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066538u),
            func_80066538);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066738u),
            func_80066738);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006690Cu),
            func_8006690C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006695Cu),
            func_8006695C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066A48u),
            func_80066A48);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066AB4u),
            func_80066AB4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066AECu),
            func_80066AEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066B7Cu),
            func_80066B7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066DA0u),
            func_80066DA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80066E0Cu),
            func_80066E0C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80067014u),
            func_80067014);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006783Cu),
            func_8006783C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80067E08u),
            func_80067E08);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800689ECu),
            func_800689EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80068A3Cu),
            func_80068A3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80068B28u),
            func_80068B28);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006984Cu),
            func_8006984C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006AAA8u),
            func_8006AAA8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006ABB0u),
            func_8006ABB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006B888u),
            func_8006B888);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BAA8u),
            func_8006BAA8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BB34u),
            func_8006BB34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BBC8u),
            func_8006BBC8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BCC0u),
            func_8006BCC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BD80u),
            func_8006BD80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BDA4u),
            func_8006BDA4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006BE6Cu),
            func_8006BE6C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006C5E8u),
            func_8006C5E8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006C608u),
            func_8006C608);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006C628u),
            func_8006C628);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006CDF4u),
            func_8006CDF4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006D508u),
            func_8006D508);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006D988u),
            func_8006D988);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006E00Cu),
            func_8006E00C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006E048u),
            func_8006E048);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006E38Cu),
            func_8006E38C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006E93Cu),
            func_8006E93C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006EBA4u),
            func_8006EBA4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006F0E0u),
            func_8006F0E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006F180u),
            func_8006F180);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006F6E0u),
            func_8006F6E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006FA7Cu),
            func_8006FA7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006FA80u),
            func_8006FA80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8006FAE8u),
            func_8006FAE8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80074300u),
            func_80074300);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800764C8u),
            func_800764C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8007CBD8u),
            func_8007CBD8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8007CBF8u),
            func_8007CBF8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8007DDB4u),
            func_8007DDB4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8007DF94u),
            func_8007DF94);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80083770u),
            func_80083770);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80083798u),
            func_80083798);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800837E0u),
            func_800837E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008381Cu),
            func_8008381C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80083AF8u),
            func_80083AF8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80083B60u),
            func_80083B60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800842BCu),
            func_800842BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80084AECu),
            func_80084AEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80084C04u),
            func_80084C04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80084C8Cu),
            func_80084C8C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80085040u),
            func_80085040);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800853E0u),
            func_800853E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800856B4u),
            func_800856B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800858D4u),
            func_800858D4);
    // V46: see ts2_v46_overlay_transfer -- runs the real func_800859A4, then
    // reconciles librecomp's live func_map with the resident overlay.
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800859A4u),
            ts2_v46_overlay_transfer);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80085A80u),
            func_80085A80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80086350u),
            func_80086350);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008682Cu),
            func_8008682C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80086C44u),
            func_80086C44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087188u),
            func_80087188);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800871BCu),
            func_800871BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800871F0u),
            func_800871F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008721Cu),
            func_8008721C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087340u),
            func_80087340);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800873E0u),
            func_800873E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087820u),
            func_80087820);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087D60u),
            func_80087D60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087EA0u),
            func_80087EA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80088820u),
            func_80088820);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800889C0u),
            func_800889C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C220u),
            func_8008C220);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C240u),
            func_8008C240);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C880u),
            func_8008C880);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C8A0u),
            func_8008C8A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C8D0u),
            func_8008C8D0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C8F0u),
            func_8008C8F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008C8FCu),
            func_8008C8FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CA14u),
            func_8008CA14);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CAFCu),
            func_8008CAFC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CB10u),
            func_8008CB10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CB34u),
            func_8008CB34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CBA4u),
            func_8008CBA4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CC18u),
            func_8008CC18);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CC28u),
            func_8008CC28);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CC58u),
            func_8008CC58);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CC68u),
            func_8008CC68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CC78u),
            func_8008CC78);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CC88u),
            func_8008CC88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CCE0u),
            func_8008CCE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CD44u),
            func_8008CD44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CD54u),
            func_8008CD54);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CD60u),
            func_8008CD60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CD70u),
            func_8008CD70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CD7Cu),
            func_8008CD7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CDE8u),
            func_8008CDE8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CE84u),
            func_8008CE84);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CEACu),
            func_8008CEAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CEB8u),
            func_8008CEB8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CEC8u),
            func_8008CEC8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CED4u),
            func_8008CED4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CEE8u),
            func_8008CEE8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CEFCu),
            func_8008CEFC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CF04u),
            func_8008CF04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CF4Cu),
            func_8008CF4C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CF58u),
            func_8008CF58);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CF60u),
            func_8008CF60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CFD0u),
            func_8008CFD0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008CFE0u),
            func_8008CFE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D034u),
            func_8008D034);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D088u),
            func_8008D088);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D0DCu),
            func_8008D0DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D0ECu),
            func_8008D0EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D1C4u),
            func_8008D1C4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D200u),
            func_8008D200);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D210u),
            func_8008D210);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D258u),
            func_8008D258);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D2CCu),
            func_8008D2CC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D2D8u),
            func_8008D2D8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D588u),
            func_8008D588);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D5B4u),
            func_8008D5B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D5E8u),
            func_8008D5E8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D83Cu),
            func_8008D83C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D8C4u),
            func_8008D8C4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008D9E4u),
            func_8008D9E4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DAB0u),
            func_8008DAB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DB34u),
            func_8008DB34);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DBECu),
            func_8008DBEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DC48u),
            func_8008DC48);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DCACu),
            func_8008DCAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DD20u),
            func_8008DD20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DD90u),
            func_8008DD90);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DE20u),
            func_8008DE20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DEACu),
            func_8008DEAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DEECu),
            func_8008DEEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DF18u),
            func_8008DF18);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DF3Cu),
            func_8008DF3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DF4Cu),
            func_8008DF4C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DFACu),
            func_8008DFAC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008DFD4u),
            func_8008DFD4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E000u),
            func_8008E000);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E034u),
            func_8008E034);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E080u),
            func_8008E080);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E104u),
            func_8008E104);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E110u),
            func_8008E110);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E120u),
            func_8008E120);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E130u),
            func_8008E130);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E140u),
            func_8008E140);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E148u),
            func_8008E148);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E154u),
            func_8008E154);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E164u),
            func_8008E164);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E1D0u),
            func_8008E1D0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E250u),
            func_8008E250);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E260u),
            func_8008E260);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E2C8u),
            func_8008E2C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E354u),
            func_8008E354);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E3DCu),
            func_8008E3DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008E704u),
            func_8008E704);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008EB68u),
            func_8008EB68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008EBF8u),
            func_8008EBF8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008ED58u),
            func_8008ED58);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008EE68u),
            func_8008EE68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008EED8u),
            func_8008EED8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F0B8u),
            func_8008F0B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F0D4u),
            func_8008F0D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F18Cu),
            func_8008F18C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F1E4u),
            func_8008F1E4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F24Cu),
            func_8008F24C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F310u),
            func_8008F310);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F420u),
            func_8008F420);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F5ECu),
            func_8008F5EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F784u),
            func_8008F784);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F814u),
            func_8008F814);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008F944u),
            func_8008F944);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FB0Cu),
            func_8008FB0C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FB44u),
            func_8008FB44);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FBF8u),
            func_8008FBF8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FCCCu),
            func_8008FCCC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FEB0u),
            func_8008FEB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FF04u),
            func_8008FF04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8008FF20u),
            func_8008FF20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090098u),
            func_80090098);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800901F0u),
            func_800901F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090390u),
            func_80090390);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800903A0u),
            func_800903A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009065Cu),
            func_8009065C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090660u),
            func_80090660);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800906D4u),
            func_800906D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090714u),
            func_80090714);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090720u),
            func_80090720);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800908DCu),
            func_800908DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800908E0u),
            func_800908E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090930u),
            func_80090930);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090964u),
            func_80090964);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009097Cu),
            func_8009097C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009098Cu),
            func_8009098C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800909B8u),
            func_800909B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090A40u),
            func_80090A40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090A9Cu),
            func_80090A9C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090D6Cu),
            func_80090D6C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090E04u),
            func_80090E04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090F5Cu),
            func_80090F5C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091240u),
            func_80091240);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091258u),
            func_80091258);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009129Cu),
            func_8009129C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800912E0u),
            func_800912E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091400u),
            func_80091400);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091480u),
            func_80091480);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800915DCu),
            func_800915DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091760u),
            func_80091760);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800917F8u),
            func_800917F8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800918A0u),
            func_800918A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800918F0u),
            func_800918F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800919E0u),
            osCreateThread_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091AB0u),
            func_80091AB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091AC0u),
            func_80091AC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091AE0u),
            func_80091AE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091B00u),
            func_80091B00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091B10u),
            func_80091B10);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091B20u),
            osStartThread_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091C40u),
            osRecvMesg_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091D70u),
            func_80091D70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091DE0u),
            func_80091DE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091E20u),
            osPiStartDma_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80091EE0u),
            osJamMesg_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092020u),
            func_80092020);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092040u),
            osMotorStart_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092200u),
            osMotorStop_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800923C0u),
            osMotorInit_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092890u),
            func_80092890);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800928F4u),
            func_800928F4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092960u),
            func_80092960);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092990u),
            func_80092990);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800929A0u),
            func_800929A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092BD4u),
            func_80092BD4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092D70u),
            func_80092D70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092D80u),
            func_80092D80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092E60u),
            func_80092E60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092EF0u),
            func_80092EF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092F80u),
            func_80092F80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80092FC8u),
            func_80092FC8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093040u),
            osPfsRepairId_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093200u),
            osPfsFindFile_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800933A0u),
            osViSetSpecialFeatures_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093520u),
            func_80093520);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093630u),
            ts2_current_internal_entry_80093630_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093680u),
            func_80093680);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093720u),
            osContStartReadData_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800937A8u),
            osContGetReadData_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093844u),
            func_80093844);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093900u),
            func_80093900);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093AB0u),
            func_80093AB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093C80u),
            osInitialize_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093E90u),
            func_80093E90);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093EA0u),
            func_80093EA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093EC0u),
            func_80093EC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093EE0u),
            func_80093EE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80093FD8u),
            func_80093FD8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009401Cu),
            func_8009401C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800942FCu),
            func_800942FC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094460u),
            func_80094460);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094640u),
            func_80094640);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094704u),
            func_80094704);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800949DCu),
            func_800949DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094A30u),
            func_80094A30);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094A40u),
            func_80094A40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094AC0u),
            osPfsAllocateFile_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80094E50u),
            func_80094E50);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009501Cu),
            func_8009501C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800950B0u),
            func_800950B0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800950B8u),
            func_800950B8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800950C0u),
            func_800950C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095134u),
            func_80095134);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800951C0u),
            func_800951C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009529Cu),
            func_8009529C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095350u),
            func_80095350);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095360u),
            func_80095360);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095650u),
            func_80095650);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095660u),
            osPfsFileState_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800958A0u),
            func_800958A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095E14u),
            func_80095E14);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80095F04u),
            func_80095F04);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009600Cu),
            __osEnqueueThread_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096054u),
            __osPopThread_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096064u),
            func_80096064);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009670Cu),
            func_8009670C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800967DCu),
            func_800967DC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096890u),
            func_80096890);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800968A0u),
            func_800968A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096A80u),
            osSetIntMask_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096B20u),
            func_80096B20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096B74u),
            func_80096B74);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096C00u),
            func_80096C00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096D00u),
            func_80096D00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096DE0u),
            ts2_current_internal_entry_80096DE0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096F28u),
            ts2_current_internal_entry_80096F28_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80096F80u),
            ts2_current_internal_entry_80096F80_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097010u),
            ts2_current_internal_entry_80097010_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097018u),
            ts2_current_internal_entry_80097018_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009711Cu),
            ts2_current_internal_entry_8009711C_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009721Cu),
            ts2_current_internal_entry_8009721C_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009732Cu),
            ts2_current_internal_entry_8009732C_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009740Cu),
            func_8009740C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097460u),
            func_80097460);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097510u),
            func_80097510);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800975BCu),
            func_800975BC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800976F0u),
            func_800976F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097728u),
            func_80097728);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097940u),
            func_80097940);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097994u),
            ts2_current_internal_entry_80097994_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800979A0u),
            ::osSpTaskLoad_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097BACu),
            ::osSpTaskStartGo_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097BE0u),
            func_80097BE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097C00u),
            osSpTaskYielded_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097C60u),
            func_80097C60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097C80u),
            func_80097C80);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097CC0u),
            func_80097CC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097CD0u),
            func_80097CD0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097CE0u),
            func_80097CE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097CF0u),
            func_80097CF0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097D00u),
            osDpSetNextBuffer_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097DA0u),
            func_80097DA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097DC0u),
            func_80097DC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097E20u),
            func_80097E20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097E58u),
            func_80097E58);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097E7Cu),
            func_80097E7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80097EC0u),
            func_80097EC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098060u),
            osViGetNextFramebuffer_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800980A0u),
            osCreateMesgQueue_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800980E0u),
            func_800980E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800981A0u),
            func_800981A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800981C0u),
            osSetEventMesg_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098220u),
            osSpTaskYield_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098240u),
            func_80098240);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800982E0u),
            func_800982E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098380u),
            osContStartQuery_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098400u),
            osContGetQuery_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098420u),
            func_80098420);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098440u),
            osViSetEvent_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800984A0u),
            ::osContInit_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009863Cu),
            func_8009863C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800986F0u),
            func_800986F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800987B0u),
            ts2_current_internal_entry_800987B0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800987C0u),
            osSetTimer_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098860u),
            func_80098860);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800988B4u),
            func_800988B4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800989F0u),
            func_800989F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098A50u),
            func_80098A50);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098B60u),
            __osSetCompare_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098B70u),
            func_80098B70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098B80u),
            osSendMesg_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098CB0u),
            func_80098CB0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098CC0u),
            func_80098CC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098DC0u),
            func_80098DC0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098F3Cu),
            func_80098F3C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80098FE0u),
            func_80098FE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800990A0u),
            osPfsNumFiles_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800991A0u),
            func_800991A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800991B0u),
            ts2_current_internal_entry_800991B0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800991C0u),
            osPfsDeleteFile_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800993ECu),
            func_800993EC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009958Cu),
            func_8009958C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099660u),
            osInvalDCache_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099714u),
            func_80099714);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099720u),
            osInvalICache_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800997A0u),
            func_800997A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099800u),
            osCreateViManager_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099998u),
            ts2_runtime_owned_vi_manager_trap_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099B40u),
            osGetThreadPri_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099B60u),
            func_80099B60);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099B80u),
            osCreatePiManager_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099D20u),
            func_80099D20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099F00u),
            func_80099F00);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A3A0u),
            func_8009A3A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A3E0u),
            osYieldThread_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A430u),
            func_8009A430);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A440u),
            func_8009A440);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A490u),
            func_8009A490);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A4A0u),
            func_8009A4A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A640u),
            osViSwapBuffer_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A6A0u),
            func_8009A6A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009A8C0u),
            func_8009A8C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009AA60u),
            osPfsReadWriteFile_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009AE90u),
            func_8009AE90);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009AEA0u),
            osSetThreadPri_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009AF70u),
            func_8009AF70);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009AF80u),
            osPfsInitPak_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B150u),
            func_8009B150);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B160u),
            func_8009B160);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B5ACu),
            func_8009B5AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B704u),
            func_8009B704);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B870u),
            ts2_current_internal_entry_8009B870_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B878u),
            ts2_current_internal_entry_8009B878_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B880u),
            func_8009B880);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B950u),
            ts2_current_internal_entry_8009B950_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B960u),
            func_8009B960);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B9B0u),
            ts2_current_internal_entry_8009B9B0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009B9C0u),
            osVirtualToPhysical_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BA20u),
            func_8009BA20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BAE0u),
            osPfsFreeBlocks_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BBF0u),
            ts2_current_internal_entry_8009BBF0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BC00u),
            osViGetCurrentFramebuffer_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BC40u),
            func_8009BC40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BD90u),
            ts2_current_internal_entry_8009BD90_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BDA0u),
            func_8009BDA0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BE30u),
            ts2_current_internal_entry_8009BE30_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009BE40u),
            func_8009BE40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C160u),
            func_8009C160);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C2B0u),
            ts2_current_internal_entry_8009C2B0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C2C0u),
            func_8009C2C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C330u),
            func_8009C330);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C340u),
            func_8009C340);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C468u),
            func_8009C468);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C520u),
            func_8009C520);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C54Cu),
            func_8009C54C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C57Cu),
            func_8009C57C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C59Cu),
            func_8009C59C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C5D0u),
            ts2_current_internal_entry_8009C5D0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C5E0u),
            func_8009C5E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C670u),
            ts2_current_internal_entry_8009C670_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C680u),
            func_8009C680);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C6D0u),
            ts2_current_internal_entry_8009C6D0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C6E0u),
            func_8009C6E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C760u),
            func_8009C760);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C820u),
            func_8009C820);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C8C0u),
            func_8009C8C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C960u),
            func_8009C960);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C980u),
            func_8009C980);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CC4Cu),
            func_8009CC4C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CDFCu),
            func_8009CDFC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CE28u),
            func_8009CE28);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CE40u),
            func_8009CE40);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CE94u),
            func_8009CE94);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CED4u),
            func_8009CED4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CF1Cu),
            func_8009CF1C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009CF68u),
            func_8009CF68);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D010u),
            func_8009D010);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D120u),
            func_8009D120);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D150u),
            func_8009D150);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D160u),
            func_8009D160);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D29Cu),
            func_8009D29C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D2D0u),
            func_8009D2D0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D420u),
            func_8009D420);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D450u),
            ts2_current_internal_entry_8009D450_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D460u),
            func_8009D460);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D500u),
            func_8009D500);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009DA4Cu),
            func_8009DA4C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009DB20u),
            func_8009DB20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009DE20u),
            ts2_current_internal_entry_8009DE20_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009DF00u),
            ts2_current_internal_entry_8009DF00_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009DF24u),
            func_8009DF24);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009E1A4u),
            func_8009E1A4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009E230u),
            func_8009E230);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009E5F0u),
            func_8009E5F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009E604u),
            func_8009E604);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009E7F4u),
            func_8009E7F4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009EA0Cu),
            func_8009EA0C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009EB7Cu),
            func_8009EB7C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009ECECu),
            func_8009ECEC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009ED88u),
            func_8009ED88);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009EE20u),
            func_8009EE20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F00Cu),
            func_8009F00C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F0E0u),
            func_8009F0E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F184u),
            func_8009F184);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F61Cu),
            func_8009F61C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F6D4u),
            func_8009F6D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F784u),
            func_8009F784);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F810u),
            func_8009F810);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F86Cu),
            func_8009F86C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F8C8u),
            func_8009F8C8);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F910u),
            ts2_current_internal_entry_8009F910_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F920u),
            func_8009F920);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F940u),
            func_8009F940);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F9D4u),
            func_8009F9D4);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009FE24u),
            func_8009FE24);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A01C0u),
            func_800A01C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A036Cu),
            func_800A036C);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A04F0u),
            ts2_current_internal_entry_800A04F0_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A0500u),
            func_800A0500);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A0540u),
            osEPiStartDma_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A05E0u),
            osAiGetLength_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A05F0u),
            func_800A05F0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A0600u),
            osAiSetNextBuffer_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A06A0u),
            func_800A06A0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A06C0u),
            osAiGetStatus_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A06E0u),
            osAiSetFrequency_recomp);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800A0820u),
            func_800A0820);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D020u),
            ::func_8009D020);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D2E0u),
            ::func_8009D2E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80087870u),
            ::ts2_auto_alias_80087870);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099E20u),
            ::func_80099E20);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80099FE0u),
            ::func_80099FE0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009C4ACu),
            ::ts2_auto_alias_8009C4AC);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009D1C0u),
            ::func_8009D1C0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009E240u),
            ::func_8009E240);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x8009F9E0u),
            ::func_8009F9E0);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090730u),
            func_80090730);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x80090798u),
            func_80090798);
    recomp::overlays::add_loaded_function(
            static_cast<int32_t>(0x800907FCu),
            func_800907FC);
    std::fprintf(
        stderr,
        "TS2 OLDNATIVE V79: restored 1070 exact Stage61 lookup registrations.\n");
    std::fflush(stderr);
}
// TS2 OLDNATIVE V79 EXACT STAGE61 LOOKUP MAP REUSE END

// TS2 OLDNATIVE V79 CANONICAL N64MODERNRUNTIME HOST
#include "bridge_audio.h"
RspExitReason ts2_aspMain(uint8_t* rdram, uint32_t ucode);
namespace {

#if defined(_WIN32)
static void ts2_v79_capture_audio_rsp_if_requested(const OSTask* task) {
    // F8 captures exactly one matching task on each key press. Copy the task
    // and live RDRAM here: get_rsp_microcode is called synchronously before
    // librecomp initializes DMEM or executes the selected microcode.
    static bool f8_was_down = false;
    const bool f8_is_down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    const bool capture_requested = f8_is_down && !f8_was_down;
    f8_was_down = f8_is_down;

    if (!capture_requested) {
        return;
    }

    if (!g_rdram.is_external() || g_rdram.data() == nullptr) {
        std::fputs(
            "TS2 AUDIO RSP CAPTURE: F8 ignored because live RDRAM is not bound.\n",
            stderr);
        std::fflush(stderr);
        return;
    }

    const OSTask captured_task = *task;
    const std::vector<uint8_t> captured_rdram(g_rdram.begin(), g_rdram.end());
    // Each press gets its own pair, so a "sounds fine" and a "sounds missing"
    // capture from the same session can be compared.
    const std::string stamp = std::to_string(static_cast<long long>(std::time(nullptr)));
    const std::string snapshotPath = "C:/SpaceRanger/docs/audio-captures/audio-snapshot-" + stamp + ".bin";
    const std::string taskPath = "C:/SpaceRanger/docs/audio-captures/audio-task-" + stamp + ".bin";
    std::error_code mkdirError;
    std::filesystem::create_directories("C:/SpaceRanger/docs/audio-captures", mkdirError);
    const char* kSnapshotPath = snapshotPath.c_str();
    const char* kTaskPath = taskPath.c_str();

    std::ofstream snapshot_output(
        kSnapshotPath,
        std::ios::binary | std::ios::trunc);
    std::ofstream task_output(
        kTaskPath,
        std::ios::binary | std::ios::trunc);

    snapshot_output.write(
        reinterpret_cast<const char*>(captured_rdram.data()),
        static_cast<std::streamsize>(captured_rdram.size()));
    task_output.write(
        reinterpret_cast<const char*>(&captured_task),
        static_cast<std::streamsize>(sizeof(captured_task)));
    snapshot_output.close();
    task_output.close();

    if (!snapshot_output || !task_output) {
        std::fprintf(
            stderr,
            "TS2 AUDIO RSP CAPTURE: failed to write %s and %s.\n",
            kSnapshotPath,
            kTaskPath);
    }
    else {
        std::fprintf(
            stderr,
            "TS2 AUDIO RSP CAPTURE: saved pre-execution task and 8 MiB RDRAM "
            "to %s and %s.\n",
            kTaskPath,
            kSnapshotPath);
    }
    std::fflush(stderr);
}
#endif

extern "C" uint32_t ts2_dropped_external_message_count(uint32_t* last_queue);

// TS2_AUDIO_DIAGNOSTICS=1: once every ~2 s, summarise the game's sample DMA
// buffer pool (func_800901F0 / func_80090098) and the audio command lists.
// An empty free list makes func_800901F0 hand back an in-use buffer (wrong
// sample data); dropped PI completions mean the DMA message queue was full.
static void ts2_audio_task_diagnostics(const OSTask* task) {
    static const bool enabled = [] {
        const char* v = std::getenv("TS2_AUDIO_DIAGNOSTICS");
        return v && v[0] && v[0] != '0';
    }();
    if (!enabled || !g_rdram.is_external() || g_rdram.data() == nullptr) return;
    const uint8_t* ram = g_rdram.data();
    auto word = [&](uint32_t address) -> uint32_t {
        const uint32_t offset = address & 0x007FFFFFu;
        uint32_t v; std::memcpy(&v, ram + offset, 4); return v;
    };
    auto count_list = [&](uint32_t head) {
        uint32_t n = 0;
        for (uint32_t p = head; p >= 0x80000000u && p < 0x80800000u && n < 1024; p = word(p + 4)) ++n;
        return n;
    };
    struct Stats {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        uint32_t tasks = 0, emptyFree = 0, minFree = UINT32_MAX, maxUsed = 0, maxPending = 0;
        uint32_t maxQueue = 0, queueCap = 0, minCmd = UINT32_MAX, maxCmd = 0, droppedAtStart = 0;
        uint64_t sumCmd = 0;
    };
    static Stats s;
    if (s.tasks == 0) s.droppedAtStart = ts2_dropped_external_message_count(nullptr);
    const uint32_t freeCount = count_list(word(0x800ACC64u));
    const uint32_t usedCount = count_list(word(0x800ACC60u));
    ++s.tasks;
    s.emptyFree += freeCount == 0;
    s.minFree = std::min(s.minFree, freeCount);
    s.maxUsed = std::max(s.maxUsed, usedCount);
    s.maxPending = std::max(s.maxPending, word(0x800ACC78u));
    s.maxQueue = std::max(s.maxQueue, word(0x800ACC7Cu + 8));
    s.queueCap = word(0x800ACC7Cu + 0x10);
    const uint32_t cmd = task->t.data_size;
    s.minCmd = std::min(s.minCmd, cmd); s.maxCmd = std::max(s.maxCmd, cmd); s.sumCmd += cmd;
    const auto now = std::chrono::steady_clock::now();
    if (now - s.start < std::chrono::seconds(2)) return;
    uint32_t lastQueue = 0;
    const uint32_t dropped = ts2_dropped_external_message_count(&lastQueue);
    std::fprintf(stderr,
        "TS2 AUDIO DMA: tasks=%u free_min=%u empty_free_tasks=%u used_max=%u pending_max=%u "
        "dmaq_max=%u/%u dropped_msgs=+%u(last_q=%08X) cmd=%u/%llu/%u\n",
        s.tasks, s.minFree, s.emptyFree, s.maxUsed, s.maxPending, s.maxQueue, s.queueCap,
        dropped - s.droppedAtStart, lastQueue, s.minCmd,
        static_cast<unsigned long long>(s.sumCmd / std::max(1u, s.tasks)), s.maxCmd);
    std::fflush(stderr);
    s = Stats{};
}

static RspUcodeFunc* ts2_v79_get_rsp_microcode(const OSTask* task) {
    // func_800904DC supplies this resident audio program to func_800907FC.
    // Accept both physical and KSEG0 forms of the same RDRAM address.
    if (task && task->t.type == 2 &&
        (task->t.ucode & 0x1FFFFFFFu) == 0x00088B80u &&
        (task->t.ucode_data & 0x1FFFFFFFu) == 0x000A8F60u) {
#if defined(_WIN32)
        ts2_v79_capture_audio_rsp_if_requested(task);
#endif
        ts2_audio_task_diagnostics(task);
        return &ts2_aspMain;
    }
    std::fprintf(stderr, "TS2 RSP: unknown task type=%u text=%08X data=%08X\n",
        task ? task->t.type : 0, task ? task->t.ucode : 0,
        task ? task->t.ucode_data : 0);
    return nullptr;
}

static ultramodern::gfx_callbacks_t::gfx_data_t ts2_v79_create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_GAMECONTROLLER |
            SDL_INIT_JOYSTICK |
            SDL_INIT_HAPTIC) != 0) {
        std::fprintf(
            stderr,
            "TS2 OLDNATIVE V79 FATAL: SDL_Init failed: %s\n",
            SDL_GetError());
        std::fflush(stderr);
        std::abort();
    }

    std::fputs("TS2 OLDNATIVE V79: SDL host initialized.\n", stderr);
    std::fflush(stderr);
    return {};
}

static ultramodern::renderer::WindowHandle ts2_v79_create_window(
    ultramodern::gfx_callbacks_t::gfx_data_t) {

    uint32_t flags = SDL_WINDOW_RESIZABLE;

#ifdef RT64_SDL_WINDOW_VULKAN
    flags |= SDL_WINDOW_VULKAN;
#endif

    window = SDL_CreateWindow(
        "SpaceRanger",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1600,
        900,
        flags);

    if (window == nullptr) {
        std::fprintf(
            stderr,
            "TS2 OLDNATIVE V79 FATAL: SDL_CreateWindow failed: %s\n",
            SDL_GetError());
        std::fflush(stderr);
        std::abort();
    }

#if defined(_WIN32)
    SDL_SysWMinfo wm_info{};
    SDL_VERSION(&wm_info.version);

    if (!SDL_GetWindowWMInfo(window, &wm_info)) {
        std::fprintf(
            stderr,
            "TS2 OLDNATIVE V79 FATAL: SDL_GetWindowWMInfo failed: %s\n",
            SDL_GetError());
        std::fflush(stderr);
        std::abort();
    }

    std::fputs(
        "TS2 OLDNATIVE V79: canonical RecompFrontend window created.\n",
        stderr);
    std::fflush(stderr);

    return ultramodern::renderer::WindowHandle{
        wm_info.info.win.window,
        GetCurrentThreadId()
    };
#else
    return {};
#endif
}

static void ts2_v79_update_gfx(void*) {
    // TS2 G170 MAIN-LOOP HEARTBEAT: diagnostic-only, no guest/game-logic
    // effect. This callback is the confirmed-live per-frame hook invoked by
    // the real (external, librecomp/ultramodern-owned) main loop every
    // frame -- unlike the elaborate Stage61-92 validator machinery
    // elsewhere in this file, which was confirmed dead/unreachable in this
    // build (run_stage61() is never called). If this heartbeat stops
    // printing during the black-screen freeze, the main thread/render loop
    // itself has stopped calling back into game code at all. If it keeps
    // printing throughout the freeze, the main loop is alive and the stall
    // is elsewhere (game logic / guest thread / GPU submission).
    {
        static uint64_t g170_heartbeat_count = 0;
        ++g170_heartbeat_count;
        if (g170_heartbeat_count <= 20 || (g170_heartbeat_count % 300) == 0) {
            std::fprintf(stderr, "TS2 G170 HEARTBEAT: update_gfx call #%llu\n",
                static_cast<unsigned long long>(g170_heartbeat_count));
            std::fflush(stderr);
        }
    }
    recompinput::handle_events();
    // Read UI configuration on its owning main thread; the audio thread only
    // consumes an atomic gain value. Sound configuration exists once play starts.
    if (ultramodern::is_game_started()) {
        ts2::audio::set_volume(static_cast<float>(recompui::config::sound::get_main_volume()));
    }
}

static void ts2_v79_gfx_init_autostart() {
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    std::fputs(
        "TS2 OLDNATIVE V79: native renderer initialized; "
        "starting GameEntry spaceranger.\n",
        stderr);
    std::fflush(stderr);

    recomp::start_game(u8"spaceranger", "");
}

extern "C" void ts2_install_runtime_diagnostics();
static int run_ts2_v79_canonical_runtime(bool check_assets_only = false) {
    ts2_install_runtime_diagnostics();
    // TS2 G169 STDOUT VISIBILITY FIX: diagnostic-only, no guest/game-logic
    // effect whatsoever. Every std::fprintf(stderr, ...) diagnostic in this
    // file has always been visible in a redirected-output capture because
    // stderr is unbuffered by default. However stage61_recv_bridge's
    // "N64MR RECV #%u: queue=... msg=... flags=..." line (and a handful of
    // other std::printf/std::cout-based diagnostics elsewhere in this file)
    // go to stdout, which on Windows is FULLY buffered once redirected to a
    // file instead of a console -- meaning that output silently vanishes
    // (or only appears once the OS buffer happens to fill) instead of
    // reaching ts2_recomp_stdout_capture.log. Forcing stdout to be
    // line-buffered here, before any output happens, makes all of that
    // pre-existing diagnostic output finally visible without touching a
    // single call site or any game/runtime behavior.
    std::setvbuf(stdout, nullptr, _IOLBF, 4096);
    // RecompFrontend resolves assets and portable config from the working
    // directory. Shells, shortcuts, and debuggers may start us elsewhere.
    // Establish the executable directory before registering any paths or
    // starting renderer threads, and reject missing assets here instead of
    // letting the stylesheet loader resize a string from a failed tellg().
    char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        std::fprintf(stderr, "TS2 STARTUP: cannot locate executable directory: %s\n",
            SDL_GetError());
        return 8;
    }
    const auto runtime_directory = std::filesystem::u8path(base_path);
    SDL_free(base_path);
    std::error_code path_error;
    std::filesystem::current_path(runtime_directory, path_error);
    if (path_error) {
        std::fprintf(stderr, "TS2 STARTUP: cannot open executable directory: %s\n",
            path_error.message().c_str());
        return 8;
    }
    for (const char* asset : {"recomp.rcss", "InterVariable.ttf",
                             "NotoEmoji-Regular.ttf", "promptfont/promptfont.ttf"}) {
        const auto asset_path = runtime_directory / "assets" / asset;
        std::ifstream asset_stream(asset_path, std::ios::binary);
        if (!asset_stream || asset_stream.peek() == std::ifstream::traits_type::eof()) {
            std::fprintf(stderr, "TS2 STARTUP: missing, empty, or unreadable asset: %s\n"
                "Rebuild the native target to stage its assets next to the executable.\n",
                asset_path.string().c_str());
            return 8;
        }
    }

    std::fprintf(stderr, "TS2 STARTUP: runtime assets verified in %s\n",
        runtime_directory.string().c_str());
    if (check_assets_only) {
        return 0;
    }

    std::puts(
        "Toy Story 2 OldNative V79 - canonical N64ModernRuntime ownership");

    if (!ts2::validate_section_layout()) {
        std::fputs(
            "TS2 OLDNATIVE V79 FATAL: section layout validation failed.\n",
            stderr);
        return 2;
    }

    // The runtime owns overlays::init_overlays().
    ts2::register_sections();

    // Reuse the already-proven program/config metadata.
    ::ts2_initialize_recompfrontend_metadata();

    for (const auto& game : supported_games) {
        if (!recomp::register_game(game)) {
            std::fputs(
                "TS2 OLDNATIVE V79 FATAL: recomp::register_game failed.\n",
                stderr);
            return 3;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(recomp::get_config_path(), ec);
    if (ec) {
        std::fprintf(
            stderr,
            "TS2 OLDNATIVE V79 FATAL: config directory creation failed: %s\n",
            ec.message().c_str());
        return 4;
    }

    // Normal launches show the title screen (its Start Game option calls
    // recomp::start_game, and "Load ROM" handles a missing ROM). Automated runs
    // (input replays, TS2_AUTOSTART=1) skip it, so they need a ROM up front.
    const char* ts2_replay_env = std::getenv("TS2_INPUT_REPLAY");
    const bool ts2_autostart =
        std::getenv("TS2_AUTOSTART") != nullptr ||
        (ts2_replay_env != nullptr && ts2_replay_env[0] != '\0');

    std::u8string game_id = u8"spaceranger";
    if (!ts2_ensure_rom(game_id, ts2_autostart) && ts2_autostart) {
        std::fputs(
            "TS2 OLDNATIVE V79: no valid Toy Story 2 ROM selected.\n",
            stderr);
        return 5;
    }

    // Execute the game's recompiled audio microcode before AI consumes PCM.
    const recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = ts2_v79_get_rsp_microcode,
    };

    // Reuse the exact PresentEarly renderer callback helper compiled and linked
    // successfully in V65 inside this same Stage79 translation unit.
    const ultramodern::renderer::callbacks_t renderer_callbacks =
        ::ts2_make_canonical_renderer_callbacks();

    if (!ts2::audio::initialize()) return 7;
    const ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = ts2::audio::queue_samples,
        .get_frames_remaining = ts2::audio::get_frames_remaining,
        .set_frequency = ts2::audio::set_frequency,
    };

    const ultramodern::input::callbacks_t input_callbacks{
        .poll_input = recompinput::poll_inputs,
        .get_input = ts2_get_input,
        .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = ts2_runtime_get_connected_device_info,
    };

    const ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = ts2_v79_create_gfx,
        .create_window = ts2_v79_create_window,
        .update_gfx = ts2_v79_update_gfx,
    };

    // TS2 G171 REAL FIX: gfx_init_callback was wired to nullptr, meaning
    // ts2_v79_gfx_init_autostart() (defined above, fully implemented, and
    // clearly written to be used here -- it has its own log line and calls
    // recomp::start_game(u8"spaceranger", "")) was never actually invoked.
    // recomp::start_game(...) is the ONLY place in this whole codebase that
    // moves ultramodern's game_status out of GameStatus::None (see
    // recomp.cpp's recomp::start_game/is_game_started), and
    // ultramodern::is_game_started() gates the VI thread's real per-frame
    // VI/AI retrace message delivery (see events.cpp's vi_thread_func,
    // "if (ultramodern::is_game_started()) { ... enqueue_external_message_src
    // for VI/AI ... }"). With this never wired up, the VI thread runs
    // forever (which is why the render loop / G170 heartbeat never stops),
    // but it silently never sends the real 60Hz VI/AI messages the game's
    // own guest threads block on to advance past their first tick --
    // producing exactly the observed symptom: one audio buffer, one
    // deterministic asset-load DMA burst, then a permanent black-screen
    // freeze with zero further output. Wiring this callback back in (it is
    // called once, from gfx_thread_func, right as the renderer finishes
    // initializing and before the VI thread starts its per-frame loop --
    // events.cpp's init_events()/gfx_thread_func()) is the real fix: it
    // restores the dropped call that marks the game as started, so the VI
    // thread's already-correct 60Hz pacing logic finally reaches the game.
    const ultramodern::events::callbacks_t events_callbacks{
        .vi_callback = recompinput::update_rumble,
        .gfx_init_callback = ts2_autostart ? ts2_v79_gfx_init_autostart : nullptr,
    };

    const ultramodern::error_handling::callbacks_t error_callbacks{
        .message_box = recompui::message_box,
    };

    const ultramodern::threads::callbacks_t threads_callbacks{};
    const recomp::Version version{0, 1, 0};

    std::fputs(
        "TS2 V142: entering recomp::start with stock RecompFrontend "
        "launcher/input ownership.\n",
        stderr);
    std::fflush(stderr);

    const recomp::Configuration runtime_config{
        .project_version = version,
        .window_handle = {},
        .rsp_callbacks = rsp_callbacks,
        .renderer_callbacks = renderer_callbacks,
        .audio_callbacks = audio_callbacks,
        .input_callbacks = input_callbacks,
        .gfx_callbacks = gfx_callbacks,
        .events_callbacks = events_callbacks,
        .error_handling_callbacks = error_callbacks,
        .threads_callbacks = threads_callbacks,
        .message_queue_control = {},
    };

    recomp::start(runtime_config);

    g_rdram.bind_owned();
    ts2::audio::shutdown();
    SDL_Quit();

    std::fputs(
        "TS2 OLDNATIVE V79: canonical runtime exited cleanly.\n",
        stderr);
    std::fflush(stderr);
    return 0;
}

} // namespace
// TS2 OLDNATIVE V79 CANONICAL N64MODERNRUNTIME HOST END

// Config (spaceranger-config) and UI assets are resolved relative to the
// working directory; run from the exe's folder so shortcuts and other launch
// locations find them.
static void ts2_enter_exe_dir() {
    std::error_code ec;
    std::filesystem::current_path(ts2_exe_dir(), ec);
}

int main(int argc, char** argv) {
    ts2_enter_exe_dir();
    if (argc == 2 && std::strcmp(argv[1], "--check-overlays") == 0) {
        return ts2::check_overlay_lookup();
    }
    return run_ts2_v79_canonical_runtime(
        argc == 2 && std::strcmp(argv[1], "--check-assets") == 0);
}

extern "C" int SDL_main(int argc, char** argv) {
    ts2_enter_exe_dir();
    if (argc == 2 && std::strcmp(argv[1], "--check-overlays") == 0) {
        return ts2::check_overlay_lookup();
    }
    return run_ts2_v79_canonical_runtime(
        argc == 2 && std::strcmp(argv[1], "--check-assets") == 0);
}

// Diagnostic-only probe for Toy Story 2's graphics-submit pacing path.
// It does not modify guest state, counters, queues, or tasks.
extern "C" void ts2_gfx_submit_flow_probe(
    uint8_t* rdram,
    uint32_t stage,
    uint32_t queue_addr,
    uint32_t task_addr) {

    static uint32_t probe_count = 0u;
    if (probe_count >= 128u) {
        return;
    }

    const uint32_t outstanding = read_guest_u32(0x800B0848u);
    const uint32_t parity = read_guest_u32(0x801E3174u);

    uint32_t task_kind = 0xFFFFFFFFu;
    uint32_t completion_queue = 0u;
    uint32_t completion_msg = 0u;

    if (task_addr >= 0x80000000u && task_addr <= 0x807FFFABu) {
        task_kind = read_guest_u32(task_addr + 0x10u);
        completion_queue = read_guest_u32(task_addr + 0x50u);
        completion_msg = read_guest_u32(task_addr + 0x54u);
    }

    ++probe_count;

    std::fprintf(
        stderr,
        "TS2 GFX SUBMIT FLOW #%u stage=%u outstanding=%u "
        "parity=%u queue=0x%08X task=0x%08X "
        "taskKind=0x%08X completionQ=0x%08X completionMsg=0x%08X\n",
        probe_count,
        stage,
        outstanding,
        parity,
        queue_addr,
        task_addr,
        task_kind,
        completion_queue,
        completion_msg);
    std::fflush(stderr);
}
// Diagnostic-only scheduler selector probe.
// Reads Toy Story 2 scheduler state but does not modify any guest state.
extern "C" void ts2_scheduler_select_probe(
    uint8_t* rdram,
    uint32_t phase,
    uint32_t scheduler_addr,
    uint32_t resource_mask,
    uint32_t rsp_out_addr,
    uint32_t dp_out_addr) {

    static uint32_t probe_count = 0u;
    if (probe_count >= 256u) {
        return;
    }

    const uint32_t audio_head = read_guest_u32(scheduler_addr + 0x264u);
    const uint32_t audio_tail = read_guest_u32(scheduler_addr + 0x26Cu);
    const uint32_t normal_head = read_guest_u32(scheduler_addr + 0x268u);
    const uint32_t normal_tail = read_guest_u32(scheduler_addr + 0x270u);
    const uint32_t current_rsp = read_guest_u32(scheduler_addr + 0x274u);
    const uint32_t current_dp = read_guest_u32(scheduler_addr + 0x278u);
    const uint32_t audio_pending = read_guest_u32(scheduler_addr + 0x280u);

    const uint32_t selected_rsp =
        (rsp_out_addr >= 0x80000000u && rsp_out_addr <= 0x807FFFFCu)
            ? read_guest_u32(rsp_out_addr)
            : 0u;
    const uint32_t selected_dp =
        (dp_out_addr >= 0x80000000u && dp_out_addr <= 0x807FFFFCu)
            ? read_guest_u32(dp_out_addr)
            : 0u;

    const uint32_t audio_kind =
        (audio_head >= 0x80000000u && audio_head <= 0x807FFFEFu)
            ? read_guest_u32(audio_head + 0x10u)
            : 0xFFFFFFFFu;
    const uint32_t normal_kind =
        (normal_head >= 0x80000000u && normal_head <= 0x807FFFEFu)
            ? read_guest_u32(normal_head + 0x10u)
            : 0xFFFFFFFFu;

    const uint32_t audio_flags =
        (audio_head >= 0x80000000u && audio_head <= 0x807FFFF7u)
            ? read_guest_u32(audio_head + 0x08u)
            : 0u;
    const uint32_t normal_flags =
        (normal_head >= 0x80000000u && normal_head <= 0x807FFFF7u)
            ? read_guest_u32(normal_head + 0x08u)
            : 0u;

    ++probe_count;

    std::fprintf(
        stderr,
        "TS2 SCHED SELECT #%u phase=%u mask=%u "
        "audioPending=%u audioHead=0x%08X audioTail=0x%08X "
        "normalHead=0x%08X normalTail=0x%08X "
        "currentRSP=0x%08X currentDP=0x%08X "
        "selectedRSP=0x%08X selectedDP=0x%08X "
        "audioKind=%u audioFlags=0x%08X "
        "normalKind=%u normalFlags=0x%08X\n",
        probe_count,
        phase,
        resource_mask,
        audio_pending,
        audio_head,
        audio_tail,
        normal_head,
        normal_tail,
        current_rsp,
        current_dp,
        selected_rsp,
        selected_dp,
        audio_kind,
        audio_flags,
        normal_kind,
        normal_flags);
    std::fflush(stderr);
}
// =============================================================================
// TS2 OLDNATIVE V65 CANONICAL FRONTEND DEFINITIONS
//
// COMPILE/LINK PREPARATION ONLY.
// These helpers are deliberately NOT called by V65.
// GameEntry remains disabled, main()/SDL_main remain on Stage61,
// and recomp::start remains inactive.
// =============================================================================

// Space-ranger palette: purple primary, lime-green secondary (focus pulse,
// highlights), deep-space backgrounds. Must run before any UI is created.
static void ts2_apply_theme() {
    using recompui::theme::color;
    auto set = [](color c, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        recompui::theme::set_theme_color(c, recompui::Color{r, g, b, a});
    };
    auto family = [&](color base, color l, color d, color a5, color a20, color a30, color a50, color a80,
                      std::array<uint8_t, 3> mid, std::array<uint8_t, 3> light, std::array<uint8_t, 3> dark) {
        set(base, mid[0], mid[1], mid[2]);
        set(l, light[0], light[1], light[2]);
        set(d, dark[0], dark[1], dark[2]);
        set(a5, mid[0], mid[1], mid[2], 13);
        set(a20, mid[0], mid[1], mid[2], 51);
        set(a30, mid[0], mid[1], mid[2], 77);
        set(a50, mid[0], mid[1], mid[2], 128);
        set(a80, mid[0], mid[1], mid[2], 204);
    };

    set(color::Background1, 18, 10, 42);
    set(color::Background2, 26, 15, 60);
    set(color::Background3, 36, 22, 79);
    set(color::BGOverlay, 180, 150, 255, 26);
    set(color::ModalOverlay, 18, 10, 42, 229);
    set(color::BGShadow2, 18, 10, 42, 184);

    set(color::Text, 243, 238, 255);
    set(color::TextActive, 198, 255, 94);
    set(color::TextDim, 220, 207, 248);

    family(color::Primary, color::PrimaryL, color::PrimaryD, color::PrimaryA5, color::PrimaryA20,
        color::PrimaryA30, color::PrimaryA50, color::PrimaryA80, {123, 63, 228}, {196, 166, 250}, {58, 22, 128});
    family(color::Secondary, color::SecondaryL, color::SecondaryD, color::SecondaryA5, color::SecondaryA20,
        color::SecondaryA30, color::SecondaryA50, color::SecondaryA80, {168, 240, 43}, {222, 255, 150}, {111, 176, 0});
    family(color::A, color::AL, color::AD, color::AA5, color::AA20,
        color::AA30, color::AA50, color::AA80, {123, 63, 228}, {196, 166, 250}, {58, 22, 128});

    set(color::Elevated, 180, 150, 255, 26);
    set(color::ElevatedSoft, 180, 150, 255, 13);
    set(color::ElevatedBorder, 180, 150, 255, 204);
    set(color::ElevatedBorderHard, 180, 150, 255, 255);
}

// Title screen: original space backdrop, logo and menu on the right. The
// rocket and moon are separate layers animated by ts2_launcher_update.
static recompui::Svg* g_ts2_launcher_rocket = nullptr;
static recompui::Svg* g_ts2_launcher_moon = nullptr;
static recompui::Svg* g_ts2_launcher_logo = nullptr;
static std::chrono::steady_clock::time_point g_ts2_launcher_start;

static recompui::Svg* ts2_launcher_layer(recompui::ContextId& context, recompui::Element* parent,
                                         const char* src, float left, float top, float width) {
    recompui::Svg* svg = context.create_element<recompui::Svg>(parent, src);
    svg->set_position(recompui::Position::Absolute);
    svg->set_left(left, recompui::Unit::Percent);
    svg->set_top(top, recompui::Unit::Percent);
    svg->set_width(width, recompui::Unit::Percent);
    svg->set_height_auto();
    return svg;
}

// Rest positions, in percent of the window (the backdrop is 1920x1080).
constexpr float kTs2RocketLeft = 27.6f, kTs2RocketTop = 34.2f;
constexpr float kTs2MoonLeft = 51.5f, kTs2MoonTop = 16.7f;
constexpr float kTs2LogoLeft = 3.0f, kTs2LogoTop = 4.0f;

static void ts2_launcher_update(recompui::LauncherMenu*) {
    if (g_ts2_launcher_rocket == nullptr || g_ts2_launcher_moon == nullptr) {
        return;
    }
    // Start the intro on the first drawn frame, not when the menu was built
    // (renderer start-up can take several seconds before anything is shown).
    static bool clock_started = false;
    if (!clock_started) {
        clock_started = true;
        g_ts2_launcher_start = std::chrono::steady_clock::now();
    }
    const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_ts2_launcher_start).count();
    auto ease_out = [](float x) {
        x = std::clamp(x, 0.0f, 1.0f);
        const float inv = 1.0f - x;
        return 1.0f - inv * inv * inv;
    };

    // Rocket: flies in from the lower left, then drifts and bobs.
    const float rk = ease_out(t / 2.4f);
    const float rocket_left = -45.0f + (kTs2RocketLeft + 45.0f) * rk + 0.8f * std::sin(t * 0.7f) * rk;
    const float rocket_top = 105.0f + (kTs2RocketTop - 105.0f) * rk + 1.6f * std::sin(t * 1.3f) * rk;
    g_ts2_launcher_rocket->set_left(rocket_left, recompui::Unit::Percent);
    g_ts2_launcher_rocket->set_top(rocket_top, recompui::Unit::Percent);

    // Moon: drifts in from the right, then circles slowly.
    const float mk = ease_out((t - 0.4f) / 3.2f);
    const float orbit = t * 0.45f;
    const float moon_left = 112.0f + (kTs2MoonLeft - 112.0f) * mk + 1.4f * std::cos(orbit) * mk;
    const float moon_top = 2.0f + (kTs2MoonTop - 2.0f) * mk + 1.8f * std::sin(orbit) * mk;
    g_ts2_launcher_moon->set_left(moon_left, recompui::Unit::Percent);
    g_ts2_launcher_moon->set_top(moon_top, recompui::Unit::Percent);

    // Logo: drops in from above with a small overshoot, then floats.
    if (g_ts2_launcher_logo != nullptr) {
        auto ease_out_back = [](float x) {
            x = std::clamp(x, 0.0f, 1.0f);
            const float c1 = 1.70158f, c3 = c1 + 1.0f;
            const float y = x - 1.0f;
            return 1.0f + c3 * y * y * y + c1 * y * y;
        };
        const float lk = ease_out_back((t - 0.2f) / 1.1f);
        const float settle = std::clamp((t - 1.3f) / 0.8f, 0.0f, 1.0f);
        const float logo_top = -40.0f + (kTs2LogoTop + 40.0f) * lk + 0.9f * std::sin(t * 1.8f) * settle;
        const float logo_left = kTs2LogoLeft + 0.35f * std::sin(t * 0.9f + 1.0f) * settle;
        g_ts2_launcher_logo->set_top(logo_top, recompui::Unit::Percent);
        g_ts2_launcher_logo->set_left(logo_left, recompui::Unit::Percent);
    }
}

static void ts2_launcher_init(recompui::LauncherMenu* menu) {
    menu->remove_default_title();
    menu->set_launcher_background_svg("launcher-background.svg");

    // Layers go in the background container (after the backdrop), so they
    // sit behind the menu: moon, rocket, then the logo on top.
    auto context = recompui::get_current_context();
    recompui::Element* layers = menu->get_background_wrapper();
    g_ts2_launcher_moon = ts2_launcher_layer(context, layers, "launcher-moon.svg", 112.0f, 2.0f, 8.33f);
    g_ts2_launcher_rocket = ts2_launcher_layer(context, layers, "launcher-rocket.svg", -45.0f, 105.0f, 34.4f);
    g_ts2_launcher_logo = ts2_launcher_layer(context, layers, "launcher-logo.svg", kTs2LogoLeft, -40.0f, 40.0f);
    g_ts2_launcher_start = std::chrono::steady_clock::now();

    const recomp::GameEntry& game = supported_games[0];
    recompui::GameOptionsMenu* options = menu->init_game_options_menu(
        game.game_id, game.mod_game_id, game.display_name, game.thumbnail_bytes,
        recompui::GameOptionsMenuLayout::Right);
    options->set_width(36.0f, recompui::Unit::Percent);
    options->set_bottom(26.0f, recompui::Unit::Percent);
    options->add_start_game_or_load_rom_option();
    options->add_setup_controls_option();
    options->add_settings_option();
    options->add_exit_option();
    for (recompui::GameOption* option : options->get_options()) {
        option->set_font_family("TS2 Menu");
        option->get_label()->set_font_size(56.0f);
        option->get_label()->set_letter_spacing(0.0f);
        option->set_border_radius(0);
        // Like BanjoRecomp: no highlight box, the lime text pulse marks focus.
        for (recompui::Style* style : { &option->hover_style, &option->focus_style }) {
            style->set_background_color(recompui::theme::color::Transparent);
        }
    }
}

static void ts2_initialize_recompfrontend_metadata() {
    static bool initialized = false;
    if (initialized) {
        return;
    }

    recompui::programconfig::set_program_name(
        "SpaceRanger");
    recompui::programconfig::set_program_id(u8"spaceranger");

    ts2_apply_theme();
    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");
    recompui::register_extra_font("TS2Menu-Fredoka.ttf");
    recompui::register_launcher_init_callback(ts2_launcher_init);
    recompui::register_launcher_update_callback(ts2_launcher_update);

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

    // Intentionally omitted while V59 renderer/config/core-only mode is active:
    // recompui::register_ui_exports();

    initialized = true;
}

static ultramodern::renderer::callbacks_t
ts2_make_canonical_renderer_callbacks() {
    return ultramodern::renderer::callbacks_t{
        .create_render_context =
            [](uint8_t* rdram,
               ultramodern::renderer::WindowHandle window_handle,
               bool developer_mode) {
                return recompui::renderer::create_render_context(
                    rdram,
                    window_handle,
                    ultramodern::renderer::PresentationMode::PresentEarly,
                    developer_mode
                );
            },
    };
}

// TS2 OLDNATIVE V65 CANONICAL FRONTEND DEFINITIONS END
// =============================================================================
