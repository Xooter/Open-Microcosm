#pragma once

#include <cmath>
#include <cstdint>

// RNG por contador: la secuencia depende solo de (seed, stream, index, step), nunca del hilo
// que la ejecuta. Así los loops paralelos con OpenMP siguen siendo deterministas por seed.
class Rng {
  public:
    Rng(std::uint64_t seed, std::uint64_t stream, std::uint64_t index, std::uint64_t step)
        : state_(mix(seed + mix(stream + mix(index + mix(step))))) {}

    std::uint64_t next() {
        state_ += 0x9E3779B97F4A7C15ull; // splitmix64
        return mix(state_);
    }

    // Uniforme en [0, 1).
    float uniform() { return static_cast<float>(next() >> 40) * 0x1.0p-24f; }

    float uniform(float lo, float hi) { return lo + (hi - lo) * uniform(); }

    // Normal estándar (Box–Muller).
    float normal() {
        const float u1 = uniform() + 0x1.0p-25f;
        const float u2 = uniform();
        return std::sqrt(-2.0f * std::log(u1)) * std::cos(6.28318530718f * u2);
    }

  private:
    static std::uint64_t mix(std::uint64_t z) {
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::uint64_t state_;
};

// Streams: separan las secuencias aleatorias de cada sistema.
namespace rng_stream {
inline constexpr std::uint64_t kInit = 1;
inline constexpr std::uint64_t kAmbient = 2;
inline constexpr std::uint64_t kBacteria = 3;
inline constexpr std::uint64_t kSpawn = 4;
inline constexpr std::uint64_t kTools = 5;
inline constexpr std::uint64_t kCells = 6;
inline constexpr std::uint64_t kBodies = 7;
inline constexpr std::uint64_t kFeeding = 8;
} // namespace rng_stream
