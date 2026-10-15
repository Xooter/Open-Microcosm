#include "bio/phagocytosis.hpp"

#include "core/profiling.hpp"

#include "core/units.hpp"
#include "sim/polygon.hpp"
#include "sim/soft_body.hpp"

#include <algorithm>
#include <cmath>

namespace {

Vec2 pos(const BodyParticles& bp, std::uint32_t s) { return {bp.x[s], bp.y[s]}; }

float contactDistance(const RingBody& predator, const RingBody& prey, const BodyParticles& bp) {
    return bp.radius[predator.ring[0]] + bp.radius[prey.ring[0]];
}

// Geometría de la copa vista desde la presa (en la imagen más cercana al depredador).
// Los labios avanzan por LONGITUD DE ARCO sobre el contorno real de la presa, desde el punto
// de contacto: un ángulo alrededor de un radio medio solo sirve para presas redondas (en una
// alargada, un contacto en la punta mandaba los objetivos 30 µm hacia el costado). El "ángulo"
// que se expone es el arco normalizado a [-π, π] (π = media vuelta al contorno), así las
// tasas y topes de la copa no cambian.
struct CupFrame {
    const RingBody& prey;
    Vec2 shift;
    float contact;
    int pn;
    std::vector<float> cum; // longitud acumulada del contorno hasta cada vértice
    float perimeter = 0.0f, anchorArc = 0.0f;
    float arcPerRad = 1.0f; // [µm/rad]

