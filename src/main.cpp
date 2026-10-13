#include "core/profiling.hpp"
#include "core/ui.hpp"
#include "core/units.hpp"
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
#include "game/progress.hpp"
#include "game/run.hpp"
#include "game/world.hpp"
#include "gpu/gpu_substep_solver.hpp"
#include "render/renderer.hpp"
#include "sim/polygon.hpp"

#include <raylib.h>
#include <raymath.h>

#include <omp.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <random>
#include <vector>

namespace {

constexpr int kScreenW = 1600;
constexpr int kScreenH = 900;

// La simulación avanza a paso fijo (determinista y estable), desacoplada del framerate.
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr int kMaxStepsPerFrame = 4;

enum class Screen { Title, Setup, Encyclopedia, Options, Playing };

struct Game {
    std::unique_ptr<World> world;
    Run run;
    Progress progress;
    std::string progressPath = defaultProgressPath();
    Options options;
    std::string optionsPath = defaultOptionsPath();
    SetupState setup;
    SandboxState sandbox;
    ExploreState explore; // modo Explorar (inspeccionar seres)
    float encyclopediaScroll = 0.0f;
    std::vector<std::string> toasts; // descubrimientos recientes
    float toastTimer = 0.0f, discoveryTimer = 0.0f;
    std::uint32_t runCounter = 0;
    GpuSubstepSolver* gpu = nullptr; // XPBD en GPU (fase 14); nullptr si no hay OpenGL 4.3
    Screen screen = Screen::Title;
};

void startRun(Game& game) {
    // Gota procedural: el bioma elegido con las variaciones de la semilla.
    WorldConfig config = makeDrop(static_cast<BiomeId>(game.setup.biome), game.setup.seed);
    const bool empty = game.setup.content != DropContent::Full;
    applyDropContent(config, game.setup.content); // sandbox: solo el agua y la química del bioma
    game.world = std::make_unique<World>(config);
    game.world->bioTimeScale() = game.options.bioTimeScale;
    game.world->lodStride = game.options.lod ? 5 : 1;
    if (game.options.gpuPhysics && game.gpu && game.gpu->ready())
        game.world->setSubstepSolver(game.gpu);
    game.progress.biomesPlayed |= 1u << static_cast<unsigned>(game.setup.biome);
    const LocomotionType founder = game.setup.founder;
    RunConfig rc;
    rc.founder = founder;
    rc.wildlife = empty ? 0.0f : dropAreaScale(config); // misma densidad de especies en cualquier placa
    rc.biome = game.setup.biome;
    rc.seed = config.seed;
    rc.genome = speciesGenome(founder);
    game.sandbox = SandboxState{};
    game.explore = ExploreState{}; // las entidades del mundo anterior ya no existen
    game.run.start(*game.world, rc, ++game.runCounter);
    game.screen = Screen::Playing;
}

// `wheelCaptured`: la rueda es de la ficha (scroll), no del zoom.
void updateCamera(Camera2D& cam, ToolState& tools, const World& world, bool wheelCaptured) {
    const Dish& dish = world.dish();
    // Con placa, el alejamiento máximo muestra la placa entera (con el canto de vidrio).
    const float minZoom = dish.active()
        ? std::min(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())) / (2.3f * dish.radius)
        : 0.4f;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        tools.follow = false; // mover la cámara a mano suelta el seguimiento
        tools.followInspected = false;
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / cam.zoom);
        cam.target = Vector2Add(cam.target, delta);
    }
    if (float wheel = wheelCaptured ? 0.0f : GetMouseWheelMove(); wheel != 0.0f) {
        if (!tools.follow && !tools.followInspected) { // zoom centrado en el cursor
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), cam);
            cam.offset = GetMousePosition();
            cam.target = mouseWorld;
        }
        cam.zoom = Clamp(cam.zoom * (1.0f + 0.1f * wheel), minZoom, 60.0f);
    }
    cam.zoom = std::max(cam.zoom, minZoom);
    if (dish.active()) {
        // El centro de la vista no sale de la placa.
        const Vector2 mid = {0.5f * static_cast<float>(GetScreenWidth()), 0.5f * static_cast<float>(GetScreenHeight())};
        const Vector2 c = GetScreenToWorld2D(mid, cam);
        const Vec2 inside = dish.clampInside({c.x, c.y});
        cam.offset = mid;
        cam.target = {inside.x, inside.y};
    }
}

