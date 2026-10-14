#include "game/region.hpp"

#include "core/profiling.hpp"
#include "game/world.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>

float polygonArea(const std::vector<Vec2>& polygon) {
    return polygon.size() < 3 ? 0.0f : std::abs(polygon::signedArea(polygon.data(), static_cast<int>(polygon.size())));
}

float polylineLength(const std::vector<Vec2>& points) {
    float length = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i)
        length += std::sqrt(dot(points[i] - points[i - 1], points[i] - points[i - 1]));
    return length;
}

RegionStats quantifyRegion(const World& world, const std::vector<Vec2>& polygon) {
    PROFILE_SCOPE("quantifyRegion");
    RegionStats s;
    if (polygon.size() < 3)
        return s;
    const Vec2* p = polygon.data();
    const int n = static_cast<int>(polygon.size());
    s.valid = true;
    s.area = polygonArea(polygon);
    Vec2 lo = polygon[0], hi = polygon[0];
    for (const Vec2& q : polygon) {
        lo = {std::min(lo.x, q.x), std::min(lo.y, q.y)};
        hi = {std::max(hi.x, q.x), std::max(hi.y, q.y)};
    }
    auto inside = [&](Vec2 q) {
        return q.x >= lo.x && q.x <= hi.x && q.y >= lo.y && q.y <= hi.y && polygon::contains(p, n, q);
    };

    // Células: los mismos individuos que cuenta el censo.
    const entt::registry& reg = world.registry();
    for (auto [e, body] : reg.view<RingBody>().each()) {
        if (reg.all_of<Rock>(e) || !inside(body.centroid))
            continue;
        ++s.cells;
        if (const Species* sp = reg.try_get<Species>(e))
            ++s.species[static_cast<int>(sp->id)];
        if (const Metabolism* m = reg.try_get<Metabolism>(e))
            s.biomass += m->biomass;
    }
    const Bacteria& bac = world.bacteria();
    for (std::size_t i = 0; i < bac.size(); ++i)
        s.bacteria += inside({bac.x[i], bac.y[i]}) ? 1 : 0;

    // Química: celdas de agua con el centro adentro.
    const Grid2D& g = world.oxygen().grid();
    const DishMask& mask = world.dishMask();
    const int x0 = std::max(0, static_cast<int>(std::floor(lo.x / g.dx))), x1 = std::min(g.nx - 1, static_cast<int>(hi.x / g.dx));
    const int y0 = std::max(0, static_cast<int>(std::floor(lo.y / g.dx))), y1 = std::min(g.ny - 1, static_cast<int>(hi.y / g.dx));
    double o2 = 0, ph = 0, po4 = 0, nit = 0, si = 0, fe = 0, b12 = 0, org = 0;
    for (int j = y0; j <= y1; ++j)
        for (int i = x0; i <= x1; ++i) {
            const std::size_t k = static_cast<std::size_t>(j) * static_cast<std::size_t>(g.nx) + static_cast<std::size_t>(i);
            if (!mask.isWater(k) || !polygon::contains(p, n, {(i + 0.5f) * g.dx, (j + 0.5f) * g.dx}))
                continue;
            ++s.waterCells;
            o2 += world.oxygen().grid().v[k];
            ph += world.ph().v[k];
            po4 += world.phosphate().grid().v[k];
            nit += world.nitrogen().grid().v[k];
            si += world.silicate().grid().v[k];
            fe += world.iron().grid().v[k];
            b12 += world.vitamin().grid().v[k];
            org += world.nutrients().grid().v[k];
        }
    if (s.waterCells > 0) {
        const double w = s.waterCells;
        s.oxygen = static_cast<float>(o2 / w);
        s.ph = static_cast<float>(ph / w);
        s.phosphate = static_cast<float>(po4 / w);
        s.nitrogen = static_cast<float>(nit / w);
        s.silicate = static_cast<float>(si / w);
        s.iron = static_cast<float>(fe / w);
        s.vitamin = static_cast<float>(b12 / w);
        s.organic = static_cast<float>(org / w);
    }
    return s;
}
