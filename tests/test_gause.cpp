// Validación: los experimentos de Gause (1934) en el cultivo bien mezclado (fase P1, paso 5).
//
// Protocolo de Gause (The Struggle for Existence, cap. V, sección II): tubos de centrífuga con 5 cm³
// de medio salino de Osterhout a 26 °C y 20 individuos de cada especie. Cada día se revuelve, se
// saca una muestra de 0,5 cm³ que se cuenta y se destruye (el raleo de la décima parte), se
// centrifuga, se cambia el medio y se agrega una cantidad fija de bacterias. Así la comida no se
// acumula: la población de equilibrio la fija la ración diaria. Gause compara "volúmenes" (sección
// II(4)): aquí, el biovolumen de cada especie (cellVolumeUm3).
#include "game/culture_run.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <utility>
#include <string>
#include <vector>

namespace {

constexpr double kDay = 24.0 * 3600.0;

// `thinning`: la muestra diaria que se descarta (Gause la saca antes del cambio de medio).
CultureRun gauseTube(std::uint64_t seed, float bacteriaPerMl = 5e7f, float thinning = 0.1f) {
    CultureSpec s;
    s.vessel = "gause_tube";
    s.medium = "osterhout_gause";
    s.temperature = 26.0f;
    s.seed = seed;
    if (thinning > 0.0f)
        s.program.entries.push_back({"dilution", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, thinning});
    s.program.entries.push_back({"medium", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 1.0f});
    s.program.entries.push_back({"bacteria", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, bacteriaPerMl});
    CultureRun r;
    r.start(s);
    r.apply({0, CultureOp::Bacteria, "", bacteriaPerMl});
    return r;
}

} // namespace

TEST_CASE("validación: Gause — cada Paramecium solo llega a una meseta" * doctest::test_suite("validacion")) {
    // En monocultivo las dos crecen hasta una capacidad de carga que fija la ración diaria (fig. 21):
    // el biovolumen se estabiliza hacia el día 10. El número de individuos sigue subiendo porque las
    // células hambrientas se dividen más chicas (docs/bugs/starved-cultures-keep-dividing.md).
    for (const char* id : {"paramecium", "paramecium_aurelia"}) {
        const int sp = static_cast<int>(*findSpecies(id));
        CultureRun r = gauseTube(22);
        r.apply({0, CultureOp::Inoculate, id, 20});
        const double start = r.culture().census().biovolume[sp];
        r.advance(16 * kDay);
        const double day16 = r.culture().census().biovolume[sp];
        r.advance(4 * kDay);
        const double day20 = r.culture().census().biovolume[sp];
        MESSAGE(id << ": biovolumen (10⁶ µm³) al empezar " << start * 1e-6 << ", día 16 " << day16 * 1e-6 << ", día 20 "
                   << day20 * 1e-6 << " (" << r.culture().count(static_cast<SpeciesId>(sp)) << " individuos)");
        CHECK(day16 > 20.0 * start);                  // creció
        CHECK(std::abs(day20 - day16) < 0.1 * day16); // meseta
    }
}

namespace {

// Cultivo mixto de las dos especies (20 + 20) hasta el día `days`: biovolumen de cada una [µm³].
std::pair<double, double> mixed(std::uint64_t seed, double days) {
    CultureRun r = gauseTube(seed);
    r.apply({0, CultureOp::Inoculate, "paramecium", 20});
    r.apply({0, CultureOp::Inoculate, "paramecium_aurelia", 20});
    r.advance(days * kDay);
    const CultureCensus c = r.culture().census();
    return {c.biovolume[static_cast<int>(SpeciesId::ParameciumAurelia)], c.biovolume[static_cast<int>(SpeciesId::Paramecium)]};
}

} // namespace

TEST_CASE("validación: Gause — en el cultivo mixto hay exclusión competitiva" * doctest::test_suite("validacion")) {
    // Con una sola comida y el raleo diario, una de las dos especies queda en minoría y baja hacia
    // la extinción (Gause 1934, cap. V).
    int excluded = 0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const auto [aurelia, caudatum] = mixed(seed, 60.0);
        const double minority = std::min(aurelia, caudatum) / std::max(1.0, aurelia + caudatum);
        MESSAGE("seed " << seed << ", día 60 (biovolumen, 10⁶ µm³): P. aurelia " << aurelia * 1e-6 << ", P. caudatum "
                        << caudatum * 1e-6);
        excluded += minority < 0.2 ? 1 : 0;
    }
    CHECK(excluded >= 4);
}

TEST_CASE("validación: Gause — P. aurelia excluye a P. caudatum" * doctest::test_suite("validacion")) {
    // Gause (1934, fig. 22): con el raleo diario P. aurelia desplaza a P. caudatum "primarily owing
    // to its high coefficient of multiplication" (sección III(5)). Sin el raleo ganaba la que se
    // adelantaba al principio.
    int wins = 0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const auto [aurelia, caudatum] = mixed(seed, 25.0);
        MESSAGE("seed " << seed << ", día 25 (biovolumen, 10⁶ µm³): P. aurelia " << aurelia * 1e-6 << ", P. caudatum "
                        << caudatum * 1e-6);
        wins += aurelia > caudatum ? 1 : 0;
    }
    CHECK(wins >= 4);
}

// DISCREPANCIA CONOCIDA (docs/bugs/gause-aurelia-does-not-win.md): el desplazamiento es más lento que
// el de Gause (día 25: P. caudatum es 23 de 163 en volumen, 0,14). `should_fail` avisa cuando se
// resuelva; no se ajustan parámetros.
TEST_CASE("validación: Gause — el desplazamiento llega al de Gause al día 25" * doctest::test_suite("validacion") *
          doctest::should_fail()) {
    double caudatumShare = 0.0;
    for (std::uint64_t seed = 1; seed <= 5; ++seed) {
        const auto [aurelia, caudatum] = mixed(seed, 25.0);
        caudatumShare += caudatum / std::max(1.0, aurelia + caudatum);
    }
    caudatumShare /= 5.0;
    MESSAGE("fracción media del biovolumen de P. caudatum al día 25: " << caudatumShare << " (Gause: 23 / 163 = 0,14)");
    CHECK(caudatumShare < 0.3);
}

TEST_CASE("validación: Gause — Didinium sin refugio extingue a Paramecium" * doctest::test_suite("validacion")) {
    // Gause (1934, cap. VI, fig. 32 y 33): en un medio homogéneo sin sedimento, Didinium come a todos
    // los paramecios y después muere de hambre. Didinium nasutum también se enquista cuando se queda
    // sin presas (Beers 1927, J. Exp. Zool. 48: 1-50): en el modelo no queda ninguno activo.
    int bothGone = 0;
    for (std::uint64_t seed = 1; seed <= 3; ++seed) {
        // Sin el raleo del experimento de competencia; las densidades de antes (20 y 3 en 0,5 mL).
        CultureRun r = gauseTube(seed, 5e7f, 0.0f);
        r.apply({0, CultureOp::Inoculate, "paramecium", 200});
        r.advance(2 * kDay);
        r.apply({0, CultureOp::Inoculate, "didinium", 30});
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
        CHECK(predatorPeak > 30); // Didinium se multiplicó comiendo
        bothGone += prey == 0 && predator == 0 ? 1 : 0;
    }
    CHECK(bothGone >= 2);
}
