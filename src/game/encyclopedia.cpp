#include "game/encyclopedia.hpp"

#include "game/progress.hpp"
#include "game/world.hpp"

#include <cmath>

OtherOrganism otherOf(FungusKind kind) {
    switch (kind) {
    case FungusKind::Chytrid: return OtherOrganism::Chytrid;
    case FungusKind::Zoophagus: return OtherOrganism::Zoophagus;
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
    case OtherOrganism::Zoophagus: return "Zoophagus insidians";
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
    case OtherOrganism::Zoophagus:
        return "Hongo depredador: sus hifas tienen clavijas adhesivas. El rotifero que muerde una queda pegado y es digerido.";
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

const char* speciesBlurb(SpeciesId id) {
    switch (id) {
    case SpeciesId::Paramecium: return "Ciliado de ~200 um que come bacterias por un surco oral. Retrocede al chocar (reaccion de evitacion) y se junta donde hay CO2.";
    case SpeciesId::Euglena: return "Flagelado verde con mancha ocular: nada hacia la luz y hace fotosintesis, pero tambien absorbe comida.";
    case SpeciesId::Amoeba: return "Ameba gigante: extiende seudopodos, envuelve a la presa con la membrana y la encierra en una vacuola.";
    case SpeciesId::Chlamydomonas: return "Alga de dos flagelos que nada como en brazada. Dentro de la pared madre se divide en 4 hijas.";
    case SpeciesId::Didinium: return "Cazador en barril: dispara su probosis contra un paramecio y lo traga entero. Con hambre se enquista.";
    case SpeciesId::Vorticella: return "Campana sobre un tallo con un resorte (espasmonema): se contrae en milisegundos ante cualquier susto.";
    case SpeciesId::Stentor: return "Trompeta azul fija por el pie; su corona de membranelas crea un remolino que trae bacterias y algas.";
    case SpeciesId::Podophrya: return "Suctorio: esfera con tentaculos capitados que se pegan a los ciliados y les succionan el citoplasma.";
    case SpeciesId::Euplotes: return "Ciliado aplanado que 'camina' sobre cirros (haces de cilios) con paradas bruscas.";
    case SpeciesId::Arcella: return "Ameba que vive dentro de una testa parda en forma de cupula; asoma seudopodos por la abertura.";
    case SpeciesId::Lacrymaria: return "'Lagrima de cisne': estira un cuello hasta 7 veces su cuerpo en segundos para atrapar presas.";
    case SpeciesId::Spirogyra: return "Alga filamentosa con el cloroplasto en espiral. Crece a lo largo y se divide con un tabique.";
    case SpeciesId::Navicula: return "Diatomea en una cajita de silice que se desliza por una ranura (rafe). Cada division achica a una hija. Necesita silice disuelta para cada valva nueva y vitamina B12 de las bacterias.";
    case SpeciesId::Volvox: return "Esfera hueca de cientos de celulas que rueda hacia la luz. Las colonias hijas crecen adentro y nacen rompiendo a la madre.";
    case SpeciesId::Pediastrum: return "Colonia plana en estrella de 16 celulas con cuernos; cada ciclo libera una colonia hija.";
    case SpeciesId::Candida: return "Levadura: absorbe azucares disueltos, respira o fermenta sin oxigeno, y se reproduce por gemacion.";
    case SpeciesId::Rotifer: return "Rotifero (Brachionus): animal de ~1000 celulas con corona ciliada, mastax que mastica y lorica; pone huevos sin macho.";
    case SpeciesId::Hydra: return "Polipo fijo al fondo: sus tentaculos disparan nematocistos que paralizan a la presa y la llevan a la boca.";
    case SpeciesId::Nauplius: return "Larva de copepodo con tres pares de patas y un ojo rojo; avanza a saltos y filtra algas.";
    case SpeciesId::Naegleria: return "Ameboflagelado de suelos y aguas termales: tolera mas de 40 grados y se enquista si se seca.";
    case SpeciesId::Trichonympha: return "Flagelado con miles de flagelos del intestino de las termitas: digiere la madera por ellas y no tolera el oxigeno.";
    case SpeciesId::Philodina: return "Rotifero bdeloideo: se arrastra como una sanguijuela. Desde hace millones de anios son todas hembras.";
    case SpeciesId::Mougeotia: return "Alga filamentosa con un cloroplasto en placa que gira: de frente con poca luz y de canto con mucha.";
    case SpeciesId::Zygnema: return "Alga filamentosa con dos cloroplastos en estrella por celula. Vive en charcas y sobre el suelo humedo.";
    case SpeciesId::Oedogonium: return "Alga filamentosa: cada division deja un anillo (capuchon) en la pared, asi se cuentan las veces que se dividio.";
    case SpeciesId::Rhabditis: return "Nematodo: ondula el cuerpo entero. En agua libre se agita sin avanzar mucho; entre granos repta con facilidad.";
    case SpeciesId::Chaetonotus: return "Gastrotrico: animal diminuto cubierto de espinas que se desliza con cilios y tiene una cola en horquilla.";
    default: return "";
    }
}

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
    if (o.zoophagusCaptures > 0) add("Zoophagus atrapa un rotifero con sus clavijas");
    if (o.buds > 0) add("Gemacion de levaduras o brotes de Hydra");
    if (o.colonyReleases > 0) add("Una colonia libera colonias hijas");
    if (o.filamentDivisions > 0) add("Spirogyra se divide dentro del filamento");
    if (o.phageLyses > 0) add("Bacterias que estallan por fagos");
    if (o.nitrogenFixed > 0.0f) add("Heterocistos fijan nitrogeno del aire");
    if (o.quorumSwitches > 0) add("Quorum sensing: C. violaceum se vuelve violeta");
    if (o.bdelloBursts > 0) add("Bdellovibrio sale de un bdeloplasto con sus hijas");
    if (o.hydraCaptures > 0) add("Hydra paraliza una presa con nematocistos");
    if (o.eggsLaid > 0) add("Un rotifero pone un huevo sin macho");
    if (o.eggsHatched > 0) add("Eclosiona un huevo de rotifero");
    if (o.arrivals > 0) add("Llega un nauplio de copepodo");
    if (o.zoospores > 0) add("Oedogonium libera una zoospora con una corona de flagelos");
    if (o.zoosporesSettled > 0) add("Una zoospora se fija y empieza un filamento nuevo");
    return fresh;
}
