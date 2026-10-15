#include "bio/body_plan.hpp"


namespace {

using K = StructureKind;
using S = StructureSource;
using A = Arrangement;

// Tamaños 0: los da la simulación (núcleo, vacuolas, organelos con partícula propia).
// Las estructuras declaradas eucariotas se muestran como UN representante junto a su ancla (un
// dictiosoma, unas cisternas de RE): la célula real tiene varios o muchos; el modelo no inventa su
// número ni su posición exacta.

// --- Bacterias -----------------------------------------------------------------------------

// Bacilo gramnegativo genérico (E. coli): envoltura de ~30 nm (membrana externa, periplasma con
// peptidoglicano fino, membrana interna; Silhavy et al. 2010), nucleoide, ~2·10⁴ ribosomas
// (7·10³ a 7·10⁴ según el crecimiento; Bremer & Dennis 2008), ~4 flagelos perítricos de 20 nm ×
// 5–10 µm (Turner & Berg 2000) y fimbrias tipo 1 (100–500, 7 nm × 1–2 µm).
constexpr StructureSpec kEcoli[] = {
    {K::OuterMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana externa"},
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.004f, 1, false, "peptidoglicano (fino)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana interna"},
    {K::Nucleoid, S::Declared, A::Interior, 1.0f, 0.5f, 1, false, "nucleoide (cromosoma circular)"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 20000, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Peritrichous, 7.0f, 0.02f, 4, false, "flagelos (haz al nadar, sueltos al girar)"},
    {K::Pili, S::Declared, A::Peritrichous, 1.0f, 0.007f, 200, false, "fimbrias tipo 1"},
};

// Chromobacterium violaceum: gramnegativa con flagelo polar. También tiene flagelos laterales,
// que el modelo todavía no representa (su número no está en los datos).
constexpr StructureSpec kViolaceum[] = {
    {K::OuterMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana externa"},
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.004f, 1, false, "peptidoglicano (fino)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana interna"},
    {K::Nucleoid, S::Declared, A::Interior, 1.0f, 0.5f, 1, false, "nucleoide"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 20000, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Polar, 6.0f, 0.02f, 1, false, "flagelo polar"},
};

// Bacillus subtilis: grampositiva, sin membrana externa; pared de peptidoglicano de 30–40 nm
// (20–80 nm en grampositivas) y ~26 flagelos perítricos (Guttenplan et al. 2013).
constexpr StructureSpec kBacillus[] = {
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.035f, 1, false, "peptidoglicano (grueso)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana plasmatica"},
    {K::Nucleoid, S::Declared, A::Interior, 1.0f, 0.5f, 1, false, "nucleoide"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 20000, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Peritrichous, 6.0f, 0.02f, 20, false, "flagelos peritricos"},
};

