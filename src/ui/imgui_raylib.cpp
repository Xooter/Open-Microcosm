#include "ui/imgui_raylib.hpp"
#include "core/paths.hpp"

#include <imgui.h>
#include <implot.h>

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace ui {

namespace {

Texture2D gFontTexture{};
bool gReady = false;

struct KeyMap {
    int raylib;
    ImGuiKey imgui;
};

constexpr KeyMap kKeys[] = {
    {KEY_TAB, ImGuiKey_Tab},           {KEY_LEFT, ImGuiKey_LeftArrow},   {KEY_RIGHT, ImGuiKey_RightArrow},
    {KEY_UP, ImGuiKey_UpArrow},        {KEY_DOWN, ImGuiKey_DownArrow},   {KEY_PAGE_UP, ImGuiKey_PageUp},
    {KEY_PAGE_DOWN, ImGuiKey_PageDown}, {KEY_HOME, ImGuiKey_Home},       {KEY_END, ImGuiKey_End},
    {KEY_INSERT, ImGuiKey_Insert},     {KEY_DELETE, ImGuiKey_Delete},    {KEY_BACKSPACE, ImGuiKey_Backspace},
    {KEY_SPACE, ImGuiKey_Space},       {KEY_ENTER, ImGuiKey_Enter},      {KEY_ESCAPE, ImGuiKey_Escape},
    {KEY_KP_ENTER, ImGuiKey_KeypadEnter}, {KEY_LEFT_CONTROL, ImGuiKey_LeftCtrl}, {KEY_RIGHT_CONTROL, ImGuiKey_RightCtrl},
    {KEY_LEFT_SHIFT, ImGuiKey_LeftShift}, {KEY_RIGHT_SHIFT, ImGuiKey_RightShift}, {KEY_LEFT_ALT, ImGuiKey_LeftAlt},
    {KEY_RIGHT_ALT, ImGuiKey_RightAlt}, {KEY_MINUS, ImGuiKey_Minus}, {KEY_EQUAL, ImGuiKey_Equal},
    {KEY_PERIOD, ImGuiKey_Period}, {KEY_COMMA, ImGuiKey_Comma}, {KEY_SLASH, ImGuiKey_Slash},
    {KEY_SEMICOLON, ImGuiKey_Semicolon}, {KEY_GRAVE, ImGuiKey_GraveAccent},
};

ImGuiKey fKey(int n) { return static_cast<ImGuiKey>(ImGuiKey_F1 + n); }

void applyCursor() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;
    switch (ImGui::GetMouseCursor()) {
    case ImGuiMouseCursor_TextInput: SetMouseCursor(MOUSE_CURSOR_IBEAM); break;
    case ImGuiMouseCursor_ResizeAll: SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL); break;
    case ImGuiMouseCursor_ResizeNS: SetMouseCursor(MOUSE_CURSOR_RESIZE_NS); break;
    case ImGuiMouseCursor_ResizeEW: SetMouseCursor(MOUSE_CURSOR_RESIZE_EW); break;
    case ImGuiMouseCursor_ResizeNESW: SetMouseCursor(MOUSE_CURSOR_RESIZE_NESW); break;
    case ImGuiMouseCursor_ResizeNWSE: SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE); break;
    case ImGuiMouseCursor_Hand: SetMouseCursor(MOUSE_CURSOR_POINTING_HAND); break;
    case ImGuiMouseCursor_NotAllowed: SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED); break;
    default: SetMouseCursor(MOUSE_CURSOR_DEFAULT); break;
    }
}

