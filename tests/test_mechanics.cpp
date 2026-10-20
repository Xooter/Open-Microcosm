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

TEST_CASE("mecánicas: Stentor se extiende, se contrae entero ante una sacudida y se vuelve a extender") {
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
    run(world, 1.0f);
    const float calmBend = bend(stalkOf(world, e)->rod.nodes);
    // Una corriente lateral a la mitad del tallo (sin tocar al zooide) lo dobla; el largo no cambia.
    auto pushMiddle = [&](float seconds) {
        for (int s = 0; s < static_cast<int>(seconds / kDt); ++s) {
            const std::vector<Vec2>& n = stalkOf(world, e)->rod.nodes;
            const Vec2 mid = n[n.size() / 2], axis = n.back() - n.front();
            const Vec2 side = Vec2{-axis.y, axis.x} * (1.0f / std::max(1e-3f, length(axis)));
            world.setStir(mid, side * 150.0f, 25.0f);
            world.step(kDt);
        }
        world.clearStir();
    };
    pushMiddle(0.5f);
    const Stalk* bent = stalkOf(world, e);
    CHECK(bend(bent->rod.nodes) > calmBend + 5.0f);
    CHECK(polyLength(bent->rod.nodes) == doctest::Approx(stalkRestLength(bent->rod, speciesInfo(bent->species).stalk)).epsilon(0.03));
    // Sin corriente vuelve a quedar derecho.
    run(world, 3.0f);
    CHECK(bend(stalkOf(world, e)->rod.nodes) < calmBend + 3.0f);
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

TEST_CASE("mecánicas: el telotroco secreta su tallo al fijarse; muerto el zooide, el tallo queda y se degrada") {
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

TEST_CASE("mecánicas: Stentor se fija por el pie y con hambre lo suelta, nada y se vuelve a fijar") {
    World world = emptyWorld();
    world.bioTimeScale() = 3600.0f;
    const entt::entity e = wild(world, "stentor", center(world));
    REQUIRE(world.registry().all_of<Anchored>(e));
    CHECK_FALSE(world.registry().valid(world.registry().get<Anchored>(e).rod)); // sin tallo
    // Sin bacterias pasa hambre: en algunas horas biológicas se suelta.
    world.registry().get<Metabolism>(e).reserves = 0.0f;
    bool released = false, reattached = false;
    for (int s = 0; s < 60 * 60 && !reattached; ++s) {
        world.step(kDt);
        if (!world.registry().valid(e))
            break;
        released = released || world.registry().all_of<Telotroch>(e);
        reattached = released && world.registry().all_of<Anchored>(e);
    }
    CHECK(released);
    CHECK(reattached);
}
