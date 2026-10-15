#include "bio/species.hpp"

namespace {

CellSpec makeSpec(float rx, float ry, float shapeRate, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  float nucleus = 0.32f, float squareness = 2.0f, float taper = 0.0f) {
    CellSpec s;
    s.radiusX = rx;
    s.radiusY = ry;
    s.shapeRate = shapeRate;
    s.r = r;
    s.g = g;
    s.b = b;
    s.nucleusFraction = nucleus;
    s.squareness = squareness;
    s.taper = taper;
    return s;
}

const SpeciesInfo kSpecies[kSpeciesCount] = {
    // Paramecium caudatum: ~180 × 50 µm, ciliado bacterívoro.
    {.name = "Paramecium", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(90.0f, 25.0f, 8.0f, 160, 176, 138), .feeding = FeedingMode::OralGroove,
     .fleeMask = speciesBit(SpeciesId::Didinium) | speciesBit(SpeciesId::Lacrymaria), .chemokinesis = true,
     .acidTrap = true, .initialCount = 20, .tMin = 5.0f, .tMax = 35.0f, .cyclosis = 4.0f},
    // Euglena gracilis: ~50 × 14 µm, mixótrofa con cloroplastos y estigma.
    {.name = "Euglena", .locomotion = LocomotionType::Flagellum,
     .spec = makeSpec(25.0f, 7.0f, 4.0f, 120, 172, 92), .feeding = FeedingMode::Anterior,
     .fleeMask = speciesBit(SpeciesId::Lacrymaria), .phototaxis = true, .initialCount = 20,
     .chloroplasts = true, .vitaminB12 = true, .phMin = 3.0f, .tMin = 10.0f, .tMax = 35.0f, .cyclosis = 1.0f},
    // Amoeba proteus (chica): ~180 µm, fagocita algas y ciliados.
    {.name = "Amoeba", .locomotion = LocomotionType::Pseudopod,
     .spec = makeSpec(90.0f, 90.0f, 0.0f, 188, 176, 160, 0.2f), .feeding = FeedingMode::Phagocytosis,
     .preyMask = speciesBit(SpeciesId::Euglena) | speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Paramecium) | speciesBit(SpeciesId::ParameciumAurelia) | speciesBit(SpeciesId::Euplotes) | speciesBit(SpeciesId::Navicula) | speciesBit(SpeciesId::Asterionella) | speciesBit(SpeciesId::Cyclotella) | speciesBit(SpeciesId::Pediastrum),
     .initialCount = 2, .handlingHours = 1.5f, .tMin = 10.0f, .tMax = 30.0f},
    // Chlamydomonas reinhardtii: ~10 µm, dos flagelos, un gran cloroplasto en copa; se duplica
    // en ~6–8 h con buena luz (3× la tasa de Euglena). Se divide dentro de la pared madre en
    // 4 hijas (palintomía).
    {.name = "Chlamydomonas", .locomotion = LocomotionType::Flagellum,
     .spec = makeSpec(5.5f, 4.5f, 6.0f, 110, 170, 80), .speedScale = 1.25f,
     .fleeMask = speciesBit(SpeciesId::Amoeba), .phototaxis = true, .photosynthesisScale = 3.0f, .fissions = 2,
     .initialCount = 150, .chloroplasts = true, .phMin = 5.0f, .tMin = -2.0f, .tMax = 35.0f,
     .salinityMax = 60.0f},
    // Didinium nasutum: ~110 × 80 µm, en barril; caza paramecios con su probóscide y los traga
    // enteros. Cuando pasa hambre se enquista.
    {.name = "Didinium", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(55.0f, 40.0f, 8.0f, 176, 168, 140), .feeding = FeedingMode::Raptorial,
     .preyMask = speciesBit(SpeciesId::Paramecium) | speciesBit(SpeciesId::ParameciumAurelia), .encysts = true, .initialCount = 2, .handlingHours = 3.0f,
     .tMin = 10.0f, .tMax = 30.0f, .cyclosis = 2.0f},
    // Vorticella campanula: campana de ~80 µm sobre un tallo de ~150 µm con espasmonema (se
    // contrae en ~10 ms, el movimiento más rápido conocido en una célula). Filtra bacterias con
    // la corona ciliar del peristoma. Al dividirse, una hija nada como telotroco y se fija.
    {.name = "Vorticella", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(40.0f, 30.0f, 10.0f, 182, 186, 168), .speedScale = 0.4f,
     .feeding = FeedingMode::OralGroove, .initialCount = 8, .sessile = true, .stalkLength = 150.0f,
     .contractileStalk = true, .tMin = 5.0f, .tMax = 32.0f, .cyclosis = 2.0f},
    // Stentor coeruleus: trompeta azul (pigmento stentorina), extendida hasta ~1 mm; aquí una
    // contraída de ~200 µm. Se fija por el pie; su corriente oral trae bacterias y algas chicas.
    {.name = "Stentor", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(100.0f, 55.0f, 6.0f, 120, 150, 200, 0.32f, 2.0f, 0.8f), .speedScale = 0.5f,
     .feeding = FeedingMode::OralGroove, .initialCount = 2, .sessile = true, .stalkLength = 20.0f,
     .mouthPreyMask = speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Euglena) | speciesBit(SpeciesId::Navicula) | speciesBit(SpeciesId::Asterionella) | speciesBit(SpeciesId::Cyclotella),
     .handlingHours = 0.5f, .tMin = 5.0f, .tMax = 28.0f, .cyclosis = 3.0f},
    // Podophrya fixa (suctorio): esfera de ~35 µm sobre un tallo; sin cilios de adulto. Sus
    // tentáculos capitados se adhieren a los ciliados que pasan y les succionan el citoplasma.
    {.name = "Podophrya", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(18.0f, 17.0f, 10.0f, 176, 180, 170), .speedScale = 0.5f,
     .feeding = FeedingMode::Tentacles,
     .preyMask = speciesBit(SpeciesId::Paramecium) | speciesBit(SpeciesId::ParameciumAurelia) | speciesBit(SpeciesId::Euplotes) | speciesBit(SpeciesId::Vorticella),
     .initialCount = 2, .sessile = true, .stalkLength = 40.0f, .tentacleReach = 45.0f, .handlingHours = 4.0f,
     .tMin = 5.0f, .tMax = 30.0f},
    // Euplotes patella: hipotrico aplanado de ~90 µm que "camina" sobre el vidrio con sus
    // cirros, con paradas bruscas. Pasta bacterias y algas chicas.
    {.name = "Euplotes", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(45.0f, 32.0f, 10.0f, 178, 176, 160), .speedScale = 0.25f,
     .feeding = FeedingMode::OralGroove,
     .fleeMask = speciesBit(SpeciesId::Lacrymaria) | speciesBit(SpeciesId::Didinium), .initialCount = 4,
     .mouthPreyMask = speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Navicula) | speciesBit(SpeciesId::Asterionella) | speciesBit(SpeciesId::Cyclotella), .walker = true,
     .handlingHours = 1.0f, .tMin = -2.0f, .tMax = 32.0f, .salinityMax = 45.0f, .cyclosis = 2.0f},
    // Arcella vulgaris: ameba con testa (caparazón orgánico pardo de ~100 µm). Pseudópodos
    // lentos por la abertura; come bacterias y algas; la testa la protege de las amebas.
    {.name = "Arcella", .locomotion = LocomotionType::Pseudopod,
     .spec = makeSpec(50.0f, 50.0f, 0.0f, 196, 170, 120, 0.25f), .speedScale = 0.5f,
     .feeding = FeedingMode::Phagocytosis,
     .preyMask = speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Euglena) | speciesBit(SpeciesId::Navicula) | speciesBit(SpeciesId::Asterionella) | speciesBit(SpeciesId::Cyclotella),
     .initialCount = 3, .handlingHours = 1.0f, .tMin = 0.0f, .tMax = 30.0f},
    // Lacrymaria olor ("lágrima de cisne"): cuerpo de ~100 µm y un cuello que se estira hasta
    // 7–8 veces ese largo en segundos para atrapar ciliados y flagelados chicos.
    {.name = "Lacrymaria", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(50.0f, 15.0f, 6.0f, 184, 188, 172), .speedScale = 0.5f,
     .feeding = FeedingMode::NeckStrike,
     .preyMask = speciesBit(SpeciesId::Chlamydomonas) | speciesBit(SpeciesId::Euglena) | speciesBit(SpeciesId::Euplotes),
     .initialCount = 2, .neckReach = 400.0f, .handlingHours = 1.5f, .tMin = 5.0f, .tMax = 30.0f,
     .cyclosis = 2.0f},
    // Spirogyra: alga verde filamentosa; células cilíndricas de ~60 × 30 µm unidas extremo con
    // extremo, con un cloroplasto en cinta helicoidal. Crece por división intercalar y se
    // fragmenta. Forma matas que dan refugio.
    {.name = "Spirogyra", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(30.0f, 15.0f, 15.0f, 140, 184, 118, 0.25f, 6.0f), .speedScale = 0.0f,
     .photosynthesisScale = 1.5f, .initialCount = 3, .chloroplasts = true, .filament = true,
     .filamentCells = 8, .fragmentationPerHour = 0.02f, .dryMassScale = 0.3f, .tMin = 5.0f, .tMax = 35.0f,
     .cyclosis = 2.0f},
    // Navicula: diatomea pennada de ~50 × 14 µm en frústula de sílice; se desliza por el rafe
    // a unos µm/s con inversiones. Cloroplastos dorados (fucoxantina).
    {.name = "Navicula", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(25.0f, 7.0f, 40.0f, 200, 182, 120), .speedScale = 0.012f, .photosynthesisScale = 1.5f,
     .initialCount = 20, .chloroplasts = true, .glider = true, .vitaminB12 = true, .frustule = true, .phMin = 6.5f, .tMin = -2.0f,
     .tMax = 35.0f, .salinityMax = 70.0f},
    // Volvox: colonia esférica de ~250 µm con cientos de células biflageladas en su superficie;
    // rueda nadando hacia la luz. Las colonias hijas crecen adentro y nacen rompiendo a la madre.
    {.name = "Volvox", .locomotion = LocomotionType::Flagellum,
     .spec = makeSpec(120.0f, 120.0f, 20.0f, 150, 196, 120, 0.12f), .speedScale = 1.5f, .phototaxis = true,
     .initialCount = 2, .chloroplasts = true, .colonyDaughters = 4, .motherDiesAtRelease = true,
     .dryMassScale = 0.03f, .tMin = 10.0f, .tMax = 32.0f},
    // Pediastrum: colonia plana (cenobio) de 16 células en estrella, inmóvil; cada ciclo libera
    // una colonia hija formada por zoosporas. Es una placa de una célula de espesor (~10 µm).
    {.name = "Pediastrum", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(30.0f, 30.0f, 30.0f, 150, 190, 110, 0.15f), .speedScale = 0.0f, .initialCount = 6,
     .chloroplasts = true, .colonyDaughters = 1, .dryMassScale = 0.25f, .tMin = 5.0f, .tMax = 35.0f},
    // Candida (levadura de agua dulce): célula oval de ~6 × 5 µm con pared de quitina, inmóvil.
    // Absorbe azúcares y aminoácidos disueltos; respira con O₂ y fermenta sin él. Se reproduce
    // por gemación: la yema crece en un polo y se separa (cicatriz de gemación).
    {.name = "Candida", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(3.2f, 2.6f, 12.0f, 214, 206, 176, 0.3f), .speedScale = 0.0f,
     .feeding = FeedingMode::Osmotroph, .initialCount = 12, .colonyDaughters = 1, .budding = true,
     .phMin = 3.0f, .phMax = 8.5f, .tMin = 0.0f, .tMax = 40.0f, .salinityMax = 60.0f},
    // Naegleria gruberi: ameboflagelado de ~20 µm (forma limax, seudópodos eruptivos) que come
    // bacterias; tolera hasta ~45 °C (aguas termales) y forma quistes.
    {.name = "Naegleria", .locomotion = LocomotionType::Pseudopod,
     .spec = makeSpec(11.0f, 8.0f, 0.0f, 196, 190, 170, 0.3f), .speedScale = 2.0f,
     .feeding = FeedingMode::Phagocytosis, .encysts = true, .initialCount = 6, .handlingHours = 0.3f,
     .phMin = 5.0f, .tMin = 10.0f, .tMax = 46.0f},
    // Trichonympha: flagelado hipermastigino de ~150 µm del intestino de las termitas, con miles
    // de flagelos en el rostro; anaerobio estricto. Fagocita astillas de madera por el polo
    // posterior y fermenta la celulosa.
    {.name = "Trichonympha", .locomotion = LocomotionType::Flagellum,
     .spec = makeSpec(75.0f, 40.0f, 6.0f, 200, 186, 150, 0.2f, 2.0f, 0.3f), .speedScale = 0.6f,
     .feeding = FeedingMode::Anterior, .initialCount = 12, .tMin = 15.0f, .tMax = 35.0f, .anaerobe = true,
     .mouthAtRear = true},
    // Mougeotia: filamento de células largas y finas (~80 × 16 µm) con un único cloroplasto en
    // placa axial que GIRA según la luz (fitocromo): con luz débil se pone de frente, con luz
    // fuerte de canto. Es el ejemplo clásico de movimiento de cloroplastos.
    {.name = "Mougeotia", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(40.0f, 8.0f, 15.0f, 150, 190, 128, 0.3f, 6.0f), .speedScale = 0.0f,
     .photosynthesisScale = 1.3f, .initialCount = 2, .chloroplasts = true, .filament = true,
     .filamentCells = 6, .fragmentationPerHour = 0.03f, .dryMassScale = 0.3f, .tMin = 2.0f, .tMax = 32.0f,
     .cyclosis = 1.5f},
    // Zygnema: células de ~40 × 28 µm con dos cloroplastos estrellados (cada uno con un
    // pirenoide) y el núcleo entre ambos. Vive en charcas y sobre suelo y musgo húmedos.
    {.name = "Zygnema", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(20.0f, 14.0f, 15.0f, 146, 186, 120, 0.25f, 6.0f), .speedScale = 0.0f,
     .photosynthesisScale = 1.4f, .initialCount = 2, .chloroplasts = true, .filament = true,
     .filamentCells = 8, .fragmentationPerHour = 0.02f, .dryMassScale = 0.35f, .tMin = 0.0f, .tMax = 30.0f,
     .cyclosis = 1.5f},
    // Oedogonium: células de ~70 × 22 µm con un cloroplasto parietal reticulado. Al dividirse,
    // la pared se rompe en un anillo y deja "capuchones" apilados en un extremo de la célula (se
    // cuentan las divisiones). También se reproduce por zoósporas: todo el protoplasto sale como
    // una célula grande con una corona de flagelos, nada ~1 h, se fija y forma un filamento nuevo.
    {.name = "Oedogonium", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(35.0f, 11.0f, 15.0f, 136, 182, 116, 0.3f, 6.0f), .speedScale = 0.0f,
     .photosynthesisScale = 1.4f, .initialCount = 2, .chloroplasts = true, .filament = true,
     .filamentCells = 7, .fragmentationPerHour = 0.02f, .zoosporeChance = 0.25f, .dryMassScale = 0.3f,
     .tMin = 5.0f, .tMax = 32.0f,
     .cyclosis = 1.5f},
    // Paramecium aurelia: ~130 × 40 µm, más chico que P. caudatum; mismo modo de vida (bacterívoro
    // por surco oral). En el cultivo mixto de Gause (1934) excluye a P. caudatum. (Simplificación:
    // la conjugación solo está modelada en P. caudatum.)
    {.name = "Paramecium aurelia", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(65.0f, 20.0f, 8.0f, 168, 180, 142), .feeding = FeedingMode::OralGroove,
     .fleeMask = speciesBit(SpeciesId::Didinium) | speciesBit(SpeciesId::Lacrymaria), .chemokinesis = true,
     .acidTrap = true, .initialCount = 20, .tMin = 5.0f, .tMax = 35.0f, .cyclosis = 4.0f},
    // Asterionella formosa: diatomea pennada planctónica, células de ~60 × 3 µm (sin rafe: no se
    // desliza). En la naturaleza forma colonias en estrella de ~8 células; acá células sueltas, y
    // algo más anchas (6 µm) para que el cuerpo blando sea estable. Tilman (1977).
    {.name = "Asterionella", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(32.0f, 3.0f, 40.0f, 206, 186, 118, 0.3f, 1.0f), .speedScale = 0.0f,
     .photosynthesisScale = 1.5f, .initialCount = 20, .chloroplasts = true, .frustule = true, .phMin = 6.0f,
     .tMin = 2.0f, .tMax = 30.0f},
    // Cyclotella meneghiniana: diatomea céntrica planctónica, disco de ~15 µm, inmóvil. Tilman (1977).
    {.name = "Cyclotella", .locomotion = LocomotionType::Cilia,
     .spec = makeSpec(8.0f, 8.0f, 40.0f, 204, 182, 112, 0.3f), .speedScale = 0.0f,
     .photosynthesisScale = 1.5f, .initialCount = 20, .chloroplasts = true, .frustule = true, .phMin = 6.0f,
     .tMin = 2.0f, .tMax = 35.0f},
};

} // namespace

