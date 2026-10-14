#include "app/app.hpp"

#include "app/tool_icons.hpp"

#include "bio/locomotion.hpp"
#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
#include "game/export.hpp"
#include "game/region.hpp"
#include "ui/being_icons.hpp"
#include "ui/i18n.hpp"
#include "ui/imgui_raylib.hpp"
#include "ui/panels/panels.hpp"
#include "ui/theme.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <raymath.h>

#include <omp.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <random>

namespace {

// Zoom máximo [px/µm]: ~0,3 nm por píxel. Alcanza para resolver la cola de un fago T4 (20 nm) y
// las capas de la envoltura bacteriana (~7 nm) en la vista de modelo.
constexpr float kMaxZoom = 3000.0f;

// Tolerancia para elegir con el mouse [µm]: ~6 px, con un mínimo de 2 µm para lo diminuto que se
// ve de lejos; ese mínimo no pasa de ~30 px (a escala de nm, 2 µm serían miles de píxeles).
float pickTolerance(float zoom) { return std::max(6.0f / zoom, std::min(2.0f, 30.0f / zoom)); }
constexpr int kScreenW = 1600;
constexpr int kScreenH = 900;

// La simulación avanza a paso fijo (determinista y estable), desacoplada del framerate.
constexpr float kFixedDt = 1.0f / 60.0f;

constexpr float kToolbarW = 52.0f;
constexpr float kStatusH = 28.0f;

// Tamaño aproximado del ser [µm] para el fantasma bajo el cursor.
float beingReach(Being b) {
    if (b.kind == BeingKind::Rock)
        return 65.0f;
    if (b.kind == BeingKind::Sediment)
        return 25.0f;
    if (b.kind != BeingKind::Species)
        return 12.0f;
    const SpeciesInfo& info = speciesInfo(b.species);
    return std::max(info.spec.radiusX, info.spec.radiusY);
}

// Anillo alrededor de lo inspeccionado (el centro de lo vivo se lee cada cuadro: la ficha se rearma menos seguido).
// Dónde está ahora lo elegido (las bacterias, por su id estable: la ficha se rearma cada 0,1 s
// y en ese tiempo una bacteria nada varios µm).
Vec2 ringCenter(const World& world, const Picked& p, const Inspection& ins) {
    Picked q = p;
    Vec2 at;
    return locatePicked(world, q, at) ? at : ins.at;
}

void drawRing(Vec2 c, float r, float zoom, Color color, float thickness) {
    DrawRing({c.x, c.y}, r, r + thickness / zoom, 0, 360, 72, color);
}

// Agrega un punto al lazo si se alejó lo suficiente del anterior (el trazo no crece sin límite).
void addLassoPoint(std::vector<Vec2>& path, Vec2 p, float minStep) {
    if (path.empty() || dot(p - path.back(), p - path.back()) >= minStep * minStep)
        path.push_back(p);
}


// Texto con fondo sobre la placa (en píxeles de pantalla).
void worldLabel(ImDrawList* dl, ImVec2 at, const char* text) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    dl->AddRectFilled({at.x - 3, at.y - 2}, {at.x + size.x + 3, at.y + size.y + 2}, IM_COL32(20, 24, 24, 200), 3.0f);
    dl->AddText(at, IM_COL32(240, 244, 240, 255), text);
}

ChemField& soluteField(World& world, int index) {
    switch (index) {
    case 0: return world.phosphateField();
    case 1: return world.nitrogenField();
    case 2: return world.silicateField();
    case 3: return world.ironField();
    case 4: return world.vitaminField();
    default: return world.nutrients();
    }
}

} // namespace

App::App() {
    options = loadOptions(optionsPath);
    progress = loadProgress(progressPath);
    plate.seed = std::random_device{}() & 0x7fffffffu;
    plate.rebuild();
    reloadStains();
    layoutPath_ = (std::filesystem::path(optionsPath).parent_path() / "layout.ini").string();
    layoutReset_ = !std::filesystem::exists(layoutPath_);
}

App::~App() = default;

void App::setTool(Tool t) {
    tools.tool = t;
    if (t != Tool::Spawn)
        catalog.armed = -1;
    if (t != Tool::Select) {
        selection.hover = {};
        selection.hoverText.clear();
    }
}

void App::reloadStains() {
    const std::string activeId = activeStain() ? activeStain()->id : "none";
    stains = builtinStains();
    for (StainSpec& s : loadUserStains(stainsDir)) {
        bool clash = false;
        for (const StainSpec& b : stains)
            clash |= b.id == s.id;
        if (!clash)
            stains.push_back(std::move(s));
    }
    stainIndex = 0;
    for (std::size_t i = 0; i < stains.size(); ++i)
        if (stains[i].id == activeId)
            stainIndex = static_cast<int>(i);
}

void App::setFullscreen(bool on) {
    if (IsWindowFullscreen() != on)
        ToggleFullscreen();
    options.fullscreen = on;
}

void App::applyOptions() {
    if (renderer) {
        renderer->setPostProcess(options.postProcess);
        renderer->setRenderScale(options.renderScale);
        renderer->setMaxDetailed(options.maxDetailed);
        renderer->setCellDetailScale(options.cellDetail);
    }
    if (IsWindowReady() && options.vsync != IsWindowState(FLAG_VSYNC_HINT)) {
        if (options.vsync)
            SetWindowState(FLAG_VSYNC_HINT);
        else
            ClearWindowState(FLAG_VSYNC_HINT);
    }
    // El resultado no depende de la cantidad de hilos (Rng por índice y paso).
    omp_set_num_threads(options.threads > 0 ? options.threads : autoThreads);
    if (world) {
        world->lodStride = options.lod ? options.lodStride : 1;
        world->setSubstepSolver(options.gpuPhysics && gpuOk ? &gpu : nullptr);
    }
    appliedFps_ = -1; // se recalcula en el próximo cuadro
}

