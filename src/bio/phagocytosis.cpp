#include "bio/phagocytosis.hpp"

#include "core/units.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {

Vec2 pos(const BodyParticles& bp, std::uint32_t s) { return {bp.x[s], bp.y[s]}; }

Vec2 normalized(Vec2 v) {
    const float len = length(v);
    return len > 1e-6f ? v * (1.0f / len) : Vec2{1.0f, 0.0f};
}

float contactDistance(const RingBody& predator, const RingBody& prey, const BodyParticles& bp) {
    return bp.radius[predator.ring[0]] + bp.radius[prey.ring[0]];
}

// Geometría de la copa vista desde la presa (en la imagen más cercana al depredador).
struct CupFrame {
    const RingBody& prey;
    Vec2 shift, preyC, nearPole;
    float contact, meanRadius;
    int pn;

    float angleOf(Vec2 p) const {
        const Vec2 rel = p - preyC;
        return std::atan2(cross(nearPole, rel), dot(nearPole, rel));
    }
    // Lugar en la copa para un ángulo: sobre la superficie real de la presa + contacto.
    Vec2 shellPoint(float phi) const {
        const Vec2 dir = rotate(nearPole, std::cos(phi), std::sin(phi));
        const polygon::Closest cl =
            polygon::closestOnBoundary(prey.poly.data(), pn, preyC - shift + dir * meanRadius);
        const Vec2 out = polygon::inwardNormal(cl.edgeDir, prey.restArea) * -1.0f;
        return cl.point + shift + out * contact;
    }
    bool attached(Vec2 p) const {
        return polygon::closestOnBoundary(prey.poly.data(), pn, p - shift).dist2 <
               (contact + 1.5f) * (contact + 1.5f);
    }
};

CupFrame makeFrame(const RingBody& predator, const BodyParticles& bp, const RingBody& prey,
                   Vec2 shift, std::uint32_t anchorSlot) {
    const Vec2 preyC = prey.centroid + shift;
    return {prey,
            shift,
            preyC,
            normalized(pos(bp, anchorSlot) - preyC),
            contactDistance(predator, prey, bp),
            std::sqrt(std::abs(prey.restArea) / units::kPi),
            static_cast<int>(prey.poly.size())};
}

std::size_t ringStep(std::size_t i, int step, std::size_t n) {
    return (i + n + static_cast<std::size_t>(step + static_cast<int>(n))) % n;
}

// Puntos adheridos a la presa desde el ancla hacia un lado (en orden de anillo).
std::vector<std::size_t> liningOf(const RingBody& predator, const BodyParticles& bp,
                                  const CupFrame& f, std::size_t anchor, int dirStep) {
    std::vector<std::size_t> lining;
    const std::size_t n = predator.ring.size();
    for (std::size_t k = 1; k < n / 2; ++k) {
        const std::size_t idx = ringStep(anchor, dirStep * static_cast<int>(k), n);
        if (!f.attached(pos(bp, predator.ring[idx])))
            break;
        lining.push_back(idx);
    }
    return lining;
}

// Sentido angular de cada lado, fijado una vez al iniciar la copa: compara el ángulo de los
// vecinos del ancla hacia adelante y hacia atrás en el anillo. Los dos lados quedan opuestos.
float senseOf(const RingBody& predator, const BodyParticles& bp, const CupFrame& f,
              std::size_t anchor, int dirStep, PhagocyticCup& cup) {
    if (cup.sense == 0.0f) {
        const std::size_t n = predator.ring.size();
        const float ahead = f.angleOf(pos(bp, predator.ring[ringStep(anchor, 3, n)]));
        const float behind = f.angleOf(pos(bp, predator.ring[ringStep(anchor, -3, n)]));
        cup.sense = ahead >= behind ? 1.0f : -1.0f;
    }
    return dirStep > 0 ? cup.sense : -cup.sense;
}

} // namespace

void holdPrey(const RingBody& prey, BodyParticles& bp, PhagocyticCup& cup) {
    cup.preyInvMass = bp.invMass[prey.ring[0]];
    for (std::uint32_t s : prey.ring)
        bp.invMass[s] = 0.05f * cup.preyInvMass;
}

void releasePrey(const RingBody& prey, BodyParticles& bp, const PhagocyticCup& cup) {
    for (std::uint32_t s : prey.ring)
        bp.invMass[s] = cup.preyInvMass;
}

