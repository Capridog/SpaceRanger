#pragma once
#include <cstddef>
#include <cstdint>

namespace ts2::audio {
bool initialize();
void shutdown();
// Runtime ABI: count is 16-bit samples in word-swapped N64 RDRAM.
void queue_samples(int16_t* samples, size_t count);
size_t get_frames_remaining();
void set_frequency(uint32_t frequency);
void set_volume(float percent);
}
