#version 430

// Fase 14: XPBD en GPU. Mismo algoritmo que sim/soft_body.cpp y sim/contacts.cpp, con dos
// diferencias de paralelismo: las aristas se resuelven por colores (pares/impares) y los
// pasos locales (flexión, tensión cortical, área) son Jacobi (se calculan y después se aplican).

struct Body {
    uint ringOff, ringCount, intOff, intCount;
    uint orgCount, lod, flags, linkOff;   // flags: 1 = aristas solo a tensión
    float restArea, shapeRate, edgeScale, bendScale;
    float areaScale, cortical;
    uint linkCount, pad0;
};

// Extremo de una unión que pertenece a un cuerpo (cada cuerpo corrige el suyo).
struct LinkRef {
    uint link;
    uint end; // 0 = a, 1 = b
};

struct Member {
    uint slot, next, owner, ringPos;
    uint ringSize, group, flags, body;    // flags: 1 = contacto consigo misma, 2 = puede chocar
};

struct Link {
    uint a, b;
    float rest, pad;
};

struct RingPoint {
    vec2 rest;   // forma de reposo (marco del cuerpo)
    float edge;  // largo de reposo de la arista hacia el siguiente
    uint slot;   // partícula
};

struct InteriorPoint {
    vec2 rest;
    float anchor; // anclaje al citoesqueleto [1/s]
    uint slot;
};

layout(std430, binding = 0) buffer PosBuf { vec4 pos[]; };              // x, y, radio, 1/masa
layout(std430, binding = 1) readonly buffer VelBuf { vec2 vel[]; };
layout(std430, binding = 2) readonly buffer BodyBuf { Body bodies[]; };
layout(std430, binding = 3) readonly buffer RingBuf { RingPoint ringPts[]; };
layout(std430, binding = 4) readonly buffer InteriorBuf { InteriorPoint intPts[]; };
layout(std430, binding = 5) buffer ScratchBuf { vec2 scratch[]; };      // uno por punto de anillo
layout(std430, binding = 6) readonly buffer MemberBuf { Member members[]; };
layout(std430, binding = 7) buffer CellCountBuf { uint cellCount[]; };  // 2 × celdas (doble buffer)
layout(std430, binding = 8) buffer CellSlotBuf { uint cellSlot[]; };    // celdas × kCellCapacity
layout(std430, binding = 9) buffer MemberCellBuf { uint memberCell[]; };
layout(std430, binding = 10) buffer ContactBuf { vec4 contact[]; };     // xy: corrección; z: puede chocar
layout(std430, binding = 11) readonly buffer LinkBuf { Link links[]; };
layout(std430, binding = 12) readonly buffer LinkRefBuf { LinkRef linkRefs[]; };

// Cubetas de capacidad fija: sin suma prefija (los puntos de más en una celda se ignoran).
const uint kCellCapacity = 48u;

uint ringSlot(uint i) { return ringPts[i].slot; }

uniform uint uSub;          // sub-paso actual
uniform uint uSubsteps;
uniform uint uStride;       // LOD: los simplificados se resuelven 1 de cada uStride
uniform float uH;           // paso del sub-paso [s]
uniform float uHIter;       // paso de cada iteración [s]
uniform float uDt;          // paso completo [s]
uniform vec2 uWorld;        // ancho y alto del mundo toroidal [µm]
uniform float uEdgeRate, uBendRate, uAreaRate, uShapeScale;
uniform uint uNumBodies, uNumMembers, uNumLinks, uNumCells;
uniform uint uGridW, uGridH;
uniform vec2 uCell;         // tamaño de celda de contactos [µm]
uniform float uLinkRate;
uniform uint uGrab;         // slot agarrado (0xffffffff = ninguno)
uniform vec2 uGrabTarget;
uniform float uSlack;       // holgura de acercamiento en un sub-paso [µm]
uniform uint uParity;       // mitad del doble buffer de la grilla que usa este sub-paso
uniform uint uIterBegin, uIterCount; // iteraciones que hace este despacho
uniform uint uPredict;      // 1: el despacho empieza con la predicción
uniform uint uApply;        // 1: aplica antes las correcciones de contacto pendientes
uniform vec3 uDish;         // placa: centro (xy) y radio (z) [µm]; radio 0 = sin placa

float relaxRate(float rate, float h) { return 1.0 - exp(-rate * h); }

float minImage(float d, float size) { return d - size * round(d / size); }
float wrapCoord(float x, float size) { return x - size * floor(x / size); }

bool farTurn() { return ((uSub + 1u) % uStride) == 0u; }

// Se resuelve en este sub-paso: detalle siempre; simplificado en su turno; estadístico nunca.
bool solving(uint lod) { return lod == 2u || (lod == 1u && farTurn()); }

float iterStep(uint lod) { return lod == 2u ? uHIter : uHIter * float(uStride); }

float cross2(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }
vec2 rotate2(vec2 v, float c, float s) { return vec2(c * v.x - s * v.y, s * v.x + c * v.y); }
