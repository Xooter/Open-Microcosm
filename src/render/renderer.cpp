#include "render/renderer.hpp"

#include "core/profiling.hpp"
#include "game/world.hpp"
#include "render/minerals.hpp"
#include "render/structure_renderer.hpp"

#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr Color kBackground = {236, 231, 214, 255}; // luz de lámpara halógena filtrada
constexpr Color kInk = {25, 30, 45, 170};
constexpr Color kDetritus = {120, 92, 55, 190};
constexpr Color kHypha = {150, 146, 128, 120};        // hifa hialina, casi transparente
constexpr Color kHyphaWall = {96, 92, 76, 170};
constexpr Color kSporangium = {120, 112, 86, 210};
constexpr Color kSpore = {90, 86, 70, 220};
constexpr Color kPlasmodium = {240, 208, 72, 235};     // Physarum: amarillo (carotenoides)
constexpr Color kPlasmodiumWall = {184, 150, 36, 235};
constexpr Color kSlime = {232, 222, 160, 26};          // rastro de baba
constexpr Color kFlake = {224, 212, 176, 230};
constexpr Color kFlakeEdge = {150, 136, 100, 230};
constexpr Color kSclerotium = {190, 120, 40, 230};
// Fagos libres en la vista de modelo: un tono propio (no es turbidez: con luz no se verían).
constexpr Color kPhageDensity = {88, 72, 150, 255};
constexpr Color kResting = {110, 78, 44, 230};         // esporangio de resistencia: pared gruesa parda
} // namespace

Renderer::Renderer() {
    cells_.setLod(lod_);
    post_ = LoadShader(nullptr, BIO_ASSET_DIR "/shaders/microscope_post.fs");
    postOk_ = post_.id != 0 && post_.id != rlGetShaderIdDefault();
    if (postOk_) {
        resolutionLoc_ = GetShaderLocation(post_, "resolution");
        timeLoc_ = GetShaderLocation(post_, "time");
    }
    glass_ = LoadShader(nullptr, BIO_ASSET_DIR "/shaders/dish_glass.fs");
    glassOk_ = glass_.id != 0 && glass_.id != rlGetShaderIdDefault();
    if (glassOk_) {
        glassLoc_.resolution = GetShaderLocation(glass_, "resolution");
        glassLoc_.offset = GetShaderLocation(glass_, "camOffset");
        glassLoc_.target = GetShaderLocation(glass_, "camTarget");
        glassLoc_.zoom = GetShaderLocation(glass_, "zoom");
        glassLoc_.dish = GetShaderLocation(glass_, "dish");
    }
}

// Pared de vidrio de la placa, en campo claro: afuera el vidrio sin agua (más claro, con
// rayas y polvo), en el borde la línea oscura de refracción y la línea de Becke brillante del
// lado del agua, y hacia adentro el menisco, que desvía la luz y oscurece el campo.
void Renderer::drawDish(const World& world, const Camera2D& camera) {
    const Dish& dish = world.dish();
    if (!dish.active())
        return;
    const auto sw = static_cast<float>(width()), sh = static_cast<float>(height());
    if (!glassOk_ || (stain_ && stain_->mode == StainMode::Fluorescence)) { // sin shader: solo el contorno
        BeginMode2D(camera);
        DrawRing({dish.center.x, dish.center.y}, dish.radius, dish.radius + 3.0f / camera.zoom, 0, 360, 256,
                 stain_ && stain_->mode == StainMode::Fluorescence ? Color{70, 76, 96, 200} : Color{60, 62, 56, 220});
        EndMode2D();
        return;
    }
    const float res[2] = {sw, sh};
    const float offset[2] = {camera.offset.x, camera.offset.y};
    const float target[2] = {camera.target.x, camera.target.y};
    const float d[3] = {dish.center.x, dish.center.y, dish.radius};
    SetShaderValue(glass_, glassLoc_.resolution, res, SHADER_UNIFORM_VEC2);
    SetShaderValue(glass_, glassLoc_.offset, offset, SHADER_UNIFORM_VEC2);
    SetShaderValue(glass_, glassLoc_.target, target, SHADER_UNIFORM_VEC2);
    SetShaderValue(glass_, glassLoc_.zoom, &camera.zoom, SHADER_UNIFORM_FLOAT);
    SetShaderValue(glass_, glassLoc_.dish, d, SHADER_UNIFORM_VEC3);
    BeginShaderMode(glass_);
    DrawRectangle(0, 0, static_cast<int>(sw), static_cast<int>(sh), WHITE);
    EndShaderMode();
}

Renderer::~Renderer() {
    if (scene_.id != 0)
        UnloadRenderTexture(scene_);
    if (postOk_)
        UnloadShader(post_);
    if (glassOk_)
        UnloadShader(glass_);
}

