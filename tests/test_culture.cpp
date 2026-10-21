// Cultivo bien mezclado (fase P1, paso 1): la misma fisiología que la placa, sin espacio.
#include "bio/physiology.hpp"
#include "game/calibration.hpp"
#include "core/units.hpp"
#include "game/culture.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr float kStep = 60.0f; // [s]

// Tubo de Gause: 0,5 cm³ de medio a 20 °C (la referencia de la fisiología).
CultureConfig tube(std::uint64_t seed = 1934) {
    CultureConfig c;
    c.volumeMl = 0.5f;
    c.depthUm = 5000.0f;
    c.seed = seed;
    c.medium.temperature = 20.0f;
    c.medium.organic = 200.0f;
    c.medium.organicInflow = 50.0f;
    c.medium.phage = 0.0f; // como un cultivo de laboratorio (los fagos se prueban aparte)
    return c;
}

double speciesBiomass(const Culture& c, SpeciesId id) {
    double b = 0.0;
    for (const CultureCell& cell : c.cells())
        b += cell.species == id ? cell.meta.biomass : 0.0;
    return b;
}

} // namespace

TEST_CASE("cultivo: P, N, Si y Fe se conservan con algas, diatomeas, bacterívoros y cazadores") {
    CultureConfig cfg = tube();
    cfg.medium.surfaceLight = 1.0f;
    Culture c(cfg);
    c.inoculateBacteria(2'000'000);
    CHECK(c.inoculate(SpeciesId::Paramecium, 20) == 20);
    CHECK(c.inoculate(SpeciesId::Didinium, 3) == 3);
    CHECK(c.inoculate(SpeciesId::Chlamydomonas, 30) == 30);
    CHECK(c.inoculate(SpeciesId::Asterionella, 30) == 30);
    const MassBudget start = c.massBudget();
    for (int s = 0; s < 3000; ++s) // 50 h
        c.step(kStep);
    const MassBudget end = c.massBudget();
    for (const Element e : {Element::P, Element::N, Element::Si, Element::Fe}) {
        const double residual = budgetResidual(start, end, e);
        MESSAGE(elementName(e) << ": total " << end.total(e) << " pmol, residuo " << residual);
        CHECK(std::abs(residual) <= 1e-4 * std::max(1.0, end.total(e)));
    }
    // Con 4·10⁶ bacterias por mL (lejos de la media saturación, ~2,5·10⁸) los paramecios apenas
    // comen y Didinium los caza: lo que importa acá es que la masa cierre con todos los procesos.
    int eaten = 0;
    for (const CultureCell& cell : c.cells())
        eaten += cell.digestion.cellsEaten;
    for (const auto& [key, n] : c.deathTally())
        eaten += key.second.rfind("comida por", 0) == 0 ? n : 0;
    CHECK(eaten > 0);
    CHECK(c.count(SpeciesId::Chlamydomonas) > 30); // las algas crecen con luz y nutrientes
}

TEST_CASE("cultivo: determinista por seed") {
    auto run = [](std::uint64_t seed) {
        Culture c(tube(seed));
        c.inoculateBacteria(1'000'000);
        c.inoculate(SpeciesId::Paramecium, 10);
        c.inoculate(SpeciesId::Didinium, 2);
        for (int s = 0; s < 600; ++s)
            c.step(kStep);
        std::vector<double> state = {static_cast<double>(c.bacteriaCount()), c.amount(Solute::Organic),
                                     c.amount(Solute::Oxygen)};
        for (const CultureCell& cell : c.cells())
            state.push_back(cell.meta.biomass + 1e3 * cell.id);
        return state;
    };
    CHECK(run(7) == run(7));
    CHECK(run(7) != run(8));
}

TEST_CASE("cultivo: especies que todavía no se simulan bien mezcladas no se siembran") {
    Culture c(tube());
    std::string why;
    CHECK_FALSE(Culture::supports(SpeciesId::Spirogyra, &why));
    CHECK_FALSE(why.empty());
    CHECK(c.inoculate(SpeciesId::Spirogyra, 3) == 0);
    CHECK(Culture::supports(SpeciesId::Paramecium));
}