// Cámara de microscopio: sigue a una célula con suavidad (sin placa, por el camino corto del
// toro).
void followCell(Camera2D& cam, const World& world, entt::entity cell, float dt) {
    if (!world.registry().valid(cell))
        return;
    const Vec2 c = world.registry().get<RingBody>(cell).centroid;
    cam.offset = {GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f};
    const float k = 1.0f - std::exp(-6.0f * dt);
    cam.target.x = wrapCoord(cam.target.x + minImage(c.x - cam.target.x, world.width()) * k, world.width());
    cam.target.y = wrapCoord(cam.target.y + minImage(c.y - cam.target.y, world.height()) * k, world.height());
}

void toggleOverlay(ToolState& tools, Overlay overlay) {
    tools.overlay = tools.overlay == overlay ? Overlay::None : overlay;
}

// Herramientas de laboratorio y controles del jugador durante la partida.
// `uiCaptured`: el click es de la barra del sandbox (o spawnea), no del jugador.
// `followKeyTaken`: F es de la ficha (seguir al inspeccionado), no de la cámara del jugador.
void handlePlayInput(World& world, ToolState& tools, const Camera2D& camera, bool uiCaptured, bool followKeyTaken) {
    if (IsKeyPressed(KEY_SPACE)) tools.paused = !tools.paused;
    if (IsKeyPressed(KEY_F1)) toggleOverlay(tools, Overlay::Velocity);
    if (IsKeyPressed(KEY_F2)) toggleOverlay(tools, Overlay::Oxygen);
    if (IsKeyPressed(KEY_F3)) toggleOverlay(tools, Overlay::Nutrients);
    if (IsKeyPressed(KEY_F4)) toggleOverlay(tools, Overlay::Light);
    if (IsKeyPressed(KEY_F5)) toggleOverlay(tools, Overlay::Phosphate);
    if (IsKeyPressed(KEY_F6)) toggleOverlay(tools, Overlay::Nitrogen);
    if (IsKeyPressed(KEY_F7)) toggleOverlay(tools, Overlay::PH);
    if (IsKeyPressed(KEY_F8)) toggleOverlay(tools, Overlay::Toxin);
    if (IsKeyPressed(KEY_F9)) toggleOverlay(tools, Overlay::Autoinducer);
    if (IsKeyPressed(KEY_F10)) toggleOverlay(tools, Overlay::Phage);
    if (IsKeyPressed(KEY_F) && !followKeyTaken) tools.follow = !tools.follow;
    if (IsKeyPressed(KEY_J)) tools.stain = (tools.stain + 1) % 3;
    if (IsKeyPressed(KEY_H)) tools.showLab = !tools.showLab;

    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    // Con placa el mundo es cerrado: afuera del agua no hay nada que tocar.
    const bool wrapMouse = !world.dish().active();
    const Vec2 pos = wrapMouse ? Vec2{wrapCoord(mouse.x, world.width()), wrapCoord(mouse.y, world.height())}
                               : world.dish().clampInside({mouse.x, mouse.y});
    if (IsKeyDown(KEY_T)) world.dropInk(pos, tools.inkRadius, 40);
    if (IsKeyPressed(KEY_B)) world.dropDetritus(pos);
    if (IsKeyPressed(KEY_C)) world.spawnCell(world.dish().clampInside(pos, 60.0f));
    if (IsKeyPressed(KEY_K)) world.spawnTestScene(50);
    if (IsKeyPressed(KEY_X)) world.removeCellAt(pos);
    if (IsKeyDown(KEY_G)) applyDropper(world, tools, pos.x, pos.y, GetFrameTime());
    if (IsKeyPressed(KEY_P)) { // controlar la célula bajo el cursor
        const Picked hit = world.pick(pos, std::max(2.0f, 6.0f / camera.zoom));
        if (hit.kind == Picked::Kind::Cell && world.registry().all_of<Locomotion>(hit.entity)) {
            world.setPlayer(hit.entity);
            tools.follow = true;
        }
    }
    if (IsKeyPressed(KEY_L)) world.lyseAt(pos);
    if (IsKeyPressed(KEY_V)) world.tryConjugate(world.player()); // conjugar con una pareja cercana
    if (IsKeyDown(KEY_Q))
        world.setSqueeze(pos, tools.squeezeSpeed);
    else
        world.clearSqueeze();

    const bool overUi = uiCaptured || (tools.showLab && CheckCollisionPointRec(GetMousePosition(), labPanelRect()));
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool left = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !overUi;

    // Laboratorio: Shift = estirar membrana, Ctrl = revolver el agua.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && shift && !overUi)
        tools.grabbing = world.beginGrab(pos);
    if (tools.grabbing) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            world.moveGrab(pos);
        } else {
            world.endGrab();
            tools.grabbing = false;
        }
    }
    const float frameDt = GetFrameTime();
    if (left && ctrl && frameDt > 0.0f) {
        const Vector2 d = Vector2Scale(GetMouseDelta(), 1.0f / (camera.zoom * frameDt * units::kTimeScale));
        world.setStir(pos, {d.x, d.y}, tools.stirRadius);
    } else {
        world.clearStir();
    }

    // Jugador: cilios con W/S/A/D, flagelo con W (gira hacia el mouse), pseudópodo con click.
    PlayerInput input;
    input.forward = (IsKeyDown(KEY_W) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) ? 1.0f : 0.0f);
    input.turn = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_A) ? 1.0f : 0.0f);
    input.propel = IsKeyDown(KEY_W) || (left && !shift && !ctrl);
    input.pseudopod = left && !shift && !ctrl && !tools.grabbing;
    input.aim = pos;
    world.setPlayerInput(input);
}

