// Hongos nuevos (Allomyces, hongo liquenizante), Physarum y la placa de Petri vacía.
#include "bio/fungi.hpp"
#include "bio/plasmodium.hpp"
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
#include "game/inspect.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <omp.h>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig quiet() {
    WorldConfig c;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.seed = 91;
    return c;
}

void steps(World& world, int n) {
    for (int s = 0; s < n; ++s)
        world.step(kDt);
}

// Entorno falso: sin agua; el fósforo siempre alcanza.
PlasmodiumEnv fakeEnv(Vec2 target) {
    PlasmodiumEnv env;
    env.food = [](Vec2) { return 0.0f; };
    env.takeFood = [](Vec2, float) { return 0.0f; };
    env.takePhosphate = [](Vec2, float pmol) { return pmol; };
    env.release = [](Vec2, float, float) {};
    env.cue = [target](Vec2 at) {
        const Vec2 d = target - at;
        const float n = length(d);
        return n > 1e-3f ? d * (1.0f / n) : Vec2{};
    };
    return env;
}

// Camino corto (S–a–b–T, recto) y un desvío largo (S–c–T).
Plasmodium twoRoutes() {
    Plasmodium pl;
    const float xs[] = {0, 33, 66, 100, 50}, ys[] = {0, 0, 0, 0, 80};
    for (int i = 0; i < 5; ++i) {
        pl.x.push_back(xs[i]);
        pl.y.push_back(ys[i]);
        pl.heading.push_back(0.0f);
        pl.tip.push_back(0);
        pl.food.push_back(-1);
        pl.fed.push_back(1);
    }
    auto edge = [&](int a, int b) {
        const float len = std::hypot(pl.x[static_cast<std::size_t>(a)] - pl.x[static_cast<std::size_t>(b)],
                                     pl.y[static_cast<std::size_t>(a)] - pl.y[static_cast<std::size_t>(b)]);
        pl.edges.push_back({a, b, len, 0.5f, 0.0f});
    };
    edge(0, 1);
    edge(1, 2);
    edge(2, 3);
    edge(0, 4);
    edge(4, 3);
    return pl;
}

} // namespace

TEST_CASE("Physarum: el flujo cumple Kirchhoff y se reparte según la conductancia") {
    PlasmodiumSystem sys;
    Plasmodium pl = twoRoutes();
    REQUIRE(sys.solveFlow(pl, 0, 3, 1.0f));
    // Sale todo el caudal de la fuente: la suma de lo que sale por sus tubos es 1.
    CHECK(pl.edges[0].flux + pl.edges[3].flux == doctest::Approx(1.0f).epsilon(1e-3));
    // Los tubos de un mismo camino llevan lo mismo (nodos intermedios sin fuga).
    CHECK(pl.edges[0].flux == doctest::Approx(pl.edges[1].flux).epsilon(1e-3));
    CHECK(pl.edges[1].flux == doctest::Approx(pl.edges[2].flux).epsilon(1e-3));
    // El camino corto es el de menor resistencia: lleva más.
    CHECK(pl.edges[0].flux > pl.edges[3].flux);
    CHECK(PlasmodiumSystem::cycles(pl) == 1);
}

TEST_CASE("Physarum: la red se queda con el camino más corto (Nakagaki 2000)") {
    PlasmodiumSystem sys;
    Plasmodium pl = twoRoutes();
    for (int i = 0; i < 200; ++i) {
        REQUIRE(sys.solveFlow(pl, 0, 3, 1.0f));
        sys.adapt(pl, 0.5f);
    }
    MESSAGE("camino corto D=" << pl.edges[0].D << ", desvío D=" << pl.edges[3].D);
    CHECK(pl.edges[0].D > 0.4f);
    CHECK(pl.edges[1].D > 0.4f);
    CHECK(pl.edges[3].D < 0.05f);
    CHECK(pl.edges[4].D < 0.05f);
}

TEST_CASE("Physarum: sin conexión entre fuente y sumidero no hay flujo") {
    PlasmodiumSystem sys;
    Plasmodium pl = twoRoutes();
    pl.x.push_back(300.0f); // nodo suelto
    pl.y.push_back(0.0f);
    pl.heading.push_back(0.0f);
    pl.tip.push_back(0);
    pl.food.push_back(-1);
    pl.fed.push_back(1);
    CHECK_FALSE(sys.solveFlow(pl, 0, 5, 1.0f));
}