std::uint32_t findContact(const RingBody& predator, const BodyParticles& bp, const RingBody& prey,
                          Vec2 preyShift, const FeedingParams& params) {
    const float reach = contactDistance(predator, prey, bp) + params.contactGap;
    const int n = static_cast<int>(prey.poly.size());
    float best = reach * reach;
    std::uint32_t found = ~0u;
    for (std::uint32_t s : predator.ring) {
        const polygon::Closest cl = polygon::closestOnBoundary(prey.poly.data(), n, pos(bp, s) - preyShift);
        if (cl.dist2 < best) {
            best = cl.dist2;
            found = s;
        }
    }
    return found;
}

void growLips(RingBody& predator, BodyParticles& bp, const RingBody& prey, Vec2 preyShift,
              PhagocyticCup& cup, const FeedingParams& params, float dt) {
    const auto it = std::find(predator.ring.begin(), predator.ring.end(), cup.anchorSlot);
    if (it == predator.ring.end())
        return;
    const CupFrame f = makeFrame(predator, bp, prey, preyShift, cup.anchorSlot);
    const float shellRadius = f.meanRadius + f.contact;
    const float spacing = bp.radius[predator.ring[0]] / 0.6f; // espaciado de membrana

    std::vector<std::uint32_t> ring = predator.ring;
    bool changed = false;
    for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
        const int dirStep = sideIndex == 0 ? 1 : -1;
        const std::size_t anchor = static_cast<std::size_t>(
            std::find(ring.begin(), ring.end(), cup.anchorSlot) - ring.begin());
        RingBody view = predator; // anillo actualizado para medir este lado
        view.ring = ring;
        const std::vector<std::size_t> lining = liningOf(view, bp, f, anchor, dirStep);
        const float sense = senseOf(view, bp, f, anchor, dirStep, cup);
        float& front = cup.front[sideIndex];
        front = std::min(units::kPi, front + params.zipperSpeed * dt / shellRadius);

        // Si el frente se alejó más de un espaciado del último punto del forro, se agrega
        // membrana nueva en el labio, sobre la superficie de la presa.
        const std::size_t last = lining.empty() ? anchor : lining.back();
        const float lastAngle = std::abs(f.angleOf(pos(bp, ring[last])));
        // Una vesícula por vez: la membrana anterior tiene que haberse adherido.
        std::uint32_t& pending = cup.pending[sideIndex];
        if (pending != ~0u && std::find(ring.begin(), ring.end(), pending) != ring.end() &&
            !f.attached(pos(bp, pending)))
            continue;
        pending = ~0u;
        if ((front - lastAngle) * shellRadius > spacing) {
            const Vec2 p = f.shellPoint(sense * (lastAngle + spacing / shellRadius));
            const std::uint32_t slot = bp.allocate(BodyKind::Membrane, p.x, p.y,
                                                   bp.radius[ring[last]], bp.invMass[ring[last]], predator.id);
            // Se inserta entre `last` y el siguiente en la dirección de este lado.
            const std::size_t at = dirStep > 0 ? last + 1 : last;
            ring.insert(ring.begin() + static_cast<std::ptrdiff_t>(at), slot);
            pending = slot;
            changed = true;
        }
    }
    if (changed)
        soft_body::resizeRing(predator, bp, std::move(ring));
}

