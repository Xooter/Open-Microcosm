// Fase 13: instrumentos de medición (serie temporal, curva de crecimiento, región, exportación).
#include "game/export.hpp"
#include "game/growth_fit.hpp"
#include "game/region.hpp"
#include "game/run.hpp"
#include "game/timeseries.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig quietWorld() {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.saprolegnia = config.chytridClusters = 0;
    config.medium.detritusSpawnRate = 0.0f;
    config.seed = 31;
    return config;
}

std::vector<Vec2> square(Vec2 c, float half) {
    return {c + Vec2{-half, -half}, c + Vec2{half, -half}, c + Vec2{half, half}, c + Vec2{-half, half}};
}

} // namespace

TEST_CASE("serie temporal: el tope de filas decima sin perder el comienzo") {
    TimeSeries s(8);
    s.setColumns({{"t", "s", "Tiempo"}, {"x", "u", "g"}});
    for (int i = 0; i < 40; ++i) {
        if (!s.offer())
            continue;
        const float row[2] = {static_cast<float>(i), 2.0f * static_cast<float>(i)};
        s.push(row);
    }
    CHECK(s.rows() <= 8);
    CHECK(s.rows() >= 4);
    CHECK(s.stride() > 1);
    CHECK(s.column(0)[0] == 0.0f);
    for (std::size_t r = 1; r < s.rows(); ++r) // el tiempo sigue creciendo y las columnas alineadas
        CHECK(s.column(0)[r] > s.column(0)[r - 1]);
    CHECK(s.column(1)[s.rows() - 1] == 2.0f * s.column(0)[s.rows() - 1]);
    CHECK(s.find("x") == 1);
    CHECK(s.find("nada") == -1);
}

TEST_CASE("curva de crecimiento: recupera µmax, latencia y K de datos sintéticos") {
    SUBCASE("logística") {
        const float k = 1000.0f, r = 0.5f, c = std::log(k / 10.0f - 1.0f);
        std::vector<float> t, n;
        for (int i = 0; i <= 60; ++i) {
            t.push_back(0.5f * static_cast<float>(i));
            n.push_back(k / (1.0f + std::exp(c - r * t.back())));
        }
        const GrowthFit fit = fitGrowth(t.data(), n.data(), t.size());
        REQUIRE(fit.valid);
        MESSAGE("µmax " << fit.muMax << " K " << fit.capacity);
        CHECK(fit.muMax == doctest::Approx(r).epsilon(0.1));
        REQUIRE(fit.logisticValid);
        CHECK(fit.capacity == doctest::Approx(k).epsilon(0.03));
        CHECK(fit.plateau);
        CHECK(fit.logistic(15.0f) == doctest::Approx(n[30]).epsilon(0.05));
    }
    SUBCASE("exponencial tras una latencia") {
        std::vector<float> t, n;
        for (int i = 0; i <= 40; ++i) {
            t.push_back(0.5f * static_cast<float>(i));
            n.push_back(t.back() < 5.0f ? 10.0f : 10.0f * std::exp(0.3f * (t.back() - 5.0f)));
        }
        const GrowthFit fit = fitGrowth(t.data(), n.data(), t.size());
        REQUIRE(fit.valid);
        MESSAGE("latencia " << fit.lagHours << " h, duplicación " << fit.doublingHours << " h");
        CHECK(fit.muMax == doctest::Approx(0.3f).epsilon(0.05));
        CHECK(fit.doublingHours == doctest::Approx(std::log(2.0f) / 0.3f).epsilon(0.05));
        CHECK(fit.lagHours == doctest::Approx(5.0f).epsilon(0.15));
        CHECK_FALSE(fit.plateau);
    }
    SUBCASE("sin crecimiento no inventa números") {
        std::vector<float> t, n;
        for (int i = 0; i < 20; ++i) {
            t.push_back(static_cast<float>(i));
            n.push_back(50.0f - static_cast<float>(i));
        }
        CHECK_FALSE(fitGrowth(t.data(), n.data(), t.size()).valid);
        CHECK_FALSE(fitGrowth(t.data(), n.data(), 3).valid);
    }
}

