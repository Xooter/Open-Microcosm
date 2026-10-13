#include "sim/fluid.hpp"

#include "core/profiling.hpp"

#include <algorithm>
#include <cmath>

Fluid::Fluid(int nx, int ny, float cellSize)
    : fx_(nx, ny, cellSize), fy_(nx, ny, cellSize), ux_(nx, ny, cellSize),
      uy_(nx, ny, cellSize), bgx_(nx, ny, cellSize), bgy_(nx, ny, cellSize), bx_(static_cast<std::size_t>(nx) * ny),
      by_(static_cast<std::size_t>(nx) * ny) {}

void Fluid::clearForces() {
    std::fill(fx_.v.begin(), fx_.v.end(), 0.0f);
    std::fill(fy_.v.begin(), fy_.v.end(), 0.0f);
}

void Fluid::addForce(Vec2 center, Vec2 force, float radius) {
    const float dx = fx_.dx;
    // Limitado a media grilla para que el wrap no sume la misma celda dos veces.
    const int reach = std::min({static_cast<int>(std::ceil(3.0f * radius / dx)),
                                fx_.nx / 2 - 1, fx_.ny / 2 - 1});
    const int ci = static_cast<int>(std::floor(center.x / dx));
    const int cj = static_cast<int>(std::floor(center.y / dx));
    const float inv2s2 = 1.0f / (2.0f * radius * radius);

    for (int dj = -reach; dj <= reach; ++dj) {
        for (int di = -reach; di <= reach; ++di) {
            const float px = (ci + di + 0.5f) * dx - center.x;
            const float py = (cj + dj + 0.5f) * dx - center.y;
            const float w = std::exp(-(px * px + py * py) * inv2s2);
            const int i = wrapIndex(ci + di, fx_.nx), j = wrapIndex(cj + dj, fx_.ny);
            fx_.at(i, j) += force.x * w;
            fy_.at(i, j) += force.y * w;
        }
    }
}

void Fluid::solve(const Fft2D& fft) {
    PROFILE_SCOPE("Fluid::solve");
    const int nx = fx_.nx, ny = fx_.ny;
    const std::size_t n = fx_.v.size();
    for (std::size_t k = 0; k < n; ++k) {
        bx_[k] = fx_.v[k];
        by_[k] = fy_.v[k];
    }
    fft.forward(bx_);
    fft.forward(by_);

    const float alpha = brinkmanFriction();
    const float w = fx_.width(), h = fx_.height();
#pragma omp parallel for schedule(static)
    for (int j = 0; j < ny; ++j) {
        const float ky = wavenumber(j, ny, h);
        for (int i = 0; i < nx; ++i) {
            const std::size_t idx = static_cast<std::size_t>(j) * nx + i;
            // Los modos de Nyquist no tienen par conjugado bien definido: se descartan.
            if (i == nx / 2 || j == ny / 2) {
                bx_[idx] = by_[idx] = 0.0f;
                continue;
            }
            const float kx = wavenumber(i, nx, w);
            const float k2 = kx * kx + ky * ky;
            Complex fx = bx_[idx], fy = by_[idx];
            if (k2 > 0.0f) {
                // Proyección incompresible: se quita la componente paralela a k.
                const Complex kdotf = kx * fx + ky * fy;
                fx -= kx * kdotf / k2;
                fy -= ky * kdotf / k2;
            }
            const float denom = viscosity * k2 + alpha;
            bx_[idx] = denom > 0.0f ? fx / denom : 0.0f;
            by_[idx] = denom > 0.0f ? fy / denom : 0.0f;
        }
    }

    fft.inverse(bx_);
    fft.inverse(by_);
    // Pared de la placa: el agua se frena junto al vidrio y no hay agua dentro de él.
    // Simplificación: la FFT no admite una pared no-slip exacta; la máscara la aproxima.
    const float* flow = flowMask_ ? flowMask_->data() : nullptr;
    for (std::size_t k = 0; k < n; ++k) {
        const float m = flow ? flow[k] : 1.0f;
        ux_.v[k] = bx_[k].real() * m;
        uy_.v[k] = by_[k].real() * m;
    }
}