void applyCup(const RingBody& predator, BodyParticles& bp, const RingBody& prey, Vec2 preyShift,
              PhagocyticCup& cup, const FeedingParams& params, float dt) {
    const auto it = std::find(predator.ring.begin(), predator.ring.end(), cup.anchorSlot);
    if (it == predator.ring.end())
        return; // el remallado quitó el ancla: World elige otra
    const auto anchor = static_cast<std::size_t>(it - predator.ring.begin());

    const CupFrame f = makeFrame(predator, bp, prey, preyShift, cup.anchorSlot);
    const float contact = f.contact;
    auto steer = [&](std::uint32_t s, Vec2 target, float maxSpeed) {
        const Vec2 to = target - pos(bp, s);
        const float dist = length(to);
        if (dist < 1e-4f)
            return;
        const float speed = std::min(maxSpeed, 1.5f * dist);
        bp.vx[s] += to.x * (speed / dist);
        bp.vy[s] += to.y * (speed / dist);
    };

    // El forro adherido (unión ligando–receptor) se reparte en orden y parejo entre el
    // contacto y el frente de su labio; el punto siguiente se cierra sobre la presa.
    float coverage = 0.0f;
    for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
        const int dirStep = sideIndex == 0 ? 1 : -1;
        const std::vector<std::size_t> lining = liningOf(predator, bp, f, anchor, dirStep);
        const float sense = senseOf(predator, bp, f, anchor, dirStep, cup);
        const float front = cup.front[sideIndex];
        for (std::size_t k = 0; k < lining.size(); ++k) {
            const float phi = sense * front * static_cast<float>(k + 1) / static_cast<float>(lining.size() + 1);
            steer(predator.ring[lining[k]], f.shellPoint(phi), 2.0f * params.zipperSpeed);
        }
        if (!lining.empty())
            coverage = std::max(coverage, std::abs(f.angleOf(pos(bp, predator.ring[lining.back()]))));
    }
    cup.coverage = coverage;
    (void)dt;

    // La adhesión es un enlace entre dos superficies: la presa queda sujeta al forro.
    const float reach = contact + params.contactGap;
    const int predN = static_cast<int>(predator.poly.size());
    for (std::uint32_t s : prey.ring) {
        const Vec2 x = pos(bp, s) + preyShift;
        const polygon::Closest cl = polygon::closestOnBoundary(predator.poly.data(), predN, x);
        if (cl.dist2 > reach * reach || cl.dist2 < 1e-8f)
            continue;
        // Enlace tipo resorte: tira hacia la distancia de contacto, con tope adhesionSpeed.
        const float d = std::sqrt(cl.dist2);
        const float pull = std::min(params.adhesionSpeed, 2.0f * std::max(0.0f, d - contact));
        bp.vx[s] += (cl.point.x - x.x) * (pull / d);
        bp.vy[s] += (cl.point.y - x.y) * (pull / d);
    }
}

bool findClosure(const RingBody& predator, const BodyParticles& bp, Vec2 preyCenter,
                 const FeedingParams& params, Closure& out) {
    const int n = static_cast<int>(predator.ring.size());
    const float touch = 2.5f * bp.radius[predator.ring[0]];
    const float bodyArea = std::abs(soft_body::area(predator, bp));
    std::vector<Vec2> pocket, outer;
    for (int i = 0; i < n; ++i) {
        const Vec2 xi = pos(bp, predator.ring[i]);
        for (int j = i + params.minPocket; j < n; ++j) {
            if (n - (j - i) < params.minPocket)
                break;
            const Vec2 d = pos(bp, predator.ring[j]) - xi;
            if (dot(d, d) > touch * touch)
                continue;
            // Dos candidatos de copa: el tramo i→j o el tramo j→i (dando la vuelta).
            for (int side = 0; side < 2; ++side) {
                pocket.clear();
                const int from = side == 0 ? i : j, to = side == 0 ? j : i + n;
                for (int k = from; k <= to; ++k)
                    pocket.push_back(pos(bp, predator.ring[k % n]));
                if (!polygon::contains(pocket.data(), static_cast<int>(pocket.size()), preyCenter))
                    continue;
                // El anillo que queda tiene que ser el cuerpo: misma orientación y la mayor
                // parte del área (un par de puntos en el pliegue de un labio no sirve).
                outer.clear();
                const int ofrom = side == 0 ? j : i, oto = side == 0 ? i + n : j;
                for (int k = ofrom; k <= oto; ++k)
                    outer.push_back(pos(bp, predator.ring[k % n]));
                const float outerArea = polygon::signedArea(outer.data(), static_cast<int>(outer.size()));
                const float pocketArea = polygon::signedArea(pocket.data(), static_cast<int>(pocket.size()));
                if (outerArea * predator.restArea <= 0.0f || std::abs(outerArea) < 0.5f * bodyArea ||
                    std::abs(pocketArea) >= std::abs(outerArea))
                    continue;
                out.pocket = pocket;
                out.preyCenter = preyCenter;
                out.outerRing.clear();
                for (int k = ofrom; k <= oto; ++k)
                    out.outerRing.push_back(predator.ring[k % n]);
                return true;
            }
        }
    }
    return false;
}
