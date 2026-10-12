#pragma once

#include <cmath>
#include <numeric>
#include <vector>

inline int wrapIndex(int i, int n) {
    i %= n;
    return i < 0 ? i + n : i;
}

inline float wrapCoord(float v, float size) {
    v -= std::floor(v / size) * size;
    return v < size ? v : 0.0f; // por redondeo float, v puede quedar exactamente en size
}

// Grilla escalar periódica, valores en el centro de cada celda: (i + 0.5)·dx.
struct Grid2D {
    int nx = 0, ny = 0;
    float dx = 1.0f;
    std::vector<float> v;

    Grid2D() = default;
    Grid2D(int nx_, int ny_, float dx_, float init = 0.0f)
        : nx(nx_), ny(ny_), dx(dx_), v(static_cast<std::size_t>(nx_) * ny_, init) {}

    float& at(int i, int j) { return v[static_cast<std::size_t>(j) * nx + i]; }
    float at(int i, int j) const { return v[static_cast<std::size_t>(j) * nx + i]; }

    float width() const { return nx * dx; }
    float height() const { return ny * dx; }

    int cellX(float x) const { return wrapIndex(static_cast<int>(std::floor(x / dx)), nx); }
    int cellY(float y) const { return wrapIndex(static_cast<int>(std::floor(y / dx)), ny); }

    // Interpolación bilineal con wrap en los bordes.
    float sample(float x, float y) const {
        const float gx = x / dx - 0.5f, gy = y / dx - 0.5f;
        const float fx = std::floor(gx), fy = std::floor(gy);
        const float tx = gx - fx, ty = gy - fy;
        const int i0 = wrapIndex(static_cast<int>(fx), nx), i1 = wrapIndex(i0 + 1, nx);
        const int j0 = wrapIndex(static_cast<int>(fy), ny), j1 = wrapIndex(j0 + 1, ny);
        const float a = at(i0, j0) + (at(i1, j0) - at(i0, j0)) * tx;
        const float b = at(i0, j1) + (at(i1, j1) - at(i0, j1)) * tx;
        return a + (b - a) * ty;
    }

    double sum() const { return std::accumulate(v.begin(), v.end(), 0.0); }
};
