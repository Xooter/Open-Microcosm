#include "ui/panel.hpp"

#include "core/profiling.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

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
        // Un panel flotante abre la primera vez en el centro, con un tamaño legible (si no, ImGui lo
        // abre del tamaño mínimo arriba a la izquierda).
        if (p->dock() == Dock::Float) {
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos({vp->WorkPos.x + 0.5f * vp->WorkSize.x, vp->WorkPos.y + 0.5f * vp->WorkSize.y},
                                    ImGuiCond_FirstUseEver, {0.5f, 0.5f});
            ImGui::SetNextWindowSize({std::min(620.0f, 0.6f * vp->WorkSize.x), std::min(440.0f, 0.6f * vp->WorkSize.y)},
                                     ImGuiCond_FirstUseEver);
        }
        // Sin botón de cerrar: los paneles se muestran u ocultan desde el menú Ver.
        if (ImGui::Begin(name.c_str())) {
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

namespace {
// El nodo central (la placa) toma lo que sobre; los demás conservan su fracción.
void scaleNode(ImGuiDockNode* node, float sx, float sy) {
    if (!node)
        return;
    if (!node->IsCentralNode()) {
        node->SizeRef.x *= sx;
        node->SizeRef.y *= sy;
    }
    scaleNode(node->ChildNodes[0], sx, sy);
    scaleNode(node->ChildNodes[1], sx, sy);
}
} // namespace

void PanelHost::scaleLayout(unsigned dockId, float sx, float sy) {
    ImGuiDockNode* root = ImGui::DockBuilderGetNode(dockId);
    if (!root)
        return;
    scaleNode(root->ChildNodes[0], sx, sy);
    scaleNode(root->ChildNodes[1], sx, sy);
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
