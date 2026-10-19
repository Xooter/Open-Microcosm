// Reactivos con mecanismo y dosis-respuesta (fase P1, paso 11).
#include "game/culture.hpp"
#include "game/dose_fit.hpp"
#include "game/prepared.hpp"
#include "game/reagent.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

const Reagent& byKey(const char* key) {
    const int i = findReagent(key);
    REQUIRE_MESSAGE(i >= 0, key);
    return reagent(i);
}

// Dilución en caldo (CLSI M07): caldo Mueller-Hinton a 35 °C, inóculo de 5·10⁵ bacterias/mL, 18 h.
// Devuelve las bacterias por mL al final con `ug` µg/mL del reactivo.
double brothFinal(const char* key, double ug) {
    CultureConfig c;
    c.volumeMl = 0.1f; // un pocillo de placa de 96
    c.depthUm = 3000.0f;
    c.seed = 7;
    if (const CultureMedium* m = findCultureMedium("mueller_hinton"))
        c.medium = m->composition;
    c.medium.temperature = 35.0f;
    c.medium.phage = 0.0f;
    Culture culture(c);
    culture.inoculateBacteria(50'000);
    culture.setReagent(findReagent(key), ug);
    for (int s = 0; s < 18 * 60; ++s)
        culture.step(60.0f);
    return static_cast<double>(culture.bacteriaCount()) / culture.volumeMl();
}

// La CIM que emerge: la menor dosis de la serie al doble sin crecimiento visible (turbidez: ~10⁷
// bacterias/mL, ESTIMACIÓN del umbral a simple vista).
double emergentMic(const char* key) {
    std::vector<double> doses, finals;
    for (double d = 0.125; d <= 64.0; d *= 2.0) {
        doses.push_back(d);
        finals.push_back(brothFinal(key, d));
        MESSAGE(key << " " << d << " ug/mL: " << finals.back() << " bacterias/mL a las 18 h");
    }
    return minimumInhibitory(doses, finals, 1e7);
}

} // namespace

TEST_CASE("reactivo: la función farmacodinámica de Regoes") {
    const Reagent& amp = byKey("ampicillin");
    CHECK(reagentNetGrowth(amp, 0.0) == doctest::Approx(amp.psiMax));
    CHECK(reagentNetGrowth(amp, amp.zMic) == doctest::Approx(0.0).epsilon(1e-9)); // zMIC: crecimiento neto 0
    CHECK(reagentNetGrowth(amp, 1e6) == doctest::Approx(amp.psiMin).epsilon(0.01));
    // Beta-lactámico: a una bacteria que no crece no la mata.
    CHECK(reagentKillRate(amp, 100.0, 0.0) == 0.0);
    CHECK(reagentKillRate(amp, 100.0, amp.psiMax) > 3.0);
    CHECK(amp.affects("bacteria"));
    CHECK_FALSE(amp.affects("protistas"));
    CHECK(reagentLoadWarnings().empty());
}

TEST_CASE("dosis-respuesta: el ajuste de Hill recupera la CI50") {
    DoseFit truth;
    truth.top = 100.0;
    truth.bottom = 5.0;
    truth.ic50 = 3.0;
    truth.hill = 1.5;
    std::vector<double> d, y;
    for (double x = 0.25; x <= 64.0; x *= 2.0) {
        d.push_back(x);
        y.push_back(truth(x));
    }
    d.push_back(0.0);
    y.push_back(100.0);
    const DoseFit f = fitDoseResponse(d, y);
    REQUIRE(f.valid);
    CHECK(f.ic50 == doctest::Approx(3.0).epsilon(0.03));
    CHECK(f.hill == doctest::Approx(1.5).epsilon(0.05));
    CHECK(f.r2 > 0.999);
    CHECK(minimumInhibitory(d, y, 50.0) == doctest::Approx(4.0));
}

TEST_CASE("reactivo en la placa: el gotero de ampicilina mata a las bacterias que crecen") {
    auto run = [](bool drug) {
        WorldConfig c;
        c.gridX = c.gridY = 64;
        c.bacteria = 3000;
        c.detritus = 0;
        c.cyanoFilaments = 0;
        c.saprolegnia = c.chytridClusters = 0;
        c.bdellovibrioFraction = 0.0f;
        c.medium.phage = 0.0f;
        c.medium.organic = 2000.0f; // crecen
        c.medium.temperature = 30.0f;
        World w(c);
        if (drug) {
            w.setReagent(findReagent("ampicillin"));
            w.setMeanConcentration(w.reagentField(), 50.0f); // ~15 zMIC
        }
        for (int s = 0; s < 1800; ++s) // 30 s = 30 min biológicos
            w.step(1.0f / 60.0f);
        return w.bacteria().size();
    };
    const std::size_t control = run(false), treated = run(true);
    MESSAGE("bacterias: control " << control << ", con ampicilina " << treated);
    CHECK(treated < control / 2);
}

TEST_CASE("validación: la CIM de la ampicilina emerge de la dilución en caldo" * doctest::test_suite("validacion")) {
    // La simulación usa la farmacodinámica de las curvas de muerte (Regoes et al. 2004), no la CIM.
    // La CIM medida por dilución al doble para la misma cepa es 8 µg/mL: la que emerge tiene que
    // caer a una dilución de distancia (la reproducibilidad de la prueba es ±1 dilución).
    const double mic = emergentMic("ampicillin");
    MESSAGE("CIM simulada " << mic << " ug/mL, publicada " << byKey("ampicillin").publishedMic);
    CHECK(mic >= byKey("ampicillin").publishedMic / 2.0);
    CHECK(mic <= byKey("ampicillin").publishedMic * 2.0);
}

TEST_CASE("validación: la CIM de la tetraciclina emerge de la dilución en caldo" * doctest::test_suite("validacion")) {
    const double mic = emergentMic("tetracycline");
    MESSAGE("CIM simulada " << mic << " ug/mL, publicada " << byKey("tetracycline").publishedMic);
    CHECK(mic >= byKey("tetracycline").publishedMic / 2.0);
    CHECK(mic <= byKey("tetracycline").publishedMic * 2.0);
}
