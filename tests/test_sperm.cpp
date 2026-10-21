// Espermatozoide humano: nado con el Q10 medido (García-Molina et al. 2023), recorrido pasivo, muestra por
// clases de la OMS, CASA, vitalidad (eosina–nigrosina y prueba hiposmótica), capacitación con reacción
// acrosómica, morfología, reotaxis y leucocitospermia.
#include "bio/physiology.hpp"
#include "game/casa.hpp"
#include "game/mass_budget.hpp"
#include "game/plate_spec.hpp"
#include "game/reagent.hpp"
#include "game/run.hpp"
#include "game/scene.hpp"
#include "game/stain.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <vector>

namespace {

constexpr float kDt = 1.0f / 60.0f;

SpeciesId sperm() { return *findSpecies("human_sperm"); }

PlateSpec plate(const char* id, std::uint64_t seed = 5) {
    PlateSpec p;
    p.prepared = id;
    p.seed = seed;
    p.rebuild();
    return p;
}

struct Session {
    std::unique_ptr<World> world;
    Run run;
    explicit Session(const PlateSpec& p, bool casa = false) : world(std::make_unique<World>(p.config)) {
        run.casa().enabled = casa;
        run.start(*world, plateRunConfig(p), 1);
    }
    void seconds(float s) {
        for (int k = 0; k < static_cast<int>(s / kDt); ++k) {
            run.interventions().beforeStep(*world, kDt);
            world->step(kDt);
            run.update(*world, kDt);
        }
    }
};

// Los espermatozoides de la placa con su estado.
template <typename Fn> void eachSperm(World& w, Fn&& fn) {
    for (auto [e, sp, ac] : w.registry().view<Species, AnimalCell>().each())
        if (sp.id == sperm())
            fn(e, ac);
}

} // namespace

TEST_CASE("espermatozoide: los datos cargan con sus mecanismos y su fisiología medida") {
    const SpeciesInfo& s = speciesInfo(sperm());
    CHECK(s.animalCell.present);
    CHECK(s.animalCell.immotileDeath);
    CHECK(s.flagellumPosterior);
    CHECK(s.swimPath.present);
    CHECK(s.motility.present);
    CHECK(s.osmoticSwelling.present);
    CHECK(s.capacitation.present);
    CHECK(s.morphotypes.size() >= 5);
    const SpeciesPhysiology& p = speciesPhysiology(sperm());
    REQUIRE(p.swimSpeed.present);
    REQUIRE(p.beatFrequency.present);
    // García-Molina et al. (2023): VAP 54,5 → 37,4 µm/s y BCF 15,3 → 12,6 Hz de 37 a 23 °C.
    CHECK(atTemperature(p.swimSpeed, 37.0f) == doctest::Approx(54.5f));
    CHECK(atTemperature(p.swimSpeed, 23.0f) == doctest::Approx(37.4f).epsilon(0.03));
    CHECK(atTemperature(p.beatFrequency, 23.0f) == doctest::Approx(12.6f).epsilon(0.03));
    for (const std::string& w : physiologyWarnings())
        CHECK_MESSAGE(w.find("human_sperm") == std::string::npos, w);
    CHECK(findReagent("progesterone") >= 0);
    CHECK(reagent(findReagent("progesterone")).signal);
}

TEST_CASE("CASA: la cinemática de una trayectoria sinusoidal") {
    // Avanza a 50 µm/s con un vaivén lateral de ±1 µm a 10 Hz, muestreado a 60 Hz durante 1 s.
    std::vector<Vec2> p;
    for (int i = 0; i <= 60; ++i) {
        const float t = static_cast<float>(i) / 60.0f;
        p.push_back({50.0f * t, std::sin(6.2831853f * 10.0f * t)});
    }
    const CasaKinematics k = casaKinematics(p, 1.0f / 60.0f, {});
    CHECK(k.vsl == doctest::Approx(50.0f).epsilon(0.02));
    CHECK(k.vap == doctest::Approx(50.0f).epsilon(0.05));
    CHECK(k.vcl > k.vap);
    CHECK(k.str == doctest::Approx(1.0f).epsilon(0.03));
    CHECK(k.bcf == doctest::Approx(10.0f).epsilon(0.15));
    CHECK(k.alh == doctest::Approx(2.0f).epsilon(0.35)); // de pico a pico (el promedio móvil lo suaviza)
    CHECK(k.grade == CasaGrade::Rapid);
}

