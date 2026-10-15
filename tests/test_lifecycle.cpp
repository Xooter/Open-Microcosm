#include "bio/division.hpp"
#include "bio/metabolism.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig emptyWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.seed = 33;
    return config;
}

// Lleva la biomasa al doble y da reservas: la célula está lista para dividirse.
void feedToDivision(World& world, entt::entity e) {
    Metabolism& m = world.registry().get<Metabolism>(e);
    m.biomass = 2.05f * m.birthBiomass;
    m.reserves = 0.6f * m.biomass;
}

bool simplePolygon(const RingBody& b, const BodyParticles& bp) {
    auto p = [&](std::size_t i) {
        const std::uint32_t s = b.ring[i % b.ring.size()];
        return Vec2{bp.x[s], bp.y[s]};
    };
    const std::size_t n = b.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1)
                continue;
            const Vec2 a = p(i), c = p(i + 1), d = p(j), e = p(j + 1);
            if (cross(c - a, d - a) * cross(c - a, e - a) < 0.0f && cross(e - d, a - d) * cross(e - d, c - d) < 0.0f)
                return false;
        }
    return true;
}

} // namespace

namespace {
// Mitosis física de un tipo de célula (un caso por tipo: corren en paralelo con ctest -j).
void dividesIntoTwo(LocomotionType type) {
    CAPTURE(locomotionName(type));
    World world(emptyWorld());
    const entt::entity e = world.spawnPlayer(type, {1024.0f, 512.0f});
    feedToDivision(world, e);
    const float before = world.registry().get<Metabolism>(e).biomass;
    for (int s = 0; s < 60 * 90 && world.registry().view<Metabolism>().size() < 2; ++s)
        world.step(kDt);
    REQUIRE(world.registry().view<Metabolism>().size() == 2);

    float total = 0.0f;
    for (auto [c, body, meta, anatomy] : world.registry().view<RingBody, Metabolism, CellAnatomy>().each()) {
        total += meta.biomass;
        CHECK(simplePolygon(body, world.bodies()));
        CHECK(meta.biomass == doctest::Approx(meta.birthBiomass));
        // Cada hija tiene su propio núcleo, dentro de su membrana.
        const Vec2 nucleus = {world.bodies().x[anatomy.nucleus], world.bodies().y[anatomy.nucleus]};
        CHECK(polygon::contains(body.poly.data(), static_cast<int>(body.poly.size()), nucleus));
        CHECK_FALSE(world.registry().all_of<Dividing>(c));
    }
    // La división cuesta reservas, no biomasa: no se pierde biomasa (y sigue creciendo
    // mientras se divide, porque tiene reservas).
    CHECK(total >= 0.99f * before);
    CHECK(total <= 1.25f * before);
    CHECK(world.registry().valid(world.player()));
}
} // namespace

TEST_CASE("mitosis física: el ciliado se divide en dos hijas válidas" * doctest::test_suite("largos")) {
    dividesIntoTwo(LocomotionType::Cilia);
}
TEST_CASE("mitosis física: el flagelado se divide en dos hijas válidas" * doctest::test_suite("largos")) {
    dividesIntoTwo(LocomotionType::Flagellum);
}
TEST_CASE("mitosis física: la ameba se divide en dos hijas válidas" * doctest::test_suite("largos")) {
    dividesIntoTwo(LocomotionType::Pseudopod);
}

TEST_CASE("sin comida la célula se consume, muere y se lisa en detritos y nutrientes" * doctest::test_suite("largos")) {
    World world(emptyWorld());
    world.bioTimeScale() = 3600.0f; // 1 s real = 1 h biológica
    world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f});
    const double nutrientsBefore = world.nutrients().grid().sum();
    int s = 0;
    for (; s < 60 * 300 && world.registry().valid(world.player()); ++s)
        world.step(kDt);
    MESSAGE("murió a las " << s * kDt << " h biológicas");
    CHECK_FALSE(world.registry().valid(world.player()));
    CHECK(world.ambient().size() >= 4); // detritos de la lisis
    CHECK(world.nutrients().grid().sum() > nutrientsBefore);
}

