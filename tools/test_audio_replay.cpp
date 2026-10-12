#include "librecomp/rsp.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

RspExitReason ts2_aspMain(uint8_t*, uint32_t);
RspExitReason ts2_aspMain_scalar(uint8_t*, uint32_t);
static RspUcodeFunc* selected;
static RspUcodeFunc* select_audio(const OSTask*) { return selected; }
int main(int argc, char** argv) {
    const char* snapshot_path = argc > 1
        ? argv[1]
        : "C:/SpaceRanger/docs/audio-snapshot.bin";
    const char* task_path = argc > 2
        ? argv[2]
        : "C:/SpaceRanger/docs/audio-task.bin";
    std::ifstream input(snapshot_path, std::ios::binary);
    std::vector<uint8_t> normal((std::istreambuf_iterator<char>(input)), {});
    if (normal.size() != 0x800000) return 2;
    auto scalar = normal;
    OSTask task{};
    std::ifstream(task_path, std::ios::binary).read(reinterpret_cast<char*>(&task), sizeof(task));
    if (task.t.type != 2 || task.t.data_size == 0) return 3;
    recomp::rsp::constants_init();
    recomp::rsp::set_callbacks({select_audio});
    selected = ts2_aspMain;
    if (!recomp::rsp::run_task(normal.data(), &task)) return 4;
    selected = ts2_aspMain_scalar;
    if (!recomp::rsp::run_task(scalar.data(), &task)) return 5;
    size_t changed = 0;
    for (size_t i = 0; i < normal.size(); ++i) if (normal[i] != scalar[i]) ++changed;
    std::ofstream("C:/SpaceRanger/docs/audio-replay-simd.bin", std::ios::binary).write(reinterpret_cast<char*>(normal.data()), normal.size());
    std::ofstream("C:/SpaceRanger/docs/audio-replay-scalar.bin", std::ios::binary).write(reinterpret_cast<char*>(scalar.data()), scalar.size());
    std::cout << "SIMD/scalar replay differing bytes: " << changed << '\n';
    return changed ? 1 : 0;
}
