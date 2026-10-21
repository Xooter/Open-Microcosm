#pragma once

#include "bio/cell.hpp"
#include "bio/genome.hpp"
#include "bio/locomotion.hpp"
#include "bio/stalk.hpp"

#include <entt/entity/entity.hpp>

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Tipos de ser con cuerpo blando (especies de protistas, algas, levaduras y, en adelante, células
// de organismos pluricelulares). Son DATOS: se cargan de los paquetes de contenido
// (assets/content/base/beings/ y los del usuario) en un registro; el índice en el registro es
// el SpeciesId. Los nombres de abajo son los seres de fábrica en su orden de carga (las escenas
// viejas guardan ese índice): el código que todavía distingue especies por nombre los usa.
enum class SpeciesId : std::uint16_t {
    Paramecium,
    Euglena,
    Amoeba,
    Chlamydomonas,
    Didinium,
    Vorticella,
    Stentor,
    Podophrya,
    Euplotes,
    Arcella,
    Lacrymaria,
    Spirogyra,
    Navicula,
    Volvox,
    Pediastrum,
    Candida,
    Naegleria,
    Trichonympha,
    Mougeotia,
    Zygnema,
    Oedogonium,
    ParameciumAurelia, // Gause (1934): excluye a P. caudatum en cultivo mixto
    Asterionella,      // Tilman (1977): diatomeas que compiten por P y Si
    Cyclotella,
};
// Tope de tipos de ser cargados (tablas de tamaño fijo: censo, pesos de población).
inline constexpr int kMaxSpecies = 512;
// Cantidad de tipos de ser cargados (los de fábrica y los de los paquetes del usuario).
int speciesCount();
// Conjunto de tipos de ser (dietas, huidas, hospedadores).
using SpeciesSet = std::bitset<kMaxSpecies>;

// Química de las reservas y de la pared: lo que las tinciones ven (el color no sale del nombre de
// la especie sino de qué polisacárido guarda y de qué está hecha su pared).
enum class Storage : std::uint8_t {
    None,
    Starch,          // almidón verdadero en el cloroplasto (clorofitas): el yodo lo vuelve azul-negro
    Glycogen,        // glucógeno (animales, ciliados, amebas, levaduras): el yodo da marrón rojizo
    Paramylon,       // β-1,3-glucano de los euglénidos: NO reacciona con el yodo
    Chrysolaminarin, // β-glucano de las diatomeas (en una vacuola): NO reacciona con el yodo
};
enum class CellWall : std::uint8_t {
    None,        // película o membrana desnuda
    Cellulose,   // algas verdes
    Silica,      // frústula de diatomea
    YeastGlucan, // levaduras: pared gruesa de glucano y manano; se tiñe como grampositiva
};
struct SpeciesChemistry {
    Storage storage = Storage::None;
    CellWall wall = CellWall::None;
};
const SpeciesChemistry& speciesChemistry(SpeciesId id);

enum class FeedingMode : std::uint8_t {
    OralGroove,   // citostoma con corriente oral (Paramecium: bacterias y detritos)
    Anterior,     // polo anterior, poco (euglénidos)
    Phagocytosis, // contacto y copa fagocítica (Amoeba)
    Raptorial,    // traga presas enteras por el polo anterior (Didinium)
    Tentacles,    // tentáculos que atrapan y succionan ciliados (suctorios)
    NeckStrike,   // dispara un "neck" extensible hacia la presa (Lacrymaria)
    Osmotroph,    // absorbe materia orgánica disuelta por la pared (levaduras)
    Axopodia,     // heliozoos: los axopodios atrapan al contacto, se acortan y la traen al cuerpo, que la traga
    Perforation,  // Vampyrella: perfora la pared de un alga y le vacía el protoplasto
    None,         // autótrofa estricta (Chlamydomonas)
};

// Célula de un organismo pluricelular en cultivo (fase 20b): qué mecanismos tiene, por nombre en
// los datos ([being.cell]). Los valores medidos (rapidez, persistencia, fases del ciclo…) van con
// su fuente en [physiology.<id>.*].
// Prolongación de la superficie, en dos clases como en la realidad:
//  - del cuerpo (`inBody`): el cuerno de la teca de Ceratium, el brazo de una desmidia, la espina caudal
//    de Phacus. Tienen citoplasma adentro: son parte de la célula y de su anillo de membrana (la silueta
//    de bio/silhouette las une al cuerpo con un redondeo), y por eso chocan y se dividen con ella;
//  - de la cubierta: las espinas de una lórica, de las placas de Coleps o de una testa. No tienen
//    citoplasma y miden ~1 µm, menos que la resolución de la membrana (3 µm): se dibujan sobre la
//    cubierta (render) y no son parte del cuerpo blando.
// Y la escotadura (`notch`): lo contrario, una entrada en el cuerpo (el seno que separa las dos
// semicélulas de Cosmarium, las incisiones de los lóbulos de Micrasterias, el lado oral de Colpoda o el
// surco oral de Paramecium). Se resta de la silueta: `length` es la profundidad desde el borde,
// `base_width` la abertura y `tip_width` el fondo. Siempre es del cuerpo.
enum class ProcessKind : std::uint8_t { Horn, Spine, Arm, Seta, Notch };
struct ProcessSpec {
    ProcessKind kind = ProcessKind::Spine;
    float at = 0.0f;        // dirección desde el polo anterior [rad], en el plano de la vista
    float length = 10.0f;   // desde la superficie [µm], para la célula del tamaño típico
    float baseWidth = 2.0f; // [µm]
    float tipWidth = 0.5f;  // [µm]
    float curve = 0.0f;     // giro total a lo largo [rad] (positivo: hacia la izquierda del eje)
    int count = 1;          // copias: repartidas en `spread`, o en toda la vuelta si spread = 0
    float spread = 0.0f;    // abanico que cubren las copias [rad]
    bool inBody = true;     // parte de la célula (cuerno, brazo) o de su cubierta (espina de lórica)
    float fillet = 0.0f;    // radio del redondeo con el cuerpo [µm] (0: medio ancho de la base)
};
// Dirección [rad desde el polo anterior] de la copia k de una prolongación.
float processAngle(const ProcessSpec& p, int k);
// Punto del eje a la fracción s ∈ [0, 1] del largo, en el marco de la prolongación (base en el origen,
// sale por +x), con su curvatura; `scale` es el tamaño de la célula respecto del típico.
Vec2 processAxisPoint(const ProcessSpec& p, float s, float scale);
// Ancho a la fracción s del largo [µm].
inline float processWidth(const ProcessSpec& p, float s, float scale) {
    return scale * (p.baseWidth + (p.tipWidth - p.baseWidth) * s);
}

// Respuesta fóbica temporal (mecanismo M1, docs/plan_mecanicas.md): la célula compara lo que siente
// ahora con lo que sintió hace un momento (memoria de `memory` segundos) y reacciona a un cambio
// brusco, como la reacción de evitación de los ciliados (Jennings) o la respuesta fotofóbica de los
// flagelados (Diehn 1973). `StepUp`: subida relativa mayor que `threshold` con el nivel arriba de
// `minLevel`; `StepDown`: bajada relativa desde un nivel arriba de `minLevel`; `Above`: el nivel
// absoluto pasa `threshold` (la deformación del agua, mecanorrecepción: mecanismo M2).
inline constexpr std::size_t kMaxResponses = 4; // memoria fija en el piloto (sin asignaciones)
enum class ResponseField : std::uint8_t { Light, Oxygen, Shear, Touch };
enum class ResponseKind : std::uint8_t { StepUp, StepDown, Above };
enum class ResponseAction : std::uint8_t { Avoid, Jump, Contract, Flash };
struct ResponseSpec {
    ResponseField field = ResponseField::Light;
    ResponseKind kind = ResponseKind::StepUp;
    ResponseAction action = ResponseAction::Avoid;
    float threshold = 0.2f;  // cambio relativo (StepUp/StepDown) o nivel [1/s para el corte] (Above)
    float minLevel = 0.0f;   // nivel por debajo del cual no responde
    float memory = 1.0f;     // [s] memoria del nivel anterior
    float refractory = 1.0f; // [s] sin volver a responder
};

// Saltos (Halteria, Mesodinium): cirros o cerdas saltadoras la lanzan unos cuerpos de largo en
// decenas de ms. Espontáneos (intervalo medio `interval`) y de escape (respuesta con `action =
// "jump"`). Sin inercia: la velocidad es la del golpe mientras dura.
struct JumpSpec {
    float speed = 0.0f;    // [µm/s] (0: no salta)
    float duration = 0.05f; // [s]
    float interval = 0.0f; // [s] medio entre saltos espontáneos (0: solo de escape)
};

// Contracción del cuerpo entero (Stentor): los mionemas, disparados por Ca²⁺, acortan la
// célula en milisegundos y se re-extiende en segundos (Newman 1972; Mathijssen et al. 2019). La forma
// de la especie es la contraída; extendida mide `extension` veces su largo (la vista conserva el área).
// La dispara una respuesta con `action = "contract"` o, cada tanto, sola (`spontaneousPerMinute`).
// Forma extendida (`profile`):
//  - uniforme: la contraída estirada a lo largo, con el ancho en proporción inversa;
//  - trompeta (Stentor): disco oral ancho, cuerpo cónico y el extremo posterior fino como un pedúnculo
//    de la misma célula, que termina en el pie;
//  - campana (zooide de los peritricos: Vorticella, Epistylis): lados convexos desde la escópula, donde
//    se une al tallo, hasta el peristoma. Contraído, el peristoma se cierra y el zooide se redondea.
// En las dos, el radio va de `footWidth`·R en el polo posterior a R en el anterior como una potencia del
// largo (`flare`: > 1 se abre al final, < 1 se abre enseguida; la campana suma el labio del peristoma), y R sale de conservar el volumen de la
// contraída (el citoplasma es incompresible): la silueta no conserva el área.
enum class ContractionProfile : std::uint8_t { Uniform, Trumpet, Bell };
struct ContractionSpec {
    float extension = 1.0f;      // largo extendido / largo contraído (1: no se contrae)
    float extensionSeconds = 10.0f; // [s] para volver a extenderse
    ContractionProfile profile = ContractionProfile::Uniform;
    float footWidth = 0.1f;      // trompeta y campana: ancho del pie / el de la boca
    float flare = 0.0f;          // exponente del perfil (0: el del perfil, 3 trompeta y 0,5 campana)
    float swimmingExtension = 1.0f; // extensión (0–1) mientras nada suelta, sin pie fijo
    // Telotroco de los peritricos: nada como un cilindro corto (la contraída, más cuadrada: exponente de
    // superelipse; 0 = la contraída tal cual) y con el polo aboral adelante, la corona de cilios atrás
    // que lo empuja (Kreutz; Lynn 2008, The Ciliated Protozoa).
    float swimmingSquareness = 0.0f;
    bool aboralFirst = false;
    float spontaneousPerMinute = 0.0f; // contracciones sin estímulo
};

// Onda de forma (game/world_shape_wave.cpp): la forma de reposo cambia en el tiempo, sin cambiar de área.
//  - metabolia (`metaboly`): el movimiento euglenoide de Euglena y Peranema. La película de bandas
//    helicoidales que se deslizan entre sí deja que una onda de ensanchamiento corra del polo anterior al
//    posterior; mientras, la célula no nada (Arroyo, Heltai, Millán & DeSimone 2012, PNAS 109: 17874).
//    Viene en episodios (`episodesPerMinute`, `episodeSeconds`);
//  - vaivén (`sway`): la parte anterior se dobla de lado a lado mientras nada (la probóscide de Dileptus
//    que busca presas).
enum class ShapeWaveKind : std::uint8_t { None, Metaboly, Sway };
struct ShapeWaveSpec {
    ShapeWaveKind kind = ShapeWaveKind::None;
    float amplitude = 0.5f;      // metabolia: ensanchamiento máximo relativo; vaivén: desvío / semieje mayor
    float periodSeconds = 10.0f; // [s]
    float wavelength = 1.0f;     // metabolia: largo de la onda / largo de la célula
    float reach = 0.4f;          // vaivén: fracción anterior del cuerpo que se dobla
    float episodesPerMinute = 0.0f; // metabolia
    float episodeSeconds = 20.0f;   // [s]
};

// Señal química que libera la célula (M3): una kairomona del depredador (Lembadion), lo que suelta la
// presa (la "estomatina" de Tetrahymena)… Un campo por señal, que difunde solo si alguien la liberó.
struct ReleaseSpec {
    std::string signal;          // id de la señal (lo nombran las que responden)
    float rate = 0.0f;           // [pmol/h biológica] por célula (0 con `relay`: solo pulsos)
    float diffusion = 100.0f;    // [µm²/s] (moléculas chicas ~500; proteínas ~50–100)
    float halfLifeHours = 6.0f;  // [h biológicas]
    // La señal es parte de un proceso biológico rápido (el relevo de AMPc): difunde en tiempo biológico
    // (D × bioTimeScale) para que la onda tenga su largo real; si no, saldría √60 veces más corta.
    bool bioTime = false;
};

// Relevo de una señal (Dictyostelium: Tomchik & Devreotes 1981; Martiel & Goldbeter 1987): una célula
// competente (con `competenceHours` de hambre) que siente la señal por encima de `threshold`, o que es
// marcapasos y le toca, suelta un pulso y queda refractaria. La onda se propaga de célula en célula.
struct RelaySpec {
    bool present = false;
    float threshold = 0.01f;       // [µM]
    float pulse = 1e-5f;           // [pmol] por disparo
    float refractoryMinutes = 5.0f; // [min biológicos]
    float pacemakerFraction = 0.05f; // células que disparan solas
    float pacemakerMinutes = 6.0f;  // período de los marcapasos
    float competenceHours = 4.0f;   // de hambre hasta volverse competente
};

// Quimiotaxis hacia una señal que liberan otros: sube el gradiente solo mientras la señal sube (el frente
// de la onda), así no vuelve detrás de ella. Solo en células competentes si la especie tiene `relay`.
struct ChemotaxisSpec {
    std::string signal;
};

// Morfo inducido (M4): con la señal por encima del umbral, la célula cambia de forma en `hours`
// (Euplotes arma alas, Tetrahymena vorax pasa a macrostoma) y, completo el cambio, de dieta y de boca.
// Sin la señal vuelve atrás en el mismo tiempo.
struct MorphSpec {
    bool present = false;
    std::string name;            // para la ficha ("con alas", "macrostoma")
    std::string signal;
    float threshold = 0.0f;      // [µM]
    float hours = 6.0f;          // [h biológicas]
    float widthScale = 1.0f, lengthScale = 1.0f; // forma relativa a la de la especie
    float areaScale = 1.0f;      // área por la misma masa (alas finas, casi sin citoplasma)
    bool feedingSet = false;
    FeedingMode feeding = FeedingMode::None;
    bool preySet = false;
    SpeciesSet prey;             // resuelta como `prey`
    float mouthWidth = -1.0f;    // < 0: la de la especie
};

// Toxicistos (Didinium, Dileptus, Litonotus): extrusomas de la probóscide o del borde que, al tocar a
// la presa, la paralizan (y en general la matan) antes de tragarla (Hausmann 1978, Int. Rev. Cytol. 52:
// 197). Mecanismo M6: la presa queda `Paralyzed`.
struct ToxicystSpec {
    float reach = 0.0f;          // alcance desde el polo anterior [µm] (0: no tiene)
    float paralysisHours = 0.5f; // [h biológicas]
    bool lethal = true;          // al terminar la parálisis, la presa muere (lisis)
};

// Sensibilidad a una señal tóxica que liberan otros (los cuerpos R de los Paramecium killer, Sonneborn
// 1938): con la señal en `ec50` [µM] el daño equivale a la dosis tóxica de referencia (como la
// microcistina), y mata si la pasa.
struct SignalToxicitySpec {
    std::string signal;
    float ec50 = 0.0f; // 0: no es sensible
};

// Fotosimbiontes dentro de un heterótrofo: endosimbiontes (las Chlorella de Paramecium bursaria, que se
// dividen con luz dentro del huésped y le pasan azúcar; Karakashian 1963) o cleptoplastos (los
// cloroplastos que Mesodinium rubrum roba a las criptofitas y usa unas semanas; Johnson et al. 2007). Se
// adquieren al comer presas de `source` y se reparten en la división. La cantidad vive en
// Metabolism::symbionts (en unidades de la especie: algas o cloroplastos).
struct PhotosymbiontSpec {
    bool present = false;
    bool kleptoplasts = false;   // no se dividen adentro: solo decaen
    SpeciesSet source;           // resuelta como `prey`
    float perPrey = 1.0f;        // lo que aporta cada presa comida
    float max = 1.0f;
    float initial = 0.0f;        // con lo que nace la silvestre
    float growthPerHour = 0.0f;  // endosimbiontes: con luz, logística hasta `max`
    float decayPerHour = 0.0f;   // cleptoplastos: siempre; endosimbiontes: en oscuridad
    float photosynthesis = 1.0f; // con la carga completa, respecto de un alga de su biomasa
};

// Bioluminiscencia (Lingulodinium, Noctiluca): la deformación del agua abre canales en los
// centelleones y la luciferina emite un destello de ~0,1 s (respuesta con `action = "flash"`); la
// capacidad se recarga en minutos y la regula un reloj circadiano: de noche (subjetiva) brilla decenas
// de veces más que de día, aun con luz constante (Hastings & Sweeney 1957; Latz et al. 2004).
struct BioluminescenceSpec {
    bool present = false;
    float flashSeconds = 0.1f;   // [s] (física: el destello es en tiempo real)
    float rechargeSeconds = 60.0f; // [s] en recuperar la carga
    bool circadian = true;       // ritmo de 24 h biológicas; la placa empieza al amanecer subjetivo
    float dayCapacity = 0.02f;   // fracción de la de noche
    std::uint8_t color[3] = {80, 170, 255}; // ~475 nm
};

// Daño fotodinámico (Blepharisma: la blefarismina, con luz y O₂, genera especies reactivas; Giese
// 1946). Por encima de `threshold` (irradiancia relativa) se paga como estrés, `stress` con la
// lámpara al máximo (en las unidades del estrés de pH).
struct LightDamageSpec {
    float threshold = 1.0f;
    float stress = 0.0f;
};

// Pigmento de estrés (astaxantina de Haematococcus; Boussiba 2000): se acumula con luz alta o falta de
// nitrógeno, sobre todo en el quiste, y filtra la luz (protege del daño fotodinámico).
struct StressPigmentSpec {
    float ratePerHour = 0.0f;  // fracción del máximo por hora biológica con estrés (0: no tiene)
    float decayPerHour = 0.0f; // sin estrés
    float lightThreshold = 1.0f; // irradiancia relativa que lo induce
    float protection = 0.0f;   // fracción del daño por luz que evita con el pigmento al máximo
    std::uint8_t color[3] = {170, 40, 30};
};

// Recorrido pasivo de un nadador que no elige rumbo (espermatozoide: no tiene con qué ver ni esquivar).
// El batido, apenas asimétrico, curva la trayectoria con radio `curvatureRadius` [µm] (cerca del vidrio
// nadan en arcos y círculos; Woolley 2003, Reproduction 126: 259) y sus fluctuaciones la hacen difundir:
// el giro es un proceso de Ornstein–Uhlenbeck con tiempo de correlación `noiseSeconds` [s] y difusión
// rotacional efectiva `rotationalDiffusion` [rad²/s]. Los choques solo los desvía la mecánica.
struct SwimPathSpec {
    bool present = false;
    float curvatureRadius = 0.0f;     // 0: recto en promedio
    float rotationalDiffusion = 0.0f;
    float noiseSeconds = 0.3f;
};

// Fracciones de una muestra según las clases de motilidad de la OMS (2021): progresivos, no progresivos
// (nadan sin avanzar: en círculos chicos o en el lugar), inmóviles vivos y muertos. Suman 1.
struct MotilityMix {
    float progressive = 1.0f, nonProgressive = 0.0f, immotile = 0.0f, dead = 0.0f;
};

// Motilidad de una población de nadadores ([being.motility]): la muestra típica, la variación de la
// rapidez entre individuos y cómo nadan los no progresivos.
struct MotilitySpec {
    bool present = false;
    MotilityMix mix;
    float speedCv = 0.0f;             // coeficiente de variación de la rapidez entre individuos
    float nonProgressiveSpeed = 0.2f; // fracción de la rapidez de los progresivos
    float nonProgressiveRadius = 6.0f; // radio de sus círculos [µm]
};

struct AnimalCellInfo {
    bool present = false;
    bool adherent = true;              // adherida al fondo por adhesiones focales: el agua no la lleva
    float roundRadius = 9.0f;          // radio redondeada (mitosis, en suspensión) [µm]
    bool contactLocomotion = true;     // inhibición por contacto de la locomoción (Abercrombie)
    bool contactProliferation = true;  // inhibición por contacto de la proliferación
    float serumMinimum = 0.005f;       // suero (fracción) que pide el punto de restricción de G1
    // Horas sin factores de crecimiento antes de entrar en apoptosis (0: sobrevive quiescente).
    float apoptosisWithoutSerumHours = 0.0f;
    bool cycles = true;                // prolifera (el neutrófilo maduro ya no se divide)
    bool chemotaxisFmlp = false;       // sigue el gradiente de péptidos formilados bacterianos
    // Muerte sin desarmarse (`death = "immotile"`): el espermatozoide casi no tiene citoplasma que partir
    // en cuerpos apoptóticos; al morir deja de batir y queda entero e inmóvil, con la membrana dañada
    // (por eso se tiñe con eosina). Por omisión, la apoptosis de las células somáticas.
    bool immotileDeath = false;
};

struct SpeciesInfo {
    const char* key = "";  // id estable de los datos ("paramecium"): lo guardan las escenas
    const char* name = ""; // el que se muestra
    const char* description = ""; // qué es y qué hace (ficha y enciclopedia)
    const char* plan = "";        // plan corporal (bio/body_plan, body_plans/)
    LocomotionType locomotion = LocomotionType::Cilia;
    CellSpec spec;                  // forma, tamaño y color típicos
    float speedScale = 1.0f;        // respecto de la velocidad de su tipo de locomoción
    // Nado helicoidal (dinoflagelados: el flagelo transversal del cíngulo hace girar la célula mientras
    // el longitudinal la empuja; ciliados y euglenoides: giran sobre su eje largo al avanzar). Vista desde
    // arriba, la hélice es una oscilación lateral de este radio y período; 0 = el bamboleo genérico del
    // flagelo, o la línea recta de los cilios.
    float helixRadius = 0.0f;       // [µm]
    float helixPeriod = 0.0f;       // [s]
    // Deslizamiento flagelar (Peranema): el flagelo, rígido y hacia adelante, se apoya en el vidrio y
    // solo bate la punta; la célula avanza pegada al sustrato, sin nadar.
    bool flagellarGliding = false;
    // Flagelo posterior (coanoflagelados, espermatozoides): la célula avanza empujada desde atrás
    // ("pusher"): el par de fuerzas sobre el agua tiene el signo opuesto al de Chlamydomonas ("puller").
    bool flagellumPosterior = false;
    // Largo del flagelo [µm] cuando no es el de un flagelado típico (~1 largo corporal): la cola del
    // espermatozoide mide ~45 µm, unas 10 veces la cabeza. Lo usan el dibujo y el dipolo que deja en el
    // agua (el empuje se reparte a lo largo de la cola). 0: el de siempre.
    float flagellumLength = 0.0f;
    FeedingMode feeding = FeedingMode::None;
    SpeciesSet prey;                // especies que caza, fagocita o atrapa
    SpeciesSet flee;                // especies de las que huye (reacción de evitación)
    bool chemokinesis = false;      // nada más recto cuando la comida mejora (ciliados)
    bool acidTrap = false;          // se acumula en zonas con CO₂ (Paramecium, Jennings)
    bool phototaxis = false;        // se orienta hacia la luz (estigma)
    float photosynthesisScale = 1.0f; // tasa fotosintética relativa
    int fissions = 1;               // 1: fisión binaria; 2: palintomía en 4 hijas
    bool encysts = false;           // forma quistes cuando pasa hambre
    bool conjugates = false;        // conjugación con otra célula de la misma especie (ciliados)
    int initialCount = 0;           // individuos al empezar una gota
    // Sésiles: viven fijas al vidrio o a un detrito por un tallo (o pie).
    bool sessile = false;
    float stalkLength = 0.0f;       // [µm]
    bool contractileStalk = false;  // espasmonema (Vorticella): se retrae en ms al ser perturbada
    SpeciesSet mouthPrey;           // células chicas que el filtrador traga con su corriente oral
    bool walker = false;            // camina sobre cirros con paradas (hipotricos: Euplotes)
    float tentacleReach = 0.0f;     // suctorios: alcance de los tentáculos desde el centro [µm]
    float neckReach = 0.0f;         // Lacrymaria: largo máximo del cuello [µm]
    // Manipulación: horas entre presas enteras (respuesta funcional de tipo II; Didinium come
    // un paramecio cada ~3 h).
    float handlingHours = 0.0f;
    // Algas.
    bool chloroplasts = false;      // fotótrofa (gen de cloroplastos activo)
    bool glider = false;            // diatomeas: se deslizan por el rafe, con inversiones
    bool vitaminB12 = false;        // auxótrofa de cobalamina: la obtiene de las bacterias
    bool frustule = false;          // sílice: cada división achica a una hija (MacDonald–Pfitzer)
    bool filament = false;          // células unidas extremo con extremo (Spirogyra)
    int filamentCells = 0;          // células por filamento al empezar
    float fragmentationPerHour = 0.0f; // probabilidad de que una unión se corte [1/h]
    // Oedogonium: fracción de los ciclos en que el protoplasto sale como una zoóspora en vez de
    // dividirse (reproducción asexual).
    float zoosporeChance = 0.0f;
    int colonyDaughters = 0;        // colonias hijas que libera (Volvox, Pediastrum)
    int colonyCells = 0;            // células de una colonia de pocas células (Gonium: 16); solo el dibujo
    bool motherDiesAtRelease = false; // Volvox: las hijas nacen rompiendo a la madre
    // Materia seca relativa a una célula maciza del mismo volumen: Volvox es una esfera hueca
    // de matriz gelatinosa (~3% de células); Spirogyra tiene una gran vacuola central.
    float dryMassScale = 1.0f;
    // Semieje vertical [µm] (lo que no se ve desde arriba); 0: el semieje menor (un cigarro, como
    // Paramecium). Un disco (Cyclotella) o una célula extendida sobre el fondo son más delgados.
    float thickness = 0.0f;
    bool budding = false;           // levaduras: la hija brota de la madre (gemación)
    // Rango de pH tolerado (fuera de él el mantenimiento se encarece).
    float phMin = 6.0f, phMax = 9.5f;
    // Temperatura [°C] y salinidad máxima [PSU] toleradas; anaerobias estrictas (el O₂ las daña).
    float tMin = 5.0f, tMax = 32.0f;
    float salinityMax = 5.0f;
    bool anaerobe = false;
    bool mouthAtRear = false;       // Trichonympha fagocita la madera por el polo posterior
    // Ciclosis: corriente citoplasmática que hace girar gránulos y vacuolas [µm/s]
    // (Paramecium ~2–5; en las células vegetales, como Spirogyra, a lo largo de la pared).
    float cyclosis = 0.0f;
    SpeciesChemistry chemistry;     // reserva y pared (tinciones)
    // Genes que difieren de los de su locomoción (p. ej. sin tricocistos, con testa).
    std::vector<std::pair<Gene, float>> genome;
    std::vector<std::string> tags;  // grupos (taxonómicos o funcionales): dietas, hospedadores
    // Apariencia: estilo de dibujo con nombre (los implementa el render; "" = el de su locomoción)
    // y color de los plastos (por omisión, el verde de la clorofila).
    const char* style = "";
    bool plastidColorSet = false;
    std::uint8_t plastidColor[3] = {0, 0, 0};
    // Bandas de gránulos de pigmento a lo largo del cuerpo (stentorina de Stentor, blefarismina de
    // Blepharisma): cuántas se ven de un lado (0: sin bandas).
    int pigmentStripes = 0;
    // Giro de una célula aplanada sobre su eje largo al nadar (la cabeza del espermatozoide, una paleta:
    // de frente se ve ancha y de canto angosta) [Hz]; solo el dibujo. 0: no gira.
    float rollFrequency = 0.0f;
    AnimalCellInfo animalCell;
    std::vector<ProcessSpec> processes; // [[being.process]]
    std::vector<ResponseSpec> responses; // [[being.response]]
    JumpSpec jump;                      // [being.jump]
    ContractionSpec contraction;        // [being.contraction]
    ShapeWaveSpec shapeWave;            // [being.shape_wave]
    BioluminescenceSpec bioluminescence; // [being.bioluminescence]
    PhotosymbiontSpec photosymbiont;     // [being.photosymbiont]
    SignalToxicitySpec signalToxicity;   // [being.signal_toxicity]
    ToxicystSpec toxicysts;             // [being.toxicysts]
    ReleaseSpec release;                // [being.release]
    RelaySpec relay;                    // [being.relay]
    ChemotaxisSpec chemotaxis;          // [being.chemotaxis]
    SwimPathSpec swimPath;              // [being.swim_path]
    MotilitySpec motility;              // [being.motility]
    MorphSpec morph;                    // [being.morph]
    bool histophagous = false;
    // Se divide dentro de un quiste reproductivo (Colpoda): mientras dura el ciclo de divisiones no nada
    // ni come, encerrada en una pared fina, y salen 2^fissions hijas.
    bool dividesInCyst = false;
    // Lórica que se mineraliza con hierro del agua (Trachelomonas): pmol Fe por pmol de biomasa al
    // completarse, y el tiempo en que precipita (0: no tiene).
    float loricaIronRatio = 0.0f;
    float loricaHours = 24.0f;
    float perforationHours = 0.5f; // Vampyrella: en abrir el agujero en la pared [h biológicas]
    // Colonia deslizante (Bacillaria paxillifer): las hijas quedan lado a lado, unidas por el rafe, y cada
    // par se desliza a lo largo del eje con la misma oscilación: la colonia se estira y se cierra como una
    // regla de carpintero (Kapinga & Gordon 1992). Amplitud relativa al largo y período [s].
    int slidingColonyMax = 0;          // células por colonia (0: no forma)
    // Colonia en estrella (Asterionella): las células irradian de un centro, unidas por el polo
    // posterior con almohadillas de mucílago, a ángulos iguales. 0: no forma.
    int starColonyMax = 0;
    // División longitudinal (peritricos: el plano pasa por el eje, de la boca a la escópula; Lynn 2008)
    // en vez de la transversal de la mayoría de los ciliados.
    bool divisionLongitudinal = false;
    // Vacuola contráctil (osmorregulación de los protistas de agua dulce): se llena despacio con el agua
    // que entra por ósmosis y se vacía de golpe. `cvPeriod` [s] es el ciclo en agua dulce (~16 mOsm); el
    // agua que entra, y así la frecuencia, es proporcional a la diferencia entre la osmolaridad del
    // citosol (`cvCytosol`, mOsm) y la del medio. 0: sin ciclo (se dibuja quieta).
    // Desmidias: al dividirse cada hija conserva una semicélula y la otra brota del istmo y crece en
    // estas horas biológicas (0: las hijas nacen enteras).
    float semicellRegrowthHours = 0.0f;
    // Volteretas (Closterium): fija una punta con mucílago, se para sobre ella y cae del otro lado; cada
    // `somersaultInterval` [s] en promedio, en `somersaultSeconds` [s]. 0: no da volteretas.
    // Conjugación escalariforme (Spirogyra): con el nitrógeno limitando, dos filamentos de tipos de
    // apareamiento distintos que quedan lado a lado se emparejan célula a célula a menos de
    // `algalPairReach` [µm] y forman un tubo (`algalTubeHours`); el contenido de la célula donante pasa a la
    // receptora (`algalTransferHours`) y ahí queda una cigospora (`algalZygoteHours` hasta madurar). Horas
    // biológicas. 0 en `algalPairReach`: no se conjuga así.
    float algalPairReach = 0.0f;
    float algalTubeHours = 3.0f;
    float algalTransferHours = 1.0f;
    float algalZygoteHours = 2.0f;
    float somersaultInterval = 0.0f;
    float somersaultSeconds = 30.0f;
    float cvPeriod = 0.0f;
    float cvCytosol = 60.0f;
    // Tecameba (Arcella, Euglypha): el cuerpo blando es la testa, rígida; solo el citoplasma que sale por
    // la abertura forma seudópodos, lobosos (dedos) o filosos (finos). 0: no; 1: lobosos; 2: filosos.
    std::uint8_t testate = 0;
    float slidingAmplitude = 0.8f;     // fracción del largo
    float slidingPeriod = 80.0f;       // [s]          // come células dañadas (paralizadas, atrapadas, moribundas)
    StalkSpec stalk;                    // [being.stalk] (o las claves viejas sessile/stalk_length/spasmoneme)
    LightDamageSpec lightDamage;        // [being.light_damage]
    StressPigmentSpec stressPigment;    // [being.stress_pigment]
    // Contorno de la célula entera cuando tiene prolongaciones en el cuerpo (bio/silhouette): denso,
    // antihorario, centrado en su centroide y para el tamaño típico. Vacío: la superelipse de `spec`.
    std::vector<Vec2> outline;
    float outlineArea = 0.0f;       // área de ese contorno [µm²]
    Vec2 outlineBodyCenter;         // centro del cuerpo en el marco del contorno
    float outlineAreaFactor = 1.0f; // área del contorno / área de la elipse del cuerpo
    float outlineReach = 0.0f;      // distancia máxima del centro al contorno [µm] (punta del cuerno)
    // Ancho mínimo de Feret de la silueta (cuerpo y prolongaciones del cuerpo) al tamaño típico [µm]:
    // lo que tiene que abrir una boca para tragarla entrando de punta (M5).
    float minWidth = 0.0f;
    // Apertura máxima de la boca (citostoma dilatado) al tamaño típico [µm]; 0 = sin límite (las amebas
    // envuelven, no tragan; o no hay dato).
    float mouthWidth = 0.0f;
};
// Estilos de dibujo que el render sabe hacer (para validar los datos).
const std::vector<std::string>& drawingStyles();

const SpeciesInfo& speciesInfo(SpeciesId id);
SpeciesId playerSpecies(LocomotionType type);
Genome speciesGenome(SpeciesId id);
// Masa de la célula relativa a la de una esfera de la misma sección vista desde arriba
// (`cellMassPmol`, que es lo que mide el cuerpo blando 2D): el volumen real es el del esferoide de
// semiejes mayor, menor y vertical, por la materia seca relativa. Una célula alargada pesa mucho
// menos que la esfera de su silueta (un paramecio, ~0,5).
float cellMassScale(const SpeciesInfo& info);
// Radio que ocupa la célula del tamaño típico, con sus cuernos o brazos [µm] (para ubicarla en la placa).
inline float speciesReach(const SpeciesInfo& info) {
    return std::max({info.spec.radiusX, info.spec.radiusY, info.outlineReach});
}
// Por id de los datos ("paramecium"); vacío si no está cargado.
std::optional<SpeciesId> findSpecies(std::string_view key);
// Los tipos de ser con una etiqueta (vacío si nadie la tiene).
SpeciesSet speciesWithTag(std::string_view tag);
// Apertura bucal (M5): la presa entra si su ancho mínimo (silueta con cuernos o brazos), a su escala
// lineal, no pasa la boca del depredador a la suya. Una función para la placa y el cultivo.
inline bool fitsMouth(const SpeciesInfo& predator, float predatorScale, const SpeciesInfo& prey, float preyScale) {
    return predator.mouthWidth <= 0.0f || prey.minWidth * preyScale <= predator.mouthWidth * predatorScale;
}

bool hasTag(SpeciesId id, std::string_view tag);

// Carga los [[being]] de un archivo TOML de un paquete (se agregan al registro, después de los que
// ya hay). Las referencias (presas, huidas) se resuelven al final de cada carga, contra todo lo
// cargado. Devuelve false y deja el motivo en `error` si el archivo no se puede leer; los seres
// con errores se saltean y quedan en `speciesLoadWarnings`. El registro carga solo el paquete de
// fábrica la primera vez que se lo usa.
bool loadSpeciesFile(const std::string& path, const std::string& pack, std::string* error = nullptr);
// Lo mismo desde el texto (paquetes .biopack).
bool loadSpeciesText(const std::string& text, const std::string& source, const std::string& pack,
                     std::string* error = nullptr);
const std::vector<std::string>& speciesLoadWarnings();
// Mientras viva, las cargas no resuelven las referencias: se resuelven todas juntas al destruirse.
// Un paquete en carpeta trae un ser por archivo y uno nombra presas de archivos que todavía no se
// leyeron (si no, avisaría referencias rotas que no lo son).
struct SpeciesLoadBatch {
    SpeciesLoadBatch();
    ~SpeciesLoadBatch();
    SpeciesLoadBatch(const SpeciesLoadBatch&) = delete;
    SpeciesLoadBatch& operator=(const SpeciesLoadBatch&) = delete;
};
// El [[being]] como se leyó, en TOML (para exportarlo a un paquete).
const std::string& speciesSource(SpeciesId id);
// Paquete de donde vino cada ser ("base" para los de fábrica).
const std::string& speciesPack(SpeciesId id);

// Componente: especie de la célula.
struct Species {
    SpeciesId id;
};

// Componente: quiste (pared resistente, metabolismo mínimo, no se mueve ni come).
struct Cyst {
    float age = 0.0f; // [s]
};

// Componente: célula sésil fija al vidrio, por un tallo (`rod`, una entidad con Stalk) o por el pie
// (sin tallo: el polo posterior sobre `holdfast`).
struct Anchored {
    Vec2 holdfast;
    float stalk = 0.0f;      // largo actual del tallo [µm] (copia para la ficha)
    float contracted = 0.0f; // 1 = recién contraído (espasmonema); vuelve a 0 en segundos (copia)
    entt::entity rod = entt::null;
};

// Componente de una entidad tallo (no es una célula: no tiene cuerpo blando). Sigue ahí cuando el
// zooide se va (telotroco) o muere, hasta degradarse.
struct Stalk {
    StalkRod rod;
    SpeciesId species{};
    entt::entity zooid = entt::null; // null: tallo vacío
    float emptyHours = 0.0f;         // [h biológicas] sin zooide
    std::uint32_t id = 0;            // estable (dibujo de la hélice)
    // Colonia: la rama sale de la punta del tallo `parent` (que la sostiene y la mueve); `root` es el
    // tallo de la colonia que está en el vidrio.
    entt::entity parent = entt::null, root = entt::null;
    int depth = 0;
    int children = 0;                // ramas vivas que salen de este (no se degrada mientras tenga)
    float turn = 0.0f;               // ángulo de la rama respecto del último tramo del padre [rad]
};

// Componente: hija nadadora de una especie sésil (telotroco), antes de fijarse.
// Componente: miembro de una colonia en estrella. `root` es la célula fundadora (también miembro, con
// `slot` 0); cada miembro apunta hacia afuera a `slot` · 360° / máximo de la de la fundadora.
struct StarMember {
    entt::entity root;
    int slot;
};

// Componente: semicélula nueva de una desmidia que crece después de la división. `full` es la forma de
// reposo completa; la mitad del lado `newSign` (+1: x > 0) es la nueva.
struct SemicellGrowth {
    float progress = 0.0f; // 0–1
    float newSign = 1.0f;
    float isthmus = 1.0f;  // medio ancho del istmo [µm, en la escala de `full`]
    float areaScale = 1.0f;
    std::vector<Vec2> full;
};

// Componente: voltereta en curso (o la espera hasta la próxima). `pivot` y `other` son índices del
// anillo de las puntas: la fija y la que se levanta.
struct Somersault {
    float wait = -1.0f;   // [s] hasta la próxima (negativo: sin sortear)
    float t = -1.0f;      // 0–1 durante la voltereta (negativo: no hay)
    std::size_t pivot = 0, other = 0;
    bool frontNext = false; // qué punta fija la próxima vez (alternan)
    Vec2 hold, axis;      // punto de apoyo y eje de la célula al empezar, en el mundo
    float areaScale = 1.0f;
    std::vector<Vec2> base;
};

// Componentes de la conjugación escalariforme. `AlgalPair`: emparejada con `partner`; `progress` 0–1
// (tubo, pasaje del contenido, cigospora). `Zygospore`: la receptora, latente (como un quiste) dentro de
// su pared. `EmptyWall`: la donante, una pared vacía que queda en el filamento.
struct AlgalPair {
    entt::entity partner;
    bool donor = false;
    float progress = 0.0f;
};
struct Zygospore {
    float maturity = 0.0f; // 0–1 (se oscurece al madurar)
};
struct EmptyWall {};

// Componente: fase del ciclo de la vacuola contráctil (0–1: diástole hasta 0,9, sístole después).
struct ContractileCycle {
    float phase = 0.0f;
};

struct Telotroch {
    float timer; // [s] hasta fijarse
};

// Componente: zoóspora de alga filamentosa (Oedogonium): nada con su corona de flagelos y,
// cuando se le acaba el tiempo, se fija y crece como un filamento nuevo.
struct Zoospore {
    float hoursLeft; // [h biológicas] hasta fijarse
};

// Componente: presa sujeta por un depredador: los tentáculos de un suctorio (le succiona el
// citoplasma), los axopodios de un heliozoo (la arrastran hasta el cuerpo, que la traga entera) o una
// Vampyrella que le perfora la pared (después de `delayHours`, le vacía el protoplasto).
struct Captured {
    entt::entity by;
    float delayHours = 0.0f; // [h biológicas] hasta empezar a drenarla (la perforación de la pared)
    bool drain = true;       // false: no la drena, la trae para tragarla (axopodios)
};

// Componente: cuello extensible de Lacrymaria.
// Componente: estado del relevo de la señal (Dictyostelium).
struct Relay {
    float starvingHours = 0.0f;
    float refractoryHours = 0.0f;
    float pacemakerHours = 0.0f; // marcapasos: horas hasta el próximo disparo
    bool pacemaker = false;
    bool competent = false;
};

// Componente: destello en curso (`glow`, 0–1) y carga disponible para el próximo.
struct Luminescence {
    float glow = 0.0f;
    float charge = 1.0f;
};

// Componente: estado de la onda de forma (ShapeWaveSpec). `base` es la forma de reposo sin deformar.
struct ShapeWave {
    float time = 0.0f; // [s] desde que empezó el episodio (o desde siempre, el vaivén)
    float left = 0.0f; // [s] que le quedan al episodio de metabolia (0: ninguno)
    std::vector<Vec2> base;
};

// Componente: estado de la contracción del cuerpo (ContractionSpec). 1 = contraída, 0 = extendida.
struct Contraction {
    float state = 1.0f;
    float stretch = 1.0f; // largo de la forma de reposo actual / el de la contraída
    float areaScale = 1.0f; // área de la silueta actual / la de la contraída (el blanco de área)
    float swim = 0.0f;      // cuánto tomó la forma de nado (0–1)
    // Formas de reposo contraída y extendida, punto a punto (la misma membrana: no "rueda"). Se rehacen
    // cuando el anillo cambia de tamaño (división).
    std::vector<Vec2> contracted, extended, swimming;
};

// Componente: paralizada por toxicistos (M6): no nada, no huye ni dispara tricocistos.
struct Paralyzed {
    float hours;       // [h biológicas] que le quedan
    bool lethal = true; // al terminar, muere
};

// Componente: cuánto avanzó el morfo inducido (0: forma de la especie, 1: el morfo) y qué estiramiento
// de la forma de reposo ya está aplicado.
struct Morph {
    float progress = 0.0f;
    float sx = 1.0f, sy = 1.0f;
};

struct Neck {
    float length = 0.0f;   // [µm]
    Vec2 direction{1.0f, 0.0f};
    entt::entity target = entt::null;
    // El cuello es parte de la célula: un tramo de `kNeckPoints` puntos de membrana alrededor del polo
    // anterior (desde el índice `first` del anillo) se despliega en un tubo. `base` es la forma de reposo
    // sin cuello (vacía hasta que se arma).
    std::vector<Vec2> base;
    std::size_t first = 0;
    float areaScale = 1.0f;
};
inline constexpr std::size_t kNeckPoints = 16;
