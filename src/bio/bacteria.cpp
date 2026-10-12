#include "bio/bacteria.hpp"

#include "core/profiling.hpp"
#include "core/rng.hpp"
#include "core/units.hpp"
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
    for (std::size_t i = 0; i < count; ++i) {
        Rng rng(seed, rng_stream::kInit, i, 0);
        x[i] = rng.uniform(0.0f, width);
        y[i] = rng.uniform(0.0f, height);
        heading[i] = rng.uniform(0.0f, 2.0f * units::kPi);
    }
}

// E. coli responde al logaritmo de la concentración (ley de Weber): detecta gradientes
// relativos a lo largo de varios órdenes de magnitud.
float Bacteria::signal(const Grid2D& nutrients, float px, float py) const {
    const float c = std::max(0.0f, nutrients.sample(px, py));
    return std::log1p(c / params.sensingKd);
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
        Rng rng(seed, rng_stream::kBacteria, static_cast<std::uint64_t>(i), stepIndex);
        float vx = 0.0f, vy = 0.0f;

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
                vx = p.speed * std::cos(heading[i]);
                vy = p.speed * std::sin(heading[i]);
            }
        }

        heading[i] += rotStd * rng.normal();
        const Vec2 u = fluid.velocityAt({x[i], y[i]});
        x[i] = wrapCoord(x[i] + (vx + u.x) * dt + transStd * rng.normal(), width);
        y[i] = wrapCoord(y[i] + (vy + u.y) * dt + transStd * rng.normal(), height);
    }
}
