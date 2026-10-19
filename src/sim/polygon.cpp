#include "sim/polygon.hpp"

#include <cmath>

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

float perimeter(const std::vector<Vec2>& outline) {
    float p = 0.0f;
    for (std::size_t i = 0; i < outline.size(); ++i)
        p += length(outline[(i + 1) % outline.size()] - outline[i]);
    return p;
}

void sampleClosed(const std::vector<Vec2>& dense, int n, float scale, float start, bool reverse, std::vector<Vec2>& out) {
    out.clear();
    const std::size_t m = dense.size();
    if (m < 3 || n < 1)
        return;
    // Largo acumulado (en el sentido pedido).
    thread_local std::vector<Vec2> path;
    thread_local std::vector<float> arc;
    path.resize(m + 1);
    arc.assign(m + 1, 0.0f);
    for (std::size_t k = 0; k <= m; ++k) {
        const std::size_t idx = reverse ? (m - k % m) % m : k % m;
        path[k] = dense[idx];
        if (k > 0)
            arc[k] = arc[k - 1] + length(path[k] - path[k - 1]);
    }
    const float total = arc[m];
    std::size_t seg = 0;
    for (int i = 0; i < n; ++i) {
        float s = (start + static_cast<float>(i) / static_cast<float>(n)) * total;
        s = std::fmod(s, total);
        if (seg > 0 && arc[seg] > s)
            seg = 0;
        while (seg + 1 < m && arc[seg + 1] < s)
            ++seg;
        const float len = arc[seg + 1] - arc[seg];
        const float t = len > 0.0f ? (s - arc[seg]) / len : 0.0f;
        out.push_back((path[seg] + (path[seg + 1] - path[seg]) * t) * scale);
    }
}

float arcFraction(const std::vector<Vec2>& dense, std::size_t index, bool reverse) {
    const std::size_t m = dense.size();
    if (m < 3 || index == 0)
        return 0.0f;
    float upTo = 0.0f, total = 0.0f;
    for (std::size_t k = 0; k < m; ++k) {
        const float len = length(dense[(k + 1) % m] - dense[k]);
        if (k < index)
            upTo += len;
        total += len;
    }
    const float f = total > 0.0f ? upTo / total : 0.0f;
    return reverse ? 1.0f - f : f;
}

} // namespace polygon
