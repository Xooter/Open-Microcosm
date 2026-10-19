#include "ui/i18n.hpp"
#include "core/pack_files.hpp"
#include "core/paths.hpp"
#include "game/content.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ui {

namespace {

struct Hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};

std::unordered_map<std::string, std::string, Hash, std::equal_to<>>& table() {
    static std::unordered_map<std::string, std::string, Hash, std::equal_to<>> t;
    return t;
}

std::string& language() {
    static std::string code = "es";
    return code;
}

// Claves sin traducción (solo con BIO_TR_FALTANTES).
std::set<std::string>& missing() {
    static std::set<std::string> m;
    return m;
}
const char* missingPath() {
    static const char* path = std::getenv("BIO_TR_FALTANTES");
    return path;
}

void loadFile(const std::string& path) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos || line.empty() || line[0] == '#')
            continue;
        std::string value = line.substr(tab + 1);
        // "\n" en el archivo es un salto de línea del texto.
        for (std::size_t p = value.find("\\n"); p != std::string::npos; p = value.find("\\n", p + 1))
            value.replace(p, 2, "\n");
        std::string key = line.substr(0, tab);
        for (std::size_t p = key.find("\\n"); p != std::string::npos; p = key.find("\\n", p + 1))
            key.replace(p, 2, "\n");
        table()[key] = value;
    }
}

} // namespace

const char* tr(const char* key) {
    const auto& t = table();
    if (t.empty())
        return key;
    const auto it = t.find(std::string_view(key));
    if (it != t.end())
        return it->second.c_str();
    if (missingPath() && key[0] != '\0')
        missing().insert(key);
    return key;
}

const std::string& currentLanguage() { return language(); }

void writeMissingKeys() {
    if (!missingPath() || missing().empty())
        return;
    std::ofstream out(missingPath());
    for (const std::string& k : missing()) {
        std::string line = k;
        for (std::size_t p = line.find('\n'); p != std::string::npos; p = line.find('\n', p + 2))
            line.replace(p, 1, "\\n");
        out << line << '\n';
    }
}

bool loadLanguage(const std::string& code) {
    table().clear();
    language() = code.empty() ? "es" : code;
    if (code.empty() || code == "es")
        return true;
    const std::string base = assetPath("lang/" + code + ".txt");
    if (!std::filesystem::exists(base))
        return false;
    loadFile(base);
    // Las traducciones de los paquetes del usuario (los textos de sus seres, medios, lecciones…).
    for (const PackInfo& p : loadedPacks()) {
        std::error_code ec;
        // "idiomas": los paquetes de antes de que las carpetas pasaran al inglés.
        for (const char* folder : {kPackLangFolder, "idiomas"}) {
            const std::filesystem::path file = std::filesystem::path(p.path) / folder / (code + ".txt");
            if (std::filesystem::is_regular_file(file, ec)) {
                loadFile(file.string());
                break;
            }
        }
    }
    return true;
}

} // namespace ui
