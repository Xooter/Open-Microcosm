#include "game/biome.hpp"

#include "core/pack_files.hpp"
#include "core/ids.hpp"
#include "core/rng.hpp"
#include "game/medium_toml.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>

// Registro de biomas (fase 20.0): los de fábrica vienen de assets/content/base/biomes/; los
// paquetes agregan los suyos detrás. Deque: agregar no mueve los ya cargados.

namespace {

struct Registry {
    std::deque<Biome> entries;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    // Ya cargado: se lee sin el candado (lo consultan los bucles paralelos de cada paso).
    std::atomic<bool> ready{false};
};
Registry& registry() {
    static Registry r;
    return r;
}

constexpr const char* kBiomeKeys[] = {
    "id",        "legacy_ids", "name",      "description", "bacteria", "detritus", "nitrogen_fixing_filaments", "oscillatoria_filaments",
    "cyanobacteria_temperature", "saprolegnia", "chytrid_clusters", "allomyces", "lichens", "physarum",
    "patchiness",   "base_weight",   "weights",       "medium",     "sediment"};

// Un [[biome]] → Biome. false si tiene errores (se descarta).
bool readBiome(const toml::table& t, const std::string& source, const std::string& pack, Biome& b,
               std::vector<std::string>& warnings) {
    toml_io::Reader r{t, source, {}};
    r.text("id", b.key);
    r.text("name", b.name);
    if (b.key.empty() || b.name.empty()) {
        warnings.push_back(source + ": un [[biome]] sin 'id' o sin 'name': se saltea");
        return false;
    }
    r.where = source + " (" + b.key + ")";
    b.pack = pack;
    r.strings("legacy_ids", b.legacyIds);
    toml_io::warnUnknown(t, kBiomeKeys, r.where, warnings);
    r.text("description", b.description);
    r.count("bacteria", b.bacteria);
    r.count("detritus", b.detritus);
    r.count("nitrogen_fixing_filaments", b.cyanoFilaments);
    r.count("oscillatoria_filaments", b.oscillatoriaFilaments);
    r.range("cyanobacteria_temperature", b.cyanoTMin, b.cyanoTMax);
    r.count("saprolegnia", b.saprolegnia);
    r.count("chytrid_clusters", b.chytridClusters);
    r.count("allomyces", b.allomyces);
    r.count("lichens", b.lichens);
    r.count("physarum", b.physarum);
    r.count("patchiness", b.patchiness);
    if (b.patchiness > 1.0f)
        r.errors.push_back(r.where + ": 'patchiness' va de 0 a 1");
    r.count("base_weight", b.baseWeight);
    if (const toml::table* w = r.table("weights")) {
        for (const auto& [k, v] : *w) {
            const auto x = v.value<double>();
            if (!x || *x < 0.0)
                r.errors.push_back(r.where + ": el peso de '" + std::string(k.str()) + "' tiene que ser un número >= 0");
            else
                b.weights.emplace_back(std::string(k.str()), static_cast<float>(*x));
        }
    }
    if (const toml::table* m = r.table("medium"))
        readMedium(*m, r.where + " [medium]", b.medium, r.errors, warnings);
    if (const toml::table* sed = r.table("sediment"))
        readSediment(*sed, r.where + " [sediment]", b.sediment, r.errors, warnings);
    if (!r.errors.empty()) {
        warnings.insert(warnings.end(), r.errors.begin(), r.errors.end());
        warnings.push_back(r.where + ": el bioma no se carga");
        return false;
    }
    return true;
}

void loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    Registry& reg = registry();
    const toml::array* biomes = doc.get("biome") ? doc.get("biome")->as_array() : nullptr;
    if (!biomes)
        return; // el archivo no trae biomas
    for (const toml::node& n : *biomes) {
        const toml::table* t = n.as_table();
        if (!t)
            continue;
        Biome b;
        if (!readBiome(*t, source, pack, b, reg.warnings))
            continue;
        b.source = toml_io::snippet("biome", *t);
        bool duplicate = false;
        for (const Biome& other : reg.entries)
            duplicate = duplicate || other.key == b.key;
        if (duplicate) {
            reg.warnings.push_back(source + ": el bioma '" + b.key + "' ya existe (paquete '" + pack + "'): se saltea");
            continue;
        }
        if (reg.entries.size() >= 0xFFFF) {
            reg.warnings.push_back(source + ": se alcanzó el tope de biomas; '" + b.key + "' no se carga");
            continue;
        }
        reg.entries.push_back(std::move(b));
    }
}

