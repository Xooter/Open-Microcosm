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
        const float fixed = p.photosynthesisPerHour * in.photosynthesisScale * std::clamp(in.light, 0.0f, 1.0f) *
                            m.biomass * hours;
        // Parte de lo fijado se exuda como materia orgánica disuelta (alimento de bacterias).
        out.exudate = p.exudation * fixed;
        m.reserves += fixed - out.exudate;
        out.o2Produced = kO2PerGlucose * fixed;
    }

    // Energía necesaria, expresada como glucosa respirada. Sin O₂ hace falta mucha más glucosa.
    const float energy = p.maintenancePerHour * in.maintenanceScale * m.biomass *
                         (1.0f + p.motilityCost * in.motilityCostScale * in.activity) * hours;
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

    // Fotótrofas: captan fosfato del agua hasta 1,5 veces su cuota (consumo de lujo).
    if (m.photosynthetic) {
        const float quota = p.phosphorusRatio * m.biomass;
        if (m.phosphorus < 1.5f * quota) {
            const float c = std::max(0.0f, in.phosphate);
            const float take = std::min(1.5f * quota - m.phosphorus,
                                        p.phosphorusUptakePerHour * quota * c / (c + p.phosphorusHalfSaturation) * hours);
            m.phosphorus += take;
            out.phosphateTaken = take;
        }
    }

    // Crecimiento con lo que sobra por encima del colchón.
    const float spare = m.reserves - p.reserveBuffer * m.biomass;
    if (spare > 0.0f) {
        float invest = std::min(spare, p.growthPerHourMax * in.growthScale * m.biomass * hours / p.growthYield);
        // Sin fósforo no hay biomasa nueva (ácidos nucleicos, fosfolípidos).
        if (m.photosynthetic)
            invest = std::min(invest, std::max(0.0f, m.phosphorus / p.phosphorusRatio - m.biomass) / p.growthYield);
        m.reserves -= invest;
        m.biomass += invest * p.growthYield;
    }
    return out;
}
