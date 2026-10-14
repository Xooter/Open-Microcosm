// Registro de parámetros (MVP: reproducibilidad): claves únicas, ida y vuelta de cada valor y
// exportación completa con su procedencia.
#include "game/export.hpp"
#include "game/parameters.hpp"
#include "game/plate_spec.hpp"
#include "game/run.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <set>
#include <string>

namespace {
WorldConfig smallConfig() {
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

TEST_CASE("parámetros: claves únicas y cada valor va y vuelve") {
    World world(smallConfig());
    std::set<std::string> keys;
    for (const ParamInfo& p : parameters()) {
        CHECK_MESSAGE(keys.insert(p.key).second, p.key);
        CHECK(findParameter(p.key) == &p);
        CHECK(((p.real != nullptr) != (p.integer != nullptr)));
        const float before = parameterValue(world, p);
        const float probe = p.integer ? before + 1.0f : before * 1.5f + 0.25f;
        setParameterValue(world, p, probe);
        CHECK_MESSAGE(parameterValue(world, p) == doctest::Approx(probe), p.key);
        setParameterValue(world, p, before);
    }
    CHECK(findParameter("no.existe") == nullptr);
}

TEST_CASE("parámetros: la corrida exportada los lleva todos, con la procedencia") {
    World world(smallConfig());
    Run run;
    RunConfig rc;
    rc.spawnFounder = false;
    rc.wildlife = 0.0f;
    run.start(world, rc, 1);
    RunProvenance prov;
    prov.engine = "GPU";
    prov.gpu = "prueba";
    const std::string json = runJson(world, run, PlateSpec{}, prov);
    for (const ParamInfo& p : parameters())
        CHECK_MESSAGE(json.find(std::string("\"") + p.key + "\"") != std::string::npos, p.key);
    for (const char* key : {"\"procedencia\"", "\"commit\"", "\"compilacion\"", "\"motor_xpbd\": \"GPU\"",
                            "\"gpu\": \"prueba\"", "\"reproducibilidad\"", "\"registro_de_parametros\""})
        CHECK_MESSAGE(json.find(key) != std::string::npos, key);
}

TEST_CASE("PlateSpec: un seed de 64 bits no pierde precisión") {
    PlateSpec a;
    a.seed = 0xfedcba9876543211ull;
    a.rebuild();
    PlateSpec b;
    REQUIRE(parsePlate(serializePlate(a), b));
    CHECK(b.seed == a.seed);
}
