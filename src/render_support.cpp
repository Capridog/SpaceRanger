#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <fstream>
#include <vector>
#include <iterator>
#include "recomp.h"

namespace {
std::atomic<int> horizontal_margin{0};
bool visibility_diagnostics() {
    static const bool enabled = std::getenv("TS2_VISIBILITY_DIAGNOSTICS") != nullptr;
    return enabled;
}
struct VisibilityStats {
    uint64_t frames = 0;
    uint64_t allocation_checks = 0, allocation_failures = 0;
    uint64_t expanded_polygons = 0, outside_polygons = 0;
    uint64_t recovered_spheres = 0;
};
thread_local VisibilityStats visibility_stats;

// The game builds each frame's display list in one fixed buffer. func_800136C4
// rejects a draw when (cursor - base) / 16 plus the request would reach 0x2600.
// Scenery is submitted before actors, so extended scenery distance can fill
// the buffer and make the game silently skip Buzz (his shadow still draws).
// The extension is therefore adaptive: it backs off whenever the scenery
// passes leave too little room, and returns to exactly the original distance
// (scale 1.0) when needed.
constexpr uint32_t display_list_cursor = 0x800BAC48u;
constexpr uint32_t display_list_base = 0x801E3E14u;
constexpr int32_t display_list_capacity = 0x2600;
struct DistanceControl {
    float scale = 1.0f;
    float ceiling = 0.0f;   // 0 = not yet initialised (uses the configured maximum)
    uint32_t calm = 0;      // world passes since the last back-off
    int32_t peak = 0;
    uint32_t failures = 0;
};
// Hysteresis: after a back-off the range may regrow only up to the distance
// that fitted, so it cannot saw-tooth (grow, overflow, drop, grow...) and pop
// scenery in and out every second. The ceiling is raised again only after a
// long stretch with plenty of headroom.
constexpr uint32_t ceiling_relax_passes = 600;
constexpr float ceiling_relax_usage = 0.40f;
thread_local DistanceControl distance_control;
float max_scenery_scale() {
    static const float value = [] {
        const char* text = std::getenv("TS2_SCENERY_DISTANCE_SCALE");
        const float parsed = text ? static_cast<float>(std::atof(text)) : 2.0f;
        return std::isfinite(parsed) ? std::clamp(parsed, 1.0f, 2.0f) : 2.0f;
    }();
    return value;
}
}

extern "C" void ts2_note_mesh_allocation(int32_t result) {
    distance_control.failures += result != 0;
    if (!visibility_diagnostics()) return;
    ++visibility_stats.allocation_checks;
    visibility_stats.allocation_failures += result != 0;
}

static void report_visibility(uint8_t* rdram) {
    if (!visibility_diagnostics()) return;
    auto& stats = visibility_stats;
    // One bounded snapshot per 60 scenery passes, not a trace per polygon.
    if (++stats.frames % 60 != 0) return;
    const int room = MEM_W(0, ADD32(0x801C8E94u, 0));
    const int count = MEM_W(0, ADD32(0x801E8CBCu, 0));
    std::fprintf(stderr, "TS2 visibility: pass=%llu margin=%d room=%d count=%d mode=%d mesh_x=%d..%d alloc=%llu failed=%llu expanded=%llu outside=%llu sphere_caps=%llu\n",
        static_cast<unsigned long long>(stats.frames), horizontal_margin.load(std::memory_order_relaxed), room, count,
        MEM_W(0, ADD32(0x801CE108u, 0)), MEM_W(0, ADD32(0x801DB430u, 0)), MEM_W(0, ADD32(0x801DB9ACu, 0)),
        static_cast<unsigned long long>(stats.allocation_checks), static_cast<unsigned long long>(stats.allocation_failures),
        static_cast<unsigned long long>(stats.expanded_polygons), static_cast<unsigned long long>(stats.outside_polygons), static_cast<unsigned long long>(stats.recovered_spheres));
    if (count > 0 && count <= 64) {
        for (int i = 0; i < count; ++i) {
            const gpr row = ADD32(0x800B04B0u + i * 0x34u, 0);
            if (i != room && !MEM_BU(0, row) && !MEM_BU(1, row)) continue;
            std::fprintf(stderr, "TS2 visibility room: id=%d flags=%u/%u/%u bounds=%d,%d,%d,%d slopes=%d,%d\n",
                i, MEM_BU(0, row), MEM_BU(1, row), MEM_BU(3, row), MEM_H(4, row), MEM_H(6, row), MEM_H(8, row), MEM_H(10, row), MEM_H(12, row), MEM_H(14, row));
        }
    }
    stats.allocation_checks = stats.allocation_failures = stats.expanded_polygons = stats.outside_polygons = 0;
    stats.recovered_spheres = 0;
}

// Published by RT64 after computing the actual output/source aspect ratio.
extern "C" void ts2_set_render_aspect(float scale) {
    // Guard band: interpolated frames show the scene from a camera between two
    // native ones, so geometry the game culled at the native screen edge would
    // pop in and out at the edges. Keep 40 guest pixels of extra geometry on
    // each side (TS2_INTERP_GUARD overrides; 0 disables).
    static const int guard = [] {
        const char* v = std::getenv("TS2_INTERP_GUARD");
        return v ? std::clamp(std::atoi(v), 0, 512) : 40;
    }();
    const int margin = (std::isfinite(scale) ?
        std::clamp(static_cast<int>(std::ceil(160.0f * (scale - 1.0f))), 0, 4096) : 0) + guard;
    if (horizontal_margin.exchange(margin, std::memory_order_relaxed) != margin) {
        std::fprintf(stderr, "TS2 RENDER: aspect %.4f, guest horizontal bounds %d..%d\n", scale, -margin, 320 + margin);
    }
}

