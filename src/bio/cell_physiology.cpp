#include "bio/cell_physiology.hpp"

#include "bio/physiology.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kMicrocystinEC = 0.05f; // [µM] daño apreciable en ciliados
constexpr float kViolaceinDetoxPerHour = 1.0f;
// Osmotrofia (levaduras): captación máxima relativa a la biomasa y afinidad.
constexpr float kOsmotrophUptakePerHour = 1.0f;
constexpr float kOsmotrophHalfSaturation = 5.0f; // [µM]
} // namespace

void applySpeciesPhysiology(MetabolicInput& in, SpeciesId id, const SpeciesInfo& info, const Heredity* heredity,
                            const MetabolismParams& params) {
    // µmax de la especie (fisiologia.toml, con su fuente) relativo al valor general del
    // metabolismo; sin dato, el general.
    const SpeciesPhysiology& physio = speciesPhysiology(id);
    in.phosphorusHalfSaturation = physio.phosphorusHalfSaturation.present ? physio.phosphorusHalfSaturation.value : 0.0f;
    in.siliconHalfSaturation = physio.siliconHalfSaturation.present ? physio.siliconHalfSaturation.value : 0.0f;
    const SourcedValue& mu = physio.growthMax;
    const float speciesGrowth = mu.present && params.growthPerHourMax > 0.0f ? mu.value / params.growthPerHourMax : 1.0f;
    in.growthScale = speciesGrowth;
    if (heredity) {
        in.growthScale = heredity->traits.growthMax * speciesGrowth;
        in.maintenanceScale = heredity->traits.maintenance;
        in.photosynthesisScale = heredity->traits.photosynthesis * info.photosynthesisScale;
        in.motilityCostScale = heredity->traits.motilityCost;
    }
}

void applyCyst(MetabolicInput& in) {
    in.maintenanceScale *= 0.05f;
    in.activity = 0.0f;
    in.photosynthesisScale = 0.0f;
}

const char* applyEnvironmentStress(const SpeciesInfo& info, const CellSurroundings& around, float* violacein,
                                   float bioDt, MetabolicInput& in) {
    in.phStress = std::max(0.0f, info.phMin - around.ph) + std::max(0.0f, around.ph - info.phMax);
    // Fuera de su rango térmico o salino (sin vacuola contráctil que alcance) se estresa; cada
    // 5 °C o 10 PSU de más cuenta como una unidad de pH.
    const float t = around.temperature;
    const float thermal = (std::max(0.0f, info.tMin - t) + std::max(0.0f, t - info.tMax)) / 5.0f;
    const float saline = std::max(0.0f, around.salinity - info.salinityMax) / 10.0f;
    // Anaerobias estrictas (Trichonympha): el O₂ las intoxica.
    const float o2 = std::max(0.0f, around.o2);
    const float oxic = info.anaerobe ? o2 / 20.0f : 0.0f;
    in.phStress += thermal + saline + oxic;
    if (info.anaerobe)
        in.fermentationScale = 0.5f;
    // Microcistina: inhibe fosfatasas; daña a los consumidores (ciliados), no a las algas.
    const float sensitivity = info.chloroplasts ? 0.05f : 1.0f;
    const float microcystin = sensitivity * std::max(0.0f, around.microcystin) / kMicrocystinEC;
    float ingested = 0.0f;
    if (violacein) {
        ingested = *violacein;
        *violacein *= std::exp(-kViolaceinDetoxPerHour * bioDt / 3600.0f);
    }
    in.toxin = microcystin + ingested;
    if (microcystin > 1.0f && microcystin >= ingested)
        return "intoxicada por microcistina";
    if (ingested > 1.0f)
        return "intoxicada por violaceina";
    if (thermal > 1.0f)
        return t > info.tMax ? "calor" : "frio";
    if (saline > 1.0f)
        return "estres osmotico";
    if (oxic > 1.0f)
        return "oxigeno (anaerobia)";
    if (in.phStress > 1.0f)
        return "pH extremo";
    return nullptr;
}

float osmotrophDemand(float biomass, float organic, float bioDt) {
    return kOsmotrophUptakePerHour * biomass * bioDt / 3600.0f * organic / (organic + kOsmotrophHalfSaturation);
}
