#include "game/content.hpp"

#include "core/paths.hpp"
#include "core/toml_reader.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

namespace {

// Huella del paquete: cambia si cambia cualquier dato (un valor, un ser, un comentario).
std::uint64_t fingerprintDir(const std::string& dir) {
    namespace fs = std::filesystem;
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec))
        if (entry.is_regular_file() && entry.path().extension() == ".toml")
            files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](const std::string& bytes) {
        for (const char c : bytes) {
            h ^= static_cast<unsigned char>(c);
            h *= 1099511628211ull;
        }
    };
    for (const fs::path& f : files) {
        mix(f.filename().string());
        std::ifstream in(f, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        mix(ss.str());
    }
    return h;
}

PackInfo readPack(const std::string& dir) {
    PackInfo p;
    p.dir = dir;
    try {
        const toml::table doc = toml::parse_file(dir + "/paquete.toml");
        if (const toml::table* t = doc.get("paquete") ? doc.get("paquete")->as_table() : nullptr) {
            toml_io::Reader r{*t, dir, {}};
            r.text("id", p.id);
            r.text("nombre", p.name);
            r.text("version", p.version);
            r.text("autor", p.author);
            r.text("licencia", p.license);
        }
    } catch (const toml::parse_error&) {
    }
    p.fingerprint = fingerprintDir(dir);
    return p;
}

} // namespace

const std::vector<PackInfo>& loadedPacks() {
    static std::vector<PackInfo> packs;
    static std::once_flag once;
    std::call_once(once, [] {
        PackInfo base = readPack(assetPath("content/base"));
        if (base.id.empty())
            base.id = "base";
        packs.push_back(std::move(base));
    });
    return packs;
}

const PackInfo* findPack(const std::string& id) {
    for (const PackInfo& p : loadedPacks())
        if (p.id == id)
            return &p;
    return nullptr;
}

std::string fingerprintHex(std::uint64_t fingerprint) {
    char buf[20];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(fingerprint));
    return buf;
}
