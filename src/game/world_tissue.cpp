// Tejidos de los preparados (fase 20c): genera las paredes celulares de un tejido vegetal, pone un
// protoplasto vivo en cada célula (si el tejido está vivo) y aplica la ósmosis: el volumen del
// protoplasto sigue la osmolaridad del medio de montaje (Boyle–van 't Hoff); la pared, rígida, lo
// limita (turgencia) y en un medio hipertónico el protoplasto se despega (plasmólisis).
#include "game/world.hpp"

#include "bio/osmosis.hpp"
#include "core/finite.hpp"
#include "core/profiling.hpp"
#include "sim/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace {

// Espesor de la capa de citoplasma entre la vacuola central y la membrana [µm] (en la epidermis de
// cebolla, unos pocos µm).
constexpr float kCorticalLayer = 4.0f;

float hash01(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return static_cast<float>(x & 0xFFFFFFu) / 16777216.0f;
}

// Clave de un vértice compartido (redondeado a 0,01 µm): dos células vecinas ven el mismo.
std::pair<long long, long long> keyOf(Vec2 p) {
    return {std::llround(p.x * 100.0f), std::llround(p.y * 100.0f)};
}

// El mismo corrimiento al azar para un vértice compartido (las paredes de dos células vecinas son
// la misma pared).
Vec2 jitterShared(Vec2 p, float amount, std::uint64_t seed) {
    const auto [kx, ky] = keyOf(p);
    const auto h = static_cast<std::uint32_t>(static_cast<std::uint64_t>(kx) * 73856093ull ^
                                              static_cast<std::uint64_t>(ky) * 19349663ull ^ seed);
    return p + Vec2{amount * (hash01(h) - 0.5f), amount * (hash01(h * 2654435761u + 1u) - 0.5f)};
}

Vec2 centroidOf(const std::vector<Vec2>& p) {
    Vec2 c;
    for (const Vec2& q : p)
        c = c + q;
    return p.empty() ? c : c * (1.0f / static_cast<float>(p.size()));
}

// Pared ondulada (epidermis de hoja): la arista entre dos vértices se reemplaza por una curva; la
// curva depende solo de la arista (sus extremos ordenados), así la ven igual las dos células.
void wavyEdge(Vec2 a, Vec2 b, float amplitude, std::uint64_t seed, std::vector<Vec2>& out) {
    const auto ka = keyOf(a), kb = keyOf(b);
    const bool flip = kb < ka;
    const Vec2 p0 = flip ? b : a, p1 = flip ? a : b;
    const auto h = static_cast<std::uint32_t>(static_cast<std::uint64_t>(ka.first + kb.first) * 83492791ull ^
                                              static_cast<std::uint64_t>(ka.second + kb.second) * 2971215073ull ^ seed);
    const float lobes = 1.0f + std::floor(2.0f * hash01(h));
    const float amp = amplitude * (0.6f + 0.8f * hash01(h + 7u)) * (hash01(h + 13u) < 0.5f ? -1.0f : 1.0f);
    const Vec2 d = p1 - p0;
    const float len = length(d);
    const Vec2 n = len > 0.0f ? Vec2{-d.y / len, d.x / len} : Vec2{};
    constexpr int kSteps = 8;
    std::vector<Vec2> pts;
    for (int k = 0; k < kSteps; ++k) {
        const float t = static_cast<float>(k) / kSteps;
        pts.push_back(p0 + d * t + n * (amp * std::sin(3.14159265f * lobes * t)));
    }
    if (flip) { // recorrida al revés: de a hacia b
        out.push_back(a);
        for (int k = kSteps - 1; k >= 1; --k)
            out.push_back(pts[static_cast<std::size_t>(k)]);
    } else {
        for (const Vec2& q : pts)
            out.push_back(q);
    }
}

} // namespace