void renderDrawData(const ImDrawData* data) {
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    const int fbHeight = GetRenderHeight();
    for (int n = 0; n < data->CmdListsCount; ++n) {
        const ImDrawList* list = data->CmdLists[n];
        const ImDrawVert* vtx = list->VtxBuffer.Data;
        const ImDrawIdx* idx = list->IdxBuffer.Data;
        for (const ImDrawCmd& cmd : list->CmdBuffer) {
            if (cmd.UserCallback) {
                if (cmd.UserCallback != ImDrawCallback_ResetRenderState)
                    cmd.UserCallback(list, &cmd);
                continue;
            }
            const float x0 = cmd.ClipRect.x - data->DisplayPos.x, y0 = cmd.ClipRect.y - data->DisplayPos.y;
            const float x1 = cmd.ClipRect.z - data->DisplayPos.x, y1 = cmd.ClipRect.w - data->DisplayPos.y;
            if (x1 <= x0 || y1 <= y0)
                continue;
            rlEnableScissorTest();
            rlScissor(static_cast<int>(x0), fbHeight - static_cast<int>(y1), static_cast<int>(x1 - x0),
                      static_cast<int>(y1 - y0));
            rlCheckRenderBatchLimit(static_cast<int>(cmd.ElemCount));
            rlBegin(RL_TRIANGLES);
            rlSetTexture(static_cast<unsigned int>(cmd.GetTexID()));
            for (unsigned i = 0; i < cmd.ElemCount; ++i) {
                const ImDrawVert& v = vtx[idx[cmd.IdxOffset + i] + cmd.VtxOffset];
                rlColor4ub(static_cast<unsigned char>(v.col & 0xFF), static_cast<unsigned char>((v.col >> 8) & 0xFF),
                           static_cast<unsigned char>((v.col >> 16) & 0xFF), static_cast<unsigned char>(v.col >> 24));
                rlTexCoord2f(v.uv.x, v.uv.y);
                rlVertex2f(v.pos.x, v.pos.y);
            }
            rlEnd();
            rlDrawRenderBatchActive();
        }
    }
    rlSetTexture(0);
    rlDisableScissorTest();
    rlEnableBackfaceCulling();
}

} // namespace

namespace {

// Glifos que Roboto no trae y los textos científicos usan: los índices y subíndices (O₂, NH₄⁺,
// 10⁻⁵) salen de los caracteres de la misma fuente a 62 % (corridos hacia arriba o hacia abajo), y
// la flecha → se dibuja. Solo se agregan si la fuente no los tiene.
struct Derived {
    ImWchar codepoint, base;
    bool super;
};
constexpr Derived kDerived[] = {
    {0x2070, '0', true},  {0x2074, '4', true},  {0x2075, '5', true},     {0x2076, '6', true},
    {0x2077, '7', true},  {0x2078, '8', true},  {0x2079, '9', true},     {0x207A, '+', true},
    {0x207B, 0x2212, true}, {0x207C, '=', true}, {0x207D, '(', true},    {0x207E, ')', true},
    {0x207F, 'n', true},  {0x2080, '0', false}, {0x2081, '1', false},    {0x2082, '2', false},
    {0x2083, '3', false}, {0x2084, '4', false}, {0x2085, '5', false},    {0x2086, '6', false},
    {0x2087, '7', false}, {0x2088, '8', false}, {0x2089, '9', false},    {0x208A, '+', false},
    {0x208B, 0x2212, false}, {0x208C, '=', false}, {0x208D, '(', false}, {0x208E, ')', false},
};
constexpr float kSmall = 0.62f; // tamaño de los índices respecto del texto

// Flecha → en el rectángulo (w × h) del atlas: un trazo y dos diagonales, con antialias por distancia.
void drawArrow(unsigned char* pixels, int texW, const ImFontAtlasCustomRect& r, float thickness) {
    const float w = r.Width, h = r.Height, mid = 0.5f * h, head = 0.42f * h;
    const float seg[3][4] = {{0.5f, mid, w - 1.0f, mid},
                             {w - 1.0f - head, mid - head, w - 1.0f, mid},
                             {w - 1.0f - head, mid + head, w - 1.0f, mid}};
    for (int y = 0; y < r.Height; ++y)
        for (int x = 0; x < r.Width; ++x) {
            const float px = x + 0.5f, py = y + 0.5f;
            float d = 1e9f;
            for (const auto& sg : seg) {
                const float dx = sg[2] - sg[0], dy = sg[3] - sg[1];
                const float t = std::clamp(((px - sg[0]) * dx + (py - sg[1]) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
                d = std::min(d, std::hypot(px - (sg[0] + t * dx), py - (sg[1] + t * dy)));
            }
            const float a = std::clamp(0.5f * thickness + 0.5f - d, 0.0f, 1.0f);
            unsigned char* p = pixels + 4 * ((r.Y + y) * texW + (r.X + x));
            p[0] = p[1] = p[2] = 255;
            p[3] = static_cast<unsigned char>(255.0f * a);
        }
}

// Arma el atlas de la fuente a `fontSize` píxeles y lo sube a la GPU (entre cuadros).
void buildFonts(float fontSize) {
    ImGuiIO& io = ImGui::GetIO();
    // Además de Latin-1: griego (ρ, μ), guiones y comillas, índices, flechas y operadores (≈, −, ≤).
    static const ImWchar kRanges[] = {0x0020, 0x00FF, 0x0370, 0x03FF, 0x2000, 0x206F, 0x2070, 0x209F,
                                      0x2190, 0x21FF, 0x2200, 0x22FF, 0};
    static const ImWchar kSmallRanges[] = {0x28, 0x29, 0x2B, 0x2B, 0x30, 0x39, 0x3D, 0x3D, 0x6E, 0x6E, 0x2212, 0x2212, 0};
    const std::string path = assetPath("fonts/Roboto-Medium.ttf");
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), fontSize, nullptr, kRanges);
    ImFont* small = io.Fonts->AddFontFromFileTTF(path.c_str(), kSmall * fontSize, nullptr, kSmallRanges);
    const int arrowW = static_cast<int>(std::lround(0.85f * fontSize)), arrowH = static_cast<int>(std::lround(0.5f * fontSize));
    const int arrow = font ? io.Fonts->AddCustomRectFontGlyph(font, 0x2192, arrowW, arrowH, arrowW + 0.2f * fontSize,
                                                              {0.1f * fontSize, 0.0f})
                           : -1;
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h); // arma el atlas
    if (font && small) {
        for (const Derived& d : kDerived) {
            const ImFontGlyph* g = small->FindGlyphNoFallback(d.base);
            if (!g || font->FindGlyphNoFallback(d.codepoint))
                continue;
            // La línea base del índice, corrida respecto de la del texto.
            const float dy = font->Ascent + (d.super ? -0.36f : 0.14f) * fontSize - small->Ascent;
            font->AddGlyph(nullptr, d.codepoint, g->X0, g->Y0 + dy, g->X1, g->Y1 + dy, g->U0, g->V0, g->U1, g->V1,
                           g->AdvanceX);
        }
        if (arrow >= 0) {
            drawArrow(pixels, w, *io.Fonts->GetCustomRectByIndex(arrow), std::max(1.2f, fontSize / 12.0f));
            // A media altura de las minúsculas (la altura x de Roboto es ~0,45 del tamaño).
            if (ImFontGlyph* g = const_cast<ImFontGlyph*>(font->FindGlyphNoFallback(0x2192))) {
                g->Y0 = font->Ascent - 0.22f * fontSize - 0.5f * arrowH;
                g->Y1 = g->Y0 + arrowH;
            }
        }
        font->BuildLookupTable();
    }
    io.FontDefault = font;
    Image img{pixels, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    gFontTexture = LoadTextureFromImage(img);
    SetTextureFilter(gFontTexture, TEXTURE_FILTER_BILINEAR);
    io.Fonts->SetTexID(static_cast<ImTextureID>(gFontTexture.id));
}

} // namespace

