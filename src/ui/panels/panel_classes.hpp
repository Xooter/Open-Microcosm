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
std::unique_ptr<Panel> makeProcessesPanel();
std::unique_ptr<Panel> makeStatsPanel();
std::unique_ptr<Panel> makeHelpPanel();

} // namespace ui