TEST_CASE("validación: la cinemática CASA de la placa a 37 °C y a 23 °C (García-Molina et al. 2023)" * doctest::test_suite("validacion")) {
    struct Measured {
        float t, vap, vsl, bcf;
    };
    // Tabla 2: semen incubado y analizado a cada temperatura.
    for (const Measured m : {Measured{37.0f, 54.5f, 43.1f, 15.3f}, Measured{23.0f, 37.4f, 22.7f, 12.6f}}) {
        PlateSpec p = plate("fresh_semen");
        p.config.medium.temperature = m.t;
        Session s(p, true);
        s.seconds(4.0f);
        const CasaSummary& c = s.run.casa().summary();
        MESSAGE(m.t << " °C: VAP " << c.vap << " VSL " << c.vsl << " BCF " << c.bcf << " (n = " << c.tracked << ")");
        REQUIRE(c.tracked > 50);
        CHECK(c.vap == doctest::Approx(m.vap).epsilon(0.15));
        CHECK(c.bcf == doctest::Approx(m.bcf).epsilon(0.1));
        // La VSL depende de la curvatura y del ruido de la trayectoria (estimados): menos estricta.
        CHECK(c.vsl == doctest::Approx(m.vsl).epsilon(0.5));
    }
}

TEST_CASE("muestra: las clases de la OMS que pide la placa y el CASA que las mide") {
    PlateSpec p = plate("fresh_semen");
    p.config.motilitySet = true;
    p.config.motility = {0.2f, 0.0f, 0.3f, 0.5f}; // astenozoospermia con poca vitalidad
    Session s(p, true);
    int all = 0, progressive = 0, dead = 0;
    eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) {
        ++all;
        progressive += ac.phase != CellPhase::Dead && ac.grade == MotilityGrade::Progressive;
        dead += ac.phase == CellPhase::Dead;
    });
    REQUIRE(all > 100);
    CHECK(static_cast<float>(progressive) / all == doctest::Approx(0.2f).epsilon(0.5));
    CHECK(static_cast<float>(dead) / all == doctest::Approx(0.5f).epsilon(0.3));
    // Los muertos no cuentan en el censo de vivos.
    CHECK(s.world->census().species[static_cast<int>(sperm())] == all - dead);
    s.seconds(2.0f);
    CHECK(s.run.casa().summary().fraction(CasaGrade::Immotile) == doctest::Approx(0.8f).epsilon(0.2));
}

TEST_CASE("muestra: la calidad y las formas normales van en el .plate") {
    PlateSpec p = plate("fresh_semen");
    p.config.motilitySet = true;
    p.config.motility = {0.3f, 0.1f, 0.2f, 0.4f};
    p.config.normalForms = 0.04f;
    PlateSpec q;
    REQUIRE(parsePlate(serializePlate(p), q));
    CHECK(q.config.motilitySet);
    CHECK(q.config.motility.dead == doctest::Approx(0.4f));
    CHECK(q.config.normalForms == doctest::Approx(0.04f));
}

TEST_CASE("muerte inmóvil: sin fructosa deja de nadar, muere entero y el balance cierra" * doctest::test_suite("largos")) {
    PlateSpec p = plate("fresh_semen");
    p.config.motilitySet = true;
    p.config.motility = {1.0f, 0.0f, 0.0f, 0.0f};
    p.config.medium.organic = 0.0f; // sin fructosa
    Session s(p);
    s.world->bioTimeScale() = 3600.0f; // 1 s real = 1 h biológica
    const MassBudget start = s.world->massBudget();
    const int before = static_cast<int>(s.world->registry().view<RingBody>().size());
    s.seconds(20.0f);
    int dead = 0, all = 0;
    eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) {
        ++all;
        dead += ac.phase == CellPhase::Dead;
    });
    CHECK(all == before); // enteros: no se lisan ni se parten
    CHECK(dead > all / 2);
    for (auto [e, sp, ac, loco] : s.world->registry().view<Species, AnimalCell, Locomotion>().each())
        if (ac.phase == CellPhase::Dead)
            CHECK(loco.speed == 0.0f);
    CHECK(std::abs(budgetResidual(start, s.world->massBudget(), Element::P)) < 1e-3);
}

TEST_CASE("prueba hiposmótica: la cola de los vivos se enrolla; la de los muertos no" * doctest::test_suite("largos")) {
    Session s(plate("semen_hos"));
    s.seconds(180.0f);
    int alive = 0, aliveSwollen = 0, dead = 0, deadSwollen = 0;
    eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) {
        if (ac.phase == CellPhase::Dead) {
            ++dead;
            deadSwollen += ac.swelling > 0.5f;
        } else {
            ++alive;
            aliveSwollen += ac.swelling > 0.5f;
        }
    });
    REQUIRE(alive > 0);
    REQUIRE(dead > 0);
    CHECK(aliveSwollen == alive);
    // Los que murieron hace menos de un minuto todavía se están deshinchando.
    CHECK(deadSwollen < dead / 4 + 1);
    // En el plasma seminal (isotónico o un poco más) no se hinchan.
    Session iso(plate("fresh_semen"));
    iso.seconds(60.0f);
    eachSperm(*iso.world, [&](entt::entity, const AnimalCell& ac) { CHECK(ac.swelling < 0.05f); });
}

