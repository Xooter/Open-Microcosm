#include "game/legacy_names.hpp"

#include <utility>

namespace {

// Generadas de tools/content_names.py (la tabla de la migración del contenido).
constexpr std::pair<std::string_view, std::string_view> kLegacyNames[] = {
    {"agregar", "add"}, {"bacterias", "bacteria"}, {"ciclo", "cycle"}, {"clave", "key"},
    {"concentracion", "concentration"}, {"cultivo", "culture"}, {"cultivo_version", "culture_version"},
    {"dilucion", "dilution"}, {"entrada", "receive"}, {"escena", "scene"}, {"evento", "event"},
    {"eventos", "events"}, {"experimento", "experiment"}, {"fagos", "phages"}, {"fosfato", "phosphate"},
    {"hierro", "iron"}, {"hilos", "threads"}, {"horas", "hours"}, {"inocular", "inoculate"},
    {"inocular_densidad", "inoculate_density"}, {"luz", "light"}, {"medio", "medium"},
    {"medio_que_entra", "inflow_medium"}, {"muestra", "sample"}, {"muestreo_h", "sample_h"},
    {"nitrogeno", "nitrogen"}, {"nombre", "name"}, {"organica", "organic"}, {"parametro", "parameter"},
    {"placa", "plate"}, {"programa", "program"}, {"quimiostato", "chemostat"}, {"rampa", "ramp"},
    {"reactivo", "reagent"}, {"recipiente", "vessel"}, {"replicas", "replicates"}, {"salida", "withdraw"},
    {"seno", "sine"}, {"silice", "silicate"}, {"soluto", "solute"}, {"temperatura", "temperature"},
    {"tipo", "kind"}, {"valores", "values"},
};

constexpr std::pair<std::string_view, std::string_view> kLegacyParameters[] = {
    {"medio.alcalinidad", "medium.alkalinity"}, {"medio.descomposicion", "medium.decomposition"},
    {"medio.consumo_nutrientes", "medium.nutrient_uptake"},
    {"medio.detritos_entrantes", "medium.detritus_inflow"},
    {"medio.aporte_organico", "medium.organic_inflow"}, {"medio.polvo", "medium.dust"},
    {"medio.perfusion", "medium.perfusion"}, {"medio.b12_fuente", "medium.b12_source"},
    {"medio.b12_degradacion", "medium.b12_decay"},
    {"medio.disolucion_frustulas", "medium.frustule_dissolution"},
    {"bacterias.quimiotaxis", "bacteria.chemotaxis"},
    {"bacterias.mantencion_ayuno", "bacteria.starvation_maintenance"},
    {"bacterias.adhesion", "bacteria.adhesion"}, {"bacterias.desprendimiento", "bacteria.detachment"},
    {"medio.fagos_inactivacion", "medium.phage_inactivation"},
    {"medio.toxinas_degradacion", "medium.toxin_decay"}, {"medio.ahl_vida_media", "medium.ahl_half_life"},
    {"hongos.absorcion", "fungi.absorption"}, {"hongos.mantenimiento", "fungi.maintenance"},
    {"hongos.hambre_a_esporas", "fungi.starvation_to_spores"}, {"hongos.exoenzimas", "fungi.exoenzymes"},
    {"hongos.allomyces_despierta", "fungi.allomyces_wakeup"}, {"hongos.liquen_azucar", "fungi.lichen_sugar"},
    {"hongos.liquen_fosforo", "fungi.lichen_phosphorus"}, {"physarum.frente", "physarum.front"},
    {"physarum.adaptacion", "physarum.adaptation"}, {"physarum.gamma", "physarum.gamma"},
    {"physarum.cambio", "physarum.turnover"}, {"physarum.evita_baba", "physarum.slime_avoidance"},
    {"physarum.evita_luz", "physarum.light_avoidance"}, {"physarum.retraccion", "physarum.retraction"},
    {"physarum.hambre_a_esclerocio", "physarum.starvation_to_sclerotium"},
    {"ambiente.temperatura", "environment.temperature"}, {"ambiente.salinidad", "environment.salinity"},
    {"ambiente.luz", "environment.light"}, {"ambiente.osmolitos", "environment.osmolytes"},
    {"tiempo.escala_biologica", "time.bio_scale"}, {"agua.espesor_pelicula", "water.film_thickness"},
    {"xpbd.subpasos", "xpbd.substeps"}, {"xpbd.memoria_forma", "xpbd.shape_memory"},
    {"xpbd.flexion", "xpbd.bending"}, {"xpbd.area", "xpbd.area"}, {"xpbd.aristas", "xpbd.edges"},
    {"xpbd.iteraciones", "xpbd.iterations"}, {"xpbd.pasadas_contacto", "xpbd.contact_passes"},
    {"lod.cada", "lod.stride"}, {"metabolismo.mantenimiento", "metabolism.maintenance"},
    {"metabolismo.costo_nado", "metabolism.swim_cost"},
    {"metabolismo.crecimiento_maximo", "metabolism.max_growth"},
    {"metabolismo.rendimiento", "metabolism.yield"}, {"metabolismo.reserva", "metabolism.storage"},
    {"metabolismo.o2_media_saturacion", "metabolism.o2_half_saturation"},
    {"metabolismo.penalidad_fermentacion", "metabolism.fermentation_penalty"},
    {"metabolismo.fotosintesis_maxima", "metabolism.max_photosynthesis"},
    {"metabolismo.exudacion", "metabolism.exudation"}, {"metabolismo.division", "metabolism.division"},
    {"metabolismo.costo_division", "metabolism.division_cost"},
    {"metabolismo.costo_division_controlado", "metabolism.controlled_division_cost"},
    {"metabolismo.fraccion_muerte", "metabolism.death_fraction"},
    {"metabolismo.cuota_maxima", "metabolism.max_quota"}, {"metabolismo.cuota_p", "metabolism.p_quota"},
    {"metabolismo.p_media_saturacion", "metabolism.p_half_saturation"},
    {"metabolismo.cuota_n", "metabolism.n_quota"},
    {"metabolismo.n_media_saturacion", "metabolism.n_half_saturation"},
    {"metabolismo.cuota_si", "metabolism.si_quota"}, {"metabolismo.cuota_fe", "metabolism.fe_quota"},
    {"metabolismo.cuota_b12", "metabolism.b12_quota"},
    {"metabolismo.si_media_saturacion", "metabolism.si_half_saturation"},
    {"metabolismo.fe_media_saturacion", "metabolism.fe_half_saturation"},
    {"metabolismo.b12_media_saturacion", "metabolism.b12_half_saturation"},
};

} // namespace

std::string_view currentName(std::string_view name) {
    for (const auto& [old, now] : kLegacyNames)
        if (name == old)
            return now;
    return name;
}

std::string currentTarget(std::string_view target) {
    const std::size_t colon = target.find(':');
    if (colon == std::string_view::npos) {
        const std::string_view param = currentParameterKey(target);
        return std::string(param.empty() ? currentName(target) : param);
    }
    // El argumento es un soluto, un id (los registros resuelven los viejos) o una clave de parámetro.
    const std::string_view arg = target.substr(colon + 1);
    const std::string_view param = currentParameterKey(arg);
    return std::string(currentName(target.substr(0, colon))) + ':' +
           std::string(param.empty() ? currentName(arg) : param);
}

std::string_view currentParameterKey(std::string_view key) {
    for (const auto& [old, now] : kLegacyParameters)
        if (key == old)
            return now;
    return {};
}
