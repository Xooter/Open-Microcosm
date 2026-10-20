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
    // 1. Arrastre: a bajo Reynolds el nodo va con el agua (la rigidez lo corrige después). La punta con
    //    zooide va donde está su polo posterior (el zooide tira del tallo).
    for (std::size_t i = 1; i < n && i < flow.size(); ++i)
        rod.nodes[i] = rod.nodes[i] + flow[i] * dt;
    if (tip)
        rod.nodes[n - 1] = *tip;
    // 2. Flexión: el ángulo de cada articulación (y el del primer tramo respecto del disco, que está
    //    empotrado en el vidrio) decae con la tasa de flexión [1/s]: no depende del paso. Relajar los
    //    ángulos endereza de verdad; acercar cada nodo a la prolongación del tramo anterior solo corría la
    //    curvatura hacia la punta y el tallo se enrollaba.
    const float keep = std::exp(-spec.bendingRate * dt);
    const float base = std::atan2(rod.baseDirection.y, rod.baseDirection.x);
    auto wrap = [](float a) {
        while (a > 3.14159265f)
            a -= 6.2831853f;
        while (a < -3.14159265f)
            a += 6.2831853f;
        return a;
    };
    // 3. Desde el disco, cada tramo con el largo de reposo y su ángulo relajado: inextensible.
    const float seg = stalkRestLength(rod, spec) / static_cast<float>(n - 1);
    float previous = base, rebuilt = base;
    Vec2 at = rod.nodes[0];
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const Vec2 d = rod.nodes[i + 1] - rod.nodes[i];
        const float angle = dot(d, d) > 1e-12f ? std::atan2(d.y, d.x) : previous;
        const float joint = wrap(angle - previous) * keep;
        previous = angle;
        rebuilt += joint;
        rod.nodes[i] = at;
        at = at + Vec2{std::cos(rebuilt), std::sin(rebuilt)} * seg;
    }
    rod.nodes[n - 1] = at;
}
