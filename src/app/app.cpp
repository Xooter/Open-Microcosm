#include "app/app.hpp"

#include "app/tool_icons.hpp"
#include "build_info.hpp"

#include "bio/locomotion.hpp"
#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/biome.hpp"
#include "game/content.hpp"
#include "game/encyclopedia.hpp"
#include "game/export.hpp"
#include "game/region.hpp"
#include "game/scene.hpp"
#include "render/photo.hpp"
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
#include <fstream>
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

} // namespace

App::App() {
    options = loadOptions(optionsPath);
    tools.modelView = options.modelView;
    if (options.tool < static_cast<int>(Tool::Spawn)) // Spawn necesita un ser armado en el catálogo
        tools.tool = static_cast<Tool>(options.tool);
    progress = loadProgress(progressPath);
    plate.seed = std::random_device{}() & 0x7fffffffu;
    plate.rebuild();
    reloadStains();
    layoutPath_ = (std::filesystem::path(optionsPath).parent_path() / "layout.ini").string();
    layoutReset_ = !std::filesystem::exists(layoutPath_);
}

App::~App() { ui::writeMissingKeys(); } // BIO_TR_FALTANTES: las claves sin traducir

void App::setTool(Tool t) {
    tools.tool = t;
    if (t != Tool::Spawn && options.tool != static_cast<int>(t)) {
        options.tool = static_cast<int>(t);
        saveOptions();
    }
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

// Pantalla completa sin bordes: la ventana toma el tamaño del monitor y la interfaz se acomoda.
// (ToggleFullscreen de raylib es exclusiva a la resolución de la ventana —1600 × 900— y mientras
// dura no actualiza el tamaño de pantalla: la UI quedaba chica.)
void App::setFullscreen(bool on) {
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) != on)
        ToggleBorderlessWindowed();
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
        // LOD y motor cambian la trayectoria: pasan por el registro de la sesión.
        const int stride = options.lod ? options.lodStride : 1;
        if (world->lodStride != stride)
            setParameter(*findParameter("lod.cada"), static_cast<float>(stride));
        SubstepSolver* solver = options.gpuPhysics && gpuOk ? &gpu : nullptr;
        if (world->substepSolver() != solver) {
            world->setSubstepSolver(solver);
            Intervention engine;
            engine.kind = InterventionKind::Engine;
            engine.index = solver ? 1 : 0;
            intervene(engine);
        }
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
    if (plate.biome < 32) // los de fábrica (la enciclopedia guarda una máscara de 32 bits)
        progress.biomesPlayed |= 1u << static_cast<unsigned>(plate.biome);
    const RunConfig rc = plateRunConfig(plate);
    catalog.armed = -1;
    selection = SelectionState{}; // las entidades del mundo anterior ya no existen
    tools.grabbing = false;
    tools.follow = false;
    tools.followSelected = false;
    if (tools.tool == Tool::Spawn)
        tools.tool = Tool::Select;
    session.start(*world, rc, ++runCounter_);
    runPlate = plate;
    // Paso 0: lo que no está en la placa pero cambia la trayectoria (escala de tiempo, LOD y
    // motor) queda en el registro, y después el guion de la escena cargada, si hay.
    setParameter(*findParameter("tiempo.escala_biologica"), options.bioTimeScale);
    setParameter(*findParameter("lod.cada"), static_cast<float>(options.lod ? options.lodStride : 1));
    applyOptions(); // anota el motor si pasa a la GPU
    if (!world->substepSolver()) {
        Intervention engine;
        engine.kind = InterventionKind::Engine;
        engine.index = 0;
        intervene(engine);
    }
    session.interventions().schedule(plateScript);
    session.notes() = plateNotes;
    // Un preparado se arma ya teñido (frotis con Giemsa, papa con Lugol…): la tinción de sus datos.
    if (const PreparedPlate* pp = plate.prepared.empty() ? nullptr : findPreparedPlate(plate.prepared); pp && !pp->stain.empty())
        for (std::size_t i = 0; i < stains.size(); ++i)
            if (stains[i].id == pp->stain)
                setStainIndex(static_cast<int>(i));
    camera.target = {0.5f * world->width(), 0.5f * world->height()};
    if (world->dish().active())
        camera.zoom = std::min(viewport.width, viewport.height) / (2.3f * world->dish().radius);
    plateDirty = false;
}

