// Preparados en portaobjetos (fase 20c): ósmosis (Boyle–van 't Hoff), plasmólisis de la epidermis
// de cebolla, fragilidad osmótica de los glóbulos rojos, mitosis de la punta de raíz y frotis de
// sangre.
#include "bio/osmosis.hpp"
#include "game/plate_spec.hpp"
#include "game/scene.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <string>

namespace {

constexpr float kDt = 1.0f / 60.0f;

PlateSpec preparedPlate(const char* id, std::uint64_t seed = 3) {
    PlateSpec p;
    p.prepared = id;
    p.seed = seed;
    p.rebuild();
    return p;
}

void steps(World& w, int n) {
    for (int s = 0; s < n; ++s)
        w.step(kDt);
}

// NaCl [g/kg] para una osmolaridad [mOsm/L] (la inversa de osmosis::mediumOsmolarity).
float salinityFor(float osmolarity) { return osmolarity / (2.0f * 0.93f * 1000.0f / 58.44f); }

} // namespace

TEST_CASE("ósmosis: la osmolaridad del medio y el volumen de Boyle–van 't Hoff") {
    // Solución fisiológica (NaCl 0,9 %): ~286 mOsm/L medidos; agua de mar (35 g/kg): ~1000–1100.
    CHECK(osmosis::mediumOsmolarity(9.0f, 0.0f) == doctest::Approx(286.0f).epsilon(0.02));
    CHECK(osmosis::mediumOsmolarity(35.0f, 0.0f) == doctest::Approx(1100.0f).epsilon(0.05));
    CHECK(osmosis::mediumOsmolarity(0.0f, 300.0f) == doctest::Approx(300.0f)); // sacarosa
    // En el medio isotónico el volumen es el normal; al doble de osmolaridad, Vb + (1 − Vb)/2.
    CHECK(osmosis::equilibriumVolume(290.0f, 290.0f, 0.4f) == doctest::Approx(1.0f));
    CHECK(osmosis::equilibriumVolume(290.0f, 580.0f, 0.4f) == doctest::Approx(0.7f));
}

TEST_CASE("preparado de cebolla: el tejido tiene paredes, protoplastos turgentes en agua, y se plasmoliza con sal") {
    PlateSpec p = preparedPlate("onion_epidermis");
    CHECK(p.kind() == PlateKind::Slide);
    p.config.gridX = p.config.gridY = 64;     // un campo chico: unas pocas células
    p.config.tissue.osmoticSeconds = 2.0f;    // el equilibrio en segundos para el test (el dato: minutos)
    World w(p.config);
    const Tissue& t = w.tissue();
    REQUIRE(t.present());
    int alive = 0;
    for (const TissueCell& c : t.cells)
        alive += c.protoplast != ~0u;
    CHECK(alive == static_cast<int>(t.cells.size()));
    steps(w, 120);
    for (const TissueCell& c : t.cells)
        CHECK(c.volume == doctest::Approx(1.0f)); // en agua: turgente, apretada contra la pared
    // Sal: ~950 mOsm (≈0,5 M de NaCl), más que el jugo celular (450).
    w.medium().salinity = salinityFor(950.0f);
    steps(w, 900);
    const float expected = osmosis::equilibriumVolume(t.sapOsmolarity, 950.0f, t.inactive);
    for (const TissueCell& c : t.cells) {
        CHECK(c.volume == doctest::Approx(expected).epsilon(0.05));
        const RingBody& body = w.registry().get<RingBody>(static_cast<entt::entity>(c.protoplast));
        MESSAGE("plasmolizada: área del protoplasto " << std::abs(body.restArea) / c.area << " de la pared");
        CHECK(std::abs(body.restArea) < 0.7f * c.area);
    }
    // Agua de nuevo: deplasmólisis.
    w.medium().salinity = 0.0f;
    steps(w, 900);
    for (const TissueCell& c : t.cells)
        CHECK(c.volume > 0.97f);
}

TEST_CASE("validación: plasmólisis incipiente de la epidermis de cebolla cuando el medio iguala al jugo celular" *
          doctest::test_suite("validacion")) {
    // El protoplasto empieza a despegarse cuando el medio alcanza la osmolaridad del jugo celular
    // (~450 mOsm, ≈0,24 M de NaCl): por debajo queda turgente; por encima, plasmolizado.
    PlateSpec p = preparedPlate("onion_epidermis");
    p.config.gridX = p.config.gridY = 64;
    p.config.tissue.osmoticSeconds = 1.0f;
    for (const float factor : {0.9f, 1.15f}) {
        World w(p.config);
        w.medium().salinity = salinityFor(factor * w.tissue().sapOsmolarity);
        steps(w, 600);
        float mean = 0.0f;
        for (const TissueCell& c : w.tissue().cells)
            mean += c.volume / static_cast<float>(w.tissue().cells.size());
        MESSAGE("medio a " << factor << " veces el jugo: volumen " << mean);
        if (factor < 1.0f)
            CHECK(mean == doctest::Approx(1.0f));
        else
            CHECK(mean < 0.92f);
    }
}