void App::applyFrameCap() {
    int fps = options.fpsLimit;
    // Sin foco o minimizada no hace falta dibujar rápido. Con 20 FPS y hasta 4 pasos por cuadro,
    // la simulación sigue en tiempo real (80 pasos/s ≥ 60).
    if (options.backgroundSaver) {
        if (IsWindowMinimized())
            fps = 20;
        else if (!IsWindowFocused())
            fps = fps > 0 ? std::min(fps, 30) : 30;
    }
    if (fps != appliedFps_) {
        SetTargetFPS(fps);
        appliedFps_ = fps;
    }
}

void App::saveOptions() const { ::saveOptions(options, optionsPath); }

// ---------------------------------------------------------------------------
// Placa
// ---------------------------------------------------------------------------

void App::createPlate() {
    WorldConfig config = plate.config;
    worldSeed = config.seed;
    worldBiome = plate.biome;
    world = std::make_unique<World>(config);
    world->bioTimeScale() = options.bioTimeScale;
    applyOptions();
    progress.biomesPlayed |= 1u << static_cast<unsigned>(plate.biome);
    RunConfig rc;
    rc.spawnFounder = false; // en el laboratorio, el ser a controlar se elige con la herramienta Controlar
    rc.wildlife = plate.wildlife();
    rc.biome = plate.biome;
    rc.seed = config.seed;
    catalog.armed = -1;
    selection = SelectionState{}; // las entidades del mundo anterior ya no existen
    tools.grabbing = false;
    tools.follow = false;
    tools.followSelected = false;
    if (tools.tool == Tool::Spawn)
        tools.tool = Tool::Select;
    session.start(*world, rc, ++runCounter_);
    camera.target = {0.5f * world->width(), 0.5f * world->height()};
    if (world->dish().active())
        camera.zoom = std::min(viewport.width, viewport.height) / (2.3f * world->dish().radius);
    plateDirty = false;
}

void App::pushMediumLive() {
    if (!world)
        return;
    MediumParams& m = world->medium();
    const MediumParams& s = plate.config.medium;
    m.temperature = s.temperature;
    m.salinity = s.salinity;
    m.anoxic = s.anoxic;
    m.surfaceLight = s.surfaceLight;
    m.alkalinity = s.alkalinity;
    m.o2AirExchange = s.o2AirExchange;
    world->refreshMedium();
}

void App::pushNutrientLive(int soluteIndex, float target) {
    if (!world)
        return;
    world->setMeanConcentration(soluteField(*world, soluteIndex), target); // entra al balance como externo
}

void App::exportData() {
    if (!world)
        return;
    session.sample(*world); // incluye el estado de este instante
    const std::string base = (std::filesystem::path(exportsDir) / timestampedName(plateName)).string();
    const bool ok = writeTextFile(base + ".csv", seriesCsv(session.series())) &&
                    writeTextFile(base + ".json", runJson(*world, session, plate)) &&
                    writeTextFile((std::filesystem::path(exportsDir) / "especies.json").string(), speciesJson());
    lastExport = ok ? base : std::string();
    notify(ok ? std::string(ui::tr("Datos exportados: ")) + base + ".csv/.json" : ui::tr("No se pudo exportar"));
}

void App::notify(const std::string& text) {
    toasts.push_back(text);
    if (toasts.size() > 5)
        toasts.erase(toasts.begin(), toasts.end() - 5);
    toastTimer = 6.0f;
}

// ---------------------------------------------------------------------------
// Comandos (menús y atajos)
// ---------------------------------------------------------------------------