RunProvenance App::provenance() const {
    RunProvenance p;
    const SubstepSolver* solver = world ? world->substepSolver() : nullptr;
    p.engine = solver ? solver->name() : "CPU";
    if (solver)
        p.gpu = gpu.deviceName();
    p.lod = world && world->lodStride > 1;
    p.protocol = protocol;
    p.unlocked = unlockedParameters;
    return p;
}

void App::setParameter(const ParamInfo& p, float value) {
    if (!world)
        return;
    Intervention iv;
    iv.kind = InterventionKind::Parameter;
    iv.key = p.key;
    iv.v[0] = value;
    intervene(iv);
    if (&p == findParameter("tiempo.escala_biologica"))
        options.bioTimeScale = value; // la preferencia de la próxima placa
}

void App::pushMediumLive() {
    if (!world)
        return;
    const MediumParams& s = plate.config.medium;
    Intervention iv;
    iv.kind = InterventionKind::Medium;
    iv.v[0] = s.temperature;
    iv.v[1] = s.salinity;
    iv.v[2] = s.surfaceLight;
    iv.v[3] = s.o2AirExchange;
    iv.v[4] = s.alkalinity;
    iv.v[5] = s.anoxic ? 1.0f : 0.0f;
    intervene(iv);
    // El aire sobre el medio (CO₂ de la incubadora) va aparte: no entra en los 6 valores de arriba.
    const MediumParams& now = world->medium();
    if (now.co2Atmosphere != s.co2Atmosphere || now.carbonatePK != s.carbonatePK || now.osmolytes != s.osmolytes) {
        Intervention air;
        air.kind = InterventionKind::Atmosphere;
        air.v[0] = s.co2Atmosphere;
        air.v[1] = s.carbonatePK;
        air.v[2] = s.osmolytes;
        intervene(air);
    }
}

void App::pushNutrientLive(int soluteIndex, float target) {
    if (!world)
        return;
    Intervention iv; // entra al balance como entrada externa
    iv.kind = InterventionKind::Concentration;
    iv.index = soluteIndex;
    iv.v[0] = target;
    intervene(iv);
}

void App::exportData() {
    if (!world)
        return;
    session.sample(*world); // incluye el estado de este instante
    const std::string base = (std::filesystem::path(exportsDir) / timestampedName(plateName)).string();
    // La escena (.plate con el registro de intervenciones) acompaña a los datos: con ella la
    // corrida se puede repetir.
    const bool ok = writeTextFile(base + ".csv", seriesCsv(session.series())) &&
                    writeTextFile(base + ".json", runJson(*world, session, runPlate, provenance())) &&
                    saveScene(base + ".plate", runPlate, session.interventions().entries(), session.notes()) &&
                    (session.notes().empty() || writeTextFile(base + "_notas.md", "# " + std::string(ui::tr("Notas")) + "\n\n" +
                                                                                       notesMarkdown(session.notes()))) &&
                    writeTextFile((std::filesystem::path(exportsDir) / "especies.json").string(), speciesJson());
    lastExport = ok ? base : std::string();
    notify(ok ? std::string(ui::tr("Datos exportados: ")) + base + ".csv/.json/.plate" : ui::tr("No se pudo exportar"));
}

void App::setTimelapse(bool on) {
    timelapse = on;
    if (!on || !world)
        return;
    timelapseDir = (std::filesystem::path(photosDir) / timestampedName("timelapse")).string();
    timelapseClock.start(world->simTime() * world->bioTimeScale() / 3600.0);
}

