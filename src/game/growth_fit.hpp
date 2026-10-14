#pragma once

#include <cstddef>

// Curva de crecimiento automática (fase 13): ajusta una serie N(t) de una población.
//
//  - Tasa específica máxima µmax [1/h]: la mayor pendiente de ln N en una ventana deslizante
//    (regresión lineal), como se mide en un cultivo en fase exponencial.
//  - Tiempo de duplicación: ln 2 / µmax.
//  - Latencia λ [h]: donde la tangente de la fase exponencial corta el nivel inicial ln N₀
//    (definición de Pirt; Zwietering et al. 1990).
//  - Capacidad de carga K: ajuste logístico (Verhulst) N = K / (1 + e^(c − r·t)). Para cada K
//    candidato, ln(K/N − 1) es lineal en t; se busca el K que minimiza el error en ln N
//    (búsqueda de sección áurea, determinista).
struct GrowthFit {
    bool valid = false;     // hay crecimiento medible
    float muMax = 0.0f;     // [1/h]
    float doublingHours = 0.0f;
    float lagHours = 0.0f;
    float tangentIntercept = 0.0f; // ln N = intercepto + µmax·t (para dibujar la tangente)
    bool logisticValid = false;
    float capacity = 0.0f;  // K [individuos]
    float logisticRate = 0.0f, logisticShift = 0.0f; // r [1/h] y c
    bool plateau = false;   // la población ya está cerca de K (K estimado con la meseta a la vista)
    float logistic(float t) const;
};

// `t` en horas biológicas (creciente), `n` en individuos. Las muestras con N ≤ 0 se ignoran.
GrowthFit fitGrowth(const float* t, const float* n, std::size_t count);