void Renderer::ensureScene() {
    const int w = width(), h = height();
    if (scene_.id != 0 && scene_.texture.width == w && scene_.texture.height == h)
        return;
    if (scene_.id != 0)
        UnloadRenderTexture(scene_);
    scene_ = LoadRenderTexture(w, h);
    SetTextureFilter(scene_.texture, TEXTURE_FILTER_BILINEAR);
}

void Renderer::drawWorldTo(const World& world, const Camera2D& camera, Overlay overlay, RenderTexture2D& out) {
    out_ = &out;
    drawWorld(world, camera, overlay);
    out_ = nullptr;
}

namespace {

// Color de cada tipo de bacteria en la instanciación: 0 bacilo gramnegativo, 1 con violaceína,
// 2 con fago, 3 bdeloplasto, 4 Bacillus (grampositivo).
void bacteriaPalette(const StainSpec* st, float out[32]) {
    const Rgba natural[8] = {rgb8(70, 80, 70, 1.0f), rgb8(96, 52, 140, 1.0f), rgb8(90, 96, 84, 1.0f), rgb8(84, 90, 76, 1.0f),
                             rgb8(70, 80, 70, 1.0f), rgb8(0, 0, 0, 1.0f), rgb8(0, 0, 0, 1.0f), rgb8(0, 0, 0, 1.0f)};
    const float alpha[8] = {150, 200, 110, 170, 150, 0, 0, 0};
    for (int k = 0; k < 8; ++k) {
        Rgba c = natural[k];
        float a = alpha[k] / 255.0f;
        if (st && st->mode == StainMode::Fluorescence) {
            // Solo brilla el nucleoide (ADN bacteriano).
            const StainResult dye = evaluateStain(*st, StainTarget::Nucleoid);
            c = dye.color;
            a = 0.9f * dye.alpha;
        } else if (st) {
            const StainTarget gram = k == 4 ? StainTarget::GramPositive : StainTarget::GramNegative;
            const StainResult g = evaluateStain(*st, gram), all = evaluateStain(*st, StainTarget::Bacteria);
            const StainResult dye = g.alpha >= all.alpha ? g : all;
            if (dye.alpha > 0.0f) {
                c = stainOver(c, dye);
                a = std::max(a, 0.92f * dye.alpha);
            }
        }
        out[4 * k] = c.r;
        out[4 * k + 1] = c.g;
        out[4 * k + 2] = c.b;
        out[4 * k + 3] = a;
    }
}

} // namespace