void World::buildTissue(const Tissue& spec) {
    PROFILE_SCOPE("World::buildTissue");
    tissue_ = spec;
    tissue_.cells.clear();
    if (!dish_.active())
        return;
    const Vec2 c = dish_.center;
    const float R = dish_.radius - 4.0f;
    const float L = spec.cellLength, W = spec.cellWidth;
    const std::uint64_t seed = config_.seed;
    auto inside = [&](const std::vector<Vec2>& poly) {
        for (const Vec2& q : poly)
            if (length(q - c) > R)
                return false;
        return true;
    };
    auto add = [&](std::vector<Vec2> poly) {
        if (poly.size() < 3 || !inside(poly))
            return;
        if (polygon::signedArea(poly.data(), static_cast<int>(poly.size())) < 0.0f)
            std::reverse(poly.begin(), poly.end());
        TissueCell cell;
        cell.wall = std::move(poly);
        cell.center = centroidOf(cell.wall);
        cell.area = polygon::signedArea(cell.wall.data(), static_cast<int>(cell.wall.size()));
        cell.seed = static_cast<std::uint32_t>(tissue_.cells.size()) * 2654435761u ^ static_cast<std::uint32_t>(seed);
        tissue_.cells.push_back(std::move(cell));
    };
    switch (spec.layout) {
    case TissueLayout::Rows: {
        // Hileras de células alargadas: las paredes transversales de una hilera no coinciden con
        // las de la vecina (como ladrillos), y los largos varían.
        // Cada hilera llena la cuerda del campo a su altura (el trozo de epidermis es más grande que
        // el campo): la primera célula, escalonada, y la última, cortada en el borde.
        int row = 0;
        for (float y = c.y - R; y + W <= c.y + R; y += W, ++row) {
            const float far = std::max(std::abs(y - c.y), std::abs(y + W - c.y));
            if (far >= R)
                continue;
            const float half = std::sqrt(R * R - far * far) - 0.5f; // las esquinas, apenas adentro
            Rng rng(seed, rng_stream::kPopulate + 70, static_cast<std::uint64_t>(row), 0);
            const float end = c.x + half;
            float x = c.x - half;
            float len = L * rng.uniform(0.3f, 1.0f);
            while (x < end - 1.0f) {
                if (end - (x + len) < 0.3f * L) // lo que sobra no alcanza para otra célula
                    len = end - x;
                add({{x, y}, {x + len, y}, {x + len, y + W}, {x, y + W}});
                x += len;
                len = L * rng.uniform(0.7f, 1.3f);
            }
        }
        break;
    }
    case TissueLayout::Files:
    case TissueLayout::Jigsaw: {
        // Grilla de vértices compartidos y corridos al azar (meristema, epidermis de hoja).
        const float side = spec.layout == TissueLayout::Files ? W : L;
        const int n = static_cast<int>(std::ceil(2.0f * R / side)) + 1;
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i) {
                const Vec2 o = {c.x - R + side * static_cast<float>(i), c.y - R + side * static_cast<float>(j)};
                const Vec2 v[4] = {jitterShared(o, 0.25f * side, seed), jitterShared(o + Vec2{side, 0.0f}, 0.25f * side, seed),
                                   jitterShared(o + Vec2{side, side}, 0.25f * side, seed),
                                   jitterShared(o + Vec2{0.0f, side}, 0.25f * side, seed)};
                if (spec.layout == TissueLayout::Files) {
                    add({v[0], v[1], v[2], v[3]});
                } else {
                    std::vector<Vec2> poly;
                    for (int k = 0; k < 4; ++k)
                        wavyEdge(v[k], v[(k + 1) % 4], 0.12f * side, seed, poly);
                    add(std::move(poly));
                }
            }
        break;
    }
    case TissueLayout::Polygonal: {
        // Parénquima: hexágonos con los vértices compartidos corridos al azar (poliedros de tamaño
        // parecido, como en un corte).
        const float r = 0.5f * L;
        const float dx = std::sqrt(3.0f) * r, dy = 1.5f * r;
        const int ny = static_cast<int>(std::ceil(2.0f * R / dy)) + 2, nx = static_cast<int>(std::ceil(2.0f * R / dx)) + 2;
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const Vec2 o = {c.x - R + dx * static_cast<float>(i) + (j % 2 ? 0.5f * dx : 0.0f), c.y - R + dy * static_cast<float>(j)};
                std::vector<Vec2> poly;
                for (int k = 0; k < 6; ++k) {
                    const float a = 3.14159265f / 3.0f * static_cast<float>(k) + 3.14159265f / 6.0f;
                    poly.push_back(jitterShared(o + Vec2{r * std::cos(a), r * std::sin(a)}, 0.3f * r, seed));
                }
                add(std::move(poly));
            }
        break;
    }
    }
    // Contenido de cada célula.
    for (TissueCell& cell : tissue_.cells) {
        Rng rng(seed, rng_stream::kPopulate + 71, cell.seed, 0);
        if (spec.content == TissueContent::Mitosis) {
            // Fase de cada célula con la fracción del ciclo que dura (aplastado y fijado: una foto).
            float u = rng.uniform();
            cell.phase = 0;
            for (int p = 0; p < 4; ++p) {
                if (u < spec.mitoticFractions[p]) {
                    cell.phase = static_cast<std::uint8_t>(p + 1);
                    break;
                }
                u -= spec.mitoticFractions[p];
            }
        } else if (spec.content == TissueContent::Stomata) {
            cell.guard = rng.uniform() < 0.12f; // índice estomático ~10–15 % (hojas de dicotiledóneas)
        }
    }
    // Células vivas: un protoplasto (cuerpo blando) en cada una, pegado a la pared por la turgencia.
    if (spec.content == TissueContent::Protoplast && !spec.species.empty()) {
        if (const auto sp = findSpecies(spec.species)) {
            for (std::size_t k = 0; k < tissue_.cells.size(); ++k) {
                TissueCell& cell = tissue_.cells[k];
                Vec2 lo = cell.wall.front(), hi = cell.wall.front();
                for (const Vec2& q : cell.wall) {
                    lo = {std::min(lo.x, q.x), std::min(lo.y, q.y)};
                    hi = {std::max(hi.x, q.x), std::max(hi.y, q.y)};
                }
                CellSpec shape = speciesInfo(*sp).spec;
                const float gap = spec.wallThickness;
                shape.radiusX = std::max(2.0f, 0.5f * (hi.x - lo.x) - gap);
                shape.radiusY = std::max(2.0f, 0.5f * (hi.y - lo.y) - gap);
                shape.squareness = 6.0f; // la forma de la pared: un rectángulo de esquinas redondeadas
                const entt::entity e = spawnOrganism(*sp, cell.center, speciesGenome(*sp), 0, 0.0f, &shape);
                registry_.emplace<Protoplast>(e, Protoplast{static_cast<std::uint32_t>(k)});
                registry_.emplace_or_replace<Adherent>(e); // dentro de su pared: el agua no lo lleva
                cell.protoplast = static_cast<std::uint32_t>(entt::to_integral(e));
                cell.volume = 1.0f;
                // Turgente, apenas más chico que el lugar que deja la pared: si el área meta no cabe,
                // la membrana sobrante se arruga en zigzag contra la pared.
                cell.fullArea = 0.97f * std::abs(registry_.get<RingBody>(e).restArea);
                // El núcleo, en la capa de citoplasma apretada contra la pared (la vacuola central
                // ocupa el resto), en un lugar al azar a lo largo de la célula.
                RingBody& pb = registry_.get<RingBody>(e);
                if (const CellAnatomy* an = registry_.try_get<CellAnatomy>(e))
                    for (std::size_t q = 0; q < pb.interior.size(); ++q)
                        if (pb.interior[q] == an->nucleus) {
                            Rng nr(seed, rng_stream::kPopulate + 74, cell.seed, 0);
                            const float rn = bodies_.radius[an->nucleus];
                            const float along = nr.uniform(-0.6f, 0.6f) * shape.radiusX;
                            const float side = (nr.uniform() < 0.5f ? -1.0f : 1.0f) * std::max(0.0f, shape.radiusY - rn - 2.0f);
                            pb.restInterior[q] = {along, side};
                        }
            }
        }
    }
}

