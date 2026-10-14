#pragma once

#include "core/vec2.hpp"

#include <algorithm>
#include <limits>
#include <vector>

// Geometría de polígonos simples (membranas). Funciona con cualquier orientación.
namespace polygon {

inline float signedArea(const Vec2* p, int n) {
    float a = 0.0f;
    for (int i = 0; i < n; ++i)
        a += cross(p[i], p[(i + 1) % n]);
    return 0.5f * a;
}

// Regla par-impar: robusta para polígonos cóncavos.
inline bool contains(const Vec2* p, int n, Vec2 q) {
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((p[i].y > q.y) != (p[j].y > q.y) &&
            q.x < (p[j].x - p[i].x) * (q.y - p[i].y) / (p[j].y - p[i].y) + p[i].x)
            inside = !inside;
    }
    return inside;
}

// ¿El polígono se cruza consigo mismo? (O(n²): solo para chequeos esporádicos).
inline bool selfIntersects(const Vec2* p, int n) {
    auto crosses = [](Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
        return cross(b - a, c - a) * cross(b - a, d - a) < 0.0f && cross(d - c, a - c) * cross(d - c, b - c) < 0.0f;
    };
    for (int i = 0; i < n; ++i)
        for (int j = i + 2; j < n; ++j)
            if (!(i == 0 && j == n - 1) && crosses(p[i], p[(i + 1) % n], p[j], p[(j + 1) % n]))
                return true;
    return false;
}

struct Closest {
    Vec2 point;
    Vec2 edgeDir; // dirección normalizada de la arista
    float dist2 = std::numeric_limits<float>::max();
    int edge = 0; // arista (edge, edge + 1)
    float t = 0;  // posición a lo largo de la arista [0, 1]
};

inline Closest closestOnBoundary(const Vec2* p, int n, Vec2 q) {
    Closest best;
    for (int i = 0; i < n; ++i) {
        const Vec2 a = p[i], ab = p[(i + 1) % n] - a;
        const float len2 = dot(ab, ab);
        const float t = len2 > 0.0f ? std::clamp(dot(q - a, ab) / len2, 0.0f, 1.0f) : 0.0f;
        const Vec2 c = a + ab * t;
        const float d2 = dot(q - c, q - c);
        if (d2 < best.dist2) {
            const float len = std::sqrt(len2);
            best = {c, len > 0.0f ? ab * (1.0f / len) : Vec2{1.0f, 0.0f}, d2, i, t};
        }
    }
    return best;
}

// Normal de la arista que apunta hacia adentro del polígono.
inline Vec2 inwardNormal(Vec2 edgeDir, float signedArea) {
    return signedArea >= 0.0f ? Vec2{-edgeDir.y, edgeDir.x} : Vec2{edgeDir.y, -edgeDir.x};
}

// Ear clipping: triángulos (índices de a 3) en `triangles`. Devuelve false si el polígono es
// degenerado (por ejemplo, auto-intersectado); en ese caso conviene un fallback.
bool triangulate(const Vec2* p, int n, std::vector<int>& triangles, std::vector<int>& scratch);

} // namespace polygon
