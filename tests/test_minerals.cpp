// Sedimento inorgánico: partículas minerales y granos de arena.
#include "game/biome.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig emptyWorld() {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.saprolegnia = config.chytridClusters = 0;
    config.seed = 77;
    return config;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

int countMinerals(const World& world, MineralKind kind) {
    const AmbientParticles& a = world.ambient();
    int n = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        n += a.kind[i] == ParticleKind::Mineral && a.mineral[i] == kind ? 1 : 0;
    return n;
}

std::uint64_t runSediment(int threads) {
    omp_set_num_threads(threads);
    WorldConfig config = emptyWorld();
    config.sediment = {};
    config.sediment[MineralKind::Clay] = 80;
    config.sediment[MineralKind::Quartz] = 30;
    config.sediment[MineralKind::Mica] = 5;
    config.sediment[MineralKind::IronOxide] = 10;
    config.sediment[MineralKind::Sulfur] = 5;
    config.sediment[MineralKind::Frustule] = 5;
    config.sediment.rocks = 4;
    World world(config);
    world.populateEcosystem(0.2f);
    steps(world, 60 * 3);
    return world.stateHash();
}

} // namespace

TEST_CASE("minerales: inertes; a las asentadas no las lleva el agua, a la arcilla sí") {
    WorldConfig config = emptyWorld();
    config.sediment = {};
    config.sediment[MineralKind::Clay] = 60;
    config.sediment[MineralKind::Quartz] = 30;
    World world(config);
    const AmbientParticles& a = world.ambient();
    REQUIRE(a.size() == 90);
    const std::vector<float> x0 = a.x, y0 = a.y;
    std::vector<MineralKind> kind0 = a.mineral;
    const Vec2 c = world.dish().center;
    world.setStir(c, {150.0f, 0.0f}, 600.0f); // corriente fuerte en toda la placa
    steps(world, 120);
    REQUIRE(a.size() == 90); // no se descomponen
    float settledMove = 0.0f, clayMove = 0.0f;
    int clay = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a.kind[i] == ParticleKind::Mineral);
        CHECK(a.mass[i] == 0.0f);
        const float d = std::hypot(a.x[i] - x0[i], a.y[i] - y0[i]);
        if (a.radius[i] >= 2.0f) {
            settledMove = std::max(settledMove, d);
        } else {
            clayMove += d;
            ++clay;
        }
    }
    MESSAGE("asentadas: " << settledMove << " µm | arcilla: " << clayMove / std::max(1, clay) << " µm en promedio");
    CHECK(settledMove < 1e-4f);
    CHECK(clay > 0);
    CHECK(clayMove / static_cast<float>(clay) > 20.0f);
}

TEST_CASE("un grano de arena es inamovible: la célula no lo atraviesa y el agua no fluye adentro") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    const float biomass = world.census().totalBiomass;
    Rng rng(1, 2, 3, 4);
    REQUIRE(world.spawnRock(c, 60.0f, MineralKind::Quartz, rng));
    CHECK(world.rockCount() == 1);
    CHECK(world.census().totalBiomass == doctest::Approx(biomass)); // no es biomasa
    CHECK_FALSE(world.spawnRock(c + Vec2{30.0f, 0.0f}, 40.0f, MineralKind::Quartz, rng)); // no se superponen

    entt::entity rock = entt::null;
    for (auto e : world.registry().view<Rock>())
        rock = e;
    const RingBody& rb = world.registry().get<RingBody>(rock);
    const BodyParticles& bp = world.bodies();
    std::vector<Vec2> before;
    for (std::uint32_t s : rb.ring)
        before.push_back({bp.x[s], bp.y[s]});

    // Un paramecio nada de frente contra el grano.
    const entt::entity cell = world.spawnOrganism(SpeciesId::Paramecium, c + Vec2{250.0f, 0.0f},
                                                  speciesGenome(SpeciesId::Paramecium), 0, 3.14159265f);
    world.setPlayer(cell);
    PlayerInput input;
    input.forward = 1.0f;
    world.setPlayerInput(input);
    int inside = 0;
    float closest = 1e9f;
    for (int s = 0; s < 60 * 5; ++s) {
        if (s == 60 * 4) // al final, el agua se revuelve encima del grano
            world.setStir(c, {80.0f, 40.0f}, 150.0f);
        world.step(kDt);
        const RingBody& cb = world.registry().get<RingBody>(cell);
        const int pn = static_cast<int>(rb.poly.size());
        closest = std::min(closest, std::sqrt(polygon::closestOnBoundary(rb.poly.data(), pn, cb.poly[0]).dist2));
        for (const Vec2& q : cb.poly)
            closest = std::min(closest, std::sqrt(polygon::closestOnBoundary(rb.poly.data(), pn, q).dist2));
        for (std::uint32_t m : cb.ring)
            inside += polygon::contains(rb.poly.data(), static_cast<int>(rb.poly.size()), {bp.x[m], bp.y[m]}) ? 1 : 0;
    }
    float moved = 0.0f;
    for (std::size_t k = 0; k < rb.ring.size(); ++k)
        moved = std::max(moved, length(Vec2{bp.x[rb.ring[k]], bp.y[rb.ring[k]]} - before[k]));
    const float flow = length(world.fluid().velocityAt(c));
    MESSAGE("grano movido " << moved << " µm | puntos adentro " << inside << " | agua en el centro " << flow
                            << " µm/s | la célula llegó a " << closest << " µm");
    CHECK(moved < 1e-4f);
    CHECK(inside == 0);
    CHECK(flow < 1e-3f);
    CHECK(closest < 5.0f); // la célula llegó hasta el grano (si no, el test no prueba nada)

    world.removeCellAt(c); // la herramienta de borrar también saca granos
    CHECK(world.rockCount() == 0);
}

TEST_CASE("Navicula muerta deja su frústula vacía en el sedimento") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    const entt::entity e = world.spawnOrganism(SpeciesId::Navicula, c, speciesGenome(SpeciesId::Navicula), 0, 0.5f);
    CHECK(countMinerals(world, MineralKind::Frustule) == 0);
    world.lyseAt(world.registry().get<RingBody>(e).centroid);
    CHECK_FALSE(world.registry().valid(e));
    CHECK(countMinerals(world, MineralKind::Frustule) == 1);
}

TEST_CASE("las gotas naturales traen sedimento según el bioma") {
    const World puddle(makeDrop(BiomeId::Puddle, 5));
    CHECK(puddle.rockCount() > 0);
    CHECK(countMinerals(puddle, MineralKind::Clay) > 100);
    const World spring(makeDrop(BiomeId::HotSpring, 5));
    CHECK(countMinerals(spring, MineralKind::Sulfur) > 0);
    const World gut(makeDrop(BiomeId::TermiteGut, 5));
    CHECK(gut.rockCount() == 0);
}

TEST_CASE("sedimento y granos deterministas sin importar la cantidad de hilos" * doctest::test_suite("largos")) {
    const std::uint64_t a = runSediment(1);
    CHECK(a == runSediment(4));
}
