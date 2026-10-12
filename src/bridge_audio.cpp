#include "bridge_audio.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <vector>

namespace ts2::audio {
namespace {
std::mutex mutex;
SDL_AudioDeviceID device = 0;
SDL_AudioStream* stream = nullptr;
SDL_AudioSpec output{};
uint32_t source_rate = 48000;
double pending_source_frames = 0;
uint64_t buffers = 0, nonzero_samples = 0;
uint64_t empty_output_submissions = 0, last_submission_ms = 0, longest_submission_gap_ms = 0;
bool owns_audio = false;
std::atomic<float> gain{1.0f};

// TS2 Level 2 audio-path diagnostic.
//
// queue_samples() receives the game's already-mixed stereo PCM. drain() sees
// that same stream after SDL's format/rate conversion and gain, immediately
// before it is queued to the output device. Logging both stages lets us tell
// whether the Level 2 "SFX + some instruments disappear" failure is already
// present in the guest mix or is introduced by the host resampler/output path.
//
// Diagnostic only. Enabled with TS2_AUDIO_DIAGNOSTICS=1.
// No sample values, queue timing, buffering, or resampling behavior is changed.
struct StereoDiagnosticStats {
    double rms_l = 0.0;
    double rms_r = 0.0;
    double rms_mid = 0.0;
    double rms_side = 0.0;
    double lr_correlation = 0.0;
    double side_to_mid_db = -300.0;
    double identical_ratio = 0.0;
    double peak = 0.0;
};

bool audio_diagnostics_enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("TS2_AUDIO_DIAGNOSTICS");
        return value && *value && value[0] != '0';
    }();
    return enabled;
}

template <typename Sample>
StereoDiagnosticStats stereo_diagnostic_stats(
    const Sample* samples, size_t count, double scale)
{
    StereoDiagnosticStats stats{};
    if (!samples || count < 2) return stats;

    const size_t frames = count / 2;
    double sum_l2 = 0.0;
    double sum_r2 = 0.0;
    double sum_lr = 0.0;
    double sum_mid2 = 0.0;
    double sum_side2 = 0.0;
    uint64_t identical = 0;

    for (size_t i = 0; i + 1 < count; i += 2) {
        const double l = static_cast<double>(samples[i]) * scale;
        const double r = static_cast<double>(samples[i + 1]) * scale;
        const double mid = 0.5 * (l + r);
        const double side = 0.5 * (l - r);

        sum_l2 += l * l;
        sum_r2 += r * r;
        sum_lr += l * r;
        sum_mid2 += mid * mid;
        sum_side2 += side * side;
        if (samples[i] == samples[i + 1]) ++identical;

        stats.peak = std::max(stats.peak, std::max(std::abs(l), std::abs(r)));
    }

    const double inv_frames = 1.0 / static_cast<double>(frames);
    stats.rms_l = std::sqrt(sum_l2 * inv_frames);
    stats.rms_r = std::sqrt(sum_r2 * inv_frames);
    stats.rms_mid = std::sqrt(sum_mid2 * inv_frames);
    stats.rms_side = std::sqrt(sum_side2 * inv_frames);

    const double corr_denom = std::sqrt(sum_l2 * sum_r2);
    stats.lr_correlation = (corr_denom > 1e-30) ? (sum_lr / corr_denom) : 0.0;

    constexpr double epsilon = 1e-30;
    stats.side_to_mid_db =
        10.0 * std::log10((sum_side2 + epsilon) / (sum_mid2 + epsilon));
    stats.identical_ratio =
        static_cast<double>(identical) / static_cast<double>(frames);
    return stats;
}

std::ofstream& audio_diagnostic_file() {
    static std::ofstream file;
    static bool attempted = false;
    if (!attempted && audio_diagnostics_enabled()) {
        attempted = true;
        file.open("audio-path.csv", std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file <<
                "row,stage,buffer,t_ms,source_rate,device_rate,frames,"
                "queued_bytes,pending_source_frames,"
                "rms_l,rms_r,rms_mid,rms_side,lr_correlation,"
                "side_to_mid_db,identical_ratio,peak\n";
            file.flush();
            std::fprintf(stderr,
                "TS2 audio diagnostic: writing pre/post-resampler stats to audio-path.csv\n");
        }
    }
    return file;
}