TEST_CASE("Physarum: explora hacia la comida, une dos copos con tubos y es determinista") {
    auto run = [] {
        PlasmodiumSystem sys;
        sys.addFlake({500.0f, 500.0f}, 400.0f, 0.1f);
        sys.addFlake({700.0f, 500.0f}, 400.0f, 0.1f);
        sys.spawn({500.0f, 500.0f}, 0.0f, 20.0f, 0.5f);
        const PlasmodiumEnv env = fakeEnv({700.0f, 500.0f});
        for (int s = 0; s < 300; ++s)
            sys.step(env, 0.04f * 3600.0f, 5, static_cast<std::uint64_t>(s)); // 0,04 h bio por paso
        return sys;
    };
    const PlasmodiumSystem a = run(), b = run();
    REQUIRE(a.plasmodia.size() == 1);
    const Plasmodium& pl = a.plasmodia[0];
    // Algún nodo toca cada copo y los dos están conectados por la red.
    std::vector<int> comp(pl.x.size(), -1);
    std::vector<std::int32_t> stack;
    int touching[2] = {-1, -1};
    for (std::size_t k = 0; k < pl.x.size(); ++k)
        for (int f = 0; f < 2; ++f)
            if (std::abs(pl.x[k] - (500.0f + 200.0f * static_cast<float>(f))) < 22.0f && std::abs(pl.y[k] - 500.0f) < 22.0f)
                touching[f] = static_cast<int>(k);
    REQUIRE(touching[0] >= 0);
    REQUIRE(touching[1] >= 0);
    stack.push_back(touching[0]);
    comp[static_cast<std::size_t>(touching[0])] = 0;
    while (!stack.empty()) {
        const auto u = stack.back();
        stack.pop_back();
        for (const Plasmodium::Edge& e : pl.edges) {
            const std::int32_t v = e.a == u ? e.b : (e.b == u ? e.a : -1);
            if (v >= 0 && comp[static_cast<std::size_t>(v)] < 0) {
                comp[static_cast<std::size_t>(v)] = 0;
                stack.push_back(v);
            }
        }
    }
    CHECK(comp[static_cast<std::size_t>(touching[1])] == 0);
    // Determinismo: dos corridas con la misma semilla dan exactamente la misma red.
    REQUIRE(b.plasmodia.size() == 1);
    CHECK(a.plasmodia[0].x == b.plasmodia[0].x);
    CHECK(a.plasmodia[0].y == b.plasmodia[0].y);
    CHECK(a.plasmodia[0].edges.size() == b.plasmodia[0].edges.size());
}

TEST_CASE("Physarum: sin comida se endurece en esclerocio y revive cuando vuelve") {
    PlasmodiumSystem sys;
    sys.spawn({400.0f, 400.0f}, 0.0f, 12.0f, 0.2f);
    float food = 0.0f;
    PlasmodiumEnv env = fakeEnv({400.0f, 400.0f});
    env.food = [&](Vec2) { return food; };
    for (int s = 0; s < 800 && sys.sclerotia.size() == 0; ++s)
        sys.step(env, 0.05f * 3600.0f, 3, static_cast<std::uint64_t>(s));
    REQUIRE(sys.sclerotia.size() == 1);
    CHECK(sys.plasmodia.empty());
    CHECK(sys.stats.sclerotiaFormed == 1);
    food = 50.0f; // vuelve la comida
    for (int s = 0; s < 400 && sys.plasmodia.empty(); ++s)
        sys.step(env, 0.05f * 3600.0f, 3, 1000 + static_cast<std::uint64_t>(s));
    CHECK(sys.plasmodia.size() == 1);
    CHECK(sys.sclerotia.size() == 0);
    CHECK(sys.stats.sclerotiaGerminated == 1);
}

TEST_CASE("Physarum en el mundo: crece sobre copos de avena y conserva el fósforo" * doctest::test_suite("largos")) {
    World world(quiet());
    world.bioTimeScale() = 600.0f;
    world.spawnBeing({BeingKind::OatFlake, SpeciesId::Paramecium}, {900.0f, 1024.0f});
    world.spawnBeing({BeingKind::OatFlake, SpeciesId::Paramecium}, {1100.0f, 1024.0f});
    world.spawnBeing({BeingKind::Physarum, SpeciesId::Paramecium}, {900.0f, 1024.0f});
    const double phosphorus = world.totalPhosphorus();
    steps(world, 60 * 30);
    const Census c = world.census();
    MESSAGE(c.plasmodia << " plasmodios, " << c.plasmodiumNodes << " nodos, " << c.plasmodiumEdges << " tubos");
    CHECK(c.plasmodiumNodes > 5);
    CHECK(world.totalPhosphorus() == doctest::Approx(phosphorus).epsilon(0.01));
}

