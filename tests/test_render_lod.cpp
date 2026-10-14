// Zoom semántico: elección del nivel de detalle por tamaño en pantalla y densidad agregada de las
// poblaciones (sale de las posiciones reales y no cambia la simulación).
#include "game/inspect.hpp"
#include "game/population_density.hpp"
#include "game/world.hpp"
#include "render/lod.hpp"

#include <doctest/doctest.h>

#include <cmath>

TEST_CASE("LOD: el fundido es monótono y densidad + individuos suman 1") {
    const LodBands b = {1.5f, 4.0f, 10.0f, 60.0f};
    float prev = -1.0f;
    for (float px = 0.1f; px < 200.0f; px *= 1.1f) {
        const LodBlend l = lodFor(px, b);
        CHECK(l.individual >= prev);
        CHECK(l.individual + l.aggregate == doctest::Approx(1.0f));
        prev = l.individual;
    }
    CHECK(lodFor(1.0f, b).level == DetailLevel::Aggregate);
    CHECK(lodFor(1.0f, b).individual == 0.0f);
    CHECK(lodFor(5.0f, b).level == DetailLevel::Simplified);
    CHECK(lodFor(5.0f, b).individual == 1.0f);
    CHECK(lodFor(20.0f, b).level == DetailLevel::Normal);
    CHECK(lodFor(80.0f, b).level == DetailLevel::Detail);
}

TEST_CASE("LOD: el tamaño en pantalla (no el zoom) decide; sin zoom semántico todo es individual") {
    RenderLodSettings s;
    // Bdellovibrio (1 µm) sigue agregado a un zoom en el que las bacterias (2 µm) ya se ven.
    CHECK(s.blend(Population::Bacteria, 2.5f).individual == 1.0f);
    CHECK(s.blend(Population::Bdellovibrio, 2.5f).individual < 1.0f);
    s.enabled = false;
    CHECK(s.blend(Population::Bacteria, 0.4f).individual == 1.0f);
    CHECK(s.blend(Population::Bacteria, 0.4f).level == DetailLevel::Simplified);
    CHECK(s.cellLevel(5.0f, 0.4f) == DetailLevel::Detail);
}

TEST_CASE("Densidad: cuenta cada individuo una vez, en celdas potencia de 2 ancladas al mundo") {
    DensityGrid g;
    g.reset(10.0f, 20.0f, 330.0f, 260.0f, 5.0f, 1 << 16, 2);
    CHECK(g.binSize() == 8.0f);
    CHECK(std::fmod(g.originX(), g.binSize()) == 0.0f);
    CHECK(std::fmod(g.originY(), g.binSize()) == 0.0f);
    int inside = 0;
    for (int i = 0; i < 1000; ++i) {
        const float x = 12.0f + static_cast<float>((i * 37) % 300), y = 25.0f + static_cast<float>((i * 53) % 230);
        g.add(x, y, i % 2, 1.0f);
        ++inside;
    }
    g.add(-50.0f, 30.0f, 0, 1.0f); // fuera de la grilla: no cuenta
    CHECK(g.sum() == doctest::Approx(inside));

    // Desplazar la vista medio píxel no mueve las celdas (la imagen no tiembla).
    DensityGrid h;
    h.reset(10.5f, 20.5f, 330.5f, 260.5f, 5.0f, 1 << 16, 2);
    CHECK(h.originX() == g.originX());
    CHECK(h.binSize() == g.binSize());

    // Con tope de celdas, las celdas crecen en vez de multiplicarse.
    DensityGrid c;
    c.reset(0.0f, 0.0f, 2048.0f, 2048.0f, 1.0f, 4096, 1);
    CHECK(c.nx() * c.ny() <= 4096);
}

TEST_CASE("Densidad: el suavizado conserva el total y el cúmulo sigue en su lugar") {
    DensityGrid g;
    g.reset(0.0f, 0.0f, 256.0f, 256.0f, 8.0f, 1 << 16, 1);
    for (int i = 0; i < 200; ++i)
        g.add(100.0f + static_cast<float>(i % 7), 60.0f + static_cast<float>(i % 5), 0, 1.0f);
    g.add(250.0f, 250.0f, 0, 1.0f); // en el borde (espejado)
    const double before = g.sum();
    g.smooth(2);
    CHECK(g.sum() == doctest::Approx(before));
    const int cx = static_cast<int>(103.0f / g.binSize()), cy = static_cast<int>(62.0f / g.binSize());
    float best = 0.0f;
    int bx = -1, by = -1;
    for (int y = 0; y < g.ny(); ++y)
        for (int x = 0; x < g.nx(); ++x)
            if (g.at(x, y, 0) > best) {
                best = g.at(x, y, 0);
                bx = x;
                by = y;
            }
    CHECK(bx == cx);
    CHECK(by == cy);
}

TEST_CASE("Densidad de la población real: la grilla suma las bacterias de la región y no toca el mundo") {
    WorldConfig config;
    config.bacteria = 4000;
    config.seed = 77;
    World world(config);
    const Bacteria& bac = world.bacteria();
    const float x0 = 600.0f, y0 = 700.0f, x1 = 1400.0f, y1 = 1300.0f;
    DensityGrid g;
    g.reset(x0, y0, x1, y1, 16.0f, 1 << 16, 8);
    int expected = 0;
    const float gx1 = g.originX() + static_cast<float>(g.nx()) * g.binSize();
    const float gy1 = g.originY() + static_cast<float>(g.ny()) * g.binSize();
    for (std::size_t i = 0; i < bac.size(); ++i) {
        g.add(bac.x[i], bac.y[i], bac.strain[i], 1.0f);
        if (bac.x[i] >= g.originX() && bac.x[i] < gx1 && bac.y[i] >= g.originY() && bac.y[i] < gy1)
            ++expected;
    }
    CHECK(expected > 0);
    CHECK(g.sum() == doctest::Approx(expected));
    CHECK(bac.size() == 4000);
}

TEST_CASE("Elegir: lo que solo se ve como densidad no se puede elegir") {
    WorldConfig config;
    config.bacteria = 0;
    config.seed = 5;
    World world(config);
    const Vec2 at = {1024.0f, 1024.0f};
    world.spawnBeing({BeingKind::Bacterium, SpeciesId::Paramecium}, at);
    REQUIRE(world.bacteria().size() > 0);
    const Vec2 b = {world.bacteria().x[0], world.bacteria().y[0]};
    const Picked hit = world.pick(b, 3.0f);
    CHECK(hit.kind == Picked::Kind::Bacterium);
    const Picked skipped = world.pick(b, 3.0f, pickBit(Picked::Kind::Bacterium));
    CHECK(skipped.kind != Picked::Kind::Bacterium);
}