// Bdellovibrio bacteriovorus: vibrio de ~1 × 0,3 µm, gramnegativo, con un flagelo polar envainado
// (la vaina lo engrosa a ~28 nm). Ribosomas: tiene, pero su número no está en los datos (0: no se
// dibujan).
constexpr StructureSpec kBdello[] = {
    {K::OuterMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana externa"},
    {K::Peptidoglycan, S::Declared, A::Envelope, 0.0f, 0.004f, 1, false, "peptidoglicano (fino)"},
    {K::PlasmaMembrane, S::Declared, A::Envelope, 0.0f, 0.007f, 1, false, "membrana interna"},
    {K::Nucleoid, S::Declared, A::Interior, 0.5f, 0.2f, 1, false, "nucleoide"},
    {K::Ribosomes, S::Declared, A::Interior, 0.02f, 0.02f, 0, false, "ribosomas 70S"},
    {K::Flagellum, S::Simulated, A::Polar, 3.0f, 0.028f, 1, false, "flagelo polar envainado"},
};

// --- Virus ---------------------------------------------------------------------------------

// Fago T4 (Myoviridae): cabeza icosaédrica alargada de 78 × 111 nm con ~56 µm de ADN doble (169
// kpb) empaquetado, cola contráctil de 100 × 20 nm (al contraerse queda en ~40 nm e inyecta el
// ADN), placa basal de ~52 nm y 6 fibras largas de ~145 nm. Sin envoltura (Leiman et al. 2003).
constexpr StructureSpec kT4[] = {
    {K::Capsid, S::Simulated, A::Interior, 0.111f, 0.078f, 1, false, "capside (cabeza icosaedrica)"},
    {K::ViralGenome, S::Declared, A::Interior, 56.0f, 0.002f, 1, false, "ADN doble (169 kpb, empaquetado)"},
    {K::Tail, S::Declared, A::Polar, 0.1f, 0.02f, 1, false, "cola contractil"},
    {K::Baseplate, S::Declared, A::Polar, 0.052f, 0.01f, 1, false, "placa basal"},
    {K::TailFibers, S::Declared, A::Polar, 0.145f, 0.002f, 6, false, "fibras de la cola"},
};

// --- Eucariotas ----------------------------------------------------------------------------

// Partes comunes. RE, Golgi y ribosomas libres existen en todos estos eucariotas pero no se simulan.
// Ribosomas 80S (~25 nm): del orden de 10³–10⁴ por µm³ de citoplasma en células eucariotas en
// crecimiento (se usa 3·10³; varía con la especie y el estado).
#define BIO_EUK_ENDOMEMBRANES                                                                        \
    {K::EndoplasmicReticulum, S::Declared, A::Perinuclear, 3.0f, 0.05f, 1, false, "reticulo endoplasmico rugoso"}, \
        {K::Golgi, S::Declared, A::Perinuclear, 1.0f, 0.3f, 1, false, "aparato de Golgi (un dictiosoma)"},          \
        {K::Ribosomes, S::Declared, A::Interior, 0.025f, 0.025f, 0, false, "ribosomas 80S", 3000.0f}

constexpr StructureSpec kCiliate[] = {
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, true, "pelicula"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "macronucleo"},
    {K::Nucleolus, S::Declared, A::Interior, 1.5f, 1.5f, 1, false, "nucleolos"},
    {K::Mitochondrion, S::Simulated, A::Interior, 2.0f, 0.5f, 0, false, "mitocondrias"},
    {K::Lysosome, S::Simulated, A::Interior, 0.0f, 0.0f, 0, false, "lisosomas"},
    {K::FoodVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "vacuolas digestivas"},
    {K::ContractileVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "vacuola contractil"},
    {K::StorageGranule, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "granulos de glucogeno"},
    BIO_EUK_ENDOMEMBRANES,
};

constexpr StructureSpec kAmoeba[] = {
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, true, "membrana plasmatica"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
    {K::Nucleolus, S::Declared, A::Interior, 1.5f, 1.5f, 1, false, "nucleolo"},
    {K::Mitochondrion, S::Simulated, A::Interior, 2.0f, 0.5f, 0, false, "mitocondrias"},
    {K::Lysosome, S::Simulated, A::Interior, 0.0f, 0.0f, 0, false, "lisosomas"},
    {K::FoodVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "vacuolas digestivas"},
    {K::ContractileVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "vacuola contractil"},
    BIO_EUK_ENDOMEMBRANES,
};

constexpr StructureSpec kEuglena[] = {
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, true, "pelicula"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
    {K::Nucleolus, S::Declared, A::Interior, 1.5f, 1.5f, 1, false, "nucleolo"},
    {K::Chloroplast, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "cloroplastos"},
    {K::Mitochondrion, S::Simulated, A::Interior, 2.0f, 0.5f, 0, false, "mitocondrias"},
    {K::Eyespot, S::Declared, A::Anterior, 2.0f, 2.0f, 1, true, "estigma (mancha ocular)"},
    {K::ContractileVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "vacuola contractil"},
    {K::StorageGranule, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "granulos de paramilon"},
    BIO_EUK_ENDOMEMBRANES,
};

