#include "core/profiling.hpp"
#include "core/ui.hpp"
#include "render/renderer.hpp"
#include "sim/world.hpp"

#include <raylib.h>
#include <raymath.h>

#include <random>

namespace {

constexpr int kScreenW = 1600;
constexpr int kScreenH = 900;
constexpr float kWorldW = 3200.0f;
constexpr float kWorldH = 1800.0f;
constexpr std::size_t kParticleCount = 20'000;

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
        cam.zoom = Clamp(cam.zoom * (1.0f + 0.1f * wheel), 0.1f, 20.0f);
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(kScreenW, kScreenH, "bio");

    std::random_device rd;
    World world(kWorldW, kWorldH, kParticleCount, rd());
    Renderer renderer;

    Camera2D camera{};
    camera.zoom = static_cast<float>(kScreenW) / kWorldW;

    bool paused = false;
    float accumulator = 0.0f;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE))
            paused = !paused;
        updateCamera(camera);

        if (!paused) {
            accumulator += GetFrameTime();
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
        ClearBackground({12, 12, 18, 255});
        renderer.drawWorld(world, camera);
        UiActions ui = drawUi(world, paused);
        EndDrawing();

        if (ui.randomizeRules)
            world.rules().randomize(rd());
        if (ui.respawn)
            world.respawn(kParticleCount, rd());

        PROFILE_FRAME();
    }

    CloseWindow();
    return 0;
}
