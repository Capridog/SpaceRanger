// Bounded, opt-in guest render provenance. No guest state is modified.
#include "recomp.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" bool ts2_request_frame_capture();
namespace {
constexpr size_t Capacity = 65536;
struct Record {
    uint64_t sequence, frame, milliseconds;
    uint32_t pc, object, r2, r3, part, matrix, display_list, camera;
    int32_t model, flags, scalar, x, y, z;
    std::array<uint32_t, 16> matrix_words;
};
struct Trace {
    std::array<Record, Capacity> records{};
    uint64_t sequence = 0, frame = 0;
    unsigned captures = 0;
    bool key_down = false;
};
bool valid(uint32_t address, uint32_t bytes) {
    const uint32_t segment = address & 0xE0000000u;
    return (segment == 0x80000000u || segment == 0xA0000000u) &&
        (address & 0x1FFFFFFFu) <= 0x800000u - bytes;
}
int32_t word(const uint8_t* memory, uint32_t address) {
    int32_t result = 0;
    if (valid(address, 4)) std::memcpy(&result, memory + (address & 0x1FFFFFFFu), 4);
    return result;
}
int32_t half(const uint8_t* memory, uint32_t address) {
    int16_t result = 0;
    if (valid(address, 2)) std::memcpy(&result, memory + ((address & 0x1FFFFFFFu) ^ 2u), 2);
    return result;
}
uint64_t now() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
void save(Trace& trace) {
    if (trace.captures >= 3 || trace.sequence == 0) return;
    char path[256];
    std::snprintf(path, sizeof(path), "C:/SpaceRanger/docs/actor-trace-%llu-%u.csv",
        static_cast<unsigned long long>(now()), ++trace.captures);
    FILE* output = std::fopen(path, "wx");
    if (!output) {
        std::fprintf(stderr, "TS2 actor trace: cannot create %s\n", path);
        return;
    }
    std::fputs("sequence,frame,utc_ms,pc,object,r2,r3,part,matrix,display_list,camera,model,flags,scalar,x,y,z", output);
    for (unsigned i = 0; i < 16; ++i) std::fprintf(output, ",matrix_word_%u", i);
    std::fputc('\n', output);
    const uint64_t first = trace.sequence > Capacity ? trace.sequence - Capacity : 0;
    for (uint64_t index = first; index < trace.sequence; ++index) {
        const auto& r = trace.records[index % Capacity];
        std::fprintf(output, "%llu,%llu,%llu,%08X,%08X,%08X,%08X,%u,%08X,%08X,%08X,%d,%d,%d,%d,%d,%d",
            static_cast<unsigned long long>(r.sequence), static_cast<unsigned long long>(r.frame),
            static_cast<unsigned long long>(r.milliseconds), r.pc, r.object, r.r2, r.r3,
            r.part, r.matrix, r.display_list, r.camera, r.model, r.flags, r.scalar, r.x, r.y, r.z);
        for (uint32_t value : r.matrix_words) std::fprintf(output, ",%08X", value);
        std::fputc('\n', output);
    }
    const bool failed = std::ferror(output) != 0;
    const int close_result = std::fclose(output);
    if (failed || close_result != 0) {
        std::fprintf(stderr, "TS2 actor trace: incomplete write to %s\n", path);
    }
    else {
        std::fprintf(stderr, "TS2 actor trace: saved %llu events to %s; RT64 capture request %s\n",
            static_cast<unsigned long long>(trace.sequence - first), path,
            ts2_request_frame_capture() ? "accepted" : "unavailable (enable TS2_FRAME_CAPTURE)");
    }
    std::fflush(stderr);
}
}

extern "C" void ts2_trace_emitter(uint8_t* rdram, uint32_t function_vram);
extern "C" void ts2_set_render_object(uint32_t object, int32_t model);

extern "C" void ts2_trace_actor_submission(uint8_t* rdram, const recomp_context* ctx, uint32_t pc) {
    // Render provenance (always on, cheap): func_80059C14 draws one render
    // object per iteration; s1 holds the object inside the object body.
    // Publish it so display-list marks carry the object/model identity.
    if (rdram != nullptr && ctx != nullptr) {
        static thread_local uint32_t published = 0;
        if (pc >= 0x80059D08u && pc <= 0x8005B580u) {
            // s1 (loaded at 0x80059D04) is the render object. Its address is
            // the identity RT64 uses (the +0x14 "model" half is not reliable).
            const uint32_t object = static_cast<uint32_t>(ctx->r17);
            if (object != published && valid(object, 0x44)) {
                published = object;
                ts2_set_render_object(object, static_cast<int32_t>(object & 0x7FFFFFu));
                ts2_trace_emitter(rdram, 0x80059C14u);
            }
        }
        else if (pc == 0x80059C14u || pc == 0x8005B5E4u || pc == 0x8000E990u) {
            published = 0;
            ts2_set_render_object(0, -1);
        }
    }
    static const bool enabled = [] {
        const char* value = std::getenv("TS2_ACTOR_SUBMISSION_TRACE");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    if (!enabled || rdram == nullptr || ctx == nullptr) return;
    // These hooks execute on the guest graphics producer. Thread-local storage
    // also prevents accidental cross-thread interleaving if another caller is added.
    static thread_local Trace trace;
    if (pc == 0x8000E990u) {
        ++trace.frame;
#ifdef _WIN32
        const bool down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        if (down && !trace.key_down) save(trace);
        trace.key_down = down;
#endif
    }
    Record& r = trace.records[trace.sequence % Capacity];
    r = {};
    r.sequence = trace.sequence++;
    r.frame = trace.frame;
    r.milliseconds = now();
    r.pc = pc;
    r.r2 = static_cast<uint32_t>(ctx->r2);
    r.r3 = static_cast<uint32_t>(ctx->r3);
    r.part = static_cast<uint32_t>(ctx->r21);
    r.matrix = static_cast<uint32_t>(word(rdram, 0x800BAB1Cu));
    r.display_list = static_cast<uint32_t>(word(rdram, 0x801C8E84u));
    r.camera = static_cast<uint32_t>(word(rdram, 0x800BAC4Cu));
    r.scalar = word(rdram, 0x800AF2B8u);
    // s1 is a render-object pointer only inside the object body, not at entry,
    // allocation failure, pass exit, or the outer frame boundary.
    if (pc >= 0x80059D08u && pc <= 0x8005B580u) {
        r.object = static_cast<uint32_t>(ctx->r17);
        if (valid(r.object, 0x44)) {
            r.model = half(rdram, r.object + 0x14);
            r.flags = half(rdram, r.object + 0x40);
            r.x = word(rdram, r.object);
            r.y = word(rdram, r.object + 4);
            r.z = word(rdram, r.object + 8);
        }
    }
    // Matrix words are meaningful only after the conversion call returns.
    if ((pc == 0x8005A940u || pc == 0x8005B1D8u) && valid(r.matrix, 64)) {
        std::memcpy(r.matrix_words.data(), rdram + (r.matrix & 0x1FFFFFFFu), 64);
    }
}
