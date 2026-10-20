// Mecánicas de los seres (docs/plan_mecanicas.md): locomoción, reacciones y defensas que se eligen por
// datos. Casos cortos (segundos de simulación) en una placa sin bacterias.
#include "ai/autopilot.hpp"
#include "bio/bacteria.hpp"
#include "bio/cell_physiology.hpp"
#include "bio/species.hpp"
#include "game/world.hpp"
#include "sim/fluid.hpp"
#include "sim/grid2d.hpp"

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

namespace {

float restHalfLength(const World& world, entt::entity e) {
    float l = 0.0f;
    for (const Vec2& q : world.registry().get<RingBody>(e).restRing)
        l = std::max(l, std::abs(q.x));
    return l;
}

bool simpleRing(const World& world, entt::entity e) {
    const RingBody& b = world.registry().get<RingBody>(e);
    const BodyParticles& bp = world.bodies();
    auto p = [&](std::size_t i) {
        const std::uint32_t s = b.ring[i % b.ring.size()];
        return Vec2{bp.x[s], bp.y[s]};
    };
    const std::size_t n = b.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1)
                continue;
            const Vec2 a = p(i), c = p(i + 1), d = p(j), f = p(j + 1);
            if (cross(c - a, d - a) * cross(c - a, f - a) < 0.0f && cross(f - d, a - d) * cross(f - d, c - d) < 0.0f)
                return false;
        }
    return true;
}

void run(World& world, float seconds) {
    for (int s = 0; s < static_cast<int>(seconds / kDt); ++s)
        world.step(kDt);
}

} // namespace

TEST_CASE("mecánicas: Stentor se extiende, se contrae entero ante una sacudida y se vuelve a extender" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    Rng rng(7, 1, 0, 0);
    const entt::entity e = world.spawnWild(*findSpecies("stentor"), center(world), rng, false); // fija por el pie
    const float contracted = restHalfLength(world, e);
    run(world, 20.0f); // nace contraída y se extiende en ~15 s
    const float extended = restHalfLength(world, e);
    CHECK(extended > 1.8f * contracted);
    CHECK(simpleRing(world, e));
    // Una corriente brusca a un costado (la deforma, no solo la empuja): se contrae en un instante.
    for (int s = 0; s < 6; ++s) {
        const RingBody& rb = world.registry().get<RingBody>(e);
        const Vec2 h = bodyHeading(rb);
        world.setStir(rb.centroid + Vec2{-h.y, h.x} * 40.0f, h * 1500.0f, 40.0f);
        world.step(kDt);
    }
    world.clearStir();
    CHECK(world.registry().get<Contraction>(e).state > 0.9f);
    CHECK(restHalfLength(world, e) < 1.2f * contracted);
    run(world, 0.5f);
    const RingBody& b = world.registry().get<RingBody>(e);
    float actual = 0.0f; // largo real del cuerpo a lo largo de su eje
    for (std::uint32_t sIdx : b.ring)
        actual = std::max(actual, std::abs(dot(Vec2{world.bodies().x[sIdx], world.bodies().y[sIdx]} - b.centroid,
                                               bodyHeading(b))));
    CHECK(actual < 0.75f * extended);
    CHECK(simpleRing(world, e));
    run(world, 20.0f);
    CHECK(restHalfLength(world, e) > 1.8f * contracted);
    CHECK(simpleRing(world, e));
}

