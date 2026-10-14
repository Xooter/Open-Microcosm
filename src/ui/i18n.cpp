#include "ui/i18n.hpp"
#include "core/paths.hpp"

#include <fstream>
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

} // namespace

const char* tr(const char* key) {
    const auto& t = table();
    if (t.empty())
        return key;
    const auto it = t.find(std::string_view(key));
    return it == t.end() ? key : it->second.c_str();
}

bool loadLanguage(const std::string& code) {
    table().clear();
    if (code.empty() || code == "es")
        return true;
    std::ifstream in(assetPath("lang/" + code + ".txt"));
    if (!in)
        return false;
    std::string line;
    while (std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos || line.empty() || line[0] == '#')
            continue;
        table()[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return true;
}

} // namespace ui
