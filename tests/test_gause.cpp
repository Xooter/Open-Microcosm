// Validación: los experimentos de Gause (1934) en el cultivo bien mezclado (fase P1, paso 5).
//
// Protocolo de Gause (The Struggle for Existence, cap. V): tubos con 0,5 cm³ de medio salino de
// Osterhout a 26 °C; cada día se centrifuga, se cambia el medio y se agrega una cantidad fija de
// bacterias. Así la comida no se acumula: la población de equilibrio la fija la ración diaria.
#include "game/culture_run.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <utility>
#include <string>
#include <vector>

namespace {

constexpr double kDay = 24.0 * 3600.0;

CultureRun gauseTube(std::uint64_t seed, float bacteriaPerMl = 5e7f) {
    CultureSpec s;
    s.vessel = "tubo_gause";
    s.medium = "osterhout_gause";
    s.temperature = 26.0f;
    s.seed = seed;
    s.program.entries.push_back({"medio", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 1.0f});
    s.program.entries.push_back({"bacterias", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, bacteriaPerMl});
    CultureRun r;
    r.start(s);
    r.apply({0, CultureOp::Bacteria, "", bacteriaPerMl});
    return r;
}

} // namespace

TEST_CASE("validación: Gause — cada Paramecium solo llega a una meseta" * doctest::test_suite("validacion")) {
    // En monocultivo las dos crecen hasta una capacidad de carga que fija la ración diaria (fig. 22).
    for (const char* id : {"paramecium", "paramecium_aurelia"}) {
        CultureRun r = gauseTube(22);
        r.apply({0, CultureOp::Inoculate, id, 5});
        r.advance(16 * kDay);
        const int day16 = r.culture().count(*findSpecies(id));
        r.advance(4 * kDay);
        const int day20 = r.culture().count(*findSpecies(id));
        MESSAGE(id << ": día 16 " << day16 << ", día 20 " << day20 << " individuos por tubo");
        CHECK(day16 > 20);                    // creció desde 5
        CHECK(std::abs(day20 - day16) < 0.3 * day16); // meseta
    }
}

namespace {

// Cultivo mixto de las dos especies (5 + 5) hasta el día `days`: individuos de cada una.
std::pair<int, int> mixed(std::uint64_t seed, double days) {
    CultureRun r = gauseTube(seed);
    r.apply({0, CultureOp::Inoculate, "paramecium", 5});
    r.apply({0, CultureOp::Inoculate, "paramecium_aurelia", 5});
    r.advance(days * kDay);
    return {r.culture().count(SpeciesId::ParameciumAurelia), r.culture().count(SpeciesId::Paramecium)};
}

} // namespace

TEST_CASE("validación: Gause — en el cultivo mixto hay exclusión competitiva" * doctest::test_suite("validacion")) {
    // Lo que el modelo reproduce: con una sola comida, una de las dos especies queda en minoría y
    // baja hacia la extinción (Gause 1934, cap. V). Cuál gana depende de la réplica: ver el test que
    // sigue.
    int excluded = 0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const auto [aurelia, caudatum] = mixed(seed, 60.0);
        const double minority = static_cast<double>(std::min(aurelia, caudatum)) / std::max(1, aurelia + caudatum);
        MESSAGE("seed " << seed << ", día 60: P. aurelia " << aurelia << ", P. caudatum " << caudatum);
        excluded += minority < 0.2 ? 1 : 0;
    }
    CHECK(excluded >= 4);
}

// DISCREPANCIA CONOCIDA (docs/validacion.md, «Gause en el cultivo»): Gause (1934, fig. 23; datos en
// gauseR) vio que gana P. aurelia (día 25: 140 contra 23 en volumen relativo). En el modelo gana la
// que se adelanta al principio: P. aurelia en 2 o 3 de 5 réplicas. La ventaja de P. aurelia sale solo
// de la alometría de la alimentación (~20 % más aclaramiento por biomasa) y no alcanza contra el
// µmax mayor de P. caudatum. `should_fail` avisa cuando se resuelva; no se ajustan parámetros.
TEST_CASE("validación: Gause — P. aurelia excluye a P. caudatum" * doctest::test_suite("validacion") *
          doctest::should_fail()) {
    int wins = 0;
    double caudatumShare = 0.0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const auto [aurelia, caudatum] = mixed(seed, 25.0);
        MESSAGE("seed " << seed << ": P. aurelia " << aurelia << ", P. caudatum " << caudatum);
        wins += aurelia > caudatum ? 1 : 0;
        caudatumShare += static_cast<double>(caudatum) / std::max(1, aurelia + caudatum);
    }
    caudatumShare /= 5.0;
    MESSAGE("fracción media de P. caudatum al día 25: " << caudatumShare << " (Gause: 23 / 163 = 0,14)");
    CHECK(wins >= 4);
    CHECK(caudatumShare < 0.3);
}

TEST_CASE("validación: Gause — Didinium sin refugio extingue a Paramecium" * doctest::test_suite("validacion")) {
    // Gause (1934, cap. VI, fig. 32 y 33): en un medio homogéneo sin sedimento, Didinium come a todos
    // los paramecios y después muere de hambre. Didinium nasutum también se enquista cuando se queda
    // sin presas (Beers 1927, J. Exp. Zool. 48: 1-50): en el modelo no queda ninguno activo.
    int bothGone = 0;
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        CultureRun r = gauseTube(seed);
        r.apply({0, CultureOp::Inoculate, "paramecium", 20});
        r.advance(2 * kDay);
        r.apply({0, CultureOp::Inoculate, "didinium", 3});
        int preyPeak = 0, predatorPeak = 0;
        for (int d = 0; d < 14; ++d) {
            r.advance(kDay);
            preyPeak = std::max(preyPeak, r.culture().count(SpeciesId::Paramecium));
            predatorPeak = std::max(predatorPeak, r.culture().count(SpeciesId::Didinium));
        }
        const int prey = r.culture().count(SpeciesId::Paramecium);
        int predator = 0; // activos (los quistes no cazan)
        for (const CultureCell& c : r.culture().cells())
            predator += c.species == SpeciesId::Didinium && !c.cyst ? 1 : 0;
        MESSAGE("seed " << seed << ": pico de paramecios " << preyPeak << ", de Didinium " << predatorPeak << "; día 16: "
                        << prey << " y " << predator);
        CHECK(predatorPeak > 3); // Didinium se multiplicó comiendo
        bothGone += prey == 0 && predator == 0 ? 1 : 0;
    }
    CHECK(bothGone >= 2);
}