TEST_CASE("mecánicas: la contracción de un Spirostomum deforma el agua a cientos de µm (Mathijssen 2019)") {
    // Onda de disparo hidrodinámica: el remolino de una contracción supera el umbral de deformación de
    // sus vecinos (1/s) a ~1 cuerpo de distancia. Se mide el flujo de fondo del paso siguiente.
    World world = emptyWorld(23);
    const Vec2 c = center(world);
    const entt::entity e = spawn(world, "spirostomum", c);
    world.registry().get<Locomotion>(e).power = 0.0f; // quieto
    run(world, 3.0f);                                 // extendido
    auto strainAt = [&](Vec2 p) {
        constexpr float d = 10.0f;
        const Fluid& f = world.fluid();
        const Vec2 ux1 = f.backgroundAt(p + Vec2{d, 0.0f}), ux0 = f.backgroundAt(p - Vec2{d, 0.0f});
        const Vec2 uy1 = f.backgroundAt(p + Vec2{0.0f, d}), uy0 = f.backgroundAt(p - Vec2{0.0f, d});
        const float dudx = (ux1.x - ux0.x) / (2 * d), dvdy = (uy1.y - uy0.y) / (2 * d);
        const float dudy = (uy1.x - uy0.x) / (2 * d), dvdx = (ux1.y - ux0.y) / (2 * d);
        return std::sqrt((dudx - dvdy) * (dudx - dvdy) + (dudy + dvdx) * (dudy + dvdx));
    };
    auto maxStrainAround = [&](float radius) {
        const Vec2 at = world.registry().get<RingBody>(e).centroid;
        float worst = 0.0f;
        for (int k = 0; k < 32; ++k) {
            const float a = 6.2831853f * static_cast<float>(k) / 32.0f;
            worst = std::max(worst, strainAt(at + Vec2{std::cos(a), std::sin(a)} * radius));
        }
        return worst;
    };
    const float calm = maxStrainAround(250.0f);
    // Un toque fuerte a un costado del centro: se contrae.
    const RingBody& rb = world.registry().get<RingBody>(e);
    const Vec2 h = bodyHeading(rb);
    world.setStir(rb.centroid + Vec2{-h.y, h.x} * 30.0f, h * 1500.0f, 30.0f);
    world.step(kDt);
    world.clearStir();
    REQUIRE(world.registry().get<Contraction>(e).state > 0.99f);
    world.step(kDt); // el impulso entra en el flujo de fondo de este paso
    CHECK(calm < 0.1f);
    CHECK(maxStrainAround(250.0f) > 1.0f);
}

namespace {

const Stalk* stalkOf(const World& world, entt::entity zooid) {
    const Anchored* an = world.registry().try_get<Anchored>(zooid);
    return an && world.registry().valid(an->rod) ? world.registry().try_get<Stalk>(an->rod) : nullptr;
}

float polyLength(const std::vector<Vec2>& p) {
    float l = 0.0f;
    for (std::size_t i = 1; i < p.size(); ++i)
        l += length(p[i] - p[i - 1]);
    return l;
}

// Desvío máximo de los nodos respecto de la recta disco–punta [µm].
float bend(const std::vector<Vec2>& p) {
    const Vec2 a = p.front(), b = p.back();
    const Vec2 u = (b - a) * (1.0f / std::max(1e-3f, length(b - a)));
    float worst = 0.0f;
    for (const Vec2& q : p)
        worst = std::max(worst, std::abs(cross(u, q - a)));
    return worst;
}

entt::entity wild(World& world, const char* key, Vec2 at) {
    Rng rng(9, 1, 0, 0);
    return world.spawnWild(*findSpecies(key), at, rng, false);
}

} // namespace

TEST_CASE("mecánicas: el tallo de Vorticella es una varilla inextensible que el agua dobla (M9)") {
    World world = emptyWorld();
    const entt::entity e = wild(world, "vorticella", center(world));
    const Stalk* st = stalkOf(world, e);
    REQUIRE(st);
    const float adult = speciesInfo(*findSpecies("vorticella")).stalk.length;
    CHECK(polyLength(st->rod.nodes) == doctest::Approx(adult).epsilon(0.02));
    // Sin el zooide (y su corriente oral, que también lo dobla) el tallo vacío queda derecho.
    const entt::entity rod = world.registry().get<Anchored>(e).rod;
    world.removeCellAt(world.registry().get<RingBody>(e).centroid);
    run(world, 2.0f);
    const float calmBend = bend(world.registry().get<Stalk>(rod).rod.nodes);
    CHECK(calmBend < 3.0f);
    // Una corriente lateral a la mitad lo dobla; el largo no cambia.
    for (int s = 0; s < 30; ++s) {
        const std::vector<Vec2>& n = world.registry().get<Stalk>(rod).rod.nodes;
        const Vec2 mid = n[n.size() / 2], axis = n.back() - n.front();
        const Vec2 side = Vec2{-axis.y, axis.x} * (1.0f / std::max(1e-3f, length(axis)));
        world.setStir(mid, side * 150.0f, 25.0f);
        world.step(kDt);
    }
    world.clearStir();
    const Stalk& bent = world.registry().get<Stalk>(rod);
    CHECK(bend(bent.rod.nodes) > calmBend + 5.0f);
    CHECK(polyLength(bent.rod.nodes) == doctest::Approx(stalkRestLength(bent.rod, speciesInfo(bent.species).stalk)).epsilon(0.03));
    // Sin corriente vuelve a quedar derecho (es elástico).
    run(world, 3.0f);
    CHECK(bend(world.registry().get<Stalk>(rod).rod.nodes) < calmBend + 2.0f);
}

