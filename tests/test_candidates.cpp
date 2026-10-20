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
TEST_CASE("candidatos: Gonium libera colonias hijas" * doctest::test_suite("largos")) { reproduces("gonium"); }
TEST_CASE("candidatos: Pandorina libera colonias hijas" * doctest::test_suite("largos")) { reproduces("pandorina"); }
TEST_CASE("candidatos: Eudorina libera colonias hijas" * doctest::test_suite("largos")) { reproduces("eudorina"); }
TEST_CASE("candidatos: Stylonychia se divide" * doctest::test_suite("largos")) { reproduces("stylonychia"); }
TEST_CASE("candidatos: Frontonia se divide" * doctest::test_suite("largos")) { reproduces("frontonia"); }
TEST_CASE("candidatos: Tokophrya se reproduce" * doctest::test_suite("largos")) { reproduces("tokophrya"); }
TEST_CASE("candidatos: Cryptomonas se divide" * doctest::test_suite("largos")) { reproduces("cryptomonas"); }
TEST_CASE("candidatos: Coleps se divide" * doctest::test_suite("largos")) { reproduces("coleps"); }
TEST_CASE("candidatos: Blepharisma se divide" * doctest::test_suite("largos")) { reproduces("blepharisma"); }
TEST_CASE("candidatos: Colpoda se divide" * doctest::test_suite("largos")) { reproduces("colpoda"); }
TEST_CASE("candidatos: Chilodonella se divide" * doctest::test_suite("largos")) { reproduces("chilodonella"); }
TEST_CASE("candidatos: Litonotus se divide" * doctest::test_suite("largos")) { reproduces("litonotus"); }
TEST_CASE("candidatos: Acineta se reproduce" * doctest::test_suite("largos")) { reproduces("acineta"); }
TEST_CASE("candidatos: Peranema se divide" * doctest::test_suite("largos")) { reproduces("peranema"); }
TEST_CASE("candidatos: Trachelomonas se divide" * doctest::test_suite("largos")) { reproduces("trachelomonas"); }
TEST_CASE("candidatos: Haematococcus se divide" * doctest::test_suite("largos")) { reproduces("haematococcus"); }
TEST_CASE("candidatos: Ceratium se divide" * doctest::test_suite("largos")) { reproduces("ceratium"); }
TEST_CASE("candidatos: Chaos se divide" * doctest::test_suite("largos")) { reproduces("chaos"); }
TEST_CASE("candidatos: Euglypha se divide" * doctest::test_suite("largos")) { reproduces("euglypha"); }
TEST_CASE("candidatos: Closterium se divide" * doctest::test_suite("largos")) { reproduces("closterium"); }
TEST_CASE("candidatos: Cosmarium se divide" * doctest::test_suite("largos")) { reproduces("cosmarium"); }
TEST_CASE("candidatos: Micrasterias se divide" * doctest::test_suite("largos")) { reproduces("micrasterias"); }
TEST_CASE("candidatos: Gomphonema se divide" * doctest::test_suite("largos")) { reproduces("gomphonema"); }
TEST_CASE("candidatos: Halteria se divide" * doctest::test_suite("largos")) { reproduces("halteria"); }
TEST_CASE("candidatos: Epistylis se divide" * doctest::test_suite("largos")) { reproduces("epistylis"); }
TEST_CASE("candidatos: Ulothrix crece en filamento" * doctest::test_suite("largos")) { reproduces("ulothrix"); }
TEST_CASE("candidatos: Phaeodactylum se divide" * doctest::test_suite("largos")) { reproduces("phaeodactylum"); }
TEST_CASE("candidatos: Thalassiosira se divide" * doctest::test_suite("largos")) { reproduces("thalassiosira"); }
TEST_CASE("candidatos: Fragilariopsis se divide" * doctest::test_suite("largos")) { reproduces("fragilariopsis"); }
TEST_CASE("candidatos: Polarella se divide" * doctest::test_suite("largos")) { reproduces("polarella"); }
TEST_CASE("candidatos: Staurastrum se divide" * doctest::test_suite("largos")) { reproduces("staurastrum"); }
TEST_CASE("candidatos: Peridinium se divide" * doctest::test_suite("largos")) { reproduces("peridinium"); }
TEST_CASE("candidatos: Spirostomum se divide" * doctest::test_suite("largos")) { reproduces("spirostomum"); }
TEST_CASE("candidatos: Dileptus se divide" * doctest::test_suite("largos")) { reproduces("dileptus"); }
TEST_CASE("candidatos: Lembadion se divide" * doctest::test_suite("largos")) { reproduces("lembadion"); }
TEST_CASE("candidatos: Euplotes octocarinatus se divide" * doctest::test_suite("largos")) { reproduces("euplotes_octocarinatus"); }
TEST_CASE("candidatos: Tetrahymena vorax se divide" * doctest::test_suite("largos")) { reproduces("tetrahymena_vorax"); }
TEST_CASE("candidatos: Ochromonas se divide" * doctest::test_suite("largos")) { reproduces("ochromonas"); }
TEST_CASE("candidatos: Actinophrys se divide" * doctest::test_suite("largos")) { reproduces("actinophrys"); }
TEST_CASE("candidatos: Vampyrella se divide" * doctest::test_suite("largos")) { reproduces("vampyrella"); }
TEST_CASE("candidatos: Salpingoeca se divide" * doctest::test_suite("largos")) { reproduces("salpingoeca"); }
TEST_CASE("candidatos: Lingulodinium se divide" * doctest::test_suite("largos")) { reproduces("lingulodinium"); }
TEST_CASE("candidatos: Paramecium bursaria se divide" * doctest::test_suite("largos")) { reproduces("paramecium_bursaria"); }
TEST_CASE("candidatos: Mesodinium se divide" * doctest::test_suite("largos")) { reproduces("mesodinium"); }
TEST_CASE("candidatos: Paramecium tetraurelia se divide" * doctest::test_suite("largos")) { reproduces("paramecium_tetraurelia"); }