void App::registerCommands() {
    using ui::Command;
    auto always = [] { return true; };
    (void)always;
    auto add = [&](const char* id, const char* label, const char* menu, ImGuiKey key, bool ctrl, std::function<void()> run,
                   std::function<bool()> enabled = {}, std::function<bool()> checked = {}) {
        Command c;
        c.id = id;
        c.label = label;
        c.menu = menu;
        c.key = key;
        c.ctrl = ctrl;
        c.run = std::move(run);
        c.enabled = std::move(enabled);
        c.checked = std::move(checked);
        commands.add(std::move(c));
    };

    add("file.create", "Crear placa", "Archivo", ImGuiKey_N, true, [this] { createPlate(); });
    add("file.plate", "Editor de placa", "Archivo", ImGuiKey_None, false, [this] { panels.show("placa"); });
    add("file.export", "Exportar datos (CSV y JSON)", "Archivo", ImGuiKey_E, true, [this] { exportData(); });
    add("file.quit", "Salir", "Archivo", ImGuiKey_Q, true, [this] { quit = true; });

    add("sim.pause", "Pausa", "Simulación", ImGuiKey_Space, false, [this] { tools.paused = !tools.paused; }, {},
        [this] { return tools.paused; });
    add("sim.step", "Avanzar un paso", "Simulación", ImGuiKey_Period, false,
        [this] {
            tools.paused = true;
            ++tools.stepRequests;
        });
    add("sim.slower", "Más lenta", "Simulación", ImGuiKey_Minus, false,
        [this] { tools.simSpeed = std::max(0.125f, tools.simSpeed * 0.5f); });
    add("sim.faster", "Más rápida", "Simulación", ImGuiKey_Equal, false,
        [this] { tools.simSpeed = std::min(8.0f, tools.simSpeed * 2.0f); });

    for (int i = 0; i < static_cast<int>(Tool::Count); ++i) {
        const Tool t = static_cast<Tool>(i);
        const ToolInfo& info = toolInfo(t);
        add(("tool." + std::to_string(i)).c_str(), info.name, "Herramientas", info.key, false, [this, t] { setTool(t); },
            t == Tool::Spawn ? std::function<bool()>([this] { return catalog.armed >= 0; }) : std::function<bool()>{},
            [this, t] { return tools.tool == t; });
    }
    add("tool.reproduce", "Reproducir al ser controlado", "Herramientas", ImGuiKey_R, false,
        [this] { world->forceReproduction(world->player()); }, [this] { return world && world->registry().valid(world->player()); });
    add("tool.conjugate", "Conjugar con una pareja cercana", "Herramientas", ImGuiKey_V, false,
        [this] { world->tryConjugate(world->player()); }, [this] { return world && world->registry().valid(world->player()); });
    add("tool.follow", "Seguir con la cámara", "Herramientas", ImGuiKey_F, false,
        [this] {
            if (selection.followable() && !world->registry().valid(world->player()))
                tools.followSelected = !tools.followSelected;
            else if (selection.followable() && tools.tool == Tool::Select)
                tools.followSelected = !tools.followSelected;
            else
                tools.follow = !tools.follow;
        });

    static const struct {
        Overlay overlay;
        ImGuiKey key;
    } kOverlays[] = {
        {Overlay::Velocity, ImGuiKey_F1}, {Overlay::Oxygen, ImGuiKey_F2},      {Overlay::Nutrients, ImGuiKey_F3},
        {Overlay::Light, ImGuiKey_F4},    {Overlay::Phosphate, ImGuiKey_F5},   {Overlay::Nitrogen, ImGuiKey_F6},
        {Overlay::PH, ImGuiKey_F7},       {Overlay::Toxin, ImGuiKey_F8},       {Overlay::Autoinducer, ImGuiKey_F9},
        {Overlay::Phage, ImGuiKey_F10},
    };
    for (const auto& o : kOverlays) {
        const Overlay ov = o.overlay;
        add(("layer." + std::to_string(static_cast<int>(ov))).c_str(), overlayName(ov), "Capas", o.key, false,
            [this, ov] { tools.overlay = tools.overlay == ov ? Overlay::None : ov; }, {},
            [this, ov] { return tools.overlay == ov; });
    }
    add("view.limitation", "Mapa de limitación", "Capas", ImGuiKey_L, false,
        [this] { tools.limitationMap = !tools.limitationMap; }, {}, [this] { return tools.limitationMap; });
    add("view.semantic", "Zoom semántico (densidad de lejos)", "Capas", ImGuiKey_None, false,
        [this] { tools.semanticZoom = !tools.semanticZoom; }, {}, [this] { return tools.semanticZoom; });
    add("view.model", "Vista de modelo (estructuras no resolubles)", "Capas", ImGuiKey_None, false,
        [this] { tools.modelView = !tools.modelView; }, {}, [this] { return tools.modelView; });
    add("view.scalebar", "Barra de escala", "Capas", ImGuiKey_None, false, [this] { tools.scaleBar = !tools.scaleBar; },
        {}, [this] { return tools.scaleBar; });
    add("view.stain", "Siguiente tinción", "Capas", ImGuiKey_J, false,
        [this] { setStainIndex((stainIndex + 1) % static_cast<int>(stains.size())); });
    add("view.stain_editor", "Editor de tinciones", "Capas", ImGuiKey_None, false, [this] { panels.show("tinciones"); });

    add("view.fullscreen", "Pantalla completa", "Ver", ImGuiKey_F11, false,
        [this] {
            setFullscreen(!options.fullscreen);
            saveOptions();
        },
        {}, [this] { return options.fullscreen; });
    add("view.post", "Óptica del microscopio", "Ver", ImGuiKey_None, false,
        [this] {
            options.postProcess = !options.postProcess;
            applyOptions();
            saveOptions();
        },
        {}, [this] { return options.postProcess; });
    add("view.lod", "LOD: simplificar lo lejano", "Ver", ImGuiKey_None, false,
        [this] {
            options.lod = !options.lod;
            applyOptions();
            saveOptions();
        },
        {}, [this] { return options.lod; });
    add("view.gpu", "Física de células en la GPU", "Ver", ImGuiKey_None, false,
        [this] {
            options.gpuPhysics = !options.gpuPhysics;
            applyOptions();
            saveOptions();
        },
        [this] { return gpuOk; }, [this] { return options.gpuPhysics && gpuOk; });
    add("view.vsync", "Sincronía vertical (VSync)", "Ver", ImGuiKey_None, false,
        [this] {
            options.vsync = !options.vsync;
            applyOptions();
            saveOptions();
        },
        {}, [this] { return options.vsync; });
    add("view.layout", "Restablecer disposición", "Ver", ImGuiKey_None, false, [this] { requestLayoutReset(); });
}

// ---------------------------------------------------------------------------
// Interfaz
// ---------------------------------------------------------------------------

void App::drawToolbar() {
    constexpr int kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    if (ImGui::BeginViewportSideBar("##toolbar", ImGui::GetMainViewport(), ImGuiDir_Left, kToolbarW, kFlags)) {
        for (int i = 0; i < static_cast<int>(Tool::Count); ++i) {
            const Tool t = static_cast<Tool>(i);
            if (t == Tool::Spawn && catalog.armed < 0)
                continue; // aparece cuando se arma un ser del catálogo
            if (i == static_cast<int>(Tool::Ink) || i == static_cast<int>(Tool::Measure) || i == static_cast<int>(Tool::Spawn))
                ImGui::Separator();
            const ToolInfo& info = toolInfo(t);
            std::string help = std::string(ui::tr(info.name)) + ": " + ui::tr(info.help);
            if (info.key != ImGuiKey_None)
                help += std::string(" [") + ImGui::GetKeyName(info.key) + "]";
            ImGui::PushID(i);
            const bool active = tools.tool == t;
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, {0.20f, 0.40f, 0.29f, 1.0f});
            if (ImGui::Button("##tool", {36.0f, 32.0f}))
                setTool(t);
            if (active)
                ImGui::PopStyleColor();
            const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
            drawToolIcon(ImGui::GetWindowDrawList(), t, {0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y)}, 20.0f,
                         ImGui::GetColorU32(active ? ImVec4(1, 1, 1, 1) : ImVec4(0.82f, 0.85f, 0.84f, 1.0f)));
            ui::tooltip(help.c_str());
            ImGui::PopID();
        }
    }
    ImGui::End();
}