extern "C" int ts2_wide_margin() {
    return horizontal_margin.load(std::memory_order_relaxed);
}

// Vertical guard band for interpolated frames (camera pitch / bob): geometry the
// game culls at the top/bottom screen edge otherwise shows as black gaps along
// the edge on in-between frames. TS2_INTERP_VGUARD overrides (0 disables).
static int ts2_vertical_guard() {
    static const int guard = [] {
        const char* v = std::getenv("TS2_INTERP_VGUARD");
        return v ? std::clamp(std::atoi(v), 0, 256) : 32;
    }();
    return guard;
}

// ---------------------------------------------------------------------------
// Render provenance (RaceWave46-style): entry hooks in every display-list
// emitter record which game function was writing at which buffer cursor.
// func_8000DEA0 (the frame builder) starts a new frame. The F9 capture saves
// the recent frames so each RT64 draw call's DL address can be attributed.
// ---------------------------------------------------------------------------
extern "C" bool ts2_request_frame_capture();
extern "C" uint64_t ts2_input_replay_position();
namespace {
// Candidate display-list write pointers (globals the emitters use most),
// recorded as value and one dereference so a Gfx** head is also covered.
constexpr uint32_t kCursorCandidates[] = {0x800BAC48u, 0x800BAC2Cu, 0x800BAC3Cu, 0x801C8E84u,
    0x801C8E10u, 0x801C8E50u, 0x801C8E98u, 0x801C937Cu, 0x801C8A78u, 0x801C8A7Cu, 0x801C8A70u,
    0x801C8A28u, 0x801C8E94u, 0x801C8E1Cu, 0x801C8E74u, 0x801C9380u};
constexpr size_t kCandidates = sizeof(kCursorCandidates) / sizeof(kCursorCandidates[0]);
struct EmitterMark { uint32_t function; int32_t model; uint32_t values[kCandidates * 2]; };
// Render object (func_80059C14 iteration) being drawn, published by the actor hooks.
std::atomic<int32_t> current_render_model{-1};
constexpr size_t kMarksPerFrame = 4096, kFrames = 8;
struct EmitterFrame { uint64_t serial = 0; uint32_t count = 0; EmitterMark marks[kMarksPerFrame]; };
std::mutex emitter_mutex;
EmitterFrame emitter_frames[kFrames];
uint64_t emitter_serial = 0;
static std::atomic<uint64_t> game_frame_serial{0};
// Completed frames, sorted by display-list head (0x801C8E84) for live lookup.
constexpr size_t kHeadSlot = 3 * 2;  // kCursorCandidates[3] = 0x801C8E84, raw value
struct SortedMark { uint32_t addr, fn; int32_t model; };
struct SortedFrame {
    uint64_t serial = 0; uint32_t start = 0; std::vector<SortedMark> marks;
    // Vertex-pool cursor (0x800BAC48) -> render object that wrote from there on.
    std::vector<std::pair<uint32_t, int32_t>> vertices;
};
SortedFrame completed_frames[4];
// Start of the display list RT64 is interpreting (set per task by RT64).
std::atomic<uint32_t> interpreting_task_start{0};
void finalize_frame(const EmitterFrame& frame) {
    auto& out = completed_frames[frame.serial % 4];
    out.serial = frame.serial;
    out.start = 0xFFFFFFFFu;
    out.marks.clear();
    out.vertices.clear();
    // Shared helpers (texture loader, DL allocator) are called from inside
    // emitters; their marks inherit the calling emitter as owner.
    uint32_t owner = 0;
    int32_t mesh_key = -1;  // 8005D69C mesh (a0), inherited by its children
    for (uint32_t i = 0; i < frame.count; ++i) {
        const uint32_t fn = frame.marks[i].function;
        if (fn != 0x800120ECu && fn != 0x800136C4u) owner = fn;
        const uint32_t pool = frame.marks[i].values[0];
        // Vertex source key: the render object when inside the object loop,
        // otherwise the writing function (0x1000000 | vram) so CPU-transformed
        // passes outside the loop (e.g. 800112F4, 8005D69C) are told apart too.
        if (pool >= 0x80000000u && pool < 0x80800000u) {
            const uint32_t writer = owner ? owner : fn;
            if (fn == 0x8005D69Cu) mesh_key = frame.marks[i].model;
            else if (fn != 0x80087EA0u && fn != 0x800120ECu && fn != 0x800136C4u) mesh_key = -1;
            const int32_t key = frame.marks[i].model >= 0 ? frame.marks[i].model
                : mesh_key >= 0 ? mesh_key : (int32_t)(0x1000000u | (writer & 0xFFFFFFu));
            out.vertices.emplace_back(pool & 0x7FFFFFu, key);
        }
        const uint32_t head = frame.marks[i].values[kHeadSlot];
        if (head >= 0x80000000u && head < 0x80800000u) {
            out.marks.push_back({head & 0x7FFFFFu, owner ? owner : fn, frame.marks[i].model});
            // The frame builder/DL init marks still hold the previous head.
            if (fn != 0x8000DEA0u && fn != 0x8000D1B8u) out.start = std::min(out.start, head & 0x7FFFFFu);
        }
    }
    std::stable_sort(out.marks.begin(), out.marks.end(),
        [](const auto& a, const auto& b) { return a.addr < b.addr; });
    std::stable_sort(out.vertices.begin(), out.vertices.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
}
}

extern "C" void ts2_set_display_list_task(uint32_t rdram_offset) {
    interpreting_task_start.store(rdram_offset & 0x7FFFFFu, std::memory_order_relaxed);
}

// Live provenance for RT64: the game function that was writing the display
// list at this RDRAM offset, from the newest completed frame covering it.
static const SortedMark* find_mark(const SortedFrame& frame, uint32_t rdram_offset) {
    auto it = std::upper_bound(frame.marks.begin(), frame.marks.end(), rdram_offset,
        [](uint32_t value, const SortedMark& m) { return value < m.addr; });
    return it == frame.marks.begin() ? nullptr : &*std::prev(it);
}

extern "C" void ts2_set_render_object(uint32_t object, int32_t model) {
    current_render_model.store(object ? model : -1, std::memory_order_relaxed);
}

static const SortedMark* provenance_lookup(uint32_t rdram_offset);
extern "C" uint32_t ts2_emitter_for_dl(uint32_t rdram_offset) {
    std::lock_guard<std::mutex> lock(emitter_mutex);
    const SortedMark* m = provenance_lookup(rdram_offset);
    return m ? m->fn : 0;
}

// Render-object model (TS2 object +0x14) that owns this DL offset, -1 if none.
extern "C" int32_t ts2_model_for_dl(uint32_t rdram_offset) {
    std::lock_guard<std::mutex> lock(emitter_mutex);
    const SortedMark* m = provenance_lookup(rdram_offset);
    return m ? m->model : -1;
}

static const SortedFrame* provenance_frame(uint32_t rdram_offset, bool vertexLookup);
static const SortedMark* provenance_lookup(uint32_t rdram_offset) {
    const SortedFrame* frame = provenance_frame(rdram_offset, false);
    return frame ? find_mark(*frame, rdram_offset) : nullptr;
}

// Render object whose vertices were written at this RDRAM offset (vertex
// pool). The game batches all objects' triangles by texture afterwards, so
// the vertex source is the only per-object key RT64 sees. -1 if none.
extern "C" int32_t ts2_object_for_vertices(uint32_t rdram_offset) {
    std::lock_guard<std::mutex> lock(emitter_mutex);
    const SortedFrame* frame = provenance_frame(rdram_offset, true);
    if (!frame || frame->vertices.empty()) return -1;
    auto it = std::upper_bound(frame->vertices.begin(), frame->vertices.end(), rdram_offset,
        [](uint32_t value, const std::pair<uint32_t, int32_t>& v) { return value < v.first; });
    return it == frame->vertices.begin() ? -1 : std::prev(it)->second;
}

static const SortedFrame* provenance_frame(uint32_t rdram_offset, bool vertexLookup) {
    // The frame still being built counts too (RT64 may interpret frame N
    // before N+1 begins), but only strictly inside the range it has written.
    static SortedFrame live;
    static uint32_t live_count = 0;
    const auto& building = emitter_frames[emitter_serial % kFrames];
    if (emitter_serial != 0 && (live.serial != building.serial || live_count != building.count)) {
        live_count = building.count;
        finalize_frame(building);
        live = completed_frames[building.serial % 4];
        completed_frames[building.serial % 4].marks.clear();
        completed_frames[building.serial % 4].serial = 0;
    }
    const SortedFrame* best = nullptr;
    // Preferred: the newest frame whose display list starts where the task
    // RT64 is interpreting starts (frames alternate between two buffers, so
    // this cannot pick up the next frame the game is already building).
    const uint32_t task = interpreting_task_start.load(std::memory_order_relaxed);
    if (task != 0) {
        auto starts_here = [&](const SortedFrame& f) {
            return !f.marks.empty() && f.start != 0xFFFFFFFFu &&
                (f.start > task ? f.start - task : task - f.start) < 0x2000u && (vertexLookup || rdram_offset >= f.start);
        };
        if (starts_here(live)) best = &live;
        for (const auto& frame : completed_frames)
            if (starts_here(frame) && (!best || frame.serial > best->serial)) best = &frame;
        static uint64_t lookups = 0, matched = 0;
        matched += best != nullptr;
        if (++lookups == 20000) {
            std::fprintf(stderr, "TS2 provenance: task-start matched %llu/20000 lookups (task %06X, live start %06X)\n",
                static_cast<unsigned long long>(matched), task, live.start);
        }
        if (best || vertexLookup) return best;
    }
    if (!live.marks.empty() && rdram_offset >= live.marks.front().addr && rdram_offset < live.marks.back().addr)
        best = &live;
    for (const auto& frame : completed_frames) {
        if (frame.marks.empty() || rdram_offset < frame.marks.front().addr ||
            rdram_offset > frame.marks.back().addr + 0x4000u) continue;
        if (!best || frame.serial > best->serial) best = &frame;
    }
    return best;
}

extern "C" void ts2_trace_emitter(uint8_t* rdram, uint32_t function_vram);

// func_8005D69C (CPU-transformed mesh renderer) is called outside the object
// loop for the room and the first-person arm. Its a0 (mesh pointer) is the
// only per-mesh identity there, so the mark carries 0x2000000 | mesh.
extern "C" void ts2_trace_mesh(uint8_t* rdram, uint32_t function_vram, uint32_t mesh) {
    const int32_t previous = current_render_model.load(std::memory_order_relaxed);
    if (previous < 0 && mesh >= 0x80000000u && mesh < 0x80800000u)
        current_render_model.store((int32_t)(0x2000000u | (mesh & 0x7FFFFFu)), std::memory_order_relaxed);
    ts2_trace_emitter(rdram, function_vram);
    current_render_model.store(previous, std::memory_order_relaxed);
}

extern "C" void ts2_trace_emitter(uint8_t* rdram, uint32_t function_vram) {
    std::lock_guard<std::mutex> lock(emitter_mutex);
    if (function_vram == 0x8000DEA0u || emitter_serial == 0) {
        if (emitter_serial != 0) finalize_frame(emitter_frames[emitter_serial % kFrames]);
        ++emitter_serial;
        game_frame_serial.store(emitter_serial, std::memory_order_relaxed);
        // TS2_AUTO_CAPTURE_FRAME=N[,M,...]: request F9-style captures at these
        // frames (lets provenance and smoothness be checked without a player).
        static const std::vector<uint64_t> auto_capture = [] {
            std::vector<uint64_t> frames;
            if (const char* v = std::getenv("TS2_AUTO_CAPTURE_FRAME")) {
                const char* p = v;
                while (*p) {
                    char* end = nullptr;
                    const uint64_t n = std::strtoull(p, &end, 10);
                    if (end == p) break;
                    if (n) frames.push_back(n);
                    p = (*end == ',') ? end + 1 : end;
                }
            }
            return frames;
        }();
        for (uint64_t n : auto_capture)
            if (emitter_serial == n) ts2_request_frame_capture();
        // Frame <-> replay poll mapping, so replay inputs can be cut at a known
        // gameplay moment (TS2_INPUT_REPLAY only).
        if (emitter_serial % 150 == 0) {
            const uint64_t poll = ts2_input_replay_position();
            if (poll) std::fprintf(stderr, "TS2 FRAMEPOS frame=%llu poll=%llu\n", (unsigned long long)emitter_serial, (unsigned long long)poll);
        }
        auto& frame = emitter_frames[emitter_serial % kFrames];
        frame.serial = emitter_serial;
        frame.count = 0;
    }
    auto& frame = emitter_frames[emitter_serial % kFrames];
    if (frame.count >= kMarksPerFrame) return;
    auto& mark = frame.marks[frame.count++];
    mark.function = function_vram;
    mark.model = current_render_model.load(std::memory_order_relaxed);
    for (size_t i = 0; i < kCandidates; ++i) {
        const uint32_t value = static_cast<uint32_t>(MEM_W(0, ADD32(kCursorCandidates[i], 0)));
        mark.values[i * 2] = value;
        mark.values[i * 2 + 1] = (value >= 0x80000000u && value < 0x807FFFFCu && (value & 3) == 0) ?
            static_cast<uint32_t>(MEM_W(0, ADD32(value, 0))) : 0;
    }
}

extern "C" void ts2_write_emitter_history(const char* path) {
    std::lock_guard<std::mutex> lock(emitter_mutex);
    std::ofstream out(path, std::ios::binary);
    out << "{\"candidates\":[";
    for (size_t i = 0; i < kCandidates; ++i) out << (i ? "," : "") << kCursorCandidates[i];
    out << "],\"frames\":[";
    bool firstFrame = true;
    constexpr uint64_t kWrittenFrames = 3;
    for (uint64_t s = emitter_serial >= kWrittenFrames ? emitter_serial - kWrittenFrames + 1 : 1; s <= emitter_serial; ++s) {
        const auto& frame = emitter_frames[s % kFrames];
        if (frame.serial != s) continue;
        out << (firstFrame ? "" : ",") << "{\"serial\":" << s << ",\"marks\":[";
        firstFrame = false;
        for (uint32_t i = 0; i < frame.count; ++i) {
            const auto& m = frame.marks[i];
            out << (i ? "," : "") << "[" << m.function;
            for (uint32_t v : m.values) out << "," << v;
            out << "]";
        }
        out << "]}";
    }
    out << "]}";
}

// ---------------------------------------------------------------------------
// Full draw distance (default; TS2_DRAW_DISTANCE=original or =extended opts out)
//
// The game culls at three kinds of distance: whole objects against the scenery
// limits (0x801CED64 / 0x801E31B8 / 0x800BAB20), billboards against the first
// one, and every polygon against the per-pass polygon limit 0x801CF094, and
// actors (func_8000AC34) against their own +0x42 value. Hooks after each of those reads pass
// the value through ts2_draw_limit(); guest memory is never rewritten, so
// gameplay code that reads the same globals is unaffected.
//
// The ceiling is the engine's own: CPU-transformed vertices are stored as
// 16-bit view-space coordinates, and func_80087EA0 itself rejects polygons at
// depth/4 >= 7000. 5000 keeps depth under 20000 and the widescreen edge x under
// 32767, so nothing can wrap.
//
// Drawing everything overflows the game's fixed per-frame buffers, which only
// the old 0x2600-entry budget (func_800136C4) protected. In full mode they move
// to the otherwise unused upper 4 MiB of RDRAM (the game never reads osMemSize
// and addresses nothing above 0x80400000):
//   0x80400000  display lists + texture-bucket chunks, 2 x 0x80000 (was 0x1C000)
//   0x80500000  vertex pool, 2 x 0x100000 (0x10000 entries, was 0x2600)
//   0x80700000  matrix pool, 4 x 0x10000 (1024 matrices each, was 300)
//   0x80740000  instanced-mesh table + nodes (was 140 nodes at 0x801DC5D0)
//   0x807F0000  canary, must stay zero
// ---------------------------------------------------------------------------
namespace {
enum class DrawMode { Full, Extended, Original };
DrawMode draw_mode() {
    static const DrawMode mode = [] {
        const char* v = std::getenv("TS2_DRAW_DISTANCE");
        if (v && std::strcmp(v, "original") == 0) return DrawMode::Original;
        if (v && std::strcmp(v, "extended") == 0) return DrawMode::Extended;
        return DrawMode::Full;
    }();
    return mode;
}
constexpr int32_t kFullDrawLimit = 5000;
constexpr uint32_t kDlBuffers[2] = {0x80400000u, 0x80480000u};
constexpr int32_t kDlSize = 0x80000;
constexpr uint32_t kVertexBuffers[2] = {0x80500000u, 0x80600000u};
constexpr int32_t kVertexCapacity = 0x100000 / 16;
constexpr uint32_t kMatrixBase = 0x80700000u;
constexpr uint32_t kMatrixStride = 0x10000u;
constexpr uint32_t kOriginalMatrixStride = 0x4B00u;
constexpr uint32_t kInstanceTable = 0x80740000u;
constexpr uint32_t kCanary = 0x807F0000u, kCanaryEnd = 0x80800000u;
// func_8000D1B8 reads this word via lui 0x800C / lw -0x78F4.
constexpr uint32_t kMatrixBasePointer = 0x800B870Cu;
bool buffers_relocated() { return draw_mode() == DrawMode::Full; }

struct DrawStats {
    uint64_t frames = 0;
    int32_t vertices = 0, display_list = 0, chunks = 0, matrices = 0, nodes = 0;
    uint32_t actors = 0, hidden_actors = 0, room_hidden = 0;
};
DrawStats draw_stats;
FILE* draw_stats_file() {
    static FILE* file = [] {
        const char* p = std::getenv("TS2_DRAWSTATS");
        return p ? std::fopen(p, "w") : nullptr;
    }();
    return file;
}
bool region_is_zero(uint8_t* rdram, uint32_t begin, uint32_t end) {
    for (uint32_t a = begin; a < end; a += 4)
        if (MEM_W(0, ADD32(a, 0)) != 0) return false;
    return true;
}
}

extern "C" int32_t ts2_draw_limit(int32_t value) {
    if (draw_mode() != DrawMode::Full || value <= 0) return value;
    return std::max(value, kFullDrawLimit);
}

// func_8000D1B8 (per-frame buffer setup), after the display-list base for this
// frame's parity is loaded into a0. Also records the previous frame's usage.
extern "C" uint32_t ts2_dl_buffer(uint8_t* rdram, uint32_t original) {
    if (!buffers_relocated()) return original;
    static bool checked = false;
    if (!checked) {
        checked = true;
        if (!region_is_zero(rdram, 0x80400000u, kCanaryEnd))
            std::fprintf(stderr, "TS2 draw distance: WARNING upper RDRAM was not empty before relocation\n");
    }
    // Previous frame's peak usage (its buffers are still described by the globals).
    if (FILE* f = draw_stats_file()) {
        auto& s = draw_stats;
        const uint32_t dl_base = MEM_W(0, ADD32(0x801CCE50u, 0));
        const int32_t dl = int32_t(MEM_W(0, ADD32(0x801C8E84u, 0)) - dl_base);
        const int32_t chunks = int32_t(dl_base + kDlSize - MEM_W(0, ADD32(0x800B86FCu, 0)));
        const int32_t vertices = int32_t(MEM_W(0, ADD32(0x800BAC48u, 0)) - MEM_W(0, ADD32(0x801E3E14u, 0))) / 16;
        const int32_t matrices = int32_t(MEM_W(0, ADD32(0x800BAB1Cu, 0)) - MEM_W(0, ADD32(0x801C8E9Cu, 0)) - 0x100) / 64;
        const int32_t nodes = MEM_W(0, ADD32(0x801DBCB0u, 0)) / 2;
        s.display_list = std::max(s.display_list, dl);
        s.chunks = std::max(s.chunks, chunks);
        s.vertices = std::max(s.vertices, vertices);
        s.matrices = std::max(s.matrices, matrices);
        s.nodes = std::max(s.nodes, nodes);
        if (++s.frames % 60 == 0) {
            const bool canary = region_is_zero(rdram, kCanary, kCanaryEnd);
            std::fprintf(f, "frame %llu dl %d+%d/%d vtx %d/%d mtx %d/%u nodes %d actors %u hidden %u roomhidden %u canary %s\n",
                static_cast<unsigned long long>(s.frames), s.display_list, s.chunks, kDlSize, s.vertices, kVertexCapacity,
                s.matrices, kMatrixStride / 64, s.nodes, s.actors, s.hidden_actors, s.room_hidden, canary ? "ok" : "DIRTY");
            std::fflush(f);
            s = DrawStats{s.frames};
        }
    }
    MEM_W(0, ADD32(kMatrixBasePointer, 0)) = kMatrixBase;
    return kDlBuffers[MEM_W(0, ADD32(0x801E3174u, 0)) & 1];
}

extern "C" int32_t ts2_dl_size(uint32_t base, int32_t original) {
    if (!buffers_relocated()) return original;
    return (base == kDlBuffers[0] || base == kDlBuffers[1]) ? kDlSize : original;
}

extern "C" uint32_t ts2_vertex_buffer(uint8_t* rdram, uint32_t original) {
    if (!buffers_relocated()) return original;
    return kVertexBuffers[MEM_W(0, ADD32(0x801E3174u, 0)) & 1];
}

extern "C" int32_t ts2_vertex_capacity(int32_t original) {
    return buffers_relocated() ? kVertexCapacity : original;
}

// v0 = *(0x800B870C) + k * 0x4B00 for the frame's matrix slot k (0..3).
extern "C" uint32_t ts2_matrix_buffer(uint32_t original) {
    if (!buffers_relocated()) return original;
    const uint32_t slot = (original - kMatrixBase) / kOriginalMatrixStride;
    return kMatrixBase + std::min(slot, 3u) * kMatrixStride;
}

extern "C" uint32_t ts2_instance_table(uint32_t original) {
    return buffers_relocated() ? kInstanceTable : original;
}

// func_80059C14 per actor: actor+0x40 bit 0 is set by func_8000AC34 when the
// actor is inside the view (0x2000 marks an actor whose room is not visible).
extern "C" void ts2_note_actor(uint8_t* rdram, uint32_t actor) {
    if (!draw_stats_file() || actor < 0x80000000u || actor >= 0x80800000u) return;
    const uint32_t flags = MEM_HU(0x40, ADD32(actor, 0));
    ++draw_stats.actors;
    if (flags & 1) return;
    if (flags & 0x2000) ++draw_stats.room_hidden;
    else ++draw_stats.hidden_actors;
}

// ---------------------------------------------------------------------------
// Rooms. The game only draws objects and actors in rooms its portal
// traversal (func_80052488) marked visible (room byte 0/1). The traversal is
// coarse: an object standing in a neighbouring room (the Game Box on Andy's
// desk, the Tin Robot in the attic) vanishes whenever the connecting portal
// leaves the view, although the object itself is in plain sight. In full mode
// every room's objects and actors are submitted and the z-buffer does the
// occlusion. Room geometry itself still follows the portal traversal.
// The per-frame object lists (0x801CE560, 0x801DBDD0: ~200 and 140 entries)
// are too small for a whole level, so they move to 0x80760000 / 0x80770000.
// ---------------------------------------------------------------------------
namespace {
constexpr uint32_t kRoomTable = 0x800B04B0u;
constexpr uint32_t kSmallObjectList = 0x80760000u, kLargeObjectList = 0x80770000u;
bool room_visible(uint8_t* rdram, uint32_t row_offset) {
    const gpr row = ADD32(kRoomTable + row_offset, 0);
    return MEM_BU(0, row) != 0 || MEM_BU(1, row) != 0;
}
}

extern "C" int32_t ts2_room_selected(int32_t visible) {
    return draw_mode() == DrawMode::Full ? 1 : visible;
}

// func_80054CDC takes the record count from the room of the *first record* at
// the cursor, so selecting a room with no records of its own would consume the
// next room's records and run past the end of the list. Only rooms that own
// records are forced in (func_800550DC already checks this itself).
extern "C" int32_t ts2_room_selected_records(uint8_t* rdram, uint32_t row_offset, int32_t visible) {
    if (draw_mode() != DrawMode::Full || visible) return visible;
    return MEM_H(0, ADD32(kRoomTable + row_offset + 0x10u, 0)) > 0 ? 1 : 0;
}

// Room byte 3 selects the room's portal cone instead of the camera frustum.
// A room that is not visible has no valid cone; widescreen always uses the
// camera frustum (as func_80054CDC's hook already does).
extern "C" int32_t ts2_room_bounded(uint8_t* rdram, uint32_t row_offset, int32_t bounded) {
    if (ts2_wide_margin()) return 0;
    if (draw_mode() == DrawMode::Full && !room_visible(rdram, row_offset)) return 0;
    return bounded;
}

extern "C" uint32_t ts2_object_list(uint32_t original) {
    if (!buffers_relocated()) return original;
    if (original == 0x801CE560u) return kSmallObjectList;
    if (original == 0x801DBDD0u) return kLargeObjectList;
    return original;
}

// func_8006984C loads the object's room screen rectangle: every polygon of the
// room's objects is clipped to the portal it was seen through. Actors
// (func_80059C14) are drawn afterwards and inherit whichever rectangle the last
// object left behind, which is how the attic's Tin Robot and tokens vanished
// with the camera angle. In full mode the rectangle is always the whole
// (widened) screen; the z-buffer handles occlusion.
extern "C" void ts2_room_mesh_bounds(uint8_t* rdram, recomp_context* ctx) {
    if (draw_mode() != DrawMode::Full) return;
    const int margin = ts2_wide_margin();
    ctx->r3 = ADD32(-margin, 0);                    // x min (0x4B4)
    ctx->r4 = ADD32(320 + margin, 0);               // x max (0x4B8)
    ctx->r5 = ADD32(-ts2_vertical_guard(), 0);      // y min (0x4B6)
    ctx->r2 = ADD32(240 + ts2_vertical_guard(), 0); // y max (0x4BA)
}

// Extend only the two scenery submission passes. Their original limits are
// restored before returning to the frame orchestrator/gameplay code.
static constexpr uint32_t scenery_limits[] = {0x801CED64u, 0x801E31B8u, 0x800BAB20u};
extern "C" void ts2_begin_scenery_distance(uint8_t* rdram, int32_t* saved) {
    // Full mode raises the limits where they are read (ts2_draw_limit); the
    // adaptive extension only applies to TS2_DRAW_DISTANCE=extended.
    const double scale = draw_mode() == DrawMode::Extended ? distance_control.scale : 1.0;
    for (int i = 0; i < 3; ++i) {
        auto &limit = MEM_W(0, ADD32(scenery_limits[i], 0));
        saved[i] = limit;
        if (limit > 0 && scale > 1.0) {
            const int64_t extended = std::min(int64_t(double(limit) * scale), int64_t(16384));
            limit = static_cast<int32_t>(std::max(int64_t(limit), extended));
        }
    }
}

static int32_t display_list_used(uint8_t* rdram) {
    int32_t bytes = MEM_W(0, ADD32(display_list_cursor, 0)) - MEM_W(0, ADD32(display_list_base, 0));
    if (bytes < 0) bytes += 15;
    return bytes >> 4;
}

// Keep extended scenery within 65% of the frame's display-list capacity so
// actors and HUD always fit. Any rejected draw drops straight back to the
// original distance; growth back toward the maximum is gradual (~2 s).
extern "C" float ts2_update_scenery_distance(int32_t used, uint32_t failures, float scale, float maximum) {
    const float usage = float(used) / float(display_list_capacity);
    if (failures != 0) return 1.0f;
    if (usage > 0.65f) return std::max(1.0f, scale - 0.25f);
    if (usage < 0.50f) return std::min(maximum, scale + 1.0f / 120.0f);
    return scale;
}

extern "C" void ts2_end_scenery_distance(uint8_t* rdram, const int32_t* saved, int world_pass) {
    for (int i = 0; i < 3; ++i) MEM_W(0, ADD32(scenery_limits[i], 0)) = saved[i];
    // Match each original pass's final assignment to the polygon depth limit.
    MEM_W(0, ADD32(0x801CF094u, 0)) = saved[world_pass ? 2 : 1];
    auto& control = distance_control;
    control.peak = std::max(control.peak, display_list_used(rdram));
    if (world_pass && draw_mode() != DrawMode::Extended) {
        control.peak = 0;
        control.failures = 0;
        report_visibility(rdram);
        return;
    }
    if (world_pass) {
        const float before = control.scale;
        const float maximum = max_scenery_scale();
        if (control.ceiling <= 0.0f) control.ceiling = maximum;
        const float before_ceiling = control.ceiling;
        control.scale = ts2_update_scenery_distance(control.peak, control.failures, control.scale, control.ceiling);
        if (control.scale < before) {
            control.ceiling = control.scale;
            control.calm = 0;
        } else if (float(control.peak) / float(display_list_capacity) < ceiling_relax_usage) {
            if (++control.calm >= ceiling_relax_passes && control.ceiling < maximum) {
                control.ceiling = std::min(maximum, control.ceiling + 0.125f);
                control.calm = 0;
            }
        } else {
            control.calm = 0;
        }
        if (visibility_diagnostics() && (control.failures || control.scale < before || control.ceiling != before_ceiling)) {
            std::fprintf(stderr, "TS2 scenery distance: used=%d/%d failures=%u scale %.2f -> %.2f ceiling %.2f\n",
                control.peak, display_list_capacity, control.failures, before, control.scale, control.ceiling);
        }
        control.peak = 0;
        control.failures = 0;
        report_visibility(rdram);
    }
}

extern "C" int32_t ts2_wide_positive_extent(int32_t extent) {
    if (extent <= 0) return extent;
    const int64_t scaled = (int64_t(extent) * (160 + ts2_wide_margin()) + 159) / 160;
    return static_cast<int32_t>(std::min(scaled, int64_t(std::numeric_limits<int32_t>::max())));
}

// The guest compares abs(x)-radius with z*slope. A sphere's support along
// the slanted side plane x-z*slope is radius*sqrt(1+slope*slope), not radius.
// Widening only the center threshold still drops spheres crossing that plane.
// Callers retain the guest path in 4:3 and preserve all other rejection tests.
extern "C" int ts2_wide_sphere_visible(int32_t outside, int32_t depth, int32_t radius, int32_t slope_fixed) {
    const double slope = double(slope_fixed) / 16384.0;
    const double support = std::max(radius, 0) * (std::hypot(1.0, slope) - 1.0);
    const bool visible = double(outside) <= double(depth) * slope + support;
    if (visibility_diagnostics()) visibility_stats.recovered_spheres += visible && double(outside) >= double(depth) * slope;
    return visible;
}

// Screen rectangles define four planes through the camera. Test sphere support
// against each plane directly, without fitting a rotated cone to the rectangle.
// Guest projection is x=160+160*X/Z, y=120+160*Y/Z.
extern "C" int ts2_sphere_in_screen_rect(int32_t x, int32_t y, int32_t z, int32_t radius,
    int32_t left, int32_t top, int32_t right, int32_t bottom) {
    if (left > right || top > bottom) return 0;
    const double r = std::max(radius, 0);
    const auto inside = [r, z](double coordinate, double edge, bool lower) {
        const double distance = coordinate * 160.0 - edge * double(z);
        const double support = r * std::hypot(160.0, edge);
        return lower ? distance >= -support : distance <= support;
    };
    return z >= -r && inside(x, left - 160.0, true) && inside(x, right - 160.0, false)
        && inside(y, top - 120.0, true) && inside(y, bottom - 120.0, false);
}

extern "C" int ts2_wide_room_sphere_visible(uint8_t* rdram, uint32_t camera, uint32_t entry) {
    const gpr c = ADD32(camera, 0), e = ADD32(entry, 0);
    const gpr row = ADD32(0x800B04B0u + MEM_BU(15, e) * 0x34u, 0);
    const bool bounded = MEM_BU(3, row) != 0 &&
        (draw_mode() != DrawMode::Full || MEM_BU(0, row) != 0 || MEM_BU(1, row) != 0);
    const int margin = ts2_wide_margin();
    return ts2_sphere_in_screen_rect(MEM_W(0x34, c), MEM_W(0x38, c), MEM_W(0x3C, c),
        MEM_HU(12, e), bounded ? MEM_H(4, row) : -margin, bounded ? MEM_H(6, row) : -ts2_vertical_guard(),
        bounded ? MEM_H(8, row) : 320 + margin, bounded ? MEM_H(10, row) : 240 + ts2_vertical_guard());
}

// Only used immediately after the guest initializes a full-screen portal.
// Portal rows contain left/top/right/bottom halfwords at offsets 4/6/8/10.
extern "C" void ts2_wide_full_portal(uint8_t* rdram, uint32_t address) {
    const int margin = ts2_wide_margin();
    if (!margin) return;
    MEM_H(4, ADD32(address, 0)) = -margin;
    MEM_H(8, ADD32(address, 0)) = 320 + margin;
    MEM_H(6, ADD32(address, 0)) = -ts2_vertical_guard();
    MEM_H(10, ADD32(address, 0)) = 240 + ts2_vertical_guard();
}

// func_8006984C initializes these separate mesh-clipping globals on the
// non-portal path. They feed every triangle/quad path in func_8005D69C.
extern "C" void ts2_wide_mesh_bounds(uint8_t* rdram) {
    const int margin = ts2_wide_margin();
    if (!margin) return;
    MEM_W(0, ADD32(0x801DB430u, 0)) = -margin;
    MEM_W(0, ADD32(0x801DB9ACu, 0)) = 320 + margin;
    // Matching Y bounds (compared against vertex screen y at +0xA in func_8005D69C).
    // Only rewrite them when they hold the stock 0/240 so a wrong address can't corrupt state.
    {
        static int yReports = 0;
        static int32_t lastTop = 1, lastBottom = 1;
        const int32_t top = MEM_W(0, ADD32(0x801DB3ECu, 0)), bottom = MEM_W(0, ADD32(0x801DBC20u, 0));
        if ((top != lastTop || bottom != lastBottom) && yReports < 8) {
            ++yReports; lastTop = top; lastBottom = bottom;
            std::fprintf(stderr, "TS2 RENDER: mesh y bounds were %d..%d\n", top, bottom);
        }
        // 0x801DB3EC is the maximum y (reject y > it), 0x801DBC20 the minimum.
        if (top == 240 && bottom == 0) {
            MEM_W(0, ADD32(0x801DB3ECu, 0)) = 240 + ts2_vertical_guard();
            MEM_W(0, ADD32(0x801DBC20u, 0)) = -ts2_vertical_guard();
        }
    }
    static std::atomic<bool> reported{false};
    if (!reported.exchange(true, std::memory_order_relaxed)) {
        std::fprintf(stderr, "TS2 RENDER: expanded non-portal mesh clipping to %d..%d\n", -margin, 320 + margin);
    }
}

// The near-plane fallback saturates X to a screen edge. Keep Y unchanged.
extern "C" void ts2_wide_portal_edge(uint8_t* rdram, uint32_t address) {
    const int margin = ts2_wide_margin();
    if (!margin) return;
    auto &x = MEM_H(0, ADD32(address, 0));
    if (x == 0) x = -margin;
    else if (x == 320) x = 320 + margin;
}

extern "C" int ts2_wide_visible(uint8_t* rdram, uint32_t address, int count, int stride, int indirect) {
    const int margin = horizontal_margin.load(std::memory_order_relaxed);
    if (margin == 0) return 0; // Keep the original 4:3 instructions untouched.
    int minimum = 32767, maximum = -32768;
    for (int i = 0; i < count; ++i) {
        uint32_t point = address + i * stride;
        if (indirect) {
            // The world renderer uses four pointer slots with a gap before slots 2/3.
            point = static_cast<uint32_t>(MEM_W(i < 2 ? i * 4 : (i - 1) * 12 + 4, ADD32(address, 0))) + 8;
        }
        const int x = MEM_H(0, ADD32(point, 0));
        minimum = std::min(minimum, x);
        maximum = std::max(maximum, x);
    }
    const bool visible = minimum <= 320 + margin && maximum >= -margin;
    if (visibility_diagnostics()) {
        visibility_stats.expanded_polygons += visible && (minimum > 320 || maximum < 0);
        visibility_stats.outside_polygons += !visible;
    }
    return visible;
}

// Game frame counter (display lists begun since boot) for frame-tagged
// input record/replay (input_replay.cpp).
extern "C" uint64_t ts2_game_frame_serial() {
    return game_frame_serial.load(std::memory_order_relaxed);
}
