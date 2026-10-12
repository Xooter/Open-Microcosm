#include "bio/metabolism.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kHour = 3600.0f;
constexpr float kO2PerGlucose = 6.0f; // C6H12O6 + 6 O2 → 6 CO2 + 6 H2O
} // namespace

MetabolicOutput updateMetabolism(Metabolism& m, const MetabolismParams& p, const MetabolicInput& in) {
    MetabolicOutput out;
    const float hours = in.bioDt / kHour;
    m.ageHours += hours;
    m.o2 = in.o2;
    m.light = in.light;
    m.reserves += in.digested;

    if (m.photosynthetic) {
        const float fixed = p.photosynthesisPerHour * std::clamp(in.light, 0.0f, 1.0f) * m.biomass * hours;
        m.reserves += fixed;
        out.o2Produced = kO2PerGlucose * fixed;
    }

    // Energía necesaria, expresada como glucosa respirada. Sin O₂ hace falta mucha más glucosa.
    const float energy = p.maintenancePerHour * m.biomass * (1.0f + p.motilityCost * in.activity) * hours;
    m.aerobic = std::max(0.0f, in.o2) / (std::max(0.0f, in.o2) + p.o2HalfSaturation);
    const float glucose = energy * (m.aerobic + (1.0f - m.aerobic) * p.fermentationPenalty);
    out.o2Consumed = kO2PerGlucose * energy * m.aerobic;

    if (m.reserves >= glucose) {
        m.reserves -= glucose;
        m.starving = false;
    } else {
        // Autofagia: lo que falta sale de la propia biomasa.
        m.biomass -= glucose - m.reserves;
        m.reserves = 0.0f;
        m.starving = true;
    }

    // Crecimiento con lo que sobra por encima del colchón.
    const float spare = m.reserves - p.reserveBuffer * m.biomass;
    if (spare > 0.0f) {
        const float invest = std::min(spare, p.growthPerHourMax * m.biomass * hours / p.growthYield);
        m.reserves -= invest;
        m.biomass += invest * p.growthYield;
    }
    return out;
}
