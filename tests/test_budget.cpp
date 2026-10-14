// Fase 13: balance de masa visible y mapa de limitación.
#include "game/biome.hpp"
#include "game/export.hpp"
#include "game/run.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr float kDt = 1.0f / 60.0f;

void steps(World& world, Run& run, int n) {
    for (int s = 0; s < n; ++s) {
        world.step(kDt);
        run.update(world, kDt);
    }
}

// Gota de estanque con su vida, achicada para que el test sea rápido.
WorldConfig pondDrop() {
    WorldConfig c = makeDrop(BiomeId::Pond, 7);
    c.bacteria = 4000;
    return c;
}

RunConfig labRun(float wildlife) {
    RunConfig rc;
    rc.spawnFounder = false;
    rc.wildlife = wildlife;
    rc.biome = static_cast<int>(BiomeId::Pond);
    rc.seed = 7;
    return rc;
}

std::uint64_t seriesHash(const TimeSeries& s) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t c = 0; c < s.columns(); ++c)
        for (float v : s.column(c)) {
            std::uint32_t bits;
            std::memcpy(&bits, &v, sizeof bits);
            h = (h ^ bits) * 1099511628211ull;
        }
    return h;
}

} // namespace

TEST_CASE("hecho de la fase 13: 1 h biológica exporta CSV con unidades y el P cierra") {
    World world(pondDrop());
    world.bioTimeScale() = 3600.0f; // 1 s real = 1 h biológica
    Run run;
    run.start(world, labRun(0.25f), 1);
    const double p0 = world.totalPhosphorus();
    steps(world, run, 60); // 1 h biológica
    run.sample(world);
    CHECK(run.series().rows() >= 2);
    const std::string csv = seriesCsv(run.series());
    CHECK(csv.find("[µM]") != std::string::npos);
    CHECK(csv.find("[pmol P]") != std::string::npos);
    CHECK(csv.find("[individuos]") != std::string::npos);
    const int tb = run.series().find("t bio");
    REQUIRE(tb >= 0);
    CHECK(run.series().column(static_cast<std::size_t>(tb)).back() == doctest::Approx(1.0f).epsilon(0.05));
    // El P cierra con la misma tolerancia que los tests de conservación.
    MESSAGE("P " << p0 << " → " << world.totalPhosphorus() << ", residuo " << run.residual(Element::P));
    CHECK(world.totalPhosphorus() == doctest::Approx(p0).epsilon(0.01));
    CHECK(std::abs(run.residual(Element::P)) < 0.01 * p0);
    // N, Si y Fe también cierran (las entradas externas, como la fijación de N₂, se descuentan).
    for (Element e : {Element::N, Element::Si, Element::Fe}) {
        const double total = run.budget().total(e);
        MESSAGE(std::string(elementName(e)) << " residuo " << run.residual(e) << " de " << total);
        CHECK(std::abs(run.residual(e)) < 0.01 * std::max(1e-9, total));
    }
    // Los flujos rotulados explican casi todo: lo "sin rotular" es despreciable en P.
    const FluxLedger& l = run.budget().ledger;
    const int p = static_cast<int>(Element::P);
    double labelled = 0.0;
    for (int f = 1; f < kFlowCount; ++f)
        labelled += l.toWater[p][f] + l.fromWater[p][f];
    const double other = l.toWater[p][0] + l.fromWater[p][0];
    MESSAGE("flujo de P rotulado " << labelled << ", sin rotular " << other);
    CHECK(other <= 0.01 * std::max(1e-9, labelled));
}

TEST_CASE("balance: el desglose suma los totales de siempre y las dosis son entradas externas") {
    World world(pondDrop());
    Run run;
    run.start(world, labRun(0.1f), 1);
    const MassBudget b = world.massBudget();
    CHECK(b.total(Element::P) == doctest::Approx(world.totalPhosphorus()));
    CHECK(b.at(Element::P, Pool::Water) > 0.0);
    CHECK(b.at(Element::C, Pool::Bacteria) > 0.0);
    CHECK(b.at(Element::C, Pool::Gas) > 0.0);

    const double before = world.totalPhosphorus();
    world.dose(world.phosphateField(), world.dish().center, 60.0f, 500.0f);
    world.setMeanConcentration(world.nitrogenField(), 2.0f * world.meanConcentration(world.nitrogen()));
    run.sample(world);
    CHECK(world.totalPhosphorus() > before);
    CHECK(run.residual(Element::P) == doctest::Approx(0.0).epsilon(1e-6).scale(before));
    CHECK(run.residual(Element::N) == doctest::Approx(0.0).epsilon(1e-6).scale(run.budget().total(Element::N)));
    CHECK(run.budget().ledger.toWater[static_cast<int>(Element::P)][static_cast<int>(Flow::Dose)] > 0.0);
}

TEST_CASE("la serie de la sesión es determinista con 1 y 4 hilos") {
    auto once = [](int threads) {
        omp_set_num_threads(threads);
        World world(pondDrop());
        Run run;
        run.start(world, labRun(0.2f), 1);
        steps(world, run, 60 * 4);
        return seriesHash(run.series());
    };
    CHECK(once(1) == once(4));
}

TEST_CASE("mapa de limitación: la célula sabe qué nutriente la frena") {
    auto limited = [](float silicate, float iron) {
        WorldConfig c;
        c.bacteria = 0;
        c.detritus = 0;
        c.cyanoFilaments = 0;
        c.saprolegnia = c.zoophagus = c.chytridClusters = 0;
        c.medium.detritusSpawnRate = 0.0f;
        c.medium.silicate = silicate;
        c.medium.iron = iron;
        World world(c);
        world.bioTimeScale() = 3600.0f;
        world.medium().surfaceLight = 1.0f;
        const entt::entity e = world.spawnOrganism(SpeciesId::Navicula, world.dish().center, speciesGenome(SpeciesId::Navicula), 0);
        Metabolism& m = world.registry().get<Metabolism>(e);
        m.reserves = 2.0f * m.biomass; // energía de sobra
        for (int s = 0; s < 60 * 4; ++s)
            world.step(kDt);
        return world.registry().get<Metabolism>(e).limiting;
    };
    CHECK(limited(0.0f, 1.0f) == Limitation::Silicon);
    CHECK(limited(300.0f, 0.0f) == Limitation::Iron);
    CHECK(std::string(limitationName(Limitation::Phosphorus)) == "Fósforo");
}
