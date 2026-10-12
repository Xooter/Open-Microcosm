#include "game/world.hpp"
#include "sim/chem_field.hpp"
#include "sim/fft.hpp"
#include "sim/fluid.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <algorithm>
#include <cmath>

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
