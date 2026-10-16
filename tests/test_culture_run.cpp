// Sesión de cultivo, escena de cultivo y gota de muestra (fase P1, paso 4).
#include "game/culture_run.hpp"
#include "game/plate_spec.hpp"
#include "game/run.hpp"
#include "game/scene.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace {

CultureSpec gauseTube(std::uint64_t seed) {
    CultureSpec s;
    s.name = "prueba";
    s.vessel = "tubo_gause";
    s.medium = "osterhout_gause";
    s.temperature = 26.0f;
    s.seed = seed;
    return s;
}

std::vector<double> fingerprint(const CultureRun& r) {
    std::vector<double> v = {static_cast<double>(r.culture().bacteriaCount()), r.culture().volumeMl(),
                             r.culture().amount(Solute::Organic)};
    for (const CultureCell& c : r.culture().cells())
        v.push_back(c.meta.biomass + 1e3 * c.id);
    return v;
}

} // namespace

TEST_CASE("escena de cultivo: guardar y cargar repite la corrida") {
    CultureRun a;
    CultureSpec spec = gauseTube(11);
    spec.program.entries.push_back({"medio", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 1.0f});
    spec.program.entries.push_back({"bacterias", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 5e7f});
    a.start(spec);
    CHECK(a.apply({0, CultureOp::Bacteria, "", 5e7}));
    CHECK(a.apply({0, CultureOp::Inoculate, "paramecium", 5}));
    a.advance(10 * 3600.0);
    CHECK(a.apply({0, CultureOp::Inoculate, "paramecium_aurelia", 5}));
    a.advance(20 * 3600.0);
    CHECK(a.apply({0, CultureOp::Dilute, "", 0.2}));
    a.advance(30 * 3600.0);
    const std::string text = serializeCultureScene(a.spec(), a.events());

    CultureSpec read;
    std::vector<CultureEvent> events;
    REQUIRE(parseCultureScene(text, read, events));
    CHECK(read.program.entries.size() == 2);
    CHECK(events.size() == a.events().size());
    CultureRun b;
    b.start(read);
    b.schedule(events);
    b.advance(60 * 3600.0);
    CHECK(fingerprint(a) == fingerprint(b));
    CHECK(a.culture().count(SpeciesId::Paramecium) > 0);
}

TEST_CASE("gota de muestra: lleva las células del cultivo con su estado a la placa") {
    CultureRun r;
    r.start(gauseTube(5));
    r.apply({0, CultureOp::Bacteria, "", 1e8});
    r.apply({0, CultureOp::Inoculate, "paramecium", 300});
    r.advance(2 * 3600.0);
    const int before = r.culture().count(SpeciesId::Paramecium);
    const double volumeBefore = r.culture().volumeMl();
    // 0,5 mL con ~600 por mL: una gota de ~0,56 µL trae ~0,3; concentrada 200 veces, ~70.
    const PlateSpec drop = r.sampleDrop(200.0f);
    const int taken = static_cast<int>(drop.config.sampled.size());
    MESSAGE("paramecios " << before << ", en la gota " << taken << ", bacterias " << drop.config.bacteria);
    CHECK(taken > 10);
    CHECK(r.culture().count(SpeciesId::Paramecium) == before - taken);
    CHECK(r.culture().volumeMl() < volumeBefore);
    CHECK(r.events().back().op == CultureOp::Sample);

    // El .plate (versión 3) guarda las células con su estado.
    PlateSpec read;
    REQUIRE(parsePlate(serializePlate(drop), read));
    REQUIRE(read.config.sampled.size() == drop.config.sampled.size());
    CHECK(read.config.sampled[0].meta.biomass == drop.config.sampled[0].meta.biomass);
    CHECK(read.config.sampled[0].genome.v == drop.config.sampled[0].genome.v);

    // Y la placa las ubica con ese estado.
    World world(read.config);
    Run run;
    run.start(world, plateRunConfig(read), 1);
    CHECK(world.census().species[static_cast<int>(SpeciesId::Paramecium)] == taken);
    double biomass = 0.0, expected = 0.0;
    for (const auto [e, m, sp] : world.registry().view<const Metabolism, const Species>().each())
        biomass += m.biomass;
    for (const SampledCell& s : read.config.sampled)
        expected += s.meta.biomass;
    CHECK(biomass == doctest::Approx(expected).epsilon(1e-5));
}

TEST_CASE("alícuota: el conteo es Poisson alrededor de la densidad") {
    CultureRun r;
    r.start(gauseTube(9));
    r.apply({0, CultureOp::Inoculate, "paramecium", 200}); // 400 por mL
    double sum = 0.0;
    const int n = 400;
    for (int k = 0; k < n; ++k)
        sum += r.countAliquot(SpeciesId::Paramecium, 0.1f, static_cast<std::uint64_t>(k));
    CHECK(sum / n == doctest::Approx(40.0).epsilon(0.05));
}
