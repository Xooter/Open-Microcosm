#include "game/calibration.hpp"

#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/culture.hpp"
#include "game/world.hpp"

#include <cmath>

FunctionalResponsePoint measureFunctionalResponse(SpeciesId id, std::size_t bacteria, int individuals, float seconds,
                                                  std::uint64_t seed) {
    PROFILE_SCOPE("measureFunctionalResponse");
    constexpr float kDt = 1.0f / 60.0f;
    WorldConfig c;
    c.gridX = c.gridY = 128;
    c.bacteria = bacteria;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.medium.temperature = 20.0f;
    c.medium.detritusSpawnRate = 0.0f;
    c.medium.phage = 0.0f;
    c.medium.organic = 0.0f; // sin comida las bacterias no se multiplican durante la medición
    c.bdellovibrioFraction = 0.0f;
    c.seed = seed;
    World world(c);
    const Vec2 ctr = {world.width() * 0.5f, world.height() * 0.5f};
    for (int k = 0; k < individuals; ++k) {
        const float a = 2.39996f * static_cast<float>(k);
        const float r = world.dish().radius * (0.25f + 0.5f * std::fmod(0.618f * static_cast<float>(k), 1.0f));
        world.spawnBeing({BeingKind::Species, id}, ctr + Vec2{r * std::cos(a), r * std::sin(a)});
    }
    auto eaten = [&] {
        double n = 0.0;
        for (const auto [e, d, sp] : world.registry().view<const Digestion, const Species>().each())
            n += sp.id == id ? d.bacteriaEaten : 0;
        return n;
    };
    for (int s = 0; s < 120; ++s) // que empiecen a nadar
        world.step(kDt);
    const double before = eaten();
    const double volume = units::kPi * world.dish().radius * world.dish().radius * world.fluid().gapHeight;
    double density = 0.0, cellSeconds = 0.0;
    const int steps = static_cast<int>(seconds / kDt);
    for (int s = 0; s < steps; ++s) {
        density += static_cast<double>(world.bacteria().size()) / volume;
        world.step(kDt);
        for (const auto [e, sp] : world.registry().view<const Species>().each())
            cellSeconds += sp.id == id ? kDt : 0.0;
    }
    FunctionalResponsePoint p;
    p.density = density / steps;
    p.individuals = individuals;
    // La boca traga a ritmo mecánico (tiempo real) y cada presa rinde por `bioScale`: por segundo
    // biológico come lo mismo que por segundo real.
    p.plate = (eaten() - before) / std::max(1e-9, cellSeconds);
    CultureConfig cc;
    cc.medium.temperature = 20.0f;
    Culture culture(cc);
    culture.inoculate(id, 1);
    if (!culture.cells().empty())
        p.culture = culture.bacterivory(culture.cells().front(), p.density);
    return p;
}
