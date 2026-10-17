#include "ui/theme.hpp"

#include "core/palette.hpp"

#include <imgui.h>
#include <implot.h>

namespace ui {

namespace {
// warn: naranja; good: verde azulado aclarado para el fondo oscuro; bad: bermellón (Okabe–Ito).
constexpr Palette kPalette = {{0.36f, 0.72f, 0.52f, 1.0f},
                              {0.90f, 0.62f, 0.0f, 1.0f},
                              {0.25f, 0.75f, 0.58f, 1.0f},
                              {0.58f, 0.60f, 0.60f, 1.0f},
                              {0.88f, 0.42f, 0.10f, 1.0f}};
float gScale = 1.0f;
} // namespace

const Palette& palette() { return kPalette; }
float uiScale() { return gScale; }
void setUiScale(float scale) { gScale = scale > 0.1f ? scale : 1.0f; }

void applyTheme() {
    // Desde el estilo de fábrica: así cambiar la escala no acumula (ScaleAllSizes multiplica).
    ImGui::GetStyle() = ImGuiStyle();
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 3.0f;
    s.FrameRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.TabRounding = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.WindowPadding = {8.0f, 8.0f};
    s.FramePadding = {6.0f, 3.0f};
    s.ItemSpacing = {6.0f, 4.0f};
    s.WindowMenuButtonPosition = ImGuiDir_None;
    ImVec4* c = s.Colors;
    auto v = [](const float (&a)[4]) { return ImVec4(a[0], a[1], a[2], a[3]); };
    const ImVec4 accent = v(kPalette.accent);
    auto shade = [&](float k, float a) { return ImVec4(accent.x * k, accent.y * k, accent.z * k, a); };
    c[ImGuiCol_WindowBg] = {0.105f, 0.115f, 0.115f, 1.0f};
    c[ImGuiCol_ChildBg] = {0.0f, 0.0f, 0.0f, 0.0f};
    c[ImGuiCol_PopupBg] = {0.12f, 0.13f, 0.13f, 0.97f};
    c[ImGuiCol_Border] = {0.22f, 0.24f, 0.24f, 1.0f};
    c[ImGuiCol_FrameBg] = {0.17f, 0.19f, 0.19f, 1.0f};
    c[ImGuiCol_FrameBgHovered] = {0.22f, 0.25f, 0.25f, 1.0f};
    c[ImGuiCol_FrameBgActive] = {0.26f, 0.30f, 0.29f, 1.0f};
    c[ImGuiCol_TitleBg] = {0.09f, 0.10f, 0.10f, 1.0f};
    c[ImGuiCol_TitleBgActive] = {0.13f, 0.15f, 0.15f, 1.0f};
    c[ImGuiCol_MenuBarBg] = {0.09f, 0.10f, 0.10f, 1.0f};
    c[ImGuiCol_Header] = shade(0.45f, 0.55f);
    c[ImGuiCol_HeaderHovered] = shade(0.6f, 0.7f);
    c[ImGuiCol_HeaderActive] = shade(0.75f, 0.8f);
    c[ImGuiCol_Button] = {0.20f, 0.23f, 0.23f, 1.0f};
    c[ImGuiCol_ButtonHovered] = shade(0.6f, 0.9f);
    c[ImGuiCol_ButtonActive] = shade(0.8f, 1.0f);
    c[ImGuiCol_SliderGrab] = shade(0.85f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_Tab] = {0.14f, 0.16f, 0.16f, 1.0f};
    c[ImGuiCol_TabHovered] = shade(0.7f, 0.9f);
    c[ImGuiCol_TabSelected] = shade(0.5f, 1.0f);
    c[ImGuiCol_TabDimmed] = {0.11f, 0.12f, 0.12f, 1.0f};
    c[ImGuiCol_TabDimmedSelected] = {0.17f, 0.20f, 0.19f, 1.0f};
    c[ImGuiCol_DockingPreview] = shade(0.8f, 0.5f);
    c[ImGuiCol_SeparatorHovered] = shade(0.8f, 0.8f);
    c[ImGuiCol_SeparatorActive] = accent;
    c[ImGuiCol_ResizeGrip] = shade(0.5f, 0.3f);
    c[ImGuiCol_ResizeGripHovered] = shade(0.7f, 0.6f);
    c[ImGuiCol_PlotHistogram] = shade(0.85f, 1.0f);
    c[ImGuiCol_TextSelectedBg] = shade(0.6f, 0.45f);
    s.ScaleAllSizes(gScale);
    // Series de los gráficos: Okabe–Ito (sin el negro, sobre el fondo oscuro).
    static int colormap = -1;
    if (colormap < 0) {
        ImVec4 colors[8];
        for (int i = 0; i < 8; ++i) {
            const Rgb8 k = okabe_ito::kOnDark[i];
            colors[i] = {k.r / 255.0f, k.g / 255.0f, k.b / 255.0f, 1.0f};
        }
        colormap = ImPlot::AddColormap("Okabe-Ito", colors, 8, true);
    }
    ImPlot::GetStyle().Colormap = colormap;
}

} // namespace ui
