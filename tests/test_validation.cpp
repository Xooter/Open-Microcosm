// Validación contra resultados publicados (MVP). Suite "validacion": ctest -L validacion.
//
// Cada test reproduce un experimento real y compara con la fuente ([physiology.<id>.*] de cada ser,
// docs/validacion.md). Son largos: la biología se acelera con la escala de tiempo biológico (600),
// que no cambia las proporciones entre procesos.
#include "bio/physiology.hpp"
#include "game/intervention.hpp"
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
    // Placa de ~860 µm: en una de 350 µm, una decena de paramecios de ~180 µm terminaba aplastada
    // contra el vidrio (ver docs/validacion.md).
    c.gridX = c.gridY = 128;
    c.bacteria = bacteria;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.medium.temperature = 20.0f; // la referencia de la fisiología: sin corrección Q10
    c.medium.detritusSpawnRate = 0.0f;
    // Sin fagos, como un cultivo de laboratorio para medir µmax: con fagos líticos, una población de
    // ~20 000 bacterias (sin resistentes a la frecuencia real de mutación) se extingue en horas y
    // el cultivo deja de ser "sin limitación".
    c.medium.phage = 0.0f;
    // Ni Bdellovibrio: en una placa de 0,86 mm, nadando a 100 µm/s, encuentran a todas las
    // bacterias y las extinguen (docs/validacion.md).
    c.bdellovibrioFraction = 0.0f;
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
    for (int k = 0; k < 2; ++k)
        world.spawnBeing({BeingKind::Species, id}, c + Vec2{(k ? 1.0f : -1.0f) * 0.3f * world.dish().radius, 0.0f});
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
    // Amoeba proteus queda afuera: su µmax publicado (Rogerson 1980) es con Tetrahymena como
    // presa, cientos por división; en una placa de 0,5 mm no hay presas protistas suficientes para
    // un cultivo sin limitación (ver docs/validacion.md).
    for (const SpeciesId id : {SpeciesId::Paramecium}) {
        const SourcedValue& mu = speciesPhysiology(id).growthMax;
        REQUIRE(mu.present);
        const float hours = 2.0f * std::log(2.0f) / mu.value; // dos duplicaciones
        const double sim = cultureGrowthRate(id, hours);
        MESSAGE(std::string(speciesInfo(id).name) << ": µ simulado " << sim << " /h, publicado " << mu.value << " /h a 20 °C ("
                                     << mu.source << ")");
        // Error relativo explícito: Approx suma una escala absoluta de 1 y con tasas ~0,01/h
        // toleraría cualquier cosa.
        CHECK(std::abs(sim - mu.value) / mu.value < 0.25);
    }
}

namespace {

// Tilman (1977): Asterionella y Cyclotella juntas en cultivo en lote con luz. Devuelve la fracción
// de la biomasa de Asterionella al agotarse el recurso limitante. Columna de agua de 2 cm
// (gapHeight): en una película de 0,1 mm toda la placa tiene menos fósforo que una sola célula
// (cuota ~0,04 pmol; 10⁴–10⁵ células/mL en un cultivo real).
double tilmanAsterionellaShare(bool siLimited) {
    WorldConfig c;
    c.gridX = c.gridY = 128;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.medium.temperature = 20.0f;
    c.medium.detritusSpawnRate = 0.0f;
    c.medium.phage = 0.0f;
    c.bdellovibrioFraction = 0.0f;
    c.seed = 1977;
    c.medium.phosphate = siLimited ? 10.0f : 0.5f;
    c.medium.silicate = siLimited ? 2.0f : 100.0f;
    World world(c);
    world.bioTimeScale() = 3000.0f;
    world.fluid().gapHeight = 20000.0f;
    const Vec2 ctr = {world.width() * 0.5f, world.height() * 0.5f};
    for (int k = 0; k < 16; ++k) {
        const float a = 2.4f * k, r = world.dish().radius * (0.15f + 0.6f * std::fmod(0.618f * k, 1.0f));
        world.spawnBeing({BeingKind::Species, k % 2 ? SpeciesId::Cyclotella : SpeciesId::Asterionella},
                         ctr + Vec2{r * std::cos(a), r * std::sin(a)});
    }
    ChemField& limiting = siLimited ? world.silicateField() : world.phosphateField();
    const float start = world.meanConcentration(limiting);
    // Hasta que el recurso limitante cae al 5 % (o 150 h biológicas).
    while (world.meanConcentration(limiting) > 0.05f * start && world.simTime() * world.bioScale() / 3600.0f < 150.0f)
        world.step(kDt);
    double ast = 0.0, cyc = 0.0;
    for (const auto [e, m, sp] : world.registry().view<const Metabolism, const Species>().each()) {
        ast += sp.id == SpeciesId::Asterionella ? m.biomass : 0.0;
        cyc += sp.id == SpeciesId::Cyclotella ? m.biomass : 0.0;
    }
    return ast / (ast + cyc);
}

} // namespace

