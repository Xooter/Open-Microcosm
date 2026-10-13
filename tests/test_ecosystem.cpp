#include "bio/species.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig ecoWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.seed = 66;
    return config;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

} // namespace

TEST_CASE("las bacterias crecen y se dividen con materia orgánica, y mueren sin ella") {
    World fed(ecoWorld(2000));
    for (float& v : fed.nutrients().grid().v)
        v = 20.0f;
    fed.medium().detritusSpawnRate = 0.0f;
    steps(fed, 60 * 60); // 1 h biológica
    MESSAGE("con comida: 2000 → " << fed.bacteria().size());
    CHECK(fed.bacteria().size() > 2600);

    World hungry(ecoWorld(2000));
    hungry.medium().detritusSpawnRate = 0.0f;
    hungry.bioTimeScale() = 600.0f;
    steps(hungry, 60 * 60); // 10 h biológicas
    MESSAGE("sin comida: 2000 → " << hungry.bacteria().size());
    CHECK(hungry.bacteria().size() < 2000);
}

TEST_CASE("el fósforo se conserva en el ecosistema completo") {
    World world(ecoWorld(20'000));
    world.populateEcosystem();
    const double before = world.totalPhosphorus();
    steps(world, 60 * 30);
    const double after = world.totalPhosphorus();
    MESSAGE("fósforo total " << before << " → " << after << " pmol");
    CHECK(after == doctest::Approx(before).epsilon(0.01));
}

TEST_CASE("Didinium traga un paramecio entero y queda saciado") {
    World world(ecoWorld());
    Genome prey = speciesGenome(SpeciesId::Paramecium);
    prey[Gene::Trichocysts] = 0.0f; // sin defensa
    const entt::entity paramecium = world.spawnOrganism(SpeciesId::Paramecium, {1100.0f, 512.0f}, prey, 0);
    const entt::entity didinium = world.spawnOrganism(SpeciesId::Didinium, {940.0f, 512.0f}, speciesGenome(SpeciesId::Didinium), 0);
    world.registry().emplace<Autopilot>(didinium);
    for (int s = 0; s < 60 * 15 && world.registry().valid(paramecium); ++s)
        world.step(kDt);
    CHECK_FALSE(world.registry().valid(paramecium));
    const Digestion& dig = world.registry().get<Digestion>(didinium);
    CHECK(dig.cellsEaten == 1);
    CHECK(satiated(dig, world.registry().get<Metabolism>(didinium).biomass));
}

TEST_CASE("reacción de evitación: el paramecio retrocede ante Didinium") {
    World world(ecoWorld());
    const entt::entity paramecium = world.spawnOrganism(SpeciesId::Paramecium, {1000.0f, 512.0f}, speciesGenome(SpeciesId::Paramecium), 0);
    world.registry().emplace<Autopilot>(paramecium);
    world.spawnOrganism(SpeciesId::Didinium, {1000.0f, 580.0f}, speciesGenome(SpeciesId::Didinium), 0); // quieto
    bool reversed = false;
    for (int s = 0; s < 60 * 3; ++s) {
        world.step(kDt);
        reversed |= world.registry().get<Autopilot>(paramecium).reverse > 0.0f;
    }
    CHECK(reversed);
}

TEST_CASE("Chlamydomonas se divide en 4 hijas (palintomía)") {
    World world(ecoWorld());
    const entt::entity c = world.spawnOrganism(SpeciesId::Chlamydomonas, {1000.0f, 1024.0f}, speciesGenome(SpeciesId::Chlamydomonas), 0);
    Metabolism& m = world.registry().get<Metabolism>(c);
    m.biomass = 4.1f * m.targetBirthBiomass;
    m.reserves = 2.0f * m.biomass;
    m.phosphorus = 0.03f * m.biomass; // con fósforo de sobra para crecer
    int most = 1;
    for (int s = 0; s < 60 * 120 && most < 4; ++s) {
        world.step(kDt);
        most = std::max(most, static_cast<int>(world.registry().view<Species>().size()));
    }
    CHECK(most == 4);
}

TEST_CASE("Didinium se enquista con hambre y sale cuando vuelve a haber presas") {
    World world(ecoWorld());
    world.bioTimeScale() = 3600.0f;
    const entt::entity d = world.spawnOrganism(SpeciesId::Didinium, {1000.0f, 512.0f}, speciesGenome(SpeciesId::Didinium), 0);
    world.registry().emplace<Autopilot>(d);
    for (int s = 0; s < 60 * 60 && !world.registry().all_of<Cyst>(d); ++s)
        world.step(kDt);
    REQUIRE(world.registry().all_of<Cyst>(d));
    world.bioTimeScale() = 60.0f;
    // Una presa cerca del quiste (Didinium nadó antes de enquistarse).
    const Vec2 at = world.registry().get<RingBody>(d).centroid;
    world.spawnOrganism(SpeciesId::Paramecium, {wrapCoord(at.x, world.width()), wrapCoord(at.y + 240.0f, world.height())},
                        speciesGenome(SpeciesId::Paramecium), 0);
    for (int s = 0; s < 60 * 40 && world.registry().all_of<Cyst>(d); ++s)
        world.step(kDt);
    CHECK_FALSE(world.registry().all_of<Cyst>(d));
}

namespace {
std::uint64_t runEcosystem(int threads) {
    omp_set_num_threads(threads);
    World world(ecoWorld(5000));
    world.populateEcosystem(0.5f);
    steps(world, 60 * 10);
    return world.stateHash();
}
} // namespace

TEST_CASE("ecosistema determinista sin importar la cantidad de hilos") {
    const std::uint64_t a = runEcosystem(1);
    CHECK(a == runEcosystem(4));
}

// Prueba larga (~5 min): 30 min de gota sin jugador. Correr con: bio_tests -tc="*30 min*" --no-skip
TEST_CASE("30 min sin jugador: ninguna especie se extingue" * doctest::skip()) {
    World world(ecoWorld(50'000));
    world.populateEcosystem();
    int minimum[kSpeciesCount];
    for (int& m : minimum)
        m = 1 << 30;
    for (int minute = 0; minute < 30; ++minute) {
        steps(world, 60 * 60);
        const Census c = world.census();
        MESSAGE("min " << minute + 1 << ": bact " << c.bacteria << " param " << c.species[0] << " eugl " << c.species[1]
                       << " ameba " << c.species[2] << " chlamy " << c.species[3] << " didin " << c.species[4]);
        for (int k = 0; k < kSpeciesCount; ++k)
            minimum[k] = std::min(minimum[k], c.species[k]);
    }
    for (int k = 0; k < kSpeciesCount; ++k)
        CHECK(minimum[k] > 0);
}