TEST_CASE("cultivo: la respuesta funcional satura en la ingestión publicada") {
    Culture c(tube());
    c.inoculate(SpeciesId::Paramecium, 1);
    const CultureCell& p = c.cells().front();
    const SpeciesPhysiology& physio = speciesPhysiology(SpeciesId::Paramecium);
    REQUIRE(physio.maxIngestion.present);
    REQUIRE(physio.clearance.present);
    // Con pocas presas, filtra a su aclaramiento; con muchas, come a su ritmo máximo.
    const double sparse = 1e-9, dense = 1.0; // [bacterias/µm³] (10³ y 10¹² por mL)
    CHECK(c.bacterivory(p, sparse) == doctest::Approx(physio.clearance.value / 3600.0 * sparse).epsilon(0.01));
    CHECK(c.bacterivory(p, dense) == doctest::Approx(physio.maxIngestion.value / 3600.0).epsilon(0.01));
}

TEST_CASE("validación: µmax de Paramecium en el cultivo bien mezclado" * doctest::test_suite("validacion")) {
    // El mismo experimento que en la placa (test_validation.cpp): cultivo sin limitación, con
    // bacterias abundantes que se reponen con materia orgánica. Pero acá a escala de Gause.
    CultureConfig cfg = tube(26);
    cfg.medium.organicInflow = 400.0f;
    Culture c(cfg);
    c.inoculateBacteria(500'000'000); // 10⁹ por mL: saturante (media saturación ~2,5·10⁸)
    c.inoculate(SpeciesId::Paramecium, 5);
    const SourcedValue& mu = speciesPhysiology(SpeciesId::Paramecium).growthMax;
    REQUIRE(mu.present);
    const float hours = 3.0f * std::log(2.0f) / mu.value; // tres duplicaciones
    const int steps = static_cast<int>(hours * 3600.0f / kStep);
    double st = 0, sy = 0, stt = 0, sty = 0, n = 0;
    for (int s = 0; s <= steps; ++s) {
        if (s % 60 == 0 && s > steps / 3) {
            const double t = c.time() / 3600.0, y = std::log(speciesBiomass(c, SpeciesId::Paramecium));
            st += t;
            sy += y;
            stt += t * t;
            sty += t * y;
            n += 1;
        }
        c.step(kStep);
    }
    const double sim = (n * sty - st * sy) / (n * stt - st * st);
    MESSAGE("µ simulado " << sim << " /h, publicado " << mu.value << " /h a 20 °C; paramecios " << c.count(SpeciesId::Paramecium)
                          << ", bacterias " << c.bacteriaCount());
    CHECK(std::abs(sim - mu.value) / mu.value < 0.25);
}

TEST_CASE("cultivo: los fagos líticos causan una epidemia y quedan las resistentes") {
    CultureConfig cfg = tube(3);
    cfg.medium.organicInflow = 0.0f;
    cfg.medium.organic = 2000.0f;
    cfg.medium.phage = 2e-7f; // 2·10⁵ por mL
    Culture c(cfg);
    c.inoculateBacteria(5'000'000);
    const double phagesBefore = c.phages();
    std::uint64_t peak = 0;
    for (int s = 0; s < 24 * 60; ++s) { // 24 h
        c.step(kStep);
        peak = std::max(peak, c.bacteriaCount());
    }
    std::uint64_t resistant = 0, total = c.bacteriaCount();
    for (const BacterialCohort& b : c.bacteria())
        resistant += b.resistant ? b.count : 0;
    MESSAGE("fagos " << phagesBefore << " -> " << c.phages() << "; bacterias pico " << peak << ", final " << total
                     << " (resistentes " << resistant << ")");
    CHECK(c.phages() > 100.0 * phagesBefore); // se multiplicaron
    CHECK(total > 0);
    CHECK(resistant > total / 2); // las sensibles se lisaron; las resistentes siguen
}

TEST_CASE("cultivo: las algas se sombrean a sí mismas (Beer–Lambert en la columna)") {
    CultureConfig cfg = tube(5);
    cfg.medium.surfaceLight = 1.0f;
    Culture sparse(cfg), dense(cfg);
    sparse.inoculate(SpeciesId::Chlamydomonas, 10);
    dense.inoculate(SpeciesId::Chlamydomonas, 100000);
    sparse.step(kStep);
    dense.step(kStep);
    MESSAGE("luz media: " << sparse.light() << " con 10 algas, " << dense.light() << " con 10⁵");
    CHECK(sparse.light() > 0.999);
    CHECK(dense.light() < sparse.light());
}