TEST_CASE("mecánicas: el espasmonema se enrolla al instante y se re-extiende; el tallo de Epistylis no se contrae") {
    auto shake = [](World& world, entt::entity e) {
        for (int s = 0; s < 4; ++s) { // una sacudida fuerte junto al zooide (lo deforma)
            const RingBody& rb = world.registry().get<RingBody>(e);
            const Vec2 h = bodyHeading(rb);
            world.setStir(rb.centroid + Vec2{-h.y, h.x} * 30.0f, h * 1500.0f, 30.0f);
            world.step(kDt);
        }
        world.clearStir();
    };
    World world = emptyWorld();
    const entt::entity e = wild(world, "vorticella", center(world));
    run(world, 1.0f);
    const Anchored& an = world.registry().get<Anchored>(e);
    const float adult = speciesInfo(*findSpecies("vorticella")).stalk.length;
    shake(world, e);
    CHECK(stalkOf(world, e)->rod.contracted > 0.9f);
    run(world, 0.2f);
    // El zooide cayó hacia el disco: el tallo mide ~1/3.
    const RingBody& rb = world.registry().get<RingBody>(e);
    const Vec2 pole = Vec2{world.bodies().x[rb.ring[World::poleIndex(rb, false)]],
                           world.bodies().y[rb.ring[World::poleIndex(rb, false)]]};
    CHECK(length(pole - an.holdfast) < 0.6f * adult);
    run(world, 5.0f);
    CHECK(polyLength(stalkOf(world, e)->rod.nodes) > 0.9f * adult);

    World other = emptyWorld();
    const entt::entity ep = wild(other, "epistylis", center(other));
    run(other, 1.0f);
    shake(other, ep);
    CHECK(stalkOf(other, ep)->rod.contracted == doctest::Approx(0.0f));
    CHECK(polyLength(stalkOf(other, ep)->rod.nodes) > 0.98f * speciesInfo(*findSpecies("epistylis")).stalk.length);
}

TEST_CASE("mecánicas: el telotroco secreta su tallo al fijarse; muerto el zooide, el tallo queda y se degrada" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f; // 1 s = 1 h biológica
    const entt::entity e = spawn(world, "vorticella", center(world));
    world.registry().emplace<Telotroch>(e, Telotroch{0.05f});
    run(world, 0.2f);
    const Stalk* st = stalkOf(world, e);
    REQUIRE(st);
    const StalkSpec& spec = speciesInfo(*findSpecies("vorticella")).stalk;
    CHECK(st->rod.length < 0.2f * spec.length);
    run(world, spec.growthHours + 0.5f);
    CHECK(stalkOf(world, e)->rod.length == doctest::Approx(spec.length));
    // Muere el zooide: queda el tallo vacío en el vidrio.
    const entt::entity rod = world.registry().get<Anchored>(e).rod;
    world.removeCellAt(world.registry().get<RingBody>(e).centroid);
    world.step(kDt);
    REQUIRE(world.registry().valid(rod));
    CHECK((world.registry().get<Stalk>(rod).zooid == entt::null));
    run(world, spec.decayHours + 1.0f);
    CHECK_FALSE(world.registry().valid(rod));
}

TEST_CASE("mecánicas: Stentor se fija por el pie y con hambre lo suelta, nada y se vuelve a fijar" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const entt::entity e = wild(world, "stentor", center(world));
    REQUIRE(world.registry().all_of<Anchored>(e));
    CHECK_FALSE(world.registry().valid(world.registry().get<Anchored>(e).rod)); // sin tallo
    // Sin bacterias pasa hambre: en algunas horas biológicas se suelta.
    world.registry().get<Metabolism>(e).reserves = 0.0f;
    bool released = false, reattached = false;
    for (int s = 0; s < 60 * 150 && !reattached; ++s) {
        world.step(kDt);
        if (!world.registry().valid(e))
            break;
        if (!released && world.registry().all_of<Telotroch>(e)) {
            released = true; // ya nadando: encontró comida (que no se muera antes de volver a fijarse)
            Metabolism& m = world.registry().get<Metabolism>(e);
            m.reserves = 3.0f * m.biomass;
        }
        reattached = released && world.registry().all_of<Anchored>(e);
    }
    CHECK(released);
    CHECK(reattached);
}