void World::updateTissue(float dt) {
    PROFILE_SCOPE("World::updateTissue");
    if (!tissue_.present() || tissue_.content != TissueContent::Protoplast)
        return;
    // Ósmosis: el agua entra o sale del protoplasto (en tiempo real: es física).
    const float outside = osmosis::mediumOsmolarity(medium_.salinity, medium_.osmolytes);
    const float relax = 1.0f - std::exp(-dt / std::max(1.0f, tissue_.osmoticSeconds));
    for (TissueCell& cell : tissue_.cells) {
        const auto e = static_cast<entt::entity>(cell.protoplast);
        if (cell.protoplast == ~0u || !registry_.valid(e) || !registry_.all_of<RingBody>(e))
            continue;
        // Con la pared llena el jugo celular tiene `sapOsmolarity`; en agua más diluida, la célula
        // querría hincharse más, pero la pared la frena (turgencia): el volumen no pasa de 1.
        const float wanted = std::min(1.0f, osmosis::equilibriumVolume(tissue_.sapOsmolarity, outside, tissue_.inactive));
        cell.volume += (wanted - cell.volume) * relax;
        RingBody& body = registry_.get<RingBody>(e);
        const float target = (cell.fullArea > 0.0f ? cell.fullArea : 0.9f * cell.area) * cell.volume;
        soft_body::setRestArea(body, body.restArea >= 0.0f ? target : -target);
        // Una membrana plasmolizada se redondea (pierde la forma de la pared): la tensión de la
        // membrana manda cuando ya no la aprieta la turgencia.
        body.shapeRate = cell.volume > 0.97f ? 2.0f : 0.3f;
    }
}