    float arcOf(const polygon::Closest& cl) const {
        const Vec2 a = prey.poly[static_cast<std::size_t>(cl.edge)];
        const Vec2 b = prey.poly[static_cast<std::size_t>((cl.edge + 1) % pn)];
        return cum[static_cast<std::size_t>(cl.edge)] + cl.t * length(b - a);
    }
    float angleOf(Vec2 p) const {
        const polygon::Closest cl = polygon::closestOnBoundary(prey.poly.data(), pn, p - shift);
        float rel = arcOf(cl) - anchorArc;
        const float half = 0.5f * perimeter;
        if (rel > half)
            rel -= perimeter;
        else if (rel < -half)
            rel += perimeter;
        return rel / arcPerRad;
    }
    // Lugar en la copa para un ángulo: sobre la superficie real de la presa + contacto.
    Vec2 shellPoint(float phi) const {
        float arc = std::fmod(anchorArc + phi * arcPerRad, perimeter);
        if (arc < 0.0f)
            arc += perimeter;
        const auto it = std::upper_bound(cum.begin(), cum.begin() + pn, arc);
        const int e = std::clamp(static_cast<int>(it - cum.begin()) - 1, 0, pn - 1);
        const Vec2 a = prey.poly[static_cast<std::size_t>(e)];
        const Vec2 ab = prey.poly[static_cast<std::size_t>((e + 1) % pn)] - a;
        const float len = length(ab);
        if (len < 1e-6f)
            return a + shift;
        const float t = std::clamp((arc - cum[static_cast<std::size_t>(e)]) / len, 0.0f, 1.0f);
        const Vec2 out = polygon::inwardNormal(ab * (1.0f / len), prey.restArea) * -1.0f;
        return a + ab * t + shift + out * contact;
    }
    bool attached(Vec2 p) const { return within(p, contact + 1.5f); }
    bool within(Vec2 p, float reach) const {
        return polygon::closestOnBoundary(prey.poly.data(), pn, p - shift).dist2 < reach * reach;
    }
};

CupFrame makeFrame(const RingBody& predator, const BodyParticles& bp, const RingBody& prey,
                   Vec2 shift, std::uint32_t anchorSlot) {
    CupFrame f{prey, shift, contactDistance(predator, prey, bp), static_cast<int>(prey.poly.size()), {}};
    f.cum.assign(static_cast<std::size_t>(f.pn) + 1, 0.0f);
    for (int i = 0; i < f.pn; ++i)
        f.cum[static_cast<std::size_t>(i) + 1] =
            f.cum[static_cast<std::size_t>(i)] +
            length(prey.poly[static_cast<std::size_t>((i + 1) % f.pn)] - prey.poly[static_cast<std::size_t>(i)]);
    f.perimeter = std::max(1e-3f, f.cum[static_cast<std::size_t>(f.pn)]);
    f.arcPerRad = 0.5f * f.perimeter / units::kPi;
    f.anchorArc = f.arcOf(polygon::closestOnBoundary(prey.poly.data(), f.pn, pos(bp, anchorSlot) - shift));
    return f;
}

std::size_t ringStep(std::size_t i, int step, std::size_t n) {
    return (i + n + static_cast<std::size_t>(step + static_cast<int>(n))) % n;
}

// Puntos adheridos a la presa desde el ancla hacia un lado (en orden de anillo) y su avance.
// El forro incluye la membrana cercana todavía no adherida (`lipReach`): en el tramo que rodea
// una punta de la presa, la membrana sin guiar se amontonaba y se plegaba sobre sí misma.
struct Lining {
    std::vector<std::size_t> idx;
    float progress = 0.0f; // [rad, ≥ 0 hacia `sense`] hasta el último punto del forro
};

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

// El avance se acumula punto a punto desde el ancla: `angleOf` envuelve a ±π y, si el ancla se
// reubica cerca de un labio, el otro forro pasa el antípoda; con el ángulo envuelto su último
// punto parecía estar *detrás* (la membrana nueva caía 8 µm atrás del labio y se doblaba).
// El forro termina donde la membrana da la vuelta (el ángulo retrocede): en un pseudópodo fino
// apretado contra la presa, la cara de enfrente también queda a `lipReach` y, tomada como forro,
// se llevaba hacia la presa a través de la otra cara.
Lining liningOf(const RingBody& predator, const BodyParticles& bp, const CupFrame& f, std::size_t anchor,
                int dirStep, float lipReach, float sense) {
    Lining out;
    const std::size_t n = predator.ring.size();
    const float backTolerance = 1.0f / f.arcPerRad; // 1 µm de arco: el forro no es perfectamente parejo
    float prev = 0.0f, acc = 0.0f, best = 0.0f;     // el ancla está en ángulo 0 por definición del marco
    for (std::size_t k = 1; k < n / 2; ++k) {
        const std::size_t idx = ringStep(anchor, dirStep * static_cast<int>(k), n);
        const Vec2 p = pos(bp, predator.ring[idx]);
        if (!f.within(p, f.contact + lipReach))
            break;
        const float a = f.angleOf(p);
        float d = a - prev;
        if (d > units::kPi)
            d -= 2.0f * units::kPi;
        else if (d < -units::kPi)
            d += 2.0f * units::kPi;
        acc += sense * d;
        if (acc < best - backTolerance)
            break;
        best = std::max(best, acc);
        prev = a;
        out.idx.push_back(idx);
        out.progress = std::max(0.0f, acc);
    }
    return out;
}

// ¿Hay otra célula en `p` (o pegada a él)? Ahí la membrana no puede ir.
bool blockedAt(const std::vector<CupObstacle>& obstacles, Vec2 p, float margin) {
    for (const CupObstacle& o : obstacles) {
        const int n = static_cast<int>(o.body->poly.size());
        const Vec2 q = p - o.shift;
        if (polygon::contains(o.body->poly.data(), n, q) ||
            polygon::closestOnBoundary(o.body->poly.data(), n, q).dist2 < margin * margin)
            return true;
    }
    return false;
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
              PhagocyticCup& cup, const FeedingParams& params, float dt,
              const std::vector<CupObstacle>& obstacles) {
    const auto it = std::find(predator.ring.begin(), predator.ring.end(), cup.anchorSlot);
    if (it == predator.ring.end())
        return;
    const CupFrame f = makeFrame(predator, bp, prey, preyShift, cup.anchorSlot);
    const float shellRadius = f.arcPerRad; // [µm/rad] sobre el contorno de la presa
    const float spacing = bp.radius[predator.ring[0]] / 0.6f; // espaciado de membrana

    // Avance de cada labio antes de agregar membrana: cada uno puede llegar hasta el otro (los
    // labios se encuentran donde sea, no a media vuelta: el ancla puede haberse reubicado).
    float progress[2];
    {
        const std::size_t anchor = static_cast<std::size_t>(it - predator.ring.begin());
        for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
            const int dirStep = sideIndex == 0 ? 1 : -1;
            progress[sideIndex] =
                liningOf(predator, bp, f, anchor, dirStep, params.lipReach, senseOf(predator, bp, f, anchor, dirStep, cup))
                    .progress;
        }
    }

