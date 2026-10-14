#pragma once

// Nivel de detalle del render según el tamaño proyectado en pantalla [px] (zoom semántico).
// Sin raylib: lo usan el render, la app (qué se puede elegir con el mouse) y los tests.
// El render solo decide CÓMO se muestra algo; la simulación sigue igual en todos los niveles.

#include "bio/body_plan.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

enum class DetailLevel : std::uint8_t {
    Aggregate,  // demasiado chico o numeroso: se ve como densidad de la población
    Simplified, // individuo sin detalle (bastoncito, silueta)
    Normal,     // morfología reconocible
    Detail,     // estructuras de la célula (envoltura, nucleoide, organelos)
    Subcellular, // estructuras de decenas de nm (ribosomas, capas de la envoltura, fimbrias)
};

// Umbrales [px] del tamaño característico proyectado. Por debajo de `aggregatePx` solo se ve la
// densidad; entre `aggregatePx` y `simplifiedPx` hay fundido cruzado densidad ↔ individuos.
struct LodBands {
    float aggregatePx = 0.0f;
    float simplifiedPx = 0.0f;
    float normalPx = 0.0f;
    float detailPx = 0.0f;
    float subcellularPx = 1e30f;
};

struct LodBlend {
    DetailLevel level = DetailLevel::Normal;
    float individual = 1.0f; // opacidad de los individuos (0–1)
    float aggregate = 0.0f;  // opacidad de la densidad (= 1 − individual)
};

inline DetailLevel detailLevelFor(float sizePx, const LodBands& b) {
    if (sizePx < b.aggregatePx)
        return DetailLevel::Aggregate;
    if (sizePx < b.normalPx)
        return DetailLevel::Simplified;
    if (sizePx < b.detailPx)
        return DetailLevel::Normal;
    if (sizePx < b.subcellularPx)
        return DetailLevel::Detail;
    return DetailLevel::Subcellular;
}

// Fundido en escala logarítmica (cada paso de la rueda multiplica el zoom): suave y sin saltos.
inline LodBlend lodFor(float sizePx, const LodBands& b) {
    LodBlend out;
    out.level = detailLevelFor(sizePx, b);
    if (b.simplifiedPx <= b.aggregatePx) {
        out.individual = sizePx < b.aggregatePx ? 0.0f : 1.0f;
    } else {
        const float lo = std::log(std::max(1e-6f, b.aggregatePx)), hi = std::log(b.simplifiedPx);
        const float t = std::clamp((std::log(std::max(1e-6f, sizePx)) - lo) / (hi - lo), 0.0f, 1.0f);
        out.individual = t * t * (3.0f - 2.0f * t);
    }
    out.aggregate = 1.0f - out.individual;
    return out;
}

// Poblaciones numerosas que tienen representación agregada.
enum class Population : std::uint8_t { Bacteria, Bdellovibrio, Count };
constexpr int kPopulationCount = static_cast<int>(Population::Count);

struct PopulationLod {
    float sizeMicrons = 1.0f; // tamaño característico (el largo) [µm]
    LodBands bands;
};

// Qué se muestra de lo que está por debajo del límite óptico.
enum class ViewMode : std::uint8_t {
    Microscope, // lo que el microscopio de campo claro de la placa podría mostrar
    Model,      // modelo didáctico: también lo que no se resuelve con luz (flagelos, RE, ribosomas)
};

// Toda la configuración del render multiescala en un solo lugar.
struct RenderLodSettings {
    // false: sin zoom semántico (todo individual con el mínimo visible de siempre y las células
    // con todos sus detalles a cualquier tamaño). Sirve para comparar.
    bool enabled = true;
    // Grilla de densidad: celdas de ~binPixels px en pantalla (lado en µm redondeado a potencia de
    // 2, anclado al mundo para que no tiemble al desplazar la cámara), con tope de celdas.
    float binPixels = 6.0f;
    int maxBins = 1 << 16;
    int smoothingPasses = 1;
    bool weightByBiomass = false; // pesar por la masa de cada individuo en vez de contarlos
    // Contraste de la densidad lejana: 1 = turbidez física (Beer–Lambert con el área real de cada
    // individuo); más alto exagera la absorbancia para que se lean las manchas a escala de placa.
    // Se interpola a 1 durante el fundido, así la transición conserva el oscurecimiento medio.
    float densityGain = 6.0f;
    PopulationLod population[kPopulationCount] = {
        // E. coli ~2 µm: densidad lejos (< 1,5 px), bastoncitos desde 4 px, cápsulas desde 10 px,
        // estructuras (envoltura, nucleoide, flagelos) desde 60 px y subcelular desde 600 px.
        {2.0f, {1.5f, 4.0f, 10.0f, 60.0f, 600.0f}},
        // Bdellovibrio ~1 µm.
        {1.0f, {1.5f, 4.0f, 10.0f, 60.0f, 600.0f}},
    };
    ViewMode view = ViewMode::Microscope;
    float opticalResolution = kOpticalResolution; // [µm]
    // Cada estructura aparece cuando su propio tamaño real proyectado lo permite: las filiformes
    // (flagelos, fimbrias, membranas) desde `lineMinPx` de espesor y las partículas (ribosomas)
    // desde `particleMinPx` de diámetro. Así el detalle se revela progresivamente.
    float lineMinPx = 0.8f;
    float particleMinPx = 3.0f;
    // Tope de individuos dibujados con estructuras por frame (los más cercanos al centro de la
    // vista); el resto queda en el bastoncito instanciado.
    int maxDetailed = 256;
    int maxParticlesPerFrame = 60000; // ribosomas y similares, entre todos los individuos
    // El ser elegido sube de nivel (se dibuja con estructuras aunque sea chico en pantalla).
    bool inspectionBoost = true;
    // Grosor de corte del modelo: como un corte fino de microscopía electrónica (~0,1 µm), se
    // dibujan solo las partículas internas de esa rebanada; en proyección se tapan todas.
    float sectionThickness = 0.1f; // [µm]
    DetailLevel cellLevel(float sizeMicrons, float zoom) const {
        return enabled ? detailLevelFor(sizeMicrons * zoom, cells) : DetailLevel::Detail;
    }
    // Células (sin agregado): silueta con membrana por debajo de normalPx; organelos y gránulos
    // desde detailPx. El tamaño es el lado mayor de su caja.
    LodBands cells = {0.0f, 0.0f, 6.0f, 20.0f, 2000.0f};

    const PopulationLod& of(Population p) const { return population[static_cast<int>(p)]; }
    LodBlend blend(Population p, float zoom) const {
        const PopulationLod& pl = of(p);
        if (!enabled) // como antes del zoom semántico: siempre el bastoncito simple
            return {DetailLevel::Simplified, 1.0f, 0.0f};
        return lodFor(pl.sizeMicrons * zoom, pl.bands);
    }
};
