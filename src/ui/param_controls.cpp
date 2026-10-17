#include "ui/param_controls.hpp"

#include "app/app.hpp"
#include "ui/i18n.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <string>

namespace ui {

void parameterControl(App& app, const ParamInfo& p) {
    const char* reason = app.protocolLockReason(p.key);
    if (reason)
        ImGui::BeginDisabled();
    float value = parameterValue(*app.world, p);
    const bool changed = p.logScale ? sliderLog(p.label, &value, p.lo, p.hi, p.format, p.help)
                                    : slider(p.label, &value, p.lo, p.hi, p.format, p.help);
    if (reason) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::PushID(p.key);
        if (ImGui::SmallButton(tr("Destrabar")))
            app.unlockParameter(p.key);
        ImGui::PopID();
        tooltip((std::string(tr("Fijado por el protocolo: ")) + tr(reason) +
                 tr(" Destrabarlo es posible, pero la corrida queda fuera de protocolo (se anota en la exportación)."))
                    .c_str());
    }
    if (changed && !reason)
        app.setParameter(p, value);
}

void parameterControls(App& app, ParamGroup group) {
    for (const ParamInfo& p : parameters())
        if (p.group == group)
            parameterControl(app, p);
}

} // namespace ui
