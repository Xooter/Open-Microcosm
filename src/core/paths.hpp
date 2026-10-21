#pragma once

#include <string>

// Rutas del programa (MVP: distribución). Sin raylib: las usan el juego, el bench y los tests.

// Carpeta del ejecutable ("" si no se puede saber).
std::string executableDir();
// Archivo de assets: primero <ejecutable>/assets (instalado), si no la carpeta del código fuente
// (desarrollo, BIO_ASSET_DIR). `relative` como "shaders/microscope_post.fs".
std::string assetPath(const std::string& relative);
// Carpeta de datos del usuario: Linux $XDG_DATA_HOME o ~/.local/share; Windows %APPDATA%;
// macOS ~/Library/Application Support. Termina en /openmicrocosm.
std::string userDataDir();
// Una carpeta dentro de la de datos (no se crea). Si todavía existe con su nombre viejo (en español,
// de antes de que los archivos pasaran al inglés) y la nueva no, se renombra: lo guardado no se pierde.
std::string userDataSubdir(const char* name, const char* legacyName = nullptr);
