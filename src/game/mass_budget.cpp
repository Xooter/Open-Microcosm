#include "game/mass_budget.hpp"

const char* elementName(Element e) {
    switch (e) {
    case Element::P: return "P";
    case Element::N: return "N";
    case Element::Si: return "Si";
    case Element::Fe: return "Fe";
    case Element::C: return "C";
    case Element::Count: break;
    }
    return "";
}

const char* poolName(Pool p) {
    switch (p) {
    case Pool::Water: return "Agua (disuelto)";
    case Pool::Cells: return "Células";
    case Pool::Vacuoles: return "Vacuolas digestivas";
    case Pool::Bacteria: return "Bacterias";
    case Pool::Cyanobacteria: return "Cianobacterias";
    case Pool::Fungi: return "Hongos";
    case Pool::Plasmodium: return "Physarum";
    case Pool::Detritus: return "Detritos";
    case Pool::Sediment: return "Sedimento";
    case Pool::Gas: return "CO2 disuelto";
    case Pool::Count: break;
    }
    return "";
}

const char* flowName(Flow f) {
    switch (f) {
    case Flow::Other: return "Sin rotular";
    case Flow::Uptake: return "Captación";
    case Flow::Excretion: return "Excreción";
    case Flow::Death: return "Muerte y lisis";
    case Flow::Spawn: return "Seres nuevos";
    case Flow::Dissolution: return "Disolución";
    case Flow::Fixation: return "Fijación de N2";
    case Flow::Dose: return "Gotero y ajustes";
    case Flow::Air: return "Aire";
    case Flow::Count: break;
    }
    return "";
}
