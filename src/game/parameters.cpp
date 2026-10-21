#include "game/parameters.hpp"

#include "game/legacy_names.hpp"
#include "game/world.hpp"

#include <cmath>

namespace {

// Acceso al valor dentro del mundo (lambdas sin captura → punteros a función).
#define BIO_REAL(expr) [](World& w) -> float* { return &(expr); }, nullptr
#define BIO_INT(expr) nullptr, [](World& w) -> int* { return &(expr); }

using G = ParamGroup;

const ParamInfo kParams[] = {
    // --- Química
    {"medium.alkalinity", "Alcalinidad [µM]", "µM", G::Chemistry, 50.0f, 5000.0f, "%.0f", true, nullptr,
     BIO_REAL(w.medium().alkalinity)},
    {"medium.decomposition", "Descomposición [1/h bio]", "1/s bio", G::Chemistry, 0.0f, 0.02f, "%.4f", false, nullptr,
     BIO_REAL(w.medium().detritusDecay)},
    {"medium.nutrient_uptake", "Consumo de nutrientes [1/s]", "1/s", G::Chemistry, 0.0f, 0.05f, "%.3f", false, nullptr,
     BIO_REAL(w.medium().nutrientUptake)},
    {"medium.detritus_inflow", "Detritos que llegan [1/s]", "1/s", G::Chemistry, 0.0f, 5.0f, "%.1f", false, nullptr,
     BIO_REAL(w.medium().detritusSpawnRate)},
    {"medium.organic_inflow", "Aporte orgánico [µM/h]", "µM/h", G::Chemistry, 0.0f, 500.0f, "%.0f", false, nullptr,
     BIO_REAL(w.medium().organicInflow)},
    {"medium.dust", "Polvo mineral [1/s]", "1/s", G::Chemistry, 0.0f, 0.2f, "%.3f", false, nullptr,
     BIO_REAL(w.medium().dustRate)},
    {"medium.perfusion", "Perfusión del medio [1/h bio]", "1/h", G::Chemistry, 0.0f, 5.0f, "%.3f", false,
     "Renueva lo disuelto con el medio de la placa al crearla (cámara de perfusión); las células se quedan.",
     BIO_REAL(w.medium().perfusionPerHour)},
    // --- Micronutrientes
    {"medium.b12_source", "B12 de las bacterias", "µM/h", G::Micronutrients, 1e-7f, 1e-3f, "%.1e", true, nullptr,
     BIO_REAL(w.medium().vitaminSourcePerHour)},
    {"medium.b12_decay", "B12 la degrada la luz [1/h]", "1/h", G::Micronutrients, 0.0f, 0.5f, "%.3f", false, nullptr,
     BIO_REAL(w.medium().vitaminDecayPerHour)},
    {"medium.frustule_dissolution", "Disolución de frústulas [1/h]", "1/h", G::Micronutrients, 0.0f, 0.05f, "%.4f", false,
     nullptr, BIO_REAL(w.medium().frustuleDissolutionPerHour)},
    // --- Genética
    {"genetics.mutation_scale", "Escala de la mutación", "×", G::Genetics, 0.001f, 10000.0f, "%.3g", true,
     "Multiplica la varianza que la mutación agrega en cada división (1 = la publicada: ~0,5 % por gen y por "
     "generación). Más de 1 es una evolución acelerada, útil para verla en una sesión pero no realista.",
     BIO_REAL(w.mutationScale())},
    // --- Microbiología
    {"bacteria.chemotaxis", "Quimiotaxis", "", G::Microbiology, 0.0f, 20.0f, "%.1f", false, nullptr,
     BIO_REAL(w.bacteria().params.chemotaxisGain)},
    {"bacteria.starvation_maintenance", "Bacterias: gasto en ayuno (x)", "", G::Microbiology, 0.05f, 1.0f, "%.2f", false,
     "Fracción de la mantención que gasta una bacteria sin comida suficiente (fase estacionaria). "
     "E. coli la baja más de 5 veces (Schink et al. 2019).",
     BIO_REAL(w.bacteria().params.starvationMaintenance)},
    {"bacteria.adhesion", "Bacterias: adhesión al vidrio", "por choque", G::Microbiology, 0.0f, 0.2f, "%.3f", false,
     "Probabilidad de quedar adherida al chocar contra el vidrio (biopelícula: refugio contra los filtradores). "
     "Estimación.",
     BIO_REAL(w.bacteria().params.attachProbability)},
    {"bacteria.detachment", "Bacterias: desprendimiento [1/h]", "1/h", G::Microbiology, 0.0f, 2.0f, "%.2f", false,
     "Tasa a la que las bacterias de la biopelícula se sueltan y vuelven a nadar. Estimación.",
     BIO_REAL(w.bacteria().params.detachPerHour)},
    {"medium.phage_inactivation", "Fagos: inactivación [1/h]", "1/h", G::Microbiology, 0.0f, 1.0f, "%.2f", false, nullptr,
     BIO_REAL(w.medium().phageDecayPerHour)},
    {"medium.toxin_decay", "Toxinas: degradación [1/h]", "1/h", G::Microbiology, 0.0f, 0.5f, "%.2f", false, nullptr,
     BIO_REAL(w.medium().toxinDecayPerHour)},
    {"medium.ahl_half_life", "AHL: vida media [h]", "h", G::Microbiology, 1.0f, 100.0f, "%.0f", false, nullptr,
     BIO_REAL(w.medium().ahlHalfLifeHours)},
    // --- Hongos
    {"fungi.absorption", "Absorción [1/h]", "1/h", G::Fungi, 0.0f, 4.0f, "%.2f", false, nullptr,
     BIO_REAL(w.fungi().params.uptakePerHour)},
    {"fungi.maintenance", "Mantenimiento [1/h]", "1/h", G::Fungi, 0.0f, 0.2f, "%.3f", false, nullptr,
     BIO_REAL(w.fungi().params.maintenancePerHour)},
    {"fungi.starvation_to_spores", "Hambre a esporas [h]", "h", G::Fungi, 0.5f, 20.0f, "%.1f", false, nullptr,
     BIO_REAL(w.fungi().params.starvationHoursToSporulate)},
    {"fungi.exoenzymes", "Exoenzimas (x)", "", G::Fungi, 1.0f, 100.0f, "%.0f", false, nullptr,
     BIO_REAL(w.fungi().params.detritusDecayBoost)},
    {"fungi.allomyces_wakeup", "Allomyces: despierta [µM]", "µM", G::Fungi, 1.0f, 50.0f, "%.0f", false, nullptr,
     BIO_REAL(w.fungi().params.restingGerminateFood)},
    {"fungi.lichen_sugar", "Liquen: azúcar [1/h]", "1/h", G::Fungi, 0.0f, 0.5f, "%.2f", false, nullptr,
     BIO_REAL(w.fungi().params.lichenSugarPerHour)},
    {"fungi.lichen_phosphorus", "Liquen: fósforo [1/h]", "1/h", G::Fungi, 0.0f, 0.5f, "%.2f", false, nullptr,
     BIO_REAL(w.fungi().params.lichenPhosphorusPerHour)},
    // --- Physarum
    {"physarum.front", "Frente [µm/h bio]", "µm/h", G::Plasmodium, 200.0f, 8000.0f, "%.0f", false, nullptr,
     BIO_REAL(w.plasmodia().params.tipSpeedPerHour)},
    {"physarum.adaptation", "Adaptación de tubos [1/h]", "1/h", G::Plasmodium, 0.1f, 4.0f, "%.2f", false, nullptr,
     BIO_REAL(w.plasmodia().params.adaptRate)},
    {"physarum.gamma", "Respuesta al flujo (gamma)", "", G::Plasmodium, 0.5f, 3.0f, "%.2f", false, nullptr,
     BIO_REAL(w.plasmodia().params.gamma)},
    {"physarum.turnover", "Cambio fuente/sumidero [h]", "h", G::Plasmodium, 0.02f, 2.0f, "%.2f", false, nullptr,
     BIO_REAL(w.plasmodia().params.switchHours)},
    {"physarum.slime_avoidance", "Evita su baba", "", G::Plasmodium, 0.0f, 4.0f, "%.1f", false, nullptr,
     BIO_REAL(w.plasmodia().params.slimeAvoidance)},
    {"physarum.light_avoidance", "Evita la luz", "", G::Plasmodium, 0.0f, 4.0f, "%.1f", false, nullptr,
     BIO_REAL(w.plasmodia().params.lightAvoidance)},
    {"physarum.retraction", "Retracción (D mínimo)", "", G::Plasmodium, 0.0f, 0.2f, "%.3f", false, nullptr,
     BIO_REAL(w.plasmodia().params.pruneD)},
    {"physarum.starvation_to_sclerotium", "Hambre a esclerocio [h]", "h", G::Plasmodium, 1.0f, 40.0f, "%.0f", false, nullptr,
     BIO_REAL(w.plasmodia().params.starveHoursToSclerotium)},
    // --- Ambiente (los cambia el panel Placa con su intervención; acá, para lecciones y programas)
    {"environment.temperature", "Temperatura [°C]", "°C", G::Environment, -2.0f, 60.0f, "%.1f", false, nullptr,
     BIO_REAL(w.medium().temperature)},
    {"environment.salinity", "Salinidad [g/kg]", "g/kg", G::Environment, 0.0f, 100.0f, "%.1f", false,
     "Sales disueltas: con los osmolitos fijan la osmolaridad del medio (plasmólisis, hemólisis).",
     BIO_REAL(w.medium().salinity)},
    {"environment.light", "Luz", "", G::Environment, 0.0f, 1.0f, "%.2f", false, nullptr, BIO_REAL(w.medium().surfaceLight)},
    {"environment.osmolytes", "Sacarosa [mOsm/L]", "mOsm/L", G::Environment, 0.0f, 2000.0f, "%.0f", false,
     "Osmolitos que no cruzan la membrana (sacarosa, manitol).", BIO_REAL(w.medium().osmolytes)},
    // --- Tiempo
    {"time.bio_scale", "Tiempo biológico (x)", "s bio/s", G::Time, 1.0f, 600.0f, "%.0f", false,
     "Compresión de la biología: 60 = 1 min real es 1 h biológica. La física siempre va en tiempo real.",
     BIO_REAL(w.bioTimeScale())},
    // --- Mecánica (agua y cuerpos blandos)
    {"water.film_thickness", "Espesor de película [µm]", "µm", G::Mechanics, 20.0f, 500.0f, "%.0f", false,
     "Controla qué tan locales son las corrientes.", BIO_REAL(w.fluid().gapHeight)},
    {"xpbd.substeps", "Subpasos XPBD", "", G::Mechanics, 1.0f, 30.0f, "%.0f", false, nullptr,
     BIO_INT(w.softTuning().substeps)},
    {"xpbd.shape_memory", "Memoria de forma (x)", "", G::Mechanics, 0.0f, 3.0f, "%.2f", false, nullptr,
     BIO_REAL(w.softTuning().shapeScale)},
    {"xpbd.bending", "Flexión [1/s]", "1/s", G::Mechanics, 0.0f, 400.0f, "%.0f", false, nullptr,
     BIO_REAL(w.softTuning().bendRate)},
    {"xpbd.area", "Área [1/s]", "1/s", G::Mechanics, 10.0f, 3000.0f, "%.0f", false, nullptr,
     BIO_REAL(w.softTuning().areaRate)},
    {"xpbd.edges", "Aristas [1/s]", "1/s", G::Mechanics, 10.0f, 10000.0f, "%.0f", true, nullptr,
     BIO_REAL(w.softTuning().edgeRate)},
    {"xpbd.iterations", "Iteraciones por subpaso", "", G::Mechanics, 1.0f, 10.0f, "%.0f", false, nullptr,
     BIO_INT(w.softTuning().iterations)},
    {"xpbd.contact_passes", "Pasadas de contacto", "", G::Mechanics, 1.0f, 10.0f, "%.0f", false, nullptr,
     BIO_INT(w.softTuning().contactPasses)},
    {"lod.stride", "LOD: sub-pasos lejanos (1 de cada N)", "", G::Engine, 1.0f, 10.0f, "%.0f", false, nullptr,
     BIO_INT(w.lodStride)},
    // --- Metabolismo (constantes fisiológicas: se exportan, sin control)
    {"metabolism.maintenance", "Mantenimiento", "1/h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().maintenancePerHour)},
    {"metabolism.swim_cost", "Costo de nadar", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().motilityCost)},
    {"metabolism.max_growth", "Crecimiento máximo", "1/h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().growthPerHourMax)},
    {"metabolism.yield", "Rendimiento", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().growthYield)},
    {"metabolism.storage", "Reserva sin invertir", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().reserveBuffer)},
    {"metabolism.o2_half_saturation", "O2: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().o2HalfSaturation)},
    {"metabolism.fermentation_penalty", "Penalidad de la fermentación", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().fermentationPenalty)},
    {"metabolism.max_photosynthesis", "Fotosíntesis máxima", "1/h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().photosynthesisPerHour)},
    {"metabolism.exudation", "Exudación", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().exudation)},
    {"metabolism.division", "Citocinesis", "h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().divisionHours)},
    {"metabolism.division_cost", "Costo de la división", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().npcDivisionCost)},
    {"metabolism.controlled_division_cost", "Costo de la división (ser controlado)", "", G::Metabolism, 0, 0, "%g",
     false, nullptr, BIO_REAL(w.metabolismParams().playerDivisionCost)},
    {"metabolism.death_fraction", "Fracción de biomasa para morir", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().deathFraction)},
    {"metabolism.max_quota", "Cuota máxima (Droop)", "× cuota mínima", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().luxuryQuota)},
    {"metabolism.p_quota", "Cuota de P", "pmol P/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().phosphorusRatio)},
    {"metabolism.p_half_saturation", "P: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().phosphorusHalfSaturation)},
    {"metabolism.n_quota", "Cuota de N", "pmol N/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().nitrogenRatio)},
    {"metabolism.n_half_saturation", "N: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().nitrogenHalfSaturation)},
    {"metabolism.si_quota", "Cuota de Si", "pmol Si/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().siliconRatio)},
    {"metabolism.fe_quota", "Cuota de Fe", "pmol Fe/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().ironRatio)},
    {"metabolism.b12_quota", "Cuota de B12", "pmol/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().vitaminRatio)},
    {"metabolism.si_half_saturation", "Si: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().siliconHalfSaturation)},
    {"metabolism.fe_half_saturation", "Fe: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().ironHalfSaturation)},
    {"metabolism.b12_half_saturation", "B12: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().vitaminHalfSaturation)},
};

#undef BIO_REAL
#undef BIO_INT

} // namespace

std::span<const ParamInfo> parameters() { return kParams; }

const ParamInfo* findParameter(std::string_view key) {
    for (const ParamInfo& p : kParams)
        if (key == p.key)
            return &p;
    // Una escena o una lección guardada con la clave vieja (en español).
    const std::string_view current = currentParameterKey(key);
    return current.empty() ? nullptr : findParameter(current);
}

float parameterValue(World& world, const ParamInfo& p) {
    return p.real ? *p.real(world) : static_cast<float>(*p.integer(world));
}

void setParameterValue(World& world, const ParamInfo& p, float value) {
    // Sin acotar al rango del control: un valor registrado se reproduce tal cual.
    if (p.real)
        *p.real(world) = value;
    else
        *p.integer(world) = static_cast<int>(std::lround(value));
    if (p.group == ParamGroup::Environment)
        world.refreshMedium(); // Q10, viscosidad y gases de equilibrio dependen del ambiente
}

const char* paramGroupName(ParamGroup group) {
    switch (group) {
    case ParamGroup::Chemistry: return "Química";
    case ParamGroup::Micronutrients: return "Micronutrientes";
    case ParamGroup::Microbiology: return "Microbiología";
    case ParamGroup::Fungi: return "Hongos y mohos";
    case ParamGroup::Plasmodium: return "Physarum";
    case ParamGroup::Mechanics: return "Agua y cuerpos blandos";
    case ParamGroup::Time: return "Tiempo";
    case ParamGroup::Metabolism: return "Metabolismo";
    case ParamGroup::Engine: return "Motor";
    case ParamGroup::Environment: return "Ambiente";
    case ParamGroup::Genetics: return "Genética";
    }
    return "";
}
