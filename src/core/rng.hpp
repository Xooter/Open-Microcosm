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

    // Poisson de media `mean` (Knuth hasta 30; arriba, normal redondeada, que ya no se distingue).
    std::uint64_t poisson(double mean) {
        if (!(mean > 0.0))
            return 0;
        if (mean > 30.0) {
            const double x = std::floor(mean + std::sqrt(mean) * static_cast<double>(normal()) + 0.5);
            return x > 0.0 ? static_cast<std::uint64_t>(x) : 0;
        }
        const double limit = std::exp(-mean);
        std::uint64_t k = 0;
        double p = 1.0;
        for (;;) {
            p *= static_cast<double>(uniform());
            if (p <= limit)
                return k;
            ++k;
        }
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
inline constexpr std::uint64_t kLysis = 9;
inline constexpr std::uint64_t kAutopilot = 10;
inline constexpr std::uint64_t kPopulate = 11;
inline constexpr std::uint64_t kMutation = 12;
inline constexpr std::uint64_t kOrganelles = 13;
inline constexpr std::uint64_t kDefense = 14;
inline constexpr std::uint64_t kCyano = 15;
inline constexpr std::uint64_t kFungi = 16;
inline constexpr std::uint64_t kStalk = 17;
inline constexpr std::uint64_t kContraction = 19;
inline constexpr std::uint64_t kShapeWave = 20;
inline constexpr std::uint64_t kSomersault = 21;
inline constexpr std::uint64_t kMatingType = 22; // (18 es kStalk + 1: contracciones espontáneas del tallo)
inline constexpr std::uint64_t kCulture = 1000; // cultivo bien mezclado (sus sub-streams: + 0..99)
} // namespace rng_stream
