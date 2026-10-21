#include "bio/physiology.hpp"

#include "core/pack_files.hpp"
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
    else if (unit == "1/day")
        out = v / 24.0f;
    else if (unit == "h") // tiempo de duplicación
        out = v > 0.0f ? std::log(2.0f) / v : 0.0f;
    else if (unit == "day")
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
    const auto v = (*e)["value"].value<double>();
    const auto unit = (*e)["unit"].value<std::string>();
    if (!v || !unit) {
        gWarnings.push_back(species + "." + key + ": falta valor o unidad");
        return s;
    }
    s.published = static_cast<float>(*v);
    s.unit = *unit;
    s.temperature = static_cast<float>((*e)["temperature"].value_or(static_cast<double>(kReferenceTemperature)));
    s.source = (*e)["source"].value_or(std::string());
    s.estimate = (*e)["estimate"].value_or(false);
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

enum class Kind { Speed, Time, Density, Flux, Concentration, Ratio, Ingestion, Clearance, SwimSpeed, Frequency, Length };

// Rapidez [µm/h], tiempo [h] o densidad [1/cm²] en la unidad publicada; las dos primeras,
// llevadas a 20 °C con Q10 (una rapidez escala como una tasa; un tiempo, al revés).
SourcedValue readScaled(const toml::table& t, const std::string& species, const char* key, Kind kind) {
    SourcedValue s;
    const toml::table* e = t[key].as_table();
    if (!e)
        return s;
    const auto v = (*e)["value"].value<double>();
    const auto unit = (*e)["unit"].value<std::string>();
    if (!v || !unit) {
        gWarnings.push_back(species + "." + key + ": falta valor o unidad");
        return s;
    }
    s.published = static_cast<float>(*v);
    s.unit = *unit;
    s.temperature = static_cast<float>((*e)["temperature"].value_or(static_cast<double>(kReferenceTemperature)));
    s.source = (*e)["source"].value_or(std::string());
    s.estimate = (*e)["estimate"].value_or(false);
    s.q10 = static_cast<float>((*e)["q10"].value_or(0.0));
    float factor = 0.0f;
    if (kind == Kind::SwimSpeed) // física con su Q10 medido: queda a la temperatura de la medición
        factor = s.unit == "um/s" ? 1.0f : 0.0f;
    else if (kind == Kind::Frequency)
        factor = s.unit == "Hz" ? 1.0f : 0.0f;
    else if (kind == Kind::Length)
        factor = s.unit == "um" || s.unit == "µm" ? 1.0f : 0.0f;
    else if (kind == Kind::Speed)
        factor = s.unit == "um/h" ? 1.0f : s.unit == "um/min" ? 60.0f : 0.0f;
    else if (kind == Kind::Time)
        factor = s.unit == "h" ? 1.0f : s.unit == "min" ? 1.0f / 60.0f : s.unit == "day" ? 24.0f : 0.0f;
    else if (kind == Kind::Density)
        factor = s.unit == "1/cm2" ? 1.0f : 0.0f;
    else if (kind == Kind::Flux) // por célula → fmol/h (1 amol/s = 3,6 fmol/h)
        factor = s.unit == "fmol/h" ? 1.0f : s.unit == "amol/s" ? 3.6f : s.unit == "pmol/h" ? 1000.0f : 0.0f;
    else if (kind == Kind::Ingestion) // presas por hora
        factor = s.unit == "bacteria/h" ? 1.0f : s.unit == "bacteria/min" ? 60.0f : 0.0f;
    else if (kind == Kind::Clearance) // → µm³/h
        factor = s.unit == "nL/h" ? 1e6f : s.unit == "uL/h" || s.unit == "µL/h" ? 1e9f : s.unit == "mL/h" ? 1e12f : 0.0f;
    else if (kind == Kind::Concentration)
        factor = s.unit == "uM" || s.unit == "µM" ? 1.0f : s.unit == "mM" ? 1000.0f : 0.0f;
    else
        factor = s.unit == "mol/mol" ? 1.0f : 0.0f;
    if (factor == 0.0f) {
        gWarnings.push_back(species + "." + key + ": unidad desconocida '" + s.unit + "'");
        return s;
    }
    s.value = s.published * factor;
    if (kind == Kind::Speed || kind == Kind::Flux || kind == Kind::Ingestion || kind == Kind::Clearance)
        s.value *= std::pow(kQ10, (kReferenceTemperature - s.temperature) / 10.0f);
    else if (kind == Kind::Time)
        s.value *= std::pow(kQ10, (s.temperature - kReferenceTemperature) / 10.0f);
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
    const auto v = (*e)["value"].value<double>();
    const auto unit = (*e)["unit"].value<std::string>();
    if (!v || !unit || *unit != "µM") {
        gWarnings.push_back(species + "." + key + ": falta valor o la unidad no es µM");
        return s;
    }
    s.published = s.value = static_cast<float>(*v);
    s.unit = *unit;
    s.temperature = static_cast<float>((*e)["temperature"].value_or(20.0));
    s.source = (*e)["source"].value_or(std::string());
    s.estimate = (*e)["estimate"].value_or(false);
    s.present = s.value > 0.0f;
    if (s.source.empty())
        gWarnings.push_back(species + "." + key + ": sin fuente");
    return s;
}

