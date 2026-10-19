#include "game/reagent.hpp"

#include "core/pack_files.hpp"
#include "core/ids.hpp"
#include "core/toml_reader.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>

namespace {

struct Registry {
    std::deque<Reagent> reagents;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    std::atomic<bool> ready{false};
};
Registry& registry() {
    static Registry r;
    return r;
}

constexpr const char* kKeys[] = {"id", "legacy_ids", "name", "description", "mechanism", "targets", "growth_dependent",
                                 "pharmacodynamics", "published_mic", "diffusion"};
constexpr const char* kPdKeys[] = {"psi_max", "psi_min", "zmic", "kappa", "source"};
constexpr const char* kMicKeys[] = {"value", "source"};

void loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    Registry& reg = registry();
    const toml::array* list = doc["reagent"].as_array();
    if (!list)
        return;
    for (const toml::node& node : *list) {
        const toml::table* t = node.as_table();
        if (!t)
            continue;
        Reagent r;
        r.pack = pack;
        toml_io::Reader rd{*t, source + " [[reagent]]", {}};
        rd.text("id", r.key);
        const std::string where = source + " reactivo '" + r.key + "'";
        toml_io::warnUnknown(*t, kKeys, where, reg.warnings);
        rd.strings("legacy_ids", r.legacyIds);
        rd.text("name", r.name);
        rd.text("description", r.description);
        rd.flag("growth_dependent", r.growthDependent);
        rd.num("diffusion", r.diffusion);
        const std::string mechanism = (*t)["mechanism"].value_or(std::string("pharmacodynamic"));
        if (mechanism != "pharmacodynamic")
            reg.warnings.push_back(where + ": mecanismo desconocido '" + mechanism + "'");
        if (const toml::array* b = (*t)["targets"].as_array())
            for (const toml::node& n : *b)
                if (auto s = n.value<std::string>())
                    r.targets.push_back(*s);
        if (const toml::table* pd = (*t)["pharmacodynamics"].as_table()) {
            toml_io::warnUnknown(*pd, kPdKeys, where + " farmacodinamia", reg.warnings);
            toml_io::Reader p{*pd, where + " farmacodinamia", {}};
            p.num("psi_max", r.psiMax);
            p.num("psi_min", r.psiMin);
            p.num("zmic", r.zMic);
            p.num("kappa", r.kappa);
            p.text("source", r.source);
            reg.warnings.insert(reg.warnings.end(), p.errors.begin(), p.errors.end());
        } else {
            reg.warnings.push_back(where + ": falta [reagent.pharmacodynamics]");
        }
        if (const toml::table* m = (*t)["published_mic"].as_table()) {
            toml_io::warnUnknown(*m, kMicKeys, where + " cim_publicada", reg.warnings);
            r.publishedMic = (*m)["value"].value_or(0.0);
            r.micSource = (*m)["source"].value_or(std::string());
        }
        reg.warnings.insert(reg.warnings.end(), rd.errors.begin(), rd.errors.end());
        if (r.source.empty())
            reg.warnings.push_back(where + ": la farmacodinamia necesita su 'source'");
        if (r.key.empty() || r.name.empty() || !(r.psiMax > 0.0) || !(r.zMic > 0.0) || !(r.kappa > 0.0)) {
            reg.warnings.push_back(where + ": sin id, nombre o farmacodinamia válida: se saltea");
            continue;
        }
        if (std::any_of(reg.reagents.begin(), reg.reagents.end(), [&](const Reagent& o) { return o.key == r.key; })) {
            reg.warnings.push_back(where + ": id repetido (queda el primero)");
            continue;
        }
        reg.reagents.push_back(std::move(r));
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
    for (const auto& file : packFolderFiles(basePackDir(), "reagents")) {
        const std::string path = file.string();
        try {
            loadParsed(toml::parse_file(path), path, "base");
        } catch (const toml::parse_error& e) {
            reg.warnings.push_back("no se pudo cargar " + path + ": " + std::string(e.description()));
        }
    }
    reg.ready.store(true, std::memory_order_release);
}

} // namespace

bool Reagent::affects(std::string_view target) const {
    return std::find(targets.begin(), targets.end(), target) != targets.end();
}

int reagentCount() {
    ensureBase();
    return static_cast<int>(registry().reagents.size());
}
const Reagent& reagent(int index) {
    ensureBase();
    return registry().reagents[static_cast<std::size_t>(index)];
}
int findReagent(std::string_view key) {
    ensureBase();
    return static_cast<int>(indexOfId(registry().reagents, key));
}
bool loadReagentsText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
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
const std::vector<std::string>& reagentLoadWarnings() {
    ensureBase();
    return registry().warnings;
}

double reagentNetGrowth(const Reagent& r, double a) {
    if (!(a > 0.0))
        return r.psiMax;
    const double x = std::pow(a / r.zMic, r.kappa);
    return r.psiMax - (r.psiMax - r.psiMin) * x / (x - r.psiMin / r.psiMax);
}

double reagentKillRate(const Reagent& r, double a, double growthPerHour) {
    const double kill = r.psiMax - reagentNetGrowth(r, a);
    if (!r.growthDependent)
        return std::max(0.0, kill);
    return std::max(0.0, kill * std::max(0.0, growthPerHour) / r.psiMax);
}