void App::takePhotos() {
    if (!world)
        return;
    const bool frame = timelapse && timelapseClock.due(world->simTime() * world->bioTimeScale() / 3600.0);
    if (!photoRequested_ && !frame)
        return;
    // El campo del centro libre del dockspace, con su zoom (sin la UI encima).
    Camera2D cam = camera;
    cam.target = GetScreenToWorld2D({viewport.x + 0.5f * viewport.width, viewport.y + 0.5f * viewport.height}, camera);
    const int w = std::max(64, static_cast<int>(viewport.width)), h = std::max(64, static_cast<int>(viewport.height));
    PhotoInfo info;
    info.plate = !runPlate.prepared.empty() ? runPlate.prepared : biome(static_cast<BiomeId>(runPlate.biome)).key;
    info.seed = runPlate.seed;
    if (const StainSpec* st = activeStain())
        info.stain = st->id;
    info.provenance = std::string(BIO_VERSION) + " " + BIO_GIT;
    info.lessonOrScene = protocol.empty() ? plateName : protocol;
    std::error_code ec;
    if (photoRequested_) {
        photoRequested_ = false;
        std::filesystem::create_directories(photosDir, ec);
        const std::string path = (std::filesystem::path(photosDir) / (timestampedName(plateName) + ".png")).string();
        const bool ok = savePhoto(*renderer, *world, cam, tools.overlay, w, h, path, info);
        lastPhoto = ok ? path : std::string();
        notify(ok ? std::string(ui::tr("Foto guardada: ")) + path : ui::tr("No se pudo guardar la foto"));
    }
    if (frame) {
        std::filesystem::create_directories(timelapseDir, ec);
        const std::string path = (std::filesystem::path(timelapseDir) / timelapseClock.frameName()).string();
        if (!savePhoto(*renderer, *world, cam, tools.overlay, w, h, path, info)) {
            timelapse = false;
            notify(ui::tr("No se pudo guardar el time-lapse"));
        }
    }
}

void App::addNote(NoteAnchor anchor, const std::string& text) {
    if (!world || text.empty())
        return;
    Note n;
    n.step = world->stepIndex();
    n.hours = world->simTime() * world->bioTimeScale() / 3600.0;
    n.anchor = anchor;
    n.text = text;
    const Vector2 mid = GetScreenToWorld2D({viewport.x + 0.5f * viewport.width, viewport.y + 0.5f * viewport.height}, camera);
    n.pos = {mid.x, mid.y};
    if (anchor == NoteAnchor::Being) {
        Picked p = selection.picked;
        Vec2 at;
        if (!p.live() || !locatePicked(*world, p, at))
            return;
        n.pos = at;
        n.pickedKind = static_cast<int>(p.kind);
        n.ref = p.kind == Picked::Kind::Cell || p.kind == Picked::Kind::Rock ? static_cast<std::uint32_t>(p.entity) : p.id;
        n.being = buildInspection(*world, p).title;
    }
    session.notes().push_back(std::move(n));
    focusNote = static_cast<int>(session.notes().size()) - 1;
}

