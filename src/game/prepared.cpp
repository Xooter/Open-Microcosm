#include "game/prepared.hpp"

#include "core/ids.hpp"
#include "core/pack_files.hpp"
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

constexpr const char* kMediumKeys[] = {"id", "legacy_ids", "name", "description", "source", "composition"};
constexpr const char* kVesselKeys[] = {"id", "legacy_ids", "name", "area_cm2", "volume_ml", "source"};
constexpr const char* kPlateKeys[] = {"id", "legacy_ids",         "name",   "description", "kind",   "medium",  "vessel",
                                      "field_um",   "environment", "seeding",     "tissue", "fixed", "stain",
                                      "cell_um",   "erythrocytes_cm2", "platelets_cm2", "bacteria_cm2"};
constexpr const char* kTissueKeys[] = {"arrangement", "content", "being", "length_um", "width_um", "wall_um",
                                       "sap_osmolarity", "inactive", "equilibration_s", "mitotic_phases"};
constexpr std::pair<const char*, TissueLayout> kLayouts[] = {{"rows", TissueLayout::Rows},
                                                             {"polygonal", TissueLayout::Polygonal},
                                                             {"files", TissueLayout::Files},
                                                             {"jigsaw", TissueLayout::Jigsaw}};
constexpr std::pair<const char*, TissueContent> kContents[] = {{"protoplast", TissueContent::Protoplast},
                                                               {"starch", TissueContent::Starch},
                                                               {"mitosis", TissueContent::Mitosis},
                                                               {"stomata", TissueContent::Stomata}};
constexpr const char* kAmbientKeys[] = {"temperature", "co2", "light"};
constexpr const char* kSeedKeys[] = {"being", "density_cm2"};
constexpr std::pair<const char*, PlateKind> kKinds[] = {{"cell_culture", PlateKind::CellCulture},
                                                        {"slide", PlateKind::Slide}};