// Una sola pasada lineal por cada población: lo que está fuera de la vista se descarta; lo que
// queda va a las instancias (si los individuos se ven) y a la grilla de densidad (si la densidad
// se ve). Recorrer los arreglos SoA contiguos cuesta menos que armar un índice espacial por
// frame (la grilla de bacterias del mundo se arma a mitad del paso y no vale al dibujar).
void Renderer::gatherPopulations(const World& world, Rectangle view, float zoom, bool fluor, const float* palette) {
    PROFILE_SCOPE("Renderer::gatherPopulations");
    const float px = 1.0f / zoom;
    const Bacteria& bac = world.bacteria();
    const Bdellovibrios& bd = world.bdellovibrios();
    PopulationPass& bp = populations_[static_cast<int>(Population::Bacteria)];
    PopulationPass& dp = populations_[static_cast<int>(Population::Bdellovibrio)];
    bp.blend = lod_.blend(Population::Bacteria, zoom);
    bp.length = bac.params.length;
    bp.thickness = 2.0f * bac.params.radius;
    dp.blend = lod_.blend(Population::Bdellovibrio, zoom);
    if (fluor) // en fluorescencia solo se ve el ADN teñido de las bacterias (como antes)
        dp.blend = {dp.blend.level, 0.0f, 0.0f};
    dp.length = 1.0f; // vibrio diminuto (~1 × 0,4 µm)
    dp.thickness = 0.4f;
    bp.instances.setPalette(palette);
    dp.instances.setPalette(palette);

    // Individuos: margen de 5 µm (como antes). Densidad: dos celdas más allá de la vista, así el
    // suavizado del borde visible tiene vecinos reales.
    constexpr float kIndividualMargin = 5.0f;
    // El elegido (por su id estable) sube a dibujarse con estructuras aunque sea chico en pantalla.
    const bool boost = lod_.enabled && lod_.inspectionBoost;
    const std::uint32_t selBacterium = boost && inspection_.kind == Picked::Kind::Bacterium ? inspection_.id : 0u;
    const std::uint32_t selBdello = boost && inspection_.kind == Picked::Kind::Bdellovibrio ? inspection_.id : 0u;
    const float cx = view.x + 0.5f * view.width, cy = view.y + 0.5f * view.height;
    auto gather = [&](PopulationPass& pass, std::size_t n, const std::vector<float>& xs, const std::vector<float>& ys,
                      const std::vector<float>& heading, const std::vector<std::uint32_t>& ids, std::uint32_t selected,
                      auto&& kindOf, auto&& weightOf) {
        pass.instances.clear();
        pass.detailed.clear();
        const bool individuals = pass.blend.individual > 0.0f, aggregate = pass.blend.aggregate > 0.0f;
        // Con estructuras solo desde el nivel Detail (los cercanos al centro, con tope).
        const bool detail = lod_.enabled && individuals && pass.blend.level >= DetailLevel::Detail;
        const float minBin = lod_.binPixels * px;
        float dm = 0.0f;
        if (aggregate) {
            dm = 2.0f * minBin + kIndividualMargin;
            pass.density.reset(view.x - dm, view.y - dm, view.x + view.width + dm, view.y + view.height + dm, minBin,
                               lod_.maxBins, 8);
            dm = 2.0f * pass.density.binSize();
        }
        if (!individuals && !aggregate)
            return;
        const float m = std::max(kIndividualMargin, dm);
        const float x0 = view.x - m, x1 = view.x + view.width + m, y0 = view.y - m, y1 = view.y + view.height + m;
        const float ix0 = view.x - kIndividualMargin, ix1 = view.x + view.width + kIndividualMargin;
        const float iy0 = view.y - kIndividualMargin, iy1 = view.y + view.height + kIndividualMargin;
        for (const Vector2 off : copies_) {
            for (std::size_t i = 0; i < n; ++i) {
                const float x = xs[i] + off.x, y = ys[i] + off.y;
                if (x < x0 || x > x1 || y < y0 || y > y1)
                    continue;
                const int kind = kindOf(i);
                const bool chosen = selected != 0u && ids[i] == selected;
                const bool near = x > ix0 && x < ix1 && y > iy0 && y < iy1;
                if (near && (detail || chosen))
                    pass.detailed.push_back({static_cast<std::uint32_t>(i), x, y,
                                             chosen ? -1.0f : (x - cx) * (x - cx) + (y - cy) * (y - cy)});
                else if (individuals && near)
                    pass.instances.push(x, y, heading[i], static_cast<float>(kind));
                if (aggregate && !chosen)
                    pass.density.add(x, y, kind, weightOf(i));
            }
        }
        // Tope de individuos con estructuras: los más cercanos al centro; el resto, instanciados.
        const auto cap = static_cast<std::size_t>(std::max(1, lod_.maxDetailed));
        if (pass.detailed.size() > cap) {
            std::nth_element(pass.detailed.begin(), pass.detailed.begin() + static_cast<std::ptrdiff_t>(cap), pass.detailed.end(),
                             [](const auto& a, const auto& b) { return a.dist2 < b.dist2; });
            for (std::size_t k = cap; k < pass.detailed.size(); ++k) {
                const auto& d = pass.detailed[k];
                pass.instances.push(d.x, d.y, heading[d.index], static_cast<float>(kindOf(d.index)));
            }
            pass.detailed.resize(cap);
        }
        if (aggregate && lod_.smoothingPasses > 0)
            pass.density.smooth(lod_.smoothingPasses);
        if (pass.instances.count() > 0)
            pass.instances.upload();
    };
    // Tipo: 0 bacilo gramnegativo, 1 violeta (quorum), 2 con fago, 3 bdeloplasto, 4 Bacillus.
    gather(bp, bac.size(), bac.x, bac.y, bac.heading, bac.id, selBacterium,
           [&](std::size_t i) {
               return bac.bdello[i] >= 0.0f ? 3 : (bac.quorum[i] ? 1 : (bac.phage[i] >= 0.0f ? 2 : (bac.strain[i] == 2 ? 4 : 0)));
           },
           [&](std::size_t i) { return lod_.weightByBiomass ? bac.mass[i] : 1.0f; });
    gather(dp, bd.size(), bd.x, bd.y, bd.heading, bd.id, selBdello, [](std::size_t) { return 0; },
           [](std::size_t) { return 1.0f; });
}

