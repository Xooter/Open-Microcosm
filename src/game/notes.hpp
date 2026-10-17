#pragma once

#include "core/vec2.hpp"

#include <cstdint>
#include <string>
#include <vector>

class World;

// Notas del usuario (fase P1, paso 14): observaciones ancladas a un ser, a un lugar o a un instante
// de la placa. Van en la escena (líneas `nota=`) y en la exportación, pero NO son intervenciones: no
// cambian el mundo, así que la corrida es la misma con o sin ellas. Al repetir la escena, cada nota
// aparece cuando la placa llega a su paso.
enum class NoteAnchor : std::uint8_t { Instant, Place, Being };

struct Note {
    std::uint64_t step = 0;  // paso de la placa al escribirla
    double hours = 0.0;      // tiempo biológico [h]
    NoteAnchor anchor = NoteAnchor::Instant;
    Vec2 pos;                // el lugar, o dónde estaba el ser [µm]
    // Ser: qué tipo es (Picked::Kind) y su referencia viva (la entidad de una célula o el id estable de
    // una bacteria): la misma al repetir la escena, porque la corrida es determinista.
    int pickedKind = 0;
    std::uint32_t ref = 0;
    std::string being;       // nombre del ser al anotarlo ("" sin ser)
    std::string text;
};

// Una línea `nota=` por nota (el texto escapado: \n y \\).
std::string serializeNotes(const std::vector<Note>& notes);
// Las notas de un texto de escena (ignora las líneas que no son `nota=` o están mal formadas).
std::vector<Note> parseNotes(const std::string& text);
// Dónde está ahora lo anotado: el ser, si sigue vivo; si no, su lugar. false para las de un instante
// o un ser que ya no existe (`pos` queda en el último lugar conocido).
bool notePosition(const World& world, const Note& note, Vec2& pos);
// Las notas en Markdown (para el informe y la exportación).
std::string notesMarkdown(const std::vector<Note>& notes);
