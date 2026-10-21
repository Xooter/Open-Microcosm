// Cultivos (fase P1): la gradilla de tubos y frascos bien mezclados. Cada uno corre en su propio
// tiempo (horas por segundo real) con la misma fisiología que la placa; se mira sacando una gota de
// muestra, que pasa a ser la placa del centro.
#include "app/app.hpp"
#include "game/culture_run.hpp"
#include "game/export.hpp"
#include "game/prepared.hpp"
#include "game/reagent.hpp"
#include "ui/i18n.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ui {

namespace {

const char* const kShapes[] = {"Rampa", "Ciclo", "Seno", "Evento"};

class CulturePanel : public Panel {
  public:
    CulturePanel() { open = false; } // se abre desde el menú Ver
    const char* key() const override { return "cultures"; }
    const char* title() const override { return "Cultivos"; }
    Dock dock() const override { return Dock::Right; }

    void draw(App& app) override {
        drawPicker(app);
        if (app.cultureIndex < 0 || app.cultureIndex >= static_cast<int>(app.cultures.size())) {
            note("Un cultivo es un tubo o un frasco de mililitros bien mezclado (como los de Gause o Tilman): las "
                 "poblaciones no caben en la placa y se simulan sin espacio, con la misma fisiología. Para mirarlo, "
                 "se saca una gota.");
            return;
        }
        CultureRun& run = *app.cultures[static_cast<std::size_t>(app.cultureIndex)];
        drawState(app, run);
        if (section("Sembrar", true))
            drawSeeding(run);
        if (section("Medio"))
            drawMedium(run);
        if (section("Programa"))
            drawProgram(run);
        if (section("Muestra", true))
            drawSample(app, run);
        if (section("Pasaje y diluciones seriadas"))
            drawPassage(app);
        if (section("Gráfico", true))
            drawPlot(run);
        if (section("Guardar y exportar"))
            drawFiles(app, run);
    }

  private:
    // Formulario de un cultivo nuevo.
    CultureSpec draft_;
    int vessel_ = -1, medium_ = -1;
    bool creating_ = false;
    // Siembra.
    int species_ = 0;
    int inoculum_ = 5;
    float bacteriaPerMl_ = 1e7f, phagesPerMl_ = 1e5f;
    float renew_ = 1.0f, dilute_ = 0.1f, gasTransfer_ = 2.07f;
    int solute_ = 2, reagent_ = 0;
    float dose_ = 8.0f;
    float soluteValue_ = 10.0f;
    // Programa.
    ProgramEntry entry_;
    char target_[64] = "light";
    // Muestra.
    float aliquotMl_ = 0.1f, concentrate_ = 1.0f;
    int countSpecies_ = 0, lastCount_ = -1;
    std::uint64_t draws_ = 0;
    std::string message_;
    bool logScale_ = true;
    bool byDensity_ = false;
    float densityPerMl_ = 100.0f;
    int dest_ = 0, serial_ = 3;
    float transferMl_ = 0.05f;

    void drawPassage(App& app) {
        CultureRun& run = *app.cultures[static_cast<std::size_t>(app.cultureIndex)];
        std::vector<int> others;
        std::vector<const char*> names;
        for (std::size_t i = 0; i < app.cultures.size(); ++i)
            if (static_cast<int>(i) != app.cultureIndex) {
                others.push_back(static_cast<int>(i));
                names.push_back(app.cultures[i]->spec().name.c_str());
            }
        sliderLog("Volumen [mL]", &transferMl_, 0.001f, std::max(0.002f, static_cast<float>(run.culture().volumeMl())), "%.3f",
                  "Lo que se pasa: células con su estado, bacterias y medio.");
        if (!others.empty()) {
            dest_ = std::clamp(dest_, 0, static_cast<int>(others.size()) - 1);
            combo("A", &dest_, names.data(), static_cast<int>(names.size()));
            if (ImGui::Button(tr("Pasar")))
                run.transferTo(*app.cultures[static_cast<std::size_t>(others[static_cast<std::size_t>(dest_)])], transferMl_);
        }
        sliderInt("Diluciones 1:10", &serial_, 1, 8,
                  "Tubos nuevos con el mismo medio: cada uno recibe la novena parte de su volumen del anterior (1:10).");
        if (ImGui::Button(tr("Dilución seriada"))) {
            CultureRun* from = &run;
            const std::string base = run.spec().name;
            for (int k = 1; k <= serial_; ++k) {
                CultureSpec spec = run.spec();
                spec.program = Program{};
                spec.name = base + " 1e-" + std::to_string(k);
                spec.seed = run.spec().seed * 31u + static_cast<std::uint64_t>(k);
                auto next = std::make_unique<CultureRun>();
                next->start(spec);
                from->transferTo(*next, next->culture().volumeMl() / 9.0);
                app.cultures.push_back(std::move(next));
                from = app.cultures.back().get();
            }
        }
    }

