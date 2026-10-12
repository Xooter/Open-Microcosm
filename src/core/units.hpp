#pragma once

// Unidades internas: micrómetros (µm), segundos de simulación (s) y µM para concentraciones.
// Las conversiones a píxeles viven solo en el render.
namespace units {

// Segundos de simulación por segundo real para la FÍSICA (nado, agua, difusión).
inline constexpr float kTimeScale = 1.0f;

// Compresión del tiempo BIOLÓGICO (metabolismo, crecimiento, digestión, división): una sola
// constante, así las proporciones entre procesos se respetan. 60 → 1 minuto real = 1 hora
// biológica: un paramecio bien alimentado se divide en ~5 min reales (real: ~5–8 h).
inline constexpr float kBioTimeScale = 60.0f;

// Fracción seca de la biomasa (~70% de una célula es agua). Las masas se expresan en pmol
// equivalentes de glucosa de materia SECA.
inline constexpr float kDryMassFraction = 0.3f;

inline constexpr double kBoltzmannTemperature = 4.05e-21; // k_B·T a 20 °C [J]
inline constexpr double kWaterDynamicViscosity = 1.0e-3;  // agua a 20 °C [Pa·s]
inline constexpr float kWaterKinematicViscosity = 1.0e6f; // 1e-6 m²/s [µm²/s]

inline constexpr float kDiffusionO2 = 2000.0f;     // O₂ en agua a 20 °C [µm²/s]
inline constexpr float kDiffusionGlucose = 600.0f; // glucosa en agua [µm²/s]
inline constexpr float kO2Saturation = 280.0f;     // O₂ en equilibrio con el aire, 20 °C [µM]
inline constexpr float kDiffusionPhosphate = 800.0f; // fosfato en agua [µm²/s]

inline constexpr double kLitersPerCubicMicrometer = 1e-15;
inline constexpr float kPi = 3.14159265358979323846f;

// Stokes–Einstein: D = kT / (6π μ r). Radio en µm, resultado en µm²/s.
// Una bacteria de 0.5 µm de radio da ~0.4 µm²/s: el browniano es real pero sutil.
constexpr float stokesEinsteinDiffusion(float radiusUm) {
    return static_cast<float>(kBoltzmannTemperature /
                              (6.0 * 3.14159265358979323846 * kWaterDynamicViscosity *
                               radiusUm * 1e-6) *
                              1e12);
}

} // namespace units
