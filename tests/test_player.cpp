#include "bio/physiology.hpp"
#include "core/finite.hpp"
#include "bio/feeding.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "bio/species.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cmath>
#include <string>

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
    // A la izquierda del centro de la placa, nadando hacia +x: ~900 µm sin llegar a la pared.
    const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {624.0f, 1024.0f});
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

TEST_CASE("el ciliado come bacterias por la boca respetando el caudal del citostoma" * doctest::test_suite("largos")) {
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
    // El caudal del citostoma: la ingestión medida de la especie (fisiologia.toml) o el general.
    const SourcedValue& measured = speciesPhysiology(playerSpecies(LocomotionType::Cilia)).maxIngestion;
    const float maxRate = measured.present ? measured.value / 3600.0f : world.feedingParams().mouthMaxRate;
    CHECK(dig.bacteriaEaten > static_cast<int>(0.5f * 10.0f * maxRate));
    CHECK(dig.bacteriaEaten <= static_cast<int>(10.0f * maxRate) + 31);
    CHECK_FALSE(dig.vacuoles.empty());
}

TEST_CASE("la ameba envuelve una célula y la encierra en un fagosoma" * doctest::test_suite("largos")) {
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

TEST_CASE("jugador y fagocitosis deterministas sin importar la cantidad de hilos" * doctest::test_suite("largos")) {
    const std::uint64_t a = runPlayer(1);
    CHECK(a == runPlayer(1));
    CHECK(a == runPlayer(4));
}

namespace {
// Vigila la membrana de la ameba durante toda la copa y después.
struct RingHealth {
    int crossings = 0, outside = 0;
    bool finite = true;
};

RingHealth ringHealth(const RingBody& body, const BodyParticles& bp) {
    RingHealth h;
    h.crossings = membraneCrossings(body, bp);
    std::vector<Vec2> poly;
    for (std::uint32_t s : body.ring) {
        poly.push_back({bp.x[s], bp.y[s]});
        h.finite = h.finite && finiteNumber(bp.x[s]) && finiteNumber(bp.y[s]);
    }
    for (std::uint32_t s : body.interior)
        if (!polygon::contains(poly.data(), static_cast<int>(poly.size()), {bp.x[s], bp.y[s]}))
            ++h.outside;
    return h;
}
} // namespace

namespace {
// La membrana de la ameba no se cruza al comer presas de distinta forma. Cada presa es un caso
// aparte (corren en paralelo con ctest -j; antes eran 9 corridas seguidas en un solo caso).
// `slack`: cruces tolerados durante la copa (una punta de 2 µm de radio se deforma sola).
struct EatCase { const char* name; float rx, ry; int count; int slack; int species = -1; bool mustEat = true; };

void amoebaEats(const EatCase& c) {
    CAPTURE(std::string(c.name));
    World world(quietWorld(300));
    const entt::entity e = world.spawnPlayer(LocomotionType::Pseudopod, {900.0f, 512.0f});
    float edge = 0.0f; // borde derecho de la ameba
    for (std::uint32_t slot : world.registry().get<RingBody>(e).ring)
        edge = std::max(edge, world.bodies().x[slot]);
    for (int k = 0; k < c.count; ++k) {
        // A ~2 µm de la membrana: en contacto, sin solapar; apiladas a lo largo del borde.
        float rx = c.rx, ry = c.ry;
        if (c.species >= 0) {
            rx = speciesInfo(static_cast<SpeciesId>(c.species)).spec.radiusX;
            ry = speciesInfo(static_cast<SpeciesId>(c.species)).spec.radiusY;
        }
        const Vec2 where = {edge + 2.0f + rx, 512.0f + (2.0f * ry + 1.0f) * (static_cast<float>(k) - 0.5f * static_cast<float>(c.count - 1))};
        if (c.species >= 0) {
            const auto id = static_cast<SpeciesId>(c.species);
            world.spawnOrganism(id, where, speciesGenome(speciesInfo(id).locomotion), 0);
            continue;
        }
        CellSpec prey;
        prey.center = where;
        prey.radiusX = c.rx;
        prey.radiusY = c.ry;
        world.spawnCell(prey);
    }
    int worstCross = 0, worstOutside = 0;
    bool finite = true;
    for (int s = 0; s < 60 * 45; ++s) {
        world.step(kDt);
        const RingHealth h = ringHealth(world.registry().get<RingBody>(e), world.bodies());
        finite = finite && h.finite;
        worstCross = std::max(worstCross, h.crossings);
        worstOutside = std::max(worstOutside, h.outside);
    }
    const RingHealth end = ringHealth(world.registry().get<RingBody>(e), world.bodies());
    CHECK(finite);
    if (c.mustEat) // con vecinas pegadas, o una punta de 2 µm, la copa puede no encontrar lugar
        CHECK(world.registry().get<Digestion>(e).cellsEaten >= 1);
    CHECK(worstCross <= c.slack);
    CHECK(worstOutside <= c.slack);
    CHECK(end.crossings == 0);
    CHECK(end.outside == 0);
}
} // namespace

TEST_CASE("la membrana de la ameba no se cruza al comer: elipse" * doctest::test_suite("largos")) { amoebaEats({"elipse", 22, 18, 1, 0}); }
TEST_CASE("la membrana de la ameba no se cruza al comer: grande" * doctest::test_suite("largos")) { amoebaEats({"grande", 40, 30, 1, 0}); }
TEST_CASE("la membrana de la ameba no se cruza al comer: chica" * doctest::test_suite("largos")) { amoebaEats({"chica", 8, 6, 1, 0}); }
TEST_CASE("la membrana de la ameba no se cruza al comer: alargada" * doctest::test_suite("largos")) { amoebaEats({"alargada", 50, 14, 1, 20}); }
TEST_CASE("la membrana de la ameba no se cruza al comer: punta" * doctest::test_suite("largos")) { amoebaEats({"punta", 50, 10, 1, 6, -1, false}); }
// Con vecinas pegadas, la copa puede no encontrar lugar.
TEST_CASE("la membrana de la ameba no se cruza al comer: dos" * doctest::test_suite("largos")) { amoebaEats({"dos", 22, 18, 2, 0, -1, false}); }
TEST_CASE("la membrana de la ameba no se cruza al comer: tres" * doctest::test_suite("largos")) { amoebaEats({"tres", 18, 14, 3, 0, -1, false}); }
// Presas vivas: se mueven y huyen; solo importa que la membrana aguante.
TEST_CASE("la membrana de la ameba no se cruza al comer: euglena" * doctest::test_suite("largos")) {
    amoebaEats({"euglena", 0, 0, 3, 0, static_cast<int>(SpeciesId::Euglena), false});
}
TEST_CASE("la membrana de la ameba no se cruza al comer: paramecio" * doctest::test_suite("largos")) {
    amoebaEats({"paramecio", 0, 0, 3, 2, static_cast<int>(SpeciesId::Paramecium), false});
}

TEST_CASE("una célula arrastrada a través de una ameba no la parte ni la atraviesa") {
    World world(quietWorld());
    const entt::entity ameba = world.spawnOrganism(SpeciesId::Amoeba, {900.0f, 512.0f}, speciesGenome(LocomotionType::Pseudopod), 0);
    CellSpec other;
    other.center = {700.0f, 512.0f};
    other.radiusX = 22.0f;
    other.radiusY = 18.0f;
    const entt::entity cell = world.spawnCell(other);
    // Se agarra el borde derecho de la célula y se lleva hacia +x a 300 µm/s, a través de la ameba.
    REQUIRE(world.beginGrab({722.0f, 512.0f}));
    float target = 722.0f, areaMin = 1e9f, areaStart = 0.0f;
    int worstCross = 0, worstOutside = 0, inside = 0;
    for (int s = 0; s < 60 * 4; ++s) {
        target += 300.0f / 60.0f;
        world.moveGrab({target, 512.0f});
        world.step(kDt);
        const RingBody& a = world.registry().get<RingBody>(ameba);
        const RingBody& c = world.registry().get<RingBody>(cell);
        if (s == 0)
            areaStart = soft_body::area(a, world.bodies());
        areaMin = std::min(areaMin, soft_body::area(a, world.bodies()));
        worstCross = std::max({worstCross, membraneCrossings(a, world.bodies()), membraneCrossings(c, world.bodies())});
        worstOutside = std::max(worstOutside, ringHealth(a, world.bodies()).outside);
        // Puntos de la membrana de la célula dentro de la ameba = se atravesaron.
        std::vector<Vec2> poly;
        for (std::uint32_t slot : a.ring)
            poly.push_back({world.bodies().x[slot], world.bodies().y[slot]});
        int n = 0;
        for (std::uint32_t slot : c.ring)
            n += polygon::contains(poly.data(), static_cast<int>(poly.size()), {world.bodies().x[slot], world.bodies().y[slot]});
        inside = std::max(inside, n);
    }
    MESSAGE("área mínima " << areaMin / areaStart << " del inicio; cruces " << worstCross << "; puntos de la célula dentro "
                          << inside << "; interior fuera " << worstOutside);
    CHECK(worstCross == 0);
    CHECK(inside == 0);
    CHECK(areaMin > 0.7f * areaStart);
}

TEST_CASE("un ciliado rápido que embiste a una ameba no la atraviesa") {
    World world(quietWorld());
    const entt::entity ameba = world.spawnOrganism(SpeciesId::Amoeba, {900.0f, 512.0f}, speciesGenome(LocomotionType::Pseudopod), 0);
    const entt::entity swimmer = world.spawnPlayer(LocomotionType::Cilia, {620.0f, 512.0f}); // ~110 µm de la ameba
    PlayerInput input;
    input.forward = 1.0f;
    int worstCross = 0, inside = 0;
    for (int s = 0; s < 60 * 6; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
        if (!world.registry().valid(ameba) || !world.registry().valid(swimmer))
            break; // fagocitado: no es un atravesamiento
        const RingBody& a = world.registry().get<RingBody>(ameba);
        const RingBody& c = world.registry().get<RingBody>(swimmer);
        if (world.registry().all_of<PhagocyticCup>(ameba))
            continue; // la copa envuelve a propósito
        // La membrana del ciliado no se mide: su propio cuerpo ya se cruza sola en este escenario.
        worstCross = std::max(worstCross, membraneCrossings(a, world.bodies()));
        std::vector<Vec2> poly;
        for (std::uint32_t slot : a.ring)
            poly.push_back({world.bodies().x[slot], world.bodies().y[slot]});
        int n = 0;
        for (std::uint32_t slot : c.ring)
            n += polygon::contains(poly.data(), static_cast<int>(poly.size()), {world.bodies().x[slot], world.bodies().y[slot]});
        inside = std::max(inside, n);
    }
    MESSAGE("cruces " << worstCross << "; puntos del ciliado dentro de la ameba " << inside);
    CHECK(worstCross == 0);
    CHECK(inside == 0);
}
