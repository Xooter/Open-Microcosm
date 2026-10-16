#include "game/prepared.hpp"

#include "core/paths.hpp"
#include "core/toml_reader.hpp"
#include "core/units.hpp"
#include "game/medium_toml.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>

namespace {

struct Registry {
    std::deque<CultureMedium> media;
    std::deque<Vessel> vessels;
    std::deque<PreparedPlate> plates;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    std::atomic<bool> ready{false}; // ya cargado: se lee sin el candado
};
Registry& registry() {
    static Registry r;
    return r;
}

void ensureBase();

template <typename T> bool duplicate(const std::deque<T>& list, const std::string& key) {
    for (const T& e : list)
        if (e.key == key)
            return true;
    return false;
}

constexpr const char* kMediumKeys[] = {"id", "nombre", "descripcion", "fuente", "composicion"};
constexpr const char* kVesselKeys[] = {"id", "nombre", "area_cm2", "volumen_ml", "fuente"};
constexpr const char* kPlateKeys[] = {"id",        "nombre",   "descripcion", "tipo", "medio",
                                      "recipiente", "campo_um", "ambiente",    "siembra"};
constexpr const char* kAmbientKeys[] = {"temperatura", "co2", "luz"};
constexpr const char* kSeedKeys[] = {"ser", "densidad_cm2"};
constexpr std::pair<const char*, PlateKind> kKinds[] = {{"cultivo", PlateKind::CellCulture}};

// Id y nombre obligatorios; el resto, con los errores en el lector.
bool head(toml_io::Reader& r, const std::string& source, const char* what, std::string& key, std::string& name,
          std::vector<std::string>& warnings) {
    r.text("id", key);
    r.text("nombre", name);
    if (key.empty() || name.empty()) {
        warnings.push_back(source + ": un [[" + what + "]] sin 'id' o sin 'nombre': se saltea");
        return false;
    }
    r.where = source + " (" + key + ")";
    return true;
}

bool finish(toml_io::Reader& r, std::vector<std::string>& warnings, const char* what) {
    if (r.errors.empty())
        return true;
    warnings.insert(warnings.end(), r.errors.begin(), r.errors.end());
    warnings.push_back(r.where + ": " + what + " no se carga");
    return false;
}

void loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    Registry& reg = registry();
    auto each = [&](const char* key, auto&& fn) {
        const toml::array* list = doc.get(key) ? doc.get(key)->as_array() : nullptr;
        if (!list)
            return;
        for (const toml::node& n : *list)
            if (const toml::table* t = n.as_table())
                fn(*t);
    };
    each("medio", [&](const toml::table& t) {
        CultureMedium m;
        toml_io::Reader r{t, source, {}};
        if (!head(r, source, "medio", m.key, m.name, reg.warnings))
            return;
        toml_io::warnUnknown(t, kMediumKeys, r.where, reg.warnings);
        r.text("descripcion", m.description);
        r.text("fuente", m.reference);
        if (m.reference.empty())
            r.errors.push_back(r.where + ": un medio necesita su 'fuente' (la composición publicada)");
        if (const toml::table* c = r.table("composicion"))
            readMedium(*c, r.where + " [composicion]", m.composition, r.errors, reg.warnings);
        m.pack = pack;
        if (!finish(r, reg.warnings, "el medio"))
            return;
        if (duplicate(reg.media, m.key)) {
            reg.warnings.push_back(source + ": el medio '" + m.key + "' ya existe: se saltea");
            return;
        }
        m.source = toml_io::snippet("medio", t);
        reg.media.push_back(std::move(m));
    });
    each("recipiente", [&](const toml::table& t) {
        Vessel v;
        toml_io::Reader r{t, source, {}};
        if (!head(r, source, "recipiente", v.key, v.name, reg.warnings))
            return;
        toml_io::warnUnknown(t, kVesselKeys, r.where, reg.warnings);
        r.count("area_cm2", v.areaCm2);
        r.count("volumen_ml", v.volumeMl);
        if (v.areaCm2 <= 0.0f || v.volumeMl <= 0.0f)
            r.errors.push_back(r.where + ": 'area_cm2' y 'volumen_ml' tienen que ser positivos");
        v.pack = pack;
        if (!finish(r, reg.warnings, "el recipiente"))
            return;
        if (duplicate(reg.vessels, v.key)) {
            reg.warnings.push_back(source + ": el recipiente '" + v.key + "' ya existe: se saltea");
            return;
        }
        v.source = toml_io::snippet("recipiente", t);
        reg.vessels.push_back(std::move(v));
    });
    each("placa", [&](const toml::table& t) {
        PreparedPlate p;
        toml_io::Reader r{t, source, {}};
        if (!head(r, source, "placa", p.key, p.name, reg.warnings))
            return;
        toml_io::warnUnknown(t, kPlateKeys, r.where, reg.warnings);
        r.text("descripcion", p.description);
        r.choice("tipo", kKinds, p.kind, true);
        r.text("medio", p.medium);
        r.text("recipiente", p.vessel);
        // Las referencias, contra lo ya cargado (los paquetes cargan medios y recipientes antes).
        if (!findCultureMedium(p.medium))
            r.errors.push_back(r.where + ": 'medio' tiene que ser el id de un medio cargado ('" + p.medium + "')");
        if (!findVessel(p.vessel))
            r.errors.push_back(r.where + ": 'recipiente' tiene que ser el id de un recipiente cargado ('" + p.vessel + "')");
        r.count("campo_um", p.fieldUm);
        if (p.fieldUm < 256.0f || p.fieldUm > 8192.0f)
            r.errors.push_back(r.where + ": 'campo_um' va de 256 a 8192 µm");
        if (const toml::table* a = r.table("ambiente")) {
            toml_io::warnUnknown(*a, kAmbientKeys, r.where + " [ambiente]", reg.warnings);
            toml_io::Reader ar{*a, r.where + " [ambiente]", {}};
            ar.num("temperatura", p.temperature);
            ar.count("co2", p.co2);
            ar.count("luz", p.light);
            if (p.co2 > 1.0f || p.light > 1.0f)
                ar.errors.push_back(ar.where + ": 'co2' y 'luz' van de 0 a 1");
            r.errors.insert(r.errors.end(), ar.errors.begin(), ar.errors.end());
        }
        if (const toml::node* s = t.get("siembra")) {
            const toml::array* list = s->as_array();
            if (!list)
                r.errors.push_back(r.where + ": 'siembra' tiene que ser una lista de [[placa.siembra]]");
            for (std::size_t i = 0; list && i < list->size(); ++i) {
                const toml::table* e = (*list)[i].as_table();
                if (!e)
                    continue;
                toml_io::warnUnknown(*e, kSeedKeys, r.where + " [siembra]", reg.warnings);
                toml_io::Reader sr{*e, r.where + " [siembra]", {}};
                std::string being;
                float density = 0.0f;
                sr.text("ser", being);
                sr.count("densidad_cm2", density);
                if (being.empty())
                    sr.errors.push_back(sr.where + ": falta 'ser'");
                r.errors.insert(r.errors.end(), sr.errors.begin(), sr.errors.end());
                p.seeding.emplace_back(being, density);
            }
        }
        p.pack = pack;
        if (!finish(r, reg.warnings, "la placa"))
            return;
        if (duplicate(reg.plates, p.key)) {
            reg.warnings.push_back(source + ": la placa '" + p.key + "' ya existe: se saltea");
            return;
        }
        p.source = toml_io::snippet("placa", t);
        reg.plates.push_back(std::move(p));
    });
}

