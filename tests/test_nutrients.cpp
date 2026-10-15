// Sílice, hierro y vitamina B₁₂: limitan el crecimiento y se conservan.
#include "bio/metabolism.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

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
    config.seed = 77;
    return config;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

// Biomasa total de las diatomeas tras `n` pasos con las condiciones dadas.
float growth(const WorldConfig& config, int n) {
    World world(config);
    world.bioTimeScale() = 3600.0f; // 1 s = 1 h biológica
    const entt::entity e = world.spawnOrganism(SpeciesId::Navicula, world.dish().center, speciesGenome(SpeciesId::Navicula), 0);
    world.medium().surfaceLight = 1.0f;
    const float before = world.registry().get<Metabolism>(e).biomass;
    world.registry().get<Metabolism>(e).reserves = 2.0f * before; // energía de sobra: limita el nutriente
    steps(world, n);
    float total = 0.0f;
    for (auto [c, m] : world.registry().view<Metabolism>().each())
        total += m.biomass;
    return total / before;
}

} // namespace

TEST_CASE("sin sílice la diatomea no crece; con sílice sí" * doctest::test_suite("largos")) {
    WorldConfig rich = quietWorld(), poor = quietWorld();
    poor.medium.silicate = 0.0f;
    const float withSi = growth(rich, 60 * 8);
    const float withoutSi = growth(poor, 60 * 8);
    MESSAGE("biomasa x" << withSi << " con Si, x" << withoutSi << " sin Si");
    CHECK(withSi > 1.2f);
    CHECK(withoutSi < 1.05f);
}

TEST_CASE("sin hierro las fotótrofas no crecen") {
    WorldConfig poor = quietWorld();
    poor.medium.iron = 0.0f;
    CHECK(growth(poor, 60 * 8) < 1.05f);
}

TEST_CASE("la auxótrofa sin B12 no crece") {
    WorldConfig poor = quietWorld();
    poor.medium.vitamin = 0.0f;
    CHECK(growth(poor, 60 * 8) < 1.05f);
}

TEST_CASE("el sílice y el hierro se conservan con crecimiento, división y muerte") {
    World world(quietWorld());
    const Vec2 c = world.dish().center;
    for (int k = 0; k < 4; ++k)
        world.spawnOrganism(SpeciesId::Navicula, c + Vec2{40.0f * static_cast<float>(k), 0.0f}, speciesGenome(SpeciesId::Navicula), 0);
    world.bioTimeScale() = 3600.0f;
    const double si = world.totalSilicon(), fe = world.totalIron();
    for (auto [e, m] : world.registry().view<Metabolism>().each())
        m.reserves = 2.0f * m.biomass;
    steps(world, 60 * 8);
    for (auto e : world.registry().view<Metabolism>())
        world.forceReproduction(e);
    steps(world, 60 * 4);
    CHECK(world.totalSilicon() == doctest::Approx(si).epsilon(0.01));
    CHECK(world.totalIron() == doctest::Approx(fe).epsilon(0.01));
}

TEST_CASE("la frústula de una diatomea muerta guarda su sílice y la devuelve al disolverse") {
    World world(quietWorld());
    const entt::entity e = world.spawnOrganism(SpeciesId::Navicula, world.dish().center, speciesGenome(SpeciesId::Navicula), 0);
    const double si = world.totalSilicon();
    const float dissolved = world.silicate().sum();
    world.lyseAt(world.registry().get<RingBody>(e).centroid);
    CHECK(world.totalSilicon() == doctest::Approx(si).epsilon(0.001));
    CHECK(world.silicate().sum() == doctest::Approx(dissolved).epsilon(1e-3)); // el Si sigue en la frústula
    world.bioTimeScale() = 36000.0f;
    steps(world, 60 * 8);
    CHECK(world.silicate().sum() > dissolved);
    CHECK(world.totalSilicon() == doctest::Approx(si).epsilon(0.001));
}

TEST_CASE("fijar la concentración media: exacta, solo en el agua y estable con el agua casi agotada") {
    World world(quietWorld());
    ChemField& p = world.phosphateField();
    // Agua casi agotada con los rizos negativos que deja la difusión espectral junto a un pozo de
    // consumo: antes, objetivo / media ~ 10⁶ escalaba también los negativos y el campo explotaba.
    std::vector<float>& v = p.grid().v;
    for (std::size_t k = 0; k < v.size(); ++k)
        if (world.dishMask().isWater(k))
            v[k] = (k % 7 == 0) ? -2e-7f : 1e-7f;
    world.setMeanConcentration(p, 0.0025f);
    CHECK(world.meanConcentration(p) == doctest::Approx(0.0025).epsilon(1e-4));
    int negative = 0; // en el agua (el vidrio guarda fantasmas y no se toca)
    for (std::size_t k = 0; k < v.size(); ++k)
        negative += world.dishMask().isWater(k) && v[k] < 0.0f;
    CHECK(negative == 0);
    // Subir con distribución: el doble en todas partes; la media exacta (antes el vidrio sumaba ~1 %).
    for (int s = 0; s < 30; ++s)
        world.step(1.0f / 60.0f);
    const float before = world.meanConcentration(p);
    world.setMeanConcentration(p, 2.0f * before);
    CHECK(world.meanConcentration(p) == doctest::Approx(2.0f * before).epsilon(1e-5));
}

TEST_CASE("Droop: en equilibrio el crecimiento sigue la Monod publicada (P limitante)") {
    // Una célula con P externo fijo y todo lo demás de sobra, hasta que su cuota se equilibra
    // (quimiostato ideal). Con Droop y la captación ligada a µmax, µ(S) = µmax·S/(K+S) con K la
    // media saturación de CRECIMIENTO (Tilman y Kilham 1976 publican esa).
    const MetabolismParams p;
    const float k = 0.04f, muMax = p.growthPerHourMax;
    for (const float s : {0.25f * k, k, 4.0f * k, 16.0f * k}) {
        Metabolism m;
        m.photosynthetic = true;
        m.biomass = 1.0f;
        m.phosphorus = p.phosphorusRatio; // cuota mínima: empieza limitada
        m.nitrogen = p.luxuryQuota * p.nitrogenRatio;
        m.iron = p.luxuryQuota * p.ironRatio;
        MetabolicInput in{};
        in.o2 = 250.0f;
        in.bioDt = 60.0f;
        in.phosphorusHalfSaturation = k;
        in.phosphate = s;
        in.nitrogen = in.iron = 1e3f; // de sobra
        float mu = 0.0f;
        for (int step = 0; step < 60 * 400; ++step) { // 400 h biológicas
            in.digested = m.biomass; // energía de sobra
            const float before = m.biomass;
            updateMetabolism(m, p, in);
            mu = std::log(m.biomass / before) / (in.bioDt / 3600.0f);
            // Renormaliza (la biomasa crecería sin límite): misma composición, otra escala.
            const float r = 1.0f / m.biomass;
            m.biomass = 1.0f;
            m.reserves *= r;
            m.phosphorus *= r;
            m.nitrogen *= r;
            m.iron *= r;
        }
        CAPTURE(s);
        const float monod = muMax * s / (k + s);
        CHECK(std::abs(mu - monod) < 0.03f * monod);
    }
}