void ensureBase() {
    Registry& reg = registry();
    if (reg.ready.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::recursive_mutex> lock(reg.mutex);
    if (reg.baseLoaded)
        return;
    reg.baseLoaded = true;
    for (const auto& path : packFolderFiles(basePackDir(), "biomes")) {
        std::string error;
        if (!loadBiomesFile(path.string(), "base", &error))
            reg.warnings.push_back("no se pudieron cargar los biomas de fábrica: " + error);
    }
    reg.ready.store(true, std::memory_order_release);
}

} // namespace

bool loadBiomesFile(const std::string& path, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(registry().mutex);
    if (pack != "base")
        ensureBase();
    try {
        loadParsed(toml::parse_file(path), path, pack);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = path + ": " + std::string(e.description());
        return false;
    }
    return true;
}

bool loadBiomesText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
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

const std::vector<std::string>& biomeLoadWarnings() {
    ensureBase();
    return registry().warnings;
}

int biomeCount() {
    ensureBase();
    return std::max(1, static_cast<int>(registry().entries.size()));
}

const Biome& biome(BiomeId id) {
    ensureBase();
    const auto& entries = registry().entries;
    if (entries.empty()) {
        // Sin el paquete de fábrica: agua dulce sin vida, para que la placa se pueda crear igual.
        static const Biome water = [] {
            Biome b;
            b.key = "agua";
            b.name = "Agua";
            b.baseWeight = 0.0f;
            return b;
        }();
        return water;
    }
    const auto i = static_cast<std::size_t>(id);
    return i < entries.size() ? entries[i] : entries.front();
}

std::optional<BiomeId> findBiome(std::string_view key) {
    ensureBase();
    const std::ptrdiff_t i = indexOfId(registry().entries, key);
    if (i < 0)
        return std::nullopt;
    return static_cast<BiomeId>(i);
}

float Biome::weight(SpeciesId id) const {
    const std::string_view key = speciesInfo(id).key;
    for (const auto& [k, w] : weights)
        if (k == key)
            return w;
    return hasTag(id, "obligate_symbiont") ? 0.0f : baseWeight;
}

float dropAreaScale(const WorldConfig& c) {
    constexpr float kReferenceArea = 2048.0f * 1024.0f; // [µm²]
    const float w = static_cast<float>(c.gridX) * c.cellSize, h = static_cast<float>(c.gridY) * c.cellSize;
    if (!c.dish)
        return w * h / kReferenceArea;
    const float r = 0.5f * std::min(w, h) - c.dishMargin;
    return 3.14159265f * r * r / kReferenceArea;
}

