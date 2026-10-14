#pragma once

#include <cstdint>
#include <span>
#include <string_view>

class World;

// Registro único de los parámetros ajustables de una placa en marcha (MVP: reproducibilidad).
// De esta tabla salen los controles de los paneles, la exportación de la corrida y el registro
// de cambios: un parámetro nuevo se suma acá y aparece en los tres lugares.
enum class ParamGroup : std::uint8_t {
    Chemistry,      // Procesos > Química
    Micronutrients, // Procesos > Micronutrientes
    Microbiology,   // Procesos > Microbiología
    Fungi,          // Procesos > Hongos y mohos
    Plasmodium,     // Procesos > Hongos y mohos (Physarum)
    Mechanics,      // Simulación > Agua y cuerpos blandos (avanzado)
    Time,           // Simulación > Tiempo
    Metabolism,     // sin control: constantes fisiológicas que se exportan
    Engine,         // del motor; se controla desde Preferencias (LOD) y se exporta
};

struct ParamInfo {
    const char* key;   // estable, no se traduce: "medio.alcalinidad"
    const char* label; // clave de traducción (texto en español)
    const char* unit;  // "" si no tiene
    ParamGroup group;
    float lo, hi;      // rango del control
    const char* format;
    bool logScale;
    const char* help;  // nullptr: sin ayuda
    float* (*real)(World&);         // valor real (nullptr si es entero)
    int* (*integer)(World&);        // valor entero (nullptr si es real)
};

std::span<const ParamInfo> parameters();
const ParamInfo* findParameter(std::string_view key);
// Valor actual como real (los enteros se convierten).
float parameterValue(World& world, const ParamInfo& p);
// Fija el valor (los enteros se redondean). No se acota al rango del control: un valor registrado
// se reproduce tal cual.
void setParameterValue(World& world, const ParamInfo& p, float value);
const char* paramGroupName(ParamGroup group);
