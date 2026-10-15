#pragma once

#include "render/triangle_batch.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

// Círculos con los segmentos justos (fase 19): DrawCircleV usa siempre 36, aunque el círculo mida
// menos de un píxel, y las células tienen cientos de gránulos. Con n segmentos el polígono se
// aparta del círculo r·(1 − cos(π/n)); se elige n para que sean ≤ 0,25 px (invisible).
inline int circleSegments(float radius, float px, int maxSegments = 36) {
    const float rpx = radius / std::max(px, 1e-6f);
    if (rpx <= 0.25f)
        return 6;
    const float n = 3.14159265f / std::acos(std::max(-1.0f, 1.0f - 0.25f / rpx));
    return std::clamp(static_cast<int>(std::ceil(n)), 6, maxSegments);
}

inline void drawCircleFast(Vector2 center, float radius, Color color, float px) {
    // Hasta 1 px de radio, un cuadrado de igual área no se distingue de un círculo: 4 vértices en
    // vez de 18 (las motas de los gránulos, de lejos, son la mayoría).
    if (radius <= px) {
        const float side = 1.7724539f * radius; // √π·r
        fastRect({center.x - 0.5f * side, center.y - 0.5f * side}, {side, side}, color);
        return;
    }
    fastCircle(center, radius, circleSegments(radius, px), color);
}

inline void drawRingFast(Vector2 center, float inner, float outer, int maxSegments, Color color, float px) {
    fastRing(center, inner, outer, 0.0f, 360.0f, circleSegments(outer, px, maxSegments), color);
}
