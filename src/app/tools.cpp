#include "app/tools.hpp"

#include "game/world.hpp"

#include <algorithm>

namespace {

struct ToolRow {
    Tool tool;
    ToolInfo info;
};

constexpr ToolRow kTools[] = {
    {Tool::Select, {"Sel", "Seleccionar", "Click: elige el ser y abre su ficha. Esc la cierra.", ImGuiKey_1}},
    {Tool::Control, {"Ctl", "Controlar", "Click en una célula: la controlás. Mantené el click para moverla (cilios: W/S/A/D).", ImGuiKey_2}},
    {Tool::Ink, {"Tin", "Tinta", "Mantené y arrastrá: suelta tinta que sigue las corrientes.", ImGuiKey_3}},
    {Tool::Detritus, {"Det", "Detrito", "Click: deja un detrito.", ImGuiKey_4}},
    {Tool::Erase, {"Bor", "Borrar", "Click: quita la célula.", ImGuiKey_5}},
    {Tool::Lyse, {"Lis", "Lisar", "Click: revienta la célula (se disuelve en nutrientes y detritos).", ImGuiKey_6}},
    {Tool::Squeeze, {"Apr", "Apretar", "Mantené: empuja el agua hacia el cursor.", ImGuiKey_7}},
    {Tool::Stir, {"Rev", "Revolver", "Mantené y arrastrá: revuelve el agua.", ImGuiKey_8}},
    {Tool::Grab, {"Est", "Estirar", "Mantené y arrastrá: estira la membrana de una célula.", ImGuiKey_9}},
    {Tool::Dropper, {"Got", "Gotero", "Mantené: gotea el soluto elegido en el panel Herramientas.", ImGuiKey_0}},
    {Tool::Measure, {"Med", "Medir", "Arrastrá: mide la distancia en µm. En modo área, dibujá un lazo: área y perímetro.", ImGuiKey_M}},
    {Tool::Region, {"Reg", "Región", "Dibujá un lazo: cuenta las células y mide la química adentro (panel Medición).", ImGuiKey_G}},
    {Tool::Spawn, {"Spw", "Spawnear", "Click: pone el ser elegido en el catálogo. Esc cancela.", ImGuiKey_None}},
};




} // namespace

const ToolInfo& toolInfo(Tool t) {
    for (const ToolRow& r : kTools)
        if (r.tool == t)
            return r.info;
    return kTools[0].info;
}

