#pragma once

#include "core/vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Tejidos de un preparado (fase 20c): células vegetales con su pared, montadas entre portaobjetos y
// cubreobjetos. La pared es rígida (celulosa) y queda fija; el protoplasto de cada célula viva es
// un cuerpo blando adentro que la ósmosis infla (turgencia) o encoge (plasmólisis). Los tejidos
// fijados (punta de raíz aplastada, papa con Lugol, epidermis de hoja) son paredes con su
// contenido, sin simular.

// Cómo se disponen las células (lo eligen los datos del preparado).
enum class TissueLayout : std::uint8_t {
    Rows,      // hileras de células alargadas desfasadas (epidermis de cebolla, hoja de Elodea)
    Polygonal, // poliedros de tamaños parecidos (parénquima de papa)
    Files,     // filas de células cuadradas chicas (meristema de la punta de raíz)
    Jigsaw,    // células de contorno ondulado con estomas (epidermis de hoja)
};

// Qué se ve adentro de cada célula fija (o además del protoplasto).
enum class TissueContent : std::uint8_t {
    Protoplast, // célula viva: protoplasto con vacuola (y plastos, según la especie)
    Starch,     // amiloplastos: granos de almidón con capas concéntricas (papa)
    Mitosis,    // núcleo en interfase o en alguna fase de la mitosis (punta de raíz)
    Stomata,    // epidermis con estomas: algunas células son pares de células oclusivas
};

struct TissueCell {
    std::vector<Vec2> wall; // contorno interior de la pared (antihorario) [µm]
    Vec2 center;
    float area = 0.0f;
    std::uint32_t protoplast = ~0u; // entidad del protoplasto (célula viva), o ninguna
    float volume = 1.0f;            // volumen del protoplasto / el de la pared llena
    float fullArea = 0.0f;          // área del protoplasto turgente (apretado contra la pared) [µm²]
    std::uint8_t phase = 0;         // Mitosis: 0 interfase, 1 profase, 2 metafase, 3 anafase, 4 telofase
    bool guard = false;             // Stomata: célula oclusiva (con cloroplastos)
    std::uint32_t seed = 0;         // detalles del dibujo (granos, cromosomas)
};

// Componente: protoplasto de la célula `cell` del tejido (vive dentro de su pared).
struct Protoplast {
    std::uint32_t cell = 0;
};

struct Tissue {
    bool enabled = false; // la placa es un preparado con tejido
    TissueLayout layout = TissueLayout::Rows;
    TissueContent content = TissueContent::Protoplast;
    std::string species;           // id del ser que forma el protoplasto (células vivas)
    float cellLength = 200.0f;     // [µm]
    float cellWidth = 50.0f;       // [µm]
    float wallThickness = 1.0f;    // [µm] pared celular (la dibuja el render)
    // Ósmosis del protoplasto (bio/osmosis): osmolaridad del jugo celular con la célula llena,
    // fracción osmóticamente inactiva y tiempo de equilibrio [s reales].
    float sapOsmolarity = 450.0f;  // [mOsm/L]
    float inactive = 0.1f;
    float osmoticSeconds = 120.0f;
    // Mitosis (punta de raíz): fracción de las células en cada fase, de las duraciones publicadas.
    float mitoticFractions[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // profase, metafase, anafase, telofase
    std::vector<TissueCell> cells;
    bool present() const { return !cells.empty(); }
};
