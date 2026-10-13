#include "game/progress.hpp"
#include "game/run.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig runWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.seed = 44;
    return config;
}

RunConfig quickRun(int competitors = 6) {
    RunConfig rc;
    rc.competitors = competitors;
    rc.wildlife = 0.0f; // estos tests prueban las reglas de la partida, no la ecología
    return rc;
}

void forceDivision(World& world) {
    Metabolism& m = world.registry().get<Metabolism>(world.player());
    m.biomass = 2.05f * m.birthBiomass;
    m.reserves = 0.6f * m.biomass;
}

// Avanza hasta que la partida pida una elección (o se acabe el tiempo).
void stepUntilChoice(World& world, Run& run, int maxSteps) {
    for (int s = 0; s < maxSteps && !run.simulationPaused() && run.state() != Run::State::Lost; ++s) {
        world.step(kDt);
        run.update(world, kDt);
    }
}

} // namespace

TEST_CASE("al dividirse el jugador elige hija; la otra queda en piloto automático") {
    World world(runWorld());
    Run run;
    run.start(world, quickRun(), 1);
    forceDivision(world);
    stepUntilChoice(world, run, 60 * 90);
    REQUIRE(run.state() == Run::State::ChoosingDaughter);
    REQUIRE(run.candidates().size() == 2);

    const entt::entity mother = run.candidates()[0], other = run.candidates()[1];
    run.cycle(1);
    run.confirm(world);
    CHECK(run.state() == Run::State::Playing);
    CHECK(world.player() == other);
    CHECK(world.registry().all_of<Player>(other));
    CHECK_FALSE(world.registry().all_of<Player>(mother));
    CHECK(world.registry().all_of<Autopilot>(mother));
    CHECK(world.registry().get<Lineage>(other).generation == 1);
}

TEST_CASE("al morir se hereda en un descendiente; sin descendientes, el linaje se extingue") {
    World world(runWorld());
    Run run;
    run.start(world, quickRun(), 1);
    forceDivision(world);
    stepUntilChoice(world, run, 60 * 90);
    run.confirm(world); // sigue con la madre

    world.lyseAt(world.registry().get<RingBody>(world.player()).centroid);
    run.update(world, kDt);
    REQUIRE(run.state() == Run::State::ChoosingHeir);
    REQUIRE(run.candidates().size() == 1);
    run.confirm(world);
    CHECK(world.registry().valid(world.player()));

    world.lyseAt(world.registry().get<RingBody>(world.player()).centroid);
    run.update(world, kDt);
    CHECK(run.state() == Run::State::Lost);
}

TEST_CASE("se gana cuando el linaje alcanza el dominio objetivo de la biomasa") {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(6);
    rc.targetShare = 0.05f;
    run.start(world, rc, 1);
    for (int s = 0; s < 120; ++s) {
        world.step(kDt);
        run.update(world, kDt);
    }
    MESSAGE("dominio " << run.census().share() * 100 << "%");
    CHECK(run.won());
    CHECK(run.justWon());
    CHECK_FALSE(run.justWon()); // el aviso se consume una sola vez
}