void log_audio_diagnostic(
    const char* stage,
    uint64_t buffer_index,
    const StereoDiagnosticStats& stats,
    size_t frames)
{
    if (!audio_diagnostics_enabled()) return;

    std::ofstream& file = audio_diagnostic_file();
    if (!file.is_open()) return;

    static uint64_t row = 0;
    const uint64_t now_ms = SDL_GetTicks64();
    const uint32_t queued_bytes = device ? SDL_GetQueuedAudioSize(device) : 0;

    file
        << (++row) << ','
        << stage << ','
        << buffer_index << ','
        << now_ms << ','
        << source_rate << ','
        << output.freq << ','
        << frames << ','
        << queued_bytes << ','
        << pending_source_frames << ','
        << stats.rms_l << ','
        << stats.rms_r << ','
        << stats.rms_mid << ','
        << stats.rms_side << ','
        << stats.lr_correlation << ','
        << stats.side_to_mid_db << ','
        << stats.identical_ratio << ','
        << stats.peak << '\n';

    // Keep crash/repro runs useful without forcing a flush for every packet.
    if ((row % 64ull) == 0ull) file.flush();
}

// Optional, bounded capture of the samples actually sent to the output device.
// Starts at the first non-silent packet; normal runs never create a file.
void capture_output(const float* samples, size_t count) {
    static const char* path = std::getenv("TS2_AUDIO_CAPTURE_PATH");
    if (!path || !*path) return;
    static std::ofstream file;
    static uint32_t written = 0;
    static const uint32_t seconds = [] {
        const char* v = std::getenv("TS2_AUDIO_CAPTURE_SECONDS");
        return v ? static_cast<uint32_t>(std::strtoul(v, nullptr, 10)) : 8u;
    }();
    const uint32_t limit = static_cast<uint32_t>(output.freq) * 2 * seconds;
    if (written >= limit) return;
    if (!file.is_open()) {
        if (std::none_of(samples, samples + count, [](float v) { return std::abs(v) > 0.000001f; })) return;
        file.open(path, std::ios::binary | std::ios::trunc);
        if (!file) return;
        char empty[44]{};
        file.write(empty, sizeof(empty));
        std::fprintf(stderr, "TS2 audio: capturing first 8 audible seconds to %s\n", path);
    }
    count = std::min<size_t>(count, limit - written);
    for (size_t i = 0; i < count; ++i) {
        const int16_t value = static_cast<int16_t>(std::clamp(samples[i], -1.0f, 1.0f) * 32767.0f);
        file.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    written += static_cast<uint32_t>(count);
    const auto end = file.tellp();
    std::array<char, 44> header{};
    auto u16 = [&](size_t offset, uint16_t value) { std::memcpy(header.data() + offset, &value, 2); };
    auto u32 = [&](size_t offset, uint32_t value) { std::memcpy(header.data() + offset, &value, 4); };
    std::memcpy(header.data(), "RIFF", 4); u32(4, 36 + written * 2);
    std::memcpy(header.data() + 8, "WAVEfmt ", 8); u32(16, 16); u16(20, 1); u16(22, 2);
    u32(24, output.freq); u32(28, output.freq * 4); u16(32, 4); u16(34, 16);
    std::memcpy(header.data() + 36, "data", 4); u32(40, written * 2);
    file.seekp(0); file.write(header.data(), header.size()); file.seekp(end); file.flush();
}

// TS2_AUDIO_INPUT_CAPTURE=path: every guest-mixed PCM buffer (before the
// resampler), from boot, as a WAV at the guest rate. Used by the automated
// replay test to compare the game's own mix between runs.
void capture_input(const int16_t* samples, size_t count) {
    static const char* path = std::getenv("TS2_AUDIO_INPUT_CAPTURE");
    if (!path || !*path) return;
    static std::ofstream file;
    static uint32_t written = 0;
    if (!file.is_open()) {
        file.open(path, std::ios::binary | std::ios::trunc);
        if (!file) return;
        char empty[44]{};
        file.write(empty, sizeof(empty));
    }
    if (written > 400u * 1024u * 1024u) return;
    file.write(reinterpret_cast<const char*>(samples), static_cast<std::streamsize>(count * sizeof(int16_t)));
    written += static_cast<uint32_t>(count * sizeof(int16_t));
    static uint64_t calls = 0;
    if ((++calls % 64) != 0) return;
    const auto end = file.tellp();
    std::array<char, 44> header{};
    auto u16 = [&](size_t offset, uint16_t value) { std::memcpy(header.data() + offset, &value, 2); };
    auto u32 = [&](size_t offset, uint32_t value) { std::memcpy(header.data() + offset, &value, 4); };
    std::memcpy(header.data(), "RIFF", 4); u32(4, 36 + written);
    std::memcpy(header.data() + 8, "WAVEfmt ", 8); u32(16, 16); u16(20, 1); u16(22, 2);
    u32(24, source_rate); u32(28, source_rate * 4); u16(32, 4); u16(34, 16);
    std::memcpy(header.data() + 36, "data", 4); u32(40, written);
    file.seekp(0); file.write(header.data(), header.size()); file.seekp(end); file.flush();
}

void apply_gain(float* samples, size_t count) {
    const float level = gain.load(std::memory_order_relaxed);
    for (size_t i = 0; i < count; ++i) samples[i] *= level;
}

void log_error(const char* operation) {
    std::fprintf(stderr, "TS2 audio: %s: %s\n", operation, SDL_GetError());
}

bool drain() {
    // Output format is always stereo float; device frequency may differ.
    std::array<float, 8192> converted{};
    for (;;) {
        const int available = SDL_AudioStreamAvailable(stream);
        if (available < 0) { log_error("stream available"); return false; }
        if (available == 0) return true;
        const int bytes = SDL_AudioStreamGet(stream, converted.data(),
            std::min(available, static_cast<int>(sizeof(converted))));
        if (bytes <= 0) { log_error("stream read"); return false; }
        const size_t converted_count = static_cast<size_t>(bytes) / sizeof(float);
        apply_gain(converted.data(), converted_count);
        capture_output(converted.data(), converted_count);
        if (audio_diagnostics_enabled()) {
            const StereoDiagnosticStats stats =
                stereo_diagnostic_stats(converted.data(), converted_count, 1.0);
            log_audio_diagnostic("output", buffers + 1, stats, converted_count / 2);
        }
        if (SDL_QueueAudio(device, converted.data(), bytes) != 0) {
            log_error("queue"); return false;
        }
        pending_source_frames = std::max(0.0, pending_source_frames -
            (bytes / (2.0 * sizeof(float))) * source_rate / output.freq);
    }
}

SDL_AudioStream* make_stream(uint32_t frequency) {
    return SDL_NewAudioStream(AUDIO_S16SYS, 2, static_cast<int>(frequency),
        AUDIO_F32SYS, 2, output.freq);
}
}

