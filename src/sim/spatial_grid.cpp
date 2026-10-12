#include "sim/spatial_grid.hpp"

#include "core/profiling.hpp"
#include "sim/grid2d.hpp"

#include <algorithm>

void SpatialGrid::build(const std::vector<float>& x, const std::vector<float>& y, float width,
                        float height, float cellSize) {
    PROFILE_SCOPE("SpatialGrid::build");
    // Mínimo 3 celdas por eje para que el wrap no visite la misma celda dos veces.
    gridW_ = std::max(3, static_cast<int>(width / cellSize));
    gridH_ = std::max(3, static_cast<int>(height / cellSize));
    const float cellW = width / gridW_, cellH = height / gridH_;

    const std::size_t n = x.size();
    const std::size_t cells = static_cast<std::size_t>(gridW_) * gridH_;
    cellOf_.resize(n);
    order_.resize(n);
    cellStart_.assign(cells + 1, 0);

    for (std::size_t i = 0; i < n; ++i) {
        const int cx = std::min(gridW_ - 1, static_cast<int>(x[i] / cellW));
        const int cy = std::min(gridH_ - 1, static_cast<int>(y[i] / cellH));
        cellOf_[i] = static_cast<std::uint32_t>(cy * gridW_ + cx);
        ++cellStart_[cellOf_[i] + 1];
    }
    for (std::size_t c = 0; c < cells; ++c)
        cellStart_[c + 1] += cellStart_[c];

    cursor_.assign(cellStart_.begin(), cellStart_.end() - 1);
    for (std::size_t i = 0; i < n; ++i)
        order_[cursor_[cellOf_[i]]++] = static_cast<std::uint32_t>(i);
}

int SpatialGrid::cellIndex(int cx, int cy) const {
    return wrapIndex(cy, gridH_) * gridW_ + wrapIndex(cx, gridW_);
}

std::uint32_t SpatialGrid::cellBegin(int cx, int cy) const {
    return cellStart_[cellIndex(cx, cy)];
}

std::uint32_t SpatialGrid::cellEnd(int cx, int cy) const {
    return cellStart_[cellIndex(cx, cy) + 1];
}