    void drawPicker(App& app) {
        std::vector<std::string> names;
        for (const auto& c : app.cultures)
            names.push_back(c->spec().name);
        std::vector<const char*> items;
        for (const std::string& n : names)
            items.push_back(n.c_str());
        if (!items.empty()) {
            int idx = std::clamp(app.cultureIndex, 0, static_cast<int>(items.size()) - 1);
            if (combo("Cultivo", &idx, items.data(), static_cast<int>(items.size())))
                app.cultureIndex = idx;
            app.cultureIndex = idx;
        }
        if (ImGui::Button(tr(creating_ ? "Cancelar" : "Nuevo cultivo...")))
            creating_ = !creating_;
        ImGui::SameLine();
        if (ImGui::Button(tr(app.culturesPaused ? "Reanudar" : "Pausar")))
            app.culturesPaused = !app.culturesPaused;
        sliderLog("Velocidad [h/s]", &app.cultureSpeed, 0.05f, 200.0f, "%.2f",
                  "Horas de cultivo por segundo real (todos los cultivos). No cambia los resultados: el paso es fijo.");
        if (creating_)
            drawNewCulture(app);
        ImGui::Separator();
    }

    void drawNewCulture(App& app) {
        std::vector<const char*> vessels, media;
        for (int i = 0; i < vesselCount(); ++i)
            vessels.push_back(vessel(i).name.c_str());
        for (int i = 0; i < cultureMediumCount(); ++i)
            media.push_back(cultureMedium(i).name.c_str());
        if (vessel_ < 0)
            for (int i = 0; i < vesselCount(); ++i)
                if (vessel(i).key == draft_.vessel)
                    vessel_ = i;
        if (medium_ < 0)
            for (int i = 0; i < cultureMediumCount(); ++i)
                if (cultureMedium(i).key == draft_.medium)
                    medium_ = i;
        static char name[64] = "cultivo";
        ImGui::InputText(tr("Nombre"), name, sizeof name);
        if (!vessels.empty()) {
            vessel_ = std::clamp(vessel_, 0, static_cast<int>(vessels.size()) - 1);
            combo("Recipiente", &vessel_, vessels.data(), static_cast<int>(vessels.size()),
                  "Fija el volumen y la profundidad del medio (intercambio de gases y luz).");
        }
        if (!media.empty()) {
            medium_ = std::clamp(medium_, 0, static_cast<int>(media.size()) - 1);
            combo("Medio", &medium_, media.data(), static_cast<int>(media.size()));
            tooltip(cultureMedium(medium_).description.c_str());
        }
        slider("Temperatura [°C]", &draft_.temperature, 0.0f, 40.0f, "%.1f");
        slider("Luz", &draft_.light, 0.0f, 1.0f, "%.2f", "Irradiancia relativa de la lámpara sobre el cultivo.");
        bool vesselMixing = draft_.gasTransfer < 0.0f;
        if (checkbox("Mezcla del recipiente", &vesselMixing,
                     "La transferencia de gases con el aire que trae el recipiente. Sin marcar, se elige a mano."))
            draft_.gasTransfer = vesselMixing ? -1.0f
                                              : (vessel_ >= 0 && vessel_ < vesselCount() ? vessel(vessel_).gasTransferCmH : 0.0f);
        if (!vesselMixing)
            gasTransferSlider(&draft_.gasTransfer);
        int seed = static_cast<int>(draft_.seed);
        if (ImGui::InputInt(tr("Seed"), &seed))
            draft_.seed = static_cast<std::uint64_t>(std::max(0, seed));
        if (ImGui::Button(tr("Crear cultivo"))) {
            draft_.name = name;
            if (vessel_ >= 0 && vessel_ < vesselCount())
                draft_.vessel = vessel(vessel_).key;
            if (medium_ >= 0 && medium_ < cultureMediumCount())
                draft_.medium = cultureMedium(medium_).key;
            auto run = std::make_unique<CultureRun>();
            run->start(draft_);
            app.cultures.push_back(std::move(run));
            app.cultureIndex = static_cast<int>(app.cultures.size()) - 1;
            creating_ = false;
        }
    }

