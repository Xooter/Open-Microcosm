#include "game/parameters.hpp"

#include "game/world.hpp"

#include <cmath>

namespace {

// Acceso al valor dentro del mundo (lambdas sin captura → punteros a función).
#define BIO_REAL(expr) [](World& w) -> float* { return &(expr); }, nullptr
#define BIO_INT(expr) nullptr, [](World& w) -> int* { return &(expr); }

using G = ParamGroup;

const ParamInfo kParams[] = {
    // --- Química
    {"medio.alcalinidad", "Alcalinidad [µM]", "µM", G::Chemistry, 50.0f, 5000.0f, "%.0f", true, nullptr,
     BIO_REAL(w.medium().alkalinity)},
    {"medio.descomposicion", "Descomposición [1/h bio]", "1/s bio", G::Chemistry, 0.0f, 0.02f, "%.4f", false, nullptr,
     BIO_REAL(w.medium().detritusDecay)},
    {"medio.consumo_nutrientes", "Consumo de nutrientes [1/s]", "1/s", G::Chemistry, 0.0f, 0.05f, "%.3f", false, nullptr,
     BIO_REAL(w.medium().nutrientUptake)},
    {"medio.detritos_entrantes", "Detritos que llegan [1/s]", "1/s", G::Chemistry, 0.0f, 5.0f, "%.1f", false, nullptr,
     BIO_REAL(w.medium().detritusSpawnRate)},
    {"medio.aporte_organico", "Aporte orgánico [µM/h]", "µM/h", G::Chemistry, 0.0f, 500.0f, "%.0f", false, nullptr,
     BIO_REAL(w.medium().organicInflow)},
    {"medio.polvo", "Polvo mineral [1/s]", "1/s", G::Chemistry, 0.0f, 0.2f, "%.3f", false, nullptr,
     BIO_REAL(w.medium().dustRate)},
    {"medio.perfusion", "Perfusión del medio [1/h bio]", "1/h", G::Chemistry, 0.0f, 5.0f, "%.3f", false,
     "Renueva lo disuelto con el medio de la placa al crearla (cámara de perfusión); las células se quedan.",
     BIO_REAL(w.medium().perfusionPerHour)},
    // --- Micronutrientes
    {"medio.b12_fuente", "B12 de las bacterias", "µM/h", G::Micronutrients, 1e-7f, 1e-3f, "%.1e", true, nullptr,
     BIO_REAL(w.medium().vitaminSourcePerHour)},
    {"medio.b12_degradacion", "B12 la degrada la luz [1/h]", "1/h", G::Micronutrients, 0.0f, 0.5f, "%.3f", false, nullptr,
     BIO_REAL(w.medium().vitaminDecayPerHour)},
    {"medio.disolucion_frustulas", "Disolución de frústulas [1/h]", "1/h", G::Micronutrients, 0.0f, 0.05f, "%.4f", false,
     nullptr, BIO_REAL(w.medium().frustuleDissolutionPerHour)},
    // --- Microbiología
    {"bacterias.quimiotaxis", "Quimiotaxis", "", G::Microbiology, 0.0f, 20.0f, "%.1f", false, nullptr,
     BIO_REAL(w.bacteria().params.chemotaxisGain)},
    {"bacterias.mantencion_ayuno", "Bacterias: gasto en ayuno (x)", "", G::Microbiology, 0.05f, 1.0f, "%.2f", false,
     "Fracción de la mantención que gasta una bacteria sin comida suficiente (fase estacionaria). "
     "E. coli la baja más de 5 veces (Schink et al. 2019).",
     BIO_REAL(w.bacteria().params.starvationMaintenance)},
    {"bacterias.adhesion", "Bacterias: adhesión al vidrio", "por choque", G::Microbiology, 0.0f, 0.2f, "%.3f", false,
     "Probabilidad de quedar adherida al chocar contra el vidrio (biopelícula: refugio contra los filtradores). "
     "Estimación.",
     BIO_REAL(w.bacteria().params.attachProbability)},
    {"bacterias.desprendimiento", "Bacterias: desprendimiento [1/h]", "1/h", G::Microbiology, 0.0f, 2.0f, "%.2f", false,
     "Tasa a la que las bacterias de la biopelícula se sueltan y vuelven a nadar. Estimación.",
     BIO_REAL(w.bacteria().params.detachPerHour)},
    {"medio.fagos_inactivacion", "Fagos: inactivación [1/h]", "1/h", G::Microbiology, 0.0f, 1.0f, "%.2f", false, nullptr,
     BIO_REAL(w.medium().phageDecayPerHour)},
    {"medio.toxinas_degradacion", "Toxinas: degradación [1/h]", "1/h", G::Microbiology, 0.0f, 0.5f, "%.2f", false, nullptr,
     BIO_REAL(w.medium().toxinDecayPerHour)},
    {"medio.ahl_vida_media", "AHL: vida media [h]", "h", G::Microbiology, 1.0f, 100.0f, "%.0f", false, nullptr,
     BIO_REAL(w.medium().ahlHalfLifeHours)},
    // --- Hongos
    {"hongos.absorcion", "Absorción [1/h]", "1/h", G::Fungi, 0.0f, 4.0f, "%.2f", false, nullptr,
     BIO_REAL(w.fungi().params.uptakePerHour)},
    {"hongos.mantenimiento", "Mantenimiento [1/h]", "1/h", G::Fungi, 0.0f, 0.2f, "%.3f", false, nullptr,
     BIO_REAL(w.fungi().params.maintenancePerHour)},
    {"hongos.hambre_a_esporas", "Hambre a esporas [h]", "h", G::Fungi, 0.5f, 20.0f, "%.1f", false, nullptr,
     BIO_REAL(w.fungi().params.starvationHoursToSporulate)},
    {"hongos.exoenzimas", "Exoenzimas (x)", "", G::Fungi, 1.0f, 100.0f, "%.0f", false, nullptr,
     BIO_REAL(w.fungi().params.detritusDecayBoost)},
    {"hongos.allomyces_despierta", "Allomyces: despierta [µM]", "µM", G::Fungi, 1.0f, 50.0f, "%.0f", false, nullptr,
     BIO_REAL(w.fungi().params.restingGerminateFood)},
    {"hongos.liquen_azucar", "Liquen: azúcar [1/h]", "1/h", G::Fungi, 0.0f, 0.5f, "%.2f", false, nullptr,
     BIO_REAL(w.fungi().params.lichenSugarPerHour)},
    {"hongos.liquen_fosforo", "Liquen: fósforo [1/h]", "1/h", G::Fungi, 0.0f, 0.5f, "%.2f", false, nullptr,
     BIO_REAL(w.fungi().params.lichenPhosphorusPerHour)},
    // --- Physarum
    {"physarum.frente", "Frente [µm/h bio]", "µm/h", G::Plasmodium, 200.0f, 8000.0f, "%.0f", false, nullptr,
     BIO_REAL(w.plasmodia().params.tipSpeedPerHour)},
    {"physarum.adaptacion", "Adaptación de tubos [1/h]", "1/h", G::Plasmodium, 0.1f, 4.0f, "%.2f", false, nullptr,
     BIO_REAL(w.plasmodia().params.adaptRate)},
    {"physarum.gamma", "Respuesta al flujo (gamma)", "", G::Plasmodium, 0.5f, 3.0f, "%.2f", false, nullptr,
     BIO_REAL(w.plasmodia().params.gamma)},
    {"physarum.cambio", "Cambio fuente/sumidero [h]", "h", G::Plasmodium, 0.02f, 2.0f, "%.2f", false, nullptr,
     BIO_REAL(w.plasmodia().params.switchHours)},
    {"physarum.evita_baba", "Evita su baba", "", G::Plasmodium, 0.0f, 4.0f, "%.1f", false, nullptr,
     BIO_REAL(w.plasmodia().params.slimeAvoidance)},
    {"physarum.evita_luz", "Evita la luz", "", G::Plasmodium, 0.0f, 4.0f, "%.1f", false, nullptr,
     BIO_REAL(w.plasmodia().params.lightAvoidance)},
    {"physarum.retraccion", "Retracción (D mínimo)", "", G::Plasmodium, 0.0f, 0.2f, "%.3f", false, nullptr,
     BIO_REAL(w.plasmodia().params.pruneD)},
    {"physarum.hambre_a_esclerocio", "Hambre a esclerocio [h]", "h", G::Plasmodium, 1.0f, 40.0f, "%.0f", false, nullptr,
     BIO_REAL(w.plasmodia().params.starveHoursToSclerotium)},
    // --- Ambiente (los cambia el panel Placa con su intervención; acá, para lecciones y programas)
    {"ambiente.temperatura", "Temperatura [°C]", "°C", G::Environment, -2.0f, 60.0f, "%.1f", false, nullptr,
     BIO_REAL(w.medium().temperature)},
    {"ambiente.salinidad", "Salinidad [g/kg]", "g/kg", G::Environment, 0.0f, 100.0f, "%.1f", false,
     "Sales disueltas: con los osmolitos fijan la osmolaridad del medio (plasmólisis, hemólisis).",
     BIO_REAL(w.medium().salinity)},
    {"ambiente.luz", "Luz", "", G::Environment, 0.0f, 1.0f, "%.2f", false, nullptr, BIO_REAL(w.medium().surfaceLight)},
    {"ambiente.osmolitos", "Sacarosa [mOsm/L]", "mOsm/L", G::Environment, 0.0f, 2000.0f, "%.0f", false,
     "Osmolitos que no cruzan la membrana (sacarosa, manitol).", BIO_REAL(w.medium().osmolytes)},
    // --- Tiempo
    {"tiempo.escala_biologica", "Tiempo biológico (x)", "s bio/s", G::Time, 1.0f, 600.0f, "%.0f", false,
     "Compresión de la biología: 60 = 1 min real es 1 h biológica. La física siempre va en tiempo real.",
     BIO_REAL(w.bioTimeScale())},
    // --- Mecánica (agua y cuerpos blandos)
    {"agua.espesor_pelicula", "Espesor de película [µm]", "µm", G::Mechanics, 20.0f, 500.0f, "%.0f", false,
     "Controla qué tan locales son las corrientes.", BIO_REAL(w.fluid().gapHeight)},
    {"xpbd.subpasos", "Subpasos XPBD", "", G::Mechanics, 1.0f, 30.0f, "%.0f", false, nullptr,
     BIO_INT(w.softTuning().substeps)},
    {"xpbd.memoria_forma", "Memoria de forma (x)", "", G::Mechanics, 0.0f, 3.0f, "%.2f", false, nullptr,
     BIO_REAL(w.softTuning().shapeScale)},
    {"xpbd.flexion", "Flexión [1/s]", "1/s", G::Mechanics, 0.0f, 400.0f, "%.0f", false, nullptr,
     BIO_REAL(w.softTuning().bendRate)},
    {"xpbd.area", "Área [1/s]", "1/s", G::Mechanics, 10.0f, 3000.0f, "%.0f", false, nullptr,
     BIO_REAL(w.softTuning().areaRate)},
    {"xpbd.aristas", "Aristas [1/s]", "1/s", G::Mechanics, 10.0f, 10000.0f, "%.0f", true, nullptr,
     BIO_REAL(w.softTuning().edgeRate)},
    {"xpbd.iteraciones", "Iteraciones por subpaso", "", G::Mechanics, 1.0f, 10.0f, "%.0f", false, nullptr,
     BIO_INT(w.softTuning().iterations)},
    {"xpbd.pasadas_contacto", "Pasadas de contacto", "", G::Mechanics, 1.0f, 10.0f, "%.0f", false, nullptr,
     BIO_INT(w.softTuning().contactPasses)},
    {"lod.cada", "LOD: sub-pasos lejanos (1 de cada N)", "", G::Engine, 1.0f, 10.0f, "%.0f", false, nullptr,
     BIO_INT(w.lodStride)},
    // --- Metabolismo (constantes fisiológicas: se exportan, sin control)
    {"metabolismo.mantenimiento", "Mantenimiento", "1/h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().maintenancePerHour)},
    {"metabolismo.costo_nado", "Costo de nadar", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().motilityCost)},
    {"metabolismo.crecimiento_maximo", "Crecimiento máximo", "1/h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().growthPerHourMax)},
    {"metabolismo.rendimiento", "Rendimiento", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().growthYield)},
    {"metabolismo.reserva", "Reserva sin invertir", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().reserveBuffer)},
    {"metabolismo.o2_media_saturacion", "O2: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().o2HalfSaturation)},
    {"metabolismo.penalidad_fermentacion", "Penalidad de la fermentación", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().fermentationPenalty)},
    {"metabolismo.fotosintesis_maxima", "Fotosíntesis máxima", "1/h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().photosynthesisPerHour)},
    {"metabolismo.exudacion", "Exudación", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().exudation)},
    {"metabolismo.division", "Citocinesis", "h", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().divisionHours)},
    {"metabolismo.costo_division", "Costo de la división", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().npcDivisionCost)},
    {"metabolismo.costo_division_controlado", "Costo de la división (ser controlado)", "", G::Metabolism, 0, 0, "%g",
     false, nullptr, BIO_REAL(w.metabolismParams().playerDivisionCost)},
    {"metabolismo.fraccion_muerte", "Fracción de biomasa para morir", "", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().deathFraction)},
    {"metabolismo.cuota_maxima", "Cuota máxima (Droop)", "× cuota mínima", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().luxuryQuota)},
    {"metabolismo.cuota_p", "Cuota de P", "pmol P/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().phosphorusRatio)},
    {"metabolismo.p_media_saturacion", "P: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().phosphorusHalfSaturation)},
    {"metabolismo.cuota_n", "Cuota de N", "pmol N/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().nitrogenRatio)},
    {"metabolismo.n_media_saturacion", "N: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().nitrogenHalfSaturation)},
    {"metabolismo.cuota_si", "Cuota de Si", "pmol Si/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().siliconRatio)},
    {"metabolismo.cuota_fe", "Cuota de Fe", "pmol Fe/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().ironRatio)},
    {"metabolismo.cuota_b12", "Cuota de B12", "pmol/pmol", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().vitaminRatio)},
    {"metabolismo.si_media_saturacion", "Si: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().siliconHalfSaturation)},
    {"metabolismo.fe_media_saturacion", "Fe: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
     BIO_REAL(w.metabolismParams().ironHalfSaturation)},
    {"metabolismo.b12_media_saturacion", "B12: media saturación", "µM", G::Metabolism, 0, 0, "%g", false, nullptr,
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
    return nullptr;
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
    }
    return "";
}