// Sandbox: spawnear el ser armado con click (Esc cancela) y forzar la reproducción (R).
// Devuelve true si el click es del sandbox.
bool handleSandboxInput(World& world, SandboxState& sandbox, const ToolState& tools, const Camera2D& camera) {
    if (IsKeyPressed(KEY_ESCAPE))
        sandbox.armed = -1;
    if (IsKeyPressed(KEY_R))
        world.forceReproduction(world.player());
    const bool overBar = sandboxBarHovered();
    const bool overLab = tools.showLab && CheckCollisionPointRec(GetMousePosition(), labPanelRect());
    if (sandbox.armed < 0 || overBar)
        return overBar;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !overLab) {
        const Vector2 m = GetScreenToWorld2D(GetMousePosition(), camera);
        const Vec2 pos = world.dish().active() ? Vec2{m.x, m.y} : Vec2{wrapCoord(m.x, world.width()), wrapCoord(m.y, world.height())};
        world.spawnBeing(beingAt(sandbox.armed), pos);
    }
    return true; // con un ser armado, el click izquierdo no mueve al jugador
}

// Modo Explorar: lo que está bajo el cursor se resalta (tooltip) y el click abre su ficha.
// Las fichas vivas se rearman ~10 veces por segundo; si el ser fue reemplazado (la zoóspora que
// salió de la célula, la cría que salió del huevo), la ficha pasa a su sucesor.
bool exploreFollowable(const ExploreState& st) {
    return st.inspection.valid && !st.inspection.dead &&
           (st.picked.kind == Picked::Kind::Cell || st.picked.kind == Picked::Kind::Rock);
}

void handleExploreInput(const World& world, ExploreState& st, ToolState& tools, const Camera2D& camera, float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        st.picked = {};
        st.inspection = {};
        tools.followInspected = false;
    }
    if (IsKeyPressed(KEY_F) && exploreFollowable(st))
        tools.followInspected = !tools.followInspected;

    const Vector2 m = GetScreenToWorld2D(GetMousePosition(), camera);
    const Vec2 pos = world.dish().active() ? Vec2{m.x, m.y} : Vec2{wrapCoord(m.x, world.width()), wrapCoord(m.y, world.height())};
    const float tolerance = std::max(2.0f, 6.0f / camera.zoom); // ~6 px en pantalla
    const bool overUi = exploreUiHovered(st) || (tools.showLab && CheckCollisionPointRec(GetMousePosition(), labPanelRect()));

    st.hoverTimer -= dt;
    if (overUi) {
        st.hover = {};
        st.hoverText.clear();
    } else if (st.hoverTimer <= 0.0f) {
        st.hoverTimer = 0.05f;
        st.hover = world.pick(pos, tolerance);
        st.hoverText.clear();
        if (st.hover) {
            const Inspection h = buildInspection(world, st.hover);
            st.hoverText = h.title;
            if (!h.headline.empty())
                st.hoverText += " - " + h.headline;
        }
    }

    st.pickedAge += dt;
    // Un click que no toca nada no cambia nada: con una tolerancia de pocos pixeles es facil
    // errarle a algo chico, y no hay que perder la ficha que se estaba leyendo (Esc la cierra).
    if (!overUi && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (const Picked hit = world.pick(pos, tolerance)) {
            st.picked = hit;
            st.inspection = buildInspection(world, hit);
            st.scroll = 0.0f;
            st.refresh = 0.1f;
            st.pickedAge = 0.0f;
        }
    }

    if (st.picked.live()) {
        st.refresh -= dt;
        if (st.refresh <= 0.0f) {
            st.refresh = 0.1f;
            st.inspection = buildInspection(world, st.picked);
            const entt::entity next = st.inspection.successor;
            if (st.inspection.dead && next != entt::null && world.registry().valid(next)) {
                st.picked = {};
                st.picked.kind = Picked::Kind::Cell;
                st.picked.entity = next;
                st.inspection = buildInspection(world, st.picked);
            }
        }
    }
}

} // namespace