    void drawState(App&, CultureRun& run) {
        const Culture& c = run.culture();
        const CultureCensus census = c.census();
        readout("Tiempo", "%.1f h (%.2f días)", run.hours(), run.hours() / 24.0);
        readout("Volumen", "%.3f mL a %.1f °C", c.volumeMl(), c.medium().temperature);
        readout("Bacterias", "%.3g por mL", static_cast<double>(census.bacteria) / std::max(1e-9, c.volumeMl()));
        if (c.phages() > 0.0)
            readout("Fagos", "%.3g por mL", c.phages() / std::max(1e-9, c.volumeMl()));
        readout("pH y luz", "%.2f | %.2f", c.ph(), c.light());
        if (ImGui::BeginTable("##pop", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn(tr("Ser"));
            ImGui::TableSetupColumn(tr("Individuos"));
            ImGui::TableSetupColumn(tr("Por mL"));
            ImGui::TableHeadersRow();
            for (int s = 0; s < speciesCount(); ++s) {
                if (census.species[s] == 0)
                    continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(tr(speciesInfo(static_cast<SpeciesId>(s)).name));
                ImGui::TableNextColumn();
                ImGui::Text("%d", census.species[s]);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", census.species[s] / std::max(1e-9, c.volumeMl()));
            }
            ImGui::EndTable();
        }
        if (census.cysts > 0)
            ImGui::TextDisabled("%d %s", census.cysts, tr("en quiste"));
    }

    // Especies que se pueden sembrar en un cultivo bien mezclado.
    static std::vector<int> supportedSpecies() {
        std::vector<int> out;
        for (int s = 0; s < speciesCount(); ++s)
            if (Culture::supports(static_cast<SpeciesId>(s)))
                out.push_back(s);
        return out;
    }

    void drawSeeding(CultureRun& run) {
        const std::vector<int> ids = supportedSpecies();
        std::vector<const char*> names;
        for (int s : ids)
            names.push_back(tr(speciesInfo(static_cast<SpeciesId>(s)).name));
        if (!names.empty()) {
            species_ = std::clamp(species_, 0, static_cast<int>(names.size()) - 1);
            combo("Ser", &species_, names.data(), static_cast<int>(names.size()));
            const char* id = speciesInfo(static_cast<SpeciesId>(ids[static_cast<std::size_t>(species_)])).key;
            checkbox("Por densidad", &byDensity_,
                     "Individuos por mL de cultivo: la cantidad es un sorteo de Poisson (un inóculo real).");
            if (byDensity_) {
                sliderLog("Densidad [por mL]", &densityPerMl_, 0.1f, 1e6f, "%.3g");
                ImGui::TextDisabled("%s %.1f", tr("Esperados:"), densityPerMl_ * run.culture().volumeMl());
                if (ImGui::Button(tr("Inocular")))
                    run.apply({0, CultureOp::InoculateDensity, id, densityPerMl_});
            } else {
                sliderInt("Individuos", &inoculum_, 1, 500);
                if (ImGui::Button(tr("Inocular")))
                    run.apply({0, CultureOp::Inoculate, id, static_cast<double>(inoculum_)});
            }
        }
        sliderLog("Bacterias [por mL]", &bacteriaPerMl_, 1e3f, 1e10f, "%.2e",
                  "Alimento: una suspensión de bacterias (Gause agregaba una cantidad fija por día).");
        if (ImGui::Button(tr("Agregar bacterias")))
            run.apply({0, CultureOp::Bacteria, "", bacteriaPerMl_});
        sliderLog("Fagos [por mL]", &phagesPerMl_, 1e2f, 1e10f, "%.2e");
        if (ImGui::Button(tr("Agregar fagos")))
            run.apply({0, CultureOp::Phages, "", phagesPerMl_});
    }

    void drawMedium(CultureRun& run) {
        slider("Fracción a renovar", &renew_, 0.0f, 1.0f, "%.2f",
               "Cambio de medio: se centrifuga, se tira el sobrenadante (con las bacterias y lo disuelto) y se agrega "
               "medio nuevo. Los protistas se quedan.");
        if (ImGui::Button(tr("Cambiar el medio")))
            run.apply({0, CultureOp::Renew, "", renew_});
        slider("Fracción a diluir", &dilute_, 0.0f, 1.0f, "%.2f",
               "Raleo o dilución: se revuelve, se saca esa fracción del cultivo con todo lo que tiene y se repone con "
               "medio nuevo. Gause sacaba y descartaba la décima parte cada día: eso es lo que desplaza a la especie "
               "que crece más lento.");
        if (ImGui::Button(tr("Diluir")))
            run.apply({0, CultureOp::Dilute, "", dilute_});
        readout("Transferencia de gases", "%.2f cm/h", static_cast<double>(run.culture().gasTransfer()));
        gasTransferSlider(&gasTransfer_);
        if (ImGui::Button(tr("Cambiar la mezcla")))
            run.apply({0, CultureOp::GasTransfer, "", gasTransfer_});
        const char* solutes[kSoluteCount];
        for (int k = 0; k < kSoluteCount; ++k)
            solutes[k] = soluteLabel(static_cast<Solute>(k));
        combo("Soluto", &solute_, solutes, kSoluteCount);
        sliderLog("Concentración [µM]", &soluteValue_, 1e-4f, 1e5f, "%.3g");
        if (ImGui::Button(tr("Fijar concentración")))
            run.apply({0, CultureOp::Concentration, soluteKey(static_cast<Solute>(solute_)), soluteValue_});
        if (reagentCount() > 0) {
            std::vector<const char*> names;
            for (int i = 0; i < reagentCount(); ++i)
                names.push_back(tr(reagent(i).name.c_str()));
            reagent_ = std::clamp(reagent_, 0, reagentCount() - 1);
            combo("Reactivo", &reagent_, names.data(), reagentCount());
            tooltip(tr(reagent(reagent_).description.c_str()));
            sliderLog("Dosis [µg/mL]", &dose_, 0.01f, 1000.0f, "%.3g",
                      "Una dosis-respuesta se mide como en el laboratorio: un lote (panel Experimento) con el factor "
                      "reagent:<id> y una serie de dosis al doble.");
            ImGui::TextDisabled("%s %.3g µg/mL", tr("Ahora:"), run.culture().reagentConcentration(reagent_));
            if (ImGui::Button(tr("Fijar la dosis")))
                run.apply({0, CultureOp::Reagent, reagent(reagent_).key, dose_});
        }
    }

    void drawProgram(CultureRun& run) {
        Program& program = run.culture().program();
        for (std::size_t i = 0; i < program.entries.size(); ++i) {
            const ProgramEntry& e = program.entries[i];
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::SmallButton("x")) {
                program.entries.erase(program.entries.begin() + static_cast<std::ptrdiff_t>(i));
                run.spec().program = program;
                ImGui::PopID();
                break;
            }
            ImGui::SameLine();
            ImGui::Text("%s: %s, %s %.1f h, %.3g→%.3g, %s %.3g", e.target.c_str(), tr(kShapes[static_cast<int>(e.shape)]),
                        tr("desde"), e.start, e.from, e.to, tr("período"), e.period);
            ImGui::PopID();
        }
        if (ImGui::SmallButton(tr("Fotoperíodo 16:8")))
            add(run, {"light", ProgramShape::Cycle, 0.0, kProgramForever, 0.0f, 1.0f, 24.0f, 16.0f / 24.0f, 0.0f});
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Protocolo de Gause")))
            gause(run);
        tooltip("Cada 24 h se saca y se descarta la décima parte del cultivo (la muestra que se cuenta), se centrifuga, se "
                "cambia todo el medio y se agregan bacterias frescas (Gause 1934, cap. V).");
        ImGui::InputText(tr("Objetivo"), target_, sizeof target_);
        tooltip("temperature, light, solute:<phosphate|nitrogen|silicate|iron|b12|organic|co2>, o una acción: bacteria, "
                "medium, dilution, inoculate:<id del ser>.");
        int shape = static_cast<int>(entry_.shape);
        if (combo("Forma", &shape, kShapes, 4))
            entry_.shape = static_cast<ProgramShape>(shape);
        float start = static_cast<float>(entry_.start);
        if (slider("Desde [h]", &start, 0.0f, 500.0f, "%.1f"))
            entry_.start = start;
        slider("Valor inicial / bajo", &entry_.from, -10.0f, 100.0f, "%.3g");
        slider("Valor final / alto", &entry_.to, -10.0f, 100.0f, "%.3g");
        sliderLog("Período [h]", &entry_.period, 0.1f, 500.0f, "%.2f");
        if (entry_.shape == ProgramShape::Cycle)
            slider("Fracción encendida", &entry_.duty, 0.0f, 1.0f, "%.2f");
        if (entry_.shape == ProgramShape::Event)
            sliderLog("Cantidad", &entry_.amount, 1e-3f, 1e10f, "%.3g");
        if (ImGui::Button(tr("Agregar al programa"))) {
            ProgramEntry e = entry_;
            e.target = target_;
            add(run, e);
        }
    }

