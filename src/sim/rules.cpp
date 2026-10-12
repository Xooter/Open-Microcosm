#include "sim/rules.hpp"

#include <random>

void Rules::randomize(std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& row : attraction)
        for (float& a : row)
            a = dist(rng);
}
