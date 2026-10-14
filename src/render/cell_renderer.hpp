#pragma once

#include "core/vec2.hpp"
#include "game/stain.hpp"

#include <raylib.h>

#include <cstdint>
#include <vector>

class World;
struct RingBody;
enum class Limitation : std::uint8_t;

// Color del mapa de limitación (lo usa también la leyenda del panel Medición).
Color limitationColor(Limitation l);

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
    void renderCoverage(const World& world, const Camera2D& camera, Rectangle view);
    // Composición en pantalla (fuera de BeginMode2D).
    void composite(const Camera2D& camera);
    // Membranas y organelos (dentro de BeginMode2D).
    void drawDetails(const World& world, const Camera2D& camera, Rectangle view);
    // Tinción activa (nullptr: sin tinción). Solo cambia colores; la simulación no se entera.
    void setStain(const StainSpec* stain) { stain_ = stain; }
    // Colorea cada célula por su `Metabolism::limiting` (lee la simulación, no la cambia).
    void setLimitationMap(bool on) { limitationMap_ = on; }
    // Tamaño de la textura de destino (0: el de la pantalla); lo usa la vista previa.
    void setTargetSize(int w, int h) { targetW_ = w; targetH_ = h; }

  private:
    void ensureTarget();
    void drawFluorescence(const World& world, const Camera2D& camera, Rectangle view);
    void fillCell(const RingBody& body, Color tint);
    template <typename Fn>
    void forEachImage(const World& world, const RingBody& body, Rectangle view, Fn&& fn);

    RenderTexture2D target_{};
    Shader shader_{};
    bool shaderOk_ = false;
    int texelLoc_ = -1, rimLoc_ = -1;
    const StainSpec* stain_ = nullptr;
    bool limitationMap_ = false;
    int targetW_ = 0, targetH_ = 0;
    int fluorLoc_ = -1;
    std::vector<int> triangles_, scratch_;
};
