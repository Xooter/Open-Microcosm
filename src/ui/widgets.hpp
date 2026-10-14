#pragma once

#include <string>

namespace ui {

// Widgets de parámetros con el estilo del laboratorio: rótulo a la izquierda (traducido), control
// a la derecha, ayuda al pasar el mouse y valor tipeable con Ctrl+click o doble click.
// `shown` multiplica el valor para mostrarlo en otra unidad (p. ej. µM → pM).
// Devuelven true si el valor cambió.
bool slider(const char* label, float* value, float lo, float hi, const char* fmt = "%.2f", const char* help = nullptr,
            float shown = 1.0f);
bool sliderLog(const char* label, float* value, float lo, float hi, const char* fmt = "%.2f", const char* help = nullptr,
               float shown = 1.0f);
bool sliderInt(const char* label, int* value, int lo, int hi, const char* help = nullptr);
bool checkbox(const char* label, bool* value, const char* help = nullptr);
// Combo con opciones separadas en un arreglo de textos (se traducen).
bool combo(const char* label, int* index, const char* const* items, int count, const char* help = nullptr);
// Fila de solo lectura: rótulo y valor.
void readout(const char* label, const char* fmt, ...);
// Barra de progreso con texto.
void bar(const char* label, float fraction, const char* text);
// Sección plegable. Devuelve true si está abierta (si es así, hay que cerrarla con nada: no
// usa pila). `id` estable aunque cambie el idioma.
bool section(const char* title, bool openByDefault = false);
// Texto atenuado que se parte en renglones.
void note(const char* text);
// "(?)" con ayuda emergente.
void helpMarker(const char* text);
// Ayuda emergente del último ítem.
void tooltip(const char* text);
// Botón cuadrado de herramienta, marcado si `active`.
bool toolButton(const char* label, bool active, const char* help);

} // namespace ui
