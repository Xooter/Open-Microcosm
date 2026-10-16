#include "bio/physiology.hpp"

#include "core/paths.hpp"
#include "core/toml_reader.hpp"

#include <toml++/toml.hpp>

#include <cmath>
#include <atomic>
#include <mutex>
#include <optional>

namespace {

SpeciesPhysiology gTable[kMaxSpecies];
std::string gSource[kMaxSpecies]; // la tabla de cada ser como se leyó (para exportarla)
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

// Las tablas [id_del_ser.parametro] de un documento (o de la tabla [fisiologia] de un .biopack).
void loadTables(const toml::table& doc) {
    for (const auto& [name, node] : doc) {
        const toml::table* t = node.as_table();
        if (!t)
            continue; // "version" y otras claves sueltas
        // Por el id del ser (seres.toml de algún paquete cargado).
        const std::optional<SpeciesId> found = findSpecies(name.str());
        const int id = found ? static_cast<int>(*found) : -1;
        if (id < 0) {
            gWarnings.push_back("especie desconocida: " + std::string(name.str()));
            continue;
        }
        gTable[id].growthMax = readRate(*t, std::string(name.str()), "crecimiento_maximo");
        gTable[id].phosphorusHalfSaturation = readConcentration(*t, std::string(name.str()), "p_media_saturacion");
        gTable[id].siliconHalfSaturation = readConcentration(*t, std::string(name.str()), "si_media_saturacion");
        gSource[id] = toml_io::tableSnippet("fisiologia", name.str(), *t);
    }
}

// Con gLoadMutex tomado. Marca la tabla como cargada al final (también si falla: no se reintenta
// en cada consulta), así ningún hilo la lee a medio llenar.
bool loadLocked(const std::string& path, std::string* error, bool reset = true) {
    if (reset) {
        for (SpeciesPhysiology& p : gTable)
            p = SpeciesPhysiology{};
        for (std::string& src : gSource)
            src.clear();
        gWarnings.clear();
    }
    toml::table doc;
    try {
        doc = toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = std::string(e.description());
        gLoaded.store(true, std::memory_order_release);
        return false;
    }
    loadTables(doc);
    gLoaded.store(true, std::memory_order_release);
    return true;
}

} // namespace

bool loadSpeciesPhysiology(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    return loadLocked(path, error);
}

bool addSpeciesPhysiology(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    if (!gLoaded.load(std::memory_order_relaxed))
        loadLocked(assetPath("content/base/fisiologia.toml"), nullptr);
    return loadLocked(path, error, false);
}

const SpeciesPhysiology& speciesPhysiology(SpeciesId id) {
    if (!gLoaded.load(std::memory_order_acquire)) {
        // La primera consulta carga la tabla de fábrica (los loops paralelos esperan acá).
        std::lock_guard<std::mutex> lock(gLoadMutex);
        if (!gLoaded.load(std::memory_order_relaxed))
            loadLocked(assetPath("content/base/fisiologia.toml"), nullptr);
    }
    return gTable[static_cast<int>(id)];
}

bool addSpeciesPhysiologyText(const std::string& text, const std::string& source, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    if (!gLoaded.load(std::memory_order_relaxed))
        loadLocked(assetPath("content/base/fisiologia.toml"), nullptr);
    toml::table doc;
    try {
        doc = toml::parse(text, source);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = source + ": " + std::string(e.description());
        return false;
    }
    const toml::table* root = doc.get("fisiologia") ? doc.get("fisiologia")->as_table() : nullptr;
    loadTables(root ? *root : doc);
    return true;
}

std::string physiologySource(SpeciesId id) {
    speciesPhysiology(id); // carga la de fábrica si hace falta
    std::lock_guard<std::mutex> lock(gLoadMutex);
    return gSource[static_cast<int>(id)];
}

const std::vector<std::string>& physiologyWarnings() { return gWarnings; }
