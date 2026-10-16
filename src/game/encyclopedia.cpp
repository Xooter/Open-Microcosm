#include "game/encyclopedia.hpp"

#include "game/progress.hpp"
#include "game/world.hpp"

#include <cmath>

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

std::vector<std::string> discoverVisible(Progress& progress, const World& world, Vec2 viewMin, Vec2 viewMax) {
    std::vector<std::string> fresh;
    const float w = world.width(), h = world.height();
    const Vec2 c = (viewMin + viewMax) * 0.5f, half = (viewMax - viewMin) * 0.5f;
    auto inView = [&](Vec2 p) {
        return std::abs(minImage(p.x - c.x, w)) < half.x && std::abs(minImage(p.y - c.y, h)) < half.y;
    };
    const entt::registry& reg = world.registry();
    for (auto [e, body, sp] : reg.view<RingBody, Species>().each()) {
        const auto bit = std::uint64_t{1} << static_cast<unsigned>(sp.id);
        if (progress.seenSpecies & bit)
            continue;
        if (inView(body.centroid)) {
            progress.seenSpecies |= bit;
            fresh.emplace_back(speciesInfo(sp.id).name);
        }
    }
    auto see = [&](OtherOrganism o, bool visible) {
        const auto bit = std::uint32_t{1} << static_cast<unsigned>(o);
        if (visible && !(progress.seenOthers & bit)) {
            progress.seenOthers |= bit;
            fresh.emplace_back(otherName(o));
        }
    };
    const Bacteria& bac = world.bacteria();
    bool bacteria = false, violet = false, bdello = false, bacillus = false;
    for (std::size_t i = 0; i < bac.size(); i += 7) { // muestreo: alcanza para notar que están
        if (!inView({bac.x[i], bac.y[i]}))
            continue;
        bacteria = true;
        violet |= bac.quorum[i] != 0;
        bacillus |= bac.strain[i] == 2;
        bdello |= bac.bdello[i] >= 0.0f;
    }
    see(OtherOrganism::Bacteria, bacteria);
    see(OtherOrganism::Violaceum, violet);
    see(OtherOrganism::Bacillus, bacillus);
    see(OtherOrganism::Bdellovibrio, bdello);
    bool cyano = false;
    for (const CyanoFilament& f : world.cyanobacteria().filaments)
        cyano |= !f.x.empty() && inView({wrapCoord(f.x[0], w), wrapCoord(f.y[0], h)});
    see(OtherOrganism::Cyanobacteria, cyano);
    for (const Mycelium& m : world.fungi().mycelia)
        if (m.kind != FungusKind::Chytrid && !m.x.empty() && inView({wrapCoord(m.x[0], w), wrapCoord(m.y[0], h)}))
            see(otherOf(m.kind), true);
    for (std::size_t i = 0; i < world.fungi().resting.size(); ++i)
        if (inView({world.fungi().resting.x[i], world.fungi().resting.y[i]}))
            see(OtherOrganism::Allomyces, true);
    for (const Plasmodium& pl : world.plasmodia().plasmodia)
        if (!pl.x.empty() && inView({wrapCoord(pl.x[0], w), wrapCoord(pl.y[0], h)}))
            see(OtherOrganism::Physarum, true);
    for (auto [e, body, li] : reg.view<RingBody, Lichenized>().each())
        see(OtherOrganism::Lichen, inView(body.centroid));
    bool chytrid = false;
    for (auto [e, body, inf] : reg.view<RingBody, ChytridInfection>().each())
        chytrid |= inView(body.centroid);
    see(OtherOrganism::Chytrid, chytrid);
    // Los fagos no se ven al microscopio óptico: se los "descubre" por las bacterias que estallan.
    see(OtherOrganism::Phage, world.observations().phageLyses > 0 && bacteria);
    return fresh;
}

std::vector<std::string> recordBehaviors(Progress& progress, const World& world) {
    std::vector<std::string> fresh;
    auto add = [&](const std::string& b) {
        if (progress.behaviors.insert(b).second)
            fresh.push_back(b);
    };
    for (const auto& [key, count] : world.deathTally())
        if (count > 0)
            add(std::string(speciesInfo(key.first).name) + ": " + key.second);
    const Observations& o = world.observations();
    if (o.chytridInfections > 0) add("Un quitridio infecta un alga");
    if (o.chytridReleases > 0) add("Esporangios de quitridio liberan zoosporas");
    if (o.restingFormed > 0) add("Allomyces forma esporangios de resistencia con hambre");
    if (o.restingGerminated > 0) add("Un esporangio de resistencia germina cuando vuelve la comida");
    if (o.lichenLinks > 0) add("Un hongo liquenizante se une a un alga (simbiosis)");
    if (o.physarumFusions > 0) add("Physarum fusiona dos tubos: la red es una sola celula");
    if (o.sclerotiaFormed > 0) add("Physarum se endurece en esclerocio por hambre");
    if (o.sclerotiaGerminated > 0) add("Un esclerocio revive cuando hay comida");
    if (o.buds > 0) add("Gemacion de levaduras");
    if (o.colonyReleases > 0) add("Una colonia libera colonias hijas");
    if (o.filamentDivisions > 0) add("Spirogyra se divide dentro del filamento");
    if (o.phageLyses > 0) add("Bacterias que estallan por fagos");
    if (o.nitrogenFixed > 0.0f) add("Heterocistos fijan nitrogeno del aire");
    if (o.quorumSwitches > 0) add("Quorum sensing: C. violaceum se vuelve violeta");
    if (o.bdelloBursts > 0) add("Bdellovibrio sale de un bdeloplasto con sus hijas");
    if (o.zoospores > 0) add("Oedogonium libera una zoospora con una corona de flagelos");
    if (o.zoosporesSettled > 0) add("Una zoospora se fija y empieza un filamento nuevo");
    return fresh;
}