bool initialize() {
    std::lock_guard lock(mutex);
    if (device) return true;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        log_error("initialize"); return false;
    }
    owns_audio = true;
    SDL_AudioSpec wanted{};
    wanted.freq = 48000;
    wanted.format = AUDIO_F32SYS;
    wanted.channels = 2;
    wanted.samples = 512;
    // No SDL callback: N64 AI submission feeds the SDL queue.
    device = SDL_OpenAudioDevice(nullptr, 0, &wanted, &output,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!device || !(stream = make_stream(source_rate))) {
        log_error("open device/resampler");
        if (device) SDL_CloseAudioDevice(device);
        device = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        owns_audio = false;
        return false;
    }
    pending_source_frames = 0;
    buffers = nonzero_samples = 0;
    empty_output_submissions = last_submission_ms = longest_submission_gap_ms = 0;
    SDL_PauseAudioDevice(device, 0);
    char* default_name = nullptr;
    SDL_AudioSpec default_spec{};
    if (SDL_GetDefaultAudioInfo(&default_name, &default_spec, 0) == 0) {
        std::fprintf(stderr, "TS2 audio: default output %s\n", default_name);
        SDL_free(default_name);
    }
    std::fprintf(stderr, "TS2 audio: %s, stereo float %d Hz, period %u frames\n",
        SDL_GetCurrentAudioDriver(), output.freq, output.samples);
    return true;
}

void set_frequency(uint32_t frequency) {
    std::lock_guard lock(mutex);
    if (frequency == source_rate) return;
    // The guest rate is supplied by osAiSetFrequency, independently of FPS.
    if (frequency < 4000 || frequency > 192000) {
        std::fprintf(stderr, "TS2 audio: rejected invalid rate %u\n", frequency);
        return;
    }
    if (!device) { source_rate = frequency; return; }
    SDL_AudioStream* replacement = make_stream(frequency);
    if (!replacement) { log_error("change sample rate"); return; }
    // Finish the old resampler without discarding already queued audio.
    if (SDL_AudioStreamFlush(stream) != 0) log_error("flush rate transition");
    drain();
    SDL_FreeAudioStream(stream);
    stream = replacement;
    pending_source_frames = 0;
    source_rate = frequency;
    std::fprintf(stderr, "TS2 audio: N64 sample rate %u Hz\n", frequency);
}

