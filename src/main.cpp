#include "core/profiling.hpp"
#include "core/ui.hpp"
#include "core/units.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <raylib.h>
#include <raymath.h>

#include <random>

namespace {

constexpr int kScreenW = 1600;
constexpr int kScreenH = 900;

// La simulación avanza a paso fijo (determinista y estable), desacoplada del framerate.
constexpr float kFixedDt = 1.0f / 60.0f;
constexpr int kMaxStepsPerFrame = 4;

void updateCamera(Camera2D& cam) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = Vector2Scale(GetMouseDelta(), -1.0f / cam.zoom);
        cam.target = Vector2Add(cam.target, delta);
    }
    if (float wheel = GetMouseWheelMove(); wheel != 0.0f) {
        // Zoom centrado en el cursor.
        Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), cam);
        cam.offset = GetMousePosition();
        cam.target = mouseWorld;
        cam.zoom = Clamp(cam.zoom * (1.0f + 0.1f * wheel), 0.2f, 60.0f);
    }
}

void toggleOverlay(ToolState& tools, Overlay overlay) {
    tools.overlay = tools.overlay == overlay ? Overlay::None : overlay;
}

void handleTools(World& world, ToolState& tools, const Camera2D& camera) {
    if (IsKeyPressed(KEY_SPACE)) tools.paused = !tools.paused;
    if (IsKeyPressed(KEY_F1)) toggleOverlay(tools, Overlay::Velocity);
    if (IsKeyPressed(KEY_F2)) toggleOverlay(tools, Overlay::Oxygen);
    if (IsKeyPressed(KEY_F3)) toggleOverlay(tools, Overlay::Nutrients);
    if (IsKeyPressed(KEY_F4)) toggleOverlay(tools, Overlay::Light);

    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), camera);
    const Vec2 pos = {mouse.x, mouse.y};
    if (IsKeyDown(KEY_T)) world.dropInk(pos, tools.inkRadius, 40);
    if (IsKeyPressed(KEY_D)) world.dropDetritus(pos);

    const bool overUi = CheckCollisionPointRec(GetMousePosition(), kUiPanel);
    const float frameDt = GetFrameTime();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !overUi && frameDt > 0.0f) {
        // Velocidad del mouse en µm por segundo de simulación.
        const Vector2 d = Vector2Scale(GetMouseDelta(), 1.0f / (camera.zoom * frameDt * units::kTimeScale));
        world.setStir(pos, {d.x, d.y}, tools.stirRadius);
    } else {
        world.clearStir();
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(kScreenW, kScreenH, "bio");

    WorldConfig config;
    config.seed = std::random_device{}();
    World world(config);
    Renderer renderer;
    ToolState tools;

    Camera2D camera{};
    camera.zoom = static_cast<float>(kScreenW) / world.width();

    float accumulator = 0.0f;
    while (!WindowShouldClose()) {
        updateCamera(camera);
        handleTools(world, tools, camera);

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

        BeginDrawing();
        renderer.drawWorld(world, camera, tools.overlay);
        drawUi(world, tools);
        EndDrawing();

        PROFILE_FRAME();
    }

    CloseWindow();
    return 0;
}
