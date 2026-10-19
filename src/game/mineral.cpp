#include "game/mineral.hpp"

#include "core/pack_files.hpp"
#include "core/ids.hpp"
#include "core/toml_reader.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>

namespace {

struct Registry {
    std::deque<MineralInfo> entries;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    // Ya cargado: se lee sin el candado (lo consultan los bucles de cada paso).
    std::atomic<bool> ready{false};
};
Registry& registry() {
    static Registry r;
    return r;
}

constexpr const char* kMineralKeys[] = {"id", "legacy_ids",      "name",  "grain_name", "description", "radius",
                                        "density_difference", "opacity", "flocculates", "style"};

bool readMineral(const toml::table& t, const std::string& source, const std::string& pack, MineralInfo& m,
                 std::vector<std::string>& warnings) {
    toml_io::Reader r{t, source, {}};
    r.text("id", m.key);
    r.text("name", m.name);
    if (m.key.empty() || m.name.empty()) {
        warnings.push_back(source + ": un [[mineral]] sin 'id' o sin 'name': se saltea");
        return false;
    }
    r.where = source + " (" + m.key + ")";
    m.pack = pack;
    r.strings("legacy_ids", m.legacyIds);
    toml_io::warnUnknown(t, kMineralKeys, r.where, warnings);
    r.text("grain_name", m.grainName);
    if (m.grainName.empty())
        m.grainName = "Grano de " + m.name;
    r.text("description", m.description);
    if (!t.get("radius"))
        r.errors.push_back(r.where + ": falta 'radius' [mínimo, máximo] en µm");
    r.range("radius", m.radiusMin, m.radiusMax);
    if (m.radiusMin <= 0.0f)
        r.errors.push_back(r.where + ": 'radius' tiene que ser positivo");
    r.num("density_difference", m.densityGap);
    r.count("opacity", m.opacity);
    if (m.opacity > 1.0f)
        r.errors.push_back(r.where + ": 'opacity' va de 0 a 1");
    r.flag("flocculates", m.flocculates);
    r.text("style", m.style);
    const auto& styles = mineralStyles();
    if (std::find(styles.begin(), styles.end(), m.style) == styles.end()) {
        std::string options;
        for (const std::string& s : styles)
            options += (options.empty() ? "" : ", ") + s;
        r.errors.push_back(r.where + ": 'style' tiene que ser uno de: " + options);
    }
    if (!r.errors.empty()) {
        warnings.insert(warnings.end(), r.errors.begin(), r.errors.end());
        warnings.push_back(r.where + ": el mineral no se carga");
        return false;
    }
    return true;
}

void loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    Registry& reg = registry();
    const toml::array* list = doc.get("mineral") ? doc.get("mineral")->as_array() : nullptr;
    if (!list)
        return;
    for (const toml::node& n : *list) {
        const toml::table* t = n.as_table();
        if (!t)
            continue;
        MineralInfo m;
        if (!readMineral(*t, source, pack, m, reg.warnings))
            continue;
        m.source = toml_io::snippet("mineral", *t);
        bool duplicate = false;
        for (const MineralInfo& other : reg.entries)
            duplicate = duplicate || other.key == m.key;
        if (duplicate) {
            reg.warnings.push_back(source + ": el mineral '" + m.key + "' ya existe (paquete '" + pack + "'): se saltea");
            continue;
        }
        if (reg.entries.size() >= static_cast<std::size_t>(kMaxMinerals)) {
            reg.warnings.push_back(source + ": se alcanzó el tope de minerales; '" + m.key + "' no se carga");
            continue;
        }
        reg.entries.push_back(std::move(m));
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
    for (const auto& path : packFolderFiles(basePackDir(), "minerals")) {
        std::string error;
        if (!loadMineralsFile(path.string(), "base", &error))
            reg.warnings.push_back("no se pudieron cargar los minerales de fábrica: " + error);
    }
    reg.ready.store(true, std::memory_order_release);
}

} // namespace

const std::vector<std::string>& mineralStyles() {
    static const std::vector<std::string> styles = {"floc", "angular_grain", "flake", "opaque_grain",
                                                    "refractile_globule", "frustule"};
    return styles;
}

bool loadMineralsFile(const std::string& path, const std::string& pack, std::string* error) {
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

bool loadMineralsText(const std::string& text, const std::string& source, const std::string& pack,
                      std::string* error) {
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

const std::vector<std::string>& mineralLoadWarnings() {
    ensureBase();
    return registry().warnings;
}

int mineralCount() {
    ensureBase();
    return static_cast<int>(registry().entries.size());
}

const MineralInfo& mineralInfo(MineralKind kind) {
    ensureBase();
    const auto& entries = registry().entries;
    if (entries.empty()) {
        static const MineralInfo clay = [] {
            MineralInfo m;
            m.key = "clay";
            m.name = m.grainName = "Arcilla";
            m.style = "floc";
            return m;
        }();
        return clay;
    }
    const auto i = static_cast<std::size_t>(kind);
    return i < entries.size() ? entries[i] : entries.front();
}

const char* mineralName(MineralKind kind) { return mineralInfo(kind).name.c_str(); }

std::optional<MineralKind> findMineral(std::string_view key) {
    ensureBase();
    const std::ptrdiff_t i = indexOfId(registry().entries, key);
    if (i < 0)
        return std::nullopt;
    return static_cast<MineralKind>(i);
}