// Las tablas [id_del_ser.parametro] de un documento (o de la tabla [physiology] de un .biopack).
void loadTables(const toml::table& doc) {
    for (const auto& [name, node] : doc) {
        const toml::table* t = node.as_table();
        if (!t)
            continue; // "version" y otras claves sueltas
        // Por el id del ser (beings/<id>.toml de algún paquete cargado).
        const std::optional<SpeciesId> found = findSpecies(name.str());
        const int id = found ? static_cast<int>(*found) : -1;
        if (id < 0) {
            gWarnings.push_back("especie desconocida: " + std::string(name.str()));
            continue;
        }
        gTable[id].growthMax = readRate(*t, std::string(name.str()), "max_growth");
        gTable[id].phosphorusHalfSaturation = readConcentration(*t, std::string(name.str()), "p_half_saturation");
        gTable[id].siliconHalfSaturation = readConcentration(*t, std::string(name.str()), "si_half_saturation");
        const std::string sp(name.str());
        SpeciesPhysiology& ph = gTable[id];
        ph.migrationSpeed = readScaled(*t, sp, "migration_speed", Kind::Speed);
        ph.persistence = readScaled(*t, sp, "persistence", Kind::Time);
        ph.phaseG1 = readScaled(*t, sp, "phase_g1", Kind::Time);
        ph.phaseS = readScaled(*t, sp, "phase_s", Kind::Time);
        ph.phaseG2 = readScaled(*t, sp, "phase_g2", Kind::Time);
        ph.phaseM = readScaled(*t, sp, "phase_m", Kind::Time);
        ph.lifespan = readScaled(*t, sp, "lifespan", Kind::Time);
        ph.motilityHalfLife = readScaled(*t, sp, "motility_half_life", Kind::Time);
        ph.capacitationTime = readScaled(*t, sp, "capacitation_time", Kind::Time);
        ph.confluence = readScaled(*t, sp, "confluence", Kind::Density);
        ph.glucoseVmax = readScaled(*t, sp, "glucose_vmax", Kind::Flux);
        ph.glucoseKm = readScaled(*t, sp, "glucose_km", Kind::Concentration);
        ph.glutamineVmax = readScaled(*t, sp, "glutamine_vmax", Kind::Flux);
        ph.glutamineKm = readScaled(*t, sp, "glutamine_km", Kind::Concentration);
        ph.lactatePerGlucose = readScaled(*t, sp, "lactate_per_glucose", Kind::Ratio);
        ph.oxygenUptake = readScaled(*t, sp, "o2_uptake", Kind::Flux);
        ph.maxIngestion = readScaled(*t, sp, "max_ingestion", Kind::Ingestion);
        ph.clearance = readScaled(*t, sp, "clearance", Kind::Clearance);
        ph.swimSpeed = readScaled(*t, sp, "swim_speed", Kind::SwimSpeed);
        ph.beatFrequency = readScaled(*t, sp, "beat_frequency", Kind::Frequency);
        ph.lateralAmplitude = readScaled(*t, sp, "lateral_amplitude", Kind::Length);
        for (const SourcedValue* v : {&ph.swimSpeed, &ph.beatFrequency, &ph.lateralAmplitude})
            if (v->present && v->q10 <= 0.0f)
                gWarnings.push_back(sp + ": el nado y el batido llevan su 'q10' medido");
        gSource[id] = toml_io::tableSnippet("physiology", name.str(), *t);
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
    // Un archivo de un ser trae sus tablas en [physiology.<id>.<clave>]; uno suelto, en
    // [<id>.<clave>].
    if (const toml::table* root = doc["physiology"].as_table())
        loadTables(*root);
    else if (!doc.contains("being"))
        loadTables(doc);
    gLoaded.store(true, std::memory_order_release);
    return true;
}

// La de fábrica: la de cada ser está en su archivo (beings/<id>.toml).
bool loadBaseLocked(std::string* error) {
    bool ok = true;
    for (SpeciesPhysiology& p : gTable)
        p = SpeciesPhysiology{};
    for (std::string& src : gSource)
        src.clear();
    gWarnings.clear();
    for (const auto& path : packFolderFiles(basePackDir(), "beings"))
        ok = loadLocked(path.string(), error, false) && ok;
    gLoaded.store(true, std::memory_order_release);
    return ok;
}

} // namespace

