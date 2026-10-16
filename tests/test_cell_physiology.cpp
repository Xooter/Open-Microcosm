// Fisiología compartida entre la placa y el cultivo bien mezclado (fase P1, paso 0).
#include "bio/bacteria.hpp"
#include "bio/cell_physiology.hpp"
#include "bio/feeding.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

TEST_CASE("digestión sin cuerpo: conserva la masa y el fósforo de las presas") {
    Digestion dig;
    FeedingParams params;
    for (int k = 0; k < 40; ++k)
        ingestContents(dig, kBacteriumArea, kBacteriumPmol, false, 1e-5f, 60.0f, 1e-4f);
    ingestContents(dig, 300.0f, 2.0f, true, 0.02f, 1.0f, 0.3f); // una presa entera
    REQUIRE(dig.vacuoles.size() == 2);
    Excreted out;
    float yield = 0.0f;
    for (int s = 0; s < 20000 && !dig.vacuoles.empty(); ++s)
        yield += digestContents(dig, params, 1.0f, 60.0f, out);
    CHECK(dig.vacuoles.empty()); // todas se cerraron, se digirieron y se expulsaron
    CHECK(dig.pmolIngested == doctest::Approx(40 * kBacteriumPmol + 2.0f));
    CHECK(out.phosphorus == doctest::Approx(40 * 1e-5f + 0.02f).epsilon(1e-4));
    CHECK(out.nitrogen == doctest::Approx(40 * 1e-4f + 0.3f).epsilon(1e-4));
    // Las bacterias rinden por bioTimeScale; lo que falta es lo expulsado sin digerir (< 2 %).
    CHECK(yield > 0.98f * (40 * kBacteriumPmol * 60.0f + 2.0f));
    CHECK(yield <= 40 * kBacteriumPmol * 60.0f + 2.0f + 1e-4f);
}

TEST_CASE("estrés del ambiente: causa de muerte y detoxificación de la violaceína") {
    SpeciesInfo info;
    info.tMin = 5.0f;
    info.tMax = 30.0f;
    CellSurroundings around;
    around.temperature = 20.0f;
    MetabolicInput in{};
    CHECK(applyEnvironmentStress(info, around, nullptr, 60.0f, in) == nullptr);
    CHECK(in.phStress == 0.0f);
    around.temperature = 40.0f; // 10 °C de más: dos unidades
    CHECK(std::string(applyEnvironmentStress(info, around, nullptr, 60.0f, in)) == "calor");
    CHECK(in.phStress == doctest::Approx(2.0f));
    around.temperature = 20.0f;
    float violacein = 2.0f;
    CHECK(std::string(applyEnvironmentStress(info, around, &violacein, 3600.0f, in)) == "intoxicada por violaceina");
    CHECK(violacein == doctest::Approx(2.0f * std::exp(-1.0f)));
}

TEST_CASE("bacteria: la mantención en ayuno es continua y la infectada no crece") {
    BacteriaParams p;
    const float maintenance = 0.01f;
    // Lo comido justo igual a la mantención: no cambia su masa.
    const float even = maintenance * 1.5f * kBacteriumPmol / p.yield;
    CHECK(bacterialMassChange(p, even, 1.5f, maintenance) == doctest::Approx(0.0f).epsilon(1e-6));
    // Sin comida adelgaza a la fracción de ayuno de la mantención.
    CHECK(bacterialMassChange(p, 0.0f, 1.5f, maintenance) == doctest::Approx(-p.starvationMaintenance * maintenance * 1.5f));
    CHECK(bacterialDemand(p, 10.0f, 1.0f, bacterialHealth(p, true, false), 1.0f) == 0.0f);
    CHECK(bacterialHealth(p, false, true) == doctest::Approx(1.0f - p.resistanceCost));
}
