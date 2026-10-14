#pragma once

#include "app/tools.hpp"
#include "game/plate_spec.hpp"
#include "game/progress.hpp"
#include "game/stain.hpp"
#include "game/run.hpp"
#include "game/world.hpp"
#include "gpu/gpu_substep_solver.hpp"
#include "render/renderer.hpp"
#include "ui/commands.hpp"
#include "ui/panel.hpp"
#include "ui/stain_preview.hpp"

#include <raylib.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// La aplicación: un laboratorio con una placa de Petri, paneles acoplables y herramientas. No hay
// pantalla de inicio: arranca con una placa vacía lista para preconfigurar y poblar.
struct App {
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Bucle principal; devuelve el código de salida.
    int run();

    // --- Placa ---
    // Crea el mundo desde `plate` (descarta el anterior).
    void createPlate();
    // La placa en pantalla ya no coincide con `plate` en algo que solo se aplica al crearla.
    bool plateDirty = false;
    std::uint64_t worldSeed = 0; // seed y bioma de la placa en pantalla
    int worldBiome = 0;
    // Cambios del editor que se aplican en vivo sobre el mundo (si `livePlate`).
    bool livePlate = true;
    void pushMediumLive();                          // temperatura, salinidad, luz, anoxia, alcalinidad
    void pushNutrientLive(int soluteIndex, float target); // lleva la concentración media de un soluto a `target`
    // Nombre con el que se guarda la placa.
    std::string plateName = "mi_placa";
    std::string platesDir = defaultPlatesDir();

    // --- Estado ---
    Options options;
    std::string optionsPath = defaultOptionsPath();
    Progress progress;
    std::string progressPath = defaultProgressPath();
    PlateSpec plate;
    std::unique_ptr<World> world;
    Run session;
    ToolState tools;
    SelectionState selection;
    CatalogState catalog;
    Camera2D camera{};
    Rectangle viewport{0, 0, 1600, 900}; // el centro libre del dockspace: donde se ve la placa

    // --- Tinciones (solo visuales) ---
    std::vector<StainSpec> stains;   // de fábrica y del usuario; la 0 es "sin tinción"
    int stainIndex = 0;              // la activa en la placa
    std::string stainsDir = defaultStainsDir();
    ui::StainPreview stainPreview;
    const StainSpec* activeStain() const { return stainIndex > 0 && stainIndex < static_cast<int>(stains.size()) ? &stains[static_cast<std::size_t>(stainIndex)] : nullptr; }
    void setStainIndex(int i) { stainIndex = std::clamp(i, 0, static_cast<int>(stains.size()) - 1); }
    void reloadStains();             // de fábrica + carpeta del usuario

    // --- Interfaz ---
    ui::Commands commands;
    ui::PanelHost panels;
    bool quit = false;
    void setTool(Tool t);
    void setFullscreen(bool on);
    void applyOptions(); // post-proceso, LOD, GPU
    void saveOptions() const;
    void requestLayoutReset() { layoutReset_ = true; }
    // Avisos breves (descubrimientos de la enciclopedia).
    std::vector<std::string> toasts;
    float toastTimer = 0.0f;

    std::unique_ptr<Renderer> renderer;
    GpuSubstepSolver gpu;
    bool gpuOk = false;

  private:
    void registerCommands();
    void buildUi(float dt);
    void drawToolbar();
    void drawStatusBar();
    void drawToasts();
    void worldInput(float dt);
    void selectionInput(float dt, const Vector2& mouseWorld, const Vec2& pos);
    void toolInput(float dt, const Vec2& pos);
    void controlInput(bool mouseFree, const Vec2& pos);
    void updateCamera(bool mouseFree);
    void stepSimulation(float dt);
    void discover(float dt);
    void drawWorldOverlays();
    Vec2 worldPos(const Vector2& m) const;

    float accumulator_ = 0.0f;
    float discoveryTimer_ = 0.0f;
    std::uint32_t runCounter_ = 0;
    std::string layoutPath_;
    bool layoutReset_ = false;
    bool panning_ = false;
    bool dockBuilt_ = false;
};
