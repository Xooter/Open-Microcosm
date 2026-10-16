#pragma once

#include "ui/panel.hpp"

#include <memory>

namespace ui {

std::unique_ptr<Panel> makePlatePanel();
std::unique_ptr<Panel> makeCatalogPanel();
std::unique_ptr<Panel> makeEncyclopediaPanel();
std::unique_ptr<Panel> makeInspectorPanel();
std::unique_ptr<Panel> makeToolsPanel();
std::unique_ptr<Panel> makeStainPanel();
std::unique_ptr<Panel> makeSimulationPanel();
std::unique_ptr<Panel> makeSettingsPanel();
std::unique_ptr<Panel> makeProcessesPanel();
std::unique_ptr<Panel> makeStatsPanel();
std::unique_ptr<Panel> makeGraphsPanel();
std::unique_ptr<Panel> makeMeasurePanel();
std::unique_ptr<Panel> makeHelpPanel();
std::unique_ptr<Panel> makeContentPanel();
std::unique_ptr<Panel> makeCulturePanel();
std::unique_ptr<Panel> makeExperimentPanel();

} // namespace ui
