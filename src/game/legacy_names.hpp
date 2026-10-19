#pragma once

#include <string>
#include <string_view>

// Nombres del formato viejo (en español) de lo que el usuario guardó: escenas de cultivo, programas,
// experimentos y claves de parámetros en las escenas de placa. Solo se leen; todo se escribe con los
// nombres actuales. Los ids del contenido viejos los resuelve cada registro (`legacy_ids`, core/ids).

// El nombre actual de una clave, una operación, un soluto o una forma de programa ("luz" → "light");
// el mismo si no es un nombre viejo.
std::string_view currentName(std::string_view name);
// Lo mismo para un destino de programa o un factor con prefijo ("agregar:fosfato" → "add:phosphate").
std::string currentTarget(std::string_view target);
// La clave actual de un parámetro del registro ("medio.alcalinidad" → "medium.alkalinity"); vacío si
// no es una clave vieja.
std::string_view currentParameterKey(std::string_view key);