// Chlamydomonas: un cloroplasto en copa con pirenoide, estigma y dos vacuolas contráctiles; pared
// de glicoproteínas.
constexpr StructureSpec kChlamydomonas[] = {
    {K::CellWall, S::Declared, A::Envelope, 0.0f, 0.2f, 1, true, "pared de glicoproteinas"},
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, false, "membrana plasmatica"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
    {K::Chloroplast, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "cloroplasto en copa"},
    {K::Pyrenoid, S::Declared, A::Interior, 2.0f, 2.0f, 1, true, "pirenoide"},
    {K::Eyespot, S::Declared, A::Interior, 1.0f, 1.0f, 1, true, "estigma"},
    {K::Mitochondrion, S::Simulated, A::Interior, 2.0f, 0.5f, 0, false, "mitocondrias"},
    {K::ContractileVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 2, true, "vacuolas contractiles"},
    BIO_EUK_ENDOMEMBRANES,
};

// Algas filamentosas y colonias: pared de celulosa, cloroplastos con pirenoides, vacuola central.
constexpr StructureSpec kGreenAlga[] = {
    {K::CellWall, S::Declared, A::Envelope, 0.0f, 0.5f, 1, true, "pared de celulosa"},
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, false, "membrana plasmatica"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
    {K::Chloroplast, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "cloroplasto"},
    {K::Pyrenoid, S::Declared, A::Interior, 2.0f, 2.0f, 0, true, "pirenoides"},
    {K::Mitochondrion, S::Simulated, A::Interior, 2.0f, 0.5f, 0, false, "mitocondrias"},
    BIO_EUK_ENDOMEMBRANES,
};

// Diatomea: frústula de sílice, plastos dorados.
constexpr StructureSpec kDiatom[] = {
    {K::CellWall, S::Simulated, A::Envelope, 0.0f, 0.3f, 1, true, "frustula de silice"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
    {K::Chloroplast, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "plastos dorados"},
    {K::Mitochondrion, S::Simulated, A::Interior, 2.0f, 0.5f, 0, false, "mitocondrias"},
    BIO_EUK_ENDOMEMBRANES,
};

// Levadura: pared de glucano y manano, vacuola grande, sin vacuola contráctil.
constexpr StructureSpec kYeast[] = {
    {K::CellWall, S::Declared, A::Envelope, 0.0f, 0.15f, 1, false, "pared de glucano y manano"},
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, false, "membrana plasmatica"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, false, "nucleo"},
    {K::Mitochondrion, S::Simulated, A::Interior, 1.0f, 0.4f, 0, false, "mitocondrias"},
    BIO_EUK_ENDOMEMBRANES,
};

// Trichonympha (parabasálido): sin mitocondrias; su orgánulo energético es el hidrogenosoma
// (~0,5–1 µm, sin cristas, fermenta piruvato a H₂ y acetato; Müller 1993). Las partículas que la
// simulación etiqueta como orgánulo energético (`OrganelleTag::Mitochondrion`) se muestran así.
constexpr StructureSpec kParabasalid[] = {
    {K::PlasmaMembrane, S::Simulated, A::Envelope, 0.0f, 0.0f, 0, true, "membrana plasmatica"},
    {K::Nucleus, S::Simulated, A::Interior, 0.0f, 0.0f, 1, true, "nucleo"},
    {K::Hydrogenosome, S::Simulated, A::Interior, 0.8f, 0.8f, 0, false, "hidrogenosomas"},
    {K::FoodVacuole, S::Simulated, A::Interior, 0.0f, 0.0f, 0, true, "vacuolas con madera"},
    BIO_EUK_ENDOMEMBRANES,
};

#undef BIO_EUK_ENDOMEMBRANES

constexpr BodyPlan kBacteriaPlans[] = {
    {"bacilo gramnegativo (tipo E. coli)", Morphology::Rod, kEcoli},
    {"Chromobacterium violaceum", Morphology::Rod, kViolaceum},
    {"Bacillus subtilis", Morphology::Rod, kBacillus},
};
constexpr BodyPlan kBdelloPlan = {"Bdellovibrio bacteriovorus", Morphology::Vibrio, kBdello};
constexpr BodyPlan kVirusPlans[] = {
    {"fago tipo T4", Morphology::HeadTailPhage, kT4},
};

