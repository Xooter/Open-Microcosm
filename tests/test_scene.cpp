// Escenas y replays (MVP: reproducibilidad): una sesión con intervenciones guardada como escena se
// repite idéntica (mismas posiciones bit a bit) al cargarla, con 1 y 4 hilos, y el registro va y
// vuelve sin cambios.
#include "game/scene.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <cstring>

namespace {

PlateSpec smallPlate() {
    PlateSpec p;
    p.biome = 1;
    p.seed = 4242;
    p.content = DropContent::NoLife; // agua, química y sedimento; los seres los pone el usuario
    p.rebuild();
    p.config.gridX = p.config.gridY = 64; // 512 µm: rápido
    p.config.bacteria = 3000;
    return p;
}

std::uint64_t positionsHash(const World& w) {
    std::uint64_t h = 1469598103934665603ull;
    const BodyParticles& bp = w.bodies();
    for (std::size_t s = 0; s < bp.capacity(); ++s)
        if (bp.alive[s])
            for (const float f : {bp.x[s], bp.y[s]}) {
                std::uint32_t bits;
                std::memcpy(&bits, &f, sizeof bits);
                h = (h ^ bits) * 1099511628211ull;
            }
    for (std::size_t i = 0; i < w.bacteria().size(); ++i)
        for (const float f : {w.bacteria().x[i], w.bacteria().y[i]}) {
            std::uint32_t bits;
            std::memcpy(&bits, &f, sizeof bits);
            h = (h ^ bits) * 1099511628211ull;
        }
    return h;
}

constexpr float kDt = 1.0f / 60.0f;
constexpr int kSteps = 150;

// Una sesión "en vivo": el usuario pone seres, gotea, revuelve y cambia un parámetro.
std::vector<Intervention> liveSession(const PlateSpec& plate, std::uint64_t& hash) {
    World world(plate.config);
    Run run;
    run.start(world, plateRunConfig(plate), 1);
    InterventionLog& log = run.interventions();
    const Vec2 c = {world.width() * 0.5f, world.height() * 0.5f};
    for (int step = 0; step < kSteps; ++step) {
        Intervention iv;
        iv.pos = c;
        if (step == 0) {
            iv.kind = InterventionKind::Spawn;
            iv.index = static_cast<int>(BeingKind::Species);
            iv.v[0] = static_cast<float>(SpeciesId::Paramecium);
            log.record(world, iv);
            iv.v[0] = static_cast<float>(SpeciesId::Euglena);
            iv.pos = c + Vec2{60.0f, 20.0f};
            log.record(world, iv);
        }
        if (step == 20) {
            Intervention drop;
            drop.kind = InterventionKind::Dropper;
            drop.active = true;
            drop.pos = c + Vec2{-50.0f, 0.0f};
            drop.index = 0;
            drop.v[0] = 40.0f;
            drop.v[1] = 1.0f;
            log.recordState(world, drop);
            log.recordState(world, drop); // repetido: no se anota de nuevo
        }
        if (step == 60) {
            Intervention off;
            off.kind = InterventionKind::Dropper;
            log.recordState(world, off);
            Intervention p;
            p.kind = InterventionKind::Parameter;
            p.key = "medio.alcalinidad";
            p.v[0] = 2500.0f;
            log.record(world, p);
        }
        if (step >= 80 && step < 100) {
            Intervention st;
            st.kind = InterventionKind::Stir;
            st.active = true;
            st.pos = c + Vec2{static_cast<float>(step - 80), 0.0f};
            st.v[0] = 300.0f;
            st.v[2] = 60.0f;
            log.recordState(world, st);
        }
        if (step == 100) {
            Intervention st;
            st.kind = InterventionKind::Stir;
            log.recordState(world, st);
        }
        log.beforeStep(world, kDt);
        world.step(kDt);
        run.update(world, kDt);
    }
    hash = positionsHash(world);
    return log.entries();
}

std::uint64_t replay(const std::string& sceneText) {
    PlateSpec plate;
    std::vector<Intervention> events;
    REQUIRE(parseScene(sceneText, plate, events));
    World world(plate.config);
    Run run;
    run.start(world, plateRunConfig(plate), 1);
    run.interventions().schedule(events);
    for (int step = 0; step < kSteps; ++step) {
        run.interventions().beforeStep(world, kDt);
        world.step(kDt);
        run.update(world, kDt);
    }
    CHECK(run.interventions().entries().size() == events.size());
    return positionsHash(world);
}

} // namespace

TEST_CASE("escena: el registro de intervenciones va y vuelve sin cambios") {
    std::vector<Intervention> list(2);
    list[0].step = 7;
    list[0].kind = InterventionKind::Parameter;
    list[0].key = "xpbd.flexion";
    list[0].v[0] = 0.1f + 1e-7f;
    list[1].step = 123456789012ull;
    list[1].kind = InterventionKind::Stir;
    list[1].active = true;
    list[1].pos = {-3.25f, 1e-30f};
    list[1].v[5] = -0.0f;
    const std::vector<Intervention> back = parseInterventions(serializeInterventions(list));
    REQUIRE(back.size() == 2);
    CHECK(back[0].step == 7);
    CHECK(back[0].key == "xpbd.flexion");
    CHECK(back[0].v[0] == list[0].v[0]);
    CHECK(back[1].step == list[1].step);
    CHECK(back[1].active);
    CHECK(back[1].pos.y == list[1].pos.y);
    CHECK(serializeInterventions(back) == serializeInterventions(list));
}

TEST_CASE("escena: una sesión con intervenciones se repite idéntica, con 1 y 4 hilos") {
    const PlateSpec plate = smallPlate();
    std::uint64_t live = 0;
    const std::vector<Intervention> events = liveSession(plate, live);
    CHECK(events.size() >= 6);
    const std::string text = serializeScene(plate, events);
    const int threads = omp_get_max_threads();
    omp_set_num_threads(1);
    const std::uint64_t one = replay(text);
    omp_set_num_threads(4);
    const std::uint64_t four = replay(text);
    omp_set_num_threads(threads);
    CHECK(one == live);
    CHECK(four == live);
}