TEST_CASE("mecánicas: los toxicistos de Dileptus paralizan a la presa antes de tragarla (M6)") {
    World world = emptyWorld();
    const Vec2 c = center(world);
    const entt::entity d = spawn(world, "dileptus", c);
    world.registry().get<Locomotion>(d).power = 0.0f; // quieto: la presa la pone el test
    const RingBody& db = world.registry().get<RingBody>(d);
    const Vec2 tip = db.centroid + bodyHeading(db) * restHalfLength(world, d);
    const auto pid = *findSpecies("paramecium");
    Genome g = speciesGenome(pid);
    g[Gene::Trichocysts] = 0.0f; // sin defensa: el resultado no depende del azar
    const entt::entity p = world.spawnOrganism(pid, tip + bodyHeading(db) * 70.0f, g, 0);
    bool paralyzed = false;
    for (int s = 0; s < 120 && !paralyzed; ++s) {
        world.step(kDt);
        paralyzed = !world.registry().valid(p) || world.registry().all_of<Paralyzed>(p);
    }
    CHECK(paralyzed);
    if (world.registry().valid(p)) { // paralizada: no nada
        const Vec2 before = world.registry().get<RingBody>(p).centroid;
        run(world, 1.0f);
        if (world.registry().valid(p))
            CHECK(length(world.registry().get<RingBody>(p).centroid - before) < 15.0f);
    }
}

TEST_CASE("mecánicas: Coleps come a mordiscos una célula paralizada más grande que él (histofagia)") {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const Vec2 c = center(world);
    const auto pid = *findSpecies("paramecium");
    const entt::entity p = world.spawnOrganism(pid, c, speciesGenome(pid), 0);
    world.registry().emplace<Paralyzed>(p, Paralyzed{100.0f, false});
    const float before = world.registry().get<Metabolism>(p).biomass;
    // Un Coleps con la boca contra el paramecio.
    const RingBody& pb = world.registry().get<RingBody>(p);
    const entt::entity k = spawn(world, "coleps", c + Vec2{0.5f * (pb.aabbMax.x - pb.aabbMin.x) + 30.0f, 0.0f});
    world.registry().get<RingBody>(k).angle = 3.14159265f;
    CHECK(world.isDamaged(p));
    run(world, 3.0f);
    CHECK((!world.registry().valid(p) || world.registry().get<Metabolism>(p).biomass < 0.8f * before));
}

TEST_CASE("mecánicas: la apertura bucal deja afuera a Staurastrum por sus brazos y no a Cosmarium (M5)") {
    const SpeciesInfo& frontonia = speciesInfo(*findSpecies("frontonia"));
    const SpeciesInfo& staurastrum = speciesInfo(*findSpecies("staurastrum"));
    const SpeciesInfo& cosmarium = speciesInfo(*findSpecies("cosmarium"));
    CHECK(staurastrum.minWidth > 2.0f * staurastrum.spec.radiusY + 30.0f); // los brazos la ensanchan
    CHECK(fitsMouth(frontonia, 1.0f, cosmarium, 1.0f));
    CHECK_FALSE(fitsMouth(frontonia, 1.0f, staurastrum, 1.0f));
    CHECK(fitsMouth(speciesInfo(*findSpecies("amoeba")), 1.0f, staurastrum, 1.0f)); // sin boca: envuelve
    // Una Frontonia chica (la mitad) ya no traga un Cosmarium.
    CHECK_FALSE(fitsMouth(frontonia, 0.5f, cosmarium, 1.0f));
}

TEST_CASE("mecánicas: la kairomona de Lembadion le arma alas a Euplotes y lo deja fuera de su boca (M3, M4)" * doctest::test_suite("largos")) {
    auto induced = [](bool withPredator) {
        World world = emptyWorld();
        world.bioTimeScale() = 3600.0f; // 1 s = 1 h biológica
        const Vec2 c = center(world);
        const entt::entity eu = spawn(world, "euplotes_octocarinatus", c);
        world.registry().get<Locomotion>(eu).power = 0.0f;
        entt::entity le = entt::null;
        if (withPredator) {
            le = spawn(world, "lembadion", c + Vec2{150.0f, 0.0f});
            world.registry().get<Locomotion>(le).power = 0.0f;
        }
        run(world, 30.0f); // la kairomona llega a su nivel en ~1 vida media (12 h) y el morfo tarda 8 h
        const bool winged = world.morphAmount(eu) > 0.9f;
        if (winged && le != entt::null) {
            CHECK_FALSE(world.fitsMouth(le, world.registry().get<RingBody>(le), eu, world.registry().get<RingBody>(eu)));
        }
        return winged;
    };
    CHECK(induced(true));
    CHECK_FALSE(induced(false));
}