void queue_samples(int16_t* samples, size_t count) {
    std::lock_guard lock(mutex);
    if (!device || !stream || !samples || !count) return;
    if ((count & 1) || count > static_cast<size_t>(std::numeric_limits<int>::max()) / 2) {
        std::fprintf(stderr, "TS2 audio: invalid stereo sample count %zu\n", count);
        return;
    }
    const uint64_t now_ms = SDL_GetTicks64();
    if (buffers != 0) {
        if (SDL_GetQueuedAudioSize(device) == 0) ++empty_output_submissions;
        longest_submission_gap_ms = std::max(longest_submission_gap_ms, now_ms - last_submission_ms);
    }
    last_submission_ms = now_ms;
    // N64ModernRuntime stores each 32-bit RDRAM word in host byte order.
    // For two signed 16-bit channels this swaps their halfword positions.
    std::vector<int16_t> pcm(count);
    for (size_t i = 0; i < count; i += 2) {
        pcm[i] = samples[i + 1];
        pcm[i + 1] = samples[i];
        nonzero_samples += (pcm[i] != 0) + (pcm[i + 1] != 0);
    }

    capture_input(pcm.data(), pcm.size());
    if (audio_diagnostics_enabled()) {
        const StereoDiagnosticStats stats =
            stereo_diagnostic_stats(pcm.data(), pcm.size(), 1.0 / 32768.0);
        log_audio_diagnostic("input", buffers + 1, stats, pcm.size() / 2);
    }

    if (SDL_AudioStreamPut(stream, pcm.data(), static_cast<int>(count * 2)) != 0) {
        log_error("stream write"); return;
    }
    pending_source_frames += count / 2;
    drain();
    ++buffers;
    // Recover from a stalled device/producer without accumulating seconds of lag.
    // Normal pacing comes from get_frames_remaining, not buffer dropping.
    const uint32_t queued = SDL_GetQueuedAudioSize(device);
    if (queued > static_cast<uint32_t>(output.freq) * 8 * 2) {
        std::fprintf(stderr, "TS2 audio: stalled queue recovery (%u bytes)\n", queued);
        SDL_ClearQueuedAudio(device);
        SDL_AudioStreamClear(stream);
        pending_source_frames = 0;
    }
    if (buffers == 1 || (buffers % 512) == 0) {
        int peak = 0;
        for (int16_t value : pcm) peak = std::max(peak, std::abs(static_cast<int>(value)));
        std::fprintf(stderr, "TS2 audio: buffers=%llu nonzero_samples=%llu queued_bytes=%u peak=%d\n",
            static_cast<unsigned long long>(buffers),
            static_cast<unsigned long long>(nonzero_samples), queued, peak);
        std::fprintf(stderr, "TS2 audio pacing: empty_submissions=%llu longest_gap_ms=%llu\n",
            static_cast<unsigned long long>(empty_output_submissions),
            static_cast<unsigned long long>(longest_submission_gap_ms));
    }
}

void set_volume(float percent) {
    gain.store(std::isfinite(percent) ? std::clamp(percent / 100.0f, 0.0f, 1.0f) : 1.0f,
        std::memory_order_relaxed);
}

size_t get_frames_remaining() {
    std::lock_guard lock(mutex);
    if (!device) return 0;
    const double queued_frames = SDL_GetQueuedAudioSize(device) / (2.0 * sizeof(float));
    // AI_LEN must describe playable output. SDL2 can retain more than the
    // game's entire target buffer in its conversion packetizer (880 source
    // frames observed at 22050 Hz). Counting that input as queued sound makes
    // the guest stop producing before SDL emits its first packet: a permanent
    // silent stall. Keep the converter's retained input out of AI backpressure.
    // The original game's roughly one-frame lead is smaller than SDL2's
    // resampling/staging bursts. Keep 40 ms of playable output in reserve so
    // AI backpressure cannot stop production while the next packet is still
    // retained by the converter. This changes buffering, never sample rate.
    constexpr double playback_reserve_seconds = 0.040;
    return static_cast<size_t>(std::ceil(std::max(0.0,
        queued_frames / output.freq - playback_reserve_seconds) * source_rate));
}

void shutdown() {
    std::lock_guard lock(mutex);
    if (audio_diagnostics_enabled()) {
        std::ofstream& file = audio_diagnostic_file();
        if (file.is_open()) file.flush();
    }
    if (device) SDL_CloseAudioDevice(device);
    device = 0;
    if (stream) SDL_FreeAudioStream(stream);
    stream = nullptr;
    pending_source_frames = 0;
    if (owns_audio) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_audio = false;
}
}
