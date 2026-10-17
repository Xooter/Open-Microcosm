#include "game/lesson.hpp"

#include "core/paths.hpp"
#include "core/toml_reader.hpp"
#include "game/culture_run.hpp"
#include "game/world.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>
#include <sstream>

namespace {

struct Registry {
    std::deque<Lesson> lessons;
    std::vector<std::string> warnings;
    std::recursive_mutex mutex;
    bool baseLoaded = false;
    std::atomic<bool> ready{false};
};
Registry& registry() {
    static Registry r;
    return r;
}

constexpr const char* kLessonKeys[] = {"id",       "titulo",   "nivel",     "objetivo", "fuente",  "placa",
                                       "bioma",    "seed",     "contenido", "cultivo",  "preparar", "controles",
                                       "fijados",  "paso",     "solucion",  "solucion_horas", "acciones"};
constexpr const char* kStepKeys[] = {"texto", "objetivo", "pregunta", "prediccion"};
constexpr const char* kCheckKeys[] = {"tipo", "clave", "op", "valor"};
constexpr const char* kQuestionKeys[] = {"texto", "opciones", "correcta", "explicacion"};
constexpr const char* kPredictionKeys[] = {"texto", "magnitud", "unidad"};
constexpr const char* kCheckKinds[] = {"tiempo_h", "poblacion",   "bacterias",    "concentracion", "ph",
                                       "plasmolisis", "hemolisis", "cerca_centro", "fraccion"};

void readCheck(const toml::table& t, const std::string& where, LessonCheck& c, std::vector<std::string>& warnings) {
    toml_io::warnUnknown(t, kCheckKeys, where, warnings);
    toml_io::Reader r{t, where, {}};
    r.text("tipo", c.kind);
    r.text("clave", c.key);
    r.text("op", c.op);
    r.num("valor", c.value);
    if (!c.kind.empty() && !toml_io::known(c.kind, kCheckKinds))
        warnings.push_back(where + ": condición desconocida '" + c.kind + "'");
    if (c.op != ">=" && c.op != "<=" && c.op != ">" && c.op != "<")
        warnings.push_back(where + ": op tiene que ser >=, <=, > o <");
    warnings.insert(warnings.end(), r.errors.begin(), r.errors.end());
}

std::vector<std::string> strings(const toml::table& t, const char* key) {
    std::vector<std::string> out;
    if (const toml::array* a = t[key].as_array())
        for (const toml::node& n : *a)
            if (auto s = n.value<std::string>())
                out.push_back(*s);
    return out;
}

void loadParsed(const toml::table& doc, const std::string& source, const std::string& pack) {
    Registry& reg = registry();
    const toml::array* list = doc["leccion"].as_array();
    if (!list)
        return;
    for (const toml::node& node : *list) {
        const toml::table* t = node.as_table();
        if (!t)
            continue;
        Lesson l;
        l.pack = pack;
        toml_io::Reader r{*t, source + " [[leccion]]", {}};
        r.text("id", l.key);
        const std::string where = source + " leccion '" + l.key + "'";
        toml_io::warnUnknown(*t, kLessonKeys, where, reg.warnings);
        r.text("titulo", l.title);
        r.text("nivel", l.level);
        r.text("objetivo", l.objective);
        r.text("fuente", l.source);
        r.text("placa", l.plate);
        r.text("bioma", l.biome);
        r.text("cultivo", l.culture);
        r.num("seed", l.seed);
        r.num("contenido", l.content);
        r.num("solucion_horas", l.solutionHours);
        l.setup = strings(*t, "preparar");
        l.exposed = strings(*t, "controles");
        l.solution = strings(*t, "solucion");
        if (const toml::array* acts = (*t)["acciones"].as_array())
            for (const toml::node& n : *acts)
                if (const toml::table* at = n.as_table()) {
                    Lesson::Action a;
                    a.label = (*at)["texto"].value_or(std::string());
                    a.event = (*at)["evento"].value_or(std::string());
                    if (!a.label.empty() && !a.event.empty())
                        l.actions.push_back(a);
                }
        if (const toml::array* locks = (*t)["fijados"].as_array())
            for (const toml::node& n : *locks)
                if (const toml::table* lt = n.as_table()) {
                    Lesson::Lock lock;
                    lock.key = (*lt)["clave"].value_or(std::string());
                    lock.reason = (*lt)["motivo"].value_or(std::string());
                    if (!lock.key.empty())
                        l.locked.push_back(lock);
                }
        if (const toml::array* steps = (*t)["paso"].as_array())
            for (const toml::node& n : *steps) {
                const toml::table* st = n.as_table();
                if (!st)
                    continue;
                LessonStep s;
                const std::string sw = where + " paso " + std::to_string(l.steps.size() + 1);
                toml_io::warnUnknown(*st, kStepKeys, sw, reg.warnings);
                s.text = (*st)["texto"].value_or(std::string());
                if (const toml::table* c = (*st)["objetivo"].as_table())
                    readCheck(*c, sw + " objetivo", s.check, reg.warnings);
                if (const toml::table* q = (*st)["pregunta"].as_table()) {
                    toml_io::warnUnknown(*q, kQuestionKeys, sw + " pregunta", reg.warnings);
                    s.question.text = (*q)["texto"].value_or(std::string());
                    s.question.options = strings(*q, "opciones");
                    s.question.correct = static_cast<int>((*q)["correcta"].value_or(std::int64_t{-1}));
                    s.question.explanation = (*q)["explicacion"].value_or(std::string());
                }
                if (const toml::table* p = (*st)["prediccion"].as_table()) {
                    toml_io::warnUnknown(*p, kPredictionKeys, sw + " prediccion", reg.warnings);
                    s.prediction.text = (*p)["texto"].value_or(std::string());
                    s.prediction.unit = (*p)["unidad"].value_or(std::string());
                    if (const toml::table* m = (*p)["magnitud"].as_table())
                        readCheck(*m, sw + " prediccion", s.prediction.quantity, reg.warnings);
                }
                l.steps.push_back(std::move(s));
            }
        reg.warnings.insert(reg.warnings.end(), r.errors.begin(), r.errors.end());
        if (l.key.empty() || l.title.empty()) {
            reg.warnings.push_back(source + ": una [[leccion]] sin 'id' o sin 'titulo': se saltea");
            continue;
        }
        if (std::any_of(reg.lessons.begin(), reg.lessons.end(), [&](const Lesson& o) { return o.key == l.key; })) {
            reg.warnings.push_back(where + ": id repetido (queda la primera)");
            continue;
        }
        reg.lessons.push_back(std::move(l));
    }
}

void ensureBase() {
    Registry& reg = registry();
    if (reg.ready.load(std::memory_order_acquire))
        return;
    std::lock_guard<std::recursive_mutex> lock(reg.mutex);
    if (reg.baseLoaded)
        return;
    reg.baseLoaded = true;
    const std::string path = assetPath("content/base/lecciones.toml");
    try {
        loadParsed(toml::parse_file(path), path, "base");
    } catch (const toml::parse_error& e) {
        reg.warnings.push_back("no se pudo cargar " + path + ": " + std::string(e.description()));
    }
    reg.ready.store(true, std::memory_order_release);
}

bool compare(double v, const std::string& op, double target) {
    if (op == "<=")
        return v <= target;
    if (op == "<")
        return v < target;
    if (op == ">")
        return v > target;
    return v >= target;
}

double plateSolute(const Census& c, const std::string& key) {
    if (key == "o2") return c.meanOxygen;
    if (key == "co2") return c.meanCO2;
    if (key == "fosfato") return c.meanPhosphate;
    if (key == "nitrogeno") return c.meanNitrogen;
    if (key == "silice") return c.meanSilicate;
    if (key == "hierro") return c.meanIron;
    if (key == "b12") return c.meanVitamin;
    if (key == "organica") return c.meanOrganic;
    return 0.0;
}

} // namespace

