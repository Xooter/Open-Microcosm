#include "app/app.hpp"

#include "app/tool_icons.hpp"

#include "bio/locomotion.hpp"
#include "core/profiling.hpp"
#include "core/units.hpp"
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
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
Vec2 ringCenter(const World& world, const Picked& p, const Inspection& ins) {
    if ((p.kind == Picked::Kind::Cell || p.kind == Picked::Kind::Rock) && world.registry().valid(p.entity) &&
        world.registry().all_of<RingBody>(p.entity))
        return world.registry().get<RingBody>(p.entity).centroid;
    return ins.at;
}

void drawRing(Vec2 c, float r, float zoom, Color color, float thickness) {
    DrawRing({c.x, c.y}, r, r + thickness / zoom, 0, 360, 72, color);
}

// Concentración media del agua [µM].
float meanOf(const World& world, const ChemField& field) {
    const Grid2D& g = field.grid();
    const float area = world.dish().active() ? units::kPi * world.dish().radius * world.dish().radius
                                             : g.width() * g.height();
    return static_cast<float>(field.sum() / std::max(1.0f, area / (g.dx * g.dx)));
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

void App::setFullscreen(bool on) {
    if (IsWindowFullscreen() != on)
        ToggleFullscreen();
    options.fullscreen = on;
}

void App::applyOptions() {
    if (renderer)
        renderer->setPostProcess(options.postProcess);
    if (world) {
        world->lodStride = options.lod ? 5 : 1;
        world->setSubstepSolver(options.gpuPhysics && gpuOk ? &gpu : nullptr);
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
    ChemField& f = soluteField(*world, soluteIndex);
    const float mean = meanOf(*world, f);
    if (mean > 1e-12f) {
        const float k = target / mean;
        for (float& v : f.grid().v)
            v *= k;
    } else {
        std::fill(f.grid().v.begin(), f.grid().v.end(), target);
    }
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
    add("view.stain", "Cambiar tinción", "Capas", ImGuiKey_J, false, [this] { tools.stain = (tools.stain + 1) % 3; });

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
            if (i == 2 || i == 10)
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
            ImGui::Text("%s: %s", ui::tr("Enciclopedia"), t.c_str());
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

    const ImGuiID dock = ImGui::DockSpaceOverViewport(0, vp, ImGuiDockNodeFlags_PassthruCentralNode);
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
        camera.zoom = Clamp(camera.zoom * (1.0f + 0.1f * wheel), minZoom, 60.0f);
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
static void followCell(Camera2D& cam, const World& world, entt::entity cell, const Rectangle& vp, float dt) {
    if (!world.registry().valid(cell))
        return;
    const Vec2 c = world.registry().get<RingBody>(cell).centroid;
    cam.offset = {vp.x + 0.5f * vp.width, vp.y + 0.5f * vp.height};
    const float k = 1.0f - std::exp(-6.0f * dt);
    cam.target.x = wrapCoord(cam.target.x + minImage(c.x - cam.target.x, world.width()) * k, world.width());
    cam.target.y = wrapCoord(cam.target.y + minImage(c.y - cam.target.y, world.height()) * k, world.height());
}

// Elegir lo que está bajo el cursor: resalta (tooltip) y el click abre su ficha. Las fichas
// vivas se rearman ~10 veces por segundo; si el ser fue reemplazado (la zoóspora que salió de la
// célula, la cría que salió del huevo), la ficha pasa a su sucesor.
void App::selectionInput(float dt, const Vector2&, const Vec2& pos) {
    SelectionState& st = selection;
    const bool mouseFree = !ImGui::GetIO().WantCaptureMouse;
    const float tolerance = std::max(2.0f, 6.0f / camera.zoom); // ~6 px en pantalla

    st.hoverTimer -= dt;
    if (!mouseFree || tools.tool != Tool::Select) {
        st.hover = {};
        st.hoverText.clear();
    } else if (st.hoverTimer <= 0.0f) {
        st.hoverTimer = 0.05f;
        st.hover = world->pick(pos, tolerance);
        st.hoverText.clear();
        if (st.hover) {
            const Inspection h = buildInspection(*world, st.hover);
            st.hoverText = h.title;
            if (!h.headline.empty())
                st.hoverText += " - " + h.headline;
        }
    }

    st.pickedAge += dt;
    // Un click que no toca nada no cambia nada: con una tolerancia de pocos píxeles es fácil
    // errarle a algo chico, y no hay que perder la ficha que se estaba leyendo (Esc la cierra).
    if (mouseFree && tools.tool == Tool::Select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (const Picked hit = world->pick(pos, tolerance)) {
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
    default: break;
    }
}

// Controlar: elegir la célula y moverla. Cilios y ondulación con W/S/A/D; flagelo con W o
// manteniendo el click (gira hacia el mouse); pseudópodo con el click.
void App::controlInput(bool mouseFree, const Vec2& pos) {
    const bool kbFree = !ImGui::GetIO().WantTextInput;
    const bool leftPressed = mouseFree && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if ((tools.tool == Tool::Control && leftPressed) || (kbFree && IsKeyPressed(KEY_P))) {
        const Picked hit = world->pick(pos, std::max(2.0f, 6.0f / camera.zoom));
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
    const int maxSteps = tools.simSpeed > 1.0f ? 8 : 4;
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
            toasts.insert(toasts.end(), fresh.begin(), fresh.end());
            if (toasts.size() > 5)
                toasts.erase(toasts.begin(), toasts.end() - 5);
            toastTimer = 5.0f;
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
            drawRing(ringCenter(*world, st.picked, st.inspection), st.inspection.radius, camera.zoom,
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
    EndMode2D();
}

// ---------------------------------------------------------------------------
// Bucle principal
// ---------------------------------------------------------------------------

int App::run() {
    // Un hilo por núcleo físico: con SMT, los hilos hermanos compiten por la misma FPU y las
    // barreras de OpenMP esperan al más lento (medido: 8 hilos ~1,6× más rápido que 16).
    if (!std::getenv("OMP_NUM_THREADS"))
        omp_set_num_threads(std::max(1, omp_get_num_procs() / 2));
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
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
        const float frameDt = GetFrameTime();
        ui::beginFrame(frameDt);
        buildUi(frameDt);
        commands.handleShortcuts();
        worldInput(frameDt);
        stepSimulation(frameDt);

        // La cámara sigue al ser controlado o al elegido.
        if (tools.followSelected && selection.followable() && world->registry().valid(selection.picked.entity))
            followCell(camera, *world, selection.picked.entity, viewport, frameDt);
        else if (tools.follow)
            followCell(camera, *world, world->player(), viewport, frameDt);
        discover(frameDt);

        renderer->setStain(static_cast<Stain>(tools.stain));
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
