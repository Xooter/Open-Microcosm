#pragma once

#include "render/cell_renderer.hpp"
#include "render/field_overlay.hpp"
#include "render/instanced_particles.hpp"

#include <raylib.h>

class World;

// Estilo campo claro (brightfield): fondo luminoso, organismos oscuros y semitransparentes.
class Renderer {
  public:
    Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    ~Renderer();

    void drawWorld(const World& world, const Camera2D& camera, Overlay overlay);
    // Tinción: ver Stain.
    void setStain(Stain stain) {
        stain_ = stain;
        cells_.setStain(stain);
    }
    Stain stain() const { return stain_; }
    void setPostProcess(bool on) { postEnabled_ = on; }

  private:
    void ensureScene();
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
    Stain stain_ = Stain::None;
    bool postEnabled_ = true;
};
