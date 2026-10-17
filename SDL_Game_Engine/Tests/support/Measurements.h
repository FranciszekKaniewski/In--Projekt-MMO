#pragma once

#include <array>
#include <cstddef>

namespace test_report {
struct MemorySample {
    const char* phase;
    int sdlBaseline, sdlCurrent;
    long long cppBaseline, cppCurrent;
};
struct FrameSample {
    int limit, currentFPS, frames, updates;
    double fps, updatesPerSecond, seconds;
};

// Fixed buffers keep recording from changing the measured allocation balance.
inline std::array<MemorySample, 256> memorySamples{};
inline std::array<FrameSample, 16> frameSamples{};
inline std::size_t memoryCount = 0, frameCount = 0;
inline bool samplesTruncated = false;

inline void recordMemory(const char* phase, int sdlBase, int sdlNow,
                         long long cppBase, long long cppNow) noexcept {
    if (memoryCount < memorySamples.size()) {
        memorySamples[memoryCount++] = {phase, sdlBase, sdlNow, cppBase, cppNow};
    } else {
        samplesTruncated = true;
    }
}
inline void recordFrame(int limit, int currentFPS, int frames, int updates,
                        double fps, double updatesPerSecond, double seconds) noexcept {
    if (frameCount < frameSamples.size()) {
        frameSamples[frameCount++] = {limit, currentFPS, frames, updates, fps, updatesPerSecond, seconds};
    } else {
        samplesTruncated = true;
    }
}
} // namespace test_report
