// Lección (fase P1): un protocolo con pasos, de los paquetes (lessons/). El alumno no necesita
// otros paneles: el de la lección prepara la placa o el cultivo, muestra los controles que expone,
// las acciones con nombre, el tiempo y cada paso con su objetivo, su pregunta o su predicción. No hay
// puntaje: se exporta un informe que corrige el docente.
#include "app/app.hpp"
#include "game/culture_run.hpp"
#include "game/export.hpp"
#include "game/intervention.hpp"
#include "game/lesson.hpp"
#include "game/scene.hpp"
#include "ui/glossary_ui.hpp"
#include "ui/i18n.hpp"
#include "ui/theme.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/param_controls.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace ui {

namespace {

class LessonPanel : public Panel {
  public:
    LessonPanel() { open = false; } // se abre desde el menú Ver
    const char* key() const override { return "lesson"; }
    const char* title() const override { return "Lección"; }
    Dock dock() const override { return Dock::Left; }

    void draw(App& app) override {
        if (lessonCount() == 0) {
            note("No hay lecciones cargadas.");
            return;
        }
        std::vector<const char*> titles;
        for (int i = 0; i < lessonCount(); ++i)
            titles.push_back(tr(lesson(i).title.c_str()));
        if (!app.lessonRequest.empty() && !active_)
            for (int i = 0; i < lessonCount(); ++i)
                if (lesson(i).key == app.lessonRequest)
                    choice_ = i;
        app.lessonRequest.clear();
        choice_ = std::clamp(choice_, 0, lessonCount() - 1);
        ImGui::BeginDisabled(active_);
        combo("Lección", &choice_, titles.data(), static_cast<int>(titles.size()));
        ImGui::EndDisabled();
        const Lesson& l = lesson(choice_);
        if (!active_) {
            if (!l.level.empty())
                ImGui::TextDisabled("%s", tr(l.level.c_str()));
            ImGui::TextWrapped("%s", tr(l.objective.c_str()));
            glossaryChips(app, {l.objective.c_str()});
            if (ImGui::Button(tr("Empezar")))
                start(app, l);
            return;
        }
        if (ImGui::SmallButton(tr("Terminar la lección")))
            stop(app);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Exportar informe")))
            report(app, l);
        if (!message_.empty())
            ImGui::TextDisabled("%s", message_.c_str());
        ImGui::Separator();
        drawTime(app, l);
        drawControls(app, l);
        ImGui::Separator();
        drawSteps(app, l);
    }

  private:
    int choice_ = 0;
    bool active_ = false;
    LessonProgress progress_;
    CultureRun* culture_ = nullptr; // el cultivo de la lección (vive en app.cultures)
    std::vector<float> guesses_;
    std::vector<std::string> answers_;
    std::string message_;
    double lastCheck_ = -1.0;

    const World* world(const App& app) const { return culture_ ? nullptr : app.world.get(); }

    void start(App& app, const Lesson& l) {
        progress_.reset(l);
        guesses_.assign(l.steps.size(), 0.0f);
        answers_.assign(l.steps.size(), std::string());
        message_.clear();
        culture_ = nullptr;
        app.protocol = l.key;
        app.protocolLocks.clear();
        for (const Lesson::Lock& lock : l.locked)
            app.protocolLocks.emplace_back(lock.key, lock.reason);
        app.unlockedParameters.clear();
        if (l.isCulture()) {
            CultureSpec spec;
            std::vector<CultureEvent> events;
            parseCultureScene(l.culture, spec, events);
            auto run = std::make_unique<CultureRun>();
            run->start(spec);
            run->schedule(events);
            culture_ = run.get();
            app.cultures.push_back(std::move(run));
            app.cultureIndex = static_cast<int>(app.cultures.size()) - 1;
            app.culturesPaused = false;
        } else {
            PlateSpec plate;
            if (!l.plate.empty())
                plate.prepared = l.plate;
            else if (const auto b = findBiome(l.biome))
                plate.biome = static_cast<int>(*b);
            plate.seed = l.seed;
            plate.content = static_cast<DropContent>(std::clamp(l.content, 0, 2));
            plate.rebuild();
            std::string text;
            for (const std::string& e : l.setup)
                text += "event=" + e + "\n";
            app.plate = plate;
            app.plateScript = parseInterventions(text);
            app.plateNotes.clear();
            app.createPlate();
            app.tools.paused = false;
        }
        active_ = true;
    }