TEST_CASE("validación: fragilidad osmótica de los glóbulos rojos (Parpart et al. 1947)" * doctest::test_suite("validacion")) {
    // Fracción hemolizada según el NaCl: empieza ~0,50 %, la mitad a ~0,42 %, casi todos ~0,35 %.
    PlateSpec p = preparedPlate("live_blood");
    const std::pair<float, float> expected[] = {{0.55f, 0.02f}, {0.42f, 0.5f}, {0.30f, 0.98f}};
    for (const auto& [percent, fraction] : expected) {
        World w(p.config);
        REQUIRE(w.erythrocytes().size() > 100);
        w.medium().salinity = percent * 10.0f; // % de NaCl → g/kg
        steps(w, 900);                          // 15 s: el equilibrio osmótico tarda segundos
        int red = 0, lysed = 0;
        for (std::size_t i = 0; i < w.erythrocytes().size(); ++i)
            if (w.erythrocytes().kind[i] == BloodParticle::Erythrocyte) {
                ++red;
                lysed += w.erythrocytes().lysed[i];
            }
        const float f = static_cast<float>(lysed) / static_cast<float>(red);
        MESSAGE("NaCl " << percent << " %: hemolizados " << 100.0f * f << " %");
        CHECK(std::abs(f - fraction) < 0.12f);
    }
    // En medio hipertónico se arrugan (crenación) y no se rompen.
    World w(p.config);
    w.medium().salinity = 20.0f;
    steps(w, 600);
    CHECK(w.erythrocytes().volume[0] < 0.8f);
    CHECK(w.erythrocytes().lysed[0] == 0);
}

TEST_CASE("validación: índice mitótico de la punta de raíz de cebolla" * doctest::test_suite("validacion")) {
    PlateSpec p = preparedPlate("onion_root_tip");
    World w(p.config);
    const Tissue& t = w.tissue();
    REQUIRE(t.cells.size() > 200);
    int phases[5] = {};
    for (const TissueCell& c : t.cells)
        ++phases[c.phase];
    const float n = static_cast<float>(t.cells.size());
    const float mitotic = static_cast<float>(phases[1] + phases[2] + phases[3] + phases[4]) / n;
    MESSAGE("células " << t.cells.size() << ": interfase " << phases[0] << ", profase " << phases[1] << ", metafase "
                       << phases[2] << ", anafase " << phases[3] << ", telofase " << phases[4]);
    // Allium cepa: índice mitótico ~8–10 %, la profase la fase más frecuente.
    CHECK(mitotic == doctest::Approx(0.09f).epsilon(0.4));
    CHECK(phases[1] > phases[2]);
    CHECK(phases[1] > phases[3]);
}

TEST_CASE("frotis de sangre: fijado, con glóbulos rojos, plaquetas y los tres tipos de leucocitos") {
    PlateSpec p = preparedPlate("blood_smear");
    World w(p.config);
    Run run;
    run.start(w, plateRunConfig(p), 1);
    CHECK(w.fixed());
    int red = 0, platelets = 0;
    for (std::size_t i = 0; i < w.erythrocytes().size(); ++i)
        (w.erythrocytes().kind[i] == BloodParticle::Erythrocyte ? red : platelets) += 1;
    const float area = 3.14159265f * w.dish().radius * w.dish().radius * 1e-8f;
    MESSAGE("glóbulos rojos " << red << ", plaquetas " << platelets << ", neutrófilos "
                              << w.census().species[static_cast<int>(*findSpecies("neutrophil"))] << ", linfocitos "
                              << w.census().species[static_cast<int>(*findSpecies("lymphocyte"))] << ", monocitos "
                              << w.census().species[static_cast<int>(*findSpecies("monocyte"))]);
    CHECK(static_cast<float>(red) == doctest::Approx(1.1e6f * area).epsilon(0.1));
    CHECK(platelets > 0);
    for (const char* key : {"neutrophil", "lymphocyte", "monocyte"})
        CHECK(w.census().species[static_cast<int>(*findSpecies(key))] >= 1);
    // Fijado: nada se mueve.
    const float x0 = w.erythrocytes().x[0];
    steps(w, 30);
    CHECK(w.erythrocytes().x[0] == x0);
}
