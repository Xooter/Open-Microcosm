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

enum class Screen { Title, Setup, Encyclopedia, Options, Playing, Over };

struct Game {
    std::unique_ptr<World> world;
    Run run;
    Progress progress;
    std::string progressPath = defaultProgressPath();
    Options options;
    std::string optionsPath = defaultOptionsPath();
    SetupState setup;
    float encyclopediaScroll = 0.0f;
    std::vector<LocomotionType> unlockedNow;
    std::vector<std::string> toasts; // descubrimientos recientes
    float toastTimer = 0.0f, discoveryTimer = 0.0f;
    std::uint32_t runCounter = 0;
    GpuSubstepSolver* gpu = nullptr; // XPBD en GPU (fase 14); nullptr si no hay OpenGL 4.3
    Screen screen = Screen::Title;
    bool recorded = false;
};

void startRun(Game& game) {
    // Gota procedural: el bioma elegido con las variaciones de la semilla.
    const WorldConfig config = makeDrop(static_cast<BiomeId>(game.setup.biome), game.setup.seed);
    game.world = std::make_unique<World>(config);
    game.world->bioTimeScale() = game.options.bioTimeScale;
    game.world->lodStride = game.options.lod ? 5 : 1;
    if (game.options.gpuPhysics && game.gpu && game.gpu->ready())
        game.world->setSubstepSolver(game.gpu);
    game.progress.biomesPlayed |= 1u << static_cast<unsigned>(game.setup.biome);
    const LocomotionType founder = game.setup.founder;
    RunConfig rc;
    rc.founder = founder;
    rc.wildlife = dropAreaScale(config); // misma densidad de especies en cualquier placa
    rc.biome = game.setup.biome;
    rc.seed = config.seed;
    Rng rng(config.seed, 0, 0, 0);
    rc.genome = game.progress.founderGenome(founder, rng); // legado de partidas anteriores
    game.run.start(*game.world, rc, ++game.runCounter);
    game.screen = Screen::Playing;
    game.recorded = false;
    game.unlockedNow.clear();
}

// Guarda el resultado de la partida una sola vez (al perder o al abandonarla).
void recordRun(Game& game) {
    if (game.recorded || !game.world)
        return;
    game.recorded = true;
    recordBehaviors(game.progress, *game.world);
    game.unlockedNow = game.progress.recordRun(game.run.bestGeneration(), game.run.bestShare(), game.run.won(),
                                               game.run.config().founder, game.run.lineageGenome());
    saveProgress(game.progress, game.progressPath);
}

