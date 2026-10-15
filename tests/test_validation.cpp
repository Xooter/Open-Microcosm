// Validación contra resultados publicados (MVP). Suite "validacion": ctest -L validacion.
//
// Cada test reproduce un experimento real y compara con la fuente (assets/species/fisiologia.toml,
// docs/validacion.md). Son largos: la biología se acelera con la escala de tiempo biológico (600),
// que no cambia las proporciones entre procesos.
#include "bio/physiology.hpp"
#include "game/sandbox.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig cultureConfig(std::size_t bacteria) {
    WorldConfig c;
    c.gridX = c.gridY = 64; // placa de 512 µm
    c.bacteria = bacteria;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.medium.temperature = 20.0f; // la referencia de la fisiología: sin corrección Q10
    c.medium.detritusSpawnRate = 0.0f;
    c.seed = 2024;
    return c;
}

double speciesBiomass(const World& w, SpeciesId id) {
    double total = 0.0;
    for (const auto [e, m, sp] : w.registry().view<const Metabolism, const Species>().each())
        if (sp.id == id)
            total += m.biomass;
    return total;
}

// Pendiente de ln(y) contra t por mínimos cuadrados.
double logSlope(const std::vector<double>& t, const std::vector<double>& y) {
    double st = 0, sy = 0, stt = 0, sty = 0;
    const double n = static_cast<double>(t.size());
    for (std::size_t i = 0; i < t.size(); ++i) {
        const double ly = std::log(y[i]);
        st += t[i];
        sy += ly;
        stt += t[i] * t[i];
        sty += t[i] * ly;
    }
    return (n * sty - st * sy) / (n * stt - st * st);
}

// Cultivo sin limitación de una heterótrofa: tasa de crecimiento de la biomasa total [1/h bio].
double cultureGrowthRate(SpeciesId id, float hours) {
    World world(cultureConfig(20'000));
    world.bioTimeScale() = 600.0f;
    world.medium().organicInflow = 400.0f; // las bacterias se reponen: la comida no limita
    const Vec2 c = {world.width() * 0.5f, world.height() * 0.5f};
    for (int k = 0; k < 4; ++k)
        world.spawnBeing({BeingKind::Species, id}, c + Vec2{60.0f * std::cos(1.6f * k), 60.0f * std::sin(1.6f * k)});
    std::vector<double> t, b;
    const float bioPerStep = world.bioTimeScale() * kDt / 3600.0f; // [h bio por paso]
    const int steps = static_cast<int>(hours / bioPerStep);
    for (int s = 0; s <= steps; ++s) {
        if (s % 60 == 0 && s > steps / 3) { // la fase exponencial, sin la aclimatación inicial
            t.push_back(world.simTime() * world.bioScale() / 3600.0);
            b.push_back(speciesBiomass(world, id));
        }
        world.step(kDt);
    }
    return logSlope(t, b);
}

} // namespace

TEST_CASE("validación: µmax de las heterótrofas en cultivo sin limitación" * doctest::test_suite("validacion")) {
    for (const SpeciesId id : {SpeciesId::Paramecium, SpeciesId::Amoeba}) {
        const SourcedValue& mu = speciesPhysiology(id).growthMax;
        REQUIRE(mu.present);
        const float hours = 2.0f * std::log(2.0f) / mu.value; // dos duplicaciones
        const double sim = cultureGrowthRate(id, hours);
        MESSAGE(std::string(speciesInfo(id).name) << ": µ simulado " << sim << " /h, publicado " << mu.value << " /h a 20 °C ("
                                     << mu.source << ")");
        CHECK(sim == doctest::Approx(mu.value).epsilon(0.25));
    }
}
