#pragma once

// Traducción de los textos que arma biosim (la ficha de game/inspect). biosim no conoce la
// interfaz: la UI instala su `ui::tr` al cargar el idioma; sin ella (tests, bench) queda el texto
// base en español. No toca la simulación.
using Localizer = const char* (*)(const char* key);

inline Localizer& localizerSlot() {
    static Localizer fn = nullptr;
    return fn;
}
inline void setLocalizer(Localizer fn) { localizerSlot() = fn; }
inline const char* localize(const char* key) { return localizerSlot() ? localizerSlot()(key) : key; }
