#include "core/profiling.hpp"
#include "core/ui.hpp"
#include "core/units.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>

#include <cmath>
#include <random>

namespace {

constexpr int kScreenW = 1600;
constexpr int kScreenH = 900;

// La simulación avanza a paso fijo (determinista y estable), desacoplada del framerate.
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr int kMaxStepsPerFrame = 4;

void updateCamera(Camera2D& cam, ToolState& tools) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        tools.follow = false; // mover la cámara a mano suelta el seguimiento
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / cam.zoom);
        cam.target = Vector2Add(cam.target, delta);
    }
    if (float wheel = GetMouseWheelMove(); wheel != 0.0f) {
        // Zoom centrado en el cursor (o en el jugador si la cámara lo sigue).
        if (!tools.follow) {
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), cam);
            cam.offset = GetMousePosition();
            cam.target = mouseWorld;
        }
        cam.zoom = Clamp(cam.zoom * (1.0f + 0.1f * wheel), 0.4f, 60.0f);
    }
}

// Cámara de microscopio: sigue al jugador con suavidad, por el camino corto del toro.
void followPlayer(Camera2D& cam, const World& world, float dt) {
    if (!world.registry().valid(world.player()))
        return;
    const Vec2 c = world.registry().get<RingBody>(world.player()).centroid;
    cam.offset = {GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f};
    const float dx = minImage(c.x - cam.target.x, world.width());
    const float dy = minImage(c.y - cam.target.y, world.height());
    const float k = 1.0f - std::exp(-6.0f * dt);
    cam.target.x = wrapCoord(cam.target.x + dx * k, world.width());
    cam.target.y = wrapCoord(cam.target.y + dy * k, world.height());
}

void toggleOverlay(ToolState& tools, Overlay overlay) {
    tools.overlay = tools.overlay == overlay ? Overlay::None : overlay;
}

void handleInput(World& world, ToolState& tools, const Camera2D& camera) {
    if (IsKeyPressed(KEY_SPACE)) tools.paused = !tools.paused;
    if (IsKeyPressed(KEY_F1)) toggleOverlay(tools, Overlay::Velocity);
    if (IsKeyPressed(KEY_F2)) toggleOverlay(tools, Overlay::Oxygen);
    if (IsKeyPressed(KEY_F3)) toggleOverlay(tools, Overlay::Nutrients);
    if (IsKeyPressed(KEY_F4)) toggleOverlay(tools, Overlay::Light);
    if (IsKeyPressed(KEY_F)) tools.follow = !tools.follow;
    if (IsKeyPressed(KEY_ONE)) world.setPlayerType(LocomotionType::Cilia);
    if (IsKeyPressed(KEY_TWO)) world.setPlayerType(LocomotionType::Flagellum);
    if (IsKeyPressed(KEY_THREE)) world.setPlayerType(LocomotionType::Pseudopod);

    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    const Vec2 pos = {wrapCoord(mouse.x, world.width()), wrapCoord(mouse.y, world.height())};
    if (IsKeyDown(KEY_T)) world.dropInk(pos, tools.inkRadius, 40);
    if (IsKeyPressed(KEY_B)) world.dropDetritus(pos);
    if (IsKeyPressed(KEY_C)) world.spawnCell(pos);
    if (IsKeyPressed(KEY_K)) world.spawnTestScene(50);
    if (IsKeyPressed(KEY_X)) world.removeCellAt(pos);
    if (IsKeyPressed(KEY_L)) world.lyseAt(pos);
    if (IsKeyPressed(KEY_R) && !world.registry().valid(world.player()))
        world.spawnPlayer(LocomotionType::Cilia, pos);
    if (IsKeyDown(KEY_Q))
        world.setSqueeze(pos, tools.squeezeSpeed);
    else
        world.clearSqueeze();

    const bool overUi = CheckCollisionPointRec(GetMousePosition(), kUiPanel);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool left = IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !overUi;

    // Herramientas de laboratorio: Shift = estirar membrana, Ctrl = revolver el agua.
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
        // Velocidad del mouse en µm por segundo de simulación.
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

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(kScreenW, kScreenH, "bio");

    WorldConfig config;
    config.seed = std::random_device{}();
    World world(config);
    world.spawnPlayer(LocomotionType::Cilia, {0.5f * world.width(), 0.5f * world.height()});
    Renderer renderer;
    ToolState tools;

    Camera2D camera{};
    camera.zoom = 2.0f;
    camera.target = {0.5f * world.width(), 0.5f * world.height()};
    camera.offset = {kScreenW * 0.5f, kScreenH * 0.5f};

    float accumulator = 0.0f;
    while (!WindowShouldClose()) {
        updateCamera(camera, tools);
        handleInput(world, tools, camera);

        if (!tools.paused) {
            accumulator += GetFrameTime() * units::kTimeScale;
            int steps = 0;
            while (accumulator >= kFixedDt && steps < kMaxStepsPerFrame) {
                world.step(kFixedDt);
                accumulator -= kFixedDt;
                ++steps;
            }
            if (steps == kMaxStepsPerFrame)
                accumulator = 0.0f; // evita la "espiral de la muerte" si la sim no da abasto
        }

        if (tools.follow)
            followPlayer(camera, world, GetFrameTime());

        BeginDrawing();
        renderer.drawWorld(world, camera, tools.overlay);
        drawUi(world, tools);
        EndDrawing();

        PROFILE_FRAME();
    }

    CloseWindow();
    return 0;
}
