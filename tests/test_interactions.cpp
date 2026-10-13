// Fases 8 y 9: algas compuestas, hongos, interacciones microbianas, metazoos y LOD.
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>

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

} // namespace

TEST_CASE("Spirogyra: una célula del filamento se divide con un tabique y las uniones se conservan") {
    World world(quietWorld());
    const auto chain = world.spawnFilament(SpeciesId::Spirogyra, {700.0f, 330.0f}, 0.15f, 7, 0);
    REQUIRE(world.links().size() == 12);
    Metabolism& m = world.registry().get<Metabolism>(chain[3]);
    m.biomass = 2.05f * m.targetBirthBiomass;
    m.reserves = 0.6f * m.biomass;
    m.phosphorus = 0.05f * m.biomass;
    m.nitrogen = 0.5f * m.biomass;
    steps(world, 60 * 40);
    CHECK(world.cellCount() == 8);
    CHECK(world.links().size() == 14);
    CHECK(world.observations().filamentDivisions == 1);
}

TEST_CASE("el nitrógeno se conserva sin fijadoras") {
    World world(quietWorld(3000));
    for (int k = 0; k < 10; ++k)
        wild(world, SpeciesId::Chlamydomonas, {300.0f + 120.0f * k, 400.0f});
    wild(world, SpeciesId::Paramecium, {900.0f, 600.0f});
    const double before = world.totalNitrogen();
    steps(world, 60 * 20);
    MESSAGE("nitrógeno " << before << " → " << world.totalNitrogen());
    CHECK(world.totalNitrogen() == doctest::Approx(before).epsilon(0.01));
}

TEST_CASE("los heterocistos fijan N₂: el nitrógeno total aumenta") {
    WorldConfig config = quietWorld();
    config.cyanoFilaments = 20;
    World world(config);
    const double before = world.totalNitrogen();
    steps(world, 60 * 20);
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

TEST_CASE("fagos líticos: las bacterias sensibles se infectan y estallan") {
    World world(quietWorld(20000));
    world.bioTimeScale() = 600.0f; // 20 s = 3 h biológicas
    steps(world, 60 * 20);
    CHECK(world.observations().phageLyses > 0);
}

TEST_CASE("Bdellovibrio entra a una bacteria, crece adentro y sale con sus hijas") {
    World world(quietWorld(30000));
    world.bioTimeScale() = 600.0f; // 30 s = 5 h biológicas (crece ~3 h adentro)
    steps(world, 60 * 30);
    CHECK(world.observations().bdelloBursts > 0);
}

TEST_CASE("el rotífero pone un huevo partenogenético que eclosiona") {
    World world(quietWorld(20000));
    world.bioTimeScale() = 600.0f;
    const entt::entity r = wild(world, SpeciesId::Rotifer, {1000.0f, 500.0f});
    world.registry().emplace<Autopilot>(r);
    REQUIRE(world.registry().all_of<Compound>(r));
    Metabolism& m = world.registry().get<Metabolism>(r);
    m.biomass = 1.5f * m.birthBiomass;
    m.reserves = 0.6f * m.biomass;
    steps(world, 60 * 150); // 25 h biológicas: el huevo tarda ~20 h
    CHECK(world.observations().eggsLaid > 0);
    CHECK(world.observations().eggsHatched > 0);
}

TEST_CASE("Hydra paraliza a un paramecio con los tentáculos y lo traga") {
    World world(quietWorld());
    const entt::entity h = wild(world, SpeciesId::Hydra, {1000.0f, 600.0f}, -1.5708f);
    world.registry().emplace<Anchored>(h, Anchored{{1000.0f, 820.0f}, 5.0f});
    Genome g = speciesGenome(SpeciesId::Paramecium);
    const entt::entity p = world.spawnOrganism(SpeciesId::Paramecium, {1000.0f, 150.0f}, g, 0, 1.5708f);
    for (int s = 0; s < 60 * 120 && world.registry().valid(p); ++s)
        world.step(kDt);
    CHECK(world.observations().hydraCaptures > 0);
    CHECK_FALSE(world.registry().valid(p));
}

TEST_CASE("LOD: con el foco lejos, las células siguen sanas y la simulación es determinista") {
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
