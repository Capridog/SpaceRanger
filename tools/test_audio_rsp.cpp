#include "bridge_audio.h"
#include "librecomp/rsp.hpp"
#include <SDL.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

RspExitReason ts2_aspMain(uint8_t*, uint32_t);
static RspUcodeFunc* select_audio(const OSTask*) { return ts2_aspMain; }
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() try {
    std::ifstream input(TS2_TEST_ROM, std::ios::binary);
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(input)), {});
    require(rom.size() >= 0xA4EE0, "ROM missing/truncated");
    std::vector<uint8_t> memory(8 * 1024 * 1024, 0xA5);
    uint8_t* rdram = memory.data();
    for (uint32_t i = 0; i < 0xF80; ++i)
        memory[(0xA8F60 + i) ^ 3] = rom[0xA3F60 + i];
    for (uint32_t i = 0; i < 64; ++i)
        memory[(0x101000 + i) ^ 3] = static_cast<uint8_t>(i * 37 + 11);
    // ABI1: set input/output/count, load PCM, save, clear, save silence.
    const uint32_t commands[] = {
        0x08000000, 0x00000040,
        0x04000000, 0x00101000,
        0x06000000, 0x00102000,
        0x02000000, 0x00000040,
        0x06000000, 0x00102100,
    };
    std::memcpy(rdram + 0x100000, commands, sizeof(commands));
    OSTask task{};
    task.t.type = 2;
    task.t.ucode = 0x80088B80;
    task.t.ucode_data = 0x800A8F60;
    task.t.data_ptr = 0x80100000;
    task.t.data_size = sizeof(commands);
    recomp::rsp::constants_init();
    recomp::rsp::set_callbacks({select_audio});
    require(recomp::rsp::run_task(rdram, &task), "RSP task failed");
    require(std::memcmp(rdram + 0x101000, rdram + 0x102000, 64) == 0,
        "RSP load/save corrupted PCM");
    for (size_t i = 0; i < 64; ++i)
        require(memory[0x102100 + i] == 0, "RSP clear/save did not produce silence");
    std::cout << "PASS: real audio microcode dispatch, DMA load/save, vector clear\n";

    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    require(ts2::audio::initialize(), "SDL dummy device failed");
    std::vector<int16_t> pcm(9600);
    for (size_t i = 0; i < pcm.size(); i += 2) {
        pcm[i] = -16384; pcm[i + 1] = 8192; // N64 word-swapped R,L.
    }
    ts2::audio::set_frequency(32000);
    ts2::audio::queue_samples(pcm.data(), pcm.size());
    const auto first = ts2::audio::get_frames_remaining();
    require(first > 1500 && first <= 4000, "AI backlog must use source-rate frames after playback reserve");
    SDL_Delay(40);
    require(ts2::audio::get_frames_remaining() < first, "SDL queue does not drain");
    ts2::audio::set_frequency(22050);
    ts2::audio::queue_samples(pcm.data(), pcm.size());
    require(ts2::audio::get_frames_remaining() < 10000, "rate transition backlog invalid");
    ts2::audio::shutdown();
    require(ts2::audio::get_frames_remaining() == 0, "shutdown leaves stale AI backlog");
    require(ts2::audio::initialize(), "SDL device reopen failed");
    ts2::audio::shutdown();
    std::cout << "PASS: SDL queue playback, source-rate backlog, live rate change, reopen\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    ts2::audio::shutdown();
    return 1;
}
