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
                         (1.0f + p.motilityCost * in.motilityCostScale * in.activity) *
                         (1.0f + p.phStressPerUnit * in.phStress + in.toxin) * hours;
    m.aerobic = std::max(0.0f, in.o2) / (std::max(0.0f, in.o2) + p.o2HalfSaturation);
    const float penalty = p.fermentationPenalty * in.fermentationScale;
    const float glucose = energy * (m.aerobic + (1.0f - m.aerobic) * penalty);
    out.o2Consumed = kO2PerGlucose * energy * m.aerobic;
    out.co2Fermented = 2.0f * energy * (1.0f - m.aerobic) * penalty; // C₆H₁₂O₆ → 2 C₂H₅OH + 2 CO₂

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
        const float kP = in.phosphorusHalfSaturation > 0.0f ? in.phosphorusHalfSaturation : p.phosphorusHalfSaturation;
        if (m.phosphorus < 1.5f * quota) {
            const float c = std::max(0.0f, in.phosphate);
            const float take = std::min(1.5f * quota - m.phosphorus,
                                        p.phosphorusUptakePerHour * quota * c / (c + kP) * hours);
            m.phosphorus += take;
            out.phosphateTaken = take;
        }
        const float nQuota = p.nitrogenRatio * m.biomass;
        if (m.nitrogen < 1.5f * nQuota) {
            const float c = std::max(0.0f, in.nitrogen);
            const float take = std::min(1.5f * nQuota - m.nitrogen,
                                        p.nitrogenUptakePerHour * nQuota * c / (c + p.nitrogenHalfSaturation) * hours);
            m.nitrogen += take;
            out.nitrogenTaken = take;
        }
    }

    // Micronutrientes: mismo esquema (consumo de lujo hasta 1,5 cuotas).
    auto uptake = [&](float& store, bool needed, float ratio, float halfSat, float perHour, float conc, float& taken) {
        if (!needed)
            return;
        const float q = ratio * m.biomass;
        if (store >= 1.5f * q)
            return;
        const float c = std::max(0.0f, conc);
        taken = std::min(1.5f * q - store, perHour * q * c / (c + halfSat) * hours);
        store += taken;
    };
    uptake(m.silicon, m.needsSilicon, p.siliconRatio,
           in.siliconHalfSaturation > 0.0f ? in.siliconHalfSaturation : p.siliconHalfSaturation, p.siliconUptakePerHour,
           in.silicate,
           out.siliconTaken);
    uptake(m.iron, m.photosynthetic, p.ironRatio, p.ironHalfSaturation, p.ironUptakePerHour, in.iron, out.ironTaken);
    uptake(m.vitamin, m.needsVitamin, p.vitaminRatio, p.vitaminHalfSaturation, p.vitaminUptakePerHour, in.vitamin,
           out.vitaminTaken);

    // Crecimiento con lo que sobra por encima del colchón. Cada factor acota la inversión; el que
    // la deja más baja es el limitante (Liebig).
    const float spare = m.reserves - p.reserveBuffer * m.biomass;
    m.limiting = m.starving ? Limitation::Starving : Limitation::Energy;
    if (spare > 0.0f) {
        const float maxRate = p.growthPerHourMax * in.growthScale * m.biomass * hours / p.growthYield;
        float invest = maxRate;
        m.limiting = Limitation::None;
        auto bound = [&](float cap, Limitation why) {
            if (cap < invest) {
                invest = cap;
                m.limiting = why;
            }
        };
        bound(spare, Limitation::Energy);
        // Sin fósforo no hay biomasa nueva (ácidos nucleicos, fosfolípidos).
        // Ni proteínas sin nitrógeno.
        if (m.photosynthetic) {
            bound(std::max(0.0f, m.phosphorus / p.phosphorusRatio - m.biomass) / p.growthYield, Limitation::Phosphorus);
            bound(std::max(0.0f, m.nitrogen / p.nitrogenRatio - m.biomass) / p.growthYield, Limitation::Nitrogen);
        }
        // Sin sílice la diatomea no arma la valva nueva; sin hierro no hay fotosistemas; sin B₁₂
        // la auxótrofa no sintetiza metionina ni nucleótidos.
        auto limit = [&](bool needed, float store, float ratio, Limitation why) {
            if (needed)
                bound(std::max(0.0f, store / ratio - m.biomass) / p.growthYield, why);
        };
        limit(m.needsSilicon, m.silicon, p.siliconRatio, Limitation::Silicon);
        limit(m.photosynthetic, m.iron, p.ironRatio, Limitation::Iron);
        limit(m.needsVitamin, m.vitamin, p.vitaminRatio, Limitation::Vitamin);
        m.reserves -= invest;
        m.biomass += invest * p.growthYield;
    }
    return out;
}

const char* limitationName(Limitation l) {
    switch (l) {
    case Limitation::None: return "Ninguno (tasa máxima)";
    case Limitation::Energy: return "Energía (comida o luz)";
    case Limitation::Phosphorus: return "Fósforo";
    case Limitation::Nitrogen: return "Nitrógeno";
    case Limitation::Silicon: return "Sílice";
    case Limitation::Iron: return "Hierro";
    case Limitation::Vitamin: return "Vitamina B12";
    case Limitation::Starving: return "Ayuno (autofagia)";
    case Limitation::Count: break;
    }
    return "";
}
