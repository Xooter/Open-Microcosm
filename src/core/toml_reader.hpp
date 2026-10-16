#pragma once

// Lector común de las tablas TOML de los paquetes de contenido (seres, biomas, medios): acumula
// errores con el lugar donde están, en vez de cortar la carga. Solo lo incluyen los .cpp de biosim
// (toml++ es una dependencia privada de la librería).

#include <toml++/toml.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace toml_io {

template <typename E, std::size_t N>
bool parseEnum(std::string_view text, const std::pair<const char*, E> (&table)[N], E& out) {
    for (const auto& [name, value] : table)
        if (text == name) {
            out = value;
            return true;
        }
    return false;
}

template <std::size_t N>
bool known(std::string_view key, const char* const (&keys)[N]) {
    for (const char* k : keys)
        if (key == k)
            return true;
    return false;
}

// Una clave que no esté en la lista es casi siempre un error de tipeo: se avisa, no se ignora.
template <std::size_t N>
void warnUnknown(const toml::table& t, const char* const (&keys)[N], const std::string& where,
                 std::vector<std::string>& warnings) {
    for (const auto& [k, v] : t)
        if (!known(k.str(), keys))
            warnings.push_back(where + ": clave desconocida '" + std::string(k.str()) + "'");
}

struct Reader {
    const toml::table& t;
    std::string where;
    std::vector<std::string> errors;

    template <typename T> void num(const char* key, T& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        if (auto v = n->value<double>())
            out = static_cast<T>(*v);
        else
            errors.push_back(where + ": '" + key + "' tiene que ser un número");
    }
    // Número que no puede ser negativo (cantidades, concentraciones).
    template <typename T> void count(const char* key, T& out) {
        double before = static_cast<double>(out);
        num(key, before);
        if (before < 0.0) {
            errors.push_back(where + ": '" + key + "' no puede ser negativo");
            return;
        }
        out = static_cast<T>(before);
    }
    void flag(const char* key, bool& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        if (auto v = n->value<bool>())
            out = *v;
        else
            errors.push_back(where + ": '" + key + "' tiene que ser true o false");
    }
    void text(const char* key, std::string& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        if (auto v = n->value<std::string>())
            out = *v;
        else
            errors.push_back(where + ": '" + key + "' tiene que ser un texto");
    }
    void range(const char* key, float& lo, float& hi) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        const toml::array* a = n->as_array();
        if (!a || a->size() != 2 || !(*a)[0].value<double>() || !(*a)[1].value<double>()) {
            errors.push_back(where + ": '" + key + "' tiene que ser [mínimo, máximo]");
            return;
        }
        lo = static_cast<float>(*(*a)[0].value<double>());
        hi = static_cast<float>(*(*a)[1].value<double>());
        if (lo > hi)
            errors.push_back(where + ": '" + key + "' tiene el mínimo mayor que el máximo");
    }
    void strings(const char* key, std::vector<std::string>& out) {
        const toml::node* n = t.get(key);
        if (!n)
            return;
        const toml::array* a = n->as_array();
        if (!a) {
            errors.push_back(where + ": '" + key + "' tiene que ser una lista de textos");
            return;
        }
        for (const toml::node& e : *a) {
            if (auto v = e.value<std::string>())
                out.push_back(*v);
            else
                errors.push_back(where + ": '" + key + "' tiene que ser una lista de textos");
        }
    }
    template <typename E, std::size_t N>
    void choice(const char* key, const std::pair<const char*, E> (&table)[N], E& out, bool required = false) {
        const toml::node* n = t.get(key);
        if (!n) {
            if (required)
                errors.push_back(where + ": falta '" + key + "'");
            return;
        }
        auto v = n->value<std::string>();
        if (!v || !parseEnum(*v, table, out)) {
            std::string options;
            for (const auto& [name, value] : table)
                options += std::string(options.empty() ? "" : ", ") + name;
            errors.push_back(where + ": '" + key + "' tiene que ser uno de: " + options);
        }
    }
    // Subtabla opcional (nullptr si no está; error si la clave existe y no es una tabla).
    const toml::table* table(const char* key) {
        const toml::node* n = t.get(key);
        if (!n)
            return nullptr;
        if (const toml::table* sub = n->as_table())
            return sub;
        errors.push_back(where + ": '" + key + "' tiene que ser una tabla");
        return nullptr;
    }
};

// Una definición como texto TOML autónomo (`[[clave]]` con sus subtablas), para exportarla en un
// paquete: se guarda al cargarla, así lo exportado es exactamente lo que se leyó.
inline std::string snippet(const char* arrayKey, const toml::table& t) {
    toml::array list;
    list.push_back(t);
    toml::table root;
    root.insert(arrayKey, std::move(list));
    std::ostringstream out;
    out << root;
    return out.str();
}

// Lo mismo para una tabla con nombre dentro de otra (`[fisiologia.paramecium]`).
inline std::string tableSnippet(const char* parent, std::string_view key, const toml::table& t) {
    toml::table inner;
    inner.insert(key, t);
    toml::table root;
    root.insert(parent, std::move(inner));
    std::ostringstream out;
    out << root;
    return out.str();
}

} // namespace toml_io
