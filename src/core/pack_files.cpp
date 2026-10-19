#include "core/pack_files.hpp"

#include "core/paths.hpp"

#include <toml++/toml.hpp>

#include <algorithm>

namespace fs = std::filesystem;

std::vector<fs::path> packFolderFiles(const fs::path& pack, std::string_view folder) {
    const fs::path dir = pack / folder;
    std::vector<fs::path> rest;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.is_regular_file() && entry.path().extension() == ".toml")
            rest.push_back(entry.path());
    std::sort(rest.begin(), rest.end());
    std::vector<fs::path> out;
    try {
        const toml::table manifest = toml::parse_file((pack / kPackManifest).string());
        if (const toml::array* order = manifest["pack"]["order"][folder].as_array())
            for (const toml::node& n : *order) {
                const auto id = n.value<std::string>();
                if (!id)
                    continue;
                const auto it = std::find(rest.begin(), rest.end(), dir / (*id + ".toml"));
                if (it == rest.end())
                    continue; // listado pero sin archivo: el paquete avisa al cargarse
                out.push_back(*it);
                rest.erase(it);
            }
    } catch (const toml::parse_error&) {
        // Sin manifiesto legible: todo por nombre (el paquete se rechaza en game/content).
    }
    out.insert(out.end(), rest.begin(), rest.end());
    return out;
}

fs::path basePackDir() { return assetPath("content/base"); }
