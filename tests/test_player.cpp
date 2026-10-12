#include "bio/feeding.hpp"
#include "bio/locomotion.hpp"
#include "game/world.hpp"
#include "sim/soft_body.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig quietWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.seed = 21;
    return config;
}

int membraneCrossings(const RingBody& body, const BodyParticles& bp) {
    auto p = [&](std::size_t i) {
        const std::uint32_t s = body.ring[i % body.ring.size()];
        return Vec2{bp.x[s], bp.y[s]};
    };
    auto crosses = [](Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
        return cross(b - a, c - a) * cross(b - a, d - a) < 0.0f &&
               cross(d - c, a - c) * cross(d - c, b - c) < 0.0f;
    };
    int count = 0;
    const std::size_t n = body.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j)
            if (!(i == 0 && j == n - 1) && crosses(p(i), p(i + 1), p(j), p(j + 1)))
                ++count;
    return count;
}

} // namespace

TEST_CASE("bajo Reynolds: el ciliado nada a su velocidad y frena en el acto") {
    World world(quietWorld());
    const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f});
    const RingBody& body = world.registry().get<RingBody>(e);
    PlayerInput input;
    input.forward = 1.0f;
    for (int s = 0; s < 30; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    const Vec2 a = body.centroid;
    for (int s = 0; s < 60; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    const float swim = length(body.centroid - a); // en 1 s
    world.setPlayerInput({});
    world.step(kDt);
    const Vec2 b = body.centroid;
    world.step(kDt);
    const float coast = length(body.centroid - b) / kDt;

    MESSAGE("nado " << swim << " µm/s, inercia al soltar " << coast << " µm/s");
    CHECK(swim == doctest::Approx(world.locomotionParams().ciliaSpeed).epsilon(0.1));
    CHECK(coast < 0.05f * swim);
}

TEST_CASE("el flagelado gira y nada hacia donde apunta") {
    World world(quietWorld());
    const entt::entity e = world.spawnPlayer(LocomotionType::Flagellum, {1024.0f, 512.0f});
    const RingBody& body = world.registry().get<RingBody>(e);
    PlayerInput input;
    input.propel = true;
    input.aim = {1024.0f, 900.0f}; // 90° respecto de su orientación inicial
    for (int s = 0; s < 300; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    CHECK(body.centroid.y > 800.0f);
    CHECK(std::abs(body.centroid.x - 1024.0f) < 40.0f);
}

TEST_CASE("el ciliado come bacterias por la boca respetando el caudal del citostoma") {
    World world(quietWorld(50'000));
    const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f});
    PlayerInput input;
    input.forward = 0.3f;
    input.turn = 0.5f;
    for (int s = 0; s < 600; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    const Digestion& dig = world.registry().get<Digestion>(e);
    MESSAGE(dig.bacteriaEaten << " bacterias en 10 s, " << dig.vacuoles.size() << " vacuolas");
    CHECK(dig.bacteriaEaten > 50);
    CHECK(dig.bacteriaEaten <= static_cast<int>(10.0f * world.feedingParams().mouthMaxRate) + 31);
    CHECK_FALSE(dig.vacuoles.empty());
}

TEST_CASE("la ameba envuelve una célula y la encierra en un fagosoma") {
    World world(quietWorld());
    const entt::entity e = world.spawnPlayer(LocomotionType::Pseudopod, {900.0f, 512.0f});
    CellSpec prey;
    prey.center = {1015.0f, 512.0f};
    prey.radiusX = 22.0f;
    prey.radiusY = 18.0f;
    const entt::entity preyEntity = world.spawnCell(prey);

    float when = -1.0f;
    for (int s = 0; s < 60 * 40 && when < 0.0f; ++s) {
        world.step(kDt);
        if (world.registry().get<Digestion>(e).cellsEaten > 0)
            when = world.simTime();
    }
    REQUIRE(when > 0.0f);
    CHECK_FALSE(world.registry().valid(preyEntity));

    const RingBody& body = world.registry().get<RingBody>(e);
    const std::size_t ringAfter = body.ring.size();
    for (int s = 0; s < 600; ++s) // 10 s más: la membrana fusionada tiene que quedar estable
        world.step(kDt);
    const Digestion& dig = world.registry().get<Digestion>(e);
    MESSAGE("fagocitada a los " << when << " s; anillo " << ringAfter << " -> " << body.ring.size());
    CHECK(membraneCrossings(body, world.bodies()) == 0);
    CHECK(body.ring.size() < 2 * ringAfter);
    CHECK(dig.vacuoles.size() == 1);
    CHECK(std::abs(soft_body::area(body, world.bodies()) - body.restArea) < 0.05f * std::abs(body.restArea));
}

namespace {
std::uint64_t runPlayer(int threads) {
    omp_set_num_threads(threads);
    World world(quietWorld(3000));
    world.spawnPlayer(LocomotionType::Pseudopod, {900.0f, 512.0f});
    CellSpec prey;
    prey.center = {1015.0f, 512.0f};
    prey.radiusX = 22.0f;
    prey.radiusY = 18.0f;
    world.spawnCell(prey);
    PlayerInput input;
    input.pseudopod = true;
    input.aim = {1100.0f, 600.0f};
    for (int s = 0; s < 240; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    return world.stateHash();
}
} // namespace

TEST_CASE("jugador y fagocitosis deterministas sin importar la cantidad de hilos") {
    const std::uint64_t a = runPlayer(1);
    CHECK(a == runPlayer(1));
    CHECK(a == runPlayer(4));
}