int lessonCount() {
    ensureBase();
    return static_cast<int>(registry().lessons.size());
}
const Lesson& lesson(int index) {
    ensureBase();
    return registry().lessons[static_cast<std::size_t>(index)];
}
const Lesson* findLesson(std::string_view key) {
    ensureBase();
    for (const Lesson& l : registry().lessons)
        if (l.key == key)
            return &l;
    return nullptr;
}
bool loadLessonsText(const std::string& text, const std::string& source, const std::string& pack, std::string* error) {
    std::lock_guard<std::recursive_mutex> lock(registry().mutex);
    ensureBase();
    try {
        loadParsed(toml::parse(text, source), source, pack);
    } catch (const toml::parse_error& e) {
        if (error)
            *error = source + ": " + std::string(e.description());
        return false;
    }
    return true;
}
const std::vector<std::string>& lessonLoadWarnings() {
    ensureBase();
    return registry().warnings;
}

double labHours(const World* world, const CultureRun* culture) {
    if (culture)
        return culture->hours();
    return world ? world->simTime() * world->bioTimeScale() / 3600.0 : 0.0;
}

double lessonQuantity(const LessonCheck& c, const World* world, const CultureRun* culture) {
    if (c.kind == "tiempo_h")
        return labHours(world, culture);
    if (culture) {
        const Culture& cu = culture->culture();
        const double ml = std::max(1e-12, cu.volumeMl());
        if (c.kind == "poblacion") {
            const std::optional<SpeciesId> id = findSpecies(c.key);
            return id ? cu.count(*id) / ml : 0.0;
        }
        if (c.kind == "bacterias")
            return static_cast<double>(cu.bacteriaCount()) / ml;
        if (c.kind == "ph")
            return cu.ph();
        if (c.kind == "concentracion")
            for (int k = 0; k < kSoluteCount; ++k)
                if (c.key == soluteKey(static_cast<Solute>(k)))
                    return cu.concentration(static_cast<Solute>(k));
        if (c.kind == "fraccion") {
            const std::size_t comma = c.key.find(',');
            const std::optional<SpeciesId> a = findSpecies(c.key.substr(0, comma));
            double total = 0.0;
            std::stringstream others(comma == std::string::npos ? c.key : c.key.substr(comma + 1));
            std::string tok;
            while (std::getline(others, tok, ','))
                if (const std::optional<SpeciesId> id = findSpecies(tok))
                    total += cu.count(*id);
            if (a && comma != std::string::npos)
                total += cu.count(*a);
            return a && total > 0.0 ? cu.count(*a) / total : 0.0;
        }
        return 0.0;
    }
    if (!world)
        return 0.0;
    const World& w = *world;
    if (c.kind == "poblacion" || c.kind == "fraccion" || c.kind == "bacterias" || c.kind == "concentracion" ||
        c.kind == "ph") {
        const Census census = w.census();
        if (c.kind == "bacterias")
            return census.bacteria;
        if (c.kind == "ph")
            return census.meanPH;
        if (c.kind == "concentracion")
            return plateSolute(census, c.key);
        if (c.kind == "poblacion") {
            const std::optional<SpeciesId> id = findSpecies(c.key);
            return id ? census.species[static_cast<int>(*id)] : 0.0;
        }
        const std::size_t comma = c.key.find(',');
        const std::optional<SpeciesId> a = findSpecies(c.key.substr(0, comma));
        double total = a ? census.species[static_cast<int>(*a)] : 0.0;
        std::stringstream others(comma == std::string::npos ? "" : c.key.substr(comma + 1));
        std::string tok;
        while (std::getline(others, tok, ','))
            if (const std::optional<SpeciesId> id = findSpecies(tok))
                total += census.species[static_cast<int>(*id)];
        return a && total > 0.0 ? census.species[static_cast<int>(*a)] / total : 0.0;
    }
    if (c.kind == "plasmolisis") {
        int cells = 0, shrunk = 0;
        for (const TissueCell& t : w.tissue().cells)
            if (t.protoplast != ~0u) {
                ++cells;
                shrunk += t.volume < 0.95f ? 1 : 0;
            }
        return cells ? static_cast<double>(shrunk) / cells : 0.0;
    }
    if (c.kind == "hemolisis") {
        const Erythrocytes& b = w.erythrocytes();
        int red = 0, lysed = 0;
        for (std::size_t i = 0; i < b.size(); ++i)
            if (b.kind[i] == BloodParticle::Erythrocyte) {
                ++red;
                lysed += b.lysed[i] ? 1 : 0;
            }
        return red ? static_cast<double>(lysed) / red : 0.0;
    }
    if (c.kind == "cerca_centro") {
        const std::optional<SpeciesId> id = findSpecies(c.key);
        if (!id)
            return 0.0;
        const Vec2 center = {0.5f * w.width(), 0.5f * w.height()};
        const float zone = 0.2f * w.dish().radius;
        int inside = 0, all = 0;
        for (const auto [e, body, sp] : w.registry().view<const RingBody, const Species>().each())
            if (sp.id == *id) {
                ++all;
                inside += length(body.centroid - center) < zone ? 1 : 0;
            }
        return all ? static_cast<double>(inside) / all : 0.0;
    }
    return 0.0;
}

