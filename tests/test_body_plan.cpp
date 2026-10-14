// Fase multiescala 2: plan corporal (qué estructuras tiene cada ser), escala biológica, miga de pan
// e identidad estable de las bacterias.
#include "bio/body_plan.hpp"
#include "game/inspect.hpp"
#include "game/scale_path.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <set>

TEST_CASE("Plan corporal: cada bacteria tiene la envoltura de su tipo de Gram") {
    const BodyPlan& ecoli = bacteriumPlan(0);
    const BodyPlan& violaceum = bacteriumPlan(1);
    const BodyPlan& bacillus = bacteriumPlan(2);
    CHECK(ecoli.has(StructureKind::OuterMembrane));
    CHECK(violaceum.has(StructureKind::OuterMembrane));
    CHECK_FALSE(bacillus.has(StructureKind::OuterMembrane)); // grampositiva: sin membrana externa
    // La pared de la grampositiva es mucho más gruesa.
    CHECK(bacillus.find(StructureKind::Peptidoglycan)->width > 4.0f * ecoli.find(StructureKind::Peptidoglycan)->width);
    // Ninguna bacteria tiene organelos con membrana.
    for (const BodyPlan* p : {&ecoli, &violaceum, &bacillus, &bdellovibrioPlan()}) {
        CHECK_FALSE(p->has(StructureKind::Nucleus));
        CHECK_FALSE(p->has(StructureKind::Mitochondrion));
        CHECK(p->has(StructureKind::Nucleoid));
        CHECK(p->find(StructureKind::Flagellum)->width < kOpticalResolution); // no se ven con luz
    }
    CHECK(bdellovibrioPlan().find(StructureKind::Flagellum)->arrangement == Arrangement::Polar);
    CHECK(ecoli.find(StructureKind::Flagellum)->arrangement == Arrangement::Peritrichous);
}

TEST_CASE("Plan corporal: el fago T4 es un virus de cabeza y cola, sin envoltura y a escala de nm") {
    const BodyPlan& t4 = virusPlan(VirusKind::T4Like);
    CHECK(t4.morphology == Morphology::HeadTailPhage);
    CHECK_FALSE(t4.has(StructureKind::ViralEnvelope));
    CHECK(t4.has(StructureKind::Capsid));
    CHECK(t4.has(StructureKind::Tail));
    const float head = t4.find(StructureKind::Capsid)->length;
    CHECK(head < 0.2f);                               // ~0,1 µm
    CHECK(head < 0.1f * bacteriumPlan(0).find(StructureKind::Flagellum)->length);
    CHECK_FALSE(visibleInBrightfield(*t4.find(StructureKind::Capsid)));
}

TEST_CASE("Plan corporal: los cloroplastos solo están en las fotótrofas y nada se repite") {
    for (int k = 0; k < kSpeciesCount; ++k) {
        const auto id = static_cast<SpeciesId>(k);
        const BodyPlan& plan = speciesPlan(id);
        INFO(speciesInfo(id).name);
        CHECK(plan.morphology == Morphology::Eukaryote);
        CHECK(plan.has(StructureKind::Nucleus));
        if (plan.has(StructureKind::Chloroplast))
            CHECK(speciesInfo(id).chloroplasts);
        std::set<StructureKind> seen;
        for (const StructureSpec& s : plan.structures)
            CHECK(seen.insert(s.kind).second);
    }
    // Trichonympha no tiene mitocondrias (tiene hidrogenosomas).
    CHECK_FALSE(speciesPlan(SpeciesId::Trichonympha).has(StructureKind::Mitochondrion));
    // Lo que no se resuelve con luz queda para la vista de modelo.
    CHECK_FALSE(visibleInBrightfield(*speciesPlan(SpeciesId::Paramecium).find(StructureKind::EndoplasmicReticulum)));
    CHECK(visibleInBrightfield(*speciesPlan(SpeciesId::Euglena).find(StructureKind::Eyespot)));
}

