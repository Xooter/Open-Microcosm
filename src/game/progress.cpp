#include "game/progress.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

std::string defaultProgressPath() {
    namespace fs = std::filesystem;
    fs::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        base = xdg;
    else if (const char* home = std::getenv("HOME"); home && *home)
        base = fs::path(home) / ".local" / "share";
    else
        base = ".";
    return (base / "biogame" / "progress.txt").string();
}

Progress loadProgress(const std::string& path) {
    Progress p;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        std::istringstream value(line.substr(eq + 1));
        if (key == "seen_species") value >> p.seenSpecies;
        else if (key == "seen_others") value >> p.seenOthers;
        else if (key == "biomes_played") value >> p.biomesPlayed;
        else if (key == "behavior") p.behaviors.insert(line.substr(eq + 1));
    }
    return p;
}

bool saveProgress(const Progress& p, const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return false;
    out << "seen_species=" << p.seenSpecies << '\n'
        << "seen_others=" << p.seenOthers << '\n'
        << "biomes_played=" << p.biomesPlayed << '\n';
    for (const std::string& b : p.behaviors)
        out << "behavior=" << b << '\n';
    return static_cast<bool>(out);
}

std::string defaultOptionsPath() {
    namespace fs = std::filesystem;
    return (fs::path(defaultProgressPath()).parent_path() / "options.txt").string();
}

Options loadOptions(const std::string& path) {
    Options o;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        std::istringstream value(line.substr(eq + 1));
        if (key == "fullscreen") value >> o.fullscreen;
        else if (key == "post_process") value >> o.postProcess;
        else if (key == "lod") value >> o.lod;
        else if (key == "gpu_physics") value >> o.gpuPhysics;
        else if (key == "language") value >> o.language;
        else if (key == "bio_time_scale") value >> o.bioTimeScale;
    }
    o.bioTimeScale = std::clamp(o.bioTimeScale, 1.0f, 600.0f);
    return o;
}

bool saveOptions(const Options& o, const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return false;
    out << "fullscreen=" << o.fullscreen << '\n'
        << "post_process=" << o.postProcess << '\n'
        << "lod=" << o.lod << '\n'
        << "gpu_physics=" << o.gpuPhysics << '\n'
        << "language=" << o.language << '\n'
        << "bio_time_scale=" << o.bioTimeScale << '\n';
    return static_cast<bool>(out);
}
