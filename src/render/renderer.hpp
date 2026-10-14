#pragma once

#include "render/cell_renderer.hpp"
#include "render/field_overlay.hpp"
#include "render/instanced_particles.hpp"

#include <raylib.h>

#include <vector>

class World;

// Estilo campo claro (brightfield): fondo luminoso, organismos oscuros y semitransparentes.
class Renderer {
  public:
    Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void drawWorld(const World& world, const Camera2D& camera, Overlay overlay);
    // Igual, pero dibuja en `out` (con su tamaño) en vez de en la pantalla: la vista previa de las
    // tinciones usa el mismo render que la placa. Fuera de BeginDrawing/EndDrawing.
    void drawWorldTo(const World& world, const Camera2D& camera, Overlay overlay, RenderTexture2D& out);
    // Tinción activa (nullptr: ninguna). El render solo cambia colores; no toca la simulación.
    void setStain(const StainSpec* stain) {
        stain_ = stain;
        cells_.setStain(stain);
    }
    const StainSpec* stain() const { return stain_; }
    void setPostProcess(bool on) { postEnabled_ = on; }
    // Mapa de limitación (fase 13): cada célula se colorea por lo que limita su crecimiento.
    void setLimitationMap(bool on) { cells_.setLimitationMap(on); }

  private:
    void ensureScene();
    int width() const { return out_ ? out_->texture.width : GetScreenWidth(); }
    int height() const { return out_ ? out_->texture.height : GetScreenHeight(); }
    void drawDish(const World& world, const Camera2D& camera);

    FieldOverlay overlay_;
    CellRenderer cells_;
    InstancedParticles particles_;
    RenderTexture2D scene_{};
    Shader post_{};
    bool postOk_ = false;
    int resolutionLoc_ = -1, timeLoc_ = -1;
    Shader glass_{};
    bool glassOk_ = false;
    struct {
        int resolution = -1, offset = -1, target = -1, zoom = -1, dish = -1;
    } glassLoc_;
    const StainSpec* stain_ = nullptr;
    RenderTexture2D* out_ = nullptr; // destino alternativo (vista previa)
    std::vector<Vector2> rockPts_; // contorno de un grano (se reusa entre frames)
    bool postEnabled_ = true;
};
