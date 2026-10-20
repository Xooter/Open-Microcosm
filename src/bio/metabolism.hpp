#pragma once

#include <cstdint>

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
    float exudation = 0.2f;              // fracción de lo fijado que las algas liberan al agua
    float divisionHours = 0.5f;          // citocinesis
    float playerDivisionCost = 0.25f;    // la reproducción del jugador es la más costosa
    float npcDivisionCost = 0.1f;
    float deathFraction = 0.5f;          // muere al perder la mitad de su biomasa de nacimiento
    // Fósforo (limita a las algas): las fotótrofas lo toman del agua para crecer. Una célula
    // con poco fósforo sigue fotosintetizando y acumula reservas (almidón, paramilón).
    // Cuotas (modelo de Droop, ver updateMetabolism): la cuota de cada nutriente es la mínima Q₀
    // (pmol por pmol de biomasa); la célula acumula hasta luxuryQuota·Q₀ y las medias saturaciones
    // son las de CRECIMIENTO (como se publican).
    float luxuryQuota = 1.5f;            // Qmax/Q₀ (consumo de lujo). SIN VERIFICAR: para el P de algas
                                         // suele ser mayor (ver docs/validacion.md)
    float phosphorusRatio = 0.01f;       // pmol P por pmol de biomasa (Redfield, aprox.)
    float phosphorusHalfSaturation = 0.5f; // [µM]
    // Nitrógeno (amonio y nitrato): también lo toman las fotótrofas. N:P ≈ 16 (Redfield).
    float nitrogenRatio = 0.16f;           // pmol N por pmol de biomasa
    float nitrogenHalfSaturation = 1.0f;   // [µM]
    // Micronutrientes. Cada uno es una cuota (pmol por pmol de biomasa) que la célula toma del agua
    // y que limita su crecimiento (Droop; entre todas, ley del mínimo de Liebig).
    //  - Sílice: solo las diatomeas; cada valva nueva de la frústula la necesita (Si:C ~ 0,1–0,2).
    //  - Hierro: las fotótrofas (fotosistemas, ferredoxina); Fe:C ~ 10⁻⁴ en cultivo.
    //  - Vitamina B₁₂: auxótrofas (Euglena, diatomeas); la sintetizan solo bacterias y arqueas.
    float siliconRatio = 0.02f; // (la frústula real: Si:C ~ 0,1; acá por pmol de materia seca)
    float siliconHalfSaturation = 2.0f;   // [µM]
    float ironRatio = 1e-4f;
    float ironHalfSaturation = 0.05f;     // [µM]
    float vitaminRatio = 1e-7f;
    float vitaminHalfSaturation = 2e-4f;  // [µM] (~200 pM)
    // pH: fuera del rango de la especie el mantenimiento se encarece (bombas de protones).
    float phStressPerUnit = 1.0f;
};

// Qué frena el crecimiento de la célula en el último paso (ley del mínimo de Liebig): el
// factor cuya cuota da menos biomasa nueva. Solo lo lee el mapa de limitación (render) y el
// análisis; no cambia la simulación.
enum class Limitation : std::uint8_t {
    None,       // crece a su tasa máxima
    Energy,     // sin reservas para invertir: falta comida (heterótrofas) o luz (fotótrofas)
    Phosphorus,
    Nitrogen,
    Silicon,
    Iron,
    Vitamin,    // B₁₂ (auxótrofas)
    Starving,   // se consume a sí misma (autofagia)
    Count
};
const char* limitationName(Limitation l);

// Componente.
struct Metabolism {
    float biomass = 0.0f;      // [pmol]
    float birthBiomass = 0.0f; // biomasa al nacer (tras la última división)
    float targetBirthBiomass = 0.0f; // la que pide su genoma: se divide al duplicarla
    float reserves = 0.0f;     // glucógeno/paramilón y otras reservas [pmol]
    float phosphorus = 0.0f;   // [pmol P] (solo se contabiliza en fotótrofas)
    float nitrogen = 0.0f;     // [pmol N] (ídem)
    float silicon = 0.0f;      // [pmol Si] (diatomeas)
    float iron = 0.0f;         // [pmol Fe] (fotótrofas)
    float vitamin = 0.0f;      // [pmol B₁₂] (auxótrofas)
    bool needsSilicon = false, needsVitamin = false;
    int fissions = 1;          // divisiones por ciclo (2 = palintomía en 4 hijas)
    int pendingFissions = 0;   // divisiones que faltan del ciclo actual
    float ageHours = 0.0f;
    float pigment = 0.0f;      // pigmento de estrés (astaxantina) [0, 1] del máximo
    bool photosynthetic = false;
    // Estado del último paso (para el HUD).
    bool starving = false;
    Limitation limiting = Limitation::None;
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
    float phosphate = 0.0f;   // [µM] en la célula
    float nitrogen = 0.0f;    // nitrógeno inorgánico disuelto [µM]
    float silicate = 0.0f;    // [µM] ácido silícico disuelto
    float iron = 0.0f;        // [µM] hierro disuelto biodisponible
    float vitamin = 0.0f;     // [µM] vitamina B₁₂ disuelta
    float phStress = 0.0f;    // unidades de pH fuera del rango tolerado
    float toxin = 0.0f;       // daño por toxinas (microcistina, violaceína): multiplica el gasto
    // Anaerobias con hidrogenosomas (Trichonympha): ~4 ATP por glucosa en vez de 2.
    float fermentationScale = 1.0f;
    // Medias saturaciones de la especie [µM] ([physiology.<id>.*] de cada ser); 0: las generales.
    float phosphorusHalfSaturation = 0.0f;
    float siliconHalfSaturation = 0.0f;
};

struct MetabolicOutput {
    float o2Consumed = 0.0f; // [pmol]
    float o2Produced = 0.0f; // [pmol]
    float phosphateTaken = 0.0f; // [pmol P]
    float nitrogenTaken = 0.0f;  // [pmol N]
    float siliconTaken = 0.0f, ironTaken = 0.0f, vitaminTaken = 0.0f; // [pmol]
    float co2Fermented = 0.0f;   // CO₂ de la fermentación [pmol] (2 por glucosa)
    float exudate = 0.0f;        // materia orgánica liberada al agua [pmol]
};

MetabolicOutput updateMetabolism(Metabolism& m, const MetabolismParams& p, const MetabolicInput& in);

// División: la madre se queda con la fracción `motherShare` de la materia y devuelve la de la hija.
// Las dos empiezan su ciclo (biomasa al nacer, edad 0).
Metabolism splitMetabolism(Metabolism& mother, float motherShare);

// Biomasa con la que se divide [pmol] (0: sin tamaño de nacimiento).
inline float divisionBiomass(const Metabolism& m) {
    const float birth = m.targetBirthBiomass > 0.0f ? m.targetBirthBiomass : m.birthBiomass;
    // En palintomía la célula crece 2^n veces y después se divide n veces seguidas.
    return m.pendingFissions > 0 ? 1.8f * birth : static_cast<float>(1 << m.fissions) * birth;
}

inline bool readyToDivide(const Metabolism& m, float cost) {
    return m.biomass >= divisionBiomass(m) && m.reserves >= cost * m.biomass;
}

inline bool isDead(const Metabolism& m, const MetabolismParams& p) {
    return m.biomass < p.deathFraction * m.birthBiomass;
}
