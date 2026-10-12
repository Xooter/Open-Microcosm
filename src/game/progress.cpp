#include "game/progress.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

std::vector<LocomotionType> Progress::recordRun(std::uint32_t generation, float share, bool won) {
    ++runs;
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
    return static_cast<bool>(out);
}