TEST_CASE("regla: largo y área en µm") {
    CHECK(polylineLength({{0, 0}, {30, 40}, {30, 50}}) == doctest::Approx(60.0f));
    CHECK(polygonArea(square({100, 100}, 10.0f)) == doctest::Approx(400.0f));
    std::vector<Vec2> cw = square({0, 0}, 5.0f);
    std::reverse(cw.begin(), cw.end());
    CHECK(polygonArea(cw) == doctest::Approx(100.0f)); // sin signo
    CHECK(polygonArea({{0, 0}, {1, 1}}) == 0.0f);
}

TEST_CASE("cuantificación por región: cuenta lo de adentro y promedia la química") {
    World world(quietWorld());
    const Vec2 c = world.dish().center;
    world.spawnOrganism(SpeciesId::Navicula, c, speciesGenome(SpeciesId::Navicula), 0);
    world.spawnOrganism(SpeciesId::Paramecium, c + Vec2{400.0f, 0.0f}, speciesGenome(SpeciesId::Paramecium), 0);
    const RegionStats in = quantifyRegion(world, square(c, 80.0f));
    REQUIRE(in.valid);
    CHECK(in.cells == 1);
    CHECK(in.species[static_cast<int>(SpeciesId::Navicula)] == 1);
    CHECK(in.area == doctest::Approx(160.0f * 160.0f));
    CHECK(in.waterCells > 0);
    CHECK(in.phosphate == doctest::Approx(quietWorld().medium.phosphate).epsilon(0.05));
    CHECK(in.ph > 4.0f);
    const RegionStats both = quantifyRegion(world, square(c + Vec2{200.0f, 0.0f}, 300.0f));
    CHECK(both.cells == 2);
    CHECK_FALSE(quantifyRegion(world, {}).valid);
}

TEST_CASE("exportar: CSV con unidades y JSON con parámetros, serie y balance") {
    World world(quietWorld());
    Run run;
    RunConfig rc;
    rc.spawnFounder = false;
    rc.wildlife = 0.0f;
    run.start(world, rc, 1);
    world.spawnOrganism(SpeciesId::Euglena, world.dish().center, speciesGenome(SpeciesId::Euglena), 0);
    run.setRegion(world, {world.dish().center + Vec2{-50, -50}, world.dish().center + Vec2{50, -50}, world.dish().center + Vec2{0, 50}});
    for (int s = 0; s < 60 * 3; ++s) {
        world.step(kDt);
        run.update(world, kDt);
    }
    REQUIRE(run.series().rows() >= 3);
    const std::string csv = seriesCsv(run.series());
    CHECK(csv.find("t [s],t bio [h],paramecium [individuos]") == 0); // las especies van por id
    CHECK(csv.find("O2 [µM]") != std::string::npos);
    CHECK(csv.find("P total [pmol P]") != std::string::npos);
    CHECK(csv.find("Región: células [individuos]") != std::string::npos);
    const std::size_t lines = static_cast<std::size_t>(std::count(csv.begin(), csv.end(), '\n'));
    CHECK(lines == run.series().rows() + 1);
    CHECK(csv.find("nan") == std::string::npos);

    PlateSpec plate;
    const std::string json = runJson(world, run, plate);
    for (const char* key : {"\"parametros\"", "\"placa\"", "\"metabolismo\"", "\"serie\"", "\"columnas\"",
                            "\"balance_de_masa\"", "\"residuo\"", "\"unidad\": \"µM\"", "\"plate_version\"",
                            "\"paquetes\"", "\"id\": \"base\"", "\"huella\"", "\"bioma_id\""})
        CHECK_MESSAGE(json.find(key) != std::string::npos, key);
    // Llaves y corchetes balanceados (JSON bien formado a grandes rasgos).
    int depth = 0;
    bool inString = false;
    for (std::size_t i = 0; i < json.size(); ++i) {
        const char ch = json[i];
        if (ch == '"' && (i == 0 || json[i - 1] != '\\'))
            inString = !inString;
        else if (!inString && (ch == '{' || ch == '['))
            ++depth;
        else if (!inString && (ch == '}' || ch == ']'))
            --depth;
        CHECK(depth >= 0);
    }
    CHECK(depth == 0);
    CHECK(json.find("nan") == std::string::npos);

    const std::string species = speciesJson();
    for (int s = 0; s < speciesCount(); ++s)
        CHECK(species.find(speciesInfo(static_cast<SpeciesId>(s)).name) != std::string::npos);
    CHECK(species.find("\"unidad\": \"°C\"") != std::string::npos);
}
