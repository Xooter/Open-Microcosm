#pragma once

#include "core/vec2.hpp"
#include "sim/body_particles.hpp"
#include "sim/dish.hpp"

#include <cstdint>
#include <vector>

// Física sin inercia (bajo Reynolds): cada restricción relaja su error con una tasa [1/s],
// corrección = 1 − exp(−tasa · h). Es independiente del sub-paso h y da un comportamiento
// viscoelástico: la célula se deforma y vuelve a su forma sin rebotar.
struct SoftBodyTuning {
    int substeps = 10;
    float edgeRate = 3000.0f; // largo de la membrana (casi inextensible)
    float bendRate = 60.0f;   // flexión local (shape matching por ventanas)
    float areaRate = 3000.0f; // área = volumen de agua (incompresible a corto plazo)
    float shapeScale = 1.0f;  // multiplica la memoria de forma propia de cada cuerpo
    int iterations = 3; // restricciones por sub-paso
    // Pasadas de contactos por sub-paso (las últimas iteraciones). Con 1, las restricciones
    // internas acomodan la forma y el contacto tiene la última palabra en cada sub-paso.
    int contactPasses = 2;
};

// Cuerpo blando: anillo de membrana + partículas interiores confinadas.
struct RingBody {
    std::uint32_t id = 0; // dueño de las partículas (para colisiones)

    std::vector<std::uint32_t> ring; // slots de membrana, en orden
    std::vector<Vec2> restRing;      // forma de reposo centrada, en el marco del cuerpo
    std::vector<float> restEdge;     // |r[i+1] − r[i]|
    float restArea = 0.0f;           // con signo (indica la orientación del anillo)
    Vec2 lastFinite;                 // último centro finito (para rearmar un cuerpo roto)
    float shapeRate = 0.0f;          // memoria de forma [1/s]: película rígida alta, ameba ~0
    // Membrana plástica (ameba): sin forma propia; la forma de reposo es un círculo cuyo
    // perímetro sigue al real con esta tasa [1/s] (reservorio de membrana). 0 = no plástica.
    float membraneReservoirRate = 0.0f;
    float edgeScale = 1.0f; // multiplica la rigidez de aristas (membrana fluida < 1)
    float bendScale = 1.0f; // multiplica la rigidez de flexión
    float areaScale = 1.0f; // multiplica la conservación de área (citoplasma que fluye < 1)
    // Tensión cortical [1/s]: cada punto tiende al punto medio de sus vecinos (flujo de
    // curvatura). Alisa arrugas de escala chica sin frenar lóbulos grandes.
    float corticalTension = 0.0f;
    float baseCorticalTension = 0.0f; // la tensión sube cuando se agota el reservorio de membrana
    bool edgeTensionOnly = false; // membrana fluida: las aristas solo resisten estirarse

    // Interior: los primeros `organelleCount` son organelos (chocan y empujan la membrana);
    // el resto son gránulos que solo se confinan.
    std::vector<std::uint32_t> interior;
    std::vector<Vec2> restInterior;
    std::vector<float> anchorRate; // anclaje al citoesqueleto [1/s], 0 = libre
    std::uint32_t organelleCount = 0;

    // Estado derivado, actualizado por soft_body::refresh.
    std::vector<Vec2> poly;
    Vec2 centroid;
    float angle = 0.0f;
    Vec2 aabbMin, aabbMax;
};

