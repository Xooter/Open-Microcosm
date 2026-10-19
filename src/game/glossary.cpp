#include "game/glossary.hpp"

#include "core/pack_files.hpp"
#include "core/toml_reader.hpp"

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <utility>

namespace {

struct Registry {
    std::deque<GlossaryTerm> terms;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    std::atomic<bool> ready{false};
};
Registry& registry() {
    static Registry r;
    return r;
}

constexpr const char* kKeys[] = {"id", "term", "definition", "source", "alias", "related", "lesson", "plate"};

void strings(const toml::table& t, const char* key, std::vector<std::string>& out) {
    if (const toml::array* a = t[key].as_array())
        for (const toml::node& n : *a)
            if (auto s = n.value<std::string>())
                out.push_back(*s);
}

void loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    Registry& reg = registry();
    const toml::array* list = doc["term"].as_array();
    if (!list)
        return;
    for (const toml::node& node : *list) {
        const toml::table* t = node.as_table();
        if (!t)
            continue;
        GlossaryTerm g;
        g.pack = pack;
        toml_io::Reader rd{*t, source + " [[term]]", {}};
        rd.text("id", g.key);
        const std::string where = source + " termino '" + g.key + "'";
        toml_io::warnUnknown(*t, kKeys, where, reg.warnings);
        rd.text("term", g.term);
        rd.text("definition", g.definition);
        rd.text("source", g.source);
        rd.text("lesson", g.lesson);
        rd.text("plate", g.plate);
        strings(*t, "alias", g.aliases);
        strings(*t, "related", g.related);
        reg.warnings.insert(reg.warnings.end(), rd.errors.begin(), rd.errors.end());
        if (g.key.empty() || g.term.empty() || g.definition.empty()) {
            reg.warnings.push_back(where + ": sin id, término o definición: se saltea");
            continue;
        }
        // El término mismo también se busca; todo en minúsculas.
        g.aliases.insert(g.aliases.begin(), g.term);
        for (std::string& a : g.aliases)
            a = foldCase(a);
        std::sort(g.aliases.begin(), g.aliases.end());
        g.aliases.erase(std::unique(g.aliases.begin(), g.aliases.end()), g.aliases.end());
        if (std::any_of(reg.terms.begin(), reg.terms.end(), [&](const GlossaryTerm& o) { return o.key == g.key; })) {
            reg.warnings.push_back(where + ": id repetido (queda el primero)");
            continue;
        }
        reg.terms.push_back(std::move(g));
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
    for (const auto& file : packFolderFiles(basePackDir(), "glossary")) {
        const std::string path = file.string();
        try {
            loadParsed(toml::parse_file(path), path, "base");
        } catch (const toml::parse_error& e) {
            reg.warnings.push_back("no se pudo cargar " + path + ": " + std::string(e.description()));
        }
    }
    reg.ready.store(true, std::memory_order_release);
}

// Un byte que forma parte de una palabra: letras ASCII, dígitos y cualquier byte de UTF-8 multibyte
// (las letras acentuadas). El guion une palabras compuestas ("quorum-sensing" no es "quorum").
bool wordByte(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c >= 0x80 || c == '-';
}

// Dónde aparece primero `word` como palabra entera en `folded` (npos si no aparece).
std::size_t firstWord(std::string_view folded, std::string_view word) {
    if (word.empty())
        return std::string_view::npos;
    for (std::size_t at = folded.find(word); at != std::string_view::npos; at = folded.find(word, at + 1)) {
        const std::size_t end = at + word.size();
        if ((at == 0 || !wordByte(static_cast<unsigned char>(folded[at - 1]))) &&
            (end >= folded.size() || !wordByte(static_cast<unsigned char>(folded[end]))))
            return at;
    }
    return std::string_view::npos;
}

} // namespace

int glossaryCount() {
    ensureBase();
    return static_cast<int>(registry().terms.size());
}
const GlossaryTerm& glossaryTerm(int index) {
    ensureBase();
    return registry().terms[static_cast<std::size_t>(index)];
}
int findGlossaryTerm(std::string_view key) {
    ensureBase();
    for (std::size_t i = 0; i < registry().terms.size(); ++i)
        if (registry().terms[i].key == key)
            return static_cast<int>(i);
    return -1;
}
bool loadGlossaryText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
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
const std::vector<std::string>& glossaryLoadWarnings() {
    ensureBase();
    return registry().warnings;
}

std::string foldCase(std::string_view text) {
    std::string out(text);
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned char& c = reinterpret_cast<unsigned char&>(out[i]);
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<unsigned char>(c + 32);
        } else if (c == 0xC3 && i + 1 < out.size()) {
            // U+00C0–U+00DE (menos ×, U+00D7): la minúscula está 0x20 más arriba.
            unsigned char& d = reinterpret_cast<unsigned char&>(out[i + 1]);
            if (d >= 0x80 && d <= 0x9E && d != 0x97)
                d = static_cast<unsigned char>(d + 0x20);
            ++i;
        }
    }
    return out;
}

bool containsWord(std::string_view folded, std::string_view word) { return firstWord(folded, word) != std::string_view::npos; }

std::vector<int> glossaryTermsIn(std::string_view text, const std::vector<std::vector<std::string>>& extra) {
    ensureBase();
    const std::string folded = foldCase(text);
    std::vector<std::pair<std::size_t, int>> found; // dónde aparece primero, término
    const Registry& reg = registry();
    for (std::size_t i = 0; i < reg.terms.size(); ++i) {
        std::size_t first = std::string::npos;
        auto look = [&](const std::string& alias) { first = std::min(first, firstWord(folded, alias)); };
        for (const std::string& a : reg.terms[i].aliases)
            look(a);
        if (i < extra.size())
            for (const std::string& a : extra[i])
                look(foldCase(a));
        if (first != std::string::npos)
            found.emplace_back(first, static_cast<int>(i));
    }
    std::sort(found.begin(), found.end());
    std::vector<int> out;
    out.reserve(found.size());
    for (const auto& f : found)
        out.push_back(f.second);
    return out;
}
