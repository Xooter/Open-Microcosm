#pragma once

#include <cstdint>
#include <vector>

// Grilla espacial periódica para búsqueda de vecinos en O(n·k).
// Counting sort por celda: los índices de una celda quedan contiguos en `order()`.
// Sin uso en la fase 1; la usan las colisiones entre células de la fase 2.
class SpatialGrid {
  public:
    // cellSize mínimo: la grilla redondea para que las celdas sean >= cellSize.
    void build(const std::vector<float>& x, const std::vector<float>& y, float width,
               float height, float cellSize);

    int cellsX() const { return gridW_; }
    int cellsY() const { return gridH_; }
    int cellOf(std::size_t particle) const { return static_cast<int>(cellOf_[particle]); }

    // Rango [begin, end) de `order()` con las partículas de la celda (cx, cy), con wrap.
    std::uint32_t cellBegin(int cx, int cy) const;
    std::uint32_t cellEnd(int cx, int cy) const;
    const std::vector<std::uint32_t>& order() const { return order_; }

  private:
    int cellIndex(int cx, int cy) const;

    int gridW_ = 0, gridH_ = 0;
    std::vector<std::uint32_t> cellOf_, cellStart_, order_, cursor_;
};
