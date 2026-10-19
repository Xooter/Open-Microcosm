#pragma once

#include <string>
#include <string_view>
#include <vector>

// Glosario (fase P1, paso 14): términos de la biología y la física de la gota (Reynolds,
// quimiotaxis, Redfield…) como datos de los paquetes (glossary/, [[term]]). Cada término
// tiene su definición, una fuente, sus otras formas en el texto (`alias`), términos relacionados y,
// si existe, dónde verlo en la simulación: una lección o una placa preparada. La "animación" del
// término es la placa misma (no hay dibujos aparte de lo que se simula).
struct GlossaryTerm {
    std::string key, term, definition, source, pack;
    std::vector<std::string> aliases; // formas en minúsculas que se buscan en los textos (incluye el término)
    std::vector<std::string> related; // ids de otros términos
    std::string lesson;               // id de una lección donde se ve ("" ninguna)
    std::string plate;                // id de una placa preparada donde se ve ("" ninguna)
};

int glossaryCount();
const GlossaryTerm& glossaryTerm(int index);
int findGlossaryTerm(std::string_view key); // -1 si no está
bool loadGlossaryText(const std::string& text, const std::string& source, const std::string& pack,
                      std::string* error = nullptr);
const std::vector<std::string>& glossaryLoadWarnings();

// Minúsculas para comparar: ASCII y las mayúsculas acentuadas de Latin-1 en UTF-8 (Á → á, Ñ → ñ).
std::string foldCase(std::string_view text);
// ¿`word` (ya en minúsculas) aparece en `folded` como palabra entera (no dentro de otra)?
bool containsWord(std::string_view folded, std::string_view word);
// Los términos que aparecen en `text`, en orden de aparición y sin repetir. `extra` suma formas por
// término (las traducidas: el texto puede estar en otro idioma); vacío o del tamaño del glosario.
std::vector<int> glossaryTermsIn(std::string_view text, const std::vector<std::vector<std::string>>& extra = {});
