#include "game/content.hpp"

#include "bio/body_plan.hpp"
#include "bio/physiology.hpp"
#include "build_info.hpp"
#include "core/paths.hpp"
#include "core/toml_reader.hpp"
#include "game/lesson.hpp"
#include "game/plate_spec.hpp"
#include "game/prepared.hpp"
#include "game/reagent.hpp"
#include "game/progress.hpp"

#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

namespace {

namespace fs = std::filesystem;

struct Packs {
    std::deque<PackInfo> list;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
};
Packs& packs() {
    static Packs p;
    return p;
}

void mix(std::uint64_t& h, const std::string& bytes) {
    for (const char c : bytes) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
}

bool readFile(const fs::path& path, std::string& out, std::vector<std::string>& messages) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        messages.push_back(path.string() + ": no se puede leer");
        return false;
    }
    if (size > kMaxPackBytes) {
        messages.push_back(path.string() + ": supera el tamaño máximo de un paquete (8 MB)");
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return static_cast<bool>(in) || in.eof();
}

// Versión "a.b.c" como números, para comparar con la del programa.
std::vector<int> versionNumbers(const std::string& v) {
    std::vector<int> out;
    int n = 0;
    bool any = false;
    for (const char c : v) {
        if (c >= '0' && c <= '9') {
            n = n * 10 + (c - '0');
            any = true;
        } else if (c == '.') {
            out.push_back(n);
            n = 0;
        } else {
            break;
        }
    }
    if (any)
        out.push_back(n);
    return out;
}

