#include "ui/glossary_ui.hpp"

#include "app/app.hpp"
#include "game/glossary.hpp"
#include "ui/i18n.hpp"
#include "ui/theme.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace ui {

namespace {


// Las formas traducidas de cada término (se rearman al cambiar el idioma o el glosario).
const std::vector<std::vector<std::string>>& translatedAliases() {
    static std::vector<std::vector<std::string>> table;
    static std::string language;
    static int count = -1;
    if (language != currentLanguage() || count != glossaryCount()) {
        language = currentLanguage();
        count = glossaryCount();
        table.assign(static_cast<std::size_t>(count), {});
        if (language != "es")
            for (int i = 0; i < count; ++i) {
                const GlossaryTerm& g = glossaryTerm(i);
                table[static_cast<std::size_t>(i)].push_back(tr(g.term.c_str()));
                for (const std::string& a : g.aliases)
                    table[static_cast<std::size_t>(i)].push_back(tr(a.c_str()));
            }
    }
    return table;
}

} // namespace

void glossaryTooltip(int term) {
    if (term < 0 || term >= glossaryCount())
        return;
    const GlossaryTerm& g = glossaryTerm(term);
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
    ImGui::TextUnformatted(tr(g.term.c_str()));
    ImGui::Separator();
    ImGui::TextWrapped("%s", tr(g.definition.c_str()));
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void glossaryChips(App& app, std::initializer_list<const char*> texts) {
    std::string all;
    for (const char* t : texts)
        if (t && *t) {
            all += t;
            all += '\n';
            all += tr(t);
            all += '\n';
        }
    const std::vector<int> found = glossaryTermsIn(all, translatedAliases());
    if (found.empty())
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, col(palette().dim));
    ImGui::TextUnformatted(tr("Glosario:"));
    ImGui::PopStyleColor();
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (const int i : found) {
        const char* label = tr(glossaryTerm(i).term.c_str());
        const float w = ImGui::CalcTextSize(label).x + 2.0f * ImGui::GetStyle().FramePadding.x;
        // En la misma línea mientras entre.
        ImGui::SameLine();
        if (ImGui::GetCursorScreenPos().x + w > right)
            ImGui::NewLine();
        ImGui::PushID(i);
        if (ImGui::SmallButton(label))
            app.showTerm(i);
        if (ImGui::IsItemHovered())
            glossaryTooltip(i);
        ImGui::PopID();
    }
}

} // namespace ui
