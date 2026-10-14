#include "ui/imgui_raylib.hpp"

#include <imgui.h>

#include <raylib.h>
#include <rlgl.h>

#include <cstdint>

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

void initBackend(const char* iniPath, float fontSize) {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.IniFilename = iniPath;
    io.ConfigDragClickToInputText = true; // Ctrl+click o doble click en un slider: se tipea el valor
    io.Fonts->AddFontFromFileTTF(BIO_ASSET_DIR "/fonts/Roboto-Medium.ttf", fontSize);
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    Image img{pixels, w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    gFontTexture = LoadTextureFromImage(img);
    SetTextureFilter(gFontTexture, TEXTURE_FILTER_BILINEAR);
    io.Fonts->SetTexID(static_cast<ImTextureID>(gFontTexture.id));
    gReady = true;
}

void shutdownBackend() {
    if (!gReady)
        return;
    UnloadTexture(gFontTexture);
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