TEST_CASE("mecánicas: Tetrahymena vorax pasa a macrostoma junto a Tetrahymena y cambia de dieta" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const Vec2 c = center(world);
    const entt::entity v = spawn(world, "tetrahymena_vorax", c);
    world.registry().get<Locomotion>(v).power = 0.0f;
    CHECK(world.feedingOf(v) == FeedingMode::OralGroove);
    for (int k = 0; k < 4; ++k) {
        const entt::entity t = spawn(world, "tetrahymena", c + Vec2{80.0f + 20.0f * k, 60.0f});
        world.registry().get<Locomotion>(t).power = 0.0f;
    }
    run(world, 20.0f); // las señales llegan a su nivel en horas y la transformación tarda 6 h
    CHECK(world.morphAmount(v) > 0.9f);
    CHECK(world.feedingOf(v) == FeedingMode::Raptorial);
    CHECK(world.preyOf(v).test(static_cast<std::size_t>(*findSpecies("tetrahymena"))));
}

TEST_CASE("mecánicas: Colpoda se divide en cuatro dentro de su quiste reproductivo, quieta y sin comer" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "colpoda", center(world));
    REQUIRE(world.forceReproduction(e));
    run(world, 0.5f);
    CHECK(world.inDivisionCyst(e));
    int count = 0;
    for (int s = 0; s < 60 * 240 && count < 4; ++s) {
        world.step(kDt);
        count = 0;
        for (auto [c, sp] : world.registry().view<Species>().each())
            count += sp.id == *findSpecies("colpoda") ? 1 : 0;
    }
    CHECK(count == 4);
}

TEST_CASE("mecánicas: la lórica de Trachelomonas toma hierro del agua y la hija nace con la suya vacía" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const entt::entity e = spawn(world, "trachelomonas", center(world));
    const double before = world.totalIron();
    run(world, 30.0f); // ~30 h biológicas
    const Metabolism& m = world.registry().get<Metabolism>(e);
    // La lórica agota el hierro disuelto a su alrededor: acumula de a poco (más del 20 % de una completa).
    CHECK(m.loricaIron > 0.2f * speciesInfo(*findSpecies("trachelomonas")).loricaIronRatio * m.biomass);
    CHECK(world.totalIron() == doctest::Approx(before).epsilon(1e-3)); // se conserva
    Metabolism mother = m;
    const Metabolism child = splitMetabolism(mother, 0.5f);
    CHECK(child.loricaIron == doctest::Approx(0.0f));
    CHECK(mother.loricaIron == doctest::Approx(m.loricaIron));
}

TEST_CASE("mecánicas: los axopodios de Actinophrys atrapan un flagelado y lo traen al cuerpo para tragarlo") {
    World world = emptyWorld();
    const Vec2 c = center(world);
    spawn(world, "actinophrys", c);
    const entt::entity p = spawn(world, "chlamydomonas", c + Vec2{55.0f, 0.0f});
    world.registry().get<Locomotion>(p).power = 0.0f;
    bool caught = false, eaten = false;
    for (int s = 0; s < 60 * 30 && !eaten; ++s) {
        world.step(kDt);
        eaten = !world.registry().valid(p);
        caught = caught || eaten || world.registry().all_of<Captured>(p);
    }
    CHECK(caught);
    CHECK(eaten);
}

TEST_CASE("mecánicas: Vampyrella perfora una célula de Spirogyra y después le vacía el protoplasto") {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const Vec2 c = center(world);
    const auto sid = *findSpecies("spirogyra");
    const entt::entity alga = world.spawnOrganism(sid, c, speciesGenome(sid), 0);
    const float before = world.registry().get<Metabolism>(alga).biomass;
    spawn(world, "vampyrella", c + Vec2{0.0f, 35.0f});
    bool perforating = false;
    for (int s = 0; s < 30 && !perforating; ++s) {
        world.step(kDt);
        if (const Captured* cap = world.registry().try_get<Captured>(alga))
            perforating = cap->delayHours > 0.0f;
    }
    CHECK(perforating);
    run(world, 2.0f); // ~2 h biológicas
    CHECK((!world.registry().valid(alga) || world.registry().get<Metabolism>(alga).biomass < 0.7f * before));
}

