#include "ui/commands.hpp"

#include "ui/i18n.hpp"

namespace ui {

std::string Commands::shortcutText(const Command& c) {
    if (c.key == ImGuiKey_None)
        return {};
    std::string s;
    if (c.ctrl)
        s += "Ctrl+";
    if (c.shift)
        s += "Shift+";
    s += ImGui::GetKeyName(c.key);
    return s;
}

void Commands::handleShortcuts() const {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
        return;
    for (const Command& c : list_) {
        if (c.key == ImGuiKey_None || !ImGui::IsKeyPressed(c.key, false))
            continue;
        if (io.KeyCtrl != c.ctrl || io.KeyShift != c.shift)
            continue;
        if (!c.enabled || c.enabled())
            c.run();
    }
}

void Commands::drawMenuItems(const char* menu) const {
    for (const Command& c : list_) {
        if (std::string(c.menu) != menu)
            continue;
        const bool on = !c.enabled || c.enabled();
        const bool check = c.checked && c.checked();
        if (ImGui::MenuItem(tr(c.label), shortcutText(c).c_str(), check, on))
            c.run();
    }
}

const Command* Commands::find(const std::string& id) const {
    for (const Command& c : list_)
        if (c.id == id)
            return &c;
    return nullptr;
}

} // namespace ui
