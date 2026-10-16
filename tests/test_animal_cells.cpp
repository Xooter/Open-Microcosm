// Células humanas en cultivo (fase 20b): adhesión, migración por arrastre (paseo persistente),
// ciclo celular por fases con su punto de restricción, inhibición por contacto, metabolismo de
// glucosa con lactato, apoptosis y quimiotaxis del neutrófilo.
#include "bio/physiology.hpp"
#include "game/plate_spec.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

// Cultivo de DMEM en la incubadora, chico y sin nada vivo.
std::unique_ptr<World> culture(int grid = 64, float bioTimeScale = 3600.0f, std::uint64_t seed = 7) {
    PlateSpec p;
    p.prepared = "cultivo_dmem";
    p.seed = seed;
    p.rebuild();
    p.config.gridX = p.config.gridY = grid;
    auto w = std::make_unique<World>(p.config);
    w->bioTimeScale() = bioTimeScale;
    return w;
}

SpeciesId id(const char* key) { return *findSpecies(key); }

entt::entity spawnAt(World& w, SpeciesId sp, Vec2 at) {
    w.spawnBeing({BeingKind::Species, sp}, at);
    entt::entity found = entt::null;
    for (auto [e, s, body] : w.registry().view<Species, RingBody>().each())
        if (s.id == sp && length(body.centroid - at) < 30.0f)
            found = e;
    return found;
}

Vec2 center(const World& w) { return {0.5f * w.width(), 0.5f * w.height()}; }

// `bioHours`: horas biológicas a la temperatura del cultivo (37 °C). bioScale() incluye el Q10
// (cuenta en "horas a 20 °C"); el tiempo transcurrido de verdad es el de bioTimeScale.
void run(World& w, float bioHours) {
    const int steps = static_cast<int>(bioHours * 3600.0f / (w.bioTimeScale() * kDt));
    for (int s = 0; s < steps; ++s)
        w.step(kDt);
}

} // namespace

