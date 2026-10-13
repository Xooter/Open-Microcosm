// Fase 11: ambiente, biomas y enciclopedia.
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
#include "game/progress.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>

namespace {

constexpr float kDt = 1.0f / 60.0f;

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

WorldConfig bare(float temperature) {
    WorldConfig c;
    c.bacteria = 3000;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.zoophagus = c.chytridClusters = 0;
    c.seed = 5;
    c.medium.temperature = temperature;
    return c;
}

} // namespace

TEST_CASE("temperatura: las bacterias crecen más rápido con calor (Q10 ≈ 2)") {
    auto grow = [](float t) {
        World world(bare(t));
        world.nutrients().grid().v.assign(world.nutrients().grid().v.size(), 20.0f);
        const auto before = static_cast<double>(world.bacteria().size());
        steps(world, 60 * 30);
        return static_cast<double>(world.bacteria().size()) / before;
    };
    const double cold = grow(10.0f), warm = grow(30.0f);
    MESSAGE("crecimiento ×" << cold << " a 10 °C, ×" << warm << " a 30 °C");
    CHECK(warm > cold);
}

TEST_CASE("intestino de termita: sin oxígeno; la anaerobia Trichonympha sobrevive") {
    WorldConfig c = makeDrop(BiomeId::TermiteGut, 3);
    c.speciesWeight[static_cast<int>(SpeciesId::Trichonympha)] = 1.0f;
    World world(c);
    CHECK(world.oxygenSaturation() == 0.0f);
    const entt::entity tri = world.spawnOrganism(SpeciesId::Trichonympha, {500.0f, 500.0f}, speciesGenome(SpeciesId::Trichonympha), 0);
    const entt::entity para = world.spawnOrganism(SpeciesId::Paramecium, {1500.0f, 500.0f}, speciesGenome(SpeciesId::Paramecium), 0);
    steps(world, 60 * 20);
    // Sin O₂ el paramecio fermenta (15× más glucosa por ATP): gasta sus reservas mucho más rápido.
    CHECK(world.registry().valid(tri));
    const float triReserves = world.registry().get<Metabolism>(tri).reserves / world.registry().get<Metabolism>(tri).biomass;
    const bool paraAlive = world.registry().valid(para);
    const float paraReserves = paraAlive ? world.registry().get<Metabolism>(para).reserves / world.registry().get<Metabolism>(para).biomass : 0.0f;
    MESSAGE("reservas relativas: Trichonympha " << triReserves << ", Paramecium " << paraReserves);
    CHECK(world.oxygen().grid().v[100] < 1.0f);
}

TEST_CASE("gotas procedurales: la seed cambia la gota y el bioma fija su ambiente") {
    const WorldConfig a = makeDrop(BiomeId::Puddle, 1), b = makeDrop(BiomeId::Puddle, 2);
    CHECK(a.medium.temperature != b.medium.temperature);
    const WorldConfig ice = makeDrop(BiomeId::SeaIce, 1), spring = makeDrop(BiomeId::HotSpring, 1);
    CHECK(ice.medium.temperature < 3.0f);
    CHECK(ice.medium.salinity > 30.0f);
    CHECK(spring.medium.temperature > 35.0f);
    CHECK(spring.speciesWeight[static_cast<int>(SpeciesId::Paramecium)] == 0.0f);
    // Misma seed, misma gota.
    const WorldConfig a2 = makeDrop(BiomeId::Puddle, 1);
    CHECK(a2.medium.temperature == a.medium.temperature);
    CHECK(a2.speciesWeight[3] == a.speciesWeight[3]);
}

TEST_CASE("enciclopedia: lo que se ve se cataloga y se guarda") {
    World world(bare(20.0f));
    const entt::entity p = world.spawnOrganism(SpeciesId::Paramecium, {1000.0f, 500.0f}, speciesGenome(SpeciesId::Paramecium), 0);
    Progress progress;
    discoverVisible(progress, world, {100.0f, 100.0f}, {300.0f, 300.0f}); // fuera de la vista
    CHECK((progress.seenSpecies & (1ull << static_cast<unsigned>(SpeciesId::Paramecium))) == 0);
    discoverVisible(progress, world, {800.0f, 400.0f}, {1200.0f, 600.0f});
    CHECK((progress.seenSpecies & (1ull << static_cast<unsigned>(SpeciesId::Paramecium))) != 0);
    world.lyseAt(world.registry().get<RingBody>(p).centroid);
    CHECK_FALSE(recordBehaviors(progress, world).empty());

    const std::string path = "test_progress_encyclopedia.txt";
    REQUIRE(saveProgress(progress, path));
    const Progress loaded = loadProgress(path);
    CHECK(loaded.seenSpecies == progress.seenSpecies);
    CHECK(loaded.behaviors == progress.behaviors);
    std::remove(path.c_str());
}