TEST_CASE("Allomyces: se bifurca en dos y con hambre forma esporangios de resistencia que germinan") {
    Fungi fungi;
    fungi.spawnMycelium(FungusKind::Allomyces, {500.0f, 500.0f}, 0.0f, 40.0f, 1.0f);
    float food = 0.0f;
    FungiEnv env;
    env.food = [&](Vec2) { return food; };
    env.takeFood = [](Vec2, float) { return 0.0f; };
    env.takePhosphate = [](Vec2, float pmol) { return pmol; };
    env.release = [](Vec2, float, float) {};
    env.substrate = [](Vec2) { return false; };
    const float bioDt = 0.05f * 3600.0f;
    bool forked = false;
    for (int s = 0; s < 40; ++s) {
        fungi.grow(env, bioDt, 1, static_cast<std::uint64_t>(s));
        const Mycelium& m = fungi.mycelia.empty() ? Mycelium{} : fungi.mycelia[0];
        std::vector<int> kids(m.x.size(), 0);
        for (std::int32_t p : m.parent)
            if (p >= 0)
                ++kids[static_cast<std::size_t>(p)];
        for (int k : kids)
            forked |= k >= 2;
    }
    CHECK(forked);
    for (int s = 40; s < 600 && fungi.stats.restingFormed == 0; ++s)
        fungi.grow(env, bioDt, 1, static_cast<std::uint64_t>(s));
    CHECK(fungi.stats.restingFormed > 0);
    CHECK(fungi.resting.size() > 0);
    food = 30.0f;
    for (int s = 600; s < 800 && fungi.stats.restingGerminated == 0; ++s)
        fungi.grow(env, bioDt, 1, static_cast<std::uint64_t>(s));
    CHECK(fungi.stats.restingGerminated > 0);
    CHECK(fungi.spores.size() > 0);
}

TEST_CASE("liquen: la hifa se une a un alga verde, recibe su azúcar y la protege" * doctest::test_suite("largos")) {
    World world(quiet());
    world.bioTimeScale() = 600.0f;
    const entt::entity alga = world.spawnOrganism(SpeciesId::Chlamydomonas, {1000.0f, 1000.0f}, speciesGenome(SpeciesId::Chlamydomonas), 0);
    // A 45 µm: en agua sin materia orgánica, las reservas de la espora alcanzan para ~50 µm de hifa
    // (a 60 µm la punta quedaba a 5 µm del alga, justo fuera del contacto, si el alga no crecía).
    world.spawnFungus(FungusKind::Mycobiont, {1045.0f, 1000.0f}, 3.14159f, 0);
    const double phosphorus = world.totalPhosphorus();
    for (int s = 0; s < 60 * 90 && !world.registry().all_of<Lichenized>(alga); ++s)
        world.step(kDt);
    REQUIRE(world.registry().valid(alga));
    REQUIRE(world.registry().all_of<Lichenized>(alga));
    CHECK(world.observations().lichenLinks > 0);
    steps(world, 60 * 20);
    CHECK(world.registry().get<Lichenized>(alga).transferred > 0.0f);
    CHECK(world.totalPhosphorus() == doctest::Approx(phosphorus).epsilon(0.01));
    // Protegida: una zoospora de quitridio pegada al alga no la infecta.
    world.fungi().spawnSpores(FungusKind::Chytrid, world.registry().get<RingBody>(alga).centroid, 5, 0.0f, 1, 1);
    steps(world, 60 * 10);
    CHECK_FALSE(world.registry().all_of<ChytridInfection>(alga));
}

TEST_CASE("placa de Petri vacía: sin vida, sin minerales, sin granos ni polvo" * doctest::test_suite("largos")) {
    WorldConfig full = makeDrop(BiomeId::Puddle, 4);
    REQUIRE(full.sediment[MineralKind::Clay] > 0);
    WorldConfig c = full;
    applyDropContent(c, DropContent::EmptyPetri);
    CHECK(c.sediment[MineralKind::Clay] == 0);
    CHECK(c.sediment.rocks == 0);
    CHECK(c.medium.dustRate == 0.0f);
    CHECK(c.bacteria == 0);
    CHECK(c.detritus == 0);
    CHECK(c.physarum == 0);
    World world(c);
    steps(world, 60 * 5);
    CHECK(world.rockCount() == 0);
    for (std::size_t i = 0; i < world.ambient().size(); ++i)
        CHECK(world.ambient().kind[i] != ParticleKind::Mineral);
    // "Sin vida" conserva el sedimento del bioma.
    WorldConfig nl = full;
    applyDropContent(nl, DropContent::NoLife);
    CHECK(nl.bacteria == 0);
    CHECK(nl.sediment[MineralKind::Clay] == full.sediment[MineralKind::Clay]);
}

