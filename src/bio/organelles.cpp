#include "bio/organelles.hpp"

#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Densidades a valor de gen 1 [por 1000 µm² de sección]. Cada partícula representa un grupo:
// una Paramecium real tiene miles de mitocondrias.
constexpr float kMitoDensity = 3.0f;
constexpr float kChloroDensity = 8.0f;
constexpr float kLysoDensity = 1.5f;
constexpr int kMaxPerType = 120;
} // namespace

int countOrganelles(const RingBody& body, const BodyParticles& bp, OrganelleTag tag) {
    int n = 0;
    for (std::uint32_t s : body.interior)
        n += bp.tag[s] == static_cast<std::uint8_t>(tag) ? 1 : 0;
    return n;
}

void refillOrganelles(RingBody& body, BodyParticles& bp, const Genome& genome, Rng& rng) {
    const float area = std::abs(soft_body::area(body, bp)) / 1000.0f;
    struct Want {
        OrganelleTag tag;
        float density, radius;
    };
    const Want wants[] = {
        {OrganelleTag::Mitochondrion, kMitoDensity * genome[Gene::Mitochondria], 0.8f},
        {OrganelleTag::Chloroplast, kChloroDensity * genome[Gene::Chloroplasts], 1.6f},
        {OrganelleTag::Lysosome, kLysoDensity * genome[Gene::Lysosomes], 0.6f},
    };
    const int pn = static_cast<int>(body.poly.size());
    for (const Want& w : wants) {
        const int target = std::min(kMaxPerType, static_cast<int>(std::round(w.density * area)));
        for (int have = countOrganelles(body, bp, w.tag); have < target; ++have) {
            Vec2 p = body.centroid;
            for (int attempt = 0; attempt < 20; ++attempt) {
                const Vec2 q = {rng.uniform(body.aabbMin.x, body.aabbMax.x), rng.uniform(body.aabbMin.y, body.aabbMax.y)};
                if (polygon::contains(body.poly.data(), pn, q)) {
                    p = q;
                    break;
                }
            }
            const std::uint32_t s = bp.allocate(BodyKind::Interior, p.x, p.y, w.radius, 1.0f, body.id);
            bp.tag[s] = static_cast<std::uint8_t>(w.tag);
            body.interior.push_back(s);
            body.restInterior.push_back(soft_body::toLocal(body, p));
            body.anchorRate.push_back(0.0f);
        }
    }
}