void App::showTerm(int index) {
    glossaryIndex = index;
    panels.show("glosario");
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
    add("file.photo", "Foto del campo (PNG y JSON)", "Archivo", ImGuiKey_F12, false, [this] { requestPhoto(); });
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
        [this] {
            tools.modelView = !tools.modelView;
            options.modelView = tools.modelView;
            saveOptions();
        },
        {}, [this] { return tools.modelView; });
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
            const PreparedPlate* prepared = runPlate.prepared.empty() ? nullptr : findPreparedPlate(runPlate.prepared);
            ImGui::Text("%s  seed %llu", prepared ? prepared->name.c_str() : biome(static_cast<BiomeId>(worldBiome)).name.c_str(),
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

void App::autosave(bool clean) {
    if (!world || autosavePath_.empty() || recoverTarget_ > 0)
        return;
    std::string text = serializeScene(runPlate, session.interventions().entries(), session.notes());
    text += "autosave_step=" + std::to_string(world->stepIndex()) + "\n";
    text += clean ? "autosave_clean=1\n" : "autosave_clean=0\n";
    writeTextFile(autosavePath_, text);
}

void App::drawRecoveryPrompt() {
    if (recoverTarget_ > 0) {
        // Barra de progreso de la recuperación.
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({vp->Pos.x + 0.5f * vp->Size.x, vp->Pos.y + 0.5f * vp->Size.y}, ImGuiCond_Always,
                                {0.5f, 0.5f});
        ImGui::SetNextWindowSize({420.0f, 0.0f});
        if (ImGui::Begin("##recuperando", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted(ui::tr("Recuperando la sesión anterior (re-simulando)..."));
            const float done = static_cast<float>(world->stepIndex()) / static_cast<float>(recoverTarget_);
            ImGui::ProgressBar(done);
            if (ImGui::Button(ui::tr("Detener aquí"))) {
                recoverTarget_ = 0;
                tools.paused = true;
            }
        }
        ImGui::End();
        return;
    }
    if (!recoveryPrompt_)
        return;
    ImGui::OpenPopup("###recuperar");
    if (ImGui::BeginPopupModal((std::string(ui::tr("Sesión anterior")) + "###recuperar").c_str(), nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(ui::tr("El programa se cerró sin terminar la sesión anterior."));
        ImGui::Text("%s %.1f min", ui::tr("Se puede recuperar re-simulándola hasta donde quedó:"),
                    static_cast<double>(recoveryStep_) * kFixedDt / 60.0);
        ImGui::TextDisabled("%s", ui::tr("Puede tardar más o menos lo mismo que duró."));
        if (ImGui::Button(ui::tr("Recuperar"))) {
            std::vector<Intervention> events;
            std::vector<std::string> missing;
            if (loadScene(autosavePath_, plate, events, &missing, &plateNotes)) {
                for (const std::string& m : missing)
                    notify(std::string(ui::tr("Falta contenido: ")) + m);
                plateScript = std::move(events);
                createPlate();
                recoverTarget_ = std::max<std::uint64_t>(recoveryStep_, 1);
            }
            recoveryPrompt_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(ui::tr("Descartar"))) {
            recoveryPrompt_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
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
    if (const ImGuiDockNode* node = ImGui::DockBuilderGetCentralNode(dock)) {
        const Rectangle old = viewport;
        viewport = {node->Pos.x, node->Pos.y, std::max(64.0f, node->Size.x), std::max(64.0f, node->Size.y)};
        // El área de la placa cambió (ventana, pantalla completa, paneles): se sigue viendo lo
        // mismo, con el mismo punto del mundo en el centro y el zoom en proporción.
        if (old.width != viewport.width || old.height != viewport.height) {
            const Vector2 center = GetScreenToWorld2D({old.x + 0.5f * old.width, old.y + 0.5f * old.height}, camera);
            camera.zoom *= std::min(viewport.width, viewport.height) / std::min(old.width, old.height);
            camera.offset = {viewport.x + 0.5f * viewport.width, viewport.y + 0.5f * viewport.height};
            camera.target = center;
        }
    }

    ImGui::BeginDisabled(recoverTarget_ > 0); // recuperando: nada de cambios a mitad del replay
    panels.drawAll(*this);
    ImGui::EndDisabled();
    drawRecoveryPrompt();

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

    // Toda acción pasa por el registro de intervenciones (reproducible). Las de mantener son
    // estados: se anotan solo cuando cambian y se sueltan solas al soltar el botón o cambiar de
    // herramienta.
    auto at = [&](InterventionKind kind) {
        Intervention iv;
        iv.kind = kind;
        iv.pos = pos;
        return iv;
    };
    {
        Intervention sq = at(InterventionKind::Squeeze);
        sq.active = tools.tool == Tool::Squeeze && down;
        sq.v[0] = sq.active ? tools.squeezeSpeed : 0.0f;
        if (!sq.active)
            sq.pos = {};
        interveneState(sq);
    }
    {
        Intervention st = at(InterventionKind::Stir);
        st.active = tools.tool == Tool::Stir && down && dt > 0.0f;
        if (st.active) {
            const Vector2 d = Vector2Scale(GetMouseDelta(), 1.0f / (camera.zoom * dt * units::kTimeScale));
            st.v[0] = d.x;
            st.v[1] = d.y;
            st.v[2] = tools.stirRadius;
        } else {
            st.pos = {};
        }
        interveneState(st);
    }
    {
        Intervention ink = at(InterventionKind::Ink);
        ink.active = tools.tool == Tool::Ink && down;
        ink.v[0] = ink.active ? tools.inkRadius : 0.0f;
        if (!ink.active)
            ink.pos = {};
        interveneState(ink);
        Intervention drop = at(InterventionKind::Dropper);
        drop.active = tools.tool == Tool::Dropper && down;
        drop.index = tools.dropper;
        if (drop.index == 7) // el reactivo elegido en Herramientas (game/reagent)
            drop.key = tools.reagent;
        drop.v[0] = drop.active ? tools.dropRadius : 0.0f;
        drop.v[1] = drop.active ? tools.dropAmount : 0.0f;
        if (!drop.active)
            drop.pos = {};
        interveneState(drop);
    }
    if (tools.grabbing && (!held || tools.tool != Tool::Grab)) {
        intervene(at(InterventionKind::GrabEnd));
        tools.grabbing = false;
    }

    switch (tools.tool) {
    case Tool::Ink:
    case Tool::Squeeze:
    case Tool::Dropper:
        break; // estados, arriba
    case Tool::Detritus:
        if (pressed)
            intervene(at(InterventionKind::Detritus));
        break;
    case Tool::Erase:
        if (pressed)
            intervene(at(InterventionKind::Erase));
        break;
    case Tool::Lyse:
        if (pressed)
            intervene(at(InterventionKind::Lyse));
        break;
    case Tool::Grab:
        if (pressed)
            tools.grabbing = intervene(at(InterventionKind::GrabBegin));
        else if (tools.grabbing && held)
            interveneState(at(InterventionKind::GrabMove));
        break;
    case Tool::Spawn:
        if (pressed && catalog.armed >= 0) {
            Intervention iv = at(InterventionKind::Spawn);
            const Being b = beingAt(catalog.armed);
            iv.index = static_cast<int>(b.kind);
            iv.v[0] = static_cast<float>(static_cast<int>(b.species));
            intervene(iv);
        }
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
        Intervention pick;
        pick.kind = InterventionKind::PlayerPick;
        pick.pos = pos;
        pick.v[0] = pickTolerance(camera.zoom);
        if (intervene(pick)) {
            tools.follow = true;
            panels.show("estadisticas");
        }
    }
    PlayerInput input;
    // La entrada del ser controlado también queda en el registro (solo cuando cambia).
    auto send = [&] {
        Intervention iv;
        iv.kind = InterventionKind::PlayerInput;
        iv.v[0] = input.forward;
        iv.v[1] = input.turn;
        iv.v[2] = input.propel ? 1.0f : 0.0f;
        iv.v[3] = input.pseudopod ? 1.0f : 0.0f;
        iv.pos = input.aim;
        interveneState(iv);
    };
    if (!world->registry().valid(world->player())) {
        send();
        return;
    }
    const bool lmb = mouseFree && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && tools.tool == Tool::Control && !leftPressed;
    const bool mouseDrive = lmb || (leftPressed && tools.tool == Tool::Control);
    input.forward = kbFree ? (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f) : 0.0f;
    input.turn = kbFree ? (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f) : 0.0f;
    input.propel = (kbFree && IsKeyDown(KEY_W)) || mouseDrive;
    input.pseudopod = mouseDrive;
    input.aim = pos;
    send();
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

void App::stepCultures(float dt) {
    if (culturesPaused || cultures.empty())
        return;
    // Cada cultivo avanza en pasos fijos (el resultado no depende de los FPS); con muchos o muy
    // rápidos, lo que no entra en ~10 ms queda para el cuadro siguiente.
    const double t0 = GetTime();
    for (const auto& c : cultures) {
        c->advance(static_cast<double>(cultureSpeed) * 3600.0 * std::min(dt, 0.1f));
        if (GetTime() - t0 > 0.010)
            break;
    }
}

const char* App::protocolLockReason(const char* key) const {
    for (const auto& [k, reason] : protocolLocks)
        if (k == key && std::find(unlockedParameters.begin(), unlockedParameters.end(), k) == unlockedParameters.end())
            return reason.c_str();
    return nullptr;
}

void App::unlockParameter(const std::string& key) {
    if (std::find(unlockedParameters.begin(), unlockedParameters.end(), key) == unlockedParameters.end())
        unlockedParameters.push_back(key);
    notify(std::string(ui::tr("Fuera de protocolo: se destrabó ")) + key);
}

void App::openSampleDrop(const PlateSpec& drop) {
    plate = drop;
    plateScript.clear();
    plateNotes.clear();
    createPlate();
    notify(ui::tr("Gota de muestra del cultivo: la placa del centro."));
}

void App::stepSimulation(float frameDt) {
    if (recoverTarget_ > 0) {
        // Recuperación: re-simula lo grabado (incluido el foco) tan rápido como se pueda, con
        // ~30 ms por cuadro para que la ventana siga respondiendo.
        const double t0 = GetTime();
        while (world->stepIndex() < recoverTarget_ && GetTime() - t0 < 0.03 && !world->fixed()) {
            session.interventions().beforeStep(*world, kFixedDt);
            world->step(kFixedDt);
            session.update(*world, kFixedDt);
        }
        if (world->stepIndex() >= recoverTarget_ || world->fixed()) {
            recoverTarget_ = 0;
            tools.paused = true;
            notify(ui::tr("Sesión recuperada (en pausa)."));
        }
        accumulator_ = 0.0f;
        return;
    }
    const Vector2 a = GetScreenToWorld2D({viewport.x, viewport.y}, camera);
    const Vector2 b = GetScreenToWorld2D({viewport.x + viewport.width, viewport.y + viewport.height}, camera);
    // LOD: lo que se ve (más un margen) se simula en detalle.
    {
        Intervention focus; // con LOD, la vista decide qué se resuelve en detalle: queda registrada
        focus.kind = InterventionKind::Focus;
        focus.active = true;
        focus.pos = {camera.target.x, camera.target.y};
        focus.v[0] = 0.5f * std::abs(b.x - a.x);
        focus.v[1] = 0.5f * std::abs(b.y - a.y);
        interveneState(focus);
    }
    if (world->fixed()) { // muestra fijada: no hay pasos (ni filas nuevas en la serie)
        accumulator_ = 0.0f;
        tools.stepRequests = 0;
        return;
    }
    auto advance = [&] {
        const double t0 = GetTime();
        session.interventions().beforeStep(*world, kFixedDt); // escena/replay y pinceles
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
    // Notas ancladas a un ser o a un lugar: un marcador numerado (al repetir una escena, aparece
    // cuando la placa llega a su paso). Al pasar el mouse, su texto.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float* accent = ui::palette().accent;
    const ImU32 noteColor = ImGui::ColorConvertFloat4ToU32({accent[0], accent[1], accent[2], 0.95f});
    for (std::size_t i = 0; i < session.notes().size(); ++i) {
        const Note& n = session.notes()[i];
        Vec2 at;
        if (n.step > world->stepIndex() || !notePosition(*world, n, at))
            continue;
        const ImVec2 c = screen(at);
        const float r = 9.0f;
        dl->AddCircleFilled(c, r, noteColor);
        dl->AddCircle(c, r, IM_COL32(20, 24, 24, 220), 0, 1.5f);
        std::snprintf(text, sizeof text, "%zu", i + 1);
        const ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText({c.x - 0.5f * ts.x, c.y - 0.5f * ts.y}, IM_COL32(20, 24, 24, 255), text);
        const float dx = mouse.x - c.x, dy = mouse.y - c.y;
        if (!ImGui::GetIO().WantCaptureMouse && dx * dx + dy * dy < r * r)
            ImGui::SetTooltip("%s %zu (t = %.2f h)\n%s", ui::tr("Nota"), i + 1, n.hours, n.text.c_str());
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
        ToggleBorderlessWindowed();

    ui::loadLanguage(options.language);
    ui::initBackend(layoutPath_.c_str(), 15.0f);
    ui::applyTheme();
    // Paquetes de contenido del usuario (<datos>/biogame/paquetes): antes del catálogo y de la
    // primera placa, así sus seres tienen ícono y las placas que los nombran se leen.
    {
        std::vector<std::string> messages;
        loadUserPacks(messages);
        if (!messages.empty())
            notify(std::string(ui::tr("Paquetes de contenido con avisos: ver el panel Contenido")));
    }
    ui::buildBeingIcons();
    ui::registerPanels(panels);
    registerCommands();

    renderer = std::make_unique<Renderer>();
    gpuOk = gpu.init();
    camera.zoom = 1.0f;
    camera.offset = {kScreenW * 0.5f, kScreenH * 0.5f};
    applyOptions();
    // ¿Quedó una sesión sin cerrar bien? (se ofrece recuperarla)
    autosavePath_ = (std::filesystem::path(optionsPath).parent_path() / "autoguardado.plate").string();
    if (std::ifstream in(autosavePath_); in) {
        std::string line;
        bool clean = true;
        while (std::getline(in, line)) {
            if (line.rfind("autosave_step=", 0) == 0)
                recoveryStep_ = std::strtoull(line.c_str() + 14, nullptr, 10);
            else if (line == "autosave_clean=0")
                clean = false;
        }
        recoveryPrompt_ = !clean && recoveryStep_ > 0;
    }
    createPlate(); // arranca con la placa preconfigurada (vacía por defecto)

    while (!WindowShouldClose() && !quit) {
        if (contentChanged) {
            ui::freeBeingIcons();
            ui::buildBeingIcons();
            contentChanged = false;
        }
        applyFrameCap();
        const float frameDt = GetFrameTime();
        tools.frameHistory[tools.frameHead] = 1000.0f * frameDt;
        tools.frameHead = (tools.frameHead + 1) % ToolState::kFrameHistory;
        const double u0 = GetTime();
        ui::beginFrame(frameDt);
        buildUi(frameDt);
        commands.handleShortcuts();
        if (recoverTarget_ == 0) // recuperando: nada de intervenciones nuevas
            worldInput(frameDt);
        const double s0 = GetTime();
        tools.uiMs += (static_cast<float>(1000.0 * (s0 - u0)) - tools.uiMs) * 0.1f;
        stepSimulation(frameDt);
        stepCultures(frameDt);
        tools.simFrameMs += (static_cast<float>(1000.0 * (GetTime() - s0)) - tools.simFrameMs) * 0.1f;
        autosaveTimer_ += frameDt;
        if (autosaveTimer_ >= 120.0f) { // cada 2 min reales
            autosaveTimer_ = 0.0f;
            autosave(false);
        }

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
        if (recoverTarget_ == 0)
            takePhotos();
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

    autosave(true); // cierre limpio: no se ofrece recuperar
    saveProgress(progress, progressPath);
    saveOptions();
    world.reset();
    renderer.reset();
    ui::freeBeingIcons();
    ui::shutdownBackend();
    CloseWindow();
    return 0;
}
