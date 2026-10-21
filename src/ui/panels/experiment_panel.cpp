// Experimento (fase P1): réplicas con seeds distintas y un factor barrido, en segundo plano, con la
// media y su intervalo de confianza. Sale del cultivo elegido, de la placa en pantalla (su escena)
// o de un experimento guardado (.toml, el mismo formato que corre `microcosm_batch`).
#include "app/app.hpp"
#include "core/paths.hpp"
#include "game/experiment.hpp"
#include "game/export.hpp"
#include "game/scene.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace ui {

namespace {

const char* const kSources[] = {"Cultivo elegido", "Placa en pantalla", "Archivo (.toml)"};

std::string experimentsDir() { return userDataSubdir("experiments", "experimentos"); }

struct Job {
    std::thread thread;
    std::atomic<int> done{0}, total{1};
    std::atomic<bool> cancel{false}, finished{false};
    ExperimentResult result;
};

class ExperimentPanel : public Panel {
  public:
    ExperimentPanel() { open = false; } // se abre desde el menú Ver
    ~ExperimentPanel() override { stop(); }
    const char* key() const override { return "experiment"; }
    const char* title() const override { return "Experimento"; }
    Dock dock() const override { return Dock::Right; }

    void draw(App& app) override {
        const bool running = job_ && !job_->finished.load();
        ImGui::BeginDisabled(running);
        combo("Origen", &source_, kSources, 3,
              "De qué sale cada réplica: el cultivo elegido desde que se creó (con su programa y sus operaciones), "
              "la escena de la placa en pantalla, o un experimento guardado.");
        if (source_ == 2)
            drawFiles();
        sliderInt("Réplicas", &replicas_, 1, 30, "Corridas por nivel, cada una con su seed.");
        sliderLog("Duración [h]", &hours_, 0.1f, 2000.0f, "%.1f", "Horas de laboratorio (biológicas en la placa).");
        sliderLog("Muestreo [h]", &sample_, 0.01f, 48.0f, "%.2f");
        ImGui::InputText(tr("Factor"), factor_, sizeof factor_);
        tooltip("Opcional. Cultivo: temperature, light, bacteria, solute:<clave>, inoculate:<id>. Placa: temperature, "
                "solute:<clave> o una clave del registro de parámetros.");
        ImGui::InputText(tr("Valores"), values_, sizeof values_);
        tooltip("Separados por comas: un nivel por valor.");
        ImGui::EndDisabled();
        if (!running && ImGui::Button(tr("Correr"))) {
            ExperimentSpec spec;
            std::string error;
            if (build(app, spec, error))
                start(spec);
            else
                message_ = error;
        }
        if (running) {
            const int done = job_->done.load(), total = std::max(1, job_->total.load());
            bar("Avance", static_cast<float>(done) / total, (std::to_string(done) + "/" + std::to_string(total)).c_str());
            if (ImGui::Button(tr("Cancelar")))
                job_->cancel = true;
        }
        if (!message_.empty())
            ImGui::TextWrapped("%s", message_.c_str());
        if (job_ && job_->finished.load())
            drawResult(app, job_->result);
    }

  private:
    int source_ = 0, replicas_ = 5, column_ = 0, file_ = -1;
    float hours_ = 72.0f, sample_ = 1.0f;
    char factor_[64] = "", values_[128] = "";
    std::string message_;
    std::unique_ptr<Job> job_;
    std::vector<std::string> files_;

    void stop() {
        if (job_ && job_->thread.joinable()) {
            job_->cancel = true;
            job_->thread.join();
        }
    }

    void start(const ExperimentSpec& spec) {
        stop();
        job_ = std::make_unique<Job>();
        job_->total = static_cast<int>(experimentLevels(spec).size()) * std::max(1, spec.replicas);
        Job* job = job_.get();
        job->thread = std::thread([job, spec] {
            job->result = runExperiment(spec, [job](int d, int) { job->done = d; }, &job->cancel);
            job->finished = true;
        });
        message_.clear();
    }