TEST_CASE("eosina–nigrosina: tiñe solo a los de membrana dañada") {
    const StainSpec* st = nullptr;
    for (const StainSpec& s : builtinStains())
        if (s.id == "eosin_nigrosin")
            st = &s;
    REQUIRE(st);
    CHECK(evaluateStain(*st, StainTarget::DamagedMembrane).alpha > 0.5f);
    CHECK(evaluateStain(*st, StainTarget::Nucleus).alpha == 0.0f);
    CHECK(evaluateStain(*st, StainTarget::Cytoplasm).alpha == 0.0f);
    CHECK_FALSE(st->vital);
}

TEST_CASE("capacitación: en HTF con albúmina se capacitan, una parte se hiperactiva; en el plasma seminal no" * doctest::test_suite("largos")) {
    Session s(plate("sperm_capacitation"));
    s.world->bioTimeScale() = 3600.0f;
    s.seconds(6.0f); // 6 h biológicas
    int alive = 0, capacitated = 0, hyper = 0;
    eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) {
        if (ac.phase == CellPhase::Dead)
            return;
        ++alive;
        capacitated += ac.capacitation >= 1.0f;
        hyper += ac.hyperactive;
    });
    REQUIRE(alive > 20);
    CHECK(capacitated == alive);
    CHECK(hyper > 0);
    CHECK(hyper < alive / 2);
    Session plasma(plate("fresh_semen"));
    plasma.world->bioTimeScale() = 3600.0f;
    plasma.seconds(6.0f);
    eachSperm(*plasma.world, [&](entt::entity, const AnimalCell& ac) { CHECK(ac.capacitation == 0.0f); });
}

TEST_CASE("reacción acrosómica: la progesterona la dispara en los capacitados" * doctest::test_suite("largos")) {
    auto reacted = [](bool progesterone) {
        Session s(plate("sperm_capacitation", 9));
        s.world->bioTimeScale() = 3600.0f;
        s.seconds(5.0f); // capacitados
        int before = 0;
        eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) { before += ac.acrosomeReacted; });
        if (progesterone) {
            s.world->setReagent(findReagent("progesterone"));
            for (float& v : s.world->reagentField().grid().v)
                v = 3.0f; // ~10 µM, saturante
        }
        s.seconds(1.0f); // 1 h biológica
        int after = 0, all = 0;
        eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) {
            all += ac.phase != CellPhase::Dead;
            after += ac.acrosomeReacted;
        });
        return static_cast<float>(after - before) / std::max(1, all);
    };
    const float control = reacted(false), induced = reacted(true);
    MESSAGE("reacción acrosómica en 1 h: " << control << " sin progesterona, " << induced << " con");
    CHECK(induced > control + 0.1f);
}

TEST_CASE("morfología: la fracción de formas normales que pide la muestra") {
    PlateSpec p = plate("fresh_semen");
    p.config.normalForms = 0.5f;
    Session s(p);
    const SpeciesInfo& info = speciesInfo(sperm());
    int all = 0, normal = 0;
    eachSperm(*s.world, [&](entt::entity, const AnimalCell& ac) {
        ++all;
        normal += info.morphotypes[ac.morphotype].normal;
    });
    REQUIRE(all > 100);
    CHECK(static_cast<float>(normal) / all == doctest::Approx(0.5f).epsilon(0.25));
}

TEST_CASE("reotaxis: en una corriente se orientan contra ella" * doctest::test_suite("largos")) {
    PlateSpec p = plate("fresh_semen");
    p.config.motilitySet = true;
    p.config.motility = {1.0f, 0.0f, 0.0f, 0.0f};
    Session s(p);
    const Vec2 c = {0.5f * s.world->width(), 0.5f * s.world->height()};
    // Una corriente hacia +x en todo el centro de la placa, unos segundos.
    float against = 0.0f;
    int n = 0;
    for (int k = 0; k < 600; ++k) {
        s.world->setStir(c, {60.0f, 0.0f}, 400.0f);
        s.run.interventions().beforeStep(*s.world, kDt);
        s.world->step(kDt);
        s.run.update(*s.world, kDt);
    }
    for (auto [e, sp, body] : s.world->registry().view<Species, RingBody>().each())
        if (sp.id == sperm() && length(body.centroid - c) < 150.0f) {
            const Vec2 u = s.world->fluid().backgroundAt(body.centroid);
            if (length(u) < 1.0f)
                continue;
            against += -std::cos(body.angle) * u.x / length(u) - std::sin(body.angle) * u.y / length(u);
            ++n;
        }
    REQUIRE(n > 5);
    MESSAGE("orientación media contra la corriente: " << against / n);
    CHECK(against / n > 0.2f);
}

TEST_CASE("leucocitospermia: los neutrófilos fagocitan espermatozoides" * doctest::test_suite("largos")) {
    Session s(plate("semen_leukocytes"));
    const int before = s.world->census().species[static_cast<int>(sperm())];
    const SpeciesId neu = *findSpecies("neutrophil");
    const int neutrophils = s.world->census().species[static_cast<int>(neu)];
    REQUIRE(neutrophils > 0);
    s.seconds(60.0f);
    CHECK(s.world->census().species[static_cast<int>(neu)] == neutrophils);
    CHECK(s.world->census().species[static_cast<int>(sperm())] < before);
}
