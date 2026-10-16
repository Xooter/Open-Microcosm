#include "game/world.hpp"
#include "sim/chem_field.hpp"
#include "sim/fft.hpp"
#include "sim/fluid.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <algorithm>
#include <cmath>
#include <memory>

TEST_CASE("el agua es incompresible: divergencia espectral ~ 0") {
    const int nx = 64, ny = 32;
    const float dx = 8.0f;
    Fft2D fft(nx, ny);
    Fluid fluid(nx, ny, dx);
    fluid.addForce({100.0f, 120.0f}, {5e5f, -2e5f}, 30.0f);
    fluid.addForce({400.0f, 60.0f}, {-1e5f, 3e5f}, 20.0f);
    fluid.solve(fft);

    std::vector<Complex> ux(fluid.ux().v.begin(), fluid.ux().v.end());
    std::vector<Complex> uy(fluid.uy().v.begin(), fluid.uy().v.end());
    fft.forward(ux);
    fft.forward(uy);

    float maxDiv = 0.0f, maxGrad = 0.0f;
    for (int j = 0; j < ny; ++j) {
        const float ky = wavenumber(j, ny, ny * dx);
        for (int i = 0; i < nx; ++i) {
            const float kx = wavenumber(i, nx, nx * dx);
            const std::size_t k = static_cast<std::size_t>(j) * nx + i;
            maxDiv = std::max(maxDiv, std::abs(kx * ux[k] + ky * uy[k]));
            maxGrad = std::max(maxGrad, std::hypot(kx, ky) * std::max(std::abs(ux[k]), std::abs(uy[k])));
        }
    }
    REQUIRE(maxGrad > 0.0f);
    CHECK(maxDiv / maxGrad < 1e-4f);
}

TEST_CASE("la difusión conserva la masa total") {
    const int nx = 64, ny = 64;
    Fft2D fft(nx, ny);
    ChemField field(nx, ny, 8.0f, 2000.0f, 0.0f);
    field.grid().at(10, 10) = 1000.0f;
    field.grid().at(40, 50) = 250.0f;
    const double before = field.grid().sum();

    for (int s = 0; s < 100; ++s)
        field.diffuse(fft, 1.0f / 60.0f);

    CHECK(field.grid().sum() == doctest::Approx(before).epsilon(1e-4));
    CHECK(field.grid().at(10, 10) < 1000.0f); // efectivamente se difundió
}

namespace {
std::uint64_t runWorld(int threads) {
    omp_set_num_threads(threads);
    WorldConfig config;
    config.gridX = 64;
    config.gridY = 32;
    config.bacteria = 2000;
    config.detritus = 50;
    config.seed = 1234;
    World world(config);
    world.dropInk({200.0f, 100.0f}, 20.0f, 200);
    for (int s = 0; s < 120; ++s) {
        if (s < 60)
            world.setStir({250.0f, 128.0f}, {80.0f, 30.0f}, 25.0f);
        else
            world.clearStir();
        world.step(1.0f / 60.0f);
    }
    return world.stateHash();
}
} // namespace

TEST_CASE("misma seed, mismo resultado, sin importar la cantidad de hilos") {
    const std::uint64_t a = runWorld(1);
    const std::uint64_t b = runWorld(1);
    const std::uint64_t c = runWorld(4);
    CHECK(a == b);
    CHECK(a == c);
}

namespace {

// Medio de cultivo sin vida, en una película chica: solo el CO₂ y el pH.
std::unique_ptr<World> sterileMedium(float celsius, float salinity, float bicarbonate, float pk, float co2Air) {
    WorldConfig c;
    c.gridX = c.gridY = 64;
    c.bacteria = 0;
    c.detritus = 0;
    c.cyanoFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.bdellovibrioFraction = 0.0f;
    c.medium.detritusSpawnRate = 0.0f;
    c.medium.phage = 0.0f;
    c.medium.temperature = celsius;
    c.medium.salinity = salinity;
    c.medium.alkalinity = bicarbonate;
    c.medium.carbonatePK = pk;
    c.medium.co2Atmosphere = co2Air;
    return std::make_unique<World>(c);
}

float meanPH(const World& w) {
    double sum = 0.0;
    int n = 0;
    for (std::size_t k = 0; k < w.ph().v.size(); ++k)
        if (w.dishMask().isWater(k)) {
            sum += w.ph().v[k];
            ++n;
        }
    return static_cast<float>(sum / n);
}

} // namespace

