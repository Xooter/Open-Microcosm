#pragma once

#include "game/world.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Placas preparadas (fase 20a): además de la gota de un bioma, placas como en un laboratorio. Son
// datos de los paquetes (medios.toml, recipientes.toml, placas.toml), con el mismo esquema de
// registro que los seres y los biomas.

// Medio de cultivo: una composición publicada (las mismas claves que el medio de un bioma).
struct CultureMedium {
    std::string key, name, description, reference, pack, source;
    MediumParams composition; // lo que el medio no dice queda como en MediumParams{}
};

// Recipiente: área de cultivo y volumen de medio de trabajo.
struct Vessel {
    std::string key, name, pack, source;
    float areaCm2 = 8.0f, volumeMl = 2.0f;
    // Profundidad del medio [µm] = volumen / área.
    float depthUm() const { return volumeMl / areaCm2 * 1e4f; }
};

enum class PlateKind : std::uint8_t { Drop, CellCulture, Slide };

struct PreparedPlate {
    std::string key, name, description, pack, source;
    PlateKind kind = PlateKind::CellCulture;
    std::string medium, vessel; // ids
    float fieldUm = 2048.0f;    // lado del campo simulado
    float temperature = 37.0f, co2 = 0.05f, light = 0.0f; // ambiente (incubadora)
    std::vector<std::pair<std::string, float>> seeding;   // id del ser, células/cm²
    Tissue tissue;       // preparado: el tejido montado (si tiene)
    bool fixed = false;  // preparado fijado (y teñido): nada vivo
    std::string stain;   // tinción con la que se prepara ("" ninguna)
    float cellSize = 8.0f; // celda de la grilla [µm] (un frotis de sangre necesita 4 µm)
    float erythrocytesPerCm2 = 0.0f, plateletsPerCm2 = 0.0f; // sangre
    float bacteriaPerCm2 = 0.0f; // bacterias del preparado (un raspado de mucosa trae las de la boca)
};

int cultureMediumCount();
const CultureMedium* findCultureMedium(std::string_view key);
const CultureMedium& cultureMedium(int index);
int vesselCount();
const Vessel* findVessel(std::string_view key);
const Vessel& vessel(int index);
int preparedPlateCount();
const PreparedPlate* findPreparedPlate(std::string_view key);
const PreparedPlate& preparedPlate(int index);

// Carga [[medio]], [[recipiente]] y [[placa]] de un texto TOML de un paquete. Los que tienen
// errores se saltean y quedan en `preparedLoadWarnings`.
bool loadPreparedText(const std::string& text, const std::string& source, const std::string& pack,
                      std::string* error = nullptr);
const std::vector<std::string>& preparedLoadWarnings();

// La placa que sale de una placa preparada: el medio con el ambiente de la incubadora, el campo
// circular con su pared de vidrio (el borde del campo simulado), la profundidad del medio del recipiente
// y sin nada vivo ni sedimento (estéril). La siembra va en `WorldConfig::seeding`.
WorldConfig makePrepared(const PreparedPlate& plate, std::uint64_t seed);
