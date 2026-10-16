#pragma once

#include <algorithm>

// Ósmosis (fase 20c): el agua atraviesa la membrana hacia donde hay más solutos. El volumen de una
// célula sigue la osmolaridad del medio según la relación de Boyle–van 't Hoff,
//     V = Vb + (V0 − Vb) · π0 / π,
// donde V0 es el volumen en el medio isotónico (osmolaridad π0) y Vb el volumen osmóticamente
// inactivo (proteínas, membranas). Es física (agua que pasa por acuaporinas), así que corre en
// tiempo real; se acerca al equilibrio con un tiempo característico que da la conductividad
// hidráulica de la membrana (segundos en un eritrocito; minutos en una célula vegetal).

namespace osmosis {

// Osmolaridad del medio [mOsm/L]: las sales disueltas como NaCl equivalente (2 iones, coeficiente
// osmótico ~0,93) más los osmolitos que no atraviesan la membrana (sacarosa, manitol). Da ~16
// mOsm en agua dulce, ~340 en el DMEM y ~1100 en el mar (agua de mar: ~1000–1100 mOsm/kg).
inline float mediumOsmolarity(float salinityGramsPerKg, float extraOsmolytes) {
    constexpr float kNaClMolarMass = 58.44f; // [g/mol]
    constexpr float kOsmoticCoefficient = 0.93f;
    return 2.0f * kOsmoticCoefficient * std::max(0.0f, salinityGramsPerKg) / kNaClMolarMass * 1000.0f +
           std::max(0.0f, extraOsmolytes);
}

// Volumen de equilibrio relativo a V0 (Boyle–van 't Hoff). `inactive`: fracción Vb/V0.
inline float equilibriumVolume(float isotonic, float osmolarity, float inactive) {
    const float pi = std::max(1.0f, osmolarity);
    return inactive + (1.0f - inactive) * isotonic / pi;
}

} // namespace osmosis
