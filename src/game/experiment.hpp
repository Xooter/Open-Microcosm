#pragma once

#include "game/culture_run.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Experimentos por lote (fase P1, paso 6): réplicas con seeds distintas y factores barridos, sin
// ventana. Sin réplicas no hay estadística, y entre máquinas la reproducibilidad prometida es
// estadística. Un experimento es un archivo TOML (como los paquetes):
//
//   [experiment]  name, kind ("culture" o "plate"), replicates, seed, hours, sample_h, threads
//   [culture]     vessel, medium, temperature, light, gas_transfer [cm/h], program = ["linea", ...]
//                 (game/program),
//                 events = ["paso op valor [clave]", ...] (como en la escena de cultivo)
//   [plate]       scene = "texto de una escena .plate (placa + eventos)"
//   [[factor]]    key, values = [...]
//
// Factores: en un cultivo, "temperature", "light", "gas_transfer" [cm/h], "bacteria" (por mL al
// empezar), "solute:<clave>" (µM al empezar), "inoculate:<id del ser>" (individuos al empezar) o
// "reagent:<id>"; en una placa,
// "temperature", "solute:<clave>" o una clave del registro de parámetros (game/parameters). Los
// guardados con las claves viejas (en español) se leen igual (game/legacy_names).
// Cada combinación de niveles corre `replicas` veces con seeds derivadas de la del experimento.
// El tiempo es la hora de laboratorio: la del cultivo, o las horas biológicas de la placa.

struct ExperimentFactor {
    std::string key;
    std::vector<double> values;
};

struct ExperimentSpec {
    enum class Kind : std::uint8_t { Culture, Plate };
    std::string name = "experimento";
    Kind kind = Kind::Culture;
    int replicas = 3;
    std::uint64_t seed = 1;
    double hours = 24.0;
    double sampleHours = 1.0;
    CultureSpec culture;
    std::vector<CultureEvent> cultureEvents;
    std::string plateScene;
    std::vector<ExperimentFactor> factors;
    int threads = 0; // 0: uno por núcleo físico
};

bool parseExperiment(const std::string& text, ExperimentSpec& spec, std::string* error = nullptr);
std::string serializeExperiment(const ExperimentSpec& spec);

// Una combinación de niveles de los factores.
struct ExperimentLevel {
    std::vector<std::pair<std::string, double>> settings;
    std::string label; // "temperatura=20, luz=0.5"
};
std::vector<ExperimentLevel> experimentLevels(const ExperimentSpec& spec);
std::uint64_t replicaSeed(std::uint64_t seed, int level, int replica);

struct ReplicaResult {
    int level = 0, replica = 0;
    std::uint64_t seed = 0;
    std::vector<std::vector<double>> values; // [columna][muestra]
};

struct ExperimentResult {
    ExperimentSpec spec;
    std::vector<ExperimentLevel> levels;
    std::vector<std::string> columns, units;
    std::vector<double> times; // [h] las muestras (iguales en todas las réplicas)
    std::vector<ReplicaResult> replicas;
    double seconds = 0.0;      // tiempo de reloj que tomó
    int column(const std::string& name) const;
};

// Media, desvío estándar (n − 1) e intervalo de confianza del 95 % (t de Student) de una columna en
// una muestra, entre las réplicas de un nivel.
struct SummaryPoint {
    int n = 0;
    double mean = 0.0, sd = 0.0, ci95 = 0.0;
};
SummaryPoint summarize(const ExperimentResult& r, int level, int column, std::size_t sample);
double studentT975(int dof);

// Corre todo el lote (en paralelo: una corrida por hilo). `progress(hechas, total)` se llama desde
// los hilos de trabajo; `cancel` lo corta entre corridas.
ExperimentResult runExperiment(const ExperimentSpec& spec, const std::function<void(int, int)>& progress = {},
                               const std::atomic<bool>* cancel = nullptr);

// Formato largo (nivel, réplica, seed, t, columna, valor), resumen (nivel, t, columna, n, media,
// de, ic95) y JSON con la procedencia, los niveles, las seeds y el resumen.
std::string experimentLongCsv(const ExperimentResult& r);
std::string experimentSummaryCsv(const ExperimentResult& r);
std::string experimentJson(const ExperimentResult& r);