bool validId(const std::string& id) {
    if (id.empty() || id.size() > kMaxPackIdLength)
        return false;
    for (const char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

// Lee y valida el manifiesto ([paquete]). false: el paquete se rechaza.
bool readManifest(const toml::table& doc, const std::string& where, PackInfo& p, std::vector<std::string>& messages) {
    const toml::table* t = doc.get("paquete") ? doc.get("paquete")->as_table() : nullptr;
    if (!t) {
        messages.push_back(where + ": falta la tabla [paquete] (id, nombre, version, autor, licencia)");
        return false;
    }
    toml_io::Reader r{*t, where, {}};
    constexpr const char* kKeys[] = {"id", "nombre", "version", "autor", "licencia", "programa_minimo"};
    toml_io::warnUnknown(*t, kKeys, where + " [paquete]", messages);
    std::string minimum;
    r.text("id", p.id);
    r.text("nombre", p.name);
    r.text("version", p.version);
    r.text("autor", p.author);
    r.text("licencia", p.license);
    r.text("programa_minimo", minimum);
    messages.insert(messages.end(), r.errors.begin(), r.errors.end());
    if (!validId(p.id)) {
        messages.push_back(where + ": el id del paquete tiene que ser de 1 a 64 letras minúsculas, dígitos o '_'");
        return false;
    }
    if (p.version.empty()) {
        messages.push_back(where + ": falta la 'version' del paquete");
        return false;
    }
    if (p.license.empty())
        messages.push_back(where + ": el paquete no declara su licencia");
    if (!minimum.empty() && versionNumbers(minimum) > versionNumbers(BIO_VERSION)) {
        messages.push_back(where + ": el paquete '" + p.id + "' necesita BioGame " + minimum + " o más nuevo (este es " +
                           BIO_VERSION + ")");
        return false;
    }
    return true;
}

// Carga las definiciones de un texto TOML en todos los registros, en orden de dependencias: los
// seres nombran planes; la fisiología, seres; los biomas, seres y minerales.
void loadDefinitions(const std::string& text, const std::string& source, const std::string& pack,
                     std::vector<std::string>& messages) {
    // Los avisos nuevos de cada registro (claves desconocidas, referencias sin resolver…) también
    // van a `messages`, así quien importa ve qué pasó con su paquete.
    const std::vector<std::string>* lists[] = {&bodyPlanWarnings(), &mineralLoadWarnings(), &speciesLoadWarnings(),
                                               &physiologyWarnings(), &biomeLoadWarnings(), &preparedLoadWarnings(),
                                               &lessonLoadWarnings(), &reagentLoadWarnings()};
    constexpr int kLists = 8;
    std::size_t before[kLists];
    for (int i = 0; i < kLists; ++i)
        before[i] = lists[i]->size();
    std::string error;
    if (!loadBodyPlansText(text, source, pack, &error))
        messages.push_back(error);
    if (!loadMineralsText(text, source, pack, &error))
        messages.push_back(error);
    if (!loadSpeciesText(text, source, pack, &error))
        messages.push_back(error);
    if (!addSpeciesPhysiologyText(text, source, &error))
        messages.push_back(error);
    if (!loadBiomesText(text, source, pack, &error))
        messages.push_back(error);
    if (!loadPreparedText(text, source, pack, &error)) // medios, recipientes y placas preparadas
        messages.push_back(error);
    if (!loadLessonsText(text, source, pack, &error)) // lecciones (nombran placas, biomas y seres)
        messages.push_back(error);
    if (!loadReagentsText(text, source, pack, &error)) // reactivos (antibióticos)
        messages.push_back(error);
    for (int i = 0; i < kLists; ++i)
        messages.insert(messages.end(), lists[i]->begin() + static_cast<std::ptrdiff_t>(std::min(before[i], lists[i]->size())),
                        lists[i]->end());
}

std::string sanitizeFileName(const std::string& name) {
    std::string out;
    for (const char c : name)
        out += ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
                   ? c
                   : '_';
    return out.substr(0, 64);
}

// Las [[escena]] de un .biopack pasan a la carpeta de placas (no se pisa una que ya existe).
void installScenes(const toml::table& doc, const std::string& where, const std::string& pack,
                   std::vector<std::string>& messages) {
    const toml::array* scenes = doc.get("escena") ? doc.get("escena")->as_array() : nullptr;
    if (!scenes)
        return;
    for (const toml::node& n : *scenes) {
        const toml::table* t = n.as_table();
        if (!t)
            continue;
        toml_io::Reader r{*t, where + " [[escena]]", {}};
        std::string id, plate;
        r.text("id", id);
        r.text("placa", plate);
        if (id.empty() || plate.empty() || !r.errors.empty()) {
            messages.push_back(where + ": una [[escena]] sin 'id' o sin 'placa': se saltea");
            continue;
        }
        PlateSpec probe;
        if (!parsePlate(plate, probe)) {
            messages.push_back(where + ": la escena '" + id + "' no es una placa válida");
            continue;
        }
        const fs::path dest = fs::path(defaultPlatesDir()) / (sanitizeFileName(pack + "_" + id) + ".plate");
        std::error_code ec;
        fs::create_directories(dest.parent_path(), ec);
        if (fs::exists(dest, ec))
            continue; // ya instalada (el paquete se carga en cada sesión)
        std::ofstream out(dest);
        out << plate;
        if (!out)
            messages.push_back(where + ": no se pudo guardar la escena '" + id + "'");
    }
}

std::uint64_t fingerprintDir(const fs::path& dir) {
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.is_regular_file() && entry.path().extension() == ".toml")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    std::uint64_t h = 1469598103934665603ull;
    for (const fs::path& f : files) {
        mix(h, f.filename().string());
        std::ifstream in(f, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        mix(h, ss.str());
    }
    return h;
}

void ensureBase() {
    Packs& p = packs();
    std::lock_guard<std::recursive_mutex> lock(p.mutex);
    if (p.baseLoaded)
        return;
    p.baseLoaded = true;
    // Los registros cargan sus archivos de fábrica solos; acá solo el manifiesto y la huella.
    const fs::path dir = assetPath("content/base");
    PackInfo base;
    base.path = dir.string();
    try {
        std::vector<std::string> ignored;
        readManifest(toml::parse_file((dir / "paquete.toml").string()), base.path, base, ignored);
    } catch (const toml::parse_error&) {
    }
    if (base.id.empty())
        base.id = "base";
    base.fingerprint = fingerprintDir(dir);
    p.list.push_front(std::move(base));
}

constexpr const char* kDirFiles[] = {"planes.toml", "minerales.toml", "seres.toml",   "fisiologia.toml",
                                     "biomas.toml", "medios.toml",    "recipientes.toml", "placas.toml",
                                     "lecciones.toml", "reactivos.toml"};
constexpr const char* kBiopackKeys[] = {"paquete", "ser",   "plan",       "mineral", "bioma",
                                        "fisiologia", "medio", "recipiente", "placa",   "escena", "leccion", "reactivo"};

} // namespace

std::vector<PackInfo> loadedPacks() {
    ensureBase();
    std::lock_guard<std::recursive_mutex> lock(packs().mutex);
    return {packs().list.begin(), packs().list.end()};
}

const PackInfo* findPack(const std::string& id) {
    ensureBase();
    std::lock_guard<std::recursive_mutex> lock(packs().mutex);
    for (const PackInfo& p : packs().list)
        if (p.id == id)
            return &p;
    return nullptr;
}

std::string fingerprintHex(std::uint64_t fingerprint) {
    char buf[20];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(fingerprint));
    return buf;
}