TEST_CASE("fibroblasto: adherido al fondo, la corriente no lo arrastra") {
    auto w = culture();
    const entt::entity e = spawnAt(*w, id("fibroblasto_dermico_humano"), center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    CHECK(w->registry().all_of<Adherent, AnimalCell>(e));
    // Sin migración, para medir solo lo que hace el agua.
    w->registry().get<AnimalCell>(e).phase = CellPhase::G0;
    const Vec2 before = w->registry().get<RingBody>(e).centroid;
    w->bioTimeScale() = 60.0f;
    for (int s = 0; s < 120; ++s) {
        w->setStir(center(*w) + Vec2{0.0f, 30.0f}, {200.0f, 0.0f}, 120.0f);
        w->step(kDt);
    }
    const Vec2 after = w->registry().get<RingBody>(e).centroid;
    // Migra a ~0,7 µm/s reales: en 2 s, a lo sumo ~2 µm. La corriente movería el agua 400 µm.
    CHECK(length(after - before) < 5.0f);
}

TEST_CASE("fibroblasto: el ciclo dura lo que suman sus fases y las hijas empiezan G1") {
    auto w = culture();
    const SpeciesId fib = id("fibroblasto_dermico_humano");
    const entt::entity e = spawnAt(*w, fib, center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    AnimalCell& ac = w->registry().get<AnimalCell>(e);
    ac.phase = CellPhase::G1;
    ac.phaseHours = 0.0f;
    const SpeciesPhysiology& p = speciesPhysiology(fib);
    // Duración del ciclo a 37 °C (vuelve al valor publicado: Q10 desde 20 °C).
    const float q10 = std::pow(2.0f, (w->medium().temperature - 20.0f) / 10.0f);
    const float cycle = (p.phaseG1.value + p.phaseS.value + p.phaseG2.value + p.phaseM.value) / q10;
    CHECK(cycle == doctest::Approx(30.0f).epsilon(0.05));
    float hours = 0.0f;
    bool sawM = false, rounded = false;
    while (w->census().species[static_cast<int>(fib)] < 2 && hours < 1.5f * cycle) {
        run(*w, 0.25f);
        hours += 0.25f;
        for (auto [ce, c, body] : w->registry().view<AnimalCell, RingBody>().each())
            if (c.phase == CellPhase::M) {
                sawM = true;
                const float r = speciesInfo(fib).animalCell.roundRadius;
                rounded = rounded || std::abs(body.restArea) < 1.5f * 3.14159f * r * r;
            }
    }
    MESSAGE("primera división a las " << hours << " h (ciclo " << cycle << " h)");
    CHECK(sawM);
    CHECK(rounded); // en mitosis se redondea
    CHECK(w->census().species[static_cast<int>(fib)] == 2);
    CHECK(hours == doctest::Approx(cycle).epsilon(0.1));
    for (auto [ce, c] : w->registry().view<AnimalCell>().each())
        CHECK(c.phase == CellPhase::G1);
}

TEST_CASE("fibroblasto: sin suero no pasa el punto de restricción y queda en G0") {
    auto w = culture();
    w->medium().serum = 0.0f;
    const SpeciesId fib = id("fibroblasto_dermico_humano");
    const entt::entity e = spawnAt(*w, fib, center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    w->registry().get<AnimalCell>(e).phaseHours = 0.0f;
    run(*w, 40.0f);
    CHECK(w->census().species[static_cast<int>(fib)] == 1);
    CHECK(w->registry().get<AnimalCell>(e).phase == CellPhase::G0);
    // Con suero de nuevo, vuelve al ciclo.
    w->medium().serum = 0.1f;
    run(*w, 2.0f);
    CHECK(w->registry().get<AnimalCell>(e).phase == CellPhase::G1);
}

TEST_CASE("inhibición por contacto: el fibroblasto rodeado queda en G0; HeLa sigue en el ciclo") {
    for (const char* key : {"fibroblasto_dermico_humano", "hela"}) {
        auto w = culture(64);
        const SpeciesId sp = id(key);
        // Un centro y seis vecinas pegadas alrededor.
        const Vec2 c = center(*w);
        const entt::entity mid = spawnAt(*w, sp, c);
        REQUIRE(static_cast<bool>(mid != entt::null));
        const CellSpec& spec = speciesInfo(sp).spec;
        for (int k = 0; k < 6; ++k) {
            const float a = 6.2831853f * static_cast<float>(k) / 6.0f;
            const float d = k % 3 == 0 ? spec.radiusX * 1.6f : spec.radiusY * 1.9f;
            w->spawnBeing({BeingKind::Species, sp}, c + Vec2{d * std::cos(a), d * std::sin(a)});
        }
        for (auto [e, ac] : w->registry().view<AnimalCell>().each()) {
            ac.phase = CellPhase::G1;
            ac.phaseHours = 0.0f;
            ac.polarity = 0.0f;
        }
        w->bioTimeScale() = 600.0f;
        run(*w, 20.0f); // pasa el punto de restricción al final de G1 (~9–17 h)
        const AnimalCell& ac = w->registry().get<AnimalCell>(mid);
        MESSAGE(std::string(key) << ": contacto " << ac.contact << ", fase " << std::string(cellPhaseName(ac.phase)));
        if (std::string(key) == "hela") {
            CHECK(ac.phase != CellPhase::G0); // sin inhibición por contacto de la proliferación
        } else {
            CHECK(ac.contact > 0.5f);
            CHECK(ac.phase == CellPhase::G0);
        }
    }
}

TEST_CASE("HeLa: consume glucosa y secreta lactato (Warburg), que acidifica el medio") {
    auto w = culture(64, 600.0f);
    const SpeciesId hela = id("hela");
    const Vec2 c = center(*w);
    for (int k = 0; k < 8; ++k)
        w->spawnBeing({BeingKind::Species, hela}, c + Vec2{60.0f * std::cos(0.785f * k), 60.0f * std::sin(0.785f * k)});
    const float glucose0 = w->meanConcentration(w->nutrients());
    run(*w, 2.0f);
    const float ph0 = 0.0f + w->ph().sample(c.x, c.y);
    run(*w, 10.0f);
    const float usedGlucose = glucose0 - w->meanConcentration(w->nutrients());
    const float lactate = w->meanConcentration(w->lactate());
    MESSAGE("glucosa consumida " << usedGlucose << " µM, lactato " << lactate << " µM");
    REQUIRE(usedGlucose > 0.0f);
    CHECK(lactate / usedGlucose == doctest::Approx(speciesPhysiology(hela).lactatePerGlucose.value).epsilon(0.05));
    CHECK(w->ph().sample(c.x, c.y) < ph0);
}

TEST_CASE("apoptosis: sin glucosa la célula se fragmenta en cuerpos apoptóticos, sin derramar nada") {
    auto w = culture(64, 600.0f);
    for (float& v : w->nutrients().grid().v)
        v = 0.0f;
    const SpeciesId fib = id("fibroblasto_dermico_humano");
    REQUIRE(static_cast<bool>(spawnAt(*w, fib, center(*w)) != entt::null));
    bool sawApoptotic = false;
    for (int h = 0; h < 40 && w->census().species[static_cast<int>(fib)] > 0; ++h) {
        run(*w, 1.0f);
        for (auto [e, ac] : w->registry().view<AnimalCell>().each())
            sawApoptotic = sawApoptotic || ac.phase == CellPhase::Apoptotic;
    }
    CHECK(sawApoptotic);
    CHECK(w->census().species[static_cast<int>(fib)] == 0);
    CHECK(w->observations().apoptoses == 1);
    // Sin lisis: no aparece materia orgánica disuelta, quedan los cuerpos apoptóticos.
    CHECK(w->meanConcentration(w->nutrients()) < 1e-3f);
    int bodies = 0;
    for (std::size_t i = 0; i < w->ambient().size(); ++i)
        bodies += w->ambient().kind[i] == ParticleKind::Detritus;
    CHECK(bodies >= 3);
}

TEST_CASE("neutrófilo: sigue el fMLP de las bacterias, las fagocita y no se divide") {
    auto w = culture(64, 60.0f);
    const Vec2 c = center(*w);
    const Vec2 colony = c + Vec2{70.0f, 0.0f};
    w->inoculate({BeingKind::Bacterium, SpeciesId::Paramecium}, 2000, colony, 15.0f);
    const SpeciesId neu = id("neutrofilo");
    const entt::entity e = spawnAt(*w, neu, c - Vec2{20.0f, 0.0f});
    REQUIRE(static_cast<bool>(e != entt::null));
    float d0 = length(w->registry().get<RingBody>(e).centroid - colony);
    run(*w, 0.05f);
    MESSAGE("fMLP en la colonia " << w->fmlp().grid().sample(colony.x, colony.y) << " µM, junto al neutrófilo "
                                  << w->fmlp().grid().sample(c.x, c.y) << " µM");
    run(*w, 0.35f); // 24 min biológicos en total
    REQUIRE(w->registry().valid(e));
    const float d1 = length(w->registry().get<RingBody>(e).centroid - colony);
    const Digestion& dig = w->registry().get<Digestion>(e);
    MESSAGE("distancia a la colonia: " << d0 << " → " << d1 << " µm; vacuolas con bacterias: " << dig.vacuoles.size());
    // Las bacterias nadan y la colonia se dispersa: se acerca a donde estaba y fagocitó alguna.
    CHECK(d1 < 0.8f * d0);
    CHECK(dig.vacuoles.size() >= 1);
    CHECK(w->census().species[static_cast<int>(neu)] == 1);
}
