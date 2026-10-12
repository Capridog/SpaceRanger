#define SDL_MAIN_HANDLED
#include "../src/bridge_audio.cpp"
#include <iostream>

int main() {
    float levels[] = {0.5f, -0.5f};
    ts2::audio::set_volume(50);
    ts2::audio::apply_gain(levels, 2);
    if (levels[0] != 0.25f || levels[1] != -0.25f) return 2;
    ts2::audio::set_volume(0);
    ts2::audio::apply_gain(levels, 2);
    if (levels[0] != 0 || levels[1] != 0) return 3;
    ts2::audio::set_volume(100);
    SDL_SetMainReady();
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    if (!ts2::audio::initialize()) return 1;
    ts2::audio::set_frequency(22050);
    std::vector<int16_t> pcm(4096, 1234);
    int generated = 0;
    uint64_t warmedEmptySubmissions = 0;
    for (int frame = 0; frame < 180; ++frame) {
        // The game's 0x800906D4 production rule, with runtime AI lead offset.
        int remainingBytes = int(ts2::audio::get_frames_remaining() * 4);
        remainingBytes = std::max(0, remainingBytes - 367);
        int wanted = (0x180 + 0x73 - ((remainingBytes >> 2) - 0x10)) & ~15;
        int frames = std::max(0x170, wanted);
        if (wanted < 0) frames = wanted; // Guest uses an unsigned comparison.
        if (frames > 0) {
            ts2::audio::queue_samples(pcm.data(), frames * 2);
            ++generated;
        }
        if (frame % 30 == 0) std::cout << "tick=" << frame << " generated=" << generated
            << " remaining=" << ts2::audio::get_frames_remaining()
            << " pending=" << ts2::audio::pending_source_frames
            << " queued=" << SDL_GetQueuedAudioSize(ts2::audio::device) << '\n';
        if (frame == 59) warmedEmptySubmissions = ts2::audio::empty_output_submissions;
        // Include occasional missed producer ticks after warm-up.
        SDL_Delay(frame > 60 && frame % 47 == 0 ? 34 : 17);
    }
    const bool progressing = generated > 30;
    const auto gaps = ts2::audio::empty_output_submissions - warmedEmptySubmissions;
    std::cout << "steady_state_empty_submissions=" << gaps << '\n';
    ts2::audio::shutdown();
    const bool continuous = gaps <= 2;
    std::cout << (progressing && continuous ? "PASS: sustained small-buffer audio with bounded scheduling jitter\n" : "FAIL: audio production stalled or repeatedly ran dry\n");
    return progressing && continuous ? 0 : 1;
}
