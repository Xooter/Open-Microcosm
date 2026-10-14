#pragma once

#include "bio/body_plan.hpp"
#include "core/vec2.hpp"
#include "render/lod.hpp"

#include <raylib.h>

#include <cstdint>

// Estructuras biológicas a escala real (envoltura, nucleoide, ribosomas, flagelos, fimbrias, fagos,
// retículo y Golgi), según el plan corporal de cada ser (bio/body_plan) y su estado simulado.
//
// Marcos anidados: cada ser se dibuja en su propio marco local (origen en su centro, eje x a lo
// largo de su rumbo, unidades µm) con rlPushMatrix; sus partes van en submarcos (el fago sobre la
// envoltura, la cola sobre el fago). Así las estructuras de nm se componen sin tocar las coordenadas
// del mundo, y la forma, posición y orientación son las del mismo individuo en todos los niveles.
//
// Cada estructura se revela sola cuando su tamaño REAL proyectado alcanza unos píxeles
// (`RenderLodSettings::lineMinPx` / `particleMinPx`), y solo si es visible en campo claro o se está
// en la vista de modelo. Lo declarado (no simulado) se ubica con un hash del id estable del ser:
// siempre en el mismo lugar, sin azar de un frame a otro.

struct StructureContext {
    const RenderLodSettings* lod = nullptr;
    float zoom = 1.0f;     // px por µm
    float time = 0.0f;     // tiempo de simulación [s]
    Rectangle view{};      // vista en µm del mundo (recorte de partículas)
    int particleBudget = 0; // ribosomas y similares que quedan por dibujar en este frame

    bool model() const { return lod->view == ViewMode::Model; }
    bool allowed(const StructureSpec& s) const { return model() || visibleInBrightfield(s, lod->opticalResolution); }
    bool line(float widthMicrons) const { return widthMicrons * zoom >= lod->lineMinPx; }
    bool particle(float diameterMicrons) const { return diameterMicrons * zoom >= lod->particleMinPx; }
};

// Estado de una bacteria que el dibujo necesita (todo leído de la simulación).
struct BacteriumState {
    Vec2 at;
    float heading = 0.0f;
    float length = 2.0f, width = 1.0f; // [µm]
    std::uint32_t id = 0;
    bool tumbling = false;
    float phageFraction = -1.0f;  // avance del período latente del fago (0–1; < 0: sana)
    int phageBurst = 0;
    float bdelloFraction = -1.0f; // avance del crecimiento del Bdellovibrio adentro (0–1; < 0: libre)
    int bdelloProgeny = 0;
    Color body{};                 // color del citoplasma (el mismo del bastoncito instanciado)
};

void drawBacteriumStructures(const BacteriumState& b, const BodyPlan& plan, StructureContext& ctx);
// Bdellovibrio libre: vibrio con flagelo polar envainado.
void drawBdellovibrioStructures(const BacteriumState& b, const BodyPlan& plan, StructureContext& ctx);
// Fago con la base en el origen del marco actual y la cabeza hacia +x. `contracted`: la vaina de la
// cola se contrajo (inyectó el ADN y la cabeza quedó vacía).
void drawPhage(const BodyPlan& plan, bool contracted, StructureContext& ctx);
// Retículo endoplasmático y Golgi junto al núcleo de una célula eucariota (estructuras declaradas:
// un representante de cada una, anclado al núcleo simulado).
void drawEndomembranes(const BodyPlan& plan, Vec2 nucleus, float nucleusRadius, Vec2 heading, std::uint32_t seed,
                       StructureContext& ctx);
