// Physarum polycephalum en el mundo: plasmodios (red de tubos que busca comida), copos de avena
// como cebo y el rastro de baba que evitan las puntas.
#include "game/world.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"

#include <algorithm>
#include <cmath>

void World::seedPlasmodia() {
    for (std::uint64_t k = 0; k < static_cast<std::uint64_t>(config_.physarum); ++k) {
        Rng rng(config_.seed, rng_stream::kFungi + 450, k, 0);
        spawnPlasmodium(randomPoint(rng, 30.0f), rng.uniform(0.0f, 2.0f * units::kPi));
    }
}

void World::spawnPlasmodium(Vec2 at, float heading) {
    // Un inóculo: reservas para unas decenas de tramos de tubo. El fósforo sale del agua.
    const PlasmodiumParams& p = plasmo_.params;
    const float reserves = 40.0f * p.nodePmol;
    const float phosphorus = -exchange(phosphate_, at, -3.0f * p.phosphorusRatio * reserves);
    plasmo_.spawn(at, heading, reserves, phosphorus);
}

void World::spawnOatFlake(Vec2 at) {
    // Un copo de avena: almidón (y su fósforo, del agua) que se disuelve despacio.
    if (plasmo_.flakes.size() >= plasmo_.params.maxFlakes)
        return;
    const float pmol = 150.0f;
    const float phosphorus = -exchange(phosphate_, at, -plasmo_.params.phosphorusRatio * pmol);
    plasmo_.addFlake(at, pmol, phosphorus);
}

void World::stepPlasmodium(float dt) {
    PROFILE_SCOPE("World::stepPlasmodium");
    if (plasmo_.plasmodia.empty() && plasmo_.flakes.size() == 0 && plasmo_.sclerotia.size() == 0)
        return;
    const float bioDt = dt * bioScale();
    const PlasmodiumParams& p = plasmo_.params;

    // La baba se degrada muy despacio.
    const float keep = std::exp(-0.02f * bioDt / 3600.0f);
    for (float& v : slime_.v)
        v *= keep;

    PlasmodiumEnv env;
    env.food = [&](Vec2 at) {
        const Vec2 w = wrapped(at);
        return nutrients_.grid().sample(w.x, w.y);
    };
    env.takeFood = [&](Vec2 at, float pmol) { return -exchange(nutrients_, wrapped(at), -pmol); };
    env.takePhosphate = [&](Vec2 at, float pmol) { return -exchange(phosphate_, wrapped(at), -pmol); };
    env.release = [&](Vec2 at, float pmol, float phosphorus) {
        if (pmol > 0.0f)
            exchange(nutrients_, wrapped(at), pmol);
        if (phosphorus > 0.0f)
            exchange(phosphate_, wrapped(at), phosphorus);
    };
    env.markSlime = [&](Vec2 at) {
        const Vec2 w = wrapped(at);
        slime_.at(slime_.cellX(w.x), slime_.cellY(w.y)) = 1.0f;
    };
    // Rumbo: sube el gradiente de materia orgánica (quimiotaxis hacia la comida), se aleja del
    // rastro de baba (ya explorado) y de la luz (fotofobia: el plasmodio huye de la lámpara).
    env.cue = [&](Vec2 at) {
        const Vec2 w = wrapped(at);
        const Grid2D& food = nutrients_.grid();
        const float g = food.dx;
        const Vec2 gf = {food.sample(w.x + g, w.y) - food.sample(w.x - g, w.y), food.sample(w.x, w.y + g) - food.sample(w.x, w.y - g)};
        const float nf = std::sqrt(dot(gf, gf));
        Vec2 cue = nf > 1e-6f ? gf * (1.0f / nf) * (nf / (nf + 0.05f)) : Vec2{};
        const Vec2 gs = {slime_.sample(w.x + g, w.y) - slime_.sample(w.x - g, w.y), slime_.sample(w.x, w.y + g) - slime_.sample(w.x, w.y - g)};
        cue = cue - gs * (p.slimeAvoidance * 0.5f);
        const Vec2 gl = {light_.sample(w.x + g, w.y) - light_.sample(w.x - g, w.y), light_.sample(w.x, w.y + g) - light_.sample(w.x, w.y - g)};
        return cue - gl * (p.lightAvoidance * 0.5f);
    };
    plasmo_.step(env, bioDt, config_.seed, step_);
    observed_.physarumFusions = plasmo_.stats.fusions;
    observed_.sclerotiaFormed = plasmo_.stats.sclerotiaFormed;
    observed_.sclerotiaGerminated = plasmo_.stats.sclerotiaGerminated;
}
