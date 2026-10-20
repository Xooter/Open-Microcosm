#pragma once

#include "core/vec2.hpp"

#include <cstdint>
#include <vector>

// Pie y tallo de los seres fijos (mecanismo M9, docs/plan_mecanicas.md). Sin raylib ni EnTT: la
// física del tallo como varilla; el mundo (game/world_stalk.cpp) le da el agua y el zooide.
//
// Tipos reales:
//  - espasmonema (Vorticella): vaina elástica con un haz contráctil de espasmina en hélice; se dobla
//    con el agua y se enrolla en ms (Ca²⁺, sin ATP: Amos 1971; Moriyama et al. 1998; Upadhyaya et al.
//    2008), se re-extiende en segundos;
//  - rígido (Epistylis, suctorios): tubo secretado, recto, que casi no se dobla ni se contrae;
//  - mucilaginoso (Gomphonema): polisacárido hialino, ancho y flexible, que se mece con la corriente;
//  - pie (Stentor): sin tallo; el polo posterior se adhiere al vidrio y puede soltarse.
enum class StalkKind : std::uint8_t { Spasmoneme, Rigid, Mucilage, Foot };

struct StalkSpec {
    StalkKind kind = StalkKind::Rigid;
    float length = 0.0f;            // [µm] adulto
    float width = 2.0f;             // [µm]
    float bendingRate = 10.0f;      // [1/s] con que vuelve a estar recto (rigidez frente al arrastre)
    float growthHours = 6.0f;       // [h biológicas] en secretarse entero después de fijarse
    float decayHours = 48.0f;       // [h biológicas] que dura un tallo vacío
    float contractedFraction = 1.0f; // largo contraído / extendido (solo el espasmonema)
    float extensionSeconds = 2.0f;  // [s] en re-extenderse
    float spontaneousPerMinute = 0.0f; // contracciones espontáneas
    bool detaches = false;          // con hambre el zooide se suelta (telotroco) y deja el tallo
};

// Varilla: nodos desde el disco adhesivo (0, fijo) hasta la punta (la última, unida al zooide).
struct StalkRod {
    std::vector<Vec2> nodes;
    Vec2 baseDirection{1.0f, 0.0f}; // dirección con que salió del disco (empotrado)
    float length = 0.0f;            // largo de reposo actual (crece después de fijarse) [µm]
    float contracted = 0.0f;        // 1 = recién contraído; vuelve a 0 al re-extenderse
};

// Nodos de un tallo de largo adulto `adultLength` (uno cada ~5 µm, al menos 3).
int stalkNodeCount(float adultLength);

// Tallo recto desde `holdfast` en la dirección `direction`.
void layStalk(StalkRod& rod, Vec2 holdfast, Vec2 direction, float length, int nodes);

// Largo de reposo efectivo (con la contracción del espasmonema).
float stalkRestLength(const StalkRod& rod, const StalkSpec& spec);

// Un paso: el agua arrastra cada nodo (`flow`, uno por nodo, [µm/s]; sin inercia), la flexión lo
// endereza hacia la dirección del disco, y la longitud se impone desde el disco hacia la punta
// (seguir al anterior: la varilla es inextensible). `tip`, si no es null, es el polo del zooide: la
// punta lo sigue (el zooide tira del tallo) antes de imponer el largo.
void stepStalk(StalkRod& rod, const StalkSpec& spec, const std::vector<Vec2>& flow, const Vec2* tip, float dt);
