#include "bio/stalk.hpp"

#include <algorithm>
#include <cmath>

int stalkNodeCount(float adultLength) { return std::max(3, static_cast<int>(std::ceil(adultLength / 5.0f)) + 1); }

void layStalk(StalkRod& rod, Vec2 holdfast, Vec2 direction, float length, int nodes) {
    const float len = std::sqrt(dot(direction, direction));
    rod.baseDirection = len > 1e-6f ? direction * (1.0f / len) : Vec2{1.0f, 0.0f};
    rod.length = length;
    rod.nodes.resize(static_cast<std::size_t>(std::max(3, nodes)));
    const float seg = length / static_cast<float>(rod.nodes.size() - 1);
    for (std::size_t i = 0; i < rod.nodes.size(); ++i)
        rod.nodes[i] = holdfast + rod.baseDirection * (seg * static_cast<float>(i));
}

float stalkRestLength(const StalkRod& rod, const StalkSpec& spec) {
    if (spec.kind != StalkKind::Spasmoneme)
        return rod.length;
    return rod.length * (1.0f - (1.0f - spec.contractedFraction) * rod.contracted);
}

void stepStalk(StalkRod& rod, const StalkSpec& spec, const std::vector<Vec2>& flow, const Vec2* tip, float dt) {
    const std::size_t n = rod.nodes.size();
    if (n < 3)
        return;
    // 1. Arrastre: a bajo Reynolds el nodo va con el agua (la rigidez lo corrige después).
    for (std::size_t i = 1; i < n && i < flow.size(); ++i)
        rod.nodes[i] = rod.nodes[i] + flow[i] * dt;
    if (tip)
        rod.nodes[n - 1] = *tip;
    // 2. Flexión: cada nodo interior se acerca a la recta de sus vecinos, y el primer tramo a la
    //    dirección del disco (está empotrado en el vidrio). Tasa [1/s]: no depende del paso.
    const float k = 1.0f - std::exp(-spec.bendingRate * dt);
    const float seg = stalkRestLength(rod, spec) / static_cast<float>(n - 1);
    rod.nodes[1] = rod.nodes[1] + (rod.nodes[0] + rod.baseDirection * seg - rod.nodes[1]) * k;
    for (std::size_t i = 2; i + 1 < n; ++i) {
        const Vec2 straight = rod.nodes[i - 1] + (rod.nodes[i - 1] - rod.nodes[i - 2]);
        rod.nodes[i] = rod.nodes[i] + (straight - rod.nodes[i]) * (0.5f * k);
    }
    // 3. Inextensible: desde el disco, cada nodo a un tramo del anterior en su dirección actual.
    for (std::size_t i = 1; i < n; ++i) {
        const Vec2 d = rod.nodes[i] - rod.nodes[i - 1];
        const float len = std::sqrt(dot(d, d));
        const Vec2 u = len > 1e-6f ? d * (1.0f / len) : rod.baseDirection;
        rod.nodes[i] = rod.nodes[i - 1] + u * seg;
    }
}