bool lessonCheckMet(const LessonCheck& c, const World* world, const CultureRun* culture) {
    return !c.present() || compare(lessonQuantity(c, world, culture), c.op, c.value);
}

void LessonProgress::reset(const Lesson& l) {
    lesson = l.key;
    step = 0;
    const std::size_t n = l.steps.size();
    answers.assign(n, -1);
    openAnswers.assign(n, std::string());
    predictions.assign(n, 0.0);
    predicted.assign(n, 0);
    measured.assign(n, 0.0);
    metAtHours.assign(n, -1.0);
    unlocked.clear();
}

std::string lessonReport(const Lesson& l, const LessonProgress& p, const std::string& provenance) {
    std::ostringstream out;
    out << "# " << l.title << "\n\n";
    if (!l.objective.empty())
        out << l.objective << "\n\n";
    if (!l.source.empty())
        out << "Fuente: " << l.source << "\n\n";
    out << provenance << "\n\n";
    if (!p.unlocked.empty()) {
        out << "**Fuera de protocolo:** se destrabaron";
        for (const std::string& k : p.unlocked)
            out << ' ' << k;
        out << ".\n\n";
    }
    for (std::size_t i = 0; i < l.steps.size(); ++i) {
        const LessonStep& s = l.steps[i];
        out << "## Paso " << i + 1 << "\n\n" << s.text << "\n\n";
        if (s.check.present())
            out << "- Objetivo: " << (i < p.metAtHours.size() && p.metAtHours[i] >= 0.0
                                          ? "cumplido a las " + std::to_string(p.metAtHours[i]) + " h"
                                          : std::string("sin cumplir"))
                << "\n";
        if (s.prediction.present() && i < p.predicted.size()) {
            out << "- Predicción: " << (p.predicted[i] ? std::to_string(p.predictions[i]) : std::string("(sin predecir)")) << ' '
                << s.prediction.unit;
            if (p.metAtHours[i] >= 0.0)
                out << "; medido: " << p.measured[i] << ' ' << s.prediction.unit;
            out << "\n";
        }
        if (s.question.present() && i < p.answers.size()) {
            out << "- " << s.question.text << "\n  Respuesta: ";
            if (!s.question.options.empty() && p.answers[i] >= 0 && p.answers[i] < static_cast<int>(s.question.options.size()))
                out << s.question.options[static_cast<std::size_t>(p.answers[i])];
            else
                out << p.openAnswers[i];
            out << "\n";
        }
        out << "\n";
    }
    return out.str();
}
