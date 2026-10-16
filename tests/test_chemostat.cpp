// Quimiostato, perfusión, siembra por densidad y pasajes (fase P1, pasos 7 y 8).
#include "game/culture_run.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr float kStep = 60.0f;

CultureConfig bacterialChemostat(std::uint64_t seed) {
    CultureConfig c;
    c.volumeMl = 0.05f;
    c.depthUm = 5000.0f;
    c.seed = seed;
    c.medium.temperature = 20.0f; // la referencia: sin Q10
    c.medium.phage = 0.0f;
    c.medium.organic = 200.0f; // S₀ del medio que entra [µM]
    return c;
}

// Fracción de Asterionella en un quimiostato de diatomeas limitado por P o por Si.
double tilmanChemostat(bool siLimited, double days) {
    CultureConfig c;
    c.volumeMl = 0.02f;
    c.depthUm = 5000.0f;
    c.seed = 1977;
    c.medium.temperature = 20.0f;
    c.medium.phage = 0.0f;
    c.medium.surfaceLight = 1.0f;
    c.medium.phosphate = siLimited ? 10.0f : 0.25f;
    c.medium.silicate = siLimited ? 3.0f : 100.0f;
    c.medium.nitrogen = 1000.0f;
    c.medium.iron = 10.0f;
    c.medium.vitamin = 3.7e-4f;
    Culture culture(c);
    culture.inoculate(SpeciesId::Asterionella, 20);
    culture.inoculate(SpeciesId::Cyclotella, 20);
    culture.setDilutionRate(0.25 / 24.0); // Tilman (1977): 0,25 por día
    for (int s = 0; s < static_cast<int>(days * 24.0 * 3600.0 / kStep); ++s)
        culture.step(kStep);
    double ast = 0.0, cyc = 0.0;
    for (const CultureCell& cell : culture.cells()) {
        ast += cell.species == SpeciesId::Asterionella ? cell.meta.biomass : 0.0;
        cyc += cell.species == SpeciesId::Cyclotella ? cell.meta.biomass : 0.0;
    }
    MESSAGE((siLimited ? "Si" : "P") << " limitante, día " << days << ": Asterionella " << culture.count(SpeciesId::Asterionella)
                                     << ", Cyclotella " << culture.count(SpeciesId::Cyclotella));
    return ast / std::max(1e-12, ast + cyc);
}

} // namespace

TEST_CASE("validación: quimiostato — estado estacionario de Monod" * doctest::test_suite("validacion")) {
    // Monod (1950): en el estado estacionario la tasa de crecimiento iguala a la de dilución, y el
    // sustrato residual es S* = K (D + m) / (µmax − D − m) (con la mantención m de la bacteria), que
    // no depende de S₀ ni de la densidad del inóculo.
    Culture culture(bacterialChemostat(1950));
    culture.inoculateBacteria(100'000);
    const double D = 0.5; // [1/h]
    culture.setDilutionRate(D);
    const BacteriaParams& p = culture.bacteriaParams();
    const double m = p.maintenancePerHour, mu = p.growthPerHourMax, K = p.halfSaturation;
    const double expected = K * (D + m) / (mu - D - m);
    for (int s = 0; s < 48 * 60; ++s) // 48 h: ~24 tiempos de residencia
        culture.step(kStep);
    double mean = 0.0;
    std::uint64_t first = culture.bacteriaCount();
    for (int s = 0; s < 12 * 60; ++s) {
        culture.step(kStep);
        mean += culture.concentration(Solute::Organic);
    }
    mean /= 12 * 60;
    const std::uint64_t last = culture.bacteriaCount();
    MESSAGE("S* simulado " << mean << " µM, Monod " << expected << " µM; bacterias " << first << " -> " << last);
    CHECK(std::abs(mean - expected) / expected < 0.05);
    CHECK(std::abs(static_cast<double>(last) - static_cast<double>(first)) < 0.1 * static_cast<double>(first)); // estacionario
}

TEST_CASE("validación: quimiostato de Tilman — con P limitante Cyclotella se extingue" * doctest::test_suite("validacion")) {
    // Tilman (1977, Ecology 58: 338-348), cultivos semicontinuos a D = 0,25/día: con P limitante
    // gana Asterionella (menor R* de P) y la otra desaparece; con Si limitante, al revés.
    CHECK(tilmanChemostat(false, 60.0) > 0.95);
}

TEST_CASE("validación: quimiostato de Tilman — con Si limitante Asterionella se extingue" * doctest::test_suite("validacion")) {
    CHECK(tilmanChemostat(true, 60.0) < 0.05);
}

TEST_CASE("perfusión: la placa renueva lo disuelto y el balance lo registra afuera") {
    WorldConfig c;
    c.gridX = c.gridY = 64;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.medium.detritusSpawnRate = 0.0f;
    c.medium.phosphate = 20.0f;
    World w(c);
    w.setMeanConcentration(w.phosphateField(), 2.0f); // agotada
    w.medium().perfusionPerHour = 2.0f;
    const MassBudget start = w.massBudget();
    for (int s = 0; s < 600; ++s) // 10 s = 10 min biológicos con la escala de 60
        w.step(1.0f / 60.0f);
    const double after = w.meanConcentration(w.phosphateField());
    const double expected = 20.0 - 18.0 * std::exp(-2.0 * 600.0 / 60.0 * w.bioTimeScale() / 3600.0);
    MESSAGE("fosfato " << after << " µM (esperado " << expected << ")");
    CHECK(after == doctest::Approx(expected).epsilon(0.05));
    CHECK(std::abs(budgetResidual(start, w.massBudget(), Element::P)) < 1e-3 * w.massBudget().total(Element::P));
}

TEST_CASE("pasaje: una transferencia lleva células con su estado y se repite al cargar la escena") {
    CultureSpec spec;
    spec.vessel = "tubo_gause";
    spec.medium = "osterhout_gause";
    spec.seed = 8;
    CultureRun a, b;
    a.start(spec);
    spec.seed = 9;
    b.start(spec);
    a.apply({0, CultureOp::Bacteria, "", 1e8});
    a.apply({0, CultureOp::InoculateDensity, "paramecium", 400.0}); // ~200 en 0,5 mL
    const int seeded = a.culture().count(SpeciesId::Paramecium);
    CHECK(seeded > 150);
    CHECK(seeded < 250);
    a.advance(5 * 3600.0);
    const double before = b.culture().volumeMl();
    a.transferTo(b, 0.05); // 1:10
    CHECK(b.culture().volumeMl() == doctest::Approx(before + 0.05));
    CHECK(b.culture().count(SpeciesId::Paramecium) > 0);
    b.advance(10 * 3600.0);
    // La escena del cultivo que recibió repite la corrida.
    CultureSpec read;
    std::vector<CultureEvent> events;
    REQUIRE(parseCultureScene(serializeCultureScene(b.spec(), b.events()), read, events));
    CultureRun c;
    c.start(read);
    c.schedule(events);
    c.advance(10 * 3600.0);
    CHECK(c.culture().count(SpeciesId::Paramecium) == b.culture().count(SpeciesId::Paramecium));
    CHECK(c.culture().bacteriaCount() == b.culture().bacteriaCount());
}