namespace {

// Calibración cruzada (paso 2 de P1): la placa (física: boca, corriente oral, altura en la
// película) y el cultivo (respuesta funcional medida) tienen que comer lo mismo a la misma
// densidad, dentro de un factor 2 (la placa no usa el aclaramiento publicado: lo produce).
void checkFunctionalResponse(std::size_t bacteria) {
    const FunctionalResponsePoint p = measureFunctionalResponse(SpeciesId::Paramecium, bacteria, 6, 20.0f);
    MESSAGE("densidad " << p.density * 1e12 << " /mL: placa " << p.plate * 3600.0 << " bacterias/h, cultivo "
                        << p.culture * 3600.0 << " bacterias/h (razón " << p.plate / p.culture << ")");
    CHECK(p.plate > 0.5 * p.culture);
    CHECK(p.plate < 2.0 * p.culture);
}

} // namespace

TEST_CASE("validación: respuesta funcional de Paramecium, placa contra cultivo, ~10⁵/mL" * doctest::test_suite("validacion")) {
    checkFunctionalResponse(100);
}

TEST_CASE("validación: respuesta funcional de Paramecium, placa contra cultivo, ~10⁶/mL" * doctest::test_suite("validacion")) {
    checkFunctionalResponse(500);
}

TEST_CASE("validación: respuesta funcional de Paramecium, placa contra cultivo, 10⁹/mL" * doctest::test_suite("validacion")) {
    checkFunctionalResponse(100'000);
}

TEST_CASE("cultivo: la mezcla del medio fija cómo entra el O₂ por la superficie") {
    // Medio quieto: el O₂ difunde por toda la columna (D / h²); mezclado: entra por la película de la
    // superficie (k / h). En un tubo de 2,8 cm la diferencia es de ~30 veces.
    const float depth = 28000.0f;
    CHECK(gasExchangeRate(depth, 0.0f) == doctest::Approx(units::kDiffusionO2 / (depth * depth)));
    CHECK(gasExchangeRate(depth, 2.07f) == doctest::Approx(2.07 * 1e4 / 3600.0 / depth));
    for (const float k : {0.0f, 2.07f}) {
        CultureConfig cfg = tube();
        cfg.depthUm = depth;
        cfg.gasTransferCmH = k;
        Culture c(cfg);
        const double sat = c.concentration(Solute::Oxygen);
        c.setConcentration(Solute::Oxygen, 0.0);
        for (int s = 0; s < 60; ++s) // 1 h
            c.step(kStep);
        const double expected = sat * (1.0 - std::exp(-gasExchangeRate(depth, k) * 3600.0));
        MESSAGE("k = " << k << " cm/h: O2 " << c.concentration(Solute::Oxygen) << " µM al cabo de 1 h (esperado " << expected << ")");
        CHECK(c.concentration(Solute::Oxygen) == doctest::Approx(expected).epsilon(0.02));
    }
    // En vivo: cambiar la mezcla cambia la tasa.
    Culture c(tube());
    c.setGasTransfer(5.0f);
    CHECK(c.gasTransfer() == doctest::Approx(5.0f));
}

TEST_CASE("cultivo: el biovolumen de cada especie es la suma del de sus células") {
    Culture c(tube());
    c.inoculate(SpeciesId::Paramecium, 10);
    c.inoculate(SpeciesId::ParameciumAurelia, 10);
    const CultureCensus census = c.census();
    for (const SpeciesId id : {SpeciesId::Paramecium, SpeciesId::ParameciumAurelia}) {
        double sum = 0.0;
        for (const CultureCell& cell : c.cells())
            sum += cell.species == id ? cellVolumeUm3(speciesInfo(id), cell.meta.biomass) : 0.0;
        CHECK(census.biovolume[static_cast<int>(id)] == doctest::Approx(sum));
    }
    // P. aurelia es más chico: por célula, ~0,4-0,5 del volumen de P. caudatum (Gause midió 0,39).
    const double ratio = census.biovolume[static_cast<int>(SpeciesId::ParameciumAurelia)] /
                         census.biovolume[static_cast<int>(SpeciesId::Paramecium)];
    MESSAGE("volumen de P. aurelia / P. caudatum: " << ratio);
    CHECK(ratio > 0.3);
    CHECK(ratio < 0.6);
}