TEST_CASE("mecánicas: sin fósforo, Anabaena/Dolichospermum forma acinetos junto a los heterocistos y germinan al volver" * doctest::test_suite("largos")) {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.seed = 5;
    config.cyanoFilaments = 6;
    config.oscillatoriaFilaments = 0;
    config.medium.phosphate = 0.0f;
    World world(config);
    world.bioTimeScale() = 3600.0f;
    auto akinetes = [&] {
        int n = 0;
        for (const CyanoFilament& f : world.cyanobacteria().filaments)
            for (std::uint8_t type : f.heterocyst)
                n += type == kAkinete ? 1 : 0;
        return n;
    };
    run(world, 40.0f); // ~40 h biológicas sin P
    const int formed = akinetes();
    CHECK(formed > 0);
    // Vuelve el fósforo: germinan.
    world.medium().phosphate = 5.0f;
    world.refreshMedium();
    run(world, 120.0f);
    CHECK(akinetes() < formed);
}

TEST_CASE("mecánicas: Spirillum volutans hace aerotaxis hacia su O₂ óptimo y forma una banda (M8)") {
    // Gradiente de O₂ exponencial a lo ancho (sin agua en movimiento), de ~0,4 a ~250 µM, con su óptimo
    // (~10 µM, el 4 % de la saturación) en el centro, lejos de los bordes periódicos de la grilla.
    constexpr int kN = 64;
    constexpr float kCell = 8.0f, kSize = kN * kCell;
    Grid2D oxygen(kN, kN, kCell), nutrients(kN, kN, kCell);
    for (int y = 0; y < kN; ++y)
        for (int x = 0; x < kN; ++x)
            oxygen.at(x, y) = 10.0f * std::exp((static_cast<float>(x) * kCell - 0.5f * kSize) / 80.0f);
    Fluid fluid(kN, kN, kCell);
    Bacteria bac;
    Rng rng(3, 1, 0, 0);
    for (int i = 0; i < 300; ++i)
        bac.add(rng.uniform(0.0f, kSize), rng.uniform(0.0f, kSize), rng.uniform(0.0f, 6.2831853f), 1.0f, kSpirillumStrain);
    bac.resetMemory(nutrients, oxygen);
    auto meanDistance = [&] { // a su óptimo, en escala logarítmica
        double d = 0.0;
        for (std::size_t i = 0; i < bac.size(); ++i)
            d += std::abs(std::log((oxygen.sample(bac.x[i], bac.y[i]) + 1.0f) / 11.0f));
        return d / static_cast<double>(bac.size());
    };
    const double before = meanDistance();
    for (int s = 0; s < 60 * 120; ++s)
        bac.step(fluid, nutrients, oxygen, kDt, 3, static_cast<std::uint64_t>(s), kSize, kSize);
    CHECK(meanDistance() < 0.6 * before);
}

TEST_CASE("mecánicas: Salpingoeca nada con el flagelo atrás y deja en el agua el par de un pusher (M7)") {
    // Puller (Chlamydomonas): delante de la célula el agua va hacia atrás. Pusher: delante va hacia adelante.
    auto aheadFlow = [](const char* key) {
        World world = emptyWorld();
        const entt::entity e = spawn(world, key, center(world));
        float along = 0.0f;
        for (int s = 0; s < 120; ++s) {
            world.step(kDt);
            const RingBody& b = world.registry().get<RingBody>(e);
            const Vec2 h = bodyHeading(b);
            if (world.registry().get<Locomotion>(e).speed > 0.0f)
                along += dot(world.fluid().velocityAt(b.centroid + h * 12.0f) - world.fluid().backgroundAt(b.centroid + h * 12.0f), h);
        }
        return along;
    };
    CHECK(aheadFlow("salpingoeca") > 0.0f);
    CHECK(aheadFlow("chlamydomonas") < 0.0f);
    CHECK(speciesInfo(*findSpecies("salpingoeca")).flagellumPosterior);
}

TEST_CASE("mecánicas: Lingulodinium destella al agitar el agua, y de noche mucho más que de día" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "lingulodinium", center(world));
    world.registry().get<Locomotion>(e).power = 0.0f;
    auto shake = [&] {
        float best = 0.0f;
        for (int s = 0; s < 6; ++s) {
            const RingBody& rb = world.registry().get<RingBody>(e);
            world.setStir(rb.centroid + Vec2{0.0f, 15.0f}, {4000.0f, 0.0f}, 15.0f); // un golpe de agua (>10/s)
            world.step(kDt);
            if (const Luminescence* l = world.registry().try_get<Luminescence>(e))
                best = std::max(best, l->glow);
        }
        world.clearStir();
        return best;
    };
    world.bioTimeScale() = 3600.0f; // ~1 h biológica después de 1 s: de día
    run(world, 1.0f);
    const float day = shake();
    run(world, 70.0f); // se recarga (y pasa a la noche subjetiva: ~71 h → hora 23)
    const float night = shake();
    CHECK(night > 0.5f);
    CHECK(day < 0.1f * night);
}

