#pragma once

#include <string>

namespace ui {

// Traducción de la interfaz. Las claves son los textos en español (el idioma base): sin
// archivo de idioma, `tr` devuelve la clave. Los idiomas son `assets/lang/<código>.txt`, con una
// línea `clave<TAB>traducción` por texto.
// Devuelve un puntero estable (vive tanto como la tabla o la clave).
const char* tr(const char* key);
// Carga el idioma `code` ("es" = el base): assets/lang/<code>.txt y, de cada paquete de contenido
// cargado que sea una carpeta, su lang/<code>.txt (los textos de sus seres, medios y lecciones).
// Si un texto en español cambia, su traducción deja de coincidir y se muestra en español (así una
// traducción vieja nunca dice otra cosa). Devuelve false si no hay archivo.
bool loadLanguage(const std::string& code);
const std::string& currentLanguage();
// Con la variable de entorno BIO_TR_FALTANTES=<archivo>, `tr` anota las claves sin traducción y
// esta función las escribe (una por línea): así se completa un idioma recorriendo los paneles.
void writeMissingKeys();

} // namespace ui
