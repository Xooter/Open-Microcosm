// Placa: el mundo es un pocillo circular con pared de vidrio.
#include "core/finite.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <doctest/doctest.h>

#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig dishWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 40;
    config.cyanoFilaments = 4;
    config.seed = 31;
    return config;
}

// Mayor distancia de un punto de membrana a la pared, hacia afuera [µm] (≤ 0: todo adentro).
float worstMembrane(const World& world) {
    float worst = -1e9f;
    const BodyParticles& bp = world.bodies();
    for (auto [e, body] : world.registry().view<RingBody>().each())
        for (std::uint32_t s : body.ring)
            worst = std::max(worst, world.dish().signedDistance({bp.x[s], bp.y[s]}));
    return worst;
}

} // namespace

TEST_CASE("la placa es un círculo centrado con margen de vidrio") {
    World world(dishWorld());
    const Dish& d = world.dish();
    REQUIRE(d.active());
    CHECK(d.center.x == doctest::Approx(0.5f * world.width()));
    CHECK(d.center.y == doctest::Approx(0.5f * world.height()));
    CHECK(d.radius < 0.5f * std::min(world.width(), world.height()));
    CHECK(world.dishMask().waterCells < static_cast<std::size_t>(256 * 256));
}

TEST_CASE("las células que nadan contra la pared no la atraviesan") {
    World world(dishWorld());
    const Dish& d = world.dish();
    // Nadadores apuntando hacia afuera, cerca del borde, en varias direcciones.
    for (int k = 0; k < 8; ++k) {
        const float a = 6.2831853f * static_cast<float>(k) / 8.0f;
        const Vec2 at = d.center + Vec2{std::cos(a), std::sin(a)} * (d.radius - 90.0f);
        const SpeciesId id = k % 2 ? SpeciesId::Paramecium : SpeciesId::Euglena;
        const entt::entity e = world.spawnOrganism(id, at, speciesGenome(id), 0, a);
        Locomotion& loco = world.registry().get<Locomotion>(e);
        loco.active = true;
        loco.forward = 1.0f;
        loco.target = at + Vec2{std::cos(a), std::sin(a)} * 500.0f;
    }
    float worst = -1e9f;
    for (int s = 0; s < 60 * 6; ++s) {
        world.step(kDt);
        worst = std::max(worst, worstMembrane(world));
    }
    MESSAGE("peor penetración de la pared: " << worst << " µm");
    CHECK(worst <= 0.01f);
}

TEST_CASE("bacterias, detritos y cianobacterias quedan dentro del agua") {
    World world(dishWorld(4000));
    // Agua revuelta hacia la pared: arrastra todo contra el vidrio.
    const Dish& d = world.dish();
    for (int s = 0; s < 60 * 5; ++s) {
        world.setStir(d.center + Vec2{d.radius - 150.0f, 0.0f}, {400.0f, 0.0f}, 120.0f);
        world.step(kDt);
    }
    const Bacteria& bac = world.bacteria();
    int out = 0;
    for (std::size_t i = 0; i < bac.size(); ++i)
        out += d.contains({bac.x[i], bac.y[i]}) ? 0 : 1;
    const AmbientParticles& amb = world.ambient();
    for (std::size_t i = 0; i < amb.size(); ++i)
        out += d.contains({amb.x[i], amb.y[i]}) ? 0 : 1;
    for (const CyanoFilament& f : world.cyanobacteria().filaments)
        for (std::size_t k = 0; k < f.x.size(); ++k)
            out += d.contains({f.x[k], f.y[k]}) ? 0 : 1;
    CHECK(out == 0);
}

TEST_CASE("el vidrio no se lleva fósforo: se conserva y no se acumula afuera" * doctest::test_suite("largos")) {
    WorldConfig config = dishWorld(2000);
    World world(config);
    for (int k = 0; k < 6; ++k)
        world.spawnOrganism(SpeciesId::Chlamydomonas, world.dish().center + Vec2{120.0f * k - 300.0f, 0.0f},
                            speciesGenome(SpeciesId::Chlamydomonas), 0);
    const double before = world.totalPhosphorus();
    for (int s = 0; s < 60 * 10; ++s)
        world.step(kDt);
    CHECK(world.totalPhosphorus() == doctest::Approx(before).epsilon(0.01));
    // Las celdas de vidrio son fantasmas: copian su imagen en el agua (no guardan masa propia).
    const DishMask& mask = world.dishMask();
    const Grid2D& p = world.phosphate().grid();
    for (std::size_t m = 0; m < mask.outside.size(); m += 97) {
        REQUIRE(finiteNumber(p.v[mask.outside[m]]));
        CHECK(p.v[mask.outside[m]] == doctest::Approx(p.v[mask.mirror[m]]));
    }
}

TEST_CASE("el agua no fluye dentro del vidrio") {
    World world(dishWorld());
    const Dish& d = world.dish();
    world.setStir(d.center + Vec2{d.radius - 40.0f, 0.0f}, {600.0f, 0.0f}, 150.0f);
    world.step(kDt);
    const Vec2 glass = world.fluid().velocityAt(d.center + Vec2{d.radius + 30.0f, 0.0f});
    const Vec2 water = world.fluid().velocityAt(d.center + Vec2{d.radius - 100.0f, 0.0f});
    CHECK(length(glass) == doctest::Approx(0.0f));
    CHECK(length(water) > 1.0f);
}


TEST_CASE("pared: un ciliado que retrocede contra el vidrio no se pliega") {
    // Una película no se amolda al vidrio: el cuerpo entero se aparta. Proyectando punto por punto,
    // la punta posterior de un Paramecium que retrocede contra la pared (reacción de evitación) se
    // aplastaba y la membrana quedaba plegada (docs/bugs/ciliate-ring-crossed-after-division.md).
    for (const std::uint64_t seed : {17ull, 20ull}) {
        WorldConfig c;
        c.bacteria = 0;
        c.detritus = 0;
        c.cyanoFilaments = 0;
        c.saprolegnia = c.chytridClusters = 0;
        c.seed = seed;
        World w(c);
        REQUIRE(w.inoculate({BeingKind::Species, SpeciesId::Paramecium}, 1, {}, 0.0f) == 1);
        int crossed = 0, samples = 0, nearWall = 0;
        for (int s = 0; s < 60 * 40; ++s) {
            w.step(1.0f / 60.0f);
            if (s % 10)
                continue;
            for (auto [e, b, sp] : w.registry().view<RingBody, Species>().each()) {
                ++samples;
                crossed += polygon::selfIntersects(b.poly.data(), static_cast<int>(b.poly.size())) ? 1 : 0;
                float wall = 1e9f;
                for (const Vec2& p : b.poly)
                    wall = std::min(wall, -w.dish().signedDistance(p));
                nearWall += wall < 5.0f ? 1 : 0;
            }
        }
        MESSAGE("seed " << seed << ": " << crossed << " de " << samples << " muestras con la membrana cruzada; " << nearWall
                        << " contra la pared");
        CHECK(nearWall > 0); // llegó a chocar
        CHECK(crossed == 0);
    }
}