    static void add(CultureRun& run, const ProgramEntry& e) {
        run.culture().program().entries.push_back(e);
        run.spec().program = run.culture().program();
    }

    // Mezcla del medio: velocidad de transferencia de gases por la superficie (0: medio quieto).
    static void gasTransferSlider(float* cmPerHour) {
        slider("Transferencia de gases [cm/h]", cmPerHour, 0.0f, 30.0f, "%.2f",
               "0: medio quieto, el O2 difunde por toda la columna (lento en un tubo hondo). Con el medio mezclado (nadadores, "
               "agitación) entra por la película de la superficie: ~2 cm/h en agua quieta sin viento (Cole y Caraco 1998); "
               "más con agitación.");
    }

    static void gause(CultureRun& run) {
        // La muestra contada y destruida (la décima parte) sale antes del cambio de medio.
        add(run, {"dilution", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 0.1f});
        add(run, {"medium", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 1.0f});
        add(run, {"bacteria", ProgramShape::Event, 24.0, kProgramForever, 0.0f, 0.0f, 24.0f, 0.5f, 5e7f});
    }

    void drawSample(App& app, CultureRun& run) {
        const Culture& c = run.culture();
        // Conteo de una alícuota (como Gause, que contaba 0,1 cm³ por día).
        std::vector<int> present;
        std::vector<const char*> names;
        for (int s = 0; s < speciesCount(); ++s)
            if (c.count(static_cast<SpeciesId>(s)) > 0) {
                present.push_back(s);
                names.push_back(tr(speciesInfo(static_cast<SpeciesId>(s)).name));
            }
        if (!present.empty()) {
            countSpecies_ = std::clamp(countSpecies_, 0, static_cast<int>(present.size()) - 1);
            combo("Contar", &countSpecies_, names.data(), static_cast<int>(names.size()));
            sliderLog("Alícuota [mL]", &aliquotMl_, 0.001f, std::max(0.002f, static_cast<float>(c.volumeMl())), "%.3f");
            if (ImGui::Button(tr("Contar la alícuota")))
                lastCount_ = run.countAliquot(static_cast<SpeciesId>(present[static_cast<std::size_t>(countSpecies_)]), aliquotMl_,
                                              draws_++);
            if (lastCount_ >= 0) {
                ImGui::SameLine();
                ImGui::Text("%d (± %.1f, Poisson) → %.1f %s", lastCount_, std::sqrt(static_cast<double>(lastCount_)),
                            lastCount_ / aliquotMl_, tr("por mL"));
            }
        }
        // Gota de muestra: lo que se ve en el campo de la placa.
        PlateSpec preview;
        preview.content = DropContent::EmptyPetri;
        preview.rebuild();
        const double ul = CultureRun::dropVolumeUl(preview.config);
        sliderLog("Concentrar (x)", &concentrate_, 1.0f, 1000.0f, "%.0f",
                  "Centrifugado suave antes de sacar la gota: a baja densidad, una gota sin concentrar casi nunca trae "
                  "un protista.");
        double expected = 0.0;
        for (int s : present)
            expected += c.count(static_cast<SpeciesId>(s));
        expected *= std::min(1.0, ul * 1e-3 * concentrate_ / std::max(1e-9, c.volumeMl()));
        ImGui::TextDisabled("%s %.3f µL: %s %.1f %s", tr("Gota de"), ul, tr("se esperan"), expected, tr("protistas"));
        if (ImGui::Button(tr("Sacar una gota y mirarla"))) {
            app.openSampleDrop(run.sampleDrop(concentrate_));
            message_ = tr("La gota es la placa del centro.");
        }
        if (!message_.empty())
            ImGui::TextDisabled("%s", message_.c_str());
    }

