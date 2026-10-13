// Algas filamentosas (Mougeotia, Zygnema, Oedogonium), Oscillatoria y el gastrotrico.
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig quietWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.saprolegnia = config.zoophagus = config.chytridClusters = 0;
    config.seed = 91;
    return config;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

// Desvío lateral máximo de la punta respecto de la recta que recorrió [µm].
float tipWander(World& world, std::size_t filament, int n) {
    const CyanoFilament& f0 = world.cyanobacteria().filaments[filament];
    const bool forward = f0.direction > 0.0f;
    auto tip = [&] {
        const CyanoFilament& f = world.cyanobacteria().filaments[filament];
        return forward ? Vec2{f.x.back(), f.y.back()} : Vec2{f.x.front(), f.y.front()};
    };
    std::vector<Vec2> path;
    for (int s = 0; s < n; ++s) {
        world.step(kDt);
        path.push_back(tip());
    }
    const Vec2 a = path.front(), b = path.back();
    Vec2 d = b - a;
    d = d * (1.0f / std::max(1e-4f, length(d)));
    float worst = 0.0f;
    for (const Vec2& p : path)
        worst = std::max(worst, std::abs((p.x - a.x) * d.y - (p.y - a.y) * d.x));
    return worst;
}

} // namespace

TEST_CASE("Mougeotia, Zygnema y Oedogonium crecen en filamento y se dividen con un tabique") {
    for (SpeciesId id : {SpeciesId::Mougeotia, SpeciesId::Zygnema, SpeciesId::Oedogonium}) {
        CAPTURE(speciesInfo(id).name);
        World world(quietWorld());
        const auto chain = world.spawnFilament(id, world.dish().center - Vec2{150.0f, 0.0f}, 0.1f, 5, 0);
        REQUIRE(world.links().size() == 8);
        REQUIRE(world.forceReproduction(chain[2]));
        steps(world, 60 * 40);
        CHECK(world.cellCount() == 6);
        CHECK(world.links().size() == 10);
        CHECK(world.observations().filamentDivisions == 1);
        // La célula que se dividió queda marcada (en Oedogonium, un capuchón por división).
        std::uint32_t generation = 0;
        for (auto [e, lin] : world.registry().view<Lineage>().each())
            generation = std::max(generation, lin.generation);
        CHECK(generation >= 1);
    }
}

TEST_CASE("Oscillatoria: sin heterocistos ni fijación de N₂, y la punta oscila al deslizarse") {
    WorldConfig config = quietWorld();
    config.oscillatoriaFilaments = 12;
    World world(config);
    world.bioTimeScale() = 3600.0f;
    const double nitrogen = world.totalNitrogen();
    const std::size_t cells = world.cyanobacteria().cellCount();
    steps(world, 60 * 20);
    int heterocysts = 0;
    for (const CyanoFilament& f : world.cyanobacteria().filaments) {
        CHECK(f.oscillatoria == 1);
        for (std::uint8_t h : f.heterocyst)
            heterocysts += h;
    }
    MESSAGE("células " << cells << " → " << world.cyanobacteria().cellCount() << ", nitrógeno " << nitrogen << " → "
                       << world.totalNitrogen());
    CHECK(heterocysts == 0);
    CHECK(world.observations().nitrogenFixed == 0.0f);
    CHECK(world.cyanobacteria().cellCount() > cells); // crecen con el N del agua
    CHECK(world.totalNitrogen() == doctest::Approx(nitrogen).epsilon(0.01));

    // Vaivén de la punta: Oscillatoria se aparta de la recta; Anabaena casi no.
    WorldConfig one = quietWorld();
    one.oscillatoriaFilaments = 1;
    World osc(one);
    one.oscillatoriaFilaments = 0;
    one.cyanoFilaments = 1;
    World ana(one);
    const float swayOsc = tipWander(osc, 0, 60 * 10), swayAna = tipWander(ana, 0, 60 * 10);
    MESSAGE("desvío de la punta: Oscillatoria " << swayOsc << " µm, Anabaena " << swayAna << " µm");
    CHECK(swayOsc > 3.0f * swayAna);
    CHECK(swayOsc > 2.0f);
}

TEST_CASE("el gastrotrico pone un huevo partenogenético que eclosiona") {
    World world(quietWorld(20000));
    world.bioTimeScale() = 600.0f;
    const entt::entity g = world.spawnOrganism(SpeciesId::Chaetonotus, world.dish().center, speciesGenome(SpeciesId::Chaetonotus), 0);
    world.registry().emplace<Autopilot>(g);
    REQUIRE(world.registry().all_of<Compound>(g));
    REQUIRE(world.forceReproduction(g));
    steps(world, 60 * 160); // ~27 h biológicas: el huevo tarda ~24 h
    CHECK(world.observations().eggsLaid > 0);
    CHECK(world.observations().eggsHatched > 0);
}

TEST_CASE("Oedogonium libera zoósporas que nadan, se fijan y vuelven a ser filamento") {
    World world(quietWorld());
    const Vec2 c = world.dish().center;
    std::vector<entt::entity> cells;
    for (int f = 0; f < 3; ++f)
        for (entt::entity e : world.spawnFilament(SpeciesId::Oedogonium, c + Vec2{-250.0f, -200.0f + 200.0f * f}, 0.0f, 7, 0))
            cells.push_back(e);
    const double phosphorus = world.totalPhosphorus();
    for (entt::entity e : cells)
        world.forceReproduction(e);
    world.step(kDt);
    REQUIRE(world.observations().zoospores > 0);
    entt::entity z = entt::null;
    for (auto e : world.registry().view<Zoospore>())
        z = e;
    REQUIRE(world.registry().valid(z));
    CHECK(world.registry().get<Locomotion>(z).type == LocomotionType::Flagellum);
    const Vec2 start = world.registry().get<RingBody>(z).centroid;
    steps(world, 60 * 5);
    REQUIRE(world.registry().valid(z));
    const float swum = length(world.registry().get<RingBody>(z).centroid - start);
    MESSAGE("la zoóspora nadó " << swum << " µm en 5 s");
    CHECK(swum > 50.0f);
    CHECK(world.totalPhosphorus() == doctest::Approx(phosphorus).epsilon(0.01));

    world.bioTimeScale() = 1200.0f; // la zoóspora nada como mucho 1 h biológica
    steps(world, 60 * 4);
    CHECK(world.observations().zoosporesSettled > 0);
    if (world.registry().valid(z)) {
        CHECK_FALSE(world.registry().all_of<Zoospore>(z));
        CHECK(world.registry().get<Locomotion>(z).type == LocomotionType::Cilia);
    }
}