bool loadSpeciesPhysiology(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    return loadLocked(path, error);
}

bool loadBasePhysiology(std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    return loadBaseLocked(error);
}

bool addSpeciesPhysiology(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    if (!gLoaded.load(std::memory_order_relaxed))
        loadBaseLocked(nullptr);
    return loadLocked(path, error, false);
}

const SpeciesPhysiology& speciesPhysiology(SpeciesId id) {
    if (!gLoaded.load(std::memory_order_acquire)) {
        // La primera consulta carga la tabla de fábrica (los loops paralelos esperan acá).
        std::lock_guard<std::mutex> lock(gLoadMutex);
        if (!gLoaded.load(std::memory_order_relaxed))
            loadBaseLocked(nullptr);
    }
    return gTable[static_cast<int>(id)];
}

bool addSpeciesPhysiologyText(const std::string& text, const std::string& source, std::string* error) {
    std::lock_guard<std::mutex> lock(gLoadMutex);
    if (!gLoaded.load(std::memory_order_relaxed))
        loadBaseLocked(nullptr);
    toml::table doc;
    try {
        doc = toml::parse(text, source);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = source + ": " + std::string(e.description());
        return false;
    }
    const toml::table* root = doc.get("physiology") ? doc.get("physiology")->as_table() : nullptr;
    loadTables(root ? *root : doc);
    return true;
}

std::string physiologySource(SpeciesId id) {
    speciesPhysiology(id); // carga la de fábrica si hace falta
    std::lock_guard<std::mutex> lock(gLoadMutex);
    return gSource[static_cast<int>(id)];
}

const std::vector<std::string>& physiologyWarnings() { return gWarnings; }

const std::vector<PhysiologyKey>& physiologyKeys() {
    using P = SpeciesPhysiology;
    static const std::vector<PhysiologyKey> keys = {
        {"max_growth", &P::growthMax, "1/h", "Crecimiento maximo"},
        {"p_half_saturation", &P::phosphorusHalfSaturation, "uM", "Media saturacion de P"},
        {"si_half_saturation", &P::siliconHalfSaturation, "uM", "Media saturacion de Si"},
        {"migration_speed", &P::migrationSpeed, "um/h", "Rapidez de migracion"},
        {"persistence", &P::persistence, "h", "Persistencia de la direccion"},
        {"phase_g1", &P::phaseG1, "h", "Fase G1"},
        {"phase_s", &P::phaseS, "h", "Fase S"},
        {"phase_g2", &P::phaseG2, "h", "Fase G2"},
        {"phase_m", &P::phaseM, "h", "Mitosis"},
        {"lifespan", &P::lifespan, "h", "Vida media"},
        {"motility_half_life", &P::motilityHalfLife, "h", "Vida media de la motilidad"},
        {"capacitation_time", &P::capacitationTime, "h", "Tiempo de capacitacion"},
        {"confluence", &P::confluence, "1/cm2", "Densidad de confluencia"},
        {"glucose_vmax", &P::glucoseVmax, "fmol/h", "Captacion maxima de glucosa"},
        {"glucose_km", &P::glucoseKm, "uM", "Km de la glucosa"},
        {"glutamine_vmax", &P::glutamineVmax, "fmol/h", "Captacion maxima de glutamina"},
        {"glutamine_km", &P::glutamineKm, "uM", "Km de la glutamina"},
        {"lactate_per_glucose", &P::lactatePerGlucose, "mol/mol", "Lactato por glucosa (Warburg)"},
        {"o2_uptake", &P::oxygenUptake, "fmol/h", "Consumo de O2"},
        {"max_ingestion", &P::maxIngestion, "bacteria/h", "Ingestion maxima de bacterias"},
        {"clearance", &P::clearance, "um3/h", "Aclaramiento (agua filtrada)"},
        {"swim_speed", &P::swimSpeed, "um/s", "Rapidez de nado (VAP)"},
        {"beat_frequency", &P::beatFrequency, "Hz", "Frecuencia del batido (BCF)"},
        {"lateral_amplitude", &P::lateralAmplitude, "um", "Desplazamiento lateral de la cabeza (ALH)"},
    };
    return keys;
}

float atTemperature(const SourcedValue& v, float t) {
    if (!v.present)
        return 0.0f;
    return v.q10 > 0.0f ? v.value * std::pow(v.q10, (t - v.temperature) / 10.0f) : v.value;
}