void App::drawStatusBar() {
    constexpr int kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking;
    if (ImGui::BeginViewportSideBar("##status", ImGui::GetMainViewport(), ImGuiDir_Down, kStatusH, kFlags)) {
        if (ImGui::SmallButton(tools.paused ? ">" : "||"))
            tools.paused = !tools.paused;
        ui::tooltip("Pausa [Espacio]");
        ImGui::SameLine();
        if (ImGui::SmallButton(">|")) {
            tools.paused = true;
            ++tools.stepRequests;
        }
        ui::tooltip("Avanzar un paso [.]");
        ImGui::SameLine();
        ImGui::Text("x%g", tools.simSpeed);
        if (world) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Text("t = %.1f s (%.2f h bio)", world->simTime(), world->simTime() * world->bioScale() / 3600.0f);
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Text("%s  seed %llu", biome(static_cast<BiomeId>(worldBiome)).name,
                        static_cast<unsigned long long>(worldSeed));
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Text("%zu %s", world->cellCount(), ui::tr("células"));
            if (world->fixed()) {
                ImGui::SameLine();
                ImGui::TextColored({0.92f, 0.60f, 0.25f, 1.0f}, "| %s", ui::tr("Muestra fijada (Ctrl+N: placa nueva)"));
            }
        }
        if (plateDirty) {
            ImGui::SameLine();
            ImGui::TextColored({0.92f, 0.60f, 0.25f, 1.0f}, "| %s", ui::tr("Hay cambios sin crear (Ctrl+N)"));
        }
        const char* tail = ui::tr(toolInfo(tools.tool).name);
        char right[96];
        std::snprintf(right, sizeof right, "%s  |  %d FPS", tail, GetFPS());
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right).x - 12.0f);
        ImGui::TextUnformatted(right);
    }
    ImGui::End();
}

void App::drawToasts() {
    if (toasts.empty() || toastTimer <= 0.0f)
        return;
    const float alpha = std::min(1.0f, toastTimer);
    ImGui::SetNextWindowPos({viewport.x + viewport.width - 12.0f, viewport.y + viewport.height - 12.0f}, ImGuiCond_Always,
                            {1.0f, 1.0f});
    ImGui::SetNextWindowBgAlpha(0.85f * alpha);
    constexpr int kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    if (ImGui::Begin("##toasts", nullptr, kFlags))
        for (const std::string& t : toasts)
            ImGui::TextUnformatted(t.c_str());
    ImGui::End();
    ImGui::PopStyleVar();
}

void App::buildUi(float) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (ImGui::BeginMainMenuBar()) {
        static const char* const kMenus[] = {"Archivo", "Simulación", "Herramientas", "Capas", "Ver"};
        for (const char* m : kMenus) {
            if (!ImGui::BeginMenu(ui::tr(m)))
                continue;
            commands.drawMenuItems(m);
            if (std::string(m) == "Ver") {
                ImGui::Separator();
                panels.drawMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    drawToolbar();
    drawStatusBar();

    // Ventana redimensionada: los paneles escalan en proporción antes de que ImGui acomode el
    // dockspace al tamaño nuevo.
    if (dockId_ != 0 && lastWorkW_ > 0.0f && lastWorkH_ > 0.0f &&
        (vp->WorkSize.x != lastWorkW_ || vp->WorkSize.y != lastWorkH_))
        panels.scaleLayout(dockId_, vp->WorkSize.x / lastWorkW_, vp->WorkSize.y / lastWorkH_);
    lastWorkW_ = vp->WorkSize.x;
    lastWorkH_ = vp->WorkSize.y;
    const ImGuiID dock = ImGui::DockSpaceOverViewport(0, vp, ImGuiDockNodeFlags_PassthruCentralNode);
    dockId_ = dock;
    if (layoutReset_ || !dockBuilt_) {
        if (layoutReset_) {
            panels.resetLayout(dock, vp->WorkSize.x, vp->WorkSize.y);
            layoutReset_ = false;
        }
        dockBuilt_ = true;
    }
    if (const ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dock))
        viewport = {node->Pos.x, node->Pos.y, std::max(64.0f, node->Size.x), std::max(64.0f, node->Size.y)};

    panels.drawAll(*this);

    // Tooltip de lo que hay bajo el cursor (herramienta Seleccionar).
    if (tools.tool == Tool::Select && !selection.hoverText.empty() && !ImGui::GetIO().WantCaptureMouse)
        ImGui::SetTooltip("%s", selection.hoverText.c_str());
    drawToasts();
}

// ---------------------------------------------------------------------------
// Entrada sobre la placa
// ---------------------------------------------------------------------------

Vec2 App::worldPos(const Vector2& m) const {
    // Con placa el mundo es cerrado: afuera del agua no hay nada que tocar.
    if (world->dish().active())
        return world->dish().clampInside({m.x, m.y});
    return {wrapCoord(m.x, world->width()), wrapCoord(m.y, world->height())};
}

void App::updateCamera(bool mouseFree) {
    const Dish& dish = world->dish();
    const Vector2 mid = {viewport.x + 0.5f * viewport.width, viewport.y + 0.5f * viewport.height};
    // Con placa, el alejamiento máximo muestra la placa entera (con el canto de vidrio).
    const float minZoom = dish.active() ? std::min(viewport.width, viewport.height) / (2.3f * dish.radius) : 0.4f;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && mouseFree)
        panning_ = true;
    if (!IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        panning_ = false;
    if (panning_) {
        tools.follow = false; // mover la cámara a mano suelta el seguimiento
        tools.followSelected = false;
        const Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / camera.zoom);
        camera.target = Vector2Add(camera.target, delta);
    }
    if (const float wheel = mouseFree ? GetMouseWheelMove() : 0.0f; wheel != 0.0f) {
        if (!tools.follow && !tools.followSelected) { // zoom centrado en el cursor
            const Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
            camera.offset = GetMousePosition();
            camera.target = mouseWorld;
        }
        camera.zoom = Clamp(camera.zoom * (1.0f + 0.1f * wheel), minZoom, kMaxZoom);
    }
    camera.zoom = std::max(camera.zoom, minZoom);
    if (dish.active()) {
        // El centro de la vista no sale de la placa.
        const Vector2 c = GetScreenToWorld2D(mid, camera);
        const Vec2 inside = dish.clampInside({c.x, c.y});
        camera.offset = mid;
        camera.target = {inside.x, inside.y};
    } else {
        camera.offset = mid;
    }
}

