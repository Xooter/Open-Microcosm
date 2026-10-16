#pragma once

#include "app/tools.hpp"
#include "game/culture_run.hpp"
#include "game/export.hpp"
#include "game/parameters.hpp"
#include "game/plate_spec.hpp"
#include "game/progress.hpp"
#include "game/stain.hpp"
#include "game/run.hpp"
#include "game/scale_path.hpp"
#include "game/world.hpp"
#include "gpu/gpu_substep_solver.hpp"
#include "render/renderer.hpp"
#include "ui/commands.hpp"
#include "ui/panel.hpp"
#include "ui/stain_preview.hpp"

#include <raylib.h>

struct ImDrawList;

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
    // Cambia un parámetro del registro (game/parameters) en la placa en marcha.
    void setParameter(const ParamInfo& p, float value);
    // Toda acción que cambia el mundo pasa por acá: se aplica entre pasos y queda en el registro
    // de la sesión con su paso (reproducible). `interveneState` solo anota si el estado cambió.
    bool intervene(const Intervention& iv) { return world && session.interventions().record(*world, iv); }
    void interveneState(const Intervention& iv) {
        if (world)
            session.interventions().recordState(*world, iv);
    }
    void pushNutrientLive(int soluteIndex, float target); // lleva la concentración media de un soluto a `target`
    // Exporta la serie (CSV), la corrida (JSON) y las fichas de especies a `exportsDir`.
    void exportData();
    RunProvenance provenance() const; // motor, GPU y LOD de la placa en pantalla
    std::string exportsDir = defaultExportsDir();
    std::string lastExport; // ruta base de la última exportación ("" si falló)
    // Nombre con el que se guarda la placa.
    std::string plateName = "mi_placa";
    std::string platesDir = defaultPlatesDir();
    // Se cargó contenido nuevo (un paquete importado): el atlas de íconos del catálogo se rehace
    // al empezar el cuadro siguiente (no se puede dibujar en una textura en medio de la UI).
    bool contentChanged = false;

    // --- Cultivos bien mezclados (fase P1): la gradilla, cada uno con su tiempo ---
    std::vector<std::unique_ptr<CultureRun>> cultures;
    int cultureIndex = -1;
    float cultureSpeed = 2.0f; // horas de cultivo por segundo real
    bool culturesPaused = false;
    void stepCultures(float dt);
    // La gota de muestra de un cultivo pasa a ser la placa del centro.
    void openSampleDrop(const PlateSpec& drop);

    // --- Estado ---
    Options options;
    std::string optionsPath = defaultOptionsPath();
    Progress progress;
    std::string progressPath = defaultProgressPath();
    PlateSpec plate;
    PlateSpec runPlate;                    // la placa con la que se creó el mundo en pantalla
    std::vector<Intervention> plateScript; // intervenciones de la escena cargada (se repiten al crear)
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
    void applyOptions(); // post-proceso, escala, VSync, hilos, LOD, GPU
    int autoThreads = 1;  // hilos de "automático" (OMP_NUM_THREADS o un hilo por núcleo físico)
    void saveOptions() const;
    void requestLayoutReset() { layoutReset_ = true; }
    // Avisos breves (descubrimientos de la enciclopedia).
    std::vector<std::string> toasts;
    float toastTimer = 0.0f;
    void notify(const std::string& text);

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
    std::uint32_t pickSkip() const;
    void toolInput(float dt, const Vec2& pos);
    void controlInput(bool mouseFree, const Vec2& pos);
    void updateCamera(bool mouseFree);
    void stepSimulation(float dt);
    void discover(float dt);
    void drawWorldOverlays();
    void drawMeasurements();
    Vec2 worldPos(const Vector2& m) const;

    void applyFrameCap();  // tope de FPS (y el del ahorro en segundo plano)
    int appliedFps_ = -1;
    // Autoguardado (MVP: estabilidad): la escena de la sesión (placa + intervenciones) cada pocos
    // minutos; si el programa se cerró mal, se ofrece recuperarla re-simulando hasta ese paso.
    void autosave(bool clean);
    void drawRecoveryPrompt();
    std::string autosavePath_;
    float autosaveTimer_ = 0.0f;
    bool recoveryPrompt_ = false;      // hay una sesión anterior sin cerrar bien
    std::uint64_t recoveryStep_ = 0;   // paso guardado
    std::uint64_t recoverTarget_ = 0;  // > 0: re-simulando hasta este paso (sin dibujar ni registrar)
    float accumulator_ = 0.0f;
    float discoveryTimer_ = 0.0f;
    std::uint32_t runCounter_ = 0;
    std::string layoutPath_;
    bool layoutReset_ = false;
    bool panning_ = false;
    // Miga de pan de la escala (se rearma ~10 veces por segundo: busca qué hay en el centro).
    ScalePath scalePath_;
    float scalePathTimer_ = 0.0f;
    void drawScaleIndicator(ImDrawList* dl);
    bool dockBuilt_ = false;
    unsigned dockId_ = 0;        // dockspace principal (el del cuadro anterior)
    float lastWorkW_ = 0.0f, lastWorkH_ = 0.0f; // para escalar los paneles cuando cambia la ventana
};
