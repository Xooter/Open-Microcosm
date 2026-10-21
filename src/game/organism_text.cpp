#include "game/organism_text.hpp"

OtherOrganism otherOf(FungusKind kind) {
    switch (kind) {
    case FungusKind::Chytrid: return OtherOrganism::Chytrid;
    case FungusKind::Allomyces: return OtherOrganism::Allomyces;
    case FungusKind::Mycobiont: return OtherOrganism::Lichen;
    default: return OtherOrganism::Saprolegnia;
    }
}

const char* otherName(OtherOrganism o) {
    switch (o) {
    case OtherOrganism::Bacteria: return "Bacterias (bacilos)";
    case OtherOrganism::Violaceum: return "Chromobacterium violaceum";
    case OtherOrganism::Bacillus: return "Bacillus subtilis";
    case OtherOrganism::Cyanobacteria: return "Cianobacterias filamentosas";
    case OtherOrganism::Chytrid: return "Rhizophydium (quitridio)";
    case OtherOrganism::Saprolegnia: return "Saprolegnia";
    case OtherOrganism::Phage: return "Bacteriofagos";
    case OtherOrganism::Bdellovibrio: return "Bdellovibrio bacteriovorus";
    case OtherOrganism::Allomyces: return "Allomyces";
    case OtherOrganism::Lichen: return "Hongo liquenizante (micobionte)";
    case OtherOrganism::Physarum: return "Physarum polycephalum (moho mucilaginoso)";
    default: return "?";
    }
}

const char* otherBlurb(OtherOrganism o) {
    switch (o) {
    case OtherOrganism::Bacteria:
        return "Nadan con 'corridas y giros' y siguen la comida comparando la concentracion de ahora con la de hace un segundo.";
    case OtherOrganism::Violaceum:
        return "Cuando son muchas, el autoinductor que liberan pasa un umbral (quorum) y fabrican violaceina, un pigmento violeta toxico para quien las come.";
    case OtherOrganism::Bacillus:
        return "Bacilo grampositivo de suelo y agua dulce: su pared de peptidoglicano es tan gruesa que retiene el cristal violeta de la tincion de Gram (las gramnegativas lo pierden con el decolorante y se ven rosadas).";
    case OtherOrganism::Cyanobacteria:
        return "Cadenas de celulas fotosinteticas; cada ~10 una se vuelve heterocisto y fija el nitrogeno del aire. Al morir liberan microcistina.";
    case OtherOrganism::Chytrid:
        return "Hongo parasito de algas: su zoospora nada hasta un alga, se enquista, la vacia con rizoides y libera decenas de zoosporas.";
    case OtherOrganism::Saprolegnia:
        return "'Moho de agua' (oomiceto): hifas que disuelven detritos con enzimas. Sin comida, las puntas se vuelven zoosporangios.";
    case OtherOrganism::Phage:
        return "Virus de bacterias: se pegan, inyectan su ADN y a la media hora la bacteria estalla liberando ~50 copias. Las resistentes sobreviven.";
    case OtherOrganism::Bdellovibrio:
        return "Bacteria cazadora de bacterias: nada a ~100 um/s, entra entre las membranas de su presa, crece adentro y sale con varias hijas.";
    case OtherOrganism::Allomyces:
        return "Hongo acuatico (Blastocladiomycota): hifas que se bifurcan en dos y zoosporas con un solo flagelo. Con hambre prolongada forma esporangios de resistencia pardos que duermen hasta que vuelve la comida.";
    case OtherOrganism::Lichen:
        return "Micobionte: hongo que vive unido a un alga verde. Ella le da azucar por fotosintesis; el le devuelve fosforo y la protege de parasitos como los quitridios. Solo no puede crecer casi nada.";
    case OtherOrganism::Physarum:
        return "NO es un hongo: es un moho mucilaginoso, una sola celula gigante con miles de nucleos. Explora en abanico, deja un rastro de baba que evita, y la red de tubos que lleva mas citoplasma se ensancha mientras el resto se retrae. Asi encuentra caminos eficientes entre trozos de comida (laberintos, redes parecidas a las del tren).";
    default: return "";
    }
}

const char* speciesBlurb(SpeciesId id) { return speciesInfo(id).description; }
