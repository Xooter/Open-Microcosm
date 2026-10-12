#include "sim/contacts.hpp"

#include "core/profiling.hpp"
#include "sim/grid2d.hpp"

#include <algorithm>
#include <cmath>

namespace {
// Vecinos de la misma membrana a esta distancia en el anillo no cuentan como contacto.
constexpr std::uint32_t kSelfSkip = 3;

std::uint32_t ringDistance(std::uint32_t a, std::uint32_t b, std::uint32_t size) {
    const std::uint32_t d = a > b ? a - b : b - a;
    return std::min(d, size - d);
}
} // namespace

void ContactSolver::prepare(const BodyParticles& bp, const std::vector<ContactMember>& members,
                            float width, float height, float maxRadius) {
    PROFILE_SCOPE("ContactSolver::prepare");
    const std::size_t n = members.size();
    wx_.resize(n);
    wy_.resize(n);
    dx_.resize(n);
    dy_.resize(n);
    for (std::size_t k = 0; k < n; ++k) {
        wx_[k] = wrapCoord(bp.x[members[k].slot], width);
        wy_[k] = wrapCoord(bp.y[members[k].slot], height);
    }
    // Celdas de 2 radios + margen: las aristas estiradas siguen dentro del vecindario 3×3.
    grid_.build(wx_, wy_, width, height, 2.0f * maxRadius + 1.0f);
}

void ContactSolver::iterate(BodyParticles& bp, const std::vector<ContactMember>& members,
                            float width, float height) {
    PROFILE_SCOPE("ContactSolver::iterate");
    const std::size_t n = members.size();
    const int gw = grid_.cellsX();
    const auto& order = grid_.order();
    // Las posiciones envueltas se refrescan: las restricciones de los cuerpos las movieron.
    for (std::size_t k = 0; k < n; ++k) {
        wx_[k] = wrapCoord(bp.x[members[k].slot], width);
        wy_[k] = wrapCoord(bp.y[members[k].slot], height);
    }

    {
#pragma omp parallel for schedule(static)
        for (std::ptrdiff_t k = 0; k < static_cast<std::ptrdiff_t>(n); ++k) {
            const ContactMember& me = members[k];
            const int cx = grid_.cellOf(k) % gw, cy = grid_.cellOf(k) / gw;
            const float rk = bp.radius[me.slot];
            float ax = 0.0f, ay = 0.0f;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    const std::uint32_t end = grid_.cellEnd(cx + ox, cy + oy);
                    for (std::uint32_t it = grid_.cellBegin(cx + ox, cy + oy); it < end; ++it) {
                        const std::uint32_t m = order[it];
                        const ContactMember& a = members[m];
                        const ContactMember& b = members[a.next];
                        if (a.owner == me.owner &&
                            std::min(ringDistance(me.ringPos, a.ringPos, me.ringSize),
                                     ringDistance(me.ringPos, b.ringPos, me.ringSize)) <= kSelfSkip)
                            continue;
                        // Arista a→b en la imagen más cercana a este punto.
                        const float ax0 = minImage(wx_[m] - wx_[k], width);
                        const float ay0 = minImage(wy_[m] - wy_[k], height);
                        const float ex = minImage(wx_[a.next] - wx_[m], width);
                        const float ey = minImage(wy_[a.next] - wy_[m], height);
                        const float len2 = ex * ex + ey * ey;
                        const float t =
                            len2 > 0.0f ? std::clamp(-(ax0 * ex + ay0 * ey) / len2, 0.0f, 1.0f) : 0.0f;
                        const float dx = -(ax0 + ex * t), dy = -(ay0 + ey * t); // punto − arista
                        const float minD = rk + bp.radius[a.slot];
                        const float d2 = dx * dx + dy * dy;
                        if (d2 >= minD * minD || d2 < 1e-12f)
                            continue;
                        const float d = std::sqrt(d2);
                        const float push = 0.5f * (minD - d) / d; // cada lado corrige la mitad
                        ax += dx * push;
                        ay += dy * push;
                    }
                }
            }
            dx_[k] = ax;
            dy_[k] = ay;
        }

        for (std::size_t k = 0; k < n; ++k) {
            bp.x[members[k].slot] += dx_[k];
            bp.y[members[k].slot] += dy_[k];
        }
    }
}