TEST_CASE("mecánicas: Paramecium bursaria vive de sus Chlorella con luz; a oscuras las pierde" * doctest::test_suite("largos")) {
    auto biomassAfter = [](float light, float& symbionts) {
        WorldConfig config;
        config.bacteria = 0;
        config.detritus = 0;
        config.seed = 7;
        config.medium.surfaceLight = light;
        World world(config);
        world.bioTimeScale() = 3600.0f;
        const entt::entity e = spawn(world, "paramecium_bursaria", center(world));
        world.registry().get<Locomotion>(e).power = 0.0f;
        const float before = world.registry().get<Metabolism>(e).biomass;
        run(world, 24.0f); // un día biológico sin comida
        if (!world.registry().valid(e))
            return 0.0f;
        symbionts = world.registry().get<Metabolism>(e).symbionts;
        return world.registry().get<Metabolism>(e).biomass / before;
    };
    float lit = 0.0f, dark = 0.0f;
    const float inLight = biomassAfter(1.0f, lit), inDark = biomassAfter(0.0f, dark);
    CHECK(inLight > inDark);
    CHECK(lit > dark);
    CHECK(dark < speciesInfo(*findSpecies("paramecium_bursaria")).photosymbiont.initial);
}

TEST_CASE("mecánicas: los cleptoplastos de Mesodinium se gastan y se reponen comiendo criptofitas") {
    const SpeciesInfo& meso = speciesInfo(*findSpecies("mesodinium"));
    MetabolismParams p;
    Metabolism m;
    m.biomass = 1.0f;
    m.symbionts = meso.photosymbiont.max;
    float sugar = 0.0f;
    for (int h = 0; h < 24 * 14; ++h)
        sugar += stepPhotosymbionts(meso, p, m, 1.0f, 3600.0f);
    CHECK(sugar > 0.0f);
    CHECK(m.symbionts < 0.5f * meso.photosymbiont.max); // dos semanas después
    const float before = m.symbionts;
    CHECK(acquireSymbionts(meso, *findSpecies("cryptomonas"), m));
    CHECK(m.symbionts > before);
    CHECK_FALSE(acquireSymbionts(meso, *findSpecies("chlorella"), m)); // de otra presa no los toma
}

TEST_CASE("mecánicas: los cuerpos R de un Paramecium killer matan a P. aurelia sensible y no a otros killer" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const Vec2 c = center(world);
    for (int k = 0; k < 6; ++k) {
        const entt::entity killer = spawn(world, "paramecium_tetraurelia", c + Vec2{30.0f * k, -40.0f});
        world.registry().get<Locomotion>(killer).power = 0.0f;
    }
    const entt::entity sensitive = spawn(world, "paramecium_aurelia", c + Vec2{60.0f, 40.0f});
    const entt::entity other = spawn(world, "paramecium_tetraurelia", c + Vec2{100.0f, 40.0f});
    world.registry().get<Locomotion>(sensitive).power = 0.0f;
    world.registry().get<Locomotion>(other).power = 0.0f;
    run(world, 30.0f); // ~30 h biológicas
    CHECK_FALSE(world.registry().valid(sensitive));
    CHECK(world.registry().valid(other));
}

TEST_CASE("mecánicas: Zoothamnium arma una colonia ramificada que se contrae entera; en Carchesium, de a uno") {
    auto colony = [](const char* key, int& contracted) {
        World world = emptyWorld();
        world.bioTimeScale() = 3600.0f;
        Rng rng(9, 1, 0, 0);
        const entt::entity e = world.spawnWild(*findSpecies(key), center(world), rng, false);
        REQUIRE(world.forceReproduction(e));
        int zooids = 1;
        for (int s = 0; s < 60 * 120 && zooids < 2; ++s) {
            world.step(kDt);
            zooids = static_cast<int>(world.registry().view<Anchored>().size());
        }
        // Las dos hijas quedaron fijas: una en el tallo y otra en una rama que sale de él.
        int branches = 0;
        for (auto [se, st] : world.registry().view<Stalk>().each())
            branches += world.registry().valid(st.parent) ? 1 : 0;
        // Una sacudida junto a uno de los zooides.
        const RingBody& rb = world.registry().get<RingBody>(e);
        for (int s = 0; s < 4; ++s) {
            world.setStir(rb.centroid + Vec2{0.0f, 25.0f}, {1500.0f, 0.0f}, 25.0f);
            world.step(kDt);
        }
        world.clearStir();
        contracted = 0;
        for (auto [se, st] : world.registry().view<Stalk>().each())
            contracted += st.rod.contracted > 0.5f ? 1 : 0;
        return branches;
    };
    int zooContracted = 0, carContracted = 0;
    CHECK(colony("zoothamnium", zooContracted) >= 1);
    CHECK(zooContracted >= 2); // la colonia entera
    CHECK(colony("carchesium", carContracted) >= 1);
    CHECK(carContracted >= 1);
}