    std::vector<std::uint32_t> ring = predator.ring;
    bool changed = false;
    for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
        const int dirStep = sideIndex == 0 ? 1 : -1;
        const std::size_t anchor = static_cast<std::size_t>(
            std::find(ring.begin(), ring.end(), cup.anchorSlot) - ring.begin());
        RingBody view = predator; // anillo actualizado para medir este lado
        view.ring = ring;
        const float sense = senseOf(view, bp, f, anchor, dirStep, cup);
        const Lining lining = liningOf(view, bp, f, anchor, dirStep, params.lipReach, sense);
        float& front = cup.front[sideIndex];
        const std::size_t last = lining.idx.empty() ? anchor : lining.idx.back();
        const float lastAngle = lining.progress;
        // El frente nunca queda detrás del forro ya adherido (presa alargada: el contacto
        // inicial puede cubrir un tramo largo), si no `applyCup` amontona los puntos.
        const float held = std::max(front, lastAngle);
        const float meet = std::max(units::kPi, 2.0f * units::kPi - progress[1 - sideIndex]);
        front = std::min(meet, held + params.zipperSpeed * dt / shellRadius);
        // Si hay otra célula pegada a la presa más adelante, el labio se detiene: no hay lugar
        // para la membrana entre las dos (si se empuja, queda aplastada y se cruza).
        const float ahead = lastAngle + 2.0f * spacing / shellRadius;
        if (!obstacles.empty() && front > lastAngle && blockedAt(obstacles, f.shellPoint(sense * std::min(front, ahead)), f.contact)) {
            front = held;
            continue;
        }

        // Si el frente se alejó más de un espaciado del último punto del forro, se agrega
        // membrana nueva en el labio, sobre la superficie de la presa.
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
              PhagocyticCup& cup, const FeedingParams& params, float dt,
              const std::vector<CupObstacle>& obstacles) {
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
        const float sense = senseOf(predator, bp, f, anchor, dirStep, cup);
        const Lining lining = liningOf(predator, bp, f, anchor, dirStep, params.lipReach, sense);
        const float reached = lining.progress;
        const float front = std::max(cup.front[sideIndex], reached);
        for (std::size_t k = 0; k < lining.idx.size(); ++k) {
            const float phi = sense * front * static_cast<float>(k + 1) / static_cast<float>(lining.idx.size() + 1);
            const Vec2 goal = f.shellPoint(phi);
            if (!obstacles.empty() && blockedAt(obstacles, goal, f.contact))
                continue;
            steer(predator.ring[lining.idx[k]], goal, 2.0f * params.zipperSpeed);
        }
        coverage = std::max(coverage, reached);
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
    PROFILE_SCOPE("findClosure");
    const int n = static_cast<int>(predator.ring.size());
    const float touch = 2.5f * bp.radius[predator.ring[0]];
    const float bodyArea = std::abs(soft_body::area(predator, bp));
    // Número de giro: ángulo acumulado del anillo alrededor de la presa. Un tramo i→j cerrado
    // por la cuerda j→i rodea a la presa si su giro total es ±2π (|giro| > π). Así cada par
    // cuesta O(1) y el polígono de la copa se arma solo para los candidatos.
    auto wrapAngle = [](float a) {
        while (a > units::kPi)
            a -= 2.0f * units::kPi;
        while (a < -units::kPi)
            a += 2.0f * units::kPi;
        return a;
    };
    std::vector<float> angle(static_cast<std::size_t>(n)), cumulative(static_cast<std::size_t>(n) + 1, 0.0f);
    for (int k = 0; k < n; ++k) {
        const Vec2 d = pos(bp, predator.ring[k]) - preyCenter;
        angle[k] = std::atan2(d.y, d.x);
    }
    for (int k = 0; k < n; ++k)
        cumulative[k + 1] = cumulative[k] + wrapAngle(angle[(k + 1) % n] - angle[k]);

    std::vector<Vec2> pocket, outer;
    for (int i = 0; i < n; ++i) {
        const Vec2 xi = pos(bp, predator.ring[i]);
        for (int j = i + params.minPocket; j < n; ++j) {
            if (n - (j - i) < params.minPocket)
                break;
            const Vec2 d = pos(bp, predator.ring[j]) - xi;
            if (dot(d, d) > touch * touch)
                continue;
            const float forward = cumulative[j] - cumulative[i] + wrapAngle(angle[i] - angle[j]);
            const float backward = (cumulative[n] - cumulative[j]) + cumulative[i] + wrapAngle(angle[j] - angle[i]);
            for (int side = 0; side < 2; ++side) {
                if (std::abs(side == 0 ? forward : backward) < units::kPi)
                    continue; // ese tramo no rodea a la presa
                pocket.clear();
                const int from = side == 0 ? i : j, to = side == 0 ? j : i + n;
                for (int k = from; k <= to; ++k)
                    pocket.push_back(pos(bp, predator.ring[k % n]));
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
                if (polygon::selfIntersects(outer.data(), static_cast<int>(outer.size())))
                    continue; // labios solapados: se espera a que la copa termine de acomodarse
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
