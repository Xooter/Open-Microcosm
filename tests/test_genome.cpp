#include "bio/genome.hpp"
#include "bio/organelles.hpp"
#include "game/progress.hpp"
#include "game/world.hpp"
#include "sim/soft_body.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {

constexpr float kDt = 1.0f / 60.0f;

WorldConfig geneWorld(std::size_t bacteria = 0) {
    WorldConfig config;
    config.bacteria = bacteria;
    config.detritus = 0;
    config.seed = 55;
    return config;
}

Genome with(LocomotionType type, Gene gene, float value) {
    Genome g = speciesGenome(type);
    g[gene] = value;
    return g;
}

// Velocidad media de nado del jugador durante 2 s.
float swimSpeed(LocomotionType type, const Genome& genome) {
    World world(geneWorld());
    const entt::entity e = world.spawnPlayer(type, {300.0f, 512.0f}, &genome);
    const RingBody& body = world.registry().get<RingBody>(e);
    PlayerInput input;
    input.forward = 1.0f;
    input.propel = true;
    input.aim = {1900.0f, 512.0f};
    for (int s = 0; s < 30; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    const Vec2 a = body.centroid;
    for (int s = 0; s < 60; ++s) {
        world.setPlayerInput(input);
        world.step(kDt);
    }
    // El mundo se envuelve: distancia por la imagen mínima.
    return length({minImage(body.centroid.x - a.x, world.width()), minImage(body.centroid.y - a.y, world.height())});
}

} // namespace

TEST_CASE("las mutaciones respetan los límites y no inventan organelos ajenos a la especie") {
    Rng rng(1, 2, 3, 4);
    for (LocomotionType type : {LocomotionType::Cilia, LocomotionType::Flagellum, LocomotionType::Pseudopod}) {
        Genome g = speciesGenome(type);
        const GeneticsSpec fast; // la escala grande lleva los genes contra sus límites
        for (int k = 0; k < 2000; ++k)
            mutateDivision(g, type, fast, 1e4f, rng);
        for (std::size_t i = 0; i < kGeneCount; ++i) {
            const auto gene = static_cast<Gene>(i);
            CHECK(g.v[i] >= geneInfo(gene).min - 1e-6f);
            CHECK(g.v[i] <= geneInfo(gene).max + 1e-6f);
            if (!geneAllowed(type, gene) && gene != Gene::MatingType)
                CHECK(g.v[i] == speciesGenome(type).v[i]);
        }
    }
}

TEST_CASE("genomas distintos se juegan distinto: motor y tamaño") {
    const float slow = swimSpeed(LocomotionType::Cilia, with(LocomotionType::Cilia, Gene::Motor, 0.5f));
    const float fast = swimSpeed(LocomotionType::Cilia, with(LocomotionType::Cilia, Gene::Motor, 2.0f));
    MESSAGE("ciliado motor 0,5: " << slow << " µm/s, motor 2: " << fast << " µm/s");
    CHECK(fast / slow == doctest::Approx(2.0f).epsilon(0.1)); // √(2 / 0,5)

    // Un flagelo empuja igual: una Euglena más grande es más lenta.
    const float small = swimSpeed(LocomotionType::Flagellum, with(LocomotionType::Flagellum, Gene::Size, 0.8f));
    const float big = swimSpeed(LocomotionType::Flagellum, with(LocomotionType::Flagellum, Gene::Size, 1.3f));
    MESSAGE("Euglena tamaño 0,8: " << small << " µm/s, tamaño 1,3: " << big << " µm/s");
    CHECK(big < 0.8f * small);
}

