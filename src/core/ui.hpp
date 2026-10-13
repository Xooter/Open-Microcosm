#pragma once

#include "game/biome.hpp"
#include "core/lab_panel.hpp"
#include "render/field_overlay.hpp"

#include "bio/locomotion.hpp"
#include "game/inspect.hpp"
#include "game/sandbox.hpp"

#include <raylib.h>

#include <optional>
#include <string>
#include <vector>

class World;
struct Progress;

struct Options;

// Pantalla de inicio (dibuja un frame completo).
enum class TitleAction { None, NewDrop, Encyclopedia, Options, Quit };
TitleAction drawTitleScreen(const Progress& progress);

// Preparar la gota: bioma, especie fundadora y semilla.
struct SetupState {
    int biome = 0;
    LocomotionType founder = LocomotionType::Cilia;
    unsigned seed = 1;
    DropContent content = DropContent::Full; // qué hay en la gota al empezar
};
enum class SetupAction { None, Start, Back };
SetupAction drawSetupScreen(const Progress& progress, SetupState& state);

// Enciclopedia: especies vistas y comportamientos observados. Devuelve true para volver.
bool drawEncyclopedia(const Progress& progress, float& scroll);
// Opciones (se aplican y guardan desde main). Devuelve true para volver.
bool drawOptions(Options& options);
// Avisos breves (descubrimientos) en la esquina.
void drawToasts(const std::vector<std::string>& lines, float alpha);
// Genoma del ser controlado y avisos de conjugación (esquina superior derecha).
// `showGenome`: dibujarlo (el modo Explorar lo lleva dentro de la ficha).
void drawControlHud(const World& world, bool showGenome = true);
// Modo sandbox: barra de íconos con todos los seres.
struct SandboxState {
    int armed = -1;       // índice del catálogo listo para spawnear (-1: ninguno)
    int category = 0;     // pestaña abierta (BeingCategory)
    bool reshape = false; // modo Forma: el ícono transforma al jugador en esa especie
};
// true si el mouse está sobre la barra (los clicks no van al mundo).
bool sandboxBarHovered();
// Dibuja la barra y el fantasma del ser elegido bajo el cursor. Los clicks en los íconos
// arman el spawn o (modo Forma) cambian la especie del jugador; el botón fuerza su reproducción.
// Devuelve true si se pidió pasar al modo Explorar.
bool drawSandboxBar(World& world, SandboxState& state, const Camera2D& camera);

// Modo Explorar: click sobre cualquier cosa y se abre su ficha (qué es, qué hace y por qué).
struct ExploreState {
    bool active = false;
    Picked picked;           // lo inspeccionado
    Inspection inspection;   // su ficha (las vivas se rearman ~10 veces por segundo)
    float refresh = 0.0f;    // [s] hasta rearmar la ficha
    float scroll = 0.0f;     // [px] de la ficha
    float pickedAge = 0.0f;  // [s] desde el click (el anillo de una foto se desvanece)
    Picked hover;            // lo que está bajo el cursor
    std::string hoverText;
    float hoverTimer = 0.0f;
};
// ¿El mouse está sobre la ficha o sobre los botones del modo? (los clicks no van al mundo).
bool exploreUiHovered(const ExploreState& state);
// Dibuja el modo Explorar: resaltado del inspeccionado, tooltip, banner y ficha. Los botones
// de la ficha (cerrar, seguir) actúan sobre `state` y `tools`. Devuelve true si se pidió salir.
bool drawExplore(const World& world, ExploreState& state, ToolState& tools, const Camera2D& camera);