    void drawPlot(CultureRun& run) {
        const TimeSeries& s = run.series();
        const int t = s.find("t");
        if (t < 0 || s.rows() < 2) {
            note("Sin muestras todavía.");
            return;
        }
        checkbox("Escala log", &logScale_);
        if (ImPlot::BeginPlot("##cultivo", {-1.0f, 260.0f})) {
            ImPlot::SetupAxes(tr("tiempo [h]"), tr("por mL"), ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            if (logScale_)
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            const std::vector<float>& x = s.column(static_cast<std::size_t>(t));
            for (std::size_t c = 0; c < s.columns(); ++c) {
                if (s.info(c).group != "Poblacion")
                    continue;
                const std::vector<float>& y = s.column(c);
                if (std::none_of(y.begin(), y.end(), [](float v) { return v > 0.0f; }))
                    continue;
                const std::optional<SpeciesId> id = findSpecies(s.info(c).name);
                ImPlot::PlotLine(id ? tr(speciesInfo(*id).name) : tr(s.info(c).name.c_str()), x.data(), y.data(),
                                 static_cast<int>(x.size()));
            }
            ImPlot::EndPlot();
        }
    }

    void drawFiles(App& app, CultureRun& run) {
        namespace fs = std::filesystem;
        const fs::path dir = defaultCulturesDir();
        if (ImGui::Button(tr("Guardar escena de cultivo"))) {
            std::error_code ec;
            fs::create_directories(dir, ec);
            const std::string path = (dir / (run.spec().name + ".culture")).string();
            message_ = writeTextFile(path, serializeCultureScene(run.spec(), run.events())) ? path : tr("No se pudo guardar.");
        }
        tooltip("El cultivo como se preparó, su programa y todo lo que se le hizo con su paso: cargarlo lo repite.");
        ImGui::SameLine();
        if (ImGui::Button(tr("Exportar CSV"))) {
            std::error_code ec;
            fs::create_directories(app.exportsDir, ec);
            const std::string path = (fs::path(app.exportsDir) / timestampedName(run.spec().name + "_cultivo")).string() + ".csv";
            message_ = writeTextFile(path, seriesCsv(run.series())) ? path : tr("No se pudo exportar.");
        }
        // Escenas guardadas.
        std::error_code ec;
        if (fs::is_directory(dir, ec))
            for (const auto& f : fs::directory_iterator(dir, ec)) {
                // ".cultivo": las guardadas antes de que los archivos pasaran al inglés.
                if (f.path().extension() != ".culture" && f.path().extension() != ".cultivo")
                    continue;
                if (ImGui::SmallButton((std::string(tr("Cargar")) + " " + f.path().stem().string()).c_str())) {
                    std::ifstream in(f.path());
                    std::stringstream text;
                    text << in.rdbuf();
                    CultureSpec spec;
                    std::vector<CultureEvent> events;
                    if (parseCultureScene(text.str(), spec, events)) {
                        auto r = std::make_unique<CultureRun>();
                        r->start(spec);
                        r->schedule(events);
                        app.cultures.push_back(std::move(r));
                        app.cultureIndex = static_cast<int>(app.cultures.size()) - 1;
                    }
                }
            }
    }
};

} // namespace

std::unique_ptr<Panel> makeCulturePanel() { return std::make_unique<CulturePanel>(); }

} // namespace ui