TEST_CASE("las hijas en piloto automático nadan y comen solas") {
    World world(runWorld(50'000));
    Run run;
    run.start(world, quickRun(0), 1);
    forceDivision(world);
    stepUntilChoice(world, run, 60 * 90);
    const entt::entity sister = run.candidates()[1];
    run.confirm(world);
    for (int s = 0; s < 60 * 20; ++s) {
        world.step(kDt);
        run.update(world, kDt);
    }
    REQUIRE(world.registry().valid(sister));
    const int eaten = world.registry().get<Digestion>(sister).bacteriaEaten;
    MESSAGE("la hermana comió " << eaten << " bacterias en 20 s");
    CHECK(eaten > 20);
}

TEST_CASE("la meta-progresión se guarda, se carga y desbloquea especies") {
    const auto path = (std::filesystem::temp_directory_path() / "biogame_test_progress.txt").string();
    std::remove(path.c_str());
    Progress p = loadProgress(path);
    CHECK(p.isUnlocked(LocomotionType::Cilia));
    CHECK_FALSE(p.isUnlocked(LocomotionType::Flagellum));

    auto fresh = p.recordRun(3, 0.2f, false, LocomotionType::Cilia, speciesGenome(LocomotionType::Cilia));
    REQUIRE(fresh.size() == 1);
    CHECK(fresh[0] == LocomotionType::Flagellum);
    fresh = p.recordRun(1, 0.6f, true, LocomotionType::Cilia, speciesGenome(LocomotionType::Cilia));
    REQUIRE(fresh.size() == 1);
    CHECK(fresh[0] == LocomotionType::Pseudopod);

    REQUIRE(saveProgress(p, path));
    const Progress q = loadProgress(path);
    CHECK(q.runs == 2);
    CHECK(q.wins == 1);
    CHECK(q.bestGeneration == 3);
    CHECK(q.bestShare == doctest::Approx(0.6f));
    CHECK(q.isUnlocked(LocomotionType::Pseudopod));
    std::remove(path.c_str());
}

namespace {
std::uint64_t runRun(int threads) {
    omp_set_num_threads(threads);
    World world(runWorld(3000));
    Run run;
    run.start(world, quickRun(4), 1);
    forceDivision(world);
    for (int s = 0; s < 60 * 40; ++s) {
        world.step(kDt);
        run.update(world, kDt);
        if (run.simulationPaused())
            run.confirm(world);
    }
    return world.stateHash();
}
} // namespace

TEST_CASE("partida con linaje y pilotos automáticos determinista sin importar los hilos") {
    const std::uint64_t a = runRun(1);
    CHECK(a == runRun(4));
}

TEST_CASE("sandbox: cambiar de forma conserva linaje y fósforo, y no cuenta como muerte") {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(0);
    rc.sandbox = true;
    run.start(world, rc, 1);
    const double phosphorus = world.totalPhosphorus();
    for (SpeciesId s : {SpeciesId::Euglena, SpeciesId::Vorticella, SpeciesId::Rotifer, SpeciesId::Amoeba}) {
        const entt::entity old = world.player();
        const entt::entity e = world.setPlayerSpecies(s, {0, 0});
        CHECK_FALSE(world.registry().valid(old));
        CHECK(world.player() == e);
        CHECK(world.registry().get<Species>(e).id == s);
        CHECK(world.registry().get<Lineage>(e).run == 1);
        for (int k = 0; k < 30; ++k) {
            world.step(kDt);
            run.update(world, kDt);
        }
        CHECK(run.state() == Run::State::Playing);
    }
    CHECK(world.totalPhosphorus() == doctest::Approx(phosphorus).epsilon(0.01));
}

TEST_CASE("sandbox: forzar la reproducción usa la vía normal de cada especie") {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(0);
    rc.sandbox = true;
    run.start(world, rc, 1);
    REQUIRE(world.forceReproduction(world.player()));
    stepUntilChoice(world, run, 60 * 90);
    CHECK(run.state() == Run::State::ChoosingDaughter);
    run.confirm(world);

    // Rotífero: pone un huevo en vez de dividirse.
    world.setPlayerSpecies(SpeciesId::Rotifer, {0, 0});
    const int before = world.census().eggs;
    REQUIRE(world.forceReproduction(world.player()));
    for (int k = 0; k < 10; ++k)
        world.step(kDt);
    CHECK(world.census().eggs == before + 1);
}

TEST_CASE("sandbox: se puede spawnear cada ser del catálogo y la partida no se pierde") {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(0);
    rc.sandbox = true;
    run.start(world, rc, 1);
    const Vec2 c = {0.5f * world.width(), 0.5f * world.height()};
    const std::size_t cells = world.cellCount(), bacteria = world.bacteria().size();
    const std::size_t filaments = world.cyanobacteria().filaments.size(), mycelia = world.fungi().mycelia.size();
    for (int i = 0; i < kBeingCount; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / kBeingCount;
        world.spawnBeing(beingAt(i), {c.x + 300.0f * std::cos(a), c.y + 300.0f * std::sin(a)});
    }
    CHECK(world.cellCount() >= cells + kSpeciesCount);
    CHECK(world.bacteria().size() > bacteria);
    CHECK(world.bdellovibrios().size() > 0);
    CHECK(world.cyanobacteria().filaments.size() == filaments + 1);
    CHECK(world.fungi().mycelia.size() == mycelia + 2);
    CHECK(world.fungi().spores.size() > 0);
    CHECK(world.phage().sum() > 0.0);
    for (int k = 0; k < 60; ++k) {
        world.step(kDt);
        run.update(world, kDt);
    }

    // Sin jugador ni descendientes, el sandbox sigue.
    if (world.registry().valid(world.player()))
        world.lyseAt(world.registry().get<RingBody>(world.player()).centroid);
    for (int k = 0; k < 90; ++k) {
        world.step(kDt);
        run.update(world, kDt);
    }
    CHECK(run.state() == Run::State::Playing);
    CHECK_FALSE(run.won());
}
