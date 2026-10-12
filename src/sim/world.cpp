#include "sim/world.hpp"

#include "core/profiling.hpp"

#include <algorithm>
#include <cmath>
#include <random>

World::World(float width, float height, std::size_t count, std::uint32_t seed)
    : width_(width), height_(height) {
    rules_.randomize(seed);
    respawn(count, seed);
}

void World::respawn(std::size_t count, std::uint32_t seed) {
    std::mt19937 rng(seed ^ 0x9E3779B9u);
    std::uniform_real_distribution<float> rx(0.0f, width_), ry(0.0f, height_);
    std::uniform_int_distribution<int> rs(0, rules_.speciesCount - 1);

    p_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        p_.x[i] = rx(rng);
        p_.y[i] = ry(rng);
        p_.vx[i] = p_.vy[i] = 0.0f;
        p_.species[i] = static_cast<std::uint8_t>(rs(rng));
    }
}

void World::step(float dt) {
    PROFILE_SCOPE("World::step");
    rebuildGrid();
    computeVelocities(dt);
    integrate(dt);
}

void World::rebuildGrid() {
    PROFILE_SCOPE("rebuildGrid");
    // Celdas >= rMax, así alcanza con mirar las 3x3 vecinas. Mínimo 3 por eje
    // para que el wrap toroidal no visite la misma celda dos veces.
    gridW_ = std::max(3, static_cast<int>(width_ / rules_.rMax));
    gridH_ = std::max(3, static_cast<int>(height_ / rules_.rMax));
    cellW_ = width_ / gridW_;
    cellH_ = height_ / gridH_;

    const std::size_t n = p_.size();
    const std::size_t cells = static_cast<std::size_t>(gridW_) * gridH_;
    cellOf_.resize(n);
    cellStart_.assign(cells + 1, 0);
    sx_.resize(n);
    sy_.resize(n);
    sSpecies_.resize(n);

    for (std::size_t i = 0; i < n; ++i) {
        int cx = std::min(gridW_ - 1, static_cast<int>(p_.x[i] / cellW_));
        int cy = std::min(gridH_ - 1, static_cast<int>(p_.y[i] / cellH_));
        std::uint32_t c = static_cast<std::uint32_t>(cy * gridW_ + cx);
        cellOf_[i] = c;
        ++cellStart_[c + 1];
    }
    for (std::size_t c = 0; c < cells; ++c)
        cellStart_[c + 1] += cellStart_[c];

    std::vector<std::uint32_t> cursor(cellStart_.begin(), cellStart_.end() - 1);
    for (std::size_t i = 0; i < n; ++i) {
        std::uint32_t dst = cursor[cellOf_[i]]++;
        sx_[dst] = p_.x[i];
        sy_[dst] = p_.y[i];
        sSpecies_[dst] = p_.species[i];
    }
}

void World::computeVelocities(float dt) {
    PROFILE_SCOPE("computeVelocities");
    const float rMax = rules_.rMax;
    const float rMax2 = rMax * rMax;
    const float invRMax = 1.0f / rMax;
    const float beta = rules_.beta;
    const float halfW = width_ * 0.5f, halfH = height_ * 0.5f;
    const float friction = std::pow(0.5f, dt / rules_.frictionHalfLife);
    const float accelScale = rMax * rules_.forceScale * dt;
    const auto n = static_cast<std::ptrdiff_t>(p_.size());

    // Cada hilo escribe sólo vx[i]/vy[i] de sus partículas: sin data races.
#pragma omp parallel for schedule(dynamic, 256)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        const float xi = p_.x[i], yi = p_.y[i];
        const float* aRow = rules_.attraction[p_.species[i]];
        const int cx = static_cast<int>(cellOf_[i] % gridW_);
        const int cy = static_cast<int>(cellOf_[i] / gridW_);

        float fx = 0.0f, fy = 0.0f;
        for (int oy = -1; oy <= 1; ++oy) {
            const int ny = (cy + oy + gridH_) % gridH_;
            for (int ox = -1; ox <= 1; ++ox) {
                const int nx = (cx + ox + gridW_) % gridW_;
                const std::uint32_t c = static_cast<std::uint32_t>(ny * gridW_ + nx);
                for (std::uint32_t k = cellStart_[c]; k < cellStart_[c + 1]; ++k) {
                    float dx = sx_[k] - xi;
                    float dy = sy_[k] - yi;
                    // Distancia mínima en un toro.
                    if (dx > halfW) dx -= width_;
                    else if (dx < -halfW) dx += width_;
                    if (dy > halfH) dy -= height_;
                    else if (dy < -halfH) dy += height_;

                    const float d2 = dx * dx + dy * dy;
                    if (d2 > 0.0f && d2 < rMax2) {
                        const float d = std::sqrt(d2);
                        const float f =
                            interactionKernel(d * invRMax, aRow[sSpecies_[k]], beta);
                        fx += dx / d * f;
                        fy += dy / d * f;
                    }
                }
            }
        }
        p_.vx[i] = p_.vx[i] * friction + fx * accelScale;
        p_.vy[i] = p_.vy[i] * friction + fy * accelScale;
    }
}

void World::integrate(float dt) {
    PROFILE_SCOPE("integrate");
    const auto n = static_cast<std::ptrdiff_t>(p_.size());
#pragma omp parallel for simd
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        float x = p_.x[i] + p_.vx[i] * dt;
        float y = p_.y[i] + p_.vy[i] * dt;
        x -= std::floor(x / width_) * width_;
        y -= std::floor(y / height_) * height_;
        // Por redondeo float, x puede quedar exactamente en width_.
        p_.x[i] = x < width_ ? x : 0.0f;
        p_.y[i] = y < height_ ? y : 0.0f;
    }
}
