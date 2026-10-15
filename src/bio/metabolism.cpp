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

    // Nutrientes con cuota celular: modelo de Droop (1968). La célula capta del agua con cinética
    // de Monod hasta una cuota máxima Qmax = L·Q₀ (consumo de lujo) y crece según su propia cuota:
    // µ = µ∞(1 − Q₀/Q), con µ∞ = µmax·L/(L−1) para que a Qmax crezca a µmax. Con la captación
    // máxima Vm = L·µmax·Q₀ y la media saturación de captación Ku = L·K, en equilibrio
    // (µ·Q = captación) el crecimiento sigue exactamente µmax·S/(K+S): K es la media saturación de
    // CRECIMIENTO publicada (Tilman y Kilham 1976). Después de agotarse el nutriente la célula sigue
    // creciendo con su reserva, como en los cultivos reales (antes, Monod en la captación con un
    // tope fijo de 0,3 cuotas/h dejaba la K efectiva varias veces menor que la publicada).
    const float muMax = p.growthPerHourMax * in.growthScale; // [1/h]
    const float luxury = std::max(1.01f, p.luxuryQuota);
    auto uptake = [&](float& store, bool needed, float ratio, float growthHalfSat, float conc, float& taken) {
        if (!needed)
            return;
        const float q0 = ratio * m.biomass;
        if (store >= luxury * q0)
            return;
        const float c = std::max(0.0f, conc);
        const float ku = luxury * growthHalfSat;
        taken = std::min(luxury * q0 - store, luxury * muMax * q0 * c / (c + ku) * hours);
        store += taken;
    };
    const float kP = in.phosphorusHalfSaturation > 0.0f ? in.phosphorusHalfSaturation : p.phosphorusHalfSaturation;
    const float kSi = in.siliconHalfSaturation > 0.0f ? in.siliconHalfSaturation : p.siliconHalfSaturation;
    uptake(m.phosphorus, m.photosynthetic, p.phosphorusRatio, kP, in.phosphate, out.phosphateTaken);
    uptake(m.nitrogen, m.photosynthetic, p.nitrogenRatio, p.nitrogenHalfSaturation, in.nitrogen, out.nitrogenTaken);
    uptake(m.silicon, m.needsSilicon, p.siliconRatio, kSi, in.silicate, out.siliconTaken);
    uptake(m.iron, m.photosynthetic, p.ironRatio, p.ironHalfSaturation, in.iron, out.ironTaken);
    uptake(m.vitamin, m.needsVitamin, p.vitaminRatio, p.vitaminHalfSaturation, in.vitamin, out.vitaminTaken);

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
        // Cada cuota acota con Droop (la tasa) y con la masa (la reserva tiene que cubrir la cuota
        // mínima de la biomasa nueva). Sin fósforo no hay ácidos nucleicos ni fosfolípidos; sin
        // nitrógeno, proteínas; sin sílice la diatomea no arma la valva nueva; sin hierro no hay
        // fotosistemas; sin B₁₂ la auxótrofa no sintetiza metionina ni nucleótidos.
        auto limit = [&](bool needed, float store, float ratio, Limitation why) {
            if (!needed)
                return;
            const float q0 = ratio * m.biomass;
            const float droop = q0 > 0.0f ? luxury / (luxury - 1.0f) * (1.0f - q0 / std::max(store, 1e-30f)) : 1.0f;
            bound(maxRate * std::clamp(droop, 0.0f, 1.0f), why);
            bound(std::max(0.0f, store / ratio - m.biomass) / p.growthYield, why);
        };
        limit(m.photosynthetic, m.phosphorus, p.phosphorusRatio, Limitation::Phosphorus);
        limit(m.photosynthetic, m.nitrogen, p.nitrogenRatio, Limitation::Nitrogen);
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
