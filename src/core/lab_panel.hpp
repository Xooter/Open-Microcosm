#pragma once

#include "render/field_overlay.hpp"

#include <raylib.h>

class World;
class Run;

// Estado de las herramientas del laboratorio (lo comparten el panel, la barra y el main).
struct ToolState {
    float stirRadius = 30.0f;     // [µm]
    float inkRadius = 15.0f;      // [µm]
    float squeezeSpeed = 40.0f;   // [µm/s]
    bool grabbing = false;
    bool follow = true;           // la cámara sigue al ser controlado
    bool followInspected = false; // modo Explorar: la cámara sigue al ser inspeccionado
    bool showLab = true;          // panel de laboratorio (H para ocultar)
    bool paused = false;
    Overlay overlay = Overlay::None;
    int stain = 0;                // Stain
    // Gotero (G): suelta un soluto en el punto del cursor. `dropAmount` es relativo a una
    // concentración típica del soluto (ver kDroppers).
    int dropper = 0;
    float dropRadius = 40.0f;     // [µm]
    float dropAmount = 1.0f;
    // Panel: scroll, secciones abiertas y el último alto del contenido.
    float labScroll = 0.0f, labContent = 0.0f;
    unsigned openSections = 0b0000'0000'0000'0111; // simulación, ambiente y macronutrientes
    // Totales conservados (se recalculan una vez por segundo) [pmol].
    float statsTimer = 0.0f;
    double totalP = 0.0, totalN = 0.0, totalSi = 0.0, totalFe = 0.0;
    // Profiling continuo (promedios suavizados) [ms].
    float stepMs = 0.0f, renderMs = 0.0f;
};

// Rectángulo que ocupa el panel (el mundo no recibe clicks adentro).
Rectangle labPanelRect();
// ¿Está el mouse sobre el panel visible?
bool labPanelHovered(const ToolState& tools);

// Panel de laboratorio: todo lo que se puede medir y cambiar de la gota, en secciones.
void drawLabPanel(World& world, const Run& run, ToolState& tools);

// Suelta el soluto elegido en `at` mientras se mantiene G (`dt` real [s]).
void applyDropper(World& world, const ToolState& tools, float x, float y, float dt);
const char* dropperName(int index);
int dropperCount();
