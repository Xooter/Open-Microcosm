#include "bio/cell.hpp"

#include "bio/division.hpp"
#include "bio/silhouette.hpp"
#include "core/units.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kGranuleRadius = 0.4f;                     // [µm]
} // namespace

entt::entity createCell(entt::registry& registry, BodyParticles& bodies, const CellSpec& spec,
                        Rng& rng) {
    const entt::entity e = registry.create();
    const auto id = static_cast<std::uint32_t>(entt::to_integral(e));
    RingBody body;
    body.id = id;
    body.shapeRate = spec.shapeRate;

    const float rx = spec.radiusX, ry = spec.radiusY;
    const float c = std::cos(spec.angle), s = std::sin(spec.angle);
    auto place = [&](Vec2 local) { return spec.center + rotate(local, c, s); };

    // Con silueta (cuerpo + cuernos o brazos), el anillo recorre ese contorno; si no, la superelipse.
    const bool outlined = spec.outline && spec.outline->size() >= 3;
    // Perímetro (polilínea densa) para espaciar la membrana uniformemente.
    float perimeter = 0.0f;
    if (outlined) {
        perimeter = outlinePerimeter(*spec.outline) * spec.outlineScale;
    } else {
        for (int k = 0; k < 256; ++k)
            perimeter += length(superellipsePoint(rx, ry, spec.squareness, 2.0f * units::kPi * (k + 1) / 256.0f, spec.taper) -
                                superellipsePoint(rx, ry, spec.squareness, 2.0f * units::kPi * k / 256.0f, spec.taper));
    }
    // Al menos 48 puntos: en células chicas (Euglena, ~14 µm de ancho) la membrana se afina.
    const int n = std::max(48, static_cast<int>(perimeter / kMembraneSpacing));
    const float membraneRadius = 0.6f * perimeter / static_cast<float>(n);

    std::vector<Vec2> sampled;
    Vec2 sampledMean;
    if (outlined) {
        sampleOutline(*spec.outline, n, spec.outlineScale, 0.0f, false, sampled);
        // El centro del cuerpo blando es el promedio de sus puntos: se centra exacto.
        for (const Vec2& q : sampled)
            sampledMean = sampledMean + q;
        sampledMean = sampledMean * (1.0f / static_cast<float>(n));
        for (Vec2& q : sampled)
            q = q - sampledMean;
    }
    for (int i = 0; i < n; ++i) {
        const float t = 2.0f * units::kPi * static_cast<float>(i) / static_cast<float>(n);
        const Vec2 local = outlined ? sampled[static_cast<std::size_t>(i)] : superellipsePoint(rx, ry, spec.squareness, t, spec.taper);
        const Vec2 p = place(local);
        body.ring.push_back(bodies.allocate(BodyKind::Membrane, p.x, p.y, membraneRadius, 1.0f, id));
        body.restRing.push_back(local);
    }
    // El núcleo, la vacuola contráctil y los gránulos van en el cuerpo, no en los cuernos.
    const Vec2 core = outlined ? spec.outlineBodyCenter * spec.outlineScale - sampledMean : Vec2{};
    for (int i = 0; i < n; ++i) {
        body.restEdge.push_back(length(body.restRing[(i + 1) % n] - body.restRing[i]));
    }
    body.restArea = polygon::signedArea(body.restRing.data(), n);

    auto addInterior = [&](Vec2 local, float radius, float invMass, float anchor) {
        local = local + core;
        const Vec2 p = place(local);
        body.interior.push_back(bodies.allocate(BodyKind::Interior, p.x, p.y, radius, invMass, id));
        body.restInterior.push_back(local);
        body.anchorRate.push_back(anchor);
        return body.interior.back();
    };

    // Organelos: núcleo central anclado al citoesqueleto y vacuola contráctil hacia un extremo.
    const float minR = std::min(rx, ry);
    CellAnatomy anatomy{};
    anatomy.nucleus = addInterior({0.1f * rx, 0.0f}, spec.nucleusFraction * minR, 0.2f, 3.0f);
    anatomy.contractileVacuole = addInterior({-0.6f * rx, 0.25f * ry}, 0.18f * minR, 0.5f, 2.0f);
    body.organelleCount = 2;

    // Gránulos citoplasmáticos libres (con browniano); más adelante, ciclosis.
    const float cellArea = units::kPi * rx * ry;
    const int granules = std::clamp(static_cast<int>(cellArea / 120.0f), 15, 120);
    for (int k = 0; k < granules; ++k) {
        const float rr = 0.8f * std::sqrt(rng.uniform());
        const float a = rng.uniform(0.0f, 2.0f * units::kPi);
        addInterior({rr * rx * std::cos(a), rr * ry * std::sin(a)}, kGranuleRadius, 1.0f, 0.0f);
    }

    soft_body::refresh(body, bodies);
    registry.emplace<RingBody>(e, std::move(body));
    registry.emplace<CellLook>(e, spec.r, spec.g, spec.b);
    registry.emplace<CellAnatomy>(e, anatomy);
    registry.emplace<CellShape>(e, CellShape{rx, ry, spec.shapeRate, spec.nucleusFraction, spec.squareness, spec.taper});
    return e;
}

void destroyCell(entt::registry& registry, BodyParticles& bodies, entt::entity cell) {
    const RingBody& body = registry.get<RingBody>(cell);
    for (std::uint32_t s : body.ring)
        bodies.release(s);
    for (std::uint32_t s : body.interior)
        bodies.release(s);
    registry.destroy(cell);
}

CellSpec randomCellSpec(Rng& rng, Vec2 center) {
    CellSpec spec;
    spec.center = center;
    const float r = rng.uniform(25.0f, 55.0f);
    const float aspect = rng.uniform(1.0f, 2.2f);
    spec.radiusX = r;
    spec.radiusY = r / aspect;
    spec.angle = rng.uniform(0.0f, 2.0f * units::kPi);
    if (rng.uniform() < 0.5f) {
        spec.shapeRate = 8.0f; // película rígida
        spec.r = 150; spec.g = 172; spec.b = 120;
    } else {
        spec.shapeRate = 0.3f; // córtex blando
        spec.r = 178; spec.g = 158; spec.b = 132;
    }
    return spec;
}
