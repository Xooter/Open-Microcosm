// Fases 8 y 9: algas compuestas, hongos, interacciones microbianas y LOD.
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <map>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig quietWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.seed = 77;
    return config;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

entt::entity wild(World& world, SpeciesId id, Vec2 at, float angle = 0.0f) {
    return world.spawnOrganism(id, at, speciesGenome(id), 0, angle);
}

// Paredes (pares de células unidas) y enlaces por pared.
std::map<std::pair<entt::entity, entt::entity>, int> walls(const World& world) {
    std::map<std::pair<entt::entity, entt::entity>, int> w;
    for (const World::AdhesionLink& l : world.links())
        ++w[{std::min(l.ownerA, l.ownerB), std::max(l.ownerA, l.ownerB)}];
    return w;
}

} // namespace

TEST_CASE("Spirogyra: la unión entre células es la pared transversal entera, no una bisagra") {
    World world(quietWorld());
    const auto chain = world.spawnFilament(SpeciesId::Spirogyra, {700.0f, 330.0f}, 0.15f, 4, 0);
    const auto w = walls(world);
    REQUIRE(w.size() == 3);
    const float width = 2.0f * speciesInfo(SpeciesId::Spirogyra).spec.radiusY;
    for (const auto& [pair, n] : w) {
        CHECK(n >= 5); // de frente a lo ancho, más los cruzados
        // Los extremos de la pared en la célula A abarcan casi todo el ancho (antes, ~5 µm).
        float lo = 1e9f, hi = -1e9f;
        const RingBody& a = world.registry().get<RingBody>(pair.first);
        const Vec2 axis = {std::cos(0.15f), std::sin(0.15f)};
        for (const World::AdhesionLink& l : world.links()) {
            const std::uint32_t s = l.ownerA == pair.first ? l.a : l.ownerB == pair.first ? l.b : ~0u;
            if (s == ~0u || !(l.ownerA == pair.second || l.ownerB == pair.second))
                continue;
            const float side = cross(axis, Vec2{world.bodies().x[s], world.bodies().y[s]} - a.centroid);
            lo = std::min(lo, side);
            hi = std::max(hi, side);
        }
        CHECK(hi - lo > 0.7f * width);
    }
}

TEST_CASE("Spirogyra: la fragmentación corta paredes enteras") {
    World world(quietWorld());
    world.bioTimeScale() = 3600.0f; // 1 s = 1 h: con 0,02/h por pared, alguna se corta en segundos
    world.spawnFilament(SpeciesId::Spirogyra, {700.0f, 330.0f}, 0.15f, 7, 0);
    const auto before = walls(world);
    const int perWall = before.begin()->second;
    for (int s = 0; s < 60 * 60 && walls(world).size() == before.size(); ++s)
        world.step(kDt);
    const auto after = walls(world);
    REQUIRE(after.size() < before.size());
    for (const auto& [pair, n] : after)
        CHECK(n == perWall); // ninguna pared a medias
}

TEST_CASE("Spirogyra: una célula del filamento se divide con un tabique y las uniones se conservan" * doctest::test_suite("largos")) {
    World world(quietWorld());
    const auto chain = world.spawnFilament(SpeciesId::Spirogyra, {700.0f, 330.0f}, 0.15f, 7, 0);
    REQUIRE(walls(world).size() == 6);
    Metabolism& m = world.registry().get<Metabolism>(chain[3]);
    m.biomass = 2.05f * m.targetBirthBiomass;
    m.reserves = 0.6f * m.biomass;
    m.phosphorus = 0.05f * m.biomass;
    m.nitrogen = 0.5f * m.biomass;
    steps(world, 60 * 40);
    CHECK(world.cellCount() == 8);
    CHECK(walls(world).size() == 7);
    CHECK(world.observations().filamentDivisions == 1);
}

TEST_CASE("el nitrógeno se conserva sin fijadoras" * doctest::test_suite("largos")) {
    World world(quietWorld(3000));
    for (int k = 0; k < 10; ++k)
        wild(world, SpeciesId::Chlamydomonas, {300.0f + 120.0f * k, 400.0f});
    wild(world, SpeciesId::Paramecium, {900.0f, 600.0f});
    const double before = world.totalNitrogen();
    steps(world, 60 * 20);
    MESSAGE("nitrógeno " << before << " → " << world.totalNitrogen());
    CHECK(world.totalNitrogen() == doctest::Approx(before).epsilon(0.01));
}

TEST_CASE("los heterocistos fijan N₂: el nitrógeno total aumenta" * doctest::test_suite("largos")) {
    WorldConfig config = quietWorld();
    config.cyanoFilaments = 20;
    World world(config);
    // Tiempo biológico acelerado: lo fijado tiene que superar el redondeo de la grilla (float).
    world.bioTimeScale() = 3600.0f;
    const double before = world.totalNitrogen();
    steps(world, 60 * 20);
    MESSAGE("nitrógeno " << before << " → " << world.totalNitrogen() << ", fijado " << world.observations().nitrogenFixed);
    CHECK(world.totalNitrogen() > before);
    CHECK(world.observations().nitrogenFixed > 0.0f);
}

TEST_CASE("quitridios: la zoospora infecta un alga, la consume y libera zoosporas") {
    World world(quietWorld());
    world.bioTimeScale() = 600.0f;
    const entt::entity alga = wild(world, SpeciesId::Navicula, {1000.0f, 500.0f});
    world.fungi().spawnSpores(FungusKind::Chytrid, {1000.0f, 470.0f}, 5, 0.0f, 1, 1);
    const double phosphorus = world.totalPhosphorus();
    bool infected = false;
    for (int s = 0; s < 60 * 60 && world.observations().chytridReleases == 0; ++s) {
        world.step(kDt);
        infected |= world.registry().valid(alga) && world.registry().all_of<ChytridInfection>(alga);
    }
    CHECK(infected);
    CHECK(world.observations().chytridReleases > 0);
    CHECK(world.totalPhosphorus() == doctest::Approx(phosphorus).epsilon(0.01));
}

TEST_CASE("fagos líticos: las bacterias sensibles se infectan y estallan" * doctest::test_suite("largos")) {
    World world(quietWorld(20000));
    world.bioTimeScale() = 600.0f; // 20 s = 3 h biológicas
    steps(world, 60 * 20);
    CHECK(world.observations().phageLyses > 0);
}

TEST_CASE("Bdellovibrio entra a una bacteria, crece adentro y sale con sus hijas" * doctest::test_suite("largos")) {
    World world(quietWorld(30000));
    world.bioTimeScale() = 600.0f; // 30 s = 5 h biológicas (crece ~3 h adentro)
    steps(world, 60 * 30);
    CHECK(world.observations().bdelloBursts > 0);
}

TEST_CASE("LOD: con el foco lejos, las células siguen sanas y la simulación es determinista" * doctest::test_suite("largos")) {
    auto run = [] {
        World world(quietWorld(5000));
        world.populateEcosystem(0.5f);
        world.setFocus({200.0f, 200.0f}, {300.0f, 200.0f});
        steps(world, 60 * 5);
        return std::pair{world.stateHash(), world.nearBodies() < world.cellCount()};
    };
    const auto [a, fewer] = run();
    const auto [b, fewer2] = run();
    CHECK(fewer);
    CHECK(a == b);
    (void)fewer2;
}
