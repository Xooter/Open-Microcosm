#pragma once

#include "render/cell_renderer.hpp"
#include "render/field_overlay.hpp"
#include "game/inspect.hpp"
#include "game/population_density.hpp"
#include "render/instanced_particles.hpp"
#include "render/lod.hpp"
#include "render/population_layer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
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
    // Fase 19: el campo se dibuja a una fracción de la resolución de la ventana y se reescala
    // (menos píxeles para los shaders). La vista previa (`drawWorldTo`) va siempre a 1.
    void setRenderScale(float scale) { renderScale_ = std::clamp(scale, 0.5f, 1.0f); }
    // Umbrales de detalle de las células ×scale (1: los de siempre).
    void setCellDetailScale(float scale) {
        lod_.cellDetailScale = std::clamp(scale, 1.0f, 4.0f);
        cells_.setLod(lod_);
    }
    // Tope de individuos con estructuras por cuadro (zoom semántico).
    void setMaxDetailed(int n) {
        lod_.maxDetailed = std::max(0, n);
        cells_.setLod(lod_);
    }
    // Mapa de limitación (fase 13): cada célula se colorea por lo que limita su crecimiento.
    void setLimitationMap(bool on) { cells_.setLimitationMap(on); }
    // Zoom semántico: umbrales de nivel de detalle y de la densidad agregada.
    const RenderLodSettings& lod() const { return lod_; }
    void setLodEnabled(bool on) {
        lod_.enabled = on;
        cells_.setLod(lod_);
    }
    // Vista de microscopio o de modelo (estructuras por debajo del límite óptico).
    void setViewMode(ViewMode mode) {
        lod_.view = mode;
        cells_.setLod(lod_);
    }
    // El ser elegido se dibuja con más detalle (es el mismo individuo simulado, no una copia).
    void setInspection(const Picked& picked) {
        inspection_ = picked;
        cells_.setInspection(picked.kind == Picked::Kind::Cell ? picked.entity : entt::null);
    }

  private:
    void ensureScene();
    // Tamaño del destino final y de la escena (escalada) en píxeles.
    int outWidth() const { return out_ ? out_->texture.width : GetScreenWidth(); }
    int outHeight() const { return out_ ? out_->texture.height : GetScreenHeight(); }
    float sceneScale() const { return out_ ? 1.0f : renderScale_; }
    int width() const { return std::max(1, static_cast<int>(std::lround(outWidth() * sceneScale()))); }
    int height() const { return std::max(1, static_cast<int>(std::lround(outHeight() * sceneScale()))); }
    void drawDish(const World& world, const Camera2D& camera);
    // Recorta por la vista y reparte cada población entre individuos y densidad (una pasada).
    void gatherPopulations(const World& world, Rectangle view, float zoom, bool fluor, const float* palette);
    void drawPopulations(float zoom, const float* palette);
    // Individuos con estructuras (los cercanos al centro y el elegido), encima de los instanciados.
    void drawDetailed(const World& world, Rectangle view, float zoom, const float* palette);
    // Fagos libres (campo de concentración): solo en la vista de modelo, no se ven con luz.
    void drawPhageField(const World& world, Rectangle view, float zoom);

    // Lo que necesita cada población numerosa para dibujarse en su nivel de detalle.
    struct PopulationPass {
        LodBlend blend;
        float length = 0.0f, thickness = 0.0f; // tamaño real [µm]
        InstancedParticles instances;
        DensityGrid density;
        PopulationLayer layer;
        // Individuos que se dibujan con estructuras: índice, posición (con su copia del mundo) y
        // distancia² al centro de la vista (el elegido va primero, con −1).
        struct Detailed {
            std::uint32_t index;
            float x, y, dist2;
        };
        std::vector<Detailed> detailed;
    };

    FieldOverlay overlay_;
    CellRenderer cells_;
    RenderLodSettings lod_;
    PopulationPass populations_[kPopulationCount];
    std::vector<Vector2> copies_; // desplazamientos de las copias del mundo que caen en la vista
    Picked inspection_;
    DensityGrid phageGrid_;
    PopulationLayer phageLayer_;
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
    float renderScale_ = 1.0f;
    RenderTexture2D* out_ = nullptr; // destino alternativo (vista previa)
    std::vector<Vector2> rockPts_; // contorno de un grano (se reusa entre frames)
    bool postEnabled_ = true;
};