WorldConfig makeDrop(BiomeId id, std::uint64_t seed) {
    const Biome& b = biome(id);
    WorldConfig c;
    c.seed = seed;
    c.medium = b.medium;
    Rng rng(seed, rng_stream::kPopulate + 900, 0, 0);
    // Variación de la gota: ninguna es igual a otra.
    c.medium.temperature += 1.5f * rng.normal();
    c.medium.surfaceLight = std::clamp(c.medium.surfaceLight * rng.uniform(0.8f, 1.2f), 0.0f, 1.0f);
    c.medium.phosphate *= rng.uniform(0.6f, 1.6f);
    c.medium.nitrogen *= rng.uniform(0.6f, 1.6f);
    c.medium.alkalinity *= rng.uniform(0.7f, 1.4f);
    // Micronutrientes: otro flujo de azar, así no cambia el resto de la gota de una seed.
    Rng micro(seed, rng_stream::kPopulate + 902, 0, 0);
    c.medium.silicate *= micro.uniform(0.6f, 1.6f);
    c.medium.iron *= micro.uniform(0.5f, 1.8f);
    c.medium.vitamin *= micro.uniform(0.5f, 1.8f);
    for (int k = 0; k < speciesCount(); ++k) {
        Rng r(seed, rng_stream::kPopulate + 901, static_cast<std::uint64_t>(k), 0);
        const bool absent = r.uniform() < 0.15f; // algunas especies no llegaron a esta gota
        c.speciesWeight[k] = absent ? 0.0f : b.weight(static_cast<SpeciesId>(k)) * r.uniform(0.5f, 1.5f);
    }
    c.bacteria = static_cast<std::size_t>(static_cast<float>(b.bacteria) * rng.uniform(0.7f, 1.3f));
    c.detritus = static_cast<std::size_t>(static_cast<float>(b.detritus) * rng.uniform(0.6f, 1.5f));
    c.cyanoFilaments = static_cast<int>(static_cast<float>(b.cyanoFilaments) * rng.uniform(0.5f, 1.5f));
    c.oscillatoriaFilaments = static_cast<int>(static_cast<float>(b.oscillatoriaFilaments) * rng.uniform(0.5f, 1.5f));
    c.cyanoTMin = b.cyanoTMin;
    c.cyanoTMax = b.cyanoTMax;
    c.saprolegnia = b.saprolegnia;
    c.chytridClusters = b.chytridClusters;
    c.allomyces = b.allomyces;
    c.lichens = b.lichens;
    c.physarum = b.physarum;
    c.patchiness = std::clamp(b.patchiness * rng.uniform(0.5f, 1.5f), 0.0f, 1.0f);
    c.sediment = b.sediment;
    const float muddy = rng.uniform(0.5f, 1.6f); // algunas gotas son más barrosas que otras
    // Misma densidad en cualquier tamaño de placa.
    const float area = dropAreaScale(c);
    auto scaled = [&](float n) { return std::max(0.0f, std::round(n * area)); };
    c.bacteria = static_cast<std::size_t>(scaled(static_cast<float>(c.bacteria)));
    c.detritus = static_cast<std::size_t>(scaled(static_cast<float>(c.detritus)));
    c.cyanoFilaments = static_cast<int>(scaled(static_cast<float>(c.cyanoFilaments)));
    c.oscillatoriaFilaments = static_cast<int>(scaled(static_cast<float>(c.oscillatoriaFilaments)));
    c.saprolegnia = static_cast<int>(scaled(static_cast<float>(c.saprolegnia)));
    c.chytridClusters = static_cast<int>(scaled(static_cast<float>(c.chytridClusters)));
    c.allomyces = static_cast<int>(scaled(static_cast<float>(c.allomyces)));
    c.lichens = static_cast<int>(scaled(static_cast<float>(c.lichens)));
    c.physarum = static_cast<int>(scaled(static_cast<float>(c.physarum)));
    for (int k = 0, n = mineralCount(); k < n; ++k)
        c.sediment.count[k] = static_cast<int>(scaled(static_cast<float>(c.sediment.count[k]) * muddy));
    c.sediment.rocks = static_cast<int>(scaled(static_cast<float>(c.sediment.rocks)));
    return c;
}

void applyDropContent(WorldConfig& c, DropContent content) {
    if (content == DropContent::Full)
        return;
    c.bacteria = 0;
    c.cyanoFilaments = 0;
    c.oscillatoriaFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.physarum = c.allomyces = c.lichens = 0;
    c.medium.phage = 0.0f;
    if (content != DropContent::EmptyPetri)
        return;
    // Un plato de Petri limpio: nada inorgánico ni muerto flotando en el agua.
    c.sediment = {};
    c.medium.dustRate = 0.0f;
    c.detritus = 0;
    c.patchiness = 0.0f; // ni manchas de materia orgánica ni los detritos que las acompañan
    c.medium.detritusSpawnRate = 0.0f;
}
