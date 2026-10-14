#pragma once

// Backend de Dear ImGui para raylib (sin rlImGui): entrada, fuente y dibujo con rlgl.
struct ImDrawData;

namespace ui {

// Crea el contexto de ImGui (docking activado), la fuente y la textura. `iniPath`: dónde se
// guarda la disposición de los paneles (debe vivir hasta `shutdownBackend`).
void initBackend(const char* iniPath, float fontSize);
void shutdownBackend();
// Alimenta la entrada de raylib y empieza el cuadro de ImGui.
void beginFrame(float dt);
// Cierra el cuadro y lo dibuja (dentro de BeginDrawing/EndDrawing, encima del mundo).
void endFrame();

} // namespace ui