TEST_CASE("Escala biológica: depende de cuánto mundo entra en la vista y es monótona") {
    CHECK(bioScaleFor(2000.0f) == BioScale::Environment);
    CHECK(bioScaleFor(400.0f) == BioScale::Organisms);
    CHECK(bioScaleFor(40.0f) == BioScale::Cells);
    CHECK(bioScaleFor(4.0f) == BioScale::CellStructures);
    CHECK(bioScaleFor(0.4f) == BioScale::Subcellular);
    int prev = 0;
    for (float v = 5000.0f; v > 0.01f; v *= 0.9f) {
        const int s = static_cast<int>(bioScaleFor(v));
        CHECK(s >= prev);
        prev = s;
    }
}

TEST_CASE("Barra de escala: largos redondos con la unidad que corresponde, hasta nm") {
    CHECK(niceLength(120.0f) == doctest::Approx(200.0f));
    CHECK(niceLength(0.04f) == doctest::Approx(0.05f));
    CHECK(formatLength(500.0f) == "500 µm");
    CHECK(formatLength(2000.0f) == "2 mm");
    CHECK(formatLength(0.05f) == "50 nm");
    CHECK(formatLength(0.5f) == "500 nm");
}

namespace {
WorldConfig bareWorld() {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.cyanoFilaments = 0;
    config.saprolegnia = config.chytridClusters = 0;
    config.medium.detritusSpawnRate = 0.0f;
    for (float& w : config.speciesWeight)
        w = 0.0f;
    config.seed = 12;
    return config;
}
} // namespace

TEST_CASE("Identidad estable: una bacteria se sigue aunque cambie su índice") {
    Bacteria b;
    for (int i = 0; i < 5; ++i)
        b.add(static_cast<float>(i), 0.0f, 0.0f, 1.0f);
    const std::uint32_t last = b.id.back();
    b.remove(1); // la última pasa al índice 1
    CHECK(b.id[1] == last);
    std::set<std::uint32_t> ids(b.id.begin(), b.id.end());
    CHECK(ids.size() == b.size());
    b.add(9.0f, 0.0f, 0.0f, 1.0f); // una hija nueva recibe un id nuevo
    CHECK(ids.count(b.id.back()) == 0);
}

TEST_CASE("Elegir una bacteria: la ficha, la cámara y la miga de pan siguen al mismo individuo") {
    World world(bareWorld());
    world.spawnBeing({BeingKind::Bacterium, SpeciesId::Paramecium}, {1024.0f, 1024.0f});
    REQUIRE(world.bacteria().size() > 3);
    const Vec2 at = {world.bacteria().x[2], world.bacteria().y[2]};
    Picked picked = world.pick(at, 0.5f);
    REQUIRE(picked.kind == Picked::Kind::Bacterium);
    const std::uint32_t id = picked.id;
    CHECK(id == world.bacteria().id[picked.index]);
    for (int k = 0; k < 120; ++k)
        world.step(1.0f / 60.0f);
    Vec2 now;
    REQUIRE(locatePicked(world, picked, now));
    CHECK(world.bacteria().id[picked.index] == id);
    CHECK(now.x == world.bacteria().x[picked.index]);
    const ScalePath path = scalePath(world, now, 3.0f, picked);
    CHECK(path.scale == BioScale::CellStructures);
    REQUIRE(path.crumbs.size() >= 3);
    CHECK(path.crumbs[0] == "Placa");
    CHECK(path.crumbs[1] == "Bacterias");
    CHECK(buildInspection(world, picked).valid);
}

TEST_CASE("Miga de pan: dentro de una célula llega hasta el núcleo") {
    World world(bareWorld());
    world.spawnBeing({BeingKind::Species, SpeciesId::Paramecium}, {1024.0f, 1024.0f});
    const auto view = world.registry().view<RingBody, CellAnatomy>();
    REQUIRE(view.begin() != view.end());
    const auto e = *view.begin();
    const std::uint32_t slot = world.registry().get<CellAnatomy>(e).nucleus;
    const Vec2 nucleus = {world.bodies().x[slot], world.bodies().y[slot]};
    const ScalePath path = scalePath(world, nucleus, 10.0f, Picked{});
    REQUIRE(path.crumbs.size() == 4);
    CHECK(path.crumbs[1] == speciesInfo(SpeciesId::Paramecium).name);
    CHECK(path.crumbs[2] == "Celula");
    CHECK(path.crumbs[3] == "Nucleo");
    CHECK(scalePath(world, nucleus, 2500.0f, Picked{}).crumbs.size() == 1); // de lejos: solo la placa
}
