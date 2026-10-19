// Programas temporales (fase P1, paso 3) y cambio de medio y dilución del cultivo.
#include "game/culture.hpp"
#include "game/program.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

TEST_CASE("programa: fotoperíodo, rampa, seno y eventos") {
    ProgramEntry light;
    light.target = "light";
    light.shape = ProgramShape::Cycle;
    light.period = 24.0f;
    light.duty = 16.0f / 24.0f; // 16:8
    light.from = 0.0f;
    light.to = 1.0f;
    float v = -1.0f;
    REQUIRE(programValue(light, 3.0, v));
    CHECK(v == 1.0f);
    REQUIRE(programValue(light, 20.0, v));
    CHECK(v == 0.0f);
    REQUIRE(programValue(light, 24.0 + 3.0, v));
    CHECK(v == 1.0f);

    ProgramEntry ramp;
    ramp.target = "temperature";
    ramp.shape = ProgramShape::Ramp;
    ramp.start = 10.0;
    ramp.end = 20.0;
    ramp.from = 15.0f;
    ramp.to = 25.0f;
    CHECK_FALSE(programValue(ramp, 5.0, v));
    REQUIRE(programValue(ramp, 15.0, v));
    CHECK(v == doctest::Approx(20.0f));
    REQUIRE(programValue(ramp, 99.0, v));
    CHECK(v == doctest::Approx(25.0f)); // después de la rampa, el valor final

    ProgramEntry sine;
    sine.target = "temperature";
    sine.shape = ProgramShape::Sine;
    sine.from = 20.0f;
    sine.to = 5.0f;
    sine.period = 24.0f;
    REQUIRE(programValue(sine, 6.0, v));
    CHECK(v == doctest::Approx(25.0f));

    ProgramEntry feed;
    feed.target = "bacteria";
    feed.shape = ProgramShape::Event;
    feed.start = 0.0;
    feed.period = 24.0f;
    feed.amount = 1e7f;
    CHECK(programEvents(feed, -1.0, 0.0) == 1); // a la hora 0
    CHECK(programEvents(feed, 0.0, 23.9) == 0);
    CHECK(programEvents(feed, 0.0, 72.0) == 3); // 24, 48 y 72
    // La suma de los eventos de pasos chicos es la de un paso grande: no depende del paso.
    int total = 0;
    for (int s = 0; s < 72 * 60; ++s)
        total += programEvents(feed, s / 60.0, (s + 1) / 60.0);
    CHECK(total == 3);
}

TEST_CASE("programa: se guarda y se lee igual (ida y vuelta)") {
    Program p;
    ProgramEntry a;
    a.target = "light";
    a.shape = ProgramShape::Cycle;
    a.duty = 2.0f / 3.0f;
    a.to = 0.7f;
    ProgramEntry b;
    b.target = "inoculate:paramecium";
    b.shape = ProgramShape::Event;
    b.start = 1.0 / 3.0;
    b.end = 100.0;
    b.period = 72.0f;
    b.amount = 1.0f;
    p.entries = {a, b};
    const std::string text = serializeProgram(p);
    Program q;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl - pos);
        ProgramEntry e;
        REQUIRE(line.rfind("program=", 0) == 0);
        REQUIRE(parseProgramEntry(line.substr(8), e));
        q.entries.push_back(e);
        pos = nl + 1;
    }
    REQUIRE(q.entries.size() == 2);
    CHECK(q.entries[0].duty == a.duty);
    CHECK(q.entries[0].end >= 0.5 * kProgramForever);
    CHECK(q.entries[1].target == b.target);
    CHECK(q.entries[1].start == b.start);
    CHECK(q.entries[1].period == b.period);
    CHECK(serializeProgram(q) == text);
}

TEST_CASE("cultivo: cambio de medio y dilución conservan el balance con sus entradas y salidas") {
    CultureConfig cfg;
    cfg.volumeMl = 0.5f;
    cfg.medium.temperature = 20.0f;
    cfg.medium.phage = 0.0f;
    cfg.medium.phosphate = 10.0f;
    cfg.medium.surfaceLight = 1.0f;
    Culture c(cfg);
    c.inoculateBacteria(1'000'000);
    c.inoculate(SpeciesId::Paramecium, 40);
    c.inoculate(SpeciesId::Chlamydomonas, 200);
    // Protocolo de Gause: cada 24 h se cambia el medio y se agregan bacterias frescas.
    ProgramEntry renew;
    renew.target = "medium";
    renew.shape = ProgramShape::Event;
    renew.start = 24.0;
    renew.period = 24.0f;
    renew.amount = 1.0f;
    ProgramEntry feed = renew;
    feed.target = "bacteria";
    feed.amount = 2e6f; // por mL
    c.program().entries = {renew, feed};
    const MassBudget start = c.massBudget();
    for (int s = 0; s < 49 * 60; ++s) // pasa dos cambios
        c.step(60.0f);
    const MassBudget end = c.massBudget();
    for (const Element e : {Element::P, Element::N, Element::Si, Element::Fe}) {
        const double r = budgetResidual(start, end, e);
        MESSAGE(elementName(e) << ": residuo " << r << " (salida con el medio "
                               << end.ledger.fromWater[static_cast<int>(e)][static_cast<int>(Flow::Washout)] << ")");
        CHECK(std::abs(r) <= 1e-4 * std::max(1.0, end.total(e)));
    }
    // Recién cambiado el medio, las bacterias son las frescas (~2·10⁶ por mL) más las que crecieron.
    CHECK(c.bacteriaCount() > 0);
    const int before = c.count(SpeciesId::Paramecium);
    c.dilute(0.5);
    const int after = c.count(SpeciesId::Paramecium);
    MESSAGE("paramecios " << before << " -> " << after << " al diluir a la mitad");
    CHECK(after < before);
    CHECK(after > before / 4);
    const MassBudget diluted = c.massBudget();
    CHECK(std::abs(budgetResidual(start, diluted, Element::P)) <= 1e-4 * std::max(1.0, diluted.total(Element::P)));
}

TEST_CASE("cultivo: un fotoperíodo apaga y prende la luz") {
    CultureConfig cfg;
    cfg.medium.phage = 0.0f;
    Culture c(cfg);
    ProgramEntry light;
    light.target = "light";
    light.shape = ProgramShape::Cycle;
    light.period = 24.0f;
    light.duty = 16.0f / 24.0f;
    light.to = 1.0f;
    c.program().entries = {light};
    c.step(60.0f);
    CHECK(c.light() > 0.99);
    for (int s = 0; s < 18 * 60; ++s)
        c.step(60.0f);
    CHECK(c.light() == 0.0);
}