const SpeciesInfo& speciesInfo(SpeciesId id) { return kSpecies[static_cast<int>(id)]; }

namespace {

// En el orden de SpeciesId. Reservas y paredes reales de cada grupo.
constexpr SpeciesChemistry kChemistry[kSpeciesCount] = {
    {Storage::Glycogen, CellWall::None},          // Paramecium
    {Storage::Paramylon, CellWall::None},         // Euglena
    {Storage::Glycogen, CellWall::None},          // Amoeba
    {Storage::Starch, CellWall::Cellulose},       // Chlamydomonas (almidón en el pirenoide)
    {Storage::Glycogen, CellWall::None},          // Didinium
    {Storage::Glycogen, CellWall::None},          // Vorticella
    {Storage::Glycogen, CellWall::None},          // Stentor
    {Storage::Glycogen, CellWall::None},          // Podophrya
    {Storage::Glycogen, CellWall::None},          // Euplotes
    {Storage::Glycogen, CellWall::None},          // Arcella
    {Storage::Glycogen, CellWall::None},          // Lacrymaria
    {Storage::Starch, CellWall::Cellulose},       // Spirogyra
    {Storage::Chrysolaminarin, CellWall::Silica}, // Navicula
    {Storage::Starch, CellWall::Cellulose},       // Volvox
    {Storage::Starch, CellWall::Cellulose},       // Pediastrum
    {Storage::Glycogen, CellWall::YeastGlucan},   // Candida
    {Storage::Glycogen, CellWall::None},          // Naegleria
    {Storage::Glycogen, CellWall::None},          // Trichonympha
    {Storage::Starch, CellWall::Cellulose},       // Mougeotia
    {Storage::Starch, CellWall::Cellulose},       // Zygnema
    {Storage::Starch, CellWall::Cellulose},       // Oedogonium
    {Storage::Glycogen, CellWall::None},          // Paramecium aurelia
    {Storage::Chrysolaminarin, CellWall::Silica}, // Asterionella
    {Storage::Chrysolaminarin, CellWall::Silica}, // Cyclotella
};

} // namespace

