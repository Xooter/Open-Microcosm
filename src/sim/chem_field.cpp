#include "sim/chem_field.hpp"

#include "core/profiling.hpp"
#include "sim/fluid.hpp"

#include <cmath>
#include <utility>

ChemField::ChemField(int nx, int ny, float cellSize, float diffusion_, float initial)
    : diffusion(diffusion_), c_(nx, ny, cellSize, initial), tmp_(nx, ny, cellSize),
      spec_(static_cast<std::size_t>(nx) * ny) {}

void ChemField::advect(const Fluid& fluid, float dt) {
    PROFILE_SCOPE("ChemField::advect");
    absorb();
    const double water = mask_ ? waterSum() : 0.0;
    const int nx = c_.nx, ny = c_.ny;
    const float dx = c_.dx;
#pragma omp parallel for schedule(static)
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            const float x = (i + 0.5f) * dx, y = (j + 0.5f) * dx;
            const float ux = fluid.ux().at(i, j), uy = fluid.uy().at(i, j);
            tmp_.at(i, j) = c_.sample(x - ux * dt, y - uy * dt);
        }
    }
    if (mask_) {
        std::swap(c_.v, tmp_.v);
        conserve(water);
        return;
    }
    // Semi-lagrangiano no conserva la masa exactamente (la interpolación suaviza y el flujo
    // discreto no es del todo incompresible): se reescala para que el total no cambie.
    double before = 0.0, after = 0.0;
    for (std::size_t k = 0; k < c_.v.size(); ++k) {
        before += c_.v[k];
        after += tmp_.v[k];
    }
    if (after > 0.0) {
        const auto s = static_cast<float>(before / after);
        for (float& v : tmp_.v)
            v *= s;
    }
    std::swap(c_.v, tmp_.v);
}

void ChemField::diffuse(const Fft2D& fft, float dt) {
    PROFILE_SCOPE("ChemField::diffuse");
    absorb();
    const double water = mask_ ? waterSum() : 0.0;
    const int nx = c_.nx, ny = c_.ny;
    const std::size_t n = c_.v.size();
    for (std::size_t k = 0; k < n; ++k)
        spec_[k] = c_.v[k];
    fft.forward(spec_);

    const float w = c_.width(), h = c_.height();
#pragma omp parallel for schedule(static)
    for (int j = 0; j < ny; ++j) {
        const float ky = wavenumber(j, ny, h);
        for (int i = 0; i < nx; ++i) {
            const float kx = wavenumber(i, nx, w);
            spec_[static_cast<std::size_t>(j) * nx + i] *=
                std::exp(-diffusion * (kx * kx + ky * ky) * dt);
        }
    }

    fft.inverse(spec_);
    for (std::size_t k = 0; k < n; ++k)
        c_.v[k] = spec_[k].real();
    if (mask_)
        conserve(water);
}

void ChemField::setMask(const DishMask* mask) {
    mask_ = mask && mask->active() ? mask : nullptr;
    ghost_.clear();
    if (!mask_)
        return;
    ghost_.resize(mask_->outside.size());
    reflect();
}

void ChemField::absorb() {
    if (!mask_)
        return;
    const auto& out = mask_->outside;
    const auto& near = mask_->nearest;
    for (std::size_t m = 0; m < out.size(); ++m) {
        c_.v[near[m]] += c_.v[out[m]] - ghost_[m];
        c_.v[out[m]] = ghost_[m];
    }
}

void ChemField::reflect() {
    const auto& out = mask_->outside;
    const auto& mir = mask_->mirror;
    for (std::size_t m = 0; m < out.size(); ++m)
        c_.v[out[m]] = ghost_[m] = c_.v[mir[m]];
}

double ChemField::waterSum() const {
    double total = 0.0;
    for (std::size_t k = 0; k < c_.v.size(); ++k)
        if (mask_->water[k])
            total += c_.v[k];
    return total;
}

void ChemField::conserve(double before) {
    // Reescalar (y no sumar) mantiene las concentraciones positivas.
    const double after = waterSum();
    if (after > 0.0 && before > 0.0) {
        const auto s = static_cast<float>(before / after);
        for (std::size_t k = 0; k < c_.v.size(); ++k)
            if (mask_->water[k])
                c_.v[k] *= s;
    }
    reflect();
}

double ChemField::sum() const {
    if (!mask_)
        return c_.sum();
    double total = waterSum();
    for (std::size_t m = 0; m < mask_->outside.size(); ++m)
        total += c_.v[mask_->outside[m]] - ghost_[m];
    return total;
}
