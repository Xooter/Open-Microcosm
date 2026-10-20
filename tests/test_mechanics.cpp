// Mecánicas de los seres (docs/plan_mecanicas.md): locomoción, reacciones y defensas que se eligen por
// datos. Casos cortos (segundos de simulación) en una placa sin bacterias.
#include "ai/autopilot.hpp"
#include "bio/species.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;

World emptyWorld(std::uint64_t seed = 7) {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.seed = seed;
    return World(config);
}

entt::entity spawn(World& world, const char* key, Vec2 at) {
    const auto id = findSpecies(key);
    REQUIRE(id);
    const entt::entity e = world.spawnOrganism(*id, at, speciesGenome(*id), 0);
    world.registry().emplace<Autopilot>(e); // como las silvestres: se mueve sola
    return e;
}

Vec2 center(const World& world) { return {0.5f * world.width(), 0.5f * world.height()}; }

// Velocidad lateral media cuadrática [µm/s]: la componente de la velocidad del centroide perpendicular
// al eje del cuerpo, en `seconds`.
float lateralSpeed(World& world, entt::entity e, float seconds) {
    double sum = 0.0;
    int n = 0;
    Vec2 prev = world.registry().get<RingBody>(e).centroid;
    for (int s = 0; s < static_cast<int>(seconds / kDt); ++s) {
        world.step(kDt);
        const RingBody& b = world.registry().get<RingBody>(e);
        const Vec2 v = (b.centroid - prev) * (1.0f / kDt);
        prev = b.centroid;
        if (s < 30)
            continue; // el primer medio segundo la célula se acomoda
        const float lateral = cross(bodyHeading(b), v);
        sum += lateral * lateral;
        ++n;
    }
    return static_cast<float>(std::sqrt(sum / std::max(1, n)));
}

} // namespace

TEST_CASE("mecánicas: Peridinium avanza en hélice (la velocidad oscila a los lados del eje)") {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "peridinium", center(world));
    // Hélice de 8 µm a 1 vuelta/s: la velocidad lateral oscila con amplitud 2π·8 ≈ 50 µm/s (RMS ≈ 35).
    CHECK(lateralSpeed(world, e, 3.0f) > 25.0f);

    // La Euglena, con el bamboleo genérico (15 %), se aparta mucho menos.
    World other = emptyWorld();
    const entt::entity eu = spawn(other, "euglena", center(other));
    CHECK(lateralSpeed(other, eu, 3.0f) < 20.0f);
}

TEST_CASE("mecánicas: Peranema se desliza recto sobre el vidrio y el agua no lo arrastra") {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "peranema", center(world));
    CHECK(world.registry().get<Locomotion>(e).gliding);
    CHECK(lateralSpeed(world, e, 2.0f) < 3.0f);

    // Una corriente fuerte (Revolver): la Euglena que nada se va con el agua; Peranema, apoyado, no.
    auto drift = [](const char* key) {
        World w = emptyWorld(11);
        const Vec2 c = center(w);
        const entt::entity cell = spawn(w, key, c);
        const float x0 = w.registry().get<RingBody>(cell).centroid.y;
        for (int s = 0; s < 60; ++s) {
            w.setStir(w.registry().get<RingBody>(cell).centroid, {0.0f, 300.0f}, 120.0f);
            w.step(kDt);
        }
        return w.registry().get<RingBody>(cell).centroid.y - x0;
    };
    const float swimmer = drift("euglena"), glider = drift("peranema");
    CHECK(swimmer > 60.0f);
    CHECK(std::abs(glider) < 0.3f * swimmer);
}