const SpeciesChemistry& speciesChemistry(SpeciesId id) { return kChemistry[static_cast<int>(id)]; }

SpeciesId playerSpecies(LocomotionType type) {
    switch (type) {
    case LocomotionType::Cilia: return SpeciesId::Paramecium;
    case LocomotionType::Flagellum: return SpeciesId::Euglena;
    case LocomotionType::Pseudopod: return SpeciesId::Amoeba;
    }
    return SpeciesId::Paramecium;
}

Genome speciesGenome(SpeciesId id) {
    Genome g = speciesGenome(speciesInfo(id).locomotion);
    g[Gene::Chloroplasts] = speciesInfo(id).chloroplasts ? 1.0f : 0.0f;
    if (speciesInfo(id).locomotion == LocomotionType::Cilia && speciesInfo(id).chloroplasts)
        g[Gene::Trichocysts] = 0.0f; // algas: sin tricocistos
    switch (id) {
    case SpeciesId::Didinium:   // toxicistos, no tricocistos
    case SpeciesId::Vorticella:
    case SpeciesId::Stentor:    // mucocistos
    case SpeciesId::Podophrya:
    case SpeciesId::Euplotes:
    case SpeciesId::Lacrymaria: // toxicistos en el cuello
        g[Gene::Trichocysts] = 0.0f;
        break;
    case SpeciesId::Arcella: g[Gene::Armor] = 0.9f; break; // testa
    case SpeciesId::Navicula: g[Gene::Armor] = 0.3f; break; // frústula: cuesta romperla, pero se come
    default: break;
    }
    return g;
}
