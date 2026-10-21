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
    p.prepared = "dmem_culture";
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
    const entt::entity e = spawnAt(*w, id("human_dermal_fibroblast"), center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    CHECK(w->registry().all_of<Adherent, AnimalCell>(e));
    // La siembra es asincrónica (cada célula en un punto del ciclo): ésta empieza G1, así no se
    // redondea para dividirse durante la medición.
    w->registry().get<AnimalCell>(e).phase = CellPhase::G1;
    w->registry().get<AnimalCell>(e).phaseHours = 0.0f;
    // Primero se extiende (llega redonda de la siembra); después, sin migrar, se mide el agua.
    run(*w, 1.5f);
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
    // Escala biológica 600: con 3600 la mitosis (el surco es física, en tiempo real) se alarga.
    auto w = culture(64, 600.0f);
    const SpeciesId fib = id("human_dermal_fibroblast");
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
    const SpeciesId fib = id("human_dermal_fibroblast");
    const entt::entity e = spawnAt(*w, fib, center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    w->registry().get<AnimalCell>(e).phase = CellPhase::G1; // antes del punto de restricción
    w->registry().get<AnimalCell>(e).phaseHours = 0.0f;
    run(*w, 40.0f);
    REQUIRE(w->registry().valid(e));
    CHECK(w->census().species[static_cast<int>(fib)] == 1);
    CHECK(w->registry().get<AnimalCell>(e).phase == CellPhase::G0);
    // Con suero de nuevo, vuelve al ciclo.
    w->medium().serum = 0.1f;
    run(*w, 2.0f);
    CHECK(w->registry().get<AnimalCell>(e).phase == CellPhase::G1);
}

TEST_CASE("inhibición por contacto: en una monocapa densa el fibroblasto queda en G0; HeLa sigue en el ciclo" *
          doctest::test_suite("largos")) {
    for (const char* key : {"human_dermal_fibroblast", "hela"}) {
        auto w = culture(64, 600.0f, 4);
        const SpeciesId sp = id(key);
        // Placa de ~0,35 mm de diámetro sembrada por encima de la confluencia de los fibroblastos.
        w->inoculate({BeingKind::Species, sp}, 45, {}, 0.0f);
        run(*w, 18.0f); // se extienden, se tocan y llegan al punto de restricción (fin de G1)
        int total = 0, quiescent = 0;
        float contact = 0.0f;
        for (auto [e, ac] : w->registry().view<AnimalCell>().each()) {
            ++total;
            quiescent += ac.phase == CellPhase::G0;
            contact += ac.contact;
        }
        REQUIRE(total > 0);
        const float fraction = static_cast<float>(quiescent) / static_cast<float>(total);
        MESSAGE(std::string(key) << ": " << total << " células, contacto medio " << contact / static_cast<float>(total)
                                 << ", en G0 " << 100.0f * fraction << " %");
        if (std::string(key) == "hela")
            CHECK(fraction < 0.05f); // sin inhibición por contacto de la proliferación
        else
            CHECK(fraction > 0.5f);
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
    const SpeciesId fib = id("human_dermal_fibroblast");
    const entt::entity e = spawnAt(*w, fib, center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    w->registry().get<AnimalCell>(e).phase = CellPhase::G1; // que no llegue a dividirse antes
    w->registry().get<AnimalCell>(e).phaseHours = 0.0f;
    bool sawApoptotic = false;
    // Cada 6 min: la apoptosis, una vez disparada, se completa en menos de una hora.
    for (int k = 0; k < 400 && w->census().species[static_cast<int>(fib)] > 0; ++k) {
        run(*w, 0.1f);
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
    // El paseo es aleatorio (persistente): en tres placas, se acerca a la colonia en todas y la
    // alcanza y fagocita en la mayoría (una sola corrida dependía de la suerte del sorteo).
    int closer = 0, fed = 0;
    for (const std::uint64_t seed : {7ull, 8ull, 9ull}) {
        auto w = culture(64, 60.0f, seed);
        const Vec2 c = center(*w);
        const Vec2 colony = c + Vec2{70.0f, 0.0f};
        w->inoculate({BeingKind::Bacterium, SpeciesId::Paramecium}, 2000, colony, 15.0f);
        const SpeciesId neu = id("neutrophil");
        const entt::entity e = spawnAt(*w, neu, c - Vec2{20.0f, 0.0f});
        REQUIRE(static_cast<bool>(e != entt::null));
        const float d0 = length(w->registry().get<RingBody>(e).centroid - colony);
        run(*w, 0.05f);
        MESSAGE("fMLP en la colonia " << w->fmlp().grid().sample(colony.x, colony.y) << " µM, junto al neutrófilo "
                                      << w->fmlp().grid().sample(c.x, c.y) << " µM");
        run(*w, 0.35f); // 24 min biológicos en total
        REQUIRE(w->registry().valid(e));
        const float d1 = length(w->registry().get<RingBody>(e).centroid - colony);
        const Digestion& dig = w->registry().get<Digestion>(e);
        MESSAGE("seed " << seed << ": distancia a la colonia " << d0 << " → " << d1 << " µm; vacuolas con bacterias: "
                        << dig.vacuoles.size());
        // Las bacterias nadan y la colonia se dispersa: se acerca a donde estaba y fagocita.
        closer += d1 < 0.8f * d0 ? 1 : 0;
        fed += dig.vacuoles.size() >= 1 ? 1 : 0;
        CHECK(w->census().species[static_cast<int>(neu)] == 1); // no se divide
    }
    CHECK(closer == 3);
    CHECK(fed >= 2);
}

// --- Validación (suite "validacion"): contra los datos con fuente de [physiology.<id>.*] ------------

TEST_CASE("validación: el fibroblasto migra como un paseo persistente (Fürth: S y P de los datos)" *
          doctest::test_suite("validacion")) {
    // 9 fibroblastos separados (sin tocarse) durante 6 h; se compara el desplazamiento cuadrático
    // medio con el de Fürth–Ornstein: <d²> = 2 S² P [t − P (1 − e^(−t/P))] (Gail & Boone 1970).
    auto w = culture(256, 600.0f, 11);
    const SpeciesId fib = id("human_dermal_fibroblast");
    const Vec2 c = center(*w);
    for (int i = -1; i <= 1; ++i)
        for (int j = -1; j <= 1; ++j)
            w->spawnBeing({BeingKind::Species, fib}, c + Vec2{350.0f * static_cast<float>(i), 350.0f * static_cast<float>(j)});
    for (auto [e, ac] : w->registry().view<AnimalCell>().each())
        ac.phase = CellPhase::G0; // sin dividirse: solo la migración
    const float q10 = std::pow(2.0f, (w->medium().temperature - 20.0f) / 10.0f);
    const SpeciesPhysiology& p = speciesPhysiology(fib);
    const float S = p.migrationSpeed.value * q10, P = p.persistence.value / q10; // a 37 °C [µm/h, h]
    std::vector<std::vector<Vec2>> tracks(9);
    constexpr float kSample = 0.1f; // [h]
    for (int k = 0; k <= 60; ++k) {
        int i = 0;
        for (auto [e, ac, body] : w->registry().view<AnimalCell, RingBody>().each())
            if (i < 9)
                tracks[static_cast<std::size_t>(i++)].push_back(body.centroid);
        run(*w, kSample);
    }
    for (const float lagH : {0.5f, 1.0f, 2.0f}) {
        const int lag = static_cast<int>(std::round(lagH / kSample));
        double sum = 0.0;
        int n = 0;
        for (const auto& tr : tracks)
            for (std::size_t t = 0; t + static_cast<std::size_t>(lag) < tr.size(); ++t) {
                const Vec2 d = tr[t + static_cast<std::size_t>(lag)] - tr[t];
                sum += static_cast<double>(dot(d, d));
                ++n;
            }
        const double msd = sum / n;
        const double furth = 2.0 * S * S * P * (lagH - P * (1.0 - std::exp(-lagH / P)));
        MESSAGE("MSD a " << lagH << " h: " << msd << " µm² (Fürth " << furth << ")");
        CHECK(msd == doctest::Approx(furth).epsilon(0.35));
    }
}

namespace {
// Tiempo de duplicación de la población a densidad baja (fase exponencial, sin contacto). Con la
// escala biológica 600: más comprimida, la mitosis (física, en tiempo real) ya no alcanza a
// cerrar el surco en lo que dura M y alarga el ciclo.
void checkDoubling(const char* key) {
    auto w = culture(256, 600.0f, 5);
    const SpeciesId sp = id(key);
    w->inoculate({BeingKind::Species, sp}, 12, {}, 0.0f); // en fases al azar (asincrónicas)
    const float doubling = std::log(2.0f) / speciesPhysiology(sp).growthMax.value /
                           std::pow(2.0f, (w->medium().temperature - 20.0f) / 10.0f);
    const int n0 = w->census().species[static_cast<int>(sp)];
    run(*w, 2.0f * doubling);
    const int n1 = w->census().species[static_cast<int>(sp)];
    const float measured = 2.0f * doubling * std::log(2.0f) / std::log(static_cast<float>(n1) / static_cast<float>(n0));
    MESSAGE(std::string(key) << ": " << n0 << " → " << n1 << " en " << 2.0f * doubling << " h (apoptosis "
                             << w->observations().apoptoses << "): duplicación " << measured << " h (dato " << doubling
                             << " h)");
    CHECK(measured == doctest::Approx(doubling).epsilon(0.25));
}
} // namespace

TEST_CASE("validación: duplicación de la población de HeLa (densidad baja)" * doctest::test_suite("validacion")) {
    checkDoubling("hela");
}

TEST_CASE("validación: duplicación de la población del fibroblasto (densidad baja)" * doctest::test_suite("validacion")) {
    checkDoubling("human_dermal_fibroblast");
}

TEST_CASE("validación: el fibroblasto llega a una meseta en confluencia (inhibición por contacto)" *
          doctest::test_suite("validacion")) {
    auto w = culture(128, 3600.0f, 9);
    const SpeciesId fib = id("human_dermal_fibroblast");
    w->inoculate({BeingKind::Species, fib}, 30, {}, 0.0f);
    std::vector<int> counts;
    for (int k = 0; k < 8; ++k) {
        run(*w, 20.0f);
        counts.push_back(w->census().species[static_cast<int>(fib)]);
    }
    const float area = 3.14159265f * w->dish().radius * w->dish().radius * 1e-8f; // [cm²]
    const float plateau = static_cast<float>(counts.back()) / area;
    MESSAGE("fibroblastos cada 20 h: " << counts[0] << " " << counts[2] << " " << counts[4] << " " << counts[6] << " "
                                       << counts.back() << "; meseta " << plateau << " /cm² (dato "
                                       << speciesPhysiology(fib).confluence.value << ")");
    // Meseta: en las últimas 40 h crece menos del 15 % (sin inhibición seguiría duplicando).
    CHECK(static_cast<float>(counts.back()) < 1.15f * static_cast<float>(counts[5]));
    CHECK(plateau == doctest::Approx(speciesPhysiology(fib).confluence.value).epsilon(0.5));
}

TEST_CASE("validación: el neutrófilo migra a la rapidez medida en un gradiente de fMLP" *
          doctest::test_suite("validacion")) {
    auto w = culture(128, 60.0f, 3);
    const SpeciesId neu = id("neutrophil");
    const entt::entity e = spawnAt(*w, neu, center(*w));
    REQUIRE(static_cast<bool>(e != entt::null));
    const Vec2 a = w->registry().get<RingBody>(e).centroid;
    run(*w, 0.1f); // 6 min
    const Vec2 b = w->registry().get<RingBody>(e).centroid;
    // Recorrido real (con giros) ≥ desplazamiento: se mide la rapidez instantánea del motor.
    const float speed = w->crawlSpeed(e) * 60.0f / w->bioTimeScale(); // [µm/min biológico]
    MESSAGE("rapidez " << speed << " µm/min; desplazamiento en 6 min " << length(b - a) << " µm");
    CHECK(speed == doctest::Approx(15.0f).epsilon(0.05)); // Zigmond 1977: ~10–20 µm/min
    CHECK(length(b - a) > 20.0f);
}
