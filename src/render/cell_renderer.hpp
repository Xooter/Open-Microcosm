#pragma once

#include "core/vec2.hpp"

#include <raylib.h>

#include <vector>

class World;
struct RingBody;

// Células en campo claro:
// 1. el relleno de cada célula se dibuja en una textura (triangulado con ear clipping);
// 2. un shader convierte esa cobertura en citoplasma translúcido con borde óptico y halo;
// 3. encima van la membrana y los organelos.
class CellRenderer {
  public:
    CellRenderer();
    CellRenderer(const CellRenderer&) = delete;
    CellRenderer& operator=(const CellRenderer&) = delete;
    ~CellRenderer();

    // Debe llamarse fuera de BeginMode2D (usa su propio render target).
    void renderCoverage(const World& world, const Camera2D& camera);
    // Composición en pantalla (fuera de BeginMode2D).
    void composite(const Camera2D& camera);
    // Membranas y organelos (dentro de BeginMode2D).
    void drawDetails(const World& world, const Camera2D& camera);

  private:
    void ensureTarget();
    void fillCell(const RingBody& body, Color tint);
    template <typename Fn> void forEachImage(const World& world, const RingBody& body, Fn&& fn);

    RenderTexture2D target_{};
    Shader shader_{};
    bool shaderOk_ = false;
    int texelLoc_ = -1, rimLoc_ = -1;
    std::vector<int> triangles_, scratch_;
};
