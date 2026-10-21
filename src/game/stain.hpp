#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Tinciones de laboratorio como datos. Una tinción es una lista de reglas "este blanco
// bioquímico toma este color"; el render pregunta `evaluateStain` por cada blanco que dibuja
// (núcleo, gránulos de almidón, vacuolas ácidas, pared bacteriana…). Es solo visual: no cambia la
// simulación (en la realidad, Gram, Lugol y DAPI fijan o matan a las células: `vital` lo marca).
// Sin dependencias de raylib: lo usan el render, el editor y los tests.

struct Rgba {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};
inline Rgba rgb8(int r, int g, int b, float a = 1.0f) {
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f, static_cast<float>(b) / 255.0f, a};
}

// Qué estructura o compuesto se tiñe.
enum class StainTarget : std::uint8_t {
    Cytoplasm,    // citoplasma y proteínas en general (células eucariotas)
    Nucleus,      // ADN del núcleo
    Starch,       // almidón verdadero (clorofitas)
    Glycogen,     // glucógeno
    Paramylon,    // paramilón (Euglena)
    Acidic,       // compartimentos ácidos: vacuolas digestivas y lisosomas (con su pH)
    GramPositive, // pared gruesa de peptidoglicano (Bacillus, levaduras)
    GramNegative, // pared fina y membrana externa (bacilos gramnegativos)
    Chlorophyll,  // clorofila (autofluorescencia)
    Nucleoid,     // ADN bacteriano
    Bacteria,     // toda la bacteria (colorantes que no distinguen)
    Hemoglobin,   // hemoglobina de los glóbulos rojos (acidófila: la tiñe la eosina)
    // Célula muerta con la membrana dañada: los colorantes de exclusión (eosina, azul tripano) no entran
    // a una célula viva, que los deja afuera, y sí a una muerta (o en apoptosis tardía).
    DamagedMembrane,
    Count
};
const char* stainTargetName(StainTarget t);

// Cómo depende el color de la situación.
enum class StainResponse : std::uint8_t {
    Fixed,         // siempre `color`
    PhIndicator,   // `color` en medio ácido, `color2` en alcalino; vira alrededor de `pivot`
    GramRetention, // `color` mientras retiene el colorante primario; `color2` (contratinción) al decolorar
};
const char* stainResponseName(StainResponse r);

enum class StainMode : std::uint8_t {
    BrightField, // el colorante absorbe luz (se ve sobre el campo claro)
    Fluorescence // fondo oscuro: solo emiten los blancos teñidos
};

struct StainRule {
    StainTarget target = StainTarget::Nucleus;
    StainResponse response = StainResponse::Fixed;
    Rgba color = {0.2f, 0.3f, 0.7f, 1.0f};
    Rgba color2 = {0.9f, 0.4f, 0.5f, 1.0f};
    float strength = 1.0f; // opacidad del colorante sobre el blanco (0 a 1)
    float pivot = 7.0f;    // PhIndicator: pH del viraje
    float width = 0.6f;    // PhIndicator: ancho del viraje [unidades de pH]
    float tau = 10.0f;     // GramRetention: constante de pérdida del colorante [s de decolorante]
};

struct StainSpec {
    std::string id;   // estable, sin espacios (nombre de archivo)
    std::string name;
    std::string note; // qué hace en la realidad
    StainMode mode = StainMode::BrightField;
    bool vital = true;      // false: en la realidad fija o mata a las células
    float dose = 1.0f;      // concentración relativa (multiplica la intensidad de las reglas)
    float time = 10.0f;     // segundos de decolorante (Gram) o de exposición
    Rgba background = {0.0f, 0.0f, 0.0f, 1.0f};
    float backgroundStrength = 0.0f; // brightfield: tiñe el fondo; fluorescencia: oscuridad del fondo
    std::vector<StainRule> rules;
};

// Resultado para un blanco: `alpha` es cuánto del color del colorante cubre al color natural
// (0: sin efecto). En fluorescencia es el brillo.
struct StainResult {
    Rgba color;
    float alpha = 0.0f;
};

// `amount`: 0 a 1 (cuánto del blanco hay: saturación de almidón, etc.); `ph`: solo para Acidic.
StainResult evaluateStain(const StainSpec& spec, StainTarget target, float amount = 1.0f, float ph = 7.0f);
bool stainHasTarget(const StainSpec& spec, StainTarget target);
// `base` con el colorante encima.
Rgba stainOver(Rgba base, const StainResult& dye);
// Color del fondo del campo para `natural`.
Rgba stainBackground(const StainSpec& spec, Rgba natural);

// Las tinciones de fábrica: ninguna, Gram, Lugol, DAPI, rojo neutro, azul de metileno, rojo Congo,
// Giemsa (sangre), orceína acética (cromosomas), eosina–nigrosina (vitalidad de los espermatozoides) y
// azul tripano (viabilidad de un cultivo).
const std::vector<StainSpec>& builtinStains();

// Texto clave=valor versionado (`stain_version=1`).
std::string serializeStain(const StainSpec& spec);
bool parseStain(const std::string& text, StainSpec& spec);
// Carpeta de tinciones del usuario: <datos>/openmicrocosm/stains.
std::string defaultStainsDir();
bool saveStain(const StainSpec& spec, const std::string& path);
bool loadStain(const std::string& path, StainSpec& spec);
std::vector<StainSpec> loadUserStains(const std::string& dir);
