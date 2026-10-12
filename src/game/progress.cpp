#include "game/progress.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

Genome Progress::founderGenome(LocomotionType t, Rng& rng) const {
    const int i = static_cast<int>(t);
    return hasLegacy[i] ? inherit(t, legacy[i], rng) : speciesGenome(t);
}

std::vector<LocomotionType> Progress::recordRun(std::uint32_t generation, float share, bool won,
                                                LocomotionType founder, const Genome& lineageGenome) {
    ++runs;
    hasLegacy[static_cast<int>(founder)] = true;
    legacy[static_cast<int>(founder)] = lineageGenome;
    if (won)
        ++wins;
    bestGeneration = std::max(bestGeneration, generation);
    bestShare = std::max(bestShare, share);
    std::vector<LocomotionType> fresh;
    auto unlock = [&](LocomotionType t, bool condition) {
        if (condition && !isUnlocked(t)) {
            unlocked[static_cast<int>(t)] = true;
            fresh.push_back(t);
        }
    };
    unlock(LocomotionType::Flagellum, bestGeneration >= 3);
    unlock(LocomotionType::Pseudopod, bestGeneration >= 5 || wins > 0);
    return fresh;
}

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
        if (key == "unlock_flagellum") value >> p.unlocked[1];
        else if (key == "unlock_pseudopod") value >> p.unlocked[2];
        else if (key == "runs") value >> p.runs;
        else if (key == "wins") value >> p.wins;
        else if (key == "best_generation") value >> p.bestGeneration;
        else if (key == "best_share") value >> p.bestShare;
        else if (key.rfind("legacy_", 0) == 0) {
            const int i = key.back() - '0';
            if (i < 0 || i > 2)
                continue;
            std::string item;
            std::size_t g = 0;
            while (std::getline(value, item, ',') && g < kGeneCount)
                p.legacy[i].v[g++] = std::stof(item);
            p.hasLegacy[i] = g == kGeneCount;
        }
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
    out << "unlock_flagellum=" << p.unlocked[1] << '\n'
        << "unlock_pseudopod=" << p.unlocked[2] << '\n'
        << "runs=" << p.runs << '\n'
        << "wins=" << p.wins << '\n'
        << "best_generation=" << p.bestGeneration << '\n'
        << "best_share=" << p.bestShare << '\n';
    for (int i = 0; i < 3; ++i) {
        if (!p.hasLegacy[i])
            continue;
        out << "legacy_" << i << '=';
        for (std::size_t g = 0; g < kGeneCount; ++g)
            out << (g ? "," : "") << p.legacy[i].v[g];
        out << '\n';
    }
    return static_cast<bool>(out);
}
