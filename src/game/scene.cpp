#include "game/scene.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

std::string serializeScene(const PlateSpec& plate, const std::vector<Intervention>& events) {
    return serializePlate(plate) + serializeInterventions(events);
}

bool parseScene(const std::string& text, PlateSpec& plate, std::vector<Intervention>& events,
                std::vector<std::string>* missing) {
    if (!parsePlate(text, plate, missing))
        return false;
    events = parseInterventions(text, missing);
    return true;
}

bool saveScene(const std::string& path, const PlateSpec& plate, const std::vector<Intervention>& events) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return false;
    out << serializeScene(plate, events);
    return static_cast<bool>(out);
}

bool loadScene(const std::string& path, PlateSpec& plate, std::vector<Intervention>& events,
               std::vector<std::string>* missing) {
    std::ifstream in(path);
    if (!in)
        return false;
    std::stringstream ss;
    ss << in.rdbuf();
    return parseScene(ss.str(), plate, events, missing);
}

RunConfig plateRunConfig(const PlateSpec& plate) {
    RunConfig rc;
    rc.spawnFounder = false; // en el laboratorio, el ser a controlar se elige con la herramienta Controlar
    rc.wildlife = plate.wildlife();
    rc.biome = plate.biome;
    rc.seed = plate.config.seed;
    return rc;
}
