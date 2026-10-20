// Mecánicas de los seres (docs/plan_mecanicas.md): locomoción, reacciones y defensas que se eligen por
// datos. Casos cortos (segundos de simulación) en una placa sin bacterias.
#include "ai/autopilot.hpp"
#include "bio/cell_physiology.hpp"
#include "bio/species.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>

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

namespace {

// Un paso del piloto con lo que se le hace sentir (sin el resto del mundo).
Intent steerWith(World& world, entt::entity e, Autopilot& pilot, Perception sense, std::uint64_t step = 0) {
    Locomotion& loco = world.registry().get<Locomotion>(e);
    Rng rng(1, 2, 3, step);
    steerAutopilot(pilot, loco, world.registry().get<RingBody>(e), speciesInfo(world.registry().get<Species>(e).id),
                   sense, rng, kDt);
    return pilot.intent;
}

} // namespace

TEST_CASE("mecánicas: respuesta fotofóbica de Blepharisma, Euglena y Chlamydomonas (M1)") {
    World world = emptyWorld();
    auto check = [&](const char* key, float from, float to, bool expected) {
        CAPTURE(std::string(key));
        CAPTURE(from);
        CAPTURE(to);
        const entt::entity e = spawn(world, key, center(world));
        Autopilot pilot;
        Perception sense;
        sense.light = from;
        for (int s = 0; s < 120; ++s) // 2 s con la luz de antes: la memoria la aprende
            steerWith(world, e, pilot, sense, static_cast<std::uint64_t>(s));
        sense.light = to;
        bool phobic = false;
        for (int s = 0; s < 6; ++s)
            phobic = phobic || steerWith(world, e, pilot, sense, static_cast<std::uint64_t>(200 + s)) == Intent::Phobic;
        CHECK(phobic == expected);
    };
    check("blepharisma", 0.4f, 0.9f, true);  // se prende la luz: reacción de evitación
    check("blepharisma", 0.9f, 0.4f, false); // se apaga: nada
    check("blepharisma", 0.05f, 0.2f, false); // sigue oscuro (debajo de su nivel mínimo)
    check("euglena", 0.8f, 1.0f, true);      // ya intensa y sube más
    check("euglena", 0.3f, 0.5f, false);     // luz moderada: la busca (fototaxis), no huye
    check("chlamydomonas", 0.65f, 1.0f, true);
    check("paramecium", 0.2f, 1.0f, false);  // sin fotorreceptor
}

TEST_CASE("mecánicas: Halteria salta sola y ante la corriente de un depredador (M2)") {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "halteria", center(world));
    // Espontáneos: en 8 s, varios pasos con desplazamientos de salto (~25 µm por paso; nadando, ~1,5).
    int jumps = 0;
    Vec2 prev = world.registry().get<RingBody>(e).centroid;
    bool inJump = false;
    for (int s = 0; s < 8 * 60; ++s) {
        world.step(kDt);
        const Vec2 c = world.registry().get<RingBody>(e).centroid;
        const bool fast = length(c - prev) > 10.0f;
        jumps += fast && !inJump ? 1 : 0;
        inJump = fast;
        prev = c;
    }
    CHECK(jumps >= 2);
    CHECK(jumps <= 15);

    // De escape: el piloto salta en cuanto la deformación del agua pasa el umbral.
    World other = emptyWorld();
    const entt::entity h = spawn(other, "halteria", center(other));
    Autopilot pilot;
    pilot.nextJump = 1e9f; // sin saltos espontáneos en esta prueba
    Perception calm;
    CHECK(steerWith(other, h, pilot, calm) != Intent::Jump);
    Perception shaken;
    shaken.shear = 5.0f;
    CHECK(steerWith(other, h, pilot, shaken, 1) == Intent::Jump);
    CHECK(other.registry().get<Locomotion>(h).burst > 1000.0f);
}

TEST_CASE("mecánicas: daño fotodinámico y pigmento de estrés (Blepharisma, Haematococcus)") {
    const SpeciesInfo& bleph = speciesInfo(*findSpecies("blepharisma"));
    CellSurroundings around;
    around.o2 = 250.0f;
    auto stressAt = [&](const SpeciesInfo& info, float light, float o2, float pigment) {
        MetabolicInput in{};
        CellSurroundings a = around;
        a.light = light;
        a.o2 = o2;
        a.pigment = pigment;
        applyEnvironmentStress(info, a, nullptr, 1.0f, in);
        return in.phStress;
    };
    CHECK(stressAt(bleph, 0.4f, 250.0f, 0.0f) == doctest::Approx(0.0f));
    CHECK(stressAt(bleph, 1.0f, 250.0f, 0.0f) == doctest::Approx(0.6f));
    CHECK(stressAt(bleph, 1.0f, 0.0f, 0.0f) == doctest::Approx(0.0f)); // sin O₂ no hay especies reactivas
    CHECK(stressAt(speciesInfo(*findSpecies("paramecium")), 1.0f, 250.0f, 0.0f) == doctest::Approx(0.0f));

    // Haematococcus: con luz alta junta astaxantina en días (más rápido en el quiste) y ésta la protege.
    const SpeciesInfo& haema = speciesInfo(*findSpecies("haematococcus"));
    Metabolism green, cyst, dark;
    for (int h = 0; h < 72; ++h) {
        updateStressPigment(haema, 1.0f, Limitation::None, false, 3600.0f, green);
        updateStressPigment(haema, 1.0f, Limitation::None, true, 3600.0f, cyst);
        updateStressPigment(haema, 0.3f, Limitation::None, false, 3600.0f, dark);
    }
    CHECK(green.pigment > 0.5f);
    CHECK(cyst.pigment > green.pigment);
    CHECK(dark.pigment == doctest::Approx(0.0f));
    Metabolism starved; // sin nitrógeno también enrojece
    for (int h = 0; h < 72; ++h)
        updateStressPigment(haema, 0.3f, Limitation::Nitrogen, false, 3600.0f, starved);
    CHECK(starved.pigment > 0.5f);
    CHECK(stressAt(haema, 1.0f, 250.0f, cyst.pigment) < 0.5f * stressAt(haema, 1.0f, 250.0f, 0.0f));
}