void initBackend(const char* iniPath, float fontSize) {
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.IniFilename = iniPath;
    io.ConfigDragClickToInputText = true; // Ctrl+click o doble click en un slider: se tipea el valor
    buildFonts(fontSize);
    gReady = true;
}

void rebuildFonts(float fontSize) {
    if (!gReady)
        return;
    UnloadTexture(gFontTexture);
    ImGui::GetIO().Fonts->Clear();
    buildFonts(fontSize);
}

void shutdownBackend() {
    if (!gReady)
        return;
    UnloadTexture(gFontTexture);
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    gReady = false;
}

void beginFrame(float dt) {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
    io.DeltaTime = dt > 0.0f ? dt : 1.0f / 60.0f;
    io.AddMousePosEvent(static_cast<float>(GetMouseX()), static_cast<float>(GetMouseY()));
    io.AddMouseButtonEvent(0, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    io.AddMouseButtonEvent(1, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    io.AddMouseButtonEvent(2, IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    const Vector2 wheel = GetMouseWheelMoveV();
    io.AddMouseWheelEvent(wheel.x, wheel.y);
    for (const KeyMap& k : kKeys)
        io.AddKeyEvent(k.imgui, IsKeyDown(k.raylib));
    for (int i = 0; i < 12; ++i)
        io.AddKeyEvent(fKey(i), IsKeyDown(KEY_F1 + i));
    for (int i = 0; i < 26; ++i)
        io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_A + i), IsKeyDown(KEY_A + i));
    for (int i = 0; i < 10; ++i)
        io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_0 + i), IsKeyDown(KEY_ZERO + i));
    io.AddKeyEvent(ImGuiMod_Ctrl, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));
    while (const int c = GetCharPressed())
        io.AddInputCharacter(static_cast<unsigned>(c));
    applyCursor();
    ImGui::NewFrame();
}

void endFrame() {
    ImGui::Render();
    renderDrawData(ImGui::GetDrawData());
}

} // namespace ui