// Id y nombre obligatorios; el resto, con los errores en el lector.
bool head(toml_io::Reader& r, const std::string& source, const char* what, std::string& key, std::string& name,
          std::vector<std::string>& legacyIds, std::vector<std::string>& warnings) {
    r.text("id", key);
    r.strings("legacy_ids", legacyIds);
    r.text("name", name);
    if (key.empty() || name.empty()) {
        warnings.push_back(source + ": un [[" + what + "]] sin 'id' o sin 'name': se saltea");
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
    each("medium", [&](const toml::table& t) {
        CultureMedium m;
        toml_io::Reader r{t, source, {}};
        if (!head(r, source, "medium", m.key, m.name, m.legacyIds, reg.warnings))
            return;
        toml_io::warnUnknown(t, kMediumKeys, r.where, reg.warnings);
        r.text("description", m.description);
        r.text("source", m.reference);
        if (m.reference.empty())
            r.errors.push_back(r.where + ": un medio necesita su 'source' (la composición publicada)");
        if (const toml::table* c = r.table("composition"))
            readMedium(*c, r.where + " [composition]", m.composition, r.errors, reg.warnings);
        m.pack = pack;
        if (!finish(r, reg.warnings, "el medio"))
            return;
        if (duplicate(reg.media, m.key)) {
            reg.warnings.push_back(source + ": el medio '" + m.key + "' ya existe: se saltea");
            return;
        }
        m.source = toml_io::snippet("medium", t);
        reg.media.push_back(std::move(m));
    });
    each("vessel", [&](const toml::table& t) {
        Vessel v;
        toml_io::Reader r{t, source, {}};
        if (!head(r, source, "vessel", v.key, v.name, v.legacyIds, reg.warnings))
            return;
        toml_io::warnUnknown(t, kVesselKeys, r.where, reg.warnings);
        r.count("area_cm2", v.areaCm2);
        r.count("volume_ml", v.volumeMl);
        if (v.areaCm2 <= 0.0f || v.volumeMl <= 0.0f)
            r.errors.push_back(r.where + ": 'area_cm2' y 'volume_ml' tienen que ser positivos");
        v.pack = pack;
        if (!finish(r, reg.warnings, "el recipiente"))
            return;
        if (duplicate(reg.vessels, v.key)) {
            reg.warnings.push_back(source + ": el recipiente '" + v.key + "' ya existe: se saltea");
            return;
        }
        v.source = toml_io::snippet("vessel", t);
        reg.vessels.push_back(std::move(v));
    });
    each("plate", [&](const toml::table& t) {
        PreparedPlate p;
        toml_io::Reader r{t, source, {}};
        if (!head(r, source, "plate", p.key, p.name, p.legacyIds, reg.warnings))
            return;
        toml_io::warnUnknown(t, kPlateKeys, r.where, reg.warnings);
        r.text("description", p.description);
        r.choice("kind", kKinds, p.kind, true);
        r.text("medium", p.medium);
        r.text("vessel", p.vessel);
        // Las referencias, contra lo ya cargado (los paquetes cargan medios y recipientes antes).
        if (!findCultureMedium(p.medium))
            r.errors.push_back(r.where + ": 'medium' tiene que ser el id de un medio cargado ('" + p.medium + "')");
        if (!findVessel(p.vessel))
            r.errors.push_back(r.where + ": 'vessel' tiene que ser el id de un recipiente cargado ('" + p.vessel + "')");
        r.count("field_um", p.fieldUm);
        if (p.fieldUm < 256.0f || p.fieldUm > 8192.0f)
            r.errors.push_back(r.where + ": 'field_um' va de 256 a 8192 µm");
        if (const toml::table* a = r.table("environment")) {
            toml_io::warnUnknown(*a, kAmbientKeys, r.where + " [environment]", reg.warnings);
            toml_io::Reader ar{*a, r.where + " [environment]", {}};
            ar.num("temperature", p.temperature);
            ar.count("co2", p.co2);
            ar.count("light", p.light);
            if (p.co2 > 1.0f || p.light > 1.0f)
                ar.errors.push_back(ar.where + ": 'co2' y 'light' van de 0 a 1");
            r.errors.insert(r.errors.end(), ar.errors.begin(), ar.errors.end());
        }
        if (const toml::node* s = t.get("seeding")) {
            const toml::array* list = s->as_array();
            if (!list)
                r.errors.push_back(r.where + ": 'seeding' tiene que ser una lista de [[plate.seeding]]");
            for (std::size_t i = 0; list && i < list->size(); ++i) {
                const toml::table* e = (*list)[i].as_table();
                if (!e)
                    continue;
                toml_io::warnUnknown(*e, kSeedKeys, r.where + " [seeding]", reg.warnings);
                toml_io::Reader sr{*e, r.where + " [seeding]", {}};
                std::string being;
                float density = 0.0f;
                sr.text("being", being);
                sr.count("density_cm2", density);
                if (being.empty())
                    sr.errors.push_back(sr.where + ": falta 'being'");
                r.errors.insert(r.errors.end(), sr.errors.begin(), sr.errors.end());
                p.seeding.emplace_back(being, density);
            }
        }
        r.flag("fixed", p.fixed);
        r.text("stain", p.stain);
        r.count("cell_um", p.cellSize);
        r.count("erythrocytes_cm2", p.erythrocytesPerCm2);
        r.count("platelets_cm2", p.plateletsPerCm2);
        r.count("bacteria_cm2", p.bacteriaPerCm2);
        if (p.cellSize < 2.0f || p.cellSize > 16.0f)
            r.errors.push_back(r.where + ": 'cell_um' va de 2 a 16 µm");
        if (const toml::table* t2 = r.table("tissue")) {
            Tissue& ts = p.tissue;
            ts.enabled = true;
            toml_io::warnUnknown(*t2, kTissueKeys, r.where + " [tissue]", reg.warnings);
            toml_io::Reader tr{*t2, r.where + " [tissue]", {}};
            tr.choice("arrangement", kLayouts, ts.layout, true);
            tr.choice("content", kContents, ts.content, true);
            tr.text("being", ts.species);
            tr.count("length_um", ts.cellLength);
            tr.count("width_um", ts.cellWidth);
            tr.count("wall_um", ts.wallThickness);
            tr.count("sap_osmolarity", ts.sapOsmolarity);
            tr.count("inactive", ts.inactive);
            tr.count("equilibration_s", ts.osmoticSeconds);
            if (const toml::node* f = t2->get("mitotic_phases")) {
                const toml::array* a = f->as_array();
                if (!a || a->size() != 4)
                    tr.errors.push_back(tr.where + ": 'mitotic_phases' es [profase, metafase, anafase, telofase] (fracciones)");
                for (std::size_t i = 0; a && i < 4 && i < a->size(); ++i)
                    ts.mitoticFractions[i] = static_cast<float>((*a)[i].value<double>().value_or(0.0));
            }
            if (ts.cellLength <= 0.0f || ts.cellWidth <= 0.0f)
                tr.errors.push_back(tr.where + ": 'length_um' y 'width_um' tienen que ser positivos");
            if (ts.content == TissueContent::Protoplast && !findSpecies(ts.species))
                tr.errors.push_back(tr.where + ": 'being' tiene que ser el id de un ser cargado ('" + ts.species + "')");
            r.errors.insert(r.errors.end(), tr.errors.begin(), tr.errors.end());
        }
        p.pack = pack;
        if (!finish(r, reg.warnings, "la placa"))
            return;
        if (duplicate(reg.plates, p.key)) {
            reg.warnings.push_back(source + ": la placa '" + p.key + "' ya existe: se saltea");
            return;
        }
        p.source = toml_io::snippet("plate", t);
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
    for (const char* folder : {"media", "vessels", "plates"})
        for (const auto& file : packFolderFiles(basePackDir(), folder)) {
            const std::string path = file.string();
            try {
                loadParsed(toml::parse_file(path), path, "base");
            } catch (const toml::parse_error& e) {
                reg.warnings.push_back("no se pudo cargar " + path + ": " + std::string(e.description()));
            }
        }
    reg.ready.store(true, std::memory_order_release);
}

template <typename T> const T* find(const std::deque<T>& list, std::string_view key) {
    const std::ptrdiff_t i = indexOfId(list, key);
    return i < 0 ? nullptr : &list[static_cast<std::size_t>(i)];
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
    // El campo simulado es un pocillo circular de vidrio. Un recipiente real es mucho más grande
    // (la pared de una placa de 35 mm está a 17 mm): el anillo marca el borde del campo simulado y
    // lo dice la descripción de la placa. Medir lejos del anillo (las células de la orilla tocan
    // vidrio, como junto a la pared real).
    c.dish = true;
    c.cellCulture = plate.kind == PlateKind::CellCulture;
    c.fixedSlide = plate.fixed;
    c.erythrocytesPerCm2 = plate.erythrocytesPerCm2;
    c.plateletsPerCm2 = plate.plateletsPerCm2;
    c.cellSize = plate.cellSize;
    int cells = 64;
    while (cells < 1024 && static_cast<float>(cells) * c.cellSize < plate.fieldUm)
        cells *= 2;
    c.tissue = plate.tissue; // las paredes las genera World con la placa (dependen de la seed)
    c.gridX = c.gridY = cells;
    if (const Vessel* v = findVessel(plate.vessel)) {
        c.depth = v->depthUm();
        // El O₂ entra y el CO₂ sale por la superficie atravesando la capa quieta de medio: el
        // tiempo de equilibrio es el de la difusión, profundidad² / D (unos 50 min en 2,5 mm).
        c.medium.o2AirExchange = units::kDiffusionO2 / (c.depth * c.depth);
    }
    // Estéril: nada vivo, ni detritos, ni sedimento, ni polvo, ni fagos (salvo las bacterias que trae
    // la muestra misma, como las de la boca en un raspado de mucosa).
    c.bacteria = static_cast<std::size_t>(std::lround(plate.bacteriaPerCm2 * 3.14159265f * 0.25f * plate.fieldUm * plate.fieldUm * 1e-8f));
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
