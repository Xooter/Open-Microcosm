#pragma once

#include "game/stain.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <raylib.h>

#include <memory>

namespace ui {

// Espécimen de prueba para el editor de tinciones: una placa chica fija con lo que cada tinción
// distingue (bacilos gramnegativos y grampositivos, levadura, algas con almidón, Euglena con
// paramilón, un ciliado con lisosomas, una ameba, una diatomea). Se dibuja con el MISMO render
// que la placa, así lo que se ve acá es lo que se verá en el laboratorio.
class StainPreview {
  public:
    enum View { Cells, Small, Bacteria, ViewCount };
    StainPreview();
    ~StainPreview();
    StainPreview(const StainPreview&) = delete;
    StainPreview& operator=(const StainPreview&) = delete;

    // Vuelve a dibujar `view` con `stain` (nullptr: sin teñir) si cambió. Devuelve la textura.
    const RenderTexture2D& render(View view, const StainSpec* stain, std::size_t revision);
    static constexpr int kWidth = 420, kHeight = 260;

  private:
    void build();

    std::unique_ptr<World> world_;
    std::unique_ptr<Renderer> renderer_;
    RenderTexture2D tex_[ViewCount][2]{}; // [vista][0: sin teñir, 1: teñida]
    std::size_t revision_[ViewCount][2]{};
    bool drawn_[ViewCount][2]{};
};

} // namespace ui