constexpr BodyPlan kCiliatePlan = {"ciliado", Morphology::Eukaryote, kCiliate};
constexpr BodyPlan kAmoebaPlan = {"ameba", Morphology::Eukaryote, kAmoeba};
constexpr BodyPlan kEuglenaPlan = {"euglenido", Morphology::Eukaryote, kEuglena};
constexpr BodyPlan kChlamyPlan = {"clorofita flagelada", Morphology::Eukaryote, kChlamydomonas};
constexpr BodyPlan kGreenPlan = {"alga verde", Morphology::Eukaryote, kGreenAlga};
constexpr BodyPlan kDiatomPlan = {"diatomea", Morphology::Eukaryote, kDiatom};
constexpr BodyPlan kYeastPlan = {"levadura", Morphology::Eukaryote, kYeast};
constexpr BodyPlan kParabasalidPlan = {"parabasalido", Morphology::Eukaryote, kParabasalid};

} // namespace

const char* structureName(StructureKind kind) {
    switch (kind) {
    case K::PlasmaMembrane: return "membrana plasmatica";
    case K::OuterMembrane: return "membrana externa";
    case K::Peptidoglycan: return "peptidoglicano";
    case K::CellWall: return "pared celular";
    case K::Capsule: return "capsula";
    case K::Nucleoid: return "nucleoide";
    case K::Ribosomes: return "ribosomas";
    case K::Flagellum: return "flagelo";
    case K::Pili: return "pili";
    case K::Nucleus: return "nucleo";
    case K::Nucleolus: return "nucleolo";
    case K::Mitochondrion: return "mitocondria";
    case K::Hydrogenosome: return "hidrogenosoma";
    case K::Chloroplast: return "cloroplasto";
    case K::Pyrenoid: return "pirenoide";
    case K::Eyespot: return "estigma";
    case K::EndoplasmicReticulum: return "reticulo endoplasmico";
    case K::Golgi: return "aparato de Golgi";
    case K::ContractileVacuole: return "vacuola contractil";
    case K::FoodVacuole: return "vacuola digestiva";
    case K::Lysosome: return "lisosoma";
    case K::StorageGranule: return "granulo de reserva";
    case K::Capsid: return "capside";
    case K::ViralGenome: return "genoma viral";
    case K::ViralEnvelope: return "envoltura viral";
    case K::Tail: return "cola";
    case K::Baseplate: return "placa basal";
    case K::TailFibers: return "fibras de la cola";
    case K::Count: break;
    }
    return "?";
}

const BodyPlan& bacteriumPlan(std::uint8_t strain) { return kBacteriaPlans[strain < 3 ? strain : 0]; }

const BodyPlan& bdellovibrioPlan() { return kBdelloPlan; }

const BodyPlan& virusPlan(VirusKind kind) { return kVirusPlans[static_cast<int>(kind)]; }

const BodyPlan& speciesPlan(SpeciesId id) {
    switch (id) {
    case SpeciesId::Paramecium:
    case SpeciesId::ParameciumAurelia:
    case SpeciesId::Didinium:
    case SpeciesId::Vorticella:
    case SpeciesId::Stentor:
    case SpeciesId::Podophrya:
    case SpeciesId::Euplotes:
    case SpeciesId::Lacrymaria: return kCiliatePlan;
    case SpeciesId::Amoeba:
    case SpeciesId::Arcella:
    case SpeciesId::Naegleria: return kAmoebaPlan;
    case SpeciesId::Euglena: return kEuglenaPlan;
    case SpeciesId::Chlamydomonas:
    case SpeciesId::Volvox: return kChlamyPlan;
    case SpeciesId::Spirogyra:
    case SpeciesId::Mougeotia:
    case SpeciesId::Zygnema:
    case SpeciesId::Oedogonium:
    case SpeciesId::Pediastrum: return kGreenPlan;
    case SpeciesId::Navicula:
    case SpeciesId::Asterionella:
    case SpeciesId::Cyclotella: return kDiatomPlan;
    case SpeciesId::Candida: return kYeastPlan;
    case SpeciesId::Trichonympha: return kParabasalidPlan;
    case SpeciesId::Count: break;
    }
    return kAmoebaPlan;
}
