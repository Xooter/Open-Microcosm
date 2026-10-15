#include "bio/bacteria.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
#include "bio/feeding.hpp"
#include "sim/fluid.hpp"

#include <algorithm>
#include <cmath>

void Bacteria::spawn(std::size_t count, float width, float height, std::uint64_t seed) {
    x.resize(count);
    y.resize(count);
    heading.resize(count);
    tumbleTimer.assign(count, 0.0f);
    memory.assign(count, 0.0f);
    tumbling.assign(count, 0);
    mass.resize(count);
    strain.assign(count, 0);
    resistant.assign(count, 0);
    quorum.assign(count, 0);
    phage.assign(count, -1.0f);
    bdello.assign(count, -1.0f);
    attached.assign(count, 0);
    id.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        id[i] = nextId++;
    for (std::size_t i = 0; i < count; ++i) {
        Rng rng(seed, rng_stream::kInit, i, 0);
        x[i] = rng.uniform(0.0f, width);
        y[i] = rng.uniform(0.0f, height);
        if (dish.active()) {
            const Vec2 p = dish.sample(x[i] / width, y[i] / height, params.length);
            x[i] = p.x;
            y[i] = p.y;
        }
        heading[i] = rng.uniform(0.0f, 2.0f * units::kPi);
        mass[i] = rng.uniform(1.0f, 2.0f); // población en distintas etapas del ciclo
        strain[i] = rng.uniform() < params.violaceumFraction ? 1 : 0;
        if (strain[i] == 0) { // otro flujo de azar: no cambia el resto de la gota de una seed
            Rng gram(seed, rng_stream::kInit + 900, i, 0);
            strain[i] = gram.uniform() < params.bacillusFraction ? 2 : 0;
        }
    }
}

void Bacteria::add(float px, float py, float h, float m, std::uint8_t st, std::uint8_t res, std::uint8_t att) {
    attached.push_back(att);
    strain.push_back(st);
    resistant.push_back(res);
    quorum.push_back(0);
    phage.push_back(-1.0f);
    bdello.push_back(-1.0f);
    id.push_back(nextId++);
    x.push_back(px);
    y.push_back(py);
    heading.push_back(h);
    tumbleTimer.push_back(0.0f);
    memory.push_back(0.0f);
    tumbling.push_back(0);
    mass.push_back(m);
}

void Bacteria::grow(const Grid2D& nutrients, float bioDt, std::vector<float>& demand) const {
    const float hours = bioDt / 3600.0f;
    const auto n = static_cast<std::ptrdiff_t>(size());
    demand.resize(size());
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        const float c = std::max(0.0f, nutrients.sample(x[i], y[i]));
        // Infectadas (fago o Bdellovibrio) no crecen: su maquinaria trabaja para el parásito.
        const float healthy = phage[i] < 0.0f && bdello[i] < 0.0f ? (resistant[i] ? 1.0f - params.resistanceCost : 1.0f) : 0.0f;
        const float growth = healthy * params.growthPerHourMax * c / (c + params.halfSaturation) * mass[i] * hours;
        demand[i] = growth / params.yield * kBacteriumPmol;
    }
}

// E. coli responde al logaritmo de la concentración (ley de Weber): detecta gradientes
// relativos a lo largo de varios órdenes de magnitud.
float Bacteria::signal(const Grid2D& nutrients, float px, float py) const {
    const float c = std::max(0.0f, nutrients.sample(px, py));
    return std::log1p(c / params.sensingKd);
}

void Bacteria::remove(std::size_t i) {
    const std::size_t last = size() - 1;
    x[i] = x[last];
    y[i] = y[last];
    heading[i] = heading[last];
    tumbleTimer[i] = tumbleTimer[last];
    memory[i] = memory[last];
    tumbling[i] = tumbling[last];
    mass[i] = mass[last];
    strain[i] = strain[last];
    resistant[i] = resistant[last];
    quorum[i] = quorum[last];
    phage[i] = phage[last];
    bdello[i] = bdello[last];
    attached[i] = attached[last];
    id[i] = id[last];
    id.pop_back();
    mass.pop_back();
    strain.pop_back();
    resistant.pop_back();
    quorum.pop_back();
    phage.pop_back();
    bdello.pop_back();
    attached.pop_back();
    x.pop_back();
    y.pop_back();
    heading.pop_back();
    tumbleTimer.pop_back();
    memory.pop_back();
    tumbling.pop_back();
}

void Bacteria::resetMemory(const Grid2D& nutrients) {
    for (std::size_t i = 0; i < size(); ++i)
        memory[i] = signal(nutrients, x[i], y[i]);
}

void Bacteria::step(const Fluid& fluid, const Grid2D& nutrients, float dt, std::uint64_t seed,
                    std::uint64_t stepIndex, float width, float height) {
    PROFILE_SCOPE("Bacteria::step");
    const BacteriaParams p = params;
    const float transStd = std::sqrt(2.0f * units::stokesEinsteinDiffusion(p.radius) * dt);
    const float rotStd = std::sqrt(2.0f * p.rotDiffusion * dt);
    const float memoryBlend = std::min(1.0f, dt / p.memoryTime);
    const float degToRad = units::kPi / 180.0f;
    const auto n = static_cast<std::ptrdiff_t>(size());

    // Cada iteración escribe solo su propio índice: sin data races y determinista.
#pragma omp parallel for schedule(static)
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        if (attached[i])
            continue; // en la biopelícula: ni nada ni la arrastra el agua
        Rng rng(seed, rng_stream::kBacteria, static_cast<std::uint64_t>(i), stepIndex);
        float vx = 0.0f, vy = 0.0f;
        // Bdeloplasto: la presa se redondea y deja de nadar. En agregado, nada menos.
        const float speed = bdello[i] >= 0.0f ? 0.0f : (quorum[i] ? p.speed * p.quorumSpeed : p.speed);

        if (tumbling[i]) {
            tumbleTimer[i] -= dt;
            if (tumbleTimer[i] <= 0.0f) {
                tumbling[i] = 0;
                const float angle = (p.tumbleAngleMean + p.tumbleAngleStd * rng.normal()) * degToRad;
                heading[i] += rng.uniform() < 0.5f ? angle : -angle;
            }
        } else {
            const float s = signal(nutrients, x[i], y[i]);
            const float change = s - memory[i];
            memory[i] += change * memoryBlend;
            // Si la concentración sube, la bacteria suprime los giros y alarga la corrida.
            const float rate =
                p.baseTumbleRate * std::clamp(1.0f - p.chemotaxisGain * change, 0.1f, 2.0f);
            if (rng.uniform() < rate * dt) {
                tumbling[i] = 1;
                tumbleTimer[i] = p.tumbleDuration;
            } else {
                vx = speed * std::cos(heading[i]);
                vy = speed * std::sin(heading[i]);
            }
        }

        heading[i] += rotStd * rng.normal();
        const Vec2 u = fluid.velocityAt({x[i], y[i]});
        x[i] = wrapCoord(x[i] + (vx + u.x) * dt + transStd * rng.normal(), width);
        y[i] = wrapCoord(y[i] + (vy + u.y) * dt + transStd * rng.normal(), height);
        // Contra la pared de la placa el rumbo se refleja (las bacterias reales terminan
        // nadando a lo largo de las superficies).
        if (dish.bounce(x[i], y[i], &heading[i], p.radius) && rng.uniform() < p.attachProbability)
            attached[i] = 1;
    }
}
