#pragma once

#include "game/inspect.hpp"
#include "render/field_overlay.hpp"

#include <imgui.h>

#include <string>
#include <vector>

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
    Measure,  // regla: distancia (arrastrar) o área (lazo) en µm
    Region,   // dibujar la región que se cuantifica (lazo)
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
    // Gotero: suelta un soluto en el punto del cursor. `dropAmount` es relativo a una
    // concentración típica del soluto.
    int dropper = 0;
    float dropRadius = 40.0f;     // [µm]
    float dropAmount = 1.0f;
    // Medición (fase 13). Los trazos están en µm del mundo.
    enum class MeasureMode : int { Distance, Area };
    MeasureMode measureMode = MeasureMode::Distance;
    bool scaleBar = true;            // barra de escala sobre la imagen
    bool limitationMap = false;      // colorear cada célula por lo que limita su crecimiento
    bool semanticZoom = true;        // de lejos, las poblaciones numerosas se ven como densidad
    bool modelView = false;          // vista de modelo: estructuras por debajo del límite óptico
    std::vector<Vec2> measurePath;   // regla: 2 puntos (distancia) o el lazo (área)
    bool measuring = false;          // arrastrando la regla
    std::vector<Vec2> lassoPath;     // región en curso (mientras se dibuja)
    bool drawingRegion = false;
    // Profiling continuo (promedios suavizados) [ms]: un paso, el dibujo del mundo, la interfaz
    // y la simulación de todo el cuadro (todos sus pasos).
    float stepMs = 0.0f, renderMs = 0.0f, uiMs = 0.0f, simFrameMs = 0.0f;
    // Duración de los últimos cuadros [ms] (anillo; `frameHead` es el próximo a escribir).
    static constexpr int kFrameHistory = 240;
    float frameHistory[kFrameHistory] = {};
    int frameHead = 0;
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
               (picked.kind == Picked::Kind::Cell || picked.kind == Picked::Kind::Rock ||
                picked.kind == Picked::Kind::Bacterium || picked.kind == Picked::Kind::Bdellovibrio);
    }
};

// Catálogo de seres (panel Catálogo).
struct CatalogState {
    int armed = -1;       // índice del catálogo listo para spawnear (-1: ninguno)
    int category = 0;     // pestaña abierta (BeingCategory; kCategoryCount = "Todos")
    bool reshape = false; // transformar al ser controlado en la especie elegida
};

// Gotero de solutos.
const char* dropperName(int index);
int dropperCount();
void applyDropper(World& world, const ToolState& tools, float x, float y, float dt);