    void drawFiles() {
        namespace fs = std::filesystem;
        if (ImGui::SmallButton(tr("Buscar experimentos"))) {
            files_.clear();
            std::error_code ec;
            if (fs::is_directory(experimentsDir(), ec))
                for (const auto& f : fs::directory_iterator(experimentsDir(), ec))
                    if (f.path().extension() == ".toml")
                        files_.push_back(f.path().string());
            std::sort(files_.begin(), files_.end());
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", experimentsDir().c_str());
        for (std::size_t i = 0; i < files_.size(); ++i)
            if (ImGui::Selectable(fs::path(files_[i]).filename().string().c_str(), file_ == static_cast<int>(i)))
                file_ = static_cast<int>(i);
    }

    bool build(App& app, ExperimentSpec& spec, std::string& error) {
        if (source_ == 2) {
            if (file_ < 0 || file_ >= static_cast<int>(files_.size())) {
                error = tr("Elegí un archivo.");
                return false;
            }
            std::ifstream in(files_[static_cast<std::size_t>(file_)]);
            std::stringstream text;
            text << in.rdbuf();
            if (!parseExperiment(text.str(), spec, &error))
                return false;
        } else if (source_ == 0) {
            if (app.cultureIndex < 0 || app.cultureIndex >= static_cast<int>(app.cultures.size())) {
                error = tr("No hay un cultivo elegido (panel Cultivos).");
                return false;
            }
            const CultureRun& run = *app.cultures[static_cast<std::size_t>(app.cultureIndex)];
            spec.kind = ExperimentSpec::Kind::Culture;
            spec.name = run.spec().name;
            spec.culture = run.spec();
            spec.culture.program = run.culture().program();
            spec.seed = run.spec().seed;
            for (const CultureEvent& e : run.events())
                if (e.op != CultureOp::Sample) // las gotas que se sacaron no son parte del protocolo
                    spec.cultureEvents.push_back(e);
        } else {
            if (!app.world) {
                error = tr("No hay placa.");
                return false;
            }
            spec.kind = ExperimentSpec::Kind::Plate;
            spec.name = app.plateName;
            spec.plateScene = serializeScene(app.runPlate, app.session.interventions().entries());
            spec.seed = app.runPlate.seed;
        }
        if (source_ != 2) {
            spec.replicas = replicas_;
            spec.hours = hours_;
            spec.sampleHours = sample_;
            spec.factors.clear();
            if (factor_[0]) {
                ExperimentFactor f;
                f.key = factor_;
                std::stringstream vs(values_);
                std::string tok;
                while (std::getline(vs, tok, ','))
                    try {
                        f.values.push_back(std::stod(tok));
                    } catch (...) {
                    }
                if (!f.values.empty())
                    spec.factors.push_back(f);
            }
        }
        return true;
    }

    void drawResult(App& app, const ExperimentResult& r) {
        ImGui::Separator();
        ImGui::Text("%s: %zu %s, %zu %s (%.1f s)", r.spec.name.c_str(), r.replicas.size(), tr("corridas"), r.levels.size(),
                    tr("niveles"), r.seconds);
        // Columnas con datos.
        std::vector<int> cols;
        std::vector<std::string> labels;
        for (std::size_t c = 0; c < r.columns.size(); ++c) {
            bool data = false;
            for (const ReplicaResult& rep : r.replicas)
                if (c < rep.values.size())
                    for (double v : rep.values[c])
                        data |= v != 0.0;
            if (!data)
                continue;
            cols.push_back(static_cast<int>(c));
            const std::optional<SpeciesId> id = findSpecies(r.columns[c]);
            labels.push_back(std::string(id ? tr(speciesInfo(*id).name) : r.columns[c].c_str()) +
                             (r.units[c].empty() ? "" : " [" + r.units[c] + "]"));
        }
        if (cols.empty())
            return;
        std::vector<const char*> items;
        for (const std::string& l : labels)
            items.push_back(l.c_str());
        column_ = std::clamp(column_, 0, static_cast<int>(items.size()) - 1);
        combo("Serie", &column_, items.data(), static_cast<int>(items.size()));
        const int c = cols[static_cast<std::size_t>(column_)];
        if (ImPlot::BeginPlot("##experimento", {-1.0f, 280.0f})) {
            ImPlot::SetupAxes(tr("tiempo [h]"), r.units[static_cast<std::size_t>(c)].c_str(), ImPlotAxisFlags_AutoFit,
                              ImPlotAxisFlags_AutoFit);
            const std::size_t n = r.times.size();
            std::vector<double> mean(n), lo(n), hi(n);
            for (std::size_t l = 0; l < r.levels.size(); ++l) {
                for (std::size_t s = 0; s < n; ++s) {
                    const SummaryPoint p = summarize(r, static_cast<int>(l), c, s);
                    mean[s] = p.mean;
                    lo[s] = p.mean - p.ci95;
                    hi[s] = p.mean + p.ci95;
                }
                const std::string label = r.levels[l].label;
                ImPlotSpec band;
                band.FillAlpha = 0.25f;
                ImPlot::PlotShaded((label + " IC 95 %").c_str(), r.times.data(), lo.data(), hi.data(), static_cast<int>(n), band);
                ImPlot::PlotLine(label.c_str(), r.times.data(), mean.data(), static_cast<int>(n));
            }
            ImPlot::EndPlot();
        }
        if (ImGui::Button(tr("Exportar"))) {
            namespace fs = std::filesystem;
            const std::string base = (fs::path(app.exportsDir) / timestampedName(r.spec.name + "_lote")).string();
            const bool ok = writeTextFile(base + "_largo.csv", experimentLongCsv(r)) &&
                            writeTextFile(base + "_resumen.csv", experimentSummaryCsv(r)) &&
                            writeTextFile(base + ".json", experimentJson(r));
            message_ = ok ? base + "_{largo,resumen}.csv, .json" : tr("No se pudo exportar.");
        }
        tooltip("Formato largo (cada réplica), resumen (media, DE e IC 95 % por nivel y tiempo) y JSON con la procedencia y las seeds.");
        ImGui::SameLine();
        if (ImGui::Button(tr("Guardar experimento"))) {
            const std::string path = (std::filesystem::path(experimentsDir()) / (r.spec.name + ".toml")).string();
            message_ = writeTextFile(path, serializeExperiment(r.spec)) ? path : tr("No se pudo guardar.");
        }
        tooltip("El experimento como archivo TOML: lo corre este panel o `microcosm_batch` sin ventana.");
    }
};

} // namespace

std::unique_ptr<Panel> makeExperimentPanel() { return std::make_unique<ExperimentPanel>(); }

} // namespace ui