TEST_CASE("la Euglena crece con luz y no en la oscuridad" * doctest::test_suite("largos")) {
    auto grow = [](float light) {
        World world(emptyWorld());
        world.medium().surfaceLight = light;
        world.bioTimeScale() = 600.0f;
        const entt::entity e = world.spawnPlayer(LocomotionType::Flagellum, {1024.0f, 512.0f});
        const Metabolism& m = world.registry().get<Metabolism>(e);
        const float start = m.biomass + m.reserves;
        for (int s = 0; s < 60 * 60; ++s) // 10 h biológicas
            world.step(kDt);
        return (m.biomass + m.reserves) / start;
    };
    const float lit = grow(1.0f), dark = grow(0.0f);
    MESSAGE("luz: x" << lit << ", oscuridad: x" << dark);
    CHECK(lit > 1.2f);
    CHECK(dark < 1.0f);
}

TEST_CASE("sin O2 la célula fermenta y gasta sus reservas mucho más rápido" * doctest::test_suite("largos")) {
    auto spent = [](bool anoxic) {
        World world(emptyWorld());
        world.bioTimeScale() = 600.0f;
        if (anoxic) {
            world.medium().o2AirExchange = 0.0f;
            for (float& v : world.oxygen().grid().v)
                v = 0.0f;
        }
        const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f});
        Metabolism& m = world.registry().get<Metabolism>(e);
        m.reserves = 10.0f * m.biomass; // sin crecer: solo se mide el gasto
        world.metabolismParams().growthPerHourMax = 0.0f;
        const float start = m.reserves;
        for (int s = 0; s < 600; ++s)
            world.step(kDt);
        return start - m.reserves;
    };
    const float aerobic = spent(false), anaerobic = spent(true);
    MESSAGE("gasto aeróbico " << aerobic << " pmol, anaeróbico " << anaerobic << " pmol");
    CHECK(anaerobic > 10.0f * aerobic);
}

TEST_CASE("comer hace crecer: el ciliado que se alimenta gana biomasa frente al que ayuna" * doctest::test_suite("largos")) {
    auto run = [](std::size_t bacteria) {
        World world(emptyWorld(bacteria));
        const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f});
        PlayerInput input;
        input.forward = 0.3f;
        input.turn = 0.4f;
        for (int s = 0; s < 60 * 60; ++s) { // 1 h biológica
            world.setPlayerInput(input);
            world.step(kDt);
        }
        const Metabolism& m = world.registry().get<Metabolism>(e);
        return std::pair{m.biomass / m.birthBiomass, m.reserves};
    };
    const auto [fedGrowth, fedReserves] = run(50'000);
    const auto [hungryGrowth, hungryReserves] = run(0);
    MESSAGE("alimentado: biomasa x" << fedGrowth << ", reservas " << fedReserves
                                    << " | en ayunas: x" << hungryGrowth << ", reservas " << hungryReserves);
    // Con el µmax real de P. caudatum (~0,026/h a 20 °C, Gause 1934) la biomasa crece ~3 % en 1 h
    // biológica: alimentada o no (la que ayuna usa sus reservas). Lo que distingue a la alimentada
    // son las reservas.
    CHECK(fedGrowth > 1.0f);
    CHECK(fedGrowth >= hungryGrowth);
    CHECK(fedReserves > hungryReserves);
}

namespace {
std::uint64_t runLife(int threads) {
    omp_set_num_threads(threads);
    World world(emptyWorld(2000));
    const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f});
    feedToDivision(world, e);
    for (int s = 0; s < 60 * 35; ++s)
        world.step(kDt);
    return world.stateHash();
}
} // namespace

TEST_CASE("metabolismo y división deterministas sin importar la cantidad de hilos" * doctest::test_suite("largos")) {
    const std::uint64_t a = runLife(1);
    CHECK(a == runLife(4));
}
