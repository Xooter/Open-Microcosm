#include "ui/widgets.hpp"

#include "ui/i18n.hpp"
#include "ui/theme.hpp"

#include <imgui.h>

#include <cfloat>
#include <cstdarg>
#include <cstdio>

namespace ui {

namespace {

constexpr float kLabelFraction = 0.42f;

ImVec4 col(const float (&a)[4]) { return ImVec4(a[0], a[1], a[2], a[3]); }

// Rótulo en la columna izquierda; deja el cursor para el control, que ocupa el resto.
void label(const char* text) {
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(text));
    ImGui::SameLine(ImGui::GetCursorStartPos().x + kLabelFraction * w);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void helpOf(const char* help) {
    if (help && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tr(help));
}

bool sliderImpl(const char* text, float* value, float lo, float hi, const char* fmt, const char* help, float shown,
                bool log) {
    ImGui::PushID(text);
    label(text);
    float t = *value * shown;
    const bool changed = ImGui::SliderFloat("##v", &t, lo * shown, hi * shown, fmt,
                                            ImGuiSliderFlags_AlwaysClamp | (log ? ImGuiSliderFlags_Logarithmic : 0));
    helpOf(help);
    ImGui::PopID();
    if (changed)
        *value = t / shown;
    return changed;
}

} // namespace

bool slider(const char* text, float* value, float lo, float hi, const char* fmt, const char* help, float shown) {
    return sliderImpl(text, value, lo, hi, fmt, help, shown, false);
}

bool sliderLog(const char* text, float* value, float lo, float hi, const char* fmt, const char* help, float shown) {
    return sliderImpl(text, value, lo, hi, fmt, help, shown, true);
}

bool sliderInt(const char* text, int* value, int lo, int hi, const char* help) {
    ImGui::PushID(text);
    label(text);
    const bool changed = ImGui::SliderInt("##v", value, lo, hi, "%d", ImGuiSliderFlags_AlwaysClamp);
    helpOf(help);
    ImGui::PopID();
    return changed;
}

bool checkbox(const char* text, bool* value, const char* help) {
    const bool changed = ImGui::Checkbox(tr(text), value);
    helpOf(help);
    return changed;
}

bool combo(const char* text, int* index, const char* const* items, int count, const char* help) {
    ImGui::PushID(text);
    label(text);
    bool changed = false;
    if (ImGui::BeginCombo("##v", tr(items[*index]))) {
        for (int i = 0; i < count; ++i)
            if (ImGui::Selectable(tr(items[i]), i == *index)) {
                *index = i;
                changed = true;
            }
        ImGui::EndCombo();
    }
    helpOf(help);
    ImGui::PopID();
    return changed;
}

void readout(const char* text, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    ImGui::TextColored(col(palette().dim), "%s", tr(text));
    ImGui::SameLine(ImGui::GetCursorStartPos().x + kLabelFraction * ImGui::GetContentRegionAvail().x);
    ImGui::TextUnformatted(buf);
}

void bar(const char* text, float fraction, const char* overlay) {
    ImGui::TextColored(col(palette().dim), "%s", tr(text));
    ImGui::SameLine(ImGui::GetCursorStartPos().x + kLabelFraction * ImGui::GetContentRegionAvail().x);
    ImGui::ProgressBar(fraction < 0.0f ? 0.0f : fraction > 1.0f ? 1.0f : fraction, {-FLT_MIN, 0.0f}, overlay);
}

bool section(const char* title, bool openByDefault) {
    return ImGui::CollapsingHeader(tr(title), openByDefault ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}

void note(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, col(palette().dim));
    ImGui::TextWrapped("%s", tr(text));
    ImGui::PopStyleColor();
}

void helpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextColored(col(palette().dim), "(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tr(text));
}

void tooltip(const char* text) { helpOf(text); }

bool toolButton(const char* text, bool active, const char* help) {
    if (active) {
        const ImVec4 a = col(palette().accent);
        ImGui::PushStyleColor(ImGuiCol_Button, {a.x * 0.55f, a.y * 0.55f, a.z * 0.55f, 1.0f});
    }
    const bool pressed = ImGui::Button(tr(text), {36.0f, 30.0f});
    if (active)
        ImGui::PopStyleColor();
    helpOf(help);
    return pressed;
}

} // namespace ui