// Cámara de microscopio: sigue a una célula con suavidad.
static void followPicked(Camera2D& cam, const World& world, Picked& picked, const Rectangle& vp, float dt) {
    Vec2 c;
    if (!locatePicked(world, picked, c))
        return;
    cam.offset = {vp.x + 0.5f * vp.width, vp.y + 0.5f * vp.height};
    // Las células se siguen con suavidad; una bacteria (~25 µm/s) se saldría de una vista de pocos
    // µm con cualquier retraso, así que la cámara va pegada a ella.
    const bool tiny = picked.kind == Picked::Kind::Bacterium || picked.kind == Picked::Kind::Bdellovibrio;
    const float k = tiny ? 1.0f : 1.0f - std::exp(-6.0f * dt);
    cam.target.x = wrapCoord(cam.target.x + minImage(c.x - cam.target.x, world.width()) * k, world.width());
    cam.target.y = wrapCoord(cam.target.y + minImage(c.y - cam.target.y, world.height()) * k, world.height());
}

// Lo que la vista muestra solo como densidad no se elige: el click no tiene que dar con un
// individuo que no se ve.
std::uint32_t App::pickSkip() const {
    std::uint32_t skip = 0;
    const RenderLodSettings& lod = renderer->lod();
    if (lod.blend(Population::Bacteria, camera.zoom).individual < 0.5f)
        skip |= pickBit(Picked::Kind::Bacterium);
    if (lod.blend(Population::Bdellovibrio, camera.zoom).individual < 0.5f)
        skip |= pickBit(Picked::Kind::Bdellovibrio);
    return skip;
}

// Elegir lo que está bajo el cursor: resalta (tooltip) y el click abre su ficha. Las fichas
// vivas se rearman ~10 veces por segundo; si el ser fue reemplazado (la zoóspora que salió de la
// célula, la cría que salió del huevo), la ficha pasa a su sucesor.
void App::selectionInput(float dt, const Vector2&, const Vec2& pos) {
    SelectionState& st = selection;
    const bool mouseFree = !ImGui::GetIO().WantCaptureMouse;
    const float tolerance = pickTolerance(camera.zoom);

    st.hoverTimer -= dt;
    if (!mouseFree || tools.tool != Tool::Select) {
        st.hover = {};
        st.hoverText.clear();
    } else if (st.hoverTimer <= 0.0f) {
        st.hoverTimer = 0.05f;
        st.hover = world->pick(pos, tolerance, pickSkip());
        st.hoverText.clear();
        if (st.hover) {
            const Inspection h = buildInspection(*world, st.hover);
            st.hoverText = h.title;
            // De cerca, además, la estructura que se está apuntando.
            const float viewMicrons = std::min(viewport.width, viewport.height) / camera.zoom;
            if (const std::string part = structureAt(*world, st.hover, pos, viewMicrons, tools.modelView); !part.empty())
                st.hoverText += std::string(" > ") + ui::tr(part.c_str());
            if (!h.headline.empty())
                st.hoverText += " - " + h.headline;
        }
    }

    st.pickedAge += dt;
    // Un click que no toca nada no cambia nada: con una tolerancia de pocos píxeles es fácil
    // errarle a algo chico, y no hay que perder la ficha que se estaba leyendo (Esc la cierra).
    if (mouseFree && tools.tool == Tool::Select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (const Picked hit = world->pick(pos, tolerance, pickSkip())) {
            st.picked = hit;
            st.inspection = buildInspection(*world, hit);
            st.refresh = 0.1f;
            st.pickedAge = 0.0f;
            panels.show("ficha");
        }
    }

    if (st.picked.live()) {
        st.refresh -= dt;
        if (st.refresh <= 0.0f) {
            st.refresh = 0.1f;
            st.inspection = buildInspection(*world, st.picked);
            const entt::entity next = st.inspection.successor;
            if (st.inspection.dead && next != entt::null && world->registry().valid(next)) {
                st.picked = {};
                st.picked.kind = Picked::Kind::Cell;
                st.picked.entity = next;
                st.inspection = buildInspection(*world, st.picked);
            }
        }
    }
}

void App::toolInput(float dt, const Vec2& pos) {
    const bool mouseFree = !ImGui::GetIO().WantCaptureMouse;
    const bool pressed = mouseFree && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool held = (mouseFree || tools.grabbing) && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool down = mouseFree && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    // Herramientas de mantener: se sueltan solas cuando cambia la herramienta o se suelta el botón.
    if (!(tools.tool == Tool::Squeeze && down))
        world->clearSqueeze();
    const bool stirring = tools.tool == Tool::Stir && down && dt > 0.0f;
    if (stirring) {
        const Vector2 d = Vector2Scale(GetMouseDelta(), 1.0f / (camera.zoom * dt * units::kTimeScale));
        world->setStir(pos, {d.x, d.y}, tools.stirRadius);
    } else {
        world->clearStir();
    }
    if (tools.grabbing && (!held || tools.tool != Tool::Grab)) {
        world->endGrab();
        tools.grabbing = false;
    }

    switch (tools.tool) {
    case Tool::Ink:
        if (down)
            world->dropInk(pos, tools.inkRadius, 40);
        break;
    case Tool::Detritus:
        if (pressed)
            world->dropDetritus(pos);
        break;
    case Tool::Erase:
        if (pressed)
            world->removeCellAt(pos);
        break;
    case Tool::Lyse:
        if (pressed)
            world->lyseAt(pos);
        break;
    case Tool::Squeeze:
        if (down)
            world->setSqueeze(pos, tools.squeezeSpeed);
        break;
    case Tool::Grab:
        if (pressed)
            tools.grabbing = world->beginGrab(pos);
        else if (tools.grabbing && held)
            world->moveGrab(pos);
        break;
    case Tool::Dropper:
        if (down)
            applyDropper(*world, tools, pos.x, pos.y, dt);
        break;
    case Tool::Spawn:
        if (pressed && catalog.armed >= 0)
            world->spawnBeing(beingAt(catalog.armed), pos);
        break;
    case Tool::Measure:
        if (pressed) {
            tools.measurePath.assign(1, pos);
            tools.measuring = true;
        } else if (tools.measuring && down) {
            if (tools.measureMode == ToolState::MeasureMode::Distance) {
                tools.measurePath.resize(1);
                tools.measurePath.push_back(pos);
            } else {
                addLassoPoint(tools.measurePath, pos, 3.0f / camera.zoom);
            }
        } else if (tools.measuring) {
            tools.measuring = false;
            panels.show("medicion");
        }
        break;
    case Tool::Region:
        if (pressed) {
            tools.lassoPath.assign(1, pos);
            tools.drawingRegion = true;
        } else if (tools.drawingRegion && down) {
            addLassoPoint(tools.lassoPath, pos, 3.0f / camera.zoom);
        } else if (tools.drawingRegion) {
            tools.drawingRegion = false;
            if (tools.lassoPath.size() >= 3 && polygonArea(tools.lassoPath) > 1.0f) {
                session.setRegion(*world, tools.lassoPath);
                panels.show("medicion");
            }
            tools.lassoPath.clear();
        }
        break;
    default: break;
    }
}