void World::confineProtoplasts() {
    PROFILE_SCOPE("World::confineProtoplasts");
    if (!tissue_.present() || tissue_.content != TissueContent::Protoplast)
        return;
    // La pared es rígida: ningún punto del protoplasto la atraviesa.
    const auto n = static_cast<std::ptrdiff_t>(tissue_.cells.size());
#pragma omp parallel for schedule(dynamic, 8)
    for (std::ptrdiff_t k = 0; k < n; ++k) {
        const TissueCell& cell = tissue_.cells[static_cast<std::size_t>(k)];
        const auto e = static_cast<entt::entity>(cell.protoplast);
        if (cell.protoplast == ~0u || !registry_.valid(e) || !registry_.all_of<RingBody>(e))
            continue;
        RingBody& body = registry_.get<RingBody>(e);
        const int pn = static_cast<int>(cell.wall.size());
        for (std::uint32_t s : body.ring) {
            const Vec2 q = {bodies_.x[s], bodies_.y[s]};
            if (!finiteNumber(q.x) || !finiteNumber(q.y))
                continue;
            const polygon::Closest cl = polygon::closestOnBoundary(cell.wall.data(), pn, q);
            const bool in = polygon::contains(cell.wall.data(), pn, q);
            const float margin = tissue_.wallThickness;
            if (in && cl.dist2 >= margin * margin)
                continue;
            // Al borde interior de la pared, hacia el centro de la célula.
            const Vec2 toCenter = cell.center - cl.point;
            const float l = length(toCenter);
            const Vec2 p = l > 1e-3f ? cl.point + toCenter * (margin / l) : cell.center;
            bodies_.x[s] = p.x;
            bodies_.y[s] = p.y;
        }
        soft_body::refresh(body, bodies_);
        // La vacuola central ocupa casi todo: los gránulos viajan en la capa delgada de citoplasma
        // junto a la membrana (la ciclosis los lleva por ahí), no por el medio de la célula.
        const int bn = static_cast<int>(body.poly.size());
        for (std::size_t q = body.organelleCount; q < body.interior.size(); ++q) {
            const std::uint32_t s = body.interior[q];
            const Vec2 p = {bodies_.x[s], bodies_.y[s]};
            if (!finiteNumber(p.x) || !finiteNumber(p.y))
                continue;
            const polygon::Closest cl = polygon::closestOnBoundary(body.poly.data(), bn, p);
            if (cl.dist2 <= kCorticalLayer * kCorticalLayer)
                continue;
            const Vec2 to = p - cl.point;
            const Vec2 moved = cl.point + to * (kCorticalLayer / std::sqrt(cl.dist2));
            bodies_.x[s] = moved.x;
            bodies_.y[s] = moved.y;
        }
    }
}

