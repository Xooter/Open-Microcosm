#include "ui/param_controls.hpp"

#include "app/app.hpp"
#include "ui/widgets.hpp"

namespace ui {

void parameterControls(App& app, ParamGroup group) {
    for (const ParamInfo& p : parameters()) {
        if (p.group != group)
            continue;
        float value = parameterValue(*app.world, p);
        const bool changed = p.logScale ? sliderLog(p.label, &value, p.lo, p.hi, p.format, p.help)
                                        : slider(p.label, &value, p.lo, p.hi, p.format, p.help);
        if (changed)
            app.setParameter(p, value);
    }
}

} // namespace ui