// Controlar: elegir la célula y moverla. Cilios y ondulación con W/S/A/D; flagelo con W o
// manteniendo el click (gira hacia el mouse); pseudópodo con el click.
void App::controlInput(bool mouseFree, const Vec2& pos) {
    const bool kbFree = !ImGui::GetIO().WantTextInput;
    const bool leftPressed = mouseFree && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if ((tools.tool == Tool::Control && leftPressed) || (kbFree && IsKeyPressed(KEY_P))) {
        const Picked hit = world->pick(pos, pickTolerance(camera.zoom));
        if (hit.kind == Picked::Kind::Cell && world->registry().all_of<Locomotion>(hit.entity) &&
            hit.entity != world->player()) {
            world->setPlayer(hit.entity);
            tools.follow = true;
            panels.show("estadisticas");
        }
    }
    PlayerInput input;
    if (!world->registry().valid(world->player())) {
        world->setPlayerInput(input);
        return;
    }
    const bool lmb = mouseFree && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && tools.tool == Tool::Control && !leftPressed;
    const bool mouseDrive = lmb || (leftPressed && tools.tool == Tool::Control);
    input.forward = kbFree ? (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f) : 0.0f;
    input.turn = kbFree ? (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f) : 0.0f;
    input.propel = (kbFree && IsKeyDown(KEY_W)) || mouseDrive;
    input.pseudopod = mouseDrive;
    input.aim = pos;
    world->setPlayerInput(input);
}

void App::worldInput(float dt) {
    const ImGuiIO& io = ImGui::GetIO();
    const bool mouseFree = !io.WantCaptureMouse;
    updateCamera(mouseFree);
    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    const Vec2 pos = worldPos(mouse);

    if (!io.WantTextInput && IsKeyPressed(KEY_ESCAPE)) {
        if (selection.inspection.valid) {
            selection.picked = {};
            selection.inspection = {};
            tools.followSelected = false;
        } else if (tools.tool != Tool::Select) {
            setTool(Tool::Select);
        }
    }
    selectionInput(dt, mouse, pos);
    toolInput(dt, pos);
    controlInput(mouseFree, pos);
}

// ---------------------------------------------------------------------------
// Simulación
// ---------------------------------------------------------------------------

void App::stepSimulation(float frameDt) {
    const Vector2 a = GetScreenToWorld2D({viewport.x, viewport.y}, camera);
    const Vector2 b = GetScreenToWorld2D({viewport.x + viewport.width, viewport.y + viewport.height}, camera);
    // LOD: lo que se ve (más un margen) se simula en detalle.
    world->setFocus({camera.target.x, camera.target.y}, {0.5f * std::abs(b.x - a.x), 0.5f * std::abs(b.y - a.y)});
    if (world->fixed()) { // muestra fijada: no hay pasos (ni filas nuevas en la serie)
        accumulator_ = 0.0f;
        tools.stepRequests = 0;
        return;
    }
    auto advance = [&] {
        const double t0 = GetTime();
        world->step(kFixedDt);
        tools.stepMs += (static_cast<float>(1000.0 * (GetTime() - t0)) - tools.stepMs) * 0.1f;
        session.update(*world, kFixedDt);
    };
    if (tools.paused) {
        accumulator_ = 0.0f;
        while (tools.stepRequests > 0) {
            advance();
            --tools.stepRequests;
        }
        return;
    }
    tools.stepRequests = 0;
    accumulator_ += frameDt * units::kTimeScale * tools.simSpeed;
    const int maxSteps = tools.simSpeed > 1.0f ? 2 * options.maxStepsPerFrame : options.maxStepsPerFrame;
    int steps = 0;
    while (accumulator_ >= kFixedDt && steps < maxSteps) {
        advance();
        accumulator_ -= kFixedDt;
        ++steps;
    }
    if (steps == maxSteps)
        accumulator_ = 0.0f; // evita la "espiral de la muerte" si la sim no da abasto
}

// Enciclopedia: lo que se ve se cataloga (dos veces por segundo).
void App::discover(float dt) {
    discoveryTimer_ -= dt;
    if (discoveryTimer_ <= 0.0f) {
        discoveryTimer_ = 0.5f;
        const Vector2 a = GetScreenToWorld2D({viewport.x, viewport.y}, camera);
        const Vector2 b = GetScreenToWorld2D({viewport.x + viewport.width, viewport.y + viewport.height}, camera);
        auto fresh = discoverVisible(progress, *world, {a.x, a.y}, {b.x, b.y});
        for (std::string& f : recordBehaviors(progress, *world))
            fresh.push_back(std::move(f));
        if (!fresh.empty()) {
            for (const std::string& f : fresh)
                notify(std::string(ui::tr("Enciclopedia")) + ": " + f);
            saveProgress(progress, progressPath);
        }
    }
    toastTimer = std::max(0.0f, toastTimer - dt);
    if (toastTimer <= 0.0f)
        toasts.clear();
}

