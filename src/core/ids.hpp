#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Búsqueda de una definición de contenido por su id. Un id renombrado sigue valiendo con su nombre
// viejo (`legacy_ids` en los datos: las placas, escenas y experimentos guardados lo nombran así).
// Primero se buscan los ids actuales de todos, así un id viejo de uno no tapa el actual de otro.
// `list` es cualquier secuencia de elementos con `key` y `legacyIds`. -1 si no está.
template <typename List> std::ptrdiff_t indexOfId(const List& list, std::string_view key) {
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].key == key)
            return static_cast<std::ptrdiff_t>(i);
    for (std::size_t i = 0; i < list.size(); ++i)
        for (const std::string& old : list[i].legacyIds)
            if (old == key)
                return static_cast<std::ptrdiff_t>(i);
    return -1;
}