int main() {
    // Un hilo por núcleo físico: con SMT, los hilos hermanos compiten por la misma FPU y las
    // barreras de OpenMP esperan al más lento (medido: 8 hilos ~1,6× más rápido que 16).
    if (!std::getenv("OMP_NUM_THREADS"))
        omp_set_num_threads(std::max(1, omp_get_num_procs() / 2));
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(kScreenW, kScreenH, "bio");
    SetExitKey(KEY_NULL); // Esc no cierra el juego

    Game game;
    game.progress = loadProgress(game.progressPath);
    game.options = loadOptions(game.optionsPath);
    game.setup.seed = std::random_device{}() & 0x7fffffffu;
    if (game.options.fullscreen)
        ToggleFullscreen();
    Renderer renderer;
    renderer.setPostProcess(game.options.postProcess);
    GpuSubstepSolver gpu;
    if (gpu.init())
        game.gpu = &gpu;
    ToolState tools;
    tools.showLab = game.options.labPanel;
    Camera2D camera{};
    camera.zoom = 2.0f;
    camera.offset = {kScreenW * 0.5f, kScreenH * 0.5f};

    float accumulator = 0.0f;
    while (!WindowShouldClose()) {
        const float frameDt = GetFrameTime();

        if (game.screen == Screen::Title) {
            switch (drawTitleScreen(game.progress)) {
            case TitleAction::NewDrop: game.screen = Screen::Setup; break;
            case TitleAction::Encyclopedia: game.screen = Screen::Encyclopedia; break;
            case TitleAction::Options: game.screen = Screen::Options; break;
            case TitleAction::Quit: CloseWindow(); return 0;
            case TitleAction::None: break;
            }
            continue;
        }
        if (game.screen == Screen::Setup) {
            const SetupAction a = drawSetupScreen(game.progress, game.setup);
            if (a == SetupAction::Back)
                game.screen = Screen::Title;
            if (a == SetupAction::Start) {
                startRun(game);
                tools.followInspected = false;
                camera.target = {0.5f * game.world->width(), 0.5f * game.world->height()};
                game.setup.seed = std::random_device{}() & 0x7fffffffu; // la próxima, otra gota
            }
            continue;
        }
        if (game.screen == Screen::Encyclopedia) {
            if (drawEncyclopedia(game.progress, game.encyclopediaScroll))
                game.screen = Screen::Title;
            continue;
        }
        if (game.screen == Screen::Options) {
            const Options before = game.options;
            const bool back = drawOptions(game.options);
            if (game.options.fullscreen != before.fullscreen)
                ToggleFullscreen();
            renderer.setPostProcess(game.options.postProcess);
            if (back) {
                if (game.world)
                    game.world->setSubstepSolver(game.options.gpuPhysics && game.gpu ? game.gpu : nullptr);
                saveOptions(game.options, game.optionsPath);
                game.screen = Screen::Title;
            }
            continue;
        }

        World& world = *game.world;
        ExploreState& explore = game.explore;
        // Dos modos: Crear (sandbox: spawnear, cambiar de forma, reproducir) y Explorar (inspeccionar).
        auto setExplore = [&](bool on) {
            if (explore.active == on)
                return;
            explore = ExploreState{};
            explore.active = on;
            tools.followInspected = false;
            game.sandbox.armed = -1;
        };
        if (game.screen == Screen::Playing && IsKeyPressed(KEY_TAB))
            setExplore(!explore.active);
        updateCamera(camera, tools, world, (explore.active && exploreUiHovered(explore)) || labPanelHovered(tools));
        if (game.screen == Screen::Playing) {
            if (explore.active) {
                handleExploreInput(world, explore, tools, camera, frameDt);
                handlePlayInput(world, tools, camera, true, exploreFollowable(explore)); // el click es de la ficha
            } else {
                const bool captured = handleSandboxInput(world, game.sandbox, tools, camera);
                handlePlayInput(world, tools, camera, captured, false);
            }
        }
        if (IsKeyPressed(KEY_N)) { // volver al inicio para armar otra gota
            saveProgress(game.progress, game.progressPath);
            game.screen = Screen::Title;
            continue;
        }

        if (game.screen == Screen::Playing && !tools.paused) {
            // LOD: lo que se ve (más un margen) se simula en detalle.
            world.setFocus({camera.target.x, camera.target.y},
                           {0.5f * static_cast<float>(GetScreenWidth()) / camera.zoom,
                            0.5f * static_cast<float>(GetScreenHeight()) / camera.zoom});
            accumulator += frameDt * units::kTimeScale;
            int steps = 0;
            while (accumulator >= kFixedDt && steps < kMaxStepsPerFrame) {
                const double t0 = GetTime();
                world.step(kFixedDt);
                tools.stepMs += (static_cast<float>(1000.0 * (GetTime() - t0)) - tools.stepMs) * 0.1f;
                game.run.update(world, kFixedDt);
                accumulator -= kFixedDt;
                ++steps;
            }
            if (steps == kMaxStepsPerFrame)
                accumulator = 0.0f; // evita la "espiral de la muerte" si la sim no da abasto
        }
        // La cámara sigue al ser controlado.
        const entt::entity focus = world.player();
        if (explore.active && tools.followInspected && exploreFollowable(explore) && world.registry().valid(explore.picked.entity))
            followCell(camera, world, explore.picked.entity, frameDt);
        else if (tools.follow)
            followCell(camera, world, focus, frameDt);

        // Enciclopedia: lo que se ve se cataloga (dos veces por segundo).
        game.discoveryTimer -= frameDt;
        if (game.screen == Screen::Playing && game.discoveryTimer <= 0.0f) {
            game.discoveryTimer = 0.5f;
            const Vector2 a = GetScreenToWorld2D({0, 0}, camera);
            const Vector2 b = GetScreenToWorld2D({static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, camera);
            auto fresh = discoverVisible(game.progress, world, {a.x, a.y}, {b.x, b.y});
            for (std::string& f : recordBehaviors(game.progress, world))
                fresh.push_back(std::move(f));
            if (!fresh.empty()) {
                game.toasts.insert(game.toasts.end(), fresh.begin(), fresh.end());
                if (game.toasts.size() > 5)
                    game.toasts.erase(game.toasts.begin(), game.toasts.end() - 5);
                game.toastTimer = 5.0f;
                saveProgress(game.progress, game.progressPath);
            }
        }
        game.toastTimer = std::max(0.0f, game.toastTimer - frameDt);
        if (game.toastTimer <= 0.0f)
            game.toasts.clear();
        renderer.setStain(static_cast<Stain>(tools.stain));
        BeginDrawing();
        const double r0 = GetTime();
        renderer.drawWorld(world, camera, tools.overlay);
        tools.renderMs += (static_cast<float>(1000.0 * (GetTime() - r0)) - tools.renderMs) * 0.1f;
        drawLabPanel(world, game.run, tools);
        drawControlHud(world, !(explore.active && explore.inspection.valid));
        bool leaveExplore = false, enterExplore = false;
        if (explore.active)
            leaveExplore = drawExplore(world, explore, tools, camera);
        else
            enterExplore = drawSandboxBar(world, game.sandbox, camera);
        drawToasts(game.toasts, std::min(1.0f, game.toastTimer));
        EndDrawing();
        if (leaveExplore)
            setExplore(false);
        else if (enterExplore)
            setExplore(true);

        PROFILE_FRAME();
    }

    if (game.screen == Screen::Playing)
        saveProgress(game.progress, game.progressPath);
    CloseWindow();
    return 0;
}