// Resultado cualitativo confirmado de Tilman (1977, Ecology 58: 338-348): con P limitante gana
// Asterionella y con Si limitante, Cyclotella. Las constantes de [physiology.<id>.*] están SIN
// VERIFICAR contra Tilman & Kilham (1976). Dos casos: corren en paralelo con ctest -j.
TEST_CASE("validación: Tilman — con P limitante gana Asterionella" * doctest::test_suite("validacion")) {
    const double share = tilmanAsterionellaShare(false);
    MESSAGE("fracción de Asterionella con P limitante: " << share);
    CHECK(share > 0.5);
}

TEST_CASE("validación: Tilman — con Si limitante gana Cyclotella" * doctest::test_suite("validacion")) {
    const double share = tilmanAsterionellaShare(true);
    MESSAGE("fracción de Asterionella con Si limitante: " << share);
    CHECK(share < 0.5);
}

namespace {

// Jennings (1906): paramecios en una placa con una fuente de CO₂ en el centro (la burbuja de su
// experimento: el gotero de CO₂ abierto). Devuelve la fracción media de paramecios a menos de
// 0,2 radios del centro (4 % del área), en los últimos 2/3 de la corrida.
double jenningsFractionNearSource(bool carbonDioxide) {
    WorldConfig c = cultureConfig(0);
    c.gridX = c.gridY = 256; // ~1,9 mm: 20 paramecios de ~200 µm sin amontonarse
    c.seed = 1906;
    World world(c);
    InterventionLog log; // el mismo camino que el gotero de la interfaz
    const Vec2 ctr = {world.width() * 0.5f, world.height() * 0.5f};
    const float radius = world.dish().radius, zone = 0.2f * radius;
    for (int k = 0; k < 20; ++k) { // repartidos lejos de la fuente
        const float a = 2.39996f * static_cast<float>(k);
        const float r = radius * (0.45f + 0.4f * std::fmod(0.618f * static_cast<float>(k), 1.0f));
        world.spawnBeing({BeingKind::Species, SpeciesId::Paramecium}, ctr + Vec2{r * std::cos(a), r * std::sin(a)});
    }
    if (carbonDioxide) {
        Intervention drop;
        drop.kind = InterventionKind::Dropper;
        drop.active = true;
        drop.pos = ctr;
        drop.index = 6;          // CO₂ disuelto
        drop.v[0] = 0.15f * radius; // radio [µm]
        drop.v[1] = 2.0f;        // 2 mM/s en la gota: ~10 mM en el centro, pH ~5,3
        log.recordState(world, drop);
    }
    const int steps = static_cast<int>(120.0f / kDt);
    double inside = 0.0, samples = 0.0;
    for (int s = 0; s < steps; ++s) {
        log.beforeStep(world, kDt);
        world.step(kDt);
        if (s % 60 != 0 || s < steps / 3)
            continue;
        for (const auto [e, body, sp] : world.registry().view<const RingBody, const Species>().each()) {
            if (sp.id != SpeciesId::Paramecium)
                continue;
            samples += 1.0;
            inside += length(body.centroid - ctr) < zone ? 1.0 : 0.0;
        }
    }
    return inside / samples;
}

} // namespace

TEST_CASE("validación: Jennings — los paramecios se acumulan en el agua con CO₂" * doctest::test_suite("validacion")) {
    // Jennings, H. S. (1906). Behavior of the Lower Organisms, cap. sobre Paramecium: entran libremente
    // a una zona con CO₂ y al salir hacia agua menos ácida hacen la reacción de evitación. Resultado
    // cualitativo (no hay conteos publicados): la densidad crece hacia la fuente. Simplificación: con
    // ácido más fuerte Jennings vio un anillo alrededor de la burbuja; acá no se modela la evitación
    // del ácido fuerte.
    const double control = jenningsFractionNearSource(false);
    const double withGas = jenningsFractionNearSource(true);
    MESSAGE("fracción de paramecios a < 0,2 radios (4 % del área): con CO₂ " << withGas << ", sin CO₂ " << control);
    CHECK(withGas > 1.5 * 0.04);
    CHECK(withGas > 2.0 * control);
}