    void stop(App& app) {
        active_ = false;
        culture_ = nullptr;
        app.protocol.clear();
        app.protocolLocks.clear();
    }

    void drawTime(App& app, const Lesson&) {
        readout("Tiempo", "%.2f h", labHours(world(app), culture_));
        if (culture_) {
            if (ImGui::Button(tr(app.culturesPaused ? "Reanudar" : "Pausar")))
                app.culturesPaused = !app.culturesPaused;
            sliderLog("Velocidad [h/s]", &app.cultureSpeed, 0.05f, 200.0f, "%.2f",
                      "Horas de cultivo por segundo real. No cambia los resultados: el paso es fijo.");
        } else {
            if (ImGui::Button(tr(app.tools.paused ? "Reanudar" : "Pausar")))
                app.tools.paused = !app.tools.paused;
            sliderLog("Velocidad (x)", &app.tools.simSpeed, 0.125f, 8.0f, "%.2f",
                      "Múltiplo del tiempo real. El paso de la física es fijo.");
        }
    }

    void drawControls(App& app, const Lesson& l) {
        if (!culture_ && app.world)
            for (const std::string& k : l.exposed)
                if (const ParamInfo* p = findParameter(k))
                    parameterControl(app, *p);
        // Los parámetros que el protocolo fija se muestran con su motivo (y se pueden destrabar).
        if (!culture_ && app.world)
            for (const Lesson::Lock& lock : l.locked)
                if (const ParamInfo* p = findParameter(lock.key))
                    parameterControl(app, *p);
        for (std::size_t i = 0; i < l.actions.size(); ++i) {
            const Lesson::Action& a = l.actions[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Button(tr(a.label.c_str())))
                act(app, a);
            ImGui::PopID();
        }
    }

    void act(App& app, const Lesson::Action& a) {
        if (culture_) {
            std::vector<CultureEvent> ev;
            CultureSpec ignored;
            parseCultureScene("culture_version=1\nevent=0 " + a.event + "\n", ignored, ev);
            for (const CultureEvent& e : ev)
                culture_->apply(e);
            return;
        }
        for (const Intervention& iv : parseInterventions("event=" + a.event + "\n")) {
            if (iv.kind == InterventionKind::Dropper || iv.kind == InterventionKind::Ink ||
                iv.kind == InterventionKind::Stir || iv.kind == InterventionKind::Squeeze)
                app.interveneState(iv);
            else
                app.intervene(iv);
        }
    }

