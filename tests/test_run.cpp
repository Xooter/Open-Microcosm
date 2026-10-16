#include "game/progress.hpp"
#include "ai/autopilot.hpp"
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

void stepSeconds(World& world, Run& run, float seconds) {
    for (int s = 0; s < static_cast<int>(seconds * 60.0f); ++s) {
        world.step(kDt);
        run.update(world, kDt);
    }
}

} // namespace

TEST_CASE("al dividirse el ser sigue controlado con la madre; la hija queda en piloto automático" * doctest::test_suite("largos")) {
    World world(runWorld());
    Run run;
    run.start(world, quickRun(), 1);
    const entt::entity mother = world.player();
    forceDivision(world);
    stepSeconds(world, run, 90.0f);
    REQUIRE(run.divisions() == 1);
    CHECK(world.player() == mother); // no hay pausa ni elección
    entt::entity daughter = entt::null;
    for (auto [e, lin] : world.registry().view<Lineage>().each())
        if (lin.run == 1 && lin.generation == 1 && e != mother)
            daughter = e;
    REQUIRE(world.registry().valid(daughter));
    CHECK(world.registry().all_of<Autopilot>(daughter));
    CHECK_FALSE(world.registry().all_of<Player>(daughter));
}

TEST_CASE("al morir el ser controlado la sesión sigue y se puede controlar otro") {
    World world(runWorld());
    Run run;
    run.start(world, quickRun(), 1);
    const Vec2 c = world.registry().get<RingBody>(world.player()).centroid;
    const entt::entity other = world.spawnOrganism(SpeciesId::Euglena, c + Vec2{200.0f, 0.0f}, speciesGenome(SpeciesId::Euglena), 1);
    world.lyseAt(world.registry().get<RingBody>(world.player()).centroid);
    run.update(world, kDt);
    CHECK_FALSE(world.registry().valid(world.player()));
    CHECK(std::string(run.deathCause()).size() > 0);
    stepSeconds(world, run, 2.0f); // sin ser controlado, el mundo sigue
    world.setPlayer(other);
    CHECK(world.player() == other);
    CHECK(world.registry().all_of<Player>(other));
    CHECK_FALSE(world.registry().all_of<Autopilot>(other));
}

