// Seres sumados desde docs/candidatos.md: cada uno se reproduce por su vía normal y sus hijas son
// cuerpos válidos (un caso por ser: corren en paralelo con ctest -j).
#include "bio/species.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

namespace {

constexpr float kDt = 1.0f / 60.0f;

bool simplePolygon(const RingBody& b, const BodyParticles& bp) {
    auto p = [&](std::size_t i) {
        const std::uint32_t s = b.ring[i % b.ring.size()];
        return Vec2{bp.x[s], bp.y[s]};
    };
    const std::size_t n = b.ring.size();
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1)
                continue;
            const Vec2 a = p(i), c = p(i + 1), d = p(j), e = p(j + 1);
            if (cross(c - a, d - a) * cross(c - a, e - a) < 0.0f && cross(e - d, a - d) * cross(e - d, c - d) < 0.0f)
                return false;
        }
    return true;
}

int countOf(World& world, SpeciesId id) {
    int n = 0;
    for (auto [e, sp] : world.registry().view<Species>().each())
        n += sp.id == id ? 1 : 0;
    return n;
}

// Siembra uno y fuerza su reproducción: en 3 min tiene que haber hijas, que al separarse son polígonos
// simples; 20 s después siguen enteras, con un área parecida a la de reposo (no explotaron ni
// colapsaron).
void reproduces(const char* key) {
    CAPTURE(std::string(key));
    const auto id = findSpecies(key);
    REQUIRE(id);
    WorldConfig config;
    config.bacteria = 20'000;
    config.detritus = 0;
    config.seed = 51;
    World world(config);
    const Vec2 c = {0.5f * world.width(), 0.5f * world.height()};
    const entt::entity e = world.spawnOrganism(*id, c, speciesGenome(*id), 0);
    REQUIRE(world.forceReproduction(e));
    for (int s = 0; s < 60 * 180 && countOf(world, *id) < 2; ++s)
        world.step(kDt);
    REQUIRE(countOf(world, *id) >= 2);
    for (auto [cell, sp, body] : world.registry().view<Species, RingBody>().each())
        if (sp.id == *id)
            CHECK(simplePolygon(body, world.bodies()));
    for (int s = 0; s < 60 * 20; ++s)
        world.step(kDt);
    for (auto [cell, sp, body] : world.registry().view<Species, RingBody>().each()) {
        if (sp.id != *id)
            continue;
        CHECK(std::isfinite(body.centroid.x));
        CHECK(std::isfinite(body.centroid.y));
        const float area = std::abs(polygon::signedArea(body.poly.data(), static_cast<int>(body.poly.size())));
        CHECK(area > 0.3f * std::abs(body.restArea));
        CHECK(area < 2.0f * std::abs(body.restArea));
    }
}

} // namespace

TEST_CASE("candidatos: Tetrahymena se divide" * doctest::test_suite("largos")) { reproduces("tetrahymena"); }
TEST_CASE("candidatos: Colpidium se divide" * doctest::test_suite("largos")) { reproduces("colpidium"); }
TEST_CASE("candidatos: Saccharomyces brota" * doctest::test_suite("largos")) { reproduces("saccharomyces"); }
TEST_CASE("candidatos: Chlorella forma autosporas" * doctest::test_suite("largos")) { reproduces("chlorella"); }
TEST_CASE("candidatos: Raphidocelis forma autosporas" * doctest::test_suite("largos")) { reproduces("raphidocelis"); }
TEST_CASE("candidatos: Desmodesmus forma autosporas" * doctest::test_suite("largos")) { reproduces("desmodesmus"); }
TEST_CASE("candidatos: Phacus se divide" * doctest::test_suite("largos")) { reproduces("phacus"); }
TEST_CASE("candidatos: Pinnularia se divide" * doctest::test_suite("largos")) { reproduces("pinnularia"); }