namespace {
bool asciiOnly(const Inspection& ins) {
    auto ok = [](const std::string& t) {
        for (unsigned char c : t)
            if (c > 127)
                return false;
        return true;
    };
    if (!ok(ins.title) || !ok(ins.subtitle) || !ok(ins.blurb) || !ok(ins.headline))
        return false;
    for (const InspectSection& sec : ins.sections) {
        if (!ok(sec.title))
            return false;
        for (const InspectLine& l : sec.lines)
            if (!ok(l.label) || !ok(l.value))
                return false;
    }
    return true;
}

std::uint64_t runMolds(int threads) {
    omp_set_num_threads(threads);
    World world(quiet());
    world.bioTimeScale() = 600.0f;
    world.spawnBeing({BeingKind::OatFlake, SpeciesId::Paramecium}, {900.0f, 1024.0f});
    world.spawnBeing({BeingKind::Physarum, SpeciesId::Paramecium}, {900.0f, 1024.0f});
    world.spawnBeing({BeingKind::Allomyces, SpeciesId::Paramecium}, {1200.0f, 1100.0f});
    world.spawnBeing({BeingKind::Lichen, SpeciesId::Paramecium}, {1100.0f, 900.0f});
    world.spawnOrganism(SpeciesId::Chlamydomonas, {1100.0f, 940.0f}, speciesGenome(SpeciesId::Chlamydomonas), 0);
    steps(world, 60 * 15);
    return world.stateHash();
}
} // namespace

TEST_CASE("Physarum, Allomyces y liquen: deterministas sin importar la cantidad de hilos" * doctest::test_suite("largos")) {
    const std::uint64_t a = runMolds(1);
    CHECK(a == runMolds(4));
}

TEST_CASE("fichas de Physarum, copo, esporangio y esclerocio: se eligen, se arman y son ASCII") {
    World world(quiet());
    world.bioTimeScale() = 600.0f;
    world.spawnBeing({BeingKind::OatFlake, SpeciesId::Paramecium}, {900.0f, 1024.0f});
    world.spawnBeing({BeingKind::Physarum, SpeciesId::Paramecium}, {900.0f, 1024.0f});
    steps(world, 60 * 10);
    REQUIRE(world.plasmodia().plasmodia.size() == 1);
    const Plasmodium& pl = world.plasmodia().plasmodia[0];
    REQUIRE(world.pick({pl.x[0], pl.y[0]}, 6.0f)); // algo se puede elegir ahí (el copo o el plasmodio)
    Picked picked;
    picked.kind = Picked::Kind::Plasmodium;
    picked.id = pl.id;
    picked.node = 0;
    const Inspection ins = buildInspection(world, picked);
    CHECK(ins.valid);
    CHECK(asciiOnly(ins));
    CHECK(ins.subtitle.find("no es un hongo") != std::string::npos);
    Picked flake;
    flake.kind = Picked::Kind::OatFlake;
    flake.index = 0;
    CHECK(asciiOnly(buildInspection(world, flake)));
    // Esporangio de resistencia y esclerocio a mano.
    world.fungi().resting.x.push_back(500.0f);
    world.fungi().resting.y.push_back(500.0f);
    world.fungi().resting.ageHours.push_back(1.0f);
    world.fungi().resting.pmol.push_back(1.0f);
    world.fungi().resting.phosphorus.push_back(0.01f);
    world.fungi().resting.kind.push_back(FungusKind::Allomyces);
    Picked rs;
    rs.kind = Picked::Kind::Resting;
    rs.index = 0;
    CHECK(asciiOnly(buildInspection(world, rs)));
    world.plasmodia().sclerotia.x.push_back(600.0f);
    world.plasmodia().sclerotia.y.push_back(600.0f);
    world.plasmodia().sclerotia.ageHours.push_back(1.0f);
    world.plasmodia().sclerotia.pmol.push_back(1.0f);
    world.plasmodia().sclerotia.phosphorus.push_back(0.01f);
    Picked sc;
    sc.kind = Picked::Kind::Sclerotium;
    sc.index = 0;
    CHECK(asciiOnly(buildInspection(world, sc)));
    // Nombres de enciclopedia de los hongos nuevos.
    CHECK(std::string(otherName(otherOf(FungusKind::Allomyces))).find("Allomyces") != std::string::npos);
    CHECK_FALSE(std::string(otherBlurb(OtherOrganism::Physarum)).empty());
    CHECK(std::string(otherBlurb(OtherOrganism::Physarum)).find("NO es un hongo") != std::string::npos);
}
