#include "sim/polygon.hpp"

#include <numeric>

namespace polygon {

namespace {
// Incluye el borde: un vértice apoyado sobre la diagonal invalida la oreja.
bool insideOrOnEdge(Vec2 q, Vec2 a, Vec2 b, Vec2 c, float sign) {
    return sign * cross(b - a, q - a) >= 0.0f && sign * cross(c - b, q - b) >= 0.0f &&
           sign * cross(a - c, q - c) >= 0.0f;
}

bool same(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
} // namespace

bool triangulate(const Vec2* p, int n, std::vector<int>& triangles, std::vector<int>& idx) {
    triangles.clear();
    if (n < 3)
        return false;
    idx.resize(n);
    std::iota(idx.begin(), idx.end(), 0);
    const float sign = signedArea(p, n) >= 0.0f ? 1.0f : -1.0f;

    int m = n, i = 0, sinceLastEar = 0;
    while (m > 3) {
        if (sinceLastEar > m)
            return false; // ninguna oreja en una vuelta completa: polígono degenerado
        const int prev = (i + m - 1) % m, cur = i % m, next = (i + 1) % m;
        const Vec2 a = p[idx[prev]], b = p[idx[cur]], c = p[idx[next]];
        bool ear = sign * cross(b - a, c - b) > 0.0f;
        for (int k = 0; ear && k < m; ++k) {
            const Vec2 q = p[idx[k]];
            if (k != prev && k != cur && k != next && !same(q, a) && !same(q, b) &&
                !same(q, c) && insideOrOnEdge(q, a, b, c, sign))
                ear = false;
        }
        if (ear) {
            triangles.insert(triangles.end(), {idx[prev], idx[cur], idx[next]});
            idx.erase(idx.begin() + cur);
            --m;
            i = cur % m;
            sinceLastEar = 0;
        } else {
            i = (cur + 1) % m;
            ++sinceLastEar;
        }
    }
    triangles.insert(triangles.end(), {idx[0], idx[1], idx[2]});
    return true;
}

} // namespace polygon
