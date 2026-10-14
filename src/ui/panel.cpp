#include "ui/panel.hpp"

#include "core/profiling.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace ui {

namespace {
std::string windowName(const Panel& p) { return std::string(tr(p.title())) + "###" + p.key(); }
} // namespace

void PanelHost::drawAll(App& app) {
    for (auto& p : panels_) {
        if (!p->open)
            continue;
        const std::string name = windowName(*p);
        if (p->focusNext) {
            ImGui::SetNextWindowFocus();
            p->focusNext = false;
        }
        if (ImGui::Begin(name.c_str(), &p->open)) {
            PROFILE_SCOPE("ui panel");
            p->draw(app);
        }
        ImGui::End();
    }
}

void PanelHost::drawMenu() {
    for (auto& p : panels_)
        ImGui::MenuItem(tr(p->title()), nullptr, &p->open);
}

Panel* PanelHost::find(const std::string& key) {
    for (auto& p : panels_)
        if (key == p->key())
            return p.get();
    return nullptr;
}

void PanelHost::show(const std::string& key) {
    if (Panel* p = find(key)) {
        p->open = true;
        p->focusNext = true;
    }
}

void PanelHost::resetLayout(unsigned dockId, float width, float height) {
    const ImGuiID root = dockId;
    ImGui::DockBuilderRemoveNode(root);
    ImGui::DockBuilderAddNode(root, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(root, {width, height});
    ImGuiID center = root;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.25f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
    const ImGuiID rightLow = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.45f, nullptr, &right);
    for (auto& p : panels_) {
        if (p->dock() == Dock::Float)
            continue;
        const ImGuiID target = p->dock() == Dock::Left ? left : p->dock() == Dock::Right ? right : rightLow;
        ImGui::DockBuilderDockWindow(windowName(*p).c_str(), target);
    }
    ImGui::DockBuilderFinish(root);
}

} // namespace ui
