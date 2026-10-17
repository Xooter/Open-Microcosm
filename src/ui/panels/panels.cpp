#include "ui/panels/panels.hpp"

#include "ui/panels/panel_classes.hpp"

namespace ui {

void registerPanels(PanelHost& host) {
    host.add(makePlatePanel());
    host.add(makeCatalogPanel());
    host.add(makeEncyclopediaPanel());
    host.add(makeInspectorPanel());
    host.add(makeToolsPanel());
    host.add(makeStainPanel());
    host.add(makeSimulationPanel());
    host.add(makeSettingsPanel());
    host.add(makeProcessesPanel());
    host.add(makeStatsPanel());
    host.add(makeGraphsPanel());
    host.add(makeMeasurePanel());
    host.add(makeContentPanel());
    host.add(makeCulturePanel());
    host.add(makeExperimentPanel());
    host.add(makeLessonPanel());
    host.add(makeHelpPanel());
}

} // namespace ui