TEST_CASE("genomas distintos se juegan distinto: cloroplastos y lisosomas" * doctest::test_suite("largos")) {
    auto lightGrowth = [](float chloroplasts) {
        World world(geneWorld());
        world.bioTimeScale() = 600.0f;
        const Genome g = with(LocomotionType::Flagellum, Gene::Chloroplasts, chloroplasts);
        const entt::entity e = world.spawnPlayer(LocomotionType::Flagellum, {1024.0f, 512.0f}, &g);
        const Metabolism& m = world.registry().get<Metabolism>(e);
        const float start = m.biomass + m.reserves;
        for (int s = 0; s < 60 * 30; ++s)
            world.step(kDt);
        return (m.biomass + m.reserves) / start;
    };
    const float few = lightGrowth(0.5f), many = lightGrowth(2.0f);
    MESSAGE("Euglena con pocos cloroplastos x" << few << ", con muchos x" << many);
    CHECK(many > few * 1.2f);

    auto digestionLeft = [](float lysosomes) {
        World world(geneWorld(20'000));
        const Genome g = with(LocomotionType::Cilia, Gene::Lysosomes, lysosomes);
        const entt::entity e = world.spawnPlayer(LocomotionType::Cilia, {1024.0f, 512.0f}, &g);
        PlayerInput input;
        input.forward = 0.5f;
        input.turn = 0.4f;
        for (int s = 0; s < 600; ++s) {
            world.setPlayerInput(input);
            world.step(kDt);
        }
        const Digestion& d = world.registry().get<Digestion>(e);
        return d.pmolDigested / std::max(1e-6f, d.pmolIngested);
    };
    const float slowDig = digestionLeft(0.5f), fastDig = digestionLeft(2.0f);
    MESSAGE("fracción digerida: lisosomas 0,5 → " << slowDig << ", lisosomas 2 → " << fastDig);
    CHECK(fastDig > slowDig * 1.3f);
}

TEST_CASE("los organelos se ven y ocupan lugar según el genoma") {
    World world(geneWorld());
    const Genome rich = with(LocomotionType::Flagellum, Gene::Chloroplasts, 2.0f);
    const Genome poor = with(LocomotionType::Flagellum, Gene::Chloroplasts, 0.5f);
    const entt::entity a = world.spawnOrganism(LocomotionType::Flagellum, {500.0f, 500.0f}, rich);
    const entt::entity b = world.spawnOrganism(LocomotionType::Flagellum, {1500.0f, 500.0f}, poor);
    const entt::entity c = world.spawnOrganism(LocomotionType::Cilia, {1000.0f, 200.0f}, speciesGenome(LocomotionType::Cilia));
    const auto& reg = world.registry();
    const int chloroA = countOrganelles(reg.get<RingBody>(a), world.bodies(), OrganelleTag::Chloroplast);
    const int chloroB = countOrganelles(reg.get<RingBody>(b), world.bodies(), OrganelleTag::Chloroplast);
    CHECK(chloroA > 2 * chloroB);
    CHECK(countOrganelles(reg.get<RingBody>(c), world.bodies(), OrganelleTag::Chloroplast) == 0);
    CHECK(countOrganelles(reg.get<RingBody>(c), world.bodies(), OrganelleTag::Mitochondrion) > 0);
    for (int s = 0; s < 120; ++s)
        world.step(kDt);
    // Más organelos → más volumen: la Euglena rica en cloroplastos es más grande.
    CHECK(std::abs(reg.get<RingBody>(a).restArea) > std::abs(reg.get<RingBody>(b).restArea));
}

TEST_CASE("defensas: la coraza impide la fagocitosis y los tricocistos rechazan el ataque" * doctest::test_suite("largos")) {
    auto attack = [](LocomotionType preyType, const Genome& preyGenome, float preyX) {
        World world(geneWorld());
        world.spawnPlayer(LocomotionType::Pseudopod, {900.0f, 512.0f});
        world.setRun(2); // la presa es de otro linaje (el propio no se come)
        const entt::entity prey = world.spawnOrganism(preyType, {preyX, 512.0f}, preyGenome);
        world.setRun(1);
        bool discharged = false, cup = false;
        for (int s = 0; s < 60 * 40 && world.registry().valid(prey); ++s) {
            world.step(kDt);
            discharged |= world.registry().all_of<Discharge>(prey);
            cup |= world.registry().all_of<PhagocyticCup>(world.player());
        }
        return std::tuple{world.registry().valid(prey), discharged, cup};
    };
    // Euglena desnuda: la ameba la fagocita (es delgada: entra por contacto, sin copa visible a
    // esta resolución). Con lórica gruesa (Trachelomonas), no.
    const auto [nakedAlive, n1, nakedCup] = attack(LocomotionType::Flagellum, speciesGenome(LocomotionType::Flagellum), 1020.0f);
    const auto [armoredAlive, n2, armoredCup] =
        attack(LocomotionType::Flagellum, with(LocomotionType::Flagellum, Gene::Armor, 0.8f), 1020.0f);
    (void)nakedCup;
    CHECK_FALSE(nakedAlive);
    CHECK_FALSE(armoredCup);
    CHECK(armoredAlive);

    // Paramecium con muchos tricocistos: los dispara al sentir el ataque.
    const auto [alive, discharged, n3] =
        attack(LocomotionType::Cilia, with(LocomotionType::Cilia, Gene::Trichocysts, 2.0f), 1085.0f);
    CHECK(discharged);
    (void)alive;
}

TEST_CASE("conjugación: tipos de apareamiento distintos intercambian genes y quedan iguales") {
    World world(geneWorld());
    world.bioTimeScale() = 600.0f; // 2 h biológicas = 12 s reales
    Genome ga = speciesGenome(LocomotionType::Cilia), gb = ga;
    ga[Gene::Motor] = 1.8f;
    ga[Gene::Lysosomes] = 0.6f;
    gb[Gene::Motor] = 0.6f;
    gb[Gene::Lysosomes] = 1.8f;
    gb[Gene::MatingType] = 1.0f;
    const entt::entity a = world.spawnPlayer(LocomotionType::Cilia, {1000.0f, 470.0f}, &ga);
    const entt::entity b = world.spawnOrganism(LocomotionType::Cilia, {1000.0f, 535.0f}, gb);

    Genome same = ga;
    same[Gene::MatingType] = 0.0f;
    const entt::entity c = world.spawnOrganism(LocomotionType::Cilia, {1000.0f, 900.0f}, same);
    CHECK((world.conjugationPartner(c) == entt::null)); // nadie compatible cerca

    REQUIRE(world.tryConjugate(a));
    CHECK(world.registry().get<Conjugating>(a).partner == b);
    for (int s = 0; s < 60 * 15 && world.registry().all_of<Conjugating>(a); ++s)
        world.step(kDt);
    REQUIRE_FALSE(world.registry().all_of<Conjugating>(a));
    const Genome& ra = world.registry().get<Heredity>(a).genome;
    const Genome& rb = world.registry().get<Heredity>(b).genome;
    for (std::size_t i = 0; i < kGeneCount; ++i)
        if (static_cast<Gene>(i) != Gene::MatingType)
            CHECK(ra.v[i] == rb.v[i]);
    // Cada gen recombinado viene de uno de los dos progenitores.
    CHECK((ra[Gene::Motor] == 1.8f || ra[Gene::Motor] == 0.6f));
}


TEST_CASE("conjugación: solo entre células de la misma especie") {
    World world(geneWorld());
    Genome other = speciesGenome(SpeciesId::Euplotes);
    other[Gene::MatingType] = 1.0f; // tipo distinto: lo único que faltaría si fuera la misma especie
    const entt::entity a = world.spawnOrganism(SpeciesId::Paramecium, {1000.0f, 470.0f}, speciesGenome(SpeciesId::Paramecium), 0);
    world.spawnOrganism(SpeciesId::Euplotes, {1000.0f, 535.0f}, other, 0);
    CHECK((world.conjugationPartner(a) == entt::null));
    // Una especie que no se conjuga (alga) tampoco busca pareja.
    const entt::entity alga = world.spawnOrganism(SpeciesId::Chlamydomonas, {400.0f, 400.0f}, speciesGenome(SpeciesId::Chlamydomonas), 0);
    CHECK((world.conjugationPartner(alga) == entt::null));
}

TEST_CASE("mutación por división: agrega la varianza publicada por generación") {
    // Cada gen permitido cambia por N(0, CVm²) por división (CVm 0,5 %, Houle et al. 1996): después de
    // n divisiones, la varianza entre linajes independientes es n·CVm².
    const GeneticsSpec spec;
    constexpr int kLines = 4000, kGenerations = 50;
    double sum = 0.0, sum2 = 0.0;
    for (int line = 0; line < kLines; ++line) {
        Genome g = speciesGenome(LocomotionType::Flagellum);
        Rng rng(7, 1, static_cast<std::uint64_t>(line), 0);
        for (int k = 0; k < kGenerations; ++k)
            mutateDivision(g, LocomotionType::Flagellum, spec, 1.0f, rng);
        sum += g[Gene::Size];
        sum2 += g[Gene::Size] * g[Gene::Size];
    }
    const double mean = sum / kLines, var = sum2 / kLines - mean * mean;
    const double expected = kGenerations * spec.mutationalCv * spec.mutationalCv;
    MESSAGE("varianza del tamaño tras " << kGenerations << " divisiones: " << var << " (esperada " << expected << ")");
    CHECK(var == doctest::Approx(expected).epsilon(0.1));
    CHECK(mean == doctest::Approx(1.0).epsilon(0.01));
}

TEST_CASE("mutación por división: el macronúcleo de un ciliado no expresa mutaciones nuevas; la escala acelera") {
    GeneticsSpec ciliate;
    ciliate.macronucleus = true;
    Genome g = speciesGenome(LocomotionType::Cilia);
    const Genome before = g;
    Rng rng(3, 1, 0, 0);
    for (int k = 0; k < 500; ++k)
        mutateDivision(g, LocomotionType::Cilia, ciliate, 1.0f, rng);
    CHECK(g.v == before.v);
    // Escala 0: sin mutación; escala 100: desvío 10 veces mayor por división.
    const GeneticsSpec spec;
    Genome h = speciesGenome(LocomotionType::Flagellum);
    mutateDivision(h, LocomotionType::Flagellum, spec, 0.0f, rng);
    CHECK(h.v == speciesGenome(LocomotionType::Flagellum).v);
    double d1 = 0.0, d100 = 0.0;
    for (int k = 0; k < 2000; ++k) {
        Genome a = speciesGenome(LocomotionType::Flagellum), b = a;
        mutateDivision(a, LocomotionType::Flagellum, spec, 1.0f, rng);
        mutateDivision(b, LocomotionType::Flagellum, spec, 100.0f, rng);
        d1 += std::abs(a[Gene::Size] - 1.0f);
        d100 += std::abs(b[Gene::Size] - 1.0f);
    }
    CHECK(d100 / d1 == doctest::Approx(10.0).epsilon(0.1));
}

TEST_CASE("siembra: un inóculo de laboratorio es un clon; una gota silvestre tiene variación") {
    WorldConfig c;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    World w(c);
    REQUIRE(w.inoculate({BeingKind::Species, SpeciesId::Euglena}, 20, {}, 0.0f) == 20);
    for (auto [e, h, sp] : w.registry().view<Heredity, Species>().each())
        CHECK(h.genome[Gene::Size] == 1.0f);
    World wild(c);
    REQUIRE(wild.inoculate({BeingKind::Species, SpeciesId::Euglena}, 40, {}, 0.0f, {}, 1.0f) == 40);
    double sum = 0.0, sum2 = 0.0;
    int n = 0;
    for (auto [e, h, sp] : wild.registry().view<Heredity, Species>().each()) {
        sum += h.genome[Gene::Size];
        sum2 += h.genome[Gene::Size] * h.genome[Gene::Size];
        ++n;
    }
    const double sd = std::sqrt(sum2 / n - (sum / n) * (sum / n));
    MESSAGE("desvío del tamaño en una población silvestre: " << sd << " (standing_cv " << speciesInfo(SpeciesId::Euglena).genetics.standingCv << ")");
    CHECK(sd > 0.03);
    CHECK(sd < 0.07);
}
