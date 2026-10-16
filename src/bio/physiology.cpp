#include "bio/physiology.hpp"

#include "core/paths.hpp"

#include <toml++/toml.hpp>

#include <cmath>
#include <atomic>
#include <mutex>

namespace {

SpeciesPhysiology gTable[kMaxSpecies];
std::vector<std::string> gWarnings;
std::atomic<bool> gLoaded{false}; // hay una tabla cargada (la de fábrica o una explícita)
std::mutex gLoadMutex;

constexpr float kReferenceTemperature = 20.0f; // [°C] la de World::bioScale
constexpr float kQ10 = 2.0f;

// Tasa en la unidad publicada → 1/h a la temperatura de la medición.
bool toPerHour(float v, const std::string& unit, float& out) {
    if (unit == "1/h")
        out = v;
    else if (unit == "1/dia")
        out = v / 24.0f;
    else if (unit == "h") // tiempo de duplicación
        out = v > 0.0f ? std::log(2.0f) / v : 0.0f;
    else if (unit == "dia")
        out = v > 0.0f ? std::log(2.0f) / (24.0f * v) : 0.0f;
    else
        return false;
    return true;
}

SourcedValue readRate(const toml::table& t, const std::string& species, const char* key) {
    SourcedValue s;
    const toml::table* e = t[key].as_table();
    if (!e)
        return s;
    const auto v = (*e)["valor"].value<double>();
    const auto unit = (*e)["unidad"].value<std::string>();
    if (!v || !unit) {
        gWarnings.push_back(species + "." + key + ": falta valor o unidad");
        return s;
    }
    s.published = static_cast<float>(*v);
    s.unit = *unit;
    s.temperature = static_cast<float>((*e)["temperatura"].value_or(static_cast<double>(kReferenceTemperature)));
    s.source = (*e)["fuente"].value_or(std::string());
    s.estimate = (*e)["estimacion"].value_or(false);
    float perHour = 0.0f;
    if (!toPerHour(s.published, s.unit, perHour)) {
        gWarnings.push_back(species + "." + key + ": unidad desconocida '" + s.unit + "'");
        return s;
    }
    // Q10: a la temperatura de referencia del modelo.
    s.value = perHour * std::pow(kQ10, (kReferenceTemperature - s.temperature) / 10.0f);
    s.present = s.value > 0.0f;
    if (s.source.empty())
        gWarnings.push_back(species + "." + key + ": sin fuente");
    return s;
}

} // namespace

namespace {

// Concentración (media saturación) en µM: sin conversión de temperatura.
SourcedValue readConcentration(const toml::table& t, const std::string& species, const char* key) {
    SourcedValue s;
    const toml::table* e = t[key].as_table();
    if (!e)
        return s;
    const auto v = (*e)["valor"].value<double>();
    const auto unit = (*e)["unidad"].value<std::string>();
    if (!v || !unit || *unit != "µM") {
        gWarnings.push_back(species + "." + key + ": falta valor o la unidad no es µM");
        return s;
    }
    s.published = s.value = static_cast<float>(*v);
    s.unit = *unit;
    s.temperature = static_cast<float>((*e)["temperatura"].value_or(20.0));
    s.source = (*e)["fuente"].value_or(std::string());
    s.estimate = (*e)["estimacion"].value_or(false);
    s.present = s.value > 0.0f;
    if (s.source.empty())
        gWarnings.push_back(species + "." + key + ": sin fuente");
    return s;
}

// Con gLoadMutex tomado. Marca la tabla como cargada al final (también si falla: no se reintenta
// en cada consulta), así ningún hilo la lee a medio llenar.
bool loadLocked(const std::string& path, std::string* error) {
    for (SpeciesPhysiology& p : gTable)
        p = SpeciesPhysiology{};
    gWarnings.clear();
    toml::table doc;
    try {
        doc = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = std::string(e.description());
        gLoaded.store(true, std::memory_order_release);
        return false;
    }
    for (const auto& [name, node] : doc) {
        const toml::table* t = node.as_table();
        if (!t)
            continue; // "version" y otras claves sueltas
        int id = -1;
        for (int i = 0; i < speciesCount(); ++i)
            if (std::string_view(name.str()) == speciesInfo(static_cast<SpeciesId>(i)).name)
                id = i;
        if (id < 0) {
            gWarnings.push_back("especie desconocida: " + std::string(name.str()));
            continue;
        }
        gTable[id].growthMax = readRate(*t, std::string(name.str()), "crecimiento_maximo");
        gTable[id].phosphorusHalfSaturation = readConcentration(*t, std::string(name.str()), "p_media_saturacion");
        gTable[id].siliconHalfSaturation = readConcentration(*t, std::string(name.str()), "si_media_saturacion");
    }
    gLoaded.store(true, std::memory_order_release);
    return true;
}

} // namespace

bool loadSpeciesPhysiology(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    return loadLocked(path, error);
}

const SpeciesPhysiology& speciesPhysiology(SpeciesId id) {
    if (!gLoaded.load(std::memory_order_acquire)) {
        // La primera consulta carga la tabla de fábrica (los loops paralelos esperan acá).
        std::lock_guard<std::mutex> lock(gLoadMutex);
        if (!gLoaded.load(std::memory_order_relaxed))
            loadLocked(assetPath("species/fisiologia.toml"), nullptr);
    }
    return gTable[static_cast<int>(id)];
}

const std::vector<std::string>& physiologyWarnings() { return gWarnings; }