std::string userPacksDir() { return (fs::path(userDataDir()) / "paquetes").string(); }

bool loadPack(const std::string& path, std::vector<std::string>& messages) {
    ensureBase();
    Packs& reg = packs();
    std::lock_guard<std::recursive_mutex> lock(reg.mutex);
    std::vector<std::string> local;
    PackInfo info;
    info.path = path;
    const fs::path p(path);
    std::error_code ec;
    bool ok = false;
    if (fs::is_directory(p, ec)) {
        // Carpeta: el manifiesto y los archivos conocidos.
        std::string manifest;
        if (readFile(p / "paquete.toml", manifest, local)) {
            try {
                ok = readManifest(toml::parse(manifest, (p / "paquete.toml").string()), path, info, local);
            } catch (const toml::parse_error& e) {
                local.push_back(path + ": " + std::string(e.description()));
            }
        }
        if (ok && findPack(info.id)) {
            local.push_back(path + ": el paquete '" + info.id + "' ya está cargado");
            ok = false;
        }
        if (ok) {
            for (const auto& entry : fs::directory_iterator(p, ec)) {
                const std::string name = entry.path().filename().string();
                if (entry.path().extension() == ".toml" && name != "paquete.toml" &&
                    std::find_if(std::begin(kDirFiles), std::end(kDirFiles), [&](const char* f) { return name == f; }) ==
                        std::end(kDirFiles))
                    local.push_back(path + ": archivo desconocido '" + name + "' (se ignora)");
            }
            for (const char* file : kDirFiles) {
                std::string text;
                if (fs::exists(p / file, ec) && readFile(p / file, text, local))
                    loadDefinitions(text, (p / file).string(), info.id, local);
            }
            info.fingerprint = fingerprintDir(p);
        }
    } else {
        // Un .biopack: todo en un archivo.
        std::string text;
        if (readFile(p, text, local)) {
            try {
                const toml::table doc = toml::parse(text, path);
                ok = readManifest(doc, path, info, local);
                if (ok && findPack(info.id)) {
                    local.push_back(path + ": el paquete '" + info.id + "' ya está cargado");
                    ok = false;
                }
                if (ok) {
                    toml_io::warnUnknown(doc, kBiopackKeys, path, local);
                    loadDefinitions(text, path, info.id, local);
                    installScenes(doc, path, info.id, local);
                    std::uint64_t h = 1469598103934665603ull;
                    mix(h, text);
                    info.fingerprint = h;
                }
            } catch (const toml::parse_error& e) {
                local.push_back(path + ": " + std::string(e.description()));
            }
        }
    }
    if (ok)
        reg.list.push_back(info);
    else
        local.push_back(path + ": el paquete no se carga");
    reg.warnings.insert(reg.warnings.end(), local.begin(), local.end());
    messages.insert(messages.end(), local.begin(), local.end());
    return ok;
}

void loadUserPacks(std::vector<std::string>& messages) {
    std::vector<fs::path> entries;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(userPacksDir(), ec))
        if (entry.is_directory() || entry.path().extension() == ".biopack")
            entries.push_back(entry.path());
    std::sort(entries.begin(), entries.end());
    for (const fs::path& e : entries)
        loadPack(e.string(), messages);
}

