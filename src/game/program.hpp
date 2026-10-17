#pragma once

#include <string>
#include <vector>

// Programas temporales (fase P1): lo que en un laboratorio se cambia con el reloj, como datos de la
// placa o del cultivo y no como intervenciones sueltas. Un fotoperíodo de 16:8, una rampa de
// temperatura, un pulso de fosfato, alimentar con bacterias cada día o cambiar el medio (el
// protocolo de Gause). Se guardan como su definición (no como miles de eventos) y se aplican una
// vez por paso, así el resultado no depende de los cuadros por segundo y una escena los repite.
//
// El reloj es la hora de laboratorio [h]: en el cultivo, su tiempo; en la placa, las horas
// biológicas (la biología está comprimida y un fotoperíodo es biología).

enum class ProgramShape : unsigned char {
    Ramp,   // de `from` a `to` entre `start` y `end` (antes, nada; después, `to`)
    Cycle,  // `to` durante `duty`·`period` de cada período y `from` el resto (fotoperíodo)
    Sine,   // `from` + `to`·sen(2π (t − start) / period)
    Event,  // una acción de `amount` cada `period` horas desde `start` (0: una sola vez)
};

// "Sin fin" (no se usa infinito: con -ffast-math no se puede comparar con él).
inline constexpr double kProgramForever = 1e30;

struct ProgramEntry {
    // Qué cambia: "temperatura", "luz", "soluto:<clave>" (fosfato, nitrogeno, silice, hierro, b12,
    // organica, co2), "parametro:<clave del registro>", o una acción: "bacterias" (bacterias por mL que
    // se agregan), "medio" (fracción del medio que se renueva: el protocolo de Gause centrifuga y
    // cambia el medio; los protistas se quedan), "dilucion" (fracción del cultivo que se reemplaza
    // por medio nuevo, con todo lo que tiene), "agregar:<soluto>" (µM que se suman) o
    // "inocular:<id del ser>" (individuos). En la placa no hay "medio" ni "dilucion" (está la perfusión).
    std::string target;
    ProgramShape shape = ProgramShape::Cycle;
    double start = 0.0;                                      // [h]
    double end = kProgramForever;                            // [h]
    float from = 0.0f, to = 0.0f;
    float period = 24.0f; // [h]
    float duty = 0.5f;    // fracción del período en `to` (ciclo)
    float amount = 0.0f;  // acción
    bool isAction() const;
};

struct Program {
    std::vector<ProgramEntry> entries;
    bool empty() const { return entries.empty(); }
};

// Valor del objetivo a la hora `hours` (no para acciones). false si a esa hora no manda (antes de
// empezar, o después de terminar si no es una rampa).
bool programValue(const ProgramEntry& e, double hours, float& value);
// Cuántas veces ocurre la acción en (from, to] [h].
int programEvents(const ProgramEntry& e, double from, double to);

// Texto: una línea por entrada, `programa=objetivo forma inicio fin desde hasta periodo ciclo
// cantidad` (fin "inf" si no termina). Los floats con precisión de ida y vuelta.
std::string serializeProgram(const Program& p);
// Lee una línea (lo que va después de "programa="). false si está mal formada.
bool parseProgramEntry(const std::string& text, ProgramEntry& out);
const char* programShapeKey(ProgramShape s); // "rampa", "ciclo", "seno", "evento"
