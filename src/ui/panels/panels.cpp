#include "ui/panels/panels.hpp"

#include "ui/panels/panel_classes.hpp"

namespace ui {

void registerPanels(PanelHost& host) {
    host.add(makePlatePanel());
    host.add(makeCatalogPanel());
    host.add(makeEncyclopediaPanel());
    host.add(makeInspectorPanel());
    host.add(makeToolsPanel());
    host.add(makeSimulationPanel());
    host.add(makeProcessesPanel());
    host.add(makeStatsPanel());
    host.add(makeHelpPanel());
}

} // namespace ui
