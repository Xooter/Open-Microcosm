#include "ui/stain_preview.hpp"

#include "core/profiling.hpp"

namespace ui {

StainPreview::StainPreview() = default;

StainPreview::~StainPreview() {
    for (auto& row : tex_)
        for (RenderTexture2D& t : row)
            if (t.id != 0)
                UnloadRenderTexture(t);
}

void StainPreview::build() {
    WorldConfig c;
    c.gridX = c.gridY = 128; // 1024 µm de lado, sin vidrio
    c.cellSize = 8.0f;
    c.dish = false;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = c.oscillatoriaFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.seed = 20241013;
    c.medium.detritusSpawnRate = 0.0f;
    c.medium.dustRate = 0.0f;
    world_ = std::make_unique<World>(c);
    renderer_ = std::make_unique<Renderer>();
    renderer_->setPostProcess(false); // el grano y la viñeta estorban para comparar colores
    // Células: dos filas de cinco (zona de 800 × 400 µm centrada en (512, 512)).
    struct Item {
        SpeciesId id;
        float x, y;
    };
    const Item cells[] = {
        // Grandes (vista general).
        {SpeciesId::Paramecium, 640, 300}, // glucógeno, lisosomas, macronúcleo
        {SpeciesId::Amoeba, 420, 340},     // glucógeno
        {SpeciesId::Spirogyra, 820, 640},  // almidón en el cloroplasto en cinta
        // Chicas (vista de detalle): reservas distintas y la levadura grampositiva.
        {SpeciesId::Chlamydomonas, 140, 300}, // almidón (pirenoide)
        {SpeciesId::Euglena, 190, 312},       // paramilón: no reacciona con el yodo
        {SpeciesId::Candida, 238, 300},       // pared de levadura: grampositiva
        {SpeciesId::Navicula, 160, 352},      // crisolaminarina, frústula de sílice
        {SpeciesId::Pediastrum, 232, 350},    // almidón
    };
    for (const Item& it : cells)
        world_->spawnBeing({BeingKind::Species, it.id}, {it.x, it.y});
    // Bacterias: racimos chicos para la vista de cerca (gramnegativas, violaceum y Bacillus).
    world_->spawnBeing({BeingKind::Bacterium, SpeciesId::Paramecium}, {455, 120});
    world_->spawnBeing({BeingKind::Bacillus, SpeciesId::Paramecium}, {500, 120});
    world_->spawnBeing({BeingKind::Violaceum, SpeciesId::Paramecium}, {545, 120});
    for (auto& row : tex_)
        for (RenderTexture2D& t : row) {
            t = LoadRenderTexture(kWidth, kHeight);
            SetTextureFilter(t.texture, TEXTURE_FILTER_BILINEAR);
        }
}

const RenderTexture2D& StainPreview::render(View view, const StainSpec* stain, std::size_t revision) {
    PROFILE_SCOPE("StainPreview::render");
    if (!world_)
        build();
    const int which = stain ? 1 : 0;
    if (!drawn_[view][which] || revision_[view][which] != revision) {
        Camera2D cam{};
        cam.offset = {0.5f * kWidth, 0.5f * kHeight};
        if (view == Cells) {
            cam.target = {520.0f, 450.0f};
            cam.zoom = 0.5f;
        } else if (view == Small) {
            cam.target = {190.0f, 327.0f};
            cam.zoom = 2.3f;
        } else { // bacterias de cerca: ~14 px por µm no hace falta, con 3 a 4 se distinguen los bacilos
            cam.target = {500.0f, 120.0f};
            cam.zoom = 3.4f;
        }
        renderer_->setStain(stain);
        renderer_->drawWorldTo(*world_, cam, Overlay::None, tex_[view][which]);
        renderer_->setStain(nullptr);
        revision_[view][which] = revision;
        drawn_[view][which] = true;
    }
    return tex_[view][which];
}

} // namespace ui
