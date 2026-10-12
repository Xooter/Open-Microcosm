#include "game/world.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <doctest/doctest.h>
#include <omp.h>

#include <algorithm>
#include <cmath>
#include <vector>

TEST_CASE("polígonos: área, contención y triangulación de un polígono cóncavo") {
    // Forma de L (cóncava), área 3.
    const std::vector<Vec2> l = {{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    const int n = static_cast<int>(l.size());
    CHECK(polygon::signedArea(l.data(), n) == doctest::Approx(3.0f));
    CHECK(polygon::contains(l.data(), n, {0.5f, 1.5f}));
    CHECK_FALSE(polygon::contains(l.data(), n, {1.5f, 1.5f})); // el hueco de la L

    std::vector<int> tris, scratch;
    REQUIRE(polygon::triangulate(l.data(), n, tris, scratch));
    CHECK(tris.size() == 3u * (n - 2));
    float sum = 0.0f;
    for (std::size_t t = 0; t < tris.size(); t += 3) {
        const Vec2 tri[3] = {l[tris[t]], l[tris[t + 1]], l[tris[t + 2]]};
        sum += std::abs(polygon::signedArea(tri, 3));
    }
    CHECK(sum == doctest::Approx(3.0f));
}

namespace {

WorldConfig cellWorld() {
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.seed = 99;
    return config;
}

struct BodyStats {
    float maxAreaError = 0.0f;    // relativo
    float maxEdgeStretch = 0.0f;  // largo / reposo
    int intruders = 0;            // partículas de membrana dentro de otra célula
    bool finite = true;
};

BodyStats measure(const World& world) {
    BodyStats st;
    const BodyParticles& bp = world.bodies();
    std::vector<const RingBody*> bodies;
    for (auto [e, body] : world.registry().view<RingBody>().each())
        bodies.push_back(&body);
    for (const RingBody* body : bodies) {
        const float a = soft_body::area(*body, bp);
        st.maxAreaError = std::max(st.maxAreaError, std::abs(a - body->restArea) / std::abs(body->restArea));
        const std::size_t n = body->ring.size();
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t s = body->ring[i], t = body->ring[(i + 1) % n];
            st.finite &= std::isfinite(bp.x[s]) && std::isfinite(bp.y[s]);
            const float len = std::hypot(bp.x[t] - bp.x[s], bp.y[t] - bp.y[s]);
            st.maxEdgeStretch = std::max(st.maxEdgeStretch, len / body->restEdge[i]);
        }
        for (const RingBody* other : bodies) {
            if (other == body)
                continue;
            for (std::uint32_t s : body->ring) {
                const Vec2 q = other->centroid + Vec2{minImage(bp.x[s] - other->centroid.x, world.width()),
                                                      minImage(bp.y[s] - other->centroid.y, world.height())};
                if (polygon::contains(other->poly.data(), static_cast<int>(other->poly.size()), q))
                    ++st.intruders;
            }
        }
    }
    return st;
}

} // namespace

TEST_CASE("50 células apretadas no explotan ni se atraviesan, y se recuperan") {
    World world(cellWorld());
    world.spawnTestScene(50);
    REQUIRE(world.cellCount() == 50);

    const Vec2 center = {world.width() * 0.5f, world.height() * 0.5f};
    BodyStats worst;
    for (int s = 0; s < 600; ++s) { // 10 s comprimiendo a 40 µm/s
        world.setSqueeze(center, 40.0f);
        world.step(1.0f / 60.0f);
        if (s % 60 == 59) {
            const BodyStats st = measure(world);
            worst.maxAreaError = std::max(worst.maxAreaError, st.maxAreaError);
            worst.maxEdgeStretch = std::max(worst.maxEdgeStretch, st.maxEdgeStretch);
            worst.intruders = std::max(worst.intruders, st.intruders);
            worst.finite &= st.finite;
        }
    }
    world.clearSqueeze();
    for (int s = 0; s < 300; ++s)
        world.step(1.0f / 60.0f);
    const BodyStats after = measure(world);

    MESSAGE("área máx. " << worst.maxAreaError * 100 << "%, estiramiento máx. "
                         << worst.maxEdgeStretch << "x, intrusiones máx. " << worst.intruders);
    CHECK(worst.finite);
    CHECK(worst.maxAreaError < 0.05f);
    CHECK(worst.maxEdgeStretch < 2.0f);
    CHECK(worst.intruders == 0);
    CHECK(after.finite);
    CHECK(after.intruders == 0);
}

TEST_CASE("una célula con película rígida recupera su forma después de estirarla") {
    World world(cellWorld());
    CellSpec spec;
    spec.center = {1000.0f, 500.0f};
    spec.radiusX = 50.0f;
    spec.radiusY = 25.0f;
    spec.shapeRate = 8.0f;
    const entt::entity cell = world.spawnCell(spec);
    const RingBody& body = world.registry().get<RingBody>(cell);

    REQUIRE(world.beginGrab({1050.0f, 500.0f}));
    world.moveGrab({1110.0f, 500.0f}); // tirar 60 µm hacia afuera
    for (int s = 0; s < 60; ++s)
        world.step(1.0f / 60.0f);
    const float stretched = soft_body::shapeError(body, world.bodies());
    world.endGrab();
    for (int s = 0; s < 180; ++s) // 3 s
        world.step(1.0f / 60.0f);
    const float recovered = soft_body::shapeError(body, world.bodies());

    MESSAGE("error de forma: estirada " << stretched << " µm, recuperada " << recovered << " µm");
    CHECK(stretched > 1.5f); // se deformó de forma medible
    CHECK(recovered < 0.1f * stretched);
}

namespace {
std::uint64_t runCells(int threads) {
    omp_set_num_threads(threads);
    WorldConfig config = cellWorld();
    config.bacteria = 500;
    World world(config);
    world.spawnTestScene(12);
    for (int s = 0; s < 120; ++s) {
        world.setSqueeze({world.width() * 0.5f, world.height() * 0.5f}, 40.0f);
        world.step(1.0f / 60.0f);
    }
    return world.stateHash();
}
} // namespace

TEST_CASE("células deterministas sin importar la cantidad de hilos") {
    const std::uint64_t a = runCells(1);
    CHECK(a == runCells(1));
    CHECK(a == runCells(4));
}