// Resaltados en el mundo: lo elegido (grueso), lo que está bajo el cursor (fino) y el alcance de la herramienta.
void App::drawWorldOverlays() {
    BeginMode2D(camera);
    const SelectionState& st = selection;
    if (st.inspection.valid && !st.inspection.dead) {
        // Lo que no tiene identidad estable (bacterias, partículas) es una foto: el anillo marca
        // dónde estaba al hacer click y se desvanece, porque el ser ya se movió.
        const float fade = st.picked.live() ? 1.0f : std::clamp(1.0f - (st.pickedAge - 0.6f) / 1.2f, 0.0f, 1.0f);
        if (fade > 0.0f)
            // Una bacteria se marca de cerca (a escala de nm, un anillo de 6 µm queda fuera de la vista).
            drawRing(ringCenter(*world, st.picked, st.inspection),
                     st.picked.kind == Picked::Kind::Bacterium || st.picked.kind == Picked::Kind::Bdellovibrio
                         ? std::max(1.6f, 14.0f / camera.zoom)
                         : st.inspection.radius,
                     camera.zoom,
                     Fade({200, 120, 20, 230}, fade), 3.5f);
    }
    const bool hoverIsPicked = st.hover.kind == st.picked.kind && st.hover.entity == st.picked.entity &&
                               st.hover.id == st.picked.id && (st.hover.live() || st.hover.index == st.picked.index);
    if (st.hover && !hoverIsPicked) {
        float r = 6.0f;
        Vec2 c = st.hover.at;
        if (st.hover.kind == Picked::Kind::Cell && world->registry().valid(st.hover.entity) &&
            world->registry().all_of<RingBody>(st.hover.entity)) {
            const RingBody& b = world->registry().get<RingBody>(st.hover.entity);
            c = b.centroid;
            r = 0.5f * std::max(b.aabbMax.x - b.aabbMin.x, b.aabbMax.y - b.aabbMin.y) + 3.0f;
        }
        drawRing(c, r, camera.zoom, {80, 80, 70, 150}, 1.5f);
    }
    // Alcance de la herramienta bajo el cursor.
    if (!ImGui::GetIO().WantCaptureMouse) {
        const Vec2 p = worldPos(GetScreenToWorld2D(GetMousePosition(), camera));
        float reach = 0.0f;
        switch (tools.tool) {
        case Tool::Ink: reach = tools.inkRadius; break;
        case Tool::Stir: reach = tools.stirRadius; break;
        case Tool::Dropper: reach = tools.dropRadius; break;
        case Tool::Spawn:
            if (catalog.armed >= 0)
                reach = beingReach(beingAt(catalog.armed));
            break;
        default: break;
        }
        if (reach > 0.0f)
            DrawRing({p.x, p.y}, reach, reach + 2.0f / camera.zoom, 0, 360, 64, {60, 110, 60, 200});
    }
    drawMeasurements();
}

// Regla, región y barra de escala (fase 13). Las líneas van en coordenadas del mundo; los
// rótulos, en la lista de fondo de ImGui (con la fuente de la UI, que tiene µ y ²).
void App::drawMeasurements() {
    const float px = 1.0f / camera.zoom;
    auto polyline = [&](const std::vector<Vec2>& pts, bool closed, Color c, float width) {
        for (std::size_t i = 1; i < pts.size(); ++i)
            DrawLineEx({pts[i - 1].x, pts[i - 1].y}, {pts[i].x, pts[i].y}, width * px, c);
        if (closed && pts.size() > 2)
            DrawLineEx({pts.back().x, pts.back().y}, {pts[0].x, pts[0].y}, width * px, c);
    };
    const Color regionColor = {40, 110, 190, 230}, rulerColor = {200, 70, 30, 240};
    const std::vector<Vec2>& region = session.region();
    polyline(region, true, regionColor, 2.0f);
    polyline(tools.lassoPath, false, regionColor, 1.5f);
    const bool ruler = tools.tool == Tool::Measure && tools.measurePath.size() >= 2;
    const bool area = tools.measureMode == ToolState::MeasureMode::Area;
    if (ruler) {
        polyline(tools.measurePath, area && !tools.measuring, rulerColor, 2.0f);
        if (!area) // marcas en los extremos, perpendiculares a la regla
            for (const Vec2& end : {tools.measurePath.front(), tools.measurePath.back()}) {
                const Vec2 d = tools.measurePath.back() - tools.measurePath.front();
                const float len = std::sqrt(dot(d, d));
                const Vec2 n = len > 0.0f ? Vec2{-d.y / len, d.x / len} * (6.0f * px) : Vec2{0.0f, 6.0f * px};
                DrawLineEx({end.x - n.x, end.y - n.y}, {end.x + n.x, end.y + n.y}, 2.0f * px, rulerColor);
            }
    }
    EndMode2D();

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto screen = [&](Vec2 w) {
        const Vector2 s = GetWorldToScreen2D({w.x, w.y}, camera);
        return ImVec2{s.x, s.y};
    };
    char text[96];
    if (ruler) {
        const ImVec2 at = screen(tools.measurePath.back());
        if (area)
            std::snprintf(text, sizeof text, "%.0f µm² | %s %.0f µm", polygonArea(tools.measurePath), ui::tr("perímetro"),
                          polylineLength(tools.measurePath) +
                              (tools.measuring ? 0.0f : polylineLength({tools.measurePath.back(), tools.measurePath.front()})));
        else
            std::snprintf(text, sizeof text, "%.1f µm", polylineLength(tools.measurePath));
        worldLabel(dl, {at.x + 12.0f, at.y - 8.0f}, text);
    }
    if (region.size() > 2 && session.regionStats().valid) {
        Vec2 top = region[0];
        for (const Vec2& q : region)
            top = q.y < top.y ? q : top;
        const RegionStats& r = session.regionStats();
        std::snprintf(text, sizeof text, "%s: %d %s, %d %s", ui::tr("Región"), r.cells, ui::tr("células"), r.bacteria,
                      ui::tr("bacterias"));
        const ImVec2 at = screen(top);
        worldLabel(dl, {at.x - 0.5f * ImGui::CalcTextSize(text).x, at.y - 24.0f}, text);
    }
    drawScaleIndicator(dl);
    if (tools.scaleBar) {
        // Barra de ~120 px con un largo redondo en µm, abajo a la izquierda del campo.
        const float um = niceLength(120.0f / camera.zoom);
        const float w = um * camera.zoom;
        const ImVec2 a{viewport.x + 24.0f, viewport.y + viewport.height - 30.0f};
        dl->AddRectFilled({a.x - 8, a.y - 22}, {a.x + w + 8, a.y + 10}, IM_COL32(20, 24, 24, 150), 3.0f);
        dl->AddRectFilled({a.x, a.y}, {a.x + w, a.y + 4}, IM_COL32(245, 245, 240, 255));
        std::snprintf(text, sizeof text, "%s", formatLength(um).c_str());
        dl->AddText({a.x + 0.5f * (w - ImGui::CalcTextSize(text).x), a.y - 19.0f}, IM_COL32(245, 245, 240, 255), text);
    }
}