TEST_CASE("CO₂: la solubilidad baja con la temperatura y con la sal (ley de Henry)") {
    const auto cold = sterileMedium(0.0f, 0.5f, 1000.0f, 6.35f, 4.2e-4f);
    const auto mild = sterileMedium(20.0f, 0.5f, 1000.0f, 6.35f, 4.2e-4f);
    const auto warm = sterileMedium(37.0f, 0.5f, 1000.0f, 6.35f, 4.2e-4f);
    const auto sea = sterileMedium(20.0f, 35.0f, 1000.0f, 6.35f, 4.2e-4f);
    CHECK(mild->co2Saturation() == doctest::Approx(15.0f));
    // K_H del CO₂: 0,077 mol/(L·atm) a 0 °C, 0,039 a 20 °C, 0,025 a 37 °C (Sander 2015).
    CHECK(cold->co2Saturation() / mild->co2Saturation() == doctest::Approx(0.077 / 0.039).epsilon(0.08));
    CHECK(warm->co2Saturation() / mild->co2Saturation() == doctest::Approx(0.025 / 0.039).epsilon(0.08));
    // El mar disuelve ~15 % menos (Weiss 1974).
    CHECK(sea->co2Saturation() / mild->co2Saturation() == doctest::Approx(0.85).epsilon(0.03));
}

TEST_CASE("DMEM: el bicarbonato fija el pH con el CO₂ de la incubadora; al aire se alcaliniza") {
    // DMEM alta glucosa: 44 mM de bicarbonato (3,7 g/L de NaHCO₃), formulado para 10 % de CO₂;
    // pK′ 6,1 a 37 °C con la fuerza iónica del medio. Henderson–Hasselbalch: pH 7,4 con 10 % y
    // ~7,7 con 5 % (por eso a 5 % se usa el DMEM con menos bicarbonato).
    auto settle = [](std::unique_ptr<World> w, float seconds) {
        for (int s = 0; s < static_cast<int>(seconds * 60.0f); ++s)
            w->step(1.0f / 60.0f);
        return meanPH(*w);
    };
    auto outside = [] { // recién sacado de la incubadora de 10 %
        auto w = sterileMedium(37.0f, 10.9f, 44000.0f, 6.1f, 0.10f);
        w->medium().co2Atmosphere = 4.2e-4f;
        w->refreshMedium();
        return w;
    };
    const float ph10 = settle(sterileMedium(37.0f, 10.9f, 44000.0f, 6.1f, 0.10f), 2.0f);
    const float ph5 = settle(sterileMedium(37.0f, 10.9f, 44000.0f, 6.1f, 0.05f), 2.0f);
    MESSAGE("DMEM a 37 °C: pH " << ph10 << " con 10 % de CO2, " << ph5 << " con 5 %");
    CHECK(ph10 == doctest::Approx(7.4f).epsilon(0.1f / 7.4f));
    CHECK(ph5 == doctest::Approx(7.7f).epsilon(0.1f / 7.7f));
    // Fuera de la incubadora el CO₂ escapa al aire y el pH sube (el rojo fenol vira a fucsia).
    const float before = settle(outside(), 0.1f);
    const float after = settle(outside(), 60.0f);
    MESSAGE("al aire: pH " << before << " → " << after << " en 1 min");
    CHECK(after > before + 1.0f);
    CHECK(after > 8.2f);
}