void ensureBase() {
    Registry& reg = registry();
    if (reg.ready.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::recursive_mutex> lock(reg.mutex);
    if (reg.baseLoaded)
        return;
    reg.baseLoaded = true;
    // En orden de dependencias: las placas nombran medios y recipientes.
    for (const char* file : {"medios.toml", "recipientes.toml", "placas.toml"}) {
        const std::string path = assetPath(std::string("content/base/") + file);
        try {
            loadParsed(toml::parse_file(path), path, "base");
        } catch (const toml::parse_error& e) {
            reg.warnings.push_back("no se pudo cargar " + path + ": " + std::string(e.description()));
        }
    }
    reg.ready.store(true, std::memory_order_release);
}

template <typename T> const T* find(const std::deque<T>& list, std::string_view key) {
    for (const T& e : list)
        if (e.key == key)
            return &e;
    return nullptr;
}

} // namespace

int cultureMediumCount() {
    ensureBase();
    return static_cast<int>(registry().media.size());
}
const CultureMedium* findCultureMedium(std::string_view key) {
    ensureBase();
    return find(registry().media, key);
}
const CultureMedium& cultureMedium(int index) {
    ensureBase();
    return registry().media[static_cast<std::size_t>(index)];
}
int vesselCount() {
    ensureBase();
    return static_cast<int>(registry().vessels.size());
}
const Vessel* findVessel(std::string_view key) {
    ensureBase();
    return find(registry().vessels, key);
}
const Vessel& vessel(int index) {
    ensureBase();
    return registry().vessels[static_cast<std::size_t>(index)];
}
int preparedPlateCount() {
    ensureBase();
    return static_cast<int>(registry().plates.size());
}
const PreparedPlate* findPreparedPlate(std::string_view key) {
    ensureBase();
    return find(registry().plates, key);
}
const PreparedPlate& preparedPlate(int index) {
    ensureBase();
    return registry().plates[static_cast<std::size_t>(index)];
}

