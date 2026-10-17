#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class World;
class CultureRun;

// Protocolos y lecciones (fase P1, paso 10). Una lección es un protocolo con pasos: qué se prepara
// (una placa preparada, una gota o un cultivo), qué controles ve el alumno, qué parámetros fija el
// protocolo (con candado y su motivo: se pueden destrabar, pero la corrida queda "fuera de
// protocolo") y los pasos, cada uno con un objetivo observable que verifica la simulación, una
// pregunta con su explicación o una predicción que después se compara con lo medido. No hay
// puntaje: el informe exportable lo corrige el docente.
//
// Datos de los paquetes (lecciones.toml, [[leccion]]); el código aporta las condiciones con nombre.

// Condición observable (mecanismos con nombre; los datos eligen cuál y con qué valor):
//   "tiempo_h"      horas de laboratorio ≥ valor
//   "poblacion"     clave = id del ser: individuos (placa) o por mL (cultivo), comparado con valor
//   "bacterias"     bacterias (placa) o por mL (cultivo)
//   "concentracion" clave = soluto (fosfato, nitrogeno, silice, hierro, b12, organica, o2, co2) [µM]
//   "ph"
//   "plasmolisis"   fracción de protoplastos plasmolizados (volumen < 95 % de la pared llena)
//   "hemolisis"     fracción de glóbulos rojos hemolizados
//   "cerca_centro"  clave = id del ser: fracción a menos de 0,2 radios del centro (Jennings)
//   "fraccion"      clave = id del ser: su fracción entre los seres de la clave2 ("a,b")
struct LessonCheck {
    std::string kind; // vacío: el paso no tiene objetivo (se avanza a mano)
    std::string key;
    std::string op = ">="; // ">=", "<=", ">", "<"
    double value = 0.0;
    bool present() const { return !kind.empty(); }
};

struct LessonQuestion {
    std::string text;
    std::vector<std::string> options;
    int correct = -1; // -1: abierta (se responde con texto)
    std::string explanation;
    bool present() const { return !text.empty(); }
};

// Predicción: antes de correr, el alumno escribe un valor de la magnitud `quantity` (una condición
// sin comparación); al cumplirse el paso se muestra el valor medido al lado.
struct LessonPrediction {
    std::string text;
    LessonCheck quantity;
    std::string unit;
    bool present() const { return !text.empty(); }
};

struct LessonStep {
    std::string text;
    LessonCheck check;
    LessonQuestion question;
    LessonPrediction prediction;
};

struct Lesson {
    std::string key, title, level, objective, source, pack;
    // Qué se prepara: una placa preparada (id de placas.toml), una gota de un bioma o un cultivo.
    std::string plate;      // id de una placa preparada
    std::string biome;      // id de bioma (gota) si no hay placa
    std::uint64_t seed = 1;
    int content = 2;        // DropContent de la gota (0 completa, 1 sin vida, 2 vacía)
    std::string culture;    // escena de cultivo (texto .cultivo) si es un cultivo
    std::vector<std::string> setup; // intervenciones al empezar (texto de game/intervention)
    std::vector<std::string> exposed; // claves del registro de parámetros que ve el alumno
    // Acciones con nombre: botones del panel de la lección (el alumno no necesita otras
    // herramientas). En una placa, una intervención (texto de game/intervention, sin el paso); en un
    // cultivo, una operación ("op valor [clave]").
    struct Action {
        std::string label, event;
    };
    std::vector<Action> actions;
    struct Lock {
        std::string key, reason;
    };
    std::vector<Lock> locked;         // fijadas por el protocolo
    std::vector<LessonStep> steps;
    // Solución (para los tests y el docente): intervenciones de la placa u operaciones del cultivo
    // ("evento=..."), que cumplen todos los objetivos.
    std::vector<std::string> solution;
    double solutionHours = 1.0; // cuánto correr la solución
    bool isCulture() const { return !culture.empty(); }
};

int lessonCount();
const Lesson& lesson(int index);
const Lesson* findLesson(std::string_view key);
bool loadLessonsText(const std::string& text, const std::string& source, const std::string& pack, std::string* error = nullptr);
const std::vector<std::string>& lessonLoadWarnings();

// Valor de la magnitud de una condición en la placa o el cultivo (NaN-libre: 0 si no aplica).
double lessonQuantity(const LessonCheck& c, const World* world, const CultureRun* culture);
bool lessonCheckMet(const LessonCheck& c, const World* world, const CultureRun* culture);
double labHours(const World* world, const CultureRun* culture);

// Lo que respondió y predijo el alumno, para el informe.
struct LessonProgress {
    std::string lesson;
    int step = 0;
    std::vector<int> answers;           // opción elegida por paso (-1: sin responder)
    std::vector<std::string> openAnswers;
    std::vector<double> predictions;    // valor predicho por paso (NaN no: `predicted`)
    std::vector<std::uint8_t> predicted;
    std::vector<double> measured;       // valor medido al cumplirse el paso
    std::vector<double> metAtHours;     // hora en que se cumplió el objetivo (-1: todavía no)
    std::vector<std::string> unlocked;  // parámetros del protocolo que se destrabaron
    void reset(const Lesson& l);
};
// Informe (Markdown): la lección, cada paso con lo respondido, lo predicho contra lo medido y si se
// salió del protocolo.
std::string lessonReport(const Lesson& l, const LessonProgress& p, const std::string& provenance);
