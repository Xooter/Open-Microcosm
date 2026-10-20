#include "bio/cell_physiology.hpp"

#include "bio/feeding.hpp"
#include "bio/physiology.hpp"
#include "core/units.hpp"

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
    // µmax de la especie ([physiology.<id>.*], con su fuente) relativo al valor general del
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
    // Daño fotodinámico (Blepharisma): proporcional a la luz por encima del umbral; el pigmento de estrés
    // la filtra. Necesita O₂ (las especies reactivas salen de él): en anoxia no daña.
    float photo = 0.0f;
    if (const LightDamageSpec& ld = info.lightDamage; ld.stress > 0.0f && around.light > ld.threshold) {
        photo = ld.stress * (around.light - ld.threshold) / (1.0f - ld.threshold) *
                (1.0f - info.stressPigment.protection * std::clamp(around.pigment, 0.0f, 1.0f)) *
                std::min(1.0f, std::max(0.0f, around.o2) / 20.0f);
    }
    in.phStress += thermal + saline + oxic + photo;
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
    if (photo > 1.0f)
        return "dano por luz";
    if (in.phStress > 1.0f)
        return "pH extremo";
    return nullptr;
}

float stepPhotosymbionts(const SpeciesInfo& host, const MetabolismParams& p, Metabolism& m, float light, float bioDt) {
    const PhotosymbiontSpec& ps = host.photosymbiont;
    if (!ps.present || m.symbionts <= 0.0f)
        return 0.0f;
    const float hours = bioDt / 3600.0f;
    const float l = std::clamp(light, 0.0f, 1.0f);
    if (ps.kleptoplasts) {
        m.symbionts *= std::exp(-ps.decayPerHour * hours); // los cloroplastos robados se gastan
    } else {
        // Las algas se dividen con luz dentro del huésped (que regula su número) y se pierden a oscuras.
        const float growth = ps.growthPerHour * l * (1.0f - m.symbionts / ps.max) - ps.decayPerHour * (1.0f - l);
        m.symbionts = std::clamp(m.symbionts * std::exp(growth * hours), 0.0f, ps.max);
    }
    // Azúcar (maltosa en P. bursaria) proporcional a la carga y a la luz, como la fotosíntesis de un alga.
    return p.photosynthesisPerHour * ps.photosynthesis * (m.symbionts / ps.max) * l * m.biomass * hours;
}

bool acquireSymbionts(const SpeciesInfo& host, SpeciesId prey, Metabolism& m) {
    const PhotosymbiontSpec& ps = host.photosymbiont;
    if (!ps.present || !ps.source.test(static_cast<std::size_t>(prey)) || m.symbionts >= ps.max)
        return false;
    m.symbionts = std::min(ps.max, m.symbionts + ps.perPrey);
    return true;
}

float loricaIronDemand(const SpeciesInfo& info, const Metabolism& m, float bioDt) {
    if (info.loricaIronRatio <= 0.0f || info.loricaHours <= 0.0f)
        return 0.0f;
    const float deficit = info.loricaIronRatio * m.biomass - m.loricaIron;
    return deficit > 0.0f ? deficit * (1.0f - std::exp(-bioDt / (info.loricaHours * 3600.0f))) : 0.0f;
}

void updateStressPigment(const SpeciesInfo& info, float light, Limitation limiting, bool cyst, float bioDt,
                         Metabolism& m) {
    const StressPigmentSpec& sp = info.stressPigment;
    if (sp.ratePerHour <= 0.0f)
        return;
    const float hours = bioDt / 3600.0f;
    const float lightStress = std::max(0.0f, light - sp.lightThreshold) / std::max(1e-3f, 1.0f - sp.lightThreshold);
    const float stress = std::min(1.0f, lightStress + (limiting == Limitation::Nitrogen ? 1.0f : 0.0f));
    // En el quiste (aplanóspora) la síntesis es mayor: es donde la célula se vuelve roja.
    const float rate = sp.ratePerHour * stress * (cyst ? 2.0f : 1.0f);
    m.pigment += (rate * (1.0f - m.pigment) - sp.decayPerHour * (stress > 0.0f ? 0.0f : m.pigment)) * hours;
    m.pigment = std::clamp(m.pigment, 0.0f, 1.0f);
}

float osmotrophDemand(float biomass, float organic, float bioDt) {
    return kOsmotrophUptakePerHour * biomass * bioDt / 3600.0f * organic / (organic + kOsmotrophHalfSaturation);
}

float targetBirthBiomass(SpeciesId species, float size) {
    const CellSpec base = speciesInfo(species).spec;
    return cellMassPmol(units::kPi * base.radiusX * base.radiusY) * size * size * size *
           cellMassScale(speciesInfo(species));
}

void daughterGenomes(const Heredity& mother, LocomotionType type, Rng& ra, Rng& rb, float rate, Heredity& a,
                     Heredity& b) {
    b = mother;
    a = mother;
    mutate(a.genome, type, ra, rate);
    mutate(b.genome, type, rb, rate);
    a.traits = traitsOf(a.genome, type);
    b.traits = traitsOf(b.genome, type);
}

void shrinkFrustule(Heredity& h, LocomotionType type) {
    h.genome[Gene::Size] *= 0.97f;
    if (h.genome[Gene::Size] < 0.75f)
        h.genome[Gene::Size] = 1.0f; // auxospora
    h.traits = traitsOf(h.genome, type);
}