void World::seedBlood() {
    // Un frotis fino: los glóbulos rojos en una capa, casi sin encimarse (se rechaza el lugar de uno
    // que caería a menos de un diámetro y medio de otro... salvo cuando ya no queda espacio).
    if (config_.erythrocytesPerCm2 <= 0.0f && config_.plateletsPerCm2 <= 0.0f)
        return;
    const float areaCm2 = dish_.active() ? 3.14159265f * dish_.radius * dish_.radius * 1e-8f : width_ * height_ * 1e-8f;
    const int red = static_cast<int>(std::round(config_.erythrocytesPerCm2 * areaCm2));
    const int platelets = static_cast<int>(std::round(config_.plateletsPerCm2 * areaCm2));
    const float cell = 2.0f * erythrocyte::kRadius;
    const int gx = std::max(1, static_cast<int>(width_ / cell)), gy = std::max(1, static_cast<int>(height_ / cell));
    std::vector<std::uint8_t> taken(static_cast<std::size_t>(gx) * static_cast<std::size_t>(gy), 0);
    std::uint64_t k = 0;
    for (int placed = 0, attempts = 0; placed < red && attempts < 20 * red; ++attempts) {
        Rng rng(config_.seed, rng_stream::kPopulate + 72, k++, 0);
        const Vec2 p = randomPoint(rng, erythrocyte::kRadius);
        const int ix = std::clamp(static_cast<int>(p.x / cell), 0, gx - 1), iy = std::clamp(static_cast<int>(p.y / cell), 0, gy - 1);
        std::uint8_t& slot = taken[static_cast<std::size_t>(iy) * static_cast<std::size_t>(gx) + static_cast<std::size_t>(ix)];
        if (slot && attempts < 10 * red)
            continue;
        slot = 1;
        const float crit = erythrocyte::kCriticalVolume + erythrocyte::kCriticalSpread * rng.normal();
        blood_.add(p.x, p.y, rng.uniform(0.0f, 6.2831853f), BloodParticle::Erythrocyte, std::max(1.2f, crit));
        ++placed;
    }
    for (int i = 0; i < platelets; ++i) {
        Rng rng(config_.seed, rng_stream::kPopulate + 73, static_cast<std::uint64_t>(i), 0);
        const Vec2 p = randomPoint(rng, 2.0f);
        blood_.add(p.x, p.y, rng.uniform(0.0f, 6.2831853f), BloodParticle::Platelet);
    }
}

void World::updateBlood(float dt) {
    PROFILE_SCOPE("World::updateBlood");
    if (blood_.size() == 0 || fixed_)
        return;
    // Ósmosis de cada eritrocito (física: tiempo real) y hemólisis al pasar el volumen crítico.
    const float outside = osmosis::mediumOsmolarity(medium_.salinity, medium_.osmolytes);
    const float wanted = osmosis::equilibriumVolume(erythrocyte::kIsotonic, outside, erythrocyte::kInactive);
    const float relax = 1.0f - std::exp(-dt / erythrocyte::kSeconds);
    for (std::size_t i = 0; i < blood_.size(); ++i) {
        // El agua los lleva (sin fijar flotan en la capa de montaje).
        const Vec2 v = fluid_.velocityAt({blood_.x[i], blood_.y[i]});
        blood_.x[i] = wrapCoord(blood_.x[i] + v.x * dt, width_);
        blood_.y[i] = wrapCoord(blood_.y[i] + v.y * dt, height_);
        if (blood_.kind[i] != BloodParticle::Erythrocyte || blood_.lysed[i])
            continue;
        blood_.volume[i] += (wanted - blood_.volume[i]) * relax;
        if (blood_.volume[i] > blood_.critical[i]) // se rompe: queda el fantasma
            blood_.lysed[i] = 1;
    }
}
