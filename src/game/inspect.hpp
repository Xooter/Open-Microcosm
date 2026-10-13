#pragma once

#include "bio/species.hpp"
#include "core/vec2.hpp"

#include <entt/entity/entity.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class World;

// Inspección (modo Explorar): click sobre cualquier cosa de la gota y se abre su ficha. Lo que
// tiene identidad estable (células, granos, micelios) se sigue en vivo; las partículas
// (bacterias, minerales, cadenas de cianobacterias) se reordenan en memoria, así que su ficha es
// una foto del momento del click.

// Lo que hay bajo el cursor.
struct Picked {
    enum class Kind : std::uint8_t {
        None,
        Cell,          // célula u organismo de cuerpo blando (también un huevo)
        Rock,          // grano de arena
        Cyanobacteria, // una cadena (index = filamento, node = célula de la cadena)
        Bacterium,
        Bdellovibrio,
        Zoospore,      // zoospora de hongo
        Hypha,         // micelio (id estable, node = nodo)
        Plasmodium,    // plasmodio de Physarum (id estable, node = nodo)
        OatFlake,      // copo de avena (index)
        Resting,       // esporangio de resistencia de Allomyces (index)
        Sclerotium,    // esclerocio de Physarum (index)
        Particle,      // partícula ambiental: tinta, detrito o mineral
    };
    Kind kind = Kind::None;
    entt::entity entity = entt::null; // Cell, Rock
    std::uint32_t id = 0;             // Hypha: id del micelio
    std::size_t index = 0;            // posición en el arreglo de partículas (foto del click)
    std::size_t node = 0;
    Vec2 at;                          // dónde estaba al elegirla [µm]

    explicit operator bool() const { return kind != Kind::None; }
    // ¿Se puede seguir en vivo (identidad estable) o solo es una foto?
    bool live() const { return kind == Kind::Cell || kind == Kind::Rock || kind == Kind::Hypha || kind == Kind::Plasmodium; }
};

struct InspectLine {
    std::string label, value; // label vacío: el valor ocupa todo el ancho
};

struct InspectSection {
    std::string title;
    std::vector<InspectLine> lines;
};

// La ficha como datos (sin raylib): la dibuja `drawInspector` y la prueban los tests. Los
// textos son ASCII porque la UI usa la fuente por defecto de raylib.
struct Inspection {
    bool valid = false;
    bool live = false;       // se vuelve a armar mientras se mira
    bool dead = false;       // el ser ya no existe: la ficha cuenta cómo murió
    bool hasGenome = false;  // se puede dibujar su genoma
    std::string title;       // nombre
    std::string subtitle;    // tipo y origen
    std::string blurb;       // qué es (texto de la enciclopedia)
    std::string headline;    // una línea: lo que está haciendo (tooltip)
    std::vector<InspectSection> sections;
    Vec2 at;                 // posición [µm]
    float radius = 10.0f;    // radio del anillo de resalte [µm]
    entt::entity entity = entt::null;
    entt::entity successor = entt::null; // si murió: quién ocupó su lugar
};

// La ficha de lo elegido (en el estado actual del mundo).
Inspection buildInspection(const World& world, const Picked& picked);
