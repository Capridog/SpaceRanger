// Input record/replay for automated testing.
//
//   TS2_INPUT_RECORD=path   record controller 1 (every game read) to path
//   TS2_INPUT_REPLAY=path   feed the recording back instead of the pad
//   TS2_REPLAY_EXIT=1       quit the process shortly after the replay ends
//
// Recordings are frame-tagged ("TS2INP2" format): every controller read is
// stored with the game frame it happened in (the display-list frame counter
// from render_support.cpp). The game reads the controller from timer-driven
// threads, so how many reads fall in one game frame depends on timing; a
// replay keyed on read order drifts as soon as a build runs faster or slower.
// Keyed on the frame instead, frame F always sees the reads recorded in
// frame F (in order; extra reads repeat the frame's last value), so a replay
// follows the recorded route on any build.
//
// Old untagged recordings (12-byte records, no header) still replay by read
// order.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <vector>

#include "recompinput/profiles.h"

extern "C" uint64_t ts2_game_frame_serial();

namespace {
struct Poll {
    uint16_t buttons;
    uint16_t pad;
    float x;
    float y;
};
static_assert(sizeof(Poll) == 12, "poll record layout");

struct FramePoll {
    uint32_t frame;
    uint16_t buttons;
    uint16_t pad;
    float x;
    float y;
};
static_assert(sizeof(FramePoll) == 16, "frame poll record layout");

constexpr char Magic[8] = { 'T', 'S', '2', 'I', 'N', 'P', '2', '\0' };

std::mutex replay_mutex;
FILE* record_file = nullptr;
bool initialized = false;
bool replaying = false;
bool frame_tagged = false;

// Untagged (legacy) replay.
std::vector<Poll> replay;
size_t replay_pos = 0;
uint64_t polls_after_end = 0;

// Frame-tagged replay.
std::vector<FramePoll> frame_replay;
uint64_t current_frame = UINT64_MAX;
size_t frame_first = 0;   // first record of current_frame (or the last earlier one)
size_t frame_count = 0;   // records in current_frame
size_t frame_read = 0;    // reads served in current_frame
bool finished_reported = false;

void init_locked() {
    if (initialized) return;
    initialized = true;
    if (const char* path = std::getenv("TS2_INPUT_REPLAY")) {
        if (FILE* f = std::fopen(path, "rb")) {
            char header[8] = {};
            if (std::fread(header, sizeof(header), 1, f) == 1 && std::memcmp(header, Magic, sizeof(Magic)) == 0) {
                FramePoll p{};
                while (std::fread(&p, sizeof(p), 1, f) == 1) frame_replay.push_back(p);
                frame_tagged = true;
                std::fprintf(stderr, "TS2 INPUT: replaying %zu frame-tagged reads (frames 0-%u) from %s\n",
                    frame_replay.size(), frame_replay.empty() ? 0u : frame_replay.back().frame, path);
            }
            else {
                std::fseek(f, 0, SEEK_SET);
                Poll p{};
                while (std::fread(&p, sizeof(p), 1, f) == 1) replay.push_back(p);
                std::fprintf(stderr, "TS2 INPUT: replaying %zu polls (untagged, read order) from %s\n", replay.size(), path);
            }
            std::fclose(f);
            replaying = true;
        }
        else {
            std::fprintf(stderr, "TS2 INPUT: cannot open replay %s\n", path);
        }
    }
    else if (const char* path = std::getenv("TS2_INPUT_RECORD")) {
        record_file = std::fopen(path, "wb");
        if (record_file) std::fwrite(Magic, sizeof(Magic), 1, record_file);
        std::fprintf(stderr, "TS2 INPUT: recording frame-tagged reads to %s (%s)\n", path, record_file ? "ok" : "FAILED");
    }
}

void replay_finished(uint64_t frame) {
    if (finished_reported) return;
    finished_reported = true;
    std::fprintf(stderr, "TS2 INPUT: replay finished at frame %llu\n", (unsigned long long)frame);
    if (std::getenv("TS2_REPLAY_EXIT")) std::_Exit(0);
}

bool frame_replay_read(uint16_t* buttons, float* x, float* y) {
    const uint64_t frame = ts2_game_frame_serial();
    if (frame != current_frame) {
        current_frame = frame;
        frame_read = 0;
        // Records are in frame order: advance to the first record of this frame.
        size_t lo = 0, hi = frame_replay.size();
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (frame_replay[mid].frame < frame) lo = mid + 1; else hi = mid;
        }
        size_t end = lo;
        while (end < frame_replay.size() && frame_replay[end].frame == frame) ++end;
        if (end > lo) {
            frame_first = lo;
            frame_count = end - lo;
        }
        else {
            // No read recorded in this frame: hold the previous state.
            frame_first = lo > 0 ? lo - 1 : 0;
            frame_count = lo > 0 ? 1 : 0;
        }
    }
    if (frame_replay.empty() || frame_count == 0) {
        *buttons = 0; *x = 0.0f; *y = 0.0f;
    }
    else {
        const size_t i = frame_first + (frame_read < frame_count ? frame_read : frame_count - 1);
        ++frame_read;
        *buttons = frame_replay[i].buttons;
        *x = frame_replay[i].x;
        *y = frame_replay[i].y;
    }
    // ~2 s after the last recorded frame.
    if (!frame_replay.empty() && frame > uint64_t(frame_replay.back().frame) + 60) {
        *buttons = 0; *x = 0.0f; *y = 0.0f;
        replay_finished(frame);
    }
    return true;
}
}

extern "C" uint64_t ts2_input_replay_position() {
    std::lock_guard<std::mutex> lock(replay_mutex);
    if (!replaying) return 0;
    return frame_tagged ? (frame_first + frame_read) : replay_pos;
}

bool ts2_get_input(int controller_num, uint16_t* buttons, float* x, float* y) {
    std::lock_guard<std::mutex> lock(replay_mutex);
    init_locked();
    if (replaying && controller_num == 0) {
        if (frame_tagged) return frame_replay_read(buttons, x, y);
        if (replay_pos < replay.size()) {
            const Poll& p = replay[replay_pos++];
            *buttons = p.buttons;
            *x = p.x;
            *y = p.y;
        }
        else {
            *buttons = 0;
            *x = 0.0f;
            *y = 0.0f;
            // ~2 s at 60 polls/s after the recording ends.
            if (++polls_after_end == 120) replay_finished(0);
        }
        return true;
    }
    const bool ok = recompinput::profiles::get_n64_input(controller_num, buttons, x, y);
    if (record_file && controller_num == 0) {
        const FramePoll p{ uint32_t(ts2_game_frame_serial()), ok ? *buttons : uint16_t(0), 0, ok ? *x : 0.0f, ok ? *y : 0.0f };
        std::fwrite(&p, sizeof(p), 1, record_file);
        std::fflush(record_file);
        // Time marks let snapshots (named by epoch seconds) be located in the recording.
        static uint64_t recorded = 0;
        if ((++recorded % 300) == 0) {
            std::fprintf(stderr, "TS2 INPUT: read %llu frame %u at %lld\n", (unsigned long long)recorded, p.frame, (long long)std::time(nullptr));
        }
    }
    return ok;
}