void updateCamera(Camera2D& cam, ToolState& tools, const World& world) {
    const Dish& dish = world.dish();
    // Con placa, el alejamiento máximo muestra la placa entera (con el canto de vidrio).
    const float minZoom = dish.active()
        ? std::min(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())) / (2.3f * dish.radius)
        : 0.4f;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        tools.follow = false; // mover la cámara a mano suelta el seguimiento
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / cam.zoom);
        cam.target = Vector2Add(cam.target, delta);
    }
    if (float wheel = GetMouseWheelMove(); wheel != 0.0f) {
        if (!tools.follow) { // zoom centrado en el cursor
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
void handlePlayInput(World& world, ToolState& tools, const Camera2D& camera) {
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
    if (IsKeyPressed(KEY_F)) tools.follow = !tools.follow;
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
    if (IsKeyPressed(KEY_L)) world.lyseAt(pos);
    if (IsKeyPressed(KEY_V)) world.tryConjugate(world.player()); // conjugar con una pareja cercana
    if (IsKeyDown(KEY_Q))
        world.setSqueeze(pos, tools.squeezeSpeed);
    else
        world.clearSqueeze();

    const bool overUi = tools.showLab && CheckCollisionPointRec(GetMousePosition(), kUiPanel);
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

// Elegir hija o heredero: Tab / flechas para ciclar, click para señalar, Enter para confirmar.
void handleChoiceInput(Game& game, const Camera2D& camera) {
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_RIGHT)) game.run.cycle(1);
    if (IsKeyPressed(KEY_LEFT)) game.run.cycle(-1);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 m = GetScreenToWorld2D(GetMousePosition(), camera);
        const World& w = *game.world;
        for (entt::entity c : game.run.candidates()) {
            const RingBody& b = w.registry().get<RingBody>(c);
            const Vec2 q = b.centroid + Vec2{minImage(m.x - b.centroid.x, w.width()), minImage(m.y - b.centroid.y, w.height())};
            if (polygon::contains(b.poly.data(), static_cast<int>(b.poly.size()), q))
                game.run.select(c);
        }
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
        game.run.confirm(*game.world);
}

// Resalta a las candidatas en el mundo (la elegida, más fuerte).
void drawCandidates(const Game& game, const Camera2D& camera) {
    const World& w = *game.world;
    BeginMode2D(camera);
    const auto& cands = game.run.candidates();
    for (std::size_t i = 0; i < cands.size(); ++i) {
        const RingBody& b = w.registry().get<RingBody>(cands[i]);
        const Vec2 c = {camera.target.x + minImage(b.centroid.x - camera.target.x, w.width()),
                        camera.target.y + minImage(b.centroid.y - camera.target.y, w.height())};
        const float r = 0.6f * length(b.aabbMax - b.aabbMin) + 6.0f;
        const bool chosen = i == game.run.selected();
        DrawRing({c.x, c.y}, r, r + (chosen ? 4.0f : 1.5f) / camera.zoom, 0, 360, 72,
                 chosen ? Color{200, 120, 20, 230} : Color{80, 80, 70, 140});
    }
    EndMode2D();
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
            case TitleAction::Play: game.screen = Screen::Setup; break;
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
        updateCamera(camera, tools, world);
        const bool choosing = game.run.simulationPaused();
        if (game.screen == Screen::Playing && !choosing)
            handlePlayInput(world, tools, camera);
        if (choosing)
            handleChoiceInput(game, camera);
        if (IsKeyPressed(KEY_N) && !choosing) { // abandonar y empezar otra partida
            recordRun(game);
            game.screen = Screen::Title;
            continue;
        }

        if (game.screen == Screen::Playing && !tools.paused && !choosing) {
            // LOD: lo que se ve (más un margen) se simula en detalle.
            world.setFocus({camera.target.x, camera.target.y},
                           {0.5f * static_cast<float>(GetScreenWidth()) / camera.zoom,
                            0.5f * static_cast<float>(GetScreenHeight()) / camera.zoom});
            accumulator += frameDt * units::kTimeScale;
            int steps = 0;
            while (accumulator >= kFixedDt && steps < kMaxStepsPerFrame && !game.run.simulationPaused()) {
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
        if (game.run.state() == Run::State::Lost && game.screen == Screen::Playing) {
            recordRun(game);
            game.screen = Screen::Over;
        }
        if (game.screen == Screen::Over && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_N)))
            game.screen = Screen::Title;

        // La cámara sigue al jugador, o a la candidata elegida durante una elección.
        entt::entity focus = world.player();
        if (game.run.simulationPaused() && !game.run.candidates().empty())
            focus = game.run.candidates()[game.run.selected()];
        if (tools.follow || game.run.simulationPaused())
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
        if (game.run.simulationPaused())
            drawCandidates(game, camera);
        drawUi(world, tools);
        drawRunHud(game.run, world);
        drawToasts(game.toasts, std::min(1.0f, game.toastTimer));
        if (game.screen == Screen::Over)
            drawGameOver(game.run, game.progress, game.unlockedNow);
        EndDrawing();

        PROFILE_FRAME();
    }

    if (game.screen == Screen::Playing)
        recordRun(game);
    CloseWindow();
    return 0;
}