namespace soft_body {

void predict(RingBody& body, BodyParticles& bp, float h);
void solve(RingBody& body, BodyParticles& bp, const SoftBodyTuning& tuning, float h);
void confineGranules(RingBody& body, BodyParticles& bp);
void refresh(RingBody& body, const BodyParticles& bp);
// Red de seguridad: si alguna partícula del cuerpo dejó de ser finita, lo rearma en su forma de
// reposo en el último centroide y ángulo válidos (los de la última `refresh`), sin velocidad.
// Devuelve true si tuvo que hacerlo (el mundo lo cuenta: no debería pasar nunca).
bool recoverNonFinite(RingBody& body, BodyParticles& bp);
// Traslada el cuerpo entero si su centroide salió del mundo toroidal.
void wrap(RingBody& body, BodyParticles& bp, float width, float height);
// Pared de la placa (vidrio, no cede): cada punto de membrana que la cruza vuelve adentro.
void confineToDish(RingBody& body, BodyParticles& bp, const Dish& dish);
// Igual, pero trasladando el cuerpo entero (sin deformarlo): cuerpos estadísticos del LOD.
void pushIntoDish(RingBody& body, BodyParticles& bp, const Dish& dish);

float area(const RingBody& body, const BodyParticles& bp);
float perimeter(const RingBody& body, const BodyParticles& bp);

// Organelos (colisionan y empujan la membrana): se insertan después de los existentes.
void addOrganelle(RingBody& body, std::uint32_t slot, Vec2 restLocal, float anchorRate);
// Quita el elemento `index` de `interior` (no libera el slot).
void removeInterior(RingBody& body, std::size_t index);
// Posición actual expresada en el marco de reposo del cuerpo.
Vec2 toLocal(const RingBody& body, Vec2 worldPos);
// Escala la forma de reposo a un área nueva (crecimiento, ingestión).
void setRestArea(RingBody& body, float newSignedArea);
// Escala el cuerpo entero (membrana e interior) alrededor del centroide hasta el área de reposo.
void scaleToRestArea(RingBody& body, BodyParticles& bp);
// Igual, pero estirando solo el eje x local (células de filamento: crecen a lo largo).
void setRestAreaAxial(RingBody& body, float newSignedArea);
// Membrana plástica: forma de reposo circular con el perímetro actual.
void makePlastic(RingBody& body, BodyParticles& bp, float reservoirRate);
void adaptPlastic(RingBody& body, const BodyParticles& bp, float dt);
// Reemplaza el anillo (fusión/fisión de membrana). La geometría actual pasa a ser la de reposo.
void replaceRing(RingBody& body, const BodyParticles& bp, std::vector<std::uint32_t> ring);
// Igual, pero conserva el área objetivo (inserción de membrana sin cambiar el volumen).
// Red de seguridad: si la membrana se cruzó consigo misma (nada la desenreda sola, no hay
// colisión entre partes de un mismo anillo), la desenreda con movimientos 2-opt (sin mover
// puntos). Devuelve si hizo algo.
bool untangle(RingBody& body, BodyParticles& bp);

void resizeRing(RingBody& body, const BodyParticles& bp, std::vector<std::uint32_t> ring);
// Re-muestrea el anillo con `count` puntos equiespaciados sobre el contorno actual (reusa los
// slots existentes, asigna o libera la diferencia). Después hay que rehacer la forma de reposo.
void resampleRing(RingBody& body, BodyParticles& bp, std::size_t count);
// Forma de reposo elíptica (semiejes rx, ry) alineada al eje principal actual del anillo.
// Para las hijas de una división: cada punto toma el radio de la elipse en su dirección.
// `squareness`: exponente de superelipse (2 = elipse, mayor = rectángulo redondeado).
void reshapeRest(RingBody& body, const BodyParticles& bp, float rx, float ry, float squareness = 2.0f,
                 float taper = 0.0f);
// Igual, pero los puntos se reparten parejos por longitud de arco sobre la forma meta, en el orden
// del anillo (el primero queda en la dirección que tiene ahora). Para cambiar mucho de forma (una
// célula redonda que se extiende en un huso): la proyección radial de reshapeRest amontona los
// puntos en los lados largos y la forma de reposo se cruza consigo misma.
// `pinMinorAxis`: corre el reparto para que dos puntos caigan justo en los extremos del eje menor
// (con un número par de puntos), donde una división estrangula la cintura.
void reshapeRestEven(RingBody& body, const BodyParticles& bp, float rx, float ry, float squareness = 2.0f,
                     float taper = 0.0f, bool pinMinorAxis = false);
// Forma de reposo de un contorno cualquiera (la silueta de una especie con cuernos o brazos), escalado
// por `scale`: se orienta como mejor calza con la forma actual del anillo (un contorno sin simetría
// no tiene eje principal confiable) y los puntos se reparten parejos por longitud de arco.
void reshapeRestToOutline(RingBody& body, const BodyParticles& bp, const std::vector<Vec2>& outline, float scale);
// Membrana plástica: inserta partículas donde la membrana se estiró y quita donde se juntó
// (la célula real agrega membrana desde reservorios internos). Devuelve true si cambió.
bool remesh(RingBody& body, BodyParticles& bp, float spacing);
// Perímetro actual / perímetro de un círculo de igual área (1 = redonda).
float membraneExcess(const RingBody& body, const BodyParticles& bp);
// Reservorio de membrana limitado (~2–3× el área aparente): al agotarse, la tensión cortical
// sube bruscamente y frena cualquier estiramiento adicional.
void updateMembraneTension(RingBody& body, const BodyParticles& bp);
// Distancia RMS de la membrana a su forma de reposo alineada [µm].
float shapeError(const RingBody& body, const BodyParticles& bp);

} // namespace soft_body