    void drawSteps(App& app, const Lesson& l) {
        const double now = labHours(world(app), culture_);
        // Los objetivos se evalúan un par de veces por segundo (el censo recorre todo).
        const double t = ImGui::GetTime();
        const bool check = t - lastCheck_ > 0.5;
        if (check)
            lastCheck_ = t;
        for (std::size_t i = 0; i < l.steps.size() && static_cast<int>(i) <= progress_.step; ++i) {
            const LessonStep& s = l.steps[i];
            ImGui::PushID(static_cast<int>(i));
            const bool current = static_cast<int>(i) == progress_.step;
            ImGui::TextColored(current ? ImVec4(1, 1, 1, 1) : ImVec4(0.6f, 0.6f, 0.6f, 1), "%s %zu", tr("Paso"), i + 1);
            ImGui::TextWrapped("%s", tr(s.text.c_str()));
            if (s.prediction.present()) {
                ImGui::TextWrapped("%s", tr(s.prediction.text.c_str()));
                if (!progress_.predicted[i]) {
                    ImGui::InputFloat(tr("Mi predicción"), &guesses_[i], 0.0f, 0.0f, "%.3f");
                    ImGui::SameLine();
                    if (ImGui::SmallButton(tr("Anotar"))) {
                        progress_.predictions[i] = guesses_[i];
                        progress_.predicted[i] = 1;
                    }
                } else {
                    ImGui::TextDisabled("%s %.3f %s", tr("Predicción:"), progress_.predictions[i], tr(s.prediction.unit.c_str()));
                    if (progress_.metAtHours[i] >= 0.0)
                        ImGui::TextDisabled("%s %.3f %s", tr("Medido:"), progress_.measured[i], tr(s.prediction.unit.c_str()));
                }
            }
            if (s.check.present()) {
                if (progress_.metAtHours[i] < 0.0 && current && check && lessonCheckMet(s.check, world(app), culture_)) {
                    progress_.metAtHours[i] = now;
                    if (s.prediction.present())
                        progress_.measured[i] = lessonQuantity(s.prediction.quantity, world(app), culture_);
                }
                if (progress_.metAtHours[i] >= 0.0)
                    ImGui::TextColored(col(palette().good), "%s %.2f h", tr("Objetivo cumplido a las"),
                                       progress_.metAtHours[i]);
                else
                    ImGui::TextDisabled("%s", tr("Objetivo: todavía no"));
            } else if (current && progress_.metAtHours[i] < 0.0) {
                progress_.metAtHours[i] = now;
            }
            if (s.question.present()) {
                ImGui::TextWrapped("%s", tr(s.question.text.c_str()));
                if (!s.question.options.empty()) {
                    for (std::size_t k = 0; k < s.question.options.size(); ++k)
                        if (ImGui::RadioButton(tr(s.question.options[k].c_str()), progress_.answers[i] == static_cast<int>(k)))
                            progress_.answers[i] = static_cast<int>(k);
                } else {
                    char buf[512];
                    std::snprintf(buf, sizeof buf, "%s", answers_[i].c_str());
                    if (ImGui::InputTextMultiline("##respuesta", buf, sizeof buf, {-1.0f, 60.0f}))
                        answers_[i] = progress_.openAnswers[i] = buf;
                }
                // La explicación, una vez respondida (sin puntaje: es para entender).
                if (progress_.answers[i] >= 0 || !progress_.openAnswers[i].empty())
                    ImGui::TextWrapped("%s", tr(s.question.explanation.c_str()));
            }
            if (current) // los términos del paso actual (con la explicación, si ya se respondió)
                glossaryChips(app, {s.text.c_str(), s.prediction.text.c_str(), s.question.text.c_str(),
                                    progress_.answers[i] >= 0 || !progress_.openAnswers[i].empty()
                                        ? s.question.explanation.c_str()
                                        : nullptr});
            const bool done = !s.check.present() || progress_.metAtHours[i] >= 0.0;
            if (current && done && i + 1 < l.steps.size() && ImGui::Button(tr("Siguiente paso")))
                ++progress_.step;
            if (current && done && i + 1 == l.steps.size())
                ImGui::TextColored(col(palette().good), "%s", tr("Lección completa: exportá el informe."));
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    void report(App& app, const Lesson& l) {
        namespace fs = std::filesystem;
        std::ostringstream prov;
        prov << "BioGame " << tr("lección") << ' ' << l.key << ", " << tr("idioma") << ' ' << currentLanguage();
        if (app.world && !culture_)
            prov << ", seed " << app.runPlate.seed;
        if (culture_)
            prov << ", seed " << culture_->spec().seed;
        const std::string path = (fs::path(app.exportsDir) / timestampedName("leccion_" + l.key)).string() + ".md";
        message_ = writeTextFile(path, lessonReport(l, progress_, prov.str())) ? path : tr("No se pudo exportar.");
    }
};

} // namespace

std::unique_ptr<Panel> makeLessonPanel() { return std::make_unique<LessonPanel>(); }

} // namespace ui