void Renderer::drawPopulations(float zoom, const float* palette) {
    PROFILE_SCOPE("Renderer::drawPopulations");
    const float px = 1.0f / zoom;
    // Primero todas las densidades (debajo) y después los individuos.
    for (PopulationPass& pass : populations_) {
        if (pass.blend.aggregate <= 0.0f)
            continue;
        // El contraste exagerado de lejos vuelve al físico (1) a medida que aparecen los individuos.
        const float gain = 1.0f + (lod_.densityGain - 1.0f) * pass.blend.aggregate;
        pass.layer.update(pass.density, palette, pass.length * pass.thickness, gain, pass.blend.aggregate);
        pass.layer.draw();
    }
    for (PopulationPass& pass : populations_) {
        if (pass.blend.individual <= 0.0f || pass.instances.count() == 0)
            continue;
        float len, thick, fade = pass.blend.individual;
        if (lod_.enabled) {
            // A menos de un píxel se dibuja de un píxel pero más transparente: cada individuo
            // oscurece lo que cubriría su área real (empalma con la densidad).
            len = std::max(pass.length, px);
            thick = std::max(pass.thickness, px);
            fade *= (pass.length * pass.thickness) / (len * thick);
        } else { // como antes: tamaño real con un mínimo visible
            len = std::max(pass.length, 1.5f * px);
            thick = std::max(pass.thickness, 0.9f * px);
        }
        const DetailLevel level = pass.blend.level == DetailLevel::Aggregate ? DetailLevel::Simplified : pass.blend.level;
        if (pass.instances.ready()) {
            pass.instances.draw(len, thick, fade, level, px);
            continue;
        }
        // Sin instancing (GPU sin soporte): un rectángulo por individuo visible.
        const std::vector<float>& d = pass.instances.data();
        for (std::size_t k = 0; k + 3 < d.size(); k += 4) {
            const auto kind = static_cast<std::size_t>(d[k + 3]);
            const float* c = palette + 4 * kind;
            const Color col = {static_cast<unsigned char>(c[0] * 255.0f), static_cast<unsigned char>(c[1] * 255.0f),
                               static_cast<unsigned char>(c[2] * 255.0f), static_cast<unsigned char>(c[3] * fade * 255.0f)};
            const float l = kind == 3 ? 0.6f * len : len, t = kind == 3 ? 0.6f * len : thick;
            DrawRectanglePro({d[k], d[k + 1], l, t}, {0.5f * l, 0.5f * t}, d[k + 2] * RAD2DEG, col);
        }
    }
}

