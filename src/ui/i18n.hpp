#pragma once

#include <string>

namespace ui {

// Traducción de la interfaz. Las claves son los textos en español (el idioma base): sin
// archivo de idioma, `tr` devuelve la clave. Los idiomas son `assets/lang/<código>.txt`, con una
// línea `clave<TAB>traducción` por texto.
// Devuelve un puntero estable (vive tanto como la tabla o la clave).
const char* tr(const char* key);
// Carga el idioma `code` ("es" = el base). Devuelve false si no hay archivo.
bool loadLanguage(const std::string& code);

} // namespace ui