bool importPack(const std::string& file, std::vector<std::string>& messages) {
    const fs::path src(file);
    std::error_code ec;
    if (src.extension() != ".biopack" || !fs::is_regular_file(src, ec)) {
        messages.push_back(file + ": no es un archivo .biopack");
        return false;
    }
    fs::create_directories(userPacksDir(), ec);
    const fs::path dest = fs::path(userPacksDir()) / src.filename();
    if (fs::exists(dest, ec) && !fs::equivalent(src, dest, ec)) {
        messages.push_back(dest.string() + ": ya hay un paquete con ese nombre de archivo");
        return false;
    }
    if (!fs::equivalent(src, dest, ec))
        fs::copy_file(src, dest, ec);
    if (ec) {
        messages.push_back(dest.string() + ": no se pudo copiar (" + ec.message() + ")");
        return false;
    }
    if (!loadPack(dest.string(), messages)) {
        fs::remove(dest, ec); // rechazado: no queda para la próxima sesión
        return false;
    }
    return true;
}

std::string biopackText(const BiopackSpec& spec, std::vector<std::string>& messages) {
    if (!validId(spec.id)) {
        messages.push_back("el id del paquete tiene que ser de 1 a 64 letras minúsculas, dígitos o '_'");
        return {};
    }
    std::ostringstream out;
    out << "# Paquete de contenido de BioGame (.biopack): solo datos. Se importa desde el panel Contenido.\n";
    toml::table manifest;
    toml::table head;
    head.insert("id", spec.id);
    head.insert("nombre", spec.name.empty() ? spec.id : spec.name);
    head.insert("version", spec.version);
    head.insert("autor", spec.author);
    head.insert("licencia", spec.license);
    head.insert("programa_minimo", std::string(BIO_VERSION));
    manifest.insert("paquete", std::move(head));
    out << manifest << "\n\n";
    std::set<std::string> plans;
    for (const SpeciesId id : spec.beings) {
        out << speciesSource(id) << "\n\n";
        // El plan corporal viaja con el ser si no es de fábrica (los de fábrica están en todos lados).
        const std::string plan = speciesInfo(id).plan;
        std::string planPack;
        const std::string planText = plan.empty() ? std::string() : bodyPlanSource(plan, &planPack);
        if (!planText.empty() && planPack != "base" && plans.insert(plan).second)
            out << planText << "\n\n";
        const std::string physiology = physiologySource(id);
        if (!physiology.empty())
            out << physiology << "\n\n";
    }
    for (const MineralKind m : spec.minerals)
        out << mineralInfo(m).source << "\n\n";
    for (const BiomeId b : spec.biomes)
        out << biome(b).source << "\n\n";
    // Una placa preparada viaja con su medio y su recipiente si no son de fábrica.
    std::set<std::string> media, vessels;
    for (const std::string& key : spec.prepared) {
        const PreparedPlate* p = findPreparedPlate(key);
        if (!p)
            continue;
        const CultureMedium* m = findCultureMedium(p->medium);
        if (m && m->pack != "base" && media.insert(m->key).second)
            out << m->source << "\n\n";
        const Vessel* v = findVessel(p->vessel);
        if (v && v->pack != "base" && vessels.insert(v->key).second)
            out << v->source << "\n\n";
        out << p->source << "\n\n";
    }
    for (const auto& [name, text] : spec.scenes) {
        toml::table scene;
        scene.insert("id", sanitizeFileName(name));
        scene.insert("placa", text);
        toml::array list;
        list.push_back(std::move(scene));
        toml::table root;
        root.insert("escena", std::move(list));
        out << root << "\n\n";
    }
    std::string text = out.str();
    // Lo exportado se vuelve a leer: un paquete que no se puede importar no sirve.
    try {
        (void)toml::parse(text, spec.id + ".biopack");
    } catch (const toml::parse_error& e) {
        messages.push_back("el paquete exportado no es TOML válido: " + std::string(e.description()));
        return {};
    }
    if (text.size() > kMaxPackBytes) {
        messages.push_back("el paquete supera el tamaño máximo (8 MB)");
        return {};
    }
    return text;
}

std::vector<std::string> contentWarnings() {
    std::vector<std::string> all;
    for (const auto* list : {&speciesLoadWarnings(), &bodyPlanWarnings(), &physiologyWarnings(), &mineralLoadWarnings(),
                             &biomeLoadWarnings(), &preparedLoadWarnings()})
        all.insert(all.end(), list->begin(), list->end());
    ensureBase();
    std::lock_guard<std::recursive_mutex> lock(packs().mutex);
    for (const std::string& w : packs().warnings)
        if (std::find(all.begin(), all.end(), w) == all.end())
            all.push_back(w);
    return all;
}