void Renderer::drawDetailed(const World& world, Rectangle view, float zoom, const float* palette) {
    PROFILE_SCOPE("Renderer::drawDetailed");
    PopulationPass& bp = populations_[static_cast<int>(Population::Bacteria)];
    PopulationPass& dp = populations_[static_cast<int>(Population::Bdellovibrio)];
    if (bp.detailed.empty() && dp.detailed.empty())
        return;
    StructureContext ctx;
    ctx.lod = &lod_;
    ctx.zoom = zoom;
    ctx.time = world.simTime();
    ctx.view = view;
    ctx.particleBudget = lod_.maxParticlesPerFrame;
    const Bacteria& bac = world.bacteria();
    auto color = [&](int kind, float fade) {
        const float* c = palette + 4 * kind;
        return Color{static_cast<unsigned char>(c[0] * 255.0f), static_cast<unsigned char>(c[1] * 255.0f),
                     static_cast<unsigned char>(c[2] * 255.0f), static_cast<unsigned char>(c[3] * fade * 255.0f)};
    };
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling(); // los marcos rotados invierten el giro de algunos triángulos
    for (const auto& d : bp.detailed) {
        const std::size_t i = d.index;
        BacteriumState st;
        st.at = {d.x, d.y};
        st.heading = bac.heading[i];
        st.length = bp.length;
        st.width = bp.thickness;
        st.id = bac.id[i];
        st.tumbling = bac.tumbling[i] != 0;
        st.phageFraction = bac.phage[i] >= 0.0f ? bac.phage[i] / bac.params.phageLatentHours : -1.0f;
        st.phageBurst = bac.params.phageBurst;
        st.bdelloFraction = bac.bdello[i] >= 0.0f ? bac.bdello[i] / bac.params.bdelloHours : -1.0f;
        st.bdelloProgeny = bac.params.bdelloProgeny;
        const int kind = bac.bdello[i] >= 0.0f ? 3 : (bac.quorum[i] ? 1 : (bac.phage[i] >= 0.0f ? 2 : (bac.strain[i] == 2 ? 4 : 0)));
        st.body = color(kind, d.dist2 < 0.0f ? 1.0f : bp.blend.individual);
        drawBacteriumStructures(st, bacteriumPlan(bac.strain[i]), ctx);
    }
    const Bdellovibrios& bd = world.bdellovibrios();
    for (const auto& d : dp.detailed) {
        BacteriumState st;
        st.at = {d.x, d.y};
        st.heading = bd.heading[d.index];
        st.length = dp.length;
        st.width = dp.thickness;
        st.id = bd.id[d.index];
        st.body = color(0, d.dist2 < 0.0f ? 1.0f : dp.blend.individual);
        drawBdellovibrioStructures(st, bdellovibrioPlan(), ctx);
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

void Renderer::drawPhageField(const World& world, Rectangle view, float zoom) {
    if (lod_.view != ViewMode::Model)
        return;
    PROFILE_SCOPE("Renderer::drawPhageField");
    // Viriones por µm² de vista: la concentración [1/µm³] por el espesor de la película. La grilla
    // del campo es gruesa; celdas más finas que media celda del campo no agregan información.
    const Grid2D& field = world.phage().grid();
    const float gap = world.fluid().gapHeight;
    const float minBin = std::max(lod_.binPixels / zoom, 0.5f * field.dx);
    phageGrid_.reset(view.x, view.y, view.x + view.width, view.y + view.height, minBin, lod_.maxBins, 1);
    const float bin = phageGrid_.binSize();
    for (int y = 0; y < phageGrid_.ny(); ++y)
        for (int x = 0; x < phageGrid_.nx(); ++x) {
            const float px = phageGrid_.originX() + (static_cast<float>(x) + 0.5f) * bin;
            const float py = phageGrid_.originY() + (static_cast<float>(y) + 0.5f) * bin;
            const float wx = wrapCoord(px, world.width()), wy = wrapCoord(py, world.height());
            phageGrid_.add(px, py, 0, std::max(0.0f, field.sample(wx, wy)) * gap * bin * bin);
        }
    // De 10⁻³ a 10 viriones/µm² de vista: con la película de 200 µm, ~5·10⁶ a ~5·10¹⁰ viriones/mL
    // (un lisado del catálogo, ~10⁸/mL, queda a un tercio de la escala).
    phageLayer_.updateLog(phageGrid_, kPhageDensity, 1e-3f, 1e1f, 0.2f);
    phageLayer_.draw();
}

void Renderer::drawWorld(const World& world, const Camera2D& camera, Overlay overlay) {
    PROFILE_SCOPE("Renderer::drawWorld");
    ensureScene();
    cells_.setTargetSize(out_ ? width() : 0, out_ ? height() : 0);
    const bool fluor = stain_ && stain_->mode == StainMode::Fluorescence;
    float palette[32];
    bacteriaPalette(stain_, palette);
    // Rectángulo visible en coordenadas del mundo [µm].
    const Vector2 a = GetScreenToWorld2D({0, 0}, camera);
    const Vector2 b = GetScreenToWorld2D({static_cast<float>(width()), static_cast<float>(height())}, camera);
    const Rectangle view = {a.x, a.y, b.x - a.x, b.y - a.y};

    cells_.renderCoverage(world, camera, view);
    overlay_.update(world, overlay);
    // Todo el campo se dibuja en una textura y después pasa por la óptica del microscopio.
    BeginTextureMode(scene_);
    {
        const Rgba nat = rgb8(kBackground.r, kBackground.g, kBackground.b);
        const Rgba bg = stain_ ? stainBackground(*stain_, nat) : nat;
        ClearBackground({static_cast<unsigned char>(bg.r * 255.0f + 0.5f), static_cast<unsigned char>(bg.g * 255.0f + 0.5f),
                         static_cast<unsigned char>(bg.b * 255.0f + 0.5f), 255});
    }
    BeginMode2D(camera);

    // Sin placa, el mundo es un toro: se dibujan las copias que caen dentro de la vista. Con
    // placa es cerrado: una sola copia.
    const float w = world.width(), h = world.height();
    const int reach = world.dish().active() ? 0 : 1;
    const float minSize = 1.5f / camera.zoom; // tamaño real en µm, con un mínimo visible
    const AmbientParticles& amb = world.ambient();
    copies_.clear();
    for (int oy = -reach; oy <= reach; ++oy)
        for (int ox = -reach; ox <= reach; ++ox)
            if (CheckCollisionRecs({ox * w, oy * h, w, h}, view))
                copies_.push_back({ox * w, oy * h});
    gatherPopulations(world, view, camera.zoom, fluor, palette);
    {
        for (const Vector2 copyOffset : copies_) {
            const float offX = copyOffset.x, offY = copyOffset.y;
            if (!fluor)
                overlay_.draw(world, offX, offY);
            const float x0 = view.x - offX - 5.0f, x1 = view.x + view.width - offX + 5.0f;
            const float y0 = view.y - offY - 5.0f, y1 = view.y + view.height - offY + 5.0f;
            auto visible = [&](float x, float y) { return x > x0 && x < x1 && y > y0 && y < y1; };

            if (!fluor) { // en fluorescencia solo se ve el ADN teñido
            for (std::size_t i = 0; i < amb.size(); ++i) {
                if (!visible(amb.x[i], amb.y[i]))
                    continue;
                const float d = std::max(2.0f * amb.radius[i], minSize);
                if (amb.kind[i] == ParticleKind::Ink)
                    DrawRectangleV({amb.x[i] + offX - d * 0.5f, amb.y[i] + offY - d * 0.5f}, {d, d}, kInk);
                else if (amb.kind[i] == ParticleKind::Mineral)
                    drawMineral(amb.mineral[i], {amb.x[i] + offX, amb.y[i] + offY}, amb.radius[i], amb.angle[i], minSize);
                else // detritos: grumos redondeados
                    DrawCircleV({amb.x[i] + offX, amb.y[i] + offY}, d * 0.5f, kDetritus);
            }
            // Granos de arena (debajo de las células, que se dibujan después).
            for (const auto [e, body, rock] : world.registry().view<RingBody, Rock>().each()) {
                if (body.aabbMax.x + offX < view.x || body.aabbMin.x + offX > view.x + view.width ||
                    body.aabbMax.y + offY < view.y || body.aabbMin.y + offY > view.y + view.height)
                    continue;
                rockPts_.resize(body.poly.size());
                for (std::size_t k = 0; k < body.poly.size(); ++k)
                    rockPts_[k] = {body.poly[k].x + offX, body.poly[k].y + offY};
                drawRock(rockPts_.data(), static_cast<int>(rockPts_.size()), {body.centroid.x + offX, body.centroid.y + offY},
                         rock.kind, rock.seed, std::max(2.5f, 2.0f * minSize));
            }
            // Cianobacterias filamentosas: cadenas de células verde-azuladas; los heterocistos,
            // más grandes, pálidos y de pared gruesa.
            for (const CyanoFilament& fil : world.cyanobacteria().filaments) {
                if (fil.x.empty())
                    continue;
                // La cadena es contigua: se lleva a la imagen de esta copia por su primera célula.
                const float fx = wrapCoord(fil.x[0], w) - fil.x[0], fy = wrapCoord(fil.y[0], h) - fil.y[0];
                if (!visible(fil.x[0] + fx, fil.y[0] + fy) && !visible(fil.x.back() + fx, fil.y.back() + fy))
                    continue;
                if (fil.oscillatoria) {
                    // Oscillatoria: tricoma continuo de células discoidales (paredes transversales
                    // apretadas), verde azulado oscuro.
                    for (std::size_t k = 0; k + 1 < fil.x.size(); ++k) {
                        const Vector2 a = {fil.x[k] + fx + offX, fil.y[k] + fy + offY};
                        const Vector2 b = {fil.x[k + 1] + fx + offX, fil.y[k + 1] + fy + offY};
                        DrawLineEx(a, b, std::max(5.5f, minSize), {58, 104, 100, 215});
                        const Vector2 d = {b.x - a.x, b.y - a.y};
                        const float len = std::max(1e-3f, std::sqrt(d.x * d.x + d.y * d.y));
                        const Vector2 m = {0.5f * (a.x + b.x), 0.5f * (a.y + b.y)}, n = {-d.y / len * 2.6f, d.x / len * 2.6f};
                        DrawLineEx({m.x - n.x, m.y - n.y}, {m.x + n.x, m.y + n.y}, std::max(0.4f, 0.5f * minSize), {36, 70, 68, 200});
                    }
                    const Vector2 tip = {fil.x.back() + fx + offX, fil.y.back() + fy + offY};
                    DrawCircleV(tip, std::max(2.75f, 0.5f * minSize), {58, 104, 100, 215}); // extremo redondeado
                    DrawCircleV({fil.x[0] + fx + offX, fil.y[0] + fy + offY}, std::max(2.75f, 0.5f * minSize), {58, 104, 100, 215});
                    continue;
                }
                for (std::size_t k = 0; k < fil.x.size(); ++k) {
                    const Vector2 p = {fil.x[k] + fx + offX, fil.y[k] + fy + offY};
                    if (fil.heterocyst[k]) {
                        DrawCircleV(p, 3.0f, {214, 222, 180, 230});
                        DrawRing(p, 2.4f, 3.0f, 0, 360, 16, {120, 140, 100, 230});
                    } else {
                        DrawCircleV(p, std::max(2.2f, minSize), {64, 128, 112, 210});
                    }
                }
            }
            // Hongos: hifas como tubos hialinos (Saprolegnia, ~12 µm), zoosporangios en las puntas
            // y zoosporas.
            for (const Mycelium& m : world.fungi().mycelia) {
                const float fx = wrapCoord(m.x[0], w) - m.x[0], fy = wrapCoord(m.y[0], h) - m.y[0];
                const FungusInfo& info = fungusInfo(m.kind);
                const float width = info.hyphaWidth;
                const Color hypha = {info.r, info.g, info.b, 120};
                const Color wallColor = {static_cast<unsigned char>(info.r * 0.64f), static_cast<unsigned char>(info.g * 0.64f),
                                         static_cast<unsigned char>(info.b * 0.64f), 170};
                for (std::size_t k = 0; k < m.x.size(); ++k) {
                    const Vector2 p = {m.x[k] + fx + offX, m.y[k] + fy + offY};
                    if (!visible(m.x[k] + fx, m.y[k] + fy))
                        continue;
                    if (m.parent[k] >= 0) {
                        const auto pk = static_cast<std::size_t>(m.parent[k]);
                        const Vector2 q = {m.x[pk] + fx + offX, m.y[pk] + fy + offY};
                        DrawLineEx(q, p, width + 1.2f, wallColor);
                        DrawLineEx(q, p, width - 0.6f, hypha);
                    }
                    if (m.sporangium[k] > 0.0f) {
                        const float r = 0.5f * width + 6.0f * std::min(1.0f, m.sporangium[k]);
                        DrawCircleV(p, r, kSporangium);
                    }
                }
            }
            // Allomyces: esporangios de resistencia (pared gruesa y parda, con poros).
            const RestingSporangia& rs = world.fungi().resting;
            for (std::size_t i = 0; i < rs.size(); ++i) {
                if (!visible(rs.x[i], rs.y[i]))
                    continue;
                const Vector2 p = {rs.x[i] + offX, rs.y[i] + offY};
                DrawCircleV(p, std::max(6.0f, minSize), kResting);
                DrawCircleV(p, std::max(3.6f, 0.6f * minSize), {176, 140, 96, 230});
                DrawCircleV({p.x - 1.6f, p.y - 1.4f}, 0.9f, kResting); // poro
            }
            // Líquenes: el haustorio que sujeta a cada alga verde.
            for (auto [e, body, li] : world.registry().view<RingBody, Lichenized>().each()) {
                if (!visible(li.point.x, li.point.y))
                    continue;
                DrawLineEx({li.point.x + offX, li.point.y + offY}, {body.centroid.x + offX, body.centroid.y + offY}, 1.4f, {96, 110, 80, 200});
                DrawCircleV({li.point.x + offX, li.point.y + offY}, 2.4f, {96, 110, 80, 220});
            }
            // Physarum: baba, copos de avena, red de tubos amarilla y esclerocios.
            const PlasmodiumSystem& ps = world.plasmodia();
            if (!ps.plasmodia.empty()) {
                const Grid2D& slime = world.slime();
                const int sx0 = std::max(0, slime.cellX(std::max(x0, 0.0f))), sx1 = std::min(slime.nx - 1, slime.cellX(std::min(x1, w - 1.0f)));
                const int sy0 = std::max(0, slime.cellY(std::max(y0, 0.0f))), sy1 = std::min(slime.ny - 1, slime.cellY(std::min(y1, h - 1.0f)));
                for (int cy = sy0; cy <= sy1; ++cy)
                    for (int cx = sx0; cx <= sx1; ++cx)
                        if (slime.at(cx, cy) > 0.25f)
                            DrawRectangleV({static_cast<float>(cx) * slime.dx + offX, static_cast<float>(cy) * slime.dx + offY},
                                           {slime.dx, slime.dx}, kSlime);
            }
            for (std::size_t i = 0; i < ps.flakes.size(); ++i) {
                if (!visible(ps.flakes.x[i], ps.flakes.y[i]))
                    continue;
                const float s = 4.0f + 0.12f * ps.flakes.pmol[i];
                const Vector2 p = {ps.flakes.x[i] + offX, ps.flakes.y[i] + offY};
                DrawEllipse(static_cast<int>(p.x), static_cast<int>(p.y), s * 1.3f, s, kFlake);
                DrawEllipseLines(static_cast<int>(p.x), static_cast<int>(p.y), s * 1.3f, s, kFlakeEdge);
            }
            for (const Plasmodium& pl : ps.plasmodia) {
                if (pl.x.empty())
                    continue;
                const float fx = wrapCoord(pl.x[0], w) - pl.x[0], fy = wrapCoord(pl.y[0], h) - pl.y[0];
                // Dos pasadas (paredes y después citoplasma, con juntas redondas) para que los
                // tubos se vean continuos y no como segmentos.
                for (int pass = 0; pass < 2; ++pass)
                    for (const Plasmodium::Edge& e : pl.edges) {
                        const auto a = static_cast<std::size_t>(e.a), b = static_cast<std::size_t>(e.b);
                        if (!visible(pl.x[a] + fx, pl.y[a] + fy) && !visible(pl.x[b] + fx, pl.y[b] + fy))
                            continue;
                        // Hagen–Poiseuille: el radio crece como D^¼.
                        const float width = std::max(1.5f, 12.0f * std::pow(e.D, 0.25f)) + (pass == 0 ? 1.6f : 0.0f);
                        const Vector2 p = {pl.x[a] + fx + offX, pl.y[a] + fy + offY}, q = {pl.x[b] + fx + offX, pl.y[b] + fy + offY};
                        const Color col = pass == 0 ? kPlasmodiumWall : kPlasmodium;
                        DrawLineEx(p, q, width, col);
                        DrawCircleV(p, 0.5f * width, col);
                        DrawCircleV(q, 0.5f * width, col);
                    }
                for (std::size_t k = 0; k < pl.x.size(); ++k) {
                    if (!visible(pl.x[k] + fx, pl.y[k] + fy))
                        continue;
                    const Vector2 p = {pl.x[k] + fx + offX, pl.y[k] + fy + offY};
                    if (pl.tip[k]) // frente de exploración: abanico más ancho
                        DrawCircleV(p, 6.0f, {240, 214, 90, 110});
                    else if (pl.food[k] >= 0)
                        DrawCircleV(p, 8.0f, {236, 204, 64, 170});
                }
            }
            for (std::size_t i = 0; i < ps.sclerotia.size(); ++i) {
                if (!visible(ps.sclerotia.x[i], ps.sclerotia.y[i]))
                    continue;
                const Vector2 p = {ps.sclerotia.x[i] + offX, ps.sclerotia.y[i] + offY};
                DrawCircleV(p, 7.0f, kSclerotium);
                DrawCircleLinesV(p, 7.0f, {120, 70, 20, 230});
            }
            const Zoospores& zs = world.fungi().spores;
            for (std::size_t i = 0; i < zs.size(); ++i) {
                if (!visible(zs.x[i], zs.y[i]))
                    continue;
                const Vector2 p = {zs.x[i] + offX, zs.y[i] + offY};
                const float r = zs.kind[i] == FungusKind::Chytrid ? 1.6f : 2.0f;
                DrawCircleV(p, std::max(r, minSize), kSpore);
                if (fungusInfo(zs.kind[i]).sporeSpeed > 0.0f) // flagelo posterior (quitridio) o dos (oomiceto)
                    DrawLineEx(p, {p.x - std::cos(zs.heading[i]) * 7.0f, p.y - std::sin(zs.heading[i]) * 7.0f},
                               std::max(0.3f, 0.5f * minSize), kSpore);
            }
            } // !fluor
        }
    }
    // Fagos libres (vista de modelo), bacterias y Bdellovibrio: densidad, individuos y, de cerca,
    // sus estructuras.
    drawPhageField(world, view, camera.zoom);
    drawPopulations(camera.zoom, palette);
    drawDetailed(world, view, camera.zoom, palette);
    EndMode2D();

    cells_.composite(camera);

    BeginMode2D(camera);
    cells_.drawDetails(world, camera, view);
    if (!fluor)
    // Lisis: la membrana revienta, el contenido se dispersa y el fantasma se desvanece.
    for (const LysisMark& m : world.lysisMarks()) {
        const float age = world.simTime() - m.time;
        const float fade = std::clamp(1.0f - age / 3.0f, 0.0f, 1.0f);
        for (int oy = -reach; oy <= reach; ++oy)
            for (int ox = -reach; ox <= reach; ++ox) {
                const Vector2 c = {m.at.x + ox * world.width(), m.at.y + oy * world.height()};
                if (c.x < view.x - m.radius * 3 || c.x > view.x + view.width + m.radius * 3 ||
                    c.y < view.y - m.radius * 3 || c.y > view.y + view.height + m.radius * 3)
                    continue;
                const float r = m.radius * (1.0f + 0.5f * age);
                DrawCircleV(c, r, {m.r, m.g, m.b, static_cast<unsigned char>(50 * fade)});
                DrawRing(c, r - 1.5f, r, 0, 360, 48, {90, 90, 70, static_cast<unsigned char>(120 * fade)});
                for (int k = 0; k < 24; ++k) { // gránulos que salen despedidos
                    const float a = 6.2831853f * static_cast<float>(k) / 24.0f + 0.37f * static_cast<float>(k % 5);
                    const float d = m.radius * (0.3f + 0.9f * age * (0.5f + 0.5f * std::sin(3.1f * k)));
                    DrawCircleV({c.x + std::cos(a) * d, c.y + std::sin(a) * d}, 0.8f, {60, 62, 48, static_cast<unsigned char>(170 * fade)});
                }
            }
    }
    EndMode2D();
    drawDish(world, camera);
    EndTextureMode();

    const auto sw = static_cast<float>(scene_.texture.width), sh = static_cast<float>(scene_.texture.height);
    const Rectangle src = {0, 0, sw, -sh}; // las render textures están invertidas en Y
    if (out_)
        BeginTextureMode(*out_);
    // La vista de modelo es un esquema, no una imagen de microscopio: sin la óptica simulada.
    if (postOk_ && postEnabled_ && lod_.view == ViewMode::Microscope) {
        const float res[2] = {sw, sh};
        const float t = static_cast<float>(GetTime());
        SetShaderValue(post_, resolutionLoc_, res, SHADER_UNIFORM_VEC2);
        SetShaderValue(post_, timeLoc_, &t, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(post_);
        DrawTextureRec(scene_.texture, src, {0, 0}, WHITE);
        EndShaderMode();
    } else {
        DrawTextureRec(scene_.texture, src, {0, 0}, WHITE);
    }
    if (out_)
        EndTextureMode();
}