// Escala biológica y miga de pan arriba a la izquierda del campo: qué nivel se está mirando
// (Placa > organismo > célula > estructura) y si es una imagen de microscopio o el modelo.
void App::drawScaleIndicator(ImDrawList* dl) {
    const float viewMicrons = std::min(viewport.width, viewport.height) / camera.zoom;
    scalePathTimer_ -= GetFrameTime();
    if (scalePathTimer_ <= 0.0f) {
        scalePathTimer_ = 0.1f;
        const Vector2 mid = GetScreenToWorld2D({viewport.x + 0.5f * viewport.width, viewport.y + 0.5f * viewport.height}, camera);
        const Picked focus = selection.inspection.valid && !selection.inspection.dead ? selection.picked : Picked{};
        scalePath_ = scalePath(*world, {mid.x, mid.y}, viewMicrons, focus);
    }
    std::string path;
    for (std::size_t i = 0; i < scalePath_.crumbs.size(); ++i) {
        if (i > 0)
            path += "  >  ";
        path += ui::tr(scalePath_.crumbs[i].c_str());
    }
    std::string sub = ui::tr(bioScaleName(scalePath_.scale));
    if (tools.modelView)
        sub += std::string("  |  ") + ui::tr("Vista de modelo: esquema didáctico, no imagen de microscopio");
    else if (1.0f / camera.zoom < 0.1f * renderer->lod().opticalResolution)
        // Más allá del límite de Abbe, agrandar no muestra nada nuevo (aumento vacío).
        sub += std::string("  |  ") + ui::tr("Por debajo del límite óptico (~0,24 µm): aumento vacío. Capas > Vista de modelo");
    const ImVec2 at = {viewport.x + 14.0f, viewport.y + 12.0f};
    worldLabel(dl, at, path.c_str());
    worldLabel(dl, {at.x, at.y + ImGui::GetTextLineHeight() + 8.0f}, sub.c_str());
}

// ---------------------------------------------------------------------------
// Bucle principal
// ---------------------------------------------------------------------------

int App::run() {
    // Un hilo por núcleo físico: con SMT, los hilos hermanos compiten por la misma FPU y las
    // barreras de OpenMP esperan al más lento (medido: 8 hilos ~1,6× más rápido que 16).
    autoThreads = std::getenv("OMP_NUM_THREADS") ? omp_get_max_threads() : std::max(1, omp_get_num_procs() / 2);
    // El MSAA solo se puede pedir al crear la ventana (por eso se aplica al reiniciar).
    unsigned flags = FLAG_WINDOW_RESIZABLE;
    if (options.vsync)
        flags |= FLAG_VSYNC_HINT;
    if (options.msaa)
        flags |= FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);
    InitWindow(kScreenW, kScreenH, "bio");
    SetExitKey(KEY_NULL); // Esc no cierra el programa
    if (options.fullscreen)
        ToggleFullscreen();

    ui::loadLanguage(options.language);
    ui::initBackend(layoutPath_.c_str(), 15.0f);
    ui::applyTheme();
    ui::buildBeingIcons();
    ui::registerPanels(panels);
    registerCommands();

    renderer = std::make_unique<Renderer>();
    gpuOk = gpu.init();
    camera.zoom = 1.0f;
    camera.offset = {kScreenW * 0.5f, kScreenH * 0.5f};
    applyOptions();
    createPlate(); // arranca con la placa preconfigurada (vacía por defecto)

    while (!WindowShouldClose() && !quit) {
        applyFrameCap();
        const float frameDt = GetFrameTime();
        tools.frameHistory[tools.frameHead] = 1000.0f * frameDt;
        tools.frameHead = (tools.frameHead + 1) % ToolState::kFrameHistory;
        const double u0 = GetTime();
        ui::beginFrame(frameDt);
        buildUi(frameDt);
        commands.handleShortcuts();
        worldInput(frameDt);
        const double s0 = GetTime();
        tools.uiMs += (static_cast<float>(1000.0 * (s0 - u0)) - tools.uiMs) * 0.1f;
        stepSimulation(frameDt);
        tools.simFrameMs += (static_cast<float>(1000.0 * (GetTime() - s0)) - tools.simFrameMs) * 0.1f;

        // La cámara sigue al ser controlado o al elegido.
        if (tools.followSelected && selection.followable()) {
            followPicked(camera, *world, selection.picked, viewport, frameDt);
        } else if (tools.follow) {
            Picked player;
            player.kind = Picked::Kind::Cell;
            player.entity = world->player();
            followPicked(camera, *world, player, viewport, frameDt);
        }
        discover(frameDt);

        renderer->setStain(activeStain());
        renderer->setLimitationMap(tools.limitationMap);
        renderer->setLodEnabled(tools.semanticZoom);
        renderer->setViewMode(tools.modelView ? ViewMode::Model : ViewMode::Microscope);
        renderer->setInspection(selection.inspection.valid && !selection.inspection.dead ? selection.picked : Picked{});
        BeginDrawing();
        ClearBackground({22, 24, 24, 255});
        const double r0 = GetTime();
        renderer->drawWorld(*world, camera, tools.overlay);
        tools.renderMs += (static_cast<float>(1000.0 * (GetTime() - r0)) - tools.renderMs) * 0.1f;
        drawWorldOverlays();
        ui::endFrame();
        EndDrawing();
        PROFILE_FRAME();
    }

    saveProgress(progress, progressPath);
    saveOptions();
    world.reset();
    renderer.reset();
    ui::freeBeingIcons();
    ui::shutdownBackend();
    CloseWindow();
    return 0;
}
