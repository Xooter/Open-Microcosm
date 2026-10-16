#pragma once

#include "game/culture.hpp"
#include "game/plate_spec.hpp"
#include "game/program.hpp"
#include "game/timeseries.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Sesión de un cultivo bien mezclado (fase P1): el cultivo, su serie temporal y el registro de lo
// que se le hizo. Como `Run` para la placa: toda operación pasa por `apply`, se anota con su paso y
// una escena de cultivo (`.cultivo`) la repite en el mismo lugar de la trayectoria.

inline constexpr float kCultureStep = 60.0f; // [s] paso del cultivo

// Lo que se preconfigura (como `PlateSpec` para la placa).
struct CultureSpec {
    std::string name = "cultivo";
    std::string vessel = "tubo_gause"; // recipientes.toml
    std::string medium = "osterhout_gause"; // medios.toml
    float temperature = 26.0f;  // [°C] (Gause cultivaba a 26 °C)
    float light = 0.0f;         // irradiancia relativa de la lámpara
    std::uint64_t seed = 1;
    Program program;
    CultureConfig config() const; // el recipiente y el medio de los paquetes cargados
};

enum class CultureOp : std::uint8_t {
    Inoculate,     // key = id del ser, value = individuos
    Bacteria,      // value = bacterias por mL que se agregan
    Renew,         // value = fracción del medio que se cambia (centrifugado: los protistas quedan)
    Dilute,        // value = fracción del cultivo que se reemplaza por medio nuevo
    Concentration, // key = soluto (soluteKey), value = µM
    Phages,        // value = viriones por mL que se agregan
    Sample,        // value = volumen sacado [µL] (gota de muestra); key = concentrado (×)
    Withdraw,      // value = volumen sacado [µL] para un pasaje a otro cultivo
    Receive,       // value = volumen recibido [µL]; key = lo que trae (células con su estado, bacterias
                   // y concentraciones: ver encodeTransfer), así la escena del cultivo que recibe se repite
    InoculateDensity, // key = id del ser, value = individuos por mL (la cantidad es un sorteo de Poisson)
    Count
};
const char* cultureOpKey(CultureOp op);

struct CultureEvent {
    std::uint64_t step = 0; // se aplica antes de este paso
    CultureOp op = CultureOp::Inoculate;
    std::string key;
    double value = 0.0;
};

class CultureRun {
  public:
    void start(const CultureSpec& spec);
    bool started() const { return culture_ != nullptr; }
    // Avanza `seconds` de cultivo (en pasos de kCultureStep; lo que sobra queda para la próxima).
    void advance(double seconds);
    // Aplica ya (entre pasos) y anota. Devuelve si tuvo efecto.
    bool apply(CultureEvent ev);
    // Programa operaciones para repetirlas (escena de cultivo).
    void schedule(std::vector<CultureEvent> script);

    // Gota de muestra: lo que contiene el volumen de una gota bajo el cubreobjetos (la placa que se
    // mira con la simulación completa), `concentrate` veces concentrado (centrifugado suave). Las
    // células y bacterias sorteadas salen del cultivo (se anota como operación).
    PlateSpec sampleDrop(float concentrate = 1.0f);
    // Conteo de una alícuota de `volumeMl` (como Gause, que contaba 0,1 cm³): Poisson alrededor de la
    // densidad real. No saca la alícuota.
    int countAliquot(SpeciesId id, float volumeMl, std::uint64_t draw) const;
    // Pasaje: saca `volumeMl` de este cultivo (sin concentrar) y lo agrega a `dest`, con las
    // células con su estado. Se anota en los dos.
    void transferTo(CultureRun& dest, double volumeMl);
    // Volumen [µL] que se ve en una gota de muestra (la placa simulada).
    static double dropVolumeUl(const WorldConfig& plate);

    Culture& culture() { return *culture_; }
    const Culture& culture() const { return *culture_; }
    const CultureSpec& spec() const { return spec_; }
    CultureSpec& spec() { return spec_; }
    const TimeSeries& series() const { return series_; }
    const std::vector<CultureEvent>& events() const { return events_; }
    float sampleHours() const { return sampleHours_; }
    void setSampleHours(float h) { sampleHours_ = h > 0.01f ? h : 0.01f; }
    double hours() const { return culture_ ? culture_->time() / 3600.0 : 0.0; }
    void sample();

  private:
    void buildColumns();
    CultureSpec spec_;
    std::unique_ptr<Culture> culture_;
    TimeSeries series_;
    std::vector<float> row_;
    std::vector<CultureEvent> events_, script_;
    std::size_t scriptNext_ = 0;
    double carry_ = 0.0;
    double nextSample_ = 0.0;
    float sampleHours_ = 1.0f;
    std::uint64_t samples_ = 0;
};

// Escena de cultivo: texto clave=valor versionado (`cultivo_version`), el programa
// (`programa=`) y las operaciones (`evento=paso op valor [clave]`).
std::string serializeCultureScene(const CultureSpec& spec, const std::vector<CultureEvent>& events);
bool parseCultureScene(const std::string& text, CultureSpec& spec, std::vector<CultureEvent>& events);
std::string defaultCulturesDir(); // <datos>/biogame/cultivos
