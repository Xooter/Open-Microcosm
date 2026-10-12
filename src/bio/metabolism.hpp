#pragma once

// Metabolismo simplificado pero real. Todo en pmol equivalentes de glucosa de materia seca y en
// tiempo BIOLÓGICO (units::kBioTimeScale).
//
//  - Respiración (con O₂): ~30 ATP por glucosa. Fermentación (sin O₂): 2 ATP, o sea, ~15 veces
//    más glucosa para la misma energía. La fracción aeróbica sigue una cinética de saturación
//    con el O₂ local (la citocromo oxidasa se satura a pocos µM).
//  - Mantenimiento: la célula gasta una fracción de su biomasa por hora; nadar la aumenta.
//  - Crecimiento: las reservas por encima de un colchón se convierten en biomasa con un
//    rendimiento < 1 y una tasa máxima.
//  - Fotosíntesis (cloroplastos): produce reservas con la luz local y libera O₂.
//  - Inanición: sin reservas, la célula se consume a sí misma (autofagia) hasta morir.
struct MetabolismParams {
    float maintenancePerHour = 0.01f;    // fracción de la biomasa [1/h]
    float motilityCost = 1.0f;           // nadar a velocidad máxima duplica el gasto
    float growthPerHourMax = 0.15f;      // duplicación mínima ~4,6 h (Paramecium: ~5–8 h)
    float growthYield = 0.6f;            // biomasa formada por glucosa invertida
    float reserveBuffer = 0.1f;          // reservas (fracción de la biomasa) que no se invierten
    float o2HalfSaturation = 2.0f;       // [µM]
    float fermentationPenalty = 15.0f;   // 30 ATP/glucosa (respiración) vs 2 (fermentación)
    float photosynthesisPerHour = 0.08f; // Euglena a plena luz: duplicación ~1 día
    float divisionHours = 0.5f;          // citocinesis
    float playerDivisionCost = 0.25f;    // la reproducción del jugador es la más costosa
    float npcDivisionCost = 0.1f;
    float deathFraction = 0.5f;          // muere al perder la mitad de su biomasa de nacimiento
};

// Componente.
struct Metabolism {
    float biomass = 0.0f;      // [pmol]
    float birthBiomass = 0.0f; // biomasa al nacer (tras la última división)
    float targetBirthBiomass = 0.0f; // la que pide su genoma: se divide al duplicarla
    float reserves = 0.0f;     // glucógeno/paramilón y otras reservas [pmol]
    float ageHours = 0.0f;
    bool photosynthetic = false;
    // Estado del último paso (para el HUD).
    bool starving = false;
    float aerobic = 1.0f;
    float o2 = 0.0f, light = 0.0f;
};

struct MetabolicInput {
    float o2;            // [µM] en la célula
    float light;         // irradiancia relativa [0, 1]
    float digested;      // alimento digerido en este paso [pmol]
    float activity;      // [0, 1] fracción de la velocidad máxima de nado
    float bioDt;         // [s biológicos]
    // Multiplicadores genéticos (ver Traits).
    float growthScale = 1.0f;
    float maintenanceScale = 1.0f;
    float photosynthesisScale = 1.0f;
    float motilityCostScale = 1.0f;
};

struct MetabolicOutput {
    float o2Consumed = 0.0f; // [pmol]
    float o2Produced = 0.0f; // [pmol]
};

MetabolicOutput updateMetabolism(Metabolism& m, const MetabolismParams& p, const MetabolicInput& in);

inline bool readyToDivide(const Metabolism& m, float cost) {
    const float birth = m.targetBirthBiomass > 0.0f ? m.targetBirthBiomass : m.birthBiomass;
    return m.biomass >= 2.0f * birth && m.reserves >= cost * m.biomass;
}

inline bool isDead(const Metabolism& m, const MetabolismParams& p) {
    return m.biomass < p.deathFraction * m.birthBiomass;
}