TEST_CASE("las hijas en piloto automático nadan y comen solas" * doctest::test_suite("largos")) {
    World world(runWorld(50'000));
    Run run;
    run.start(world, quickRun(0), 1);
    forceDivision(world);
    stepSeconds(world, run, 90.0f);
    entt::entity sister = entt::null;
    for (auto [e, lin] : world.registry().view<Lineage>().each())
        if (e != world.player() && lin.run == 1)
            sister = e;
    REQUIRE(world.registry().valid(sister));
    const int before = world.registry().get<Digestion>(sister).bacteriaEaten;
    stepSeconds(world, run, 20.0f);
    REQUIRE(world.registry().valid(sister));
    const int eaten = world.registry().get<Digestion>(sister).bacteriaEaten - before;
    MESSAGE("la hermana comió " << eaten << " bacterias en 20 s");
    CHECK(eaten > 20);
}

TEST_CASE("el registro de la enciclopedia se guarda y se carga") {
    const auto path = (std::filesystem::temp_directory_path() / "biogame_test_progress.txt").string();
    std::remove(path.c_str());
    Progress p = loadProgress(path);
    CHECK(p.seenSpecies == 0);
    p.seenSpecies = 0b101;
    p.biomesPlayed = 0b10;
    p.behaviors.insert("fagocitosis");
    REQUIRE(saveProgress(p, path));
    const Progress q = loadProgress(path);
    CHECK(q.seenSpecies == 0b101);
    CHECK(q.biomesPlayed == 0b10);
    CHECK(q.behaviors.count("fagocitosis") == 1);
    std::remove(path.c_str());
}

namespace {
std::uint64_t runRun(int threads) {
    omp_set_num_threads(threads);
    World world(runWorld(3000));
    Run run;
    run.start(world, quickRun(4), 1);
    forceDivision(world);
    stepSeconds(world, run, 40.0f);
    return world.stateHash();
}
} // namespace

TEST_CASE("sesión con pilotos automáticos determinista sin importar los hilos" * doctest::test_suite("largos")) {
    const std::uint64_t a = runRun(1);
    CHECK(a == runRun(4));
}

TEST_CASE("sandbox: cambiar de forma conserva linaje y fósforo, y no cuenta como muerte") {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(0);
    run.start(world, rc, 1);
    const double phosphorus = world.totalPhosphorus();
    for (SpeciesId s : {SpeciesId::Euglena, SpeciesId::Vorticella, SpeciesId::Stentor, SpeciesId::Amoeba}) {
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
    }
    CHECK(world.totalPhosphorus() == doctest::Approx(phosphorus).epsilon(0.01));
}

TEST_CASE("sandbox: forzar la reproducción usa la vía normal de cada especie" * doctest::test_suite("largos")) {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(0);
    run.start(world, rc, 1);
    REQUIRE(world.forceReproduction(world.player()));
    stepSeconds(world, run, 90.0f);
    CHECK(run.divisions() == 1);

    // Levadura: brota una yema en vez de dividirse.
    world.setPlayerSpecies(SpeciesId::Candida, {0, 0});
    const int before = world.observations().buds;
    REQUIRE(world.forceReproduction(world.player()));
    for (int k = 0; k < 10; ++k)
        world.step(kDt);
    CHECK(world.observations().buds == before + 1);
}

TEST_CASE("sandbox: se puede spawnear cada ser del catálogo y la sesión sigue") {
    World world(runWorld());
    Run run;
    RunConfig rc = quickRun(0);
    run.start(world, rc, 1);
    const Vec2 c = {0.5f * world.width(), 0.5f * world.height()};
    const std::size_t cells = world.cellCount(), bacteria = world.bacteria().size();
    const std::size_t filaments = world.cyanobacteria().filaments.size(), mycelia = world.fungi().mycelia.size();
    for (int i = 0; i < beingCount(); ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / beingCount();
        world.spawnBeing(beingAt(i), {c.x + 300.0f * std::cos(a), c.y + 300.0f * std::sin(a)});
    }
    CHECK(world.cellCount() >= cells + speciesCount());
    CHECK(world.bacteria().size() > bacteria);
    CHECK(world.bdellovibrios().size() > 0);
    CHECK(world.cyanobacteria().filaments.size() == filaments + 2); // Anabaena y Oscillatoria
    CHECK(world.fungi().mycelia.size() == mycelia + 3); // Saprolegnia, Allomyces y liquenizante
    CHECK(world.plasmodia().plasmodia.size() >= 1);      // Physarum
    CHECK(world.plasmodia().flakes.size() == 1);         // copo de avena
    CHECK(world.fungi().spores.size() > 0);
    CHECK(world.phage().sum() > 0.0);
    for (int k = 0; k < 60; ++k) {
        world.step(kDt);
        run.update(world, kDt);
    }

    // Sin ser controlado, la sesión sigue.
    if (world.registry().valid(world.player()))
        world.lyseAt(world.registry().get<RingBody>(world.player()).centroid);
    for (int k = 0; k < 90; ++k) {
        world.step(kDt);
        run.update(world, kDt);
    }
}

TEST_CASE("sandbox: cada ser del catálogo cae en la categoría que le corresponde") {
    using C = BeingCategory;
    const std::pair<C, std::vector<SpeciesId>> expected[] = {
        {C::Ciliates, {SpeciesId::Paramecium, SpeciesId::Didinium, SpeciesId::Vorticella, SpeciesId::Stentor,
                       SpeciesId::Podophrya, SpeciesId::Euplotes, SpeciesId::Lacrymaria, SpeciesId::ParameciumAurelia}},
        {C::Flagellates, {SpeciesId::Euglena, SpeciesId::Trichonympha}},
        {C::Amoebae, {SpeciesId::Amoeba, SpeciesId::Arcella, SpeciesId::Naegleria}},
        {C::Algae, {SpeciesId::Chlamydomonas, SpeciesId::Spirogyra, SpeciesId::Navicula, SpeciesId::Volvox,
                    SpeciesId::Pediastrum, SpeciesId::Mougeotia, SpeciesId::Zygnema, SpeciesId::Oedogonium,
                    SpeciesId::Asterionella, SpeciesId::Cyclotella}},
        {C::Fungi, {SpeciesId::Candida}},
        {C::HumanCells, {*findSpecies("fibroblasto_dermico_humano"), *findSpecies("hela"), *findSpecies("neutrofilo")}},
    };
    int listed = 0;
    for (const auto& [category, species] : expected)
        for (SpeciesId id : species) {
            CAPTURE(std::string(speciesInfo(id).name));
            CHECK(categoryOf({BeingKind::Species, id}) == category);
            ++listed;
        }
    CHECK(listed == speciesCount()); // ninguna especie quedó sin revisar

    CHECK(categoryOf({BeingKind::Chytrid, SpeciesId::Paramecium}) == C::Fungi);
    CHECK(categoryOf({BeingKind::Saprolegnia, SpeciesId::Paramecium}) == C::Fungi);
    for (BeingKind k : {BeingKind::Bacterium, BeingKind::Violaceum, BeingKind::Bdellovibrio, BeingKind::Cyanobacterium,
                        BeingKind::Oscillatoria, BeingKind::Phage})
        CHECK(categoryOf({k, SpeciesId::Paramecium}) == C::Bacteria);
    for (BeingKind k : {BeingKind::Clay, BeingKind::Quartz, BeingKind::Mica, BeingKind::IronOxide, BeingKind::Sulfur,
                        BeingKind::Frustule, BeingKind::Rock, BeingKind::Sediment})
        CHECK(categoryOf({k, SpeciesId::Paramecium}) == C::Sediment);

    // Ninguna pestaña queda vacía y entre todas suman el catálogo entero.
    int count[kCategoryCount] = {}, total = 0;
    for (int i = 0; i < beingCount(); ++i)
        ++count[static_cast<int>(categoryOf(beingAt(i)))];
    for (int c = 0; c < kCategoryCount; ++c) {
        CHECK(count[c] > 0);
        CHECK(std::string(categoryName(static_cast<C>(c))).size() > 0);
        total += count[c];
    }
    CHECK(total == beingCount());
}