bool loadPreparedText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(registry().mutex);
    ensureBase();
    try {
        loadParsed(toml::parse(text, source), source, pack);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = source + ": " + std::string(e.description());
        return false;
    }
    return true;
}

const std::vector<std::string>& preparedLoadWarnings() {
    ensureBase();
    return registry().warnings;
}

WorldConfig makePrepared(const PreparedPlate& plate, std::uint64_t seed) {
    WorldConfig c;
    c.seed = seed;
    if (const CultureMedium* m = findCultureMedium(plate.medium))
        c.medium = m->composition;
    c.medium.temperature = plate.temperature;
    c.medium.co2Atmosphere = plate.co2;
    c.medium.surfaceLight = plate.light;
    // Un campo del fondo de un recipiente grande: periódico (la monocapa sigue más allá) y sin la
    // pared de vidrio de la gota.
    c.dish = false;
    c.cellCulture = plate.kind == PlateKind::CellCulture;
    int cells = 64;
    while (cells < 1024 && static_cast<float>(cells) * c.cellSize < plate.fieldUm)
        cells *= 2;
    c.gridX = c.gridY = cells;
    if (const Vessel* v = findVessel(plate.vessel)) {
        c.depth = v->depthUm();
        // El O₂ entra y el CO₂ sale por la superficie atravesando la capa quieta de medio: el
        // tiempo de equilibrio es el de la difusión, profundidad² / D (unos 50 min en 2,5 mm).
        c.medium.o2AirExchange = units::kDiffusionO2 / (c.depth * c.depth);
    }
    // Estéril: nada vivo, ni detritos, ni sedimento, ni polvo, ni fagos.
    c.bacteria = 0;
    c.bdellovibrioFraction = 0.0f;
    c.detritus = 0;
    c.cyanoFilaments = c.oscillatoriaFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.allomyces = c.lichens = c.physarum = 0;
    c.patchiness = 0.0f;
    c.sediment = {};
    for (float& w : c.speciesWeight)
        w = 0.0f;
    c.medium.dustRate = 0.0f;
    c.medium.detritusSpawnRate = 0.0f;
    c.medium.organicInflow = 0.0f;
    c.medium.phage = 0.0f;
    // Siembra inicial: células por cm² del campo.
    const float areaCm2 = static_cast<float>(cells) * c.cellSize * static_cast<float>(cells) * c.cellSize * 1e-8f;
    for (const auto& [key, density] : plate.seeding)
        if (const auto id = findSpecies(key))
            c.seeding.emplace_back(*id, static_cast<int>(std::round(density * areaCm2)));
    return c;
}
