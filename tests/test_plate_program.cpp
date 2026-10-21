// Programas temporales en la placa (fase P1, paso 12).
#include "game/intervention.hpp"
#include "game/plate_spec.hpp"
#include "game/program.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

namespace {

WorldConfig small() {
    WorldConfig c;
    c.gridX = c.gridY = 64;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.medium.detritusSpawnRate = 0.0f;
    return c;
}

} // namespace

TEST_CASE("programa en la placa: fotoperíodo y rampa con las horas biológicas") {
    WorldConfig c = small();
    c.program.entries.push_back({"light", ProgramShape::Cycle, 0.0, kProgramForever, 0.0f, 0.8f, 24.0f, 16.0f / 24.0f, 0.0f});
    c.program.entries.push_back({"temperature", ProgramShape::Ramp, 0.0, 10.0, 20.0f, 30.0f, 24.0f, 0.5f, 0.0f});
    World w(c);
    w.step(1.0f / 60.0f);
    CHECK(w.medium().surfaceLight == doctest::Approx(0.8f));
    // 5 h biológicas = 5 min reales con la escala de 60: la rampa va por la mitad.
    for (int s = 0; s < 5 * 3600; ++s)
        w.step(1.0f / 60.0f);
    CHECK(w.medium().temperature == doctest::Approx(25.0f).epsilon(0.01));
    CHECK(w.bioScale() > 60.0f); // el Q10 sigue a la temperatura
}

TEST_CASE("programa en la placa: se guarda en el .plate y una intervención lo cambia en vivo") {
    PlateSpec p;
    p.config.program.entries.push_back({"solute:phosphate", ProgramShape::Event, 1.0, kProgramForever, 0.0f, 0.0f, 0.0f, 0.5f, 3.0f});
    p.config.program.entries.push_back({"light", ProgramShape::Sine, 0.0, kProgramForever, 0.5f, 0.4f, 24.0f, 0.5f, 0.0f});
    PlateSpec read;
    REQUIRE(parsePlate(serializePlate(p), read));
    REQUIRE(read.config.program.entries.size() == 2);
    CHECK(read.config.program.entries[1].shape == ProgramShape::Sine);
    CHECK(read.config.program.entries[1].from == 0.5f);

    World w(small());
    Intervention iv;
    iv.kind = InterventionKind::Program;
    iv.key = "luz,ciclo,0,inf,0,1,24,0.5,0";
    // La clave sobrevive a la escena (una sola palabra).
    const std::vector<Intervention> back = parseInterventions(serializeInterventions({iv}));
    REQUIRE(back.size() == 1);
    CHECK(applyIntervention(w, back[0]));
    REQUIRE(w.program().entries.size() == 1);
    CHECK(w.program().entries[0].target == "light");
    iv.index = 1;
    applyIntervention(w, iv);
    CHECK(w.program().entries.empty());
}

TEST_CASE("programa en la placa: un pulso agrega soluto a la hora programada") {
    WorldConfig c = small();
    c.medium.phosphate = 5.0f;
    c.program.entries.push_back({"add:phosphate", ProgramShape::Event, 0.5, kProgramForever, 0.0f, 0.0f, 0.0f, 0.5f, 3.0f});
    World w(c);
    const float before = w.meanConcentration(w.phosphateField());
    for (int s = 0; s < 60 * 60; ++s) // 1 h biológica
        w.step(1.0f / 60.0f);
    CHECK(w.meanConcentration(w.phosphateField()) == doctest::Approx(before + 3.0f).epsilon(0.02));
}

TEST_CASE("raleo de la placa: sale la fracción de lo suspendido, lo pegado se queda y el balance cierra") {
    World w(small());
    REQUIRE(w.inoculate({BeingKind::Species, SpeciesId::Paramecium}, 40, {}, 0.0f) == 40);
    REQUIRE(w.inoculate({BeingKind::Bacterium, SpeciesId{}}, 2000, {}, 0.0f) == 2000);
    for (std::size_t i = 0; i < 400; ++i) // biopelícula: pegadas al vidrio
        w.bacteria().attached[i] = 1;
    w.step(1.0f / 60.0f);
    const MassBudget start = w.massBudget();
    const int cells = w.census().species[static_cast<int>(SpeciesId::Paramecium)];
    Intervention iv;
    iv.kind = InterventionKind::Thin;
    iv.v[0] = 0.5f;
    REQUIRE(applyIntervention(w, iv));
    const int left = w.census().species[static_cast<int>(SpeciesId::Paramecium)];
    int attached = 0;
    for (std::size_t i = 0; i < w.bacteria().size(); ++i)
        attached += w.bacteria().attached[i];
    const std::size_t freeLeft = w.bacteria().size() - static_cast<std::size_t>(attached);
    MESSAGE("paramecios " << cells << " -> " << left << ", bacterias libres 1600 -> " << freeLeft << ", pegadas " << attached);
    CHECK(left > 8); // binomial(40, 0,5): ±3 desvíos
    CHECK(left < 32);
    CHECK(attached == 400);
    CHECK(std::abs(static_cast<double>(freeLeft) - 800.0) < 4.0 * std::sqrt(400.0));
    for (Element e : {Element::P, Element::N})
        CHECK(std::abs(budgetResidual(start, w.massBudget(), e)) < 1e-3 * std::max(1.0, start.total(e)));
}

TEST_CASE("raleo de la placa: el programa lo repite cada período (\"dilution\")") {
    // Dos placas iguales, una con el raleo a la mitad cada hora biológica: después de dos raleos
    // quedan ~1/4 de los detritos del control (los dos se descomponen igual).
    auto run = [](bool thinning) {
        WorldConfig c = small();
        if (thinning)
            c.program.entries.push_back({"dilution", ProgramShape::Event, 1.0, kProgramForever, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f});
        World w(c);
        for (int k = 0; k < 400; ++k) {
            Rng rng(7, 99, static_cast<std::uint64_t>(k), 0);
            w.dropDetritus(w.randomPoint(rng, 20.0f));
        }
        while (w.programHours() < 2.05)
            w.step(1.0f / 60.0f);
        return w.ambient().size();
    };
    const std::size_t control = run(false), thinned = run(true);
    MESSAGE("detritos: control " << control << ", raleada " << thinned);
    REQUIRE(control > 200);
    CHECK(static_cast<double>(thinned) / static_cast<double>(control) == doctest::Approx(0.25).epsilon(0.4));
}
