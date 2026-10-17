#pragma once

#include <cstddef>
#include <vector>

// Dosis-respuesta (fase P1, paso 11): lo que se mide al final de un lote con la dosis como factor.
// Log-logística de cuatro parámetros (Hill): y = abajo + (arriba − abajo) / (1 + (d / CI50)^h). Con
// los extremos de los datos como asíntotas; CI50 y h por mínimos cuadrados (búsqueda en grilla y
// sección áurea, determinista). Las dosis 0 entran como control (asíntota de arriba).
struct DoseFit {
    bool valid = false;
    double ic50 = 0.0, hill = 1.0, top = 0.0, bottom = 0.0, r2 = 0.0;
    double operator()(double dose) const;
};
DoseFit fitDoseResponse(const std::vector<double>& doses, const std::vector<double>& responses);

// CIM como en la dilución en caldo: la menor dosis sin crecimiento visible (respuesta por debajo de
// `visible`). 0 si todas crecen.
double minimumInhibitory(const std::vector<double>& doses, const std::vector<double>& responses, double visible);
