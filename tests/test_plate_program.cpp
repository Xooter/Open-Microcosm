// Programas temporales en la placa (fase P1, paso 12).
#include "game/intervention.hpp"
#include "game/plate_spec.hpp"
#include "game/program.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

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
