// Nematodo: nado ondulatorio por fuerzas resistivas.
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <doctest/doctest.h>

#include <cmath>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig emptyWorld() {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.saprolegnia = config.zoophagus = config.chytridClusters = 0;
    config.seed = 123;
    return config;
}

struct Swim {
    float distance;   // avance neto del centroide [µm]
    float areaError;  // peor error relativo de área
    float speed;      // [µm/s]
};

// El nematodo es el jugador y avanza (W) durante `seconds`.
Swim swim(World& world, float seconds, float turn = 0.0f) {
    const Vec2 c = world.dish().center;
    const entt::entity e = world.spawnOrganism(SpeciesId::Rhabditis, c, speciesGenome(SpeciesId::Rhabditis), 0, 0.0f);
    world.setPlayer(e);
    PlayerInput in;
    in.forward = 1.0f;
    in.turn = turn;
    world.setPlayerInput(in);
    const RingBody& b = world.registry().get<RingBody>(e);
    const float rest = std::abs(b.restArea);
    float worst = 0.0f;
    const int n = static_cast<int>(seconds / kDt);
    for (int s = 0; s < n; ++s) {
        world.step(kDt);
        worst = std::max(worst, std::abs(std::abs(soft_body::area(b, world.bodies())) - rest) / rest);
    }
    const float d = length(b.centroid - c);
    return {d, worst, d / seconds};
}

} // namespace

TEST_CASE("nematodo: avanza por arrastre anisótropo; sin anisotropía no avanza") {
    World water(emptyWorld());
    const Swim w = swim(water, 6.0f);
    // Al agitarse empuja el agua (fuerza neta cero, pero el agua de alrededor se mueve).
    float stirred = 0.0f;
    for (const Vec2& p : water.registry().get<Undulation>(water.player()).spine)
        stirred = std::max(stirred, length(water.fluid().velocityAt(p)));
    float segment = 0.0f;
    const Undulation& un = water.registry().get<Undulation>(water.player());
    for (const Vec2& f : un.waterPush)
        segment = std::max(segment, length(f) / un.dragRatio);
    MESSAGE("agua junto al cuerpo: " << stirred << " µm/s | tramo más rápido ~" << segment << " µm/s");
    CHECK(stirred > 1.0f);
    CHECK(stirred < segment); // el agua pegada al cuerpo lo acompaña, no lo supera

    World iso(emptyWorld());
    iso.locomotionParams().dragRatioWater = iso.locomotionParams().dragRatioSubstrate = 1.0f;
    const Swim i = swim(iso, 6.0f);

    WorldConfig muddy = emptyWorld();
    muddy.sediment = {.quartz = 2500};
    World mud(muddy);
    const Swim m = swim(mud, 6.0f);

    MESSAGE("agua " << w.speed << " µm/s | isótropo " << i.speed << " µm/s | entre granos " << m.speed
                    << " µm/s | error de área " << w.areaError << ", " << m.areaError);
    // Con arrastre isótropo las fuerzas resistivas dan avance nulo; queda solo una deriva chica
    // de la membrana que sigue la forma con retraso.
    CHECK(i.distance < 0.1f * w.distance);
    CHECK(w.speed > 30.0f);
    CHECK(m.speed > 1.5f * w.speed);
    CHECK(w.areaError < 0.1f);
    CHECK(m.areaError < 0.1f);
}

TEST_CASE("nematodo: gira doblando el cuerpo y retrocede invirtiendo la onda") {
    World turning(emptyWorld());
    const Swim t = swim(turning, 6.0f, 1.0f);
    const RingBody& tb = turning.registry().get<RingBody>(turning.player());
    MESSAGE("con giro: rumbo final " << tb.angle << " rad, avance " << t.distance << " µm");
    CHECK(std::remainder(tb.angle, 6.2831853f) > 0.5f); // turn > 0: el rumbo crece, como en los ciliados

    World back(emptyWorld());
    const Vec2 c = back.dish().center;
    const entt::entity e = back.spawnOrganism(SpeciesId::Rhabditis, c, speciesGenome(SpeciesId::Rhabditis), 0, 0.0f);
    back.setPlayer(e);
    PlayerInput in;
    in.forward = -1.0f;
    back.setPlayerInput(in);
    for (int s = 0; s < 60 * 4; ++s)
        back.step(kDt);
    const Vec2 d = back.registry().get<RingBody>(e).centroid - c;
    MESSAGE("marcha atrás: " << d.x << ", " << d.y << " µm");
    CHECK(d.x < -20.0f); // nació mirando a +x
}

TEST_CASE("nematodo: pone un huevo que eclosiona en un nematodo que ondula") {
    WorldConfig config = emptyWorld();
    config.bacteria = 20000;
    World world(config);
    world.bioTimeScale() = 600.0f;
    const entt::entity e = world.spawnOrganism(SpeciesId::Rhabditis, world.dish().center, speciesGenome(SpeciesId::Rhabditis), 0);
    world.registry().emplace<Autopilot>(e);
    REQUIRE(world.forceReproduction(e));
    for (int s = 0; s < 60 * 100 && world.observations().eggsHatched == 0; ++s) // ~17 h biológicas
        world.step(1.0f / 60.0f);
    CHECK(world.observations().eggsLaid > 0);
    REQUIRE(world.observations().eggsHatched > 0);
    int undulating = 0;
    for (auto [y, und, sp] : world.registry().view<Undulation, Species>().each())
        undulating += sp.id == SpeciesId::Rhabditis ? 1 : 0;
    CHECK(undulating >= 2);
}

TEST_CASE("nematodo: sin orden de avanzar se relaja y queda recto") {
    World world(emptyWorld());
    const Vec2 c = world.dish().center;
    const entt::entity e = world.spawnOrganism(SpeciesId::Rhabditis, c, speciesGenome(SpeciesId::Rhabditis), 0, 0.0f);
    world.setPlayer(e);
    PlayerInput in;
    in.forward = 1.0f;
    world.setPlayerInput(in);
    for (int s = 0; s < 60 * 2; ++s)
        world.step(kDt);
    // Curvatura: cuanto se aparta el eje de la recta que une sus extremos (con la onda a pleno).
    auto bow = [&] {
        const Undulation& u = world.registry().get<Undulation>(e);
        const Vec2 a = u.spine.front(), b = u.spine.back();
        Vec2 d = b - a;
        d = d * (1.0f / std::max(1e-3f, length(d)));
        float worst = 0.0f;
        for (const Vec2& p : u.spine)
            worst = std::max(worst, std::abs((p.x - a.x) * d.y - (p.y - a.y) * d.x));
        return worst;
    };
    const float swimming = bow();
    in.forward = 0.0f;
    world.setPlayerInput(in);
    for (int s = 0; s < 60 * 3; ++s)
        world.step(kDt);
    const float resting = bow();
    MESSAGE("flecha del eje: nadando " << swimming << " um, en reposo " << resting << " um");
    CHECK(swimming > 10.0f);
    CHECK(resting < 0.2f * swimming);
}
