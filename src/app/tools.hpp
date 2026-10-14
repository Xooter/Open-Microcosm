#pragma once

#include "game/inspect.hpp"
#include "render/field_overlay.hpp"

#include <imgui.h>

#include <string>

class World;

// Herramienta activa: define qué hace el click izquierdo sobre la placa. Con "Seleccionar" (la
// de siempre) un click elige el ser y abre su ficha.
enum class Tool : int {
    Select,   // elegir el ser bajo el cursor (ficha, tooltip)
    Control,  // tomar el control de una célula y moverla
    Ink,      // tinta (arrastrar)
    Detritus, // un detrito
    Erase,    // quitar la célula
    Lyse,     // lisar la célula
    Squeeze,  // apretar el agua
    Stir,     // revolver el agua (arrastrar)
    Grab,     // estirar la membrana (arrastrar)
    Dropper,  // gotero de solutos (mantener)
    Spawn,    // poner el ser armado en el catálogo
    Count
};

struct ToolInfo {
    const char* glyph; // rótulo corto del botón
    const char* name;  // clave de traducción
    const char* help;  // clave de traducción
    ImGuiKey key;      // atajo (ImGuiKey_None: ninguno)
};
const ToolInfo& toolInfo(Tool t);

// Estado de las herramientas y de la sesión (lo comparten los paneles y el bucle principal).
struct ToolState {
    Tool tool = Tool::Select;
    float stirRadius = 30.0f;     // [µm]
    float inkRadius = 15.0f;      // [µm]
    float squeezeSpeed = 40.0f;   // [µm/s]
    bool grabbing = false;
    bool follow = false;          // la cámara sigue al ser controlado
    bool followSelected = false;  // la cámara sigue al ser elegido
    bool paused = false;
    float simSpeed = 1.0f;        // velocidad de la simulación (múltiplo del tiempo real)
    int stepRequests = 0;         // pasos sueltos pedidos con la simulación en pausa
    Overlay overlay = Overlay::None;
    int stain = 0;                // Stain
    // Gotero: suelta un soluto en el punto del cursor. `dropAmount` es relativo a una
    // concentración típica del soluto.
    int dropper = 0;
    float dropRadius = 40.0f;     // [µm]
    float dropAmount = 1.0f;
    // Totales conservados (se recalculan una vez por segundo) [pmol].
    float statsTimer = 0.0f;
    double totalP = 0.0, totalN = 0.0, totalSi = 0.0, totalFe = 0.0;
    // Profiling continuo (promedios suavizados) [ms].
    float stepMs = 0.0f, renderMs = 0.0f;
};

// Lo elegido con la herramienta Seleccionar.
struct SelectionState {
    Picked picked;           // lo elegido
    Inspection inspection;   // su ficha (las vivas se rearman ~10 veces por segundo)
    float refresh = 0.0f;    // [s] hasta rearmar la ficha
    float pickedAge = 0.0f;  // [s] desde el click (el anillo de una foto se desvanece)
    Picked hover;            // lo que está bajo el cursor
    std::string hoverText;
    float hoverTimer = 0.0f;
    bool followable() const {
        return inspection.valid && !inspection.dead &&
               (picked.kind == Picked::Kind::Cell || picked.kind == Picked::Kind::Rock);
    }
};

// Catálogo de seres (panel Catálogo).
struct CatalogState {
    int armed = -1;       // índice del catálogo listo para spawnear (-1: ninguno)
    int category = 0;     // pestaña abierta (BeingCategory)
    bool reshape = false; // transformar al ser controlado en la especie elegida
};

// Gotero de solutos.
const char* dropperName(int index);
int dropperCount();
void applyDropper(World& world, const ToolState& tools, float x, float y, float dt);
