#pragma once

#include "bio/species.hpp"
#include "core/vec2.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class World;

// CASA (análisis de semen asistido por computadora): sigue la cabeza de cada nadador de una especie
// cuadro a cuadro y mide su cinemática como los equipos de laboratorio (OMS 2021; Mortimer 2000, Hum.
// Reprod. Update 6: 136). Es medición: solo lee el mundo, no es una intervención.
//  VCL: rapidez curvilínea (la trayectoria punto a punto); VSL: rectilínea (del primero al último);
//  VAP: la de la trayectoria media (promedio móvil de 5 puntos); LIN = VSL/VCL, STR = VSL/VAP,
//  WOB = VAP/VCL; ALH: amplitud del desplazamiento lateral de la cabeza (de pico a pico); BCF: frecuencia
//  con que la trayectoria cruza la media (una ida y vuelta por batido: se cuentan los pares de cruces).
enum class CasaGrade : std::uint8_t { Rapid, Slow, NonProgressive, Immotile };
inline constexpr int kCasaGrades = 4;
const char* casaGradeName(CasaGrade g);

// Clases por umbrales (OMS 2021: progresivo rápido ≥ 25 µm/s, lento 5–25 µm/s, no progresivo sin avance;
// un equipo CASA los fija por la VSL y trata como inmóvil lo que apenas se mueve).
struct CasaThresholds {
    float immotileVcl = 10.0f;   // [µm/s]
    float progressiveVsl = 5.0f; // [µm/s]
    float rapidVsl = 25.0f;      // [µm/s]
};

struct CasaKinematics {
    std::uint32_t id = 0; // id estable del cuerpo (RingBody::id)
    float vcl = 0, vsl = 0, vap = 0, lin = 0, str = 0, wob = 0, alh = 0, bcf = 0;
    CasaGrade grade = CasaGrade::Immotile;
};

struct CasaSummary {
    int tracked = 0;                 // con la trayectoria completa
    int count[kCasaGrades] = {};
    float vcl = 0, vsl = 0, vap = 0, lin = 0, str = 0, wob = 0, alh = 0, bcf = 0; // medias de los móviles
    float fraction(CasaGrade g) const { return tracked ? static_cast<float>(count[static_cast<int>(g)]) / tracked : 0.0f; }
    float progressive() const { return fraction(CasaGrade::Rapid) + fraction(CasaGrade::Slow); }
};

class Casa {
  public:
    bool enabled = false;
    bool speciesSet = false;
    SpeciesId species{};
    float trackSeconds = 1.0f; // largo de la trayectoria que se analiza [s]
    CasaThresholds thresholds;

    // Un punto por paso (llamar después de cada paso con su dt). Recalcula cada `kEvery` pasos.
    void sample(const World& world, float dt);
    void clear();
    const std::vector<CasaKinematics>& results() const { return results_; }
    const CasaSummary& summary() const { return summary_; }

  private:
    struct Track {
        std::vector<Vec2> pts; // buffer circular
        int head = 0, count = 0;
        std::uint64_t seen = 0;
    };
    void analyze(float dt);
    std::unordered_map<std::uint32_t, Track> tracks_;
    std::vector<CasaKinematics> results_;
    CasaSummary summary_;
    std::vector<Vec2> scratch_;
    std::uint64_t tick_ = 0;
};

// La cinemática de una trayectoria de puntos tomados cada `dt` segundos (sin raylib ni mundo).
CasaKinematics casaKinematics(const std::vector<Vec2>& pts, float dt, const CasaThresholds& thr);
std::string casaCsv(const Casa& casa);