TEST_CASE("mecánicas: la colonia de Bacillaria se estira y se cierra como una regla de carpintero" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "bacillaria", center(world));
    REQUIRE(world.forceReproduction(e));
    entt::entity child = entt::null;
    for (int s = 0; s < 60 * 240 && child == entt::null; ++s) {
        world.step(kDt);
        for (auto [c, sp] : world.registry().view<Species>().each())
            if (c != e && sp.id == *findSpecies("bacillaria"))
                child = c;
    }
    REQUIRE((child != entt::null));
    // A lo largo de un período, el corrimiento de la hija sobre el eje de la madre cambia de signo.
    float lo = 1e9f, hi = -1e9f;
    for (int s = 0; s < 60 * 60; ++s) {
        world.step(kDt);
        const RingBody& a = world.registry().get<RingBody>(e);
        const RingBody& b = world.registry().get<RingBody>(child);
        const float along = dot(b.centroid - a.centroid, bodyHeading(a));
        lo = std::min(lo, along);
        hi = std::max(hi, along);
    }
    CHECK(hi - lo > 40.0f);
}

TEST_CASE("mecánicas: la testa de Arcella no se deforma; avanza arrastrada por los seudópodos de la abertura" * doctest::test_suite("largos")) {
    World world = emptyWorld();
    const entt::entity e = spawn(world, "arcella", center(world));
    CHECK(world.registry().get<RingBody>(e).membraneReservoirRate <= 0.0f); // no es plástica
    const Vec2 start = world.registry().get<RingBody>(e).centroid;
    run(world, 30.0f);
    const RingBody& b = world.registry().get<RingBody>(e);
    CHECK(length(b.centroid - start) > 10.0f);
    // Sigue casi redonda: el radio de cada punto de la membrana no se aparta más de un 15 % del medio.
    float mean = 0.0f;
    for (std::uint32_t s : b.ring)
        mean += length(Vec2{world.bodies().x[s], world.bodies().y[s]} - b.centroid);
    mean /= static_cast<float>(b.ring.size());
    float worst = 0.0f;
    for (std::uint32_t s : b.ring)
        worst = std::max(worst, std::abs(length(Vec2{world.bodies().x[s], world.bodies().y[s]} - b.centroid) - mean));
    CHECK(worst < 0.15f * mean);
}

TEST_CASE("mecánicas: con hambre, Dictyostelium releva pulsos de AMPc y se agrega hacia un centro" * doctest::test_suite("largos")) {
    World world = emptyWorld(13);
    world.bioTimeScale() = 600.0f; // horas de hambre en segundos, sin apurar demasiado la migración
    const Vec2 c = center(world);
    std::vector<entt::entity> cells;
    Rng rng(4, 1, 0, 0);
    for (int k = 0; k < 40; ++k) {
        const float a = rng.uniform(0.0f, 6.2831853f), r = 300.0f * std::sqrt(rng.uniform());
        const entt::entity e = spawn(world, "dictyostelium", c + Vec2{std::cos(a), std::sin(a)} * r);
        world.registry().get<Metabolism>(e).reserves = 0.0f; // sin bacterias: pasan hambre
        cells.push_back(e);
    }
    auto spread = [&] {
        Vec2 mean;
        int n = 0;
        for (entt::entity e : cells)
            if (world.registry().valid(e)) {
                mean = mean + world.registry().get<RingBody>(e).centroid;
                ++n;
            }
        mean = mean * (1.0f / std::max(1, n));
        float d = 0.0f;
        for (entt::entity e : cells)
            if (world.registry().valid(e))
                d += length(world.registry().get<RingBody>(e).centroid - mean);
        return d / std::max(1, n);
    };
    run(world, 30.0f); // ~5 h biológicas: competentes
    int competent = 0;
    for (entt::entity e : cells)
        if (const Relay* r = world.registry().try_get<Relay>(e))
            competent += r->competent ? 1 : 0;
    CHECK(competent > 20);
    const float before = spread();
    run(world, 60.0f);
    CHECK(spread() < 0.7f * before);
}
