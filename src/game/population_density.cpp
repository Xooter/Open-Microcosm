#include "game/population_density.hpp"

#include "core/profiling.hpp"

#include <algorithm>
#include <cmath>

void DensityGrid::reset(float x0, float y0, float x1, float y1, float minBin, int maxBins, int kinds) {
    PROFILE_SCOPE("DensityGrid::reset");
    kinds_ = static_cast<std::size_t>(std::max(1, kinds));
    bin_ = std::exp2(std::ceil(std::log2(std::max(1e-3f, minBin))));
    const auto cells = [&] {
        originX_ = std::floor(x0 / bin_) * bin_;
        originY_ = std::floor(y0 / bin_) * bin_;
        nx_ = std::max(1, static_cast<int>(std::ceil((x1 - originX_) / bin_)));
        ny_ = std::max(1, static_cast<int>(std::ceil((y1 - originY_) / bin_)));
        return static_cast<long long>(nx_) * ny_;
    };
    while (cells() > std::max(1, maxBins))
        bin_ *= 2.0f;
    invBin_ = 1.0f / bin_;
    v_.assign(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_) * kinds_, 0.0f);
}

void DensityGrid::smooth(int passes) {
    PROFILE_SCOPE("DensityGrid::smooth");
    const std::size_t k = kinds_, nx = static_cast<std::size_t>(nx_), ny = static_cast<std::size_t>(ny_);
    tmp_.resize(v_.size());
    for (int p = 0; p < passes; ++p) {
        // Horizontal: v_ → tmp_.
        for (std::size_t y = 0; y < ny; ++y)
            for (std::size_t x = 0; x < nx; ++x) {
                const std::size_t c = (y * nx + x) * k, l = (y * nx + (x > 0 ? x - 1 : x)) * k,
                                  r = (y * nx + (x + 1 < nx ? x + 1 : x)) * k;
                for (std::size_t j = 0; j < k; ++j)
                    tmp_[c + j] = 0.5f * v_[c + j] + 0.25f * (v_[l + j] + v_[r + j]);
            }
        // Vertical: tmp_ → v_.
        for (std::size_t y = 0; y < ny; ++y)
            for (std::size_t x = 0; x < nx; ++x) {
                const std::size_t c = (y * nx + x) * k, u = ((y > 0 ? y - 1 : y) * nx + x) * k,
                                  d = ((y + 1 < ny ? y + 1 : y) * nx + x) * k;
                for (std::size_t j = 0; j < k; ++j)
                    v_[c + j] = 0.5f * tmp_[c + j] + 0.25f * (tmp_[u + j] + tmp_[d + j]);
            }
    }
}

double DensityGrid::sum() const {
    double s = 0.0;
    for (float x : v_)
        s += x;
    return s;
}
