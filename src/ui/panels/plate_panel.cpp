// Editor de placa: preconfigura la placa de Petri (bioma, seed, contenido, ambiente, nutrientes,
// sedimento y poblaciones). Lo que se puede cambiar con la placa en marcha (ambiente y
// nutrientes) se aplica en vivo; el resto se aplica al crear la placa.
#include "app/app.hpp"
#include "game/scene.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

#include <algorithm>
#include <random>

namespace ui {

namespace {

constexpr const char* kContents[] = {"Completa", "Sin vida", "Placa vacía"};
constexpr const char* kContentNotes[] = {"Todas las poblaciones, el sedimento y el polvo del bioma.",
                                         "Solo agua, química y sedimento: sin vida (agregala desde el catálogo).",
                                         "Placa limpia: sin vida, minerales, polvo ni detritos. Solo agua y química."};
constexpr const char* kGrids[] = {"128", "256", "512"};

class PlatePanel : public Panel {
  public:
    const char* key() const override { return "placa"; }
    const char* title() const override { return "Placa"; }
    Dock dock() const override { return Dock::Left; }

    void draw(App& app) override {
        PlateSpec& p = app.plate;
        const bool dirty = app.plateDirty;
        if (dirty)
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.45f, 0.15f, 1.0f});
        if (ImGui::Button(tr("Crear placa (Ctrl+N)"), {-FLT_MIN, 32.0f}))
            app.createPlate();
        if (dirty)
            ImGui::PopStyleColor();
        tooltip("Crea el mundo con esta configuración. La placa en pantalla se descarta.");
        checkbox("Aplicar en vivo", &app.livePlate,
                 "Ambiente y nutrientes se aplican a la placa en marcha apenas se cambian; lo demás, al crearla.");
        if (dirty)
            note("Hay cambios que se aplican al crear la placa.");
        ImGui::Spacing();

        drawBase(app, p);
        if (section("Ambiente (en vivo)", true))
            drawEnvironment(app, p);
        if (section("Nutrientes (en vivo)", true))
            drawNutrients(app, p);
        if (section("Sedimento (al crear)"))
            drawSediment(app, p);
        if (section("Poblaciones (al crear)"))
            drawPopulations(app, p);
        if (section("Placa y grilla (al crear)"))
            drawGeometry(app, p);
        if (section("Guardar y cargar", true))
            drawFiles(app, p);
    }

  private:
    static void dirtyIf(App& app, bool changed) { app.plateDirty |= changed; }

    void drawBase(App& app, PlateSpec& p) {
        if (!section("Base", true))
            return;
        std::vector<const char*> names;
        for (int b = 0; b < kBiomeCount; ++b)
            names.push_back(biome(static_cast<BiomeId>(b)).name);
        int b = p.biome;
        if (combo("Bioma", &b, names.data(), kBiomeCount, "Ambiente de partida: química, temperatura y elenco de especies.")) {
            p.biome = b;
            p.rebuild(); // cambia todo lo derivado del bioma
            app.plateDirty = true;
        }
        note(biome(static_cast<BiomeId>(p.biome)).description);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Semilla"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-60.0f);
        if (ImGui::InputScalar("##seed", ImGuiDataType_U64, &p.seed)) {
            p.rebuild();
            app.plateDirty = true;
        }
        tooltip("La misma semilla y el mismo bioma dan siempre la misma gota.");
        ImGui::SameLine();
        if (ImGui::Button(tr("Otra"))) {
            p.seed = std::random_device{}() & 0x7fffffffu;
            p.rebuild();
            app.plateDirty = true;
        }
        int c = static_cast<int>(p.content);
        if (combo("Contenido", &c, kContents, 3)) {
            p.content = static_cast<DropContent>(c);
            p.rebuild();
            app.plateDirty = true;
        }
        note(kContentNotes[static_cast<int>(p.content)]);
        note("Cambiar bioma, semilla o contenido restablece los ajustes de abajo.");
    }

    void drawEnvironment(App& app, PlateSpec& p) {
        MediumParams& m = p.config.medium;
        bool changed = false;
        changed |= slider("Temperatura [°C]", &m.temperature, -2.0f, 60.0f, "%.1f",
                          "Q10 = 2 en todas las tasas biológicas; cambia la viscosidad y el O2 de saturación.");
        changed |= slider("Salinidad [PSU]", &m.salinity, 0.0f, 100.0f, "%.1f", "Agua dulce < 1; mar ~35; salmuera hasta ~100.");
        changed |= slider("Luz", &m.surfaceLight, 0.0f, 1.0f, "%.2f", "Irradiancia relativa de la lámpara.");
        changed |= slider("Intercambio O2 [1/s]", &m.o2AirExchange, 0.0f, 0.5f, "%.3f", "Reposición de oxígeno desde el aire.");
        changed |= sliderLog("Alcalinidad [µM]", &m.alkalinity, 50.0f, 5000.0f, "%.0f", "Bicarbonato: fija el pH junto con el CO2.");
        changed |= checkbox("Anóxico (sin O2 del aire)", &m.anoxic);
        if (changed) {
            if (app.livePlate)
                app.pushMediumLive();
            else
                app.plateDirty = true;
        }
    }

    void nutrient(App& app, const char* label, int index, float* value, float lo, float hi, const char* fmt,
                  const char* help, float shown = 1.0f) {
        if (sliderLog(label, value, lo, hi, fmt, help, shown)) {
            if (app.livePlate)
                app.pushNutrientLive(index, *value);
            else
                app.plateDirty = true;
        }
    }

    void drawNutrients(App& app, PlateSpec& p) {
        MediumParams& m = p.config.medium;
        nutrient(app, "Fosfato [µM]", 0, &m.phosphate, 0.1f, 2000.0f, "%.1f", "Limita a las algas cuando florecen.");
        nutrient(app, "Nitrógeno [µM]", 1, &m.nitrogen, 1.0f, 10000.0f, "%.0f", "Con N:P < 16 el agua queda limitada por N: favorece a las cianobacterias fijadoras.");
        nutrient(app, "Sílice [µM]", 2, &m.silicate, 1.0f, 1000.0f, "%.0f", "Las diatomeas la necesitan para cada valva.");
        nutrient(app, "Hierro [µM]", 3, &m.iron, 0.01f, 50.0f, "%.2f", "Limita a los fotótrofos en el mar abierto.");
        nutrient(app, "Vitamina B12 [pM]", 4, &m.vitamin, 1e-5f, 0.1f, "%.1f", "Euglena y las diatomeas no la sintetizan: la aportan las bacterias.", 1e6f);
        note("Llevan la concentración media de toda la placa al valor elegido.");
    }

    void drawSediment(App& app, PlateSpec& p) {
        Sediment& s = p.config.sediment;
        bool c = false;
        c |= sliderInt("Arcilla", &s.clay, 0, 600);
        c |= sliderInt("Limo de cuarzo", &s.quartz, 0, 600);
        c |= sliderInt("Mica", &s.mica, 0, 600);
        c |= sliderInt("Óxido de hierro", &s.ironOxide, 0, 600);
        c |= sliderInt("Azufre", &s.sulfur, 0, 600);
        c |= sliderInt("Frústulas vacías", &s.frustules, 0, 600);
        c |= sliderInt("Granos de arena", &s.rocks, 0, 40);
        c |= slider("Grano mín. [µm]", &s.rockMin, 20.0f, 250.0f, "%.0f");
        c |= slider("Grano máx. [µm]", &s.rockMax, 20.0f, 400.0f, "%.0f");
        s.rockMax = std::max(s.rockMax, s.rockMin);
        dirtyIf(app, c);
    }

    void drawPopulations(App& app, PlateSpec& p) {
        WorldConfig& c = p.config;
        bool ch = false;
        float bacteria = static_cast<float>(c.bacteria);
        if (sliderLog("Bacterias", &bacteria, 100.0f, 200000.0f, "%.0f")) {
            c.bacteria = static_cast<std::size_t>(bacteria);
            ch = true;
        }
        float detritus = static_cast<float>(c.detritus);
        if (slider("Detritos", &detritus, 0.0f, 500.0f, "%.0f")) {
            c.detritus = static_cast<std::size_t>(detritus);
            ch = true;
        }
        ch |= sliderInt("Anabaena (filamentos)", &c.cyanoFilaments, 0, 80);
        ch |= sliderInt("Oscillatoria (tricomas)", &c.oscillatoriaFilaments, 0, 80);
        ch |= sliderInt("Saprolegnia", &c.saprolegnia, 0, 30);
        ch |= sliderInt("Quitridios (grupos)", &c.chytridClusters, 0, 30);
        ch |= sliderInt("Allomyces", &c.allomyces, 0, 30);
        ch |= sliderInt("Hongos liquenizantes", &c.lichens, 0, 30);
        ch |= sliderInt("Physarum", &c.physarum, 0, 10);
        ch |= slider("Manchas de materia orgánica", &c.patchiness, 0.0f, 1.0f, "%.2f");
        ImGui::Separator();
        ImGui::TextUnformatted(tr("Peso inicial de cada especie"));
        helpMarker("Multiplica la población que le toca al bioma. 0 = ausente. Solo vale con contenido Completa.");
        for (int cat = 0; cat < kCategoryCount; ++cat) {
            bool any = false;
            for (int i = 0; i < kSpeciesCount; ++i)
                any |= static_cast<int>(categoryOf(beingAt(i))) == cat;
            if (!any || !ImGui::TreeNode(tr(categoryName(static_cast<BeingCategory>(cat)))))
                continue;
            for (int i = 0; i < kSpeciesCount; ++i)
                if (static_cast<int>(categoryOf(beingAt(i))) == cat)
                    ch |= slider(speciesInfo(static_cast<SpeciesId>(i)).name, &c.speciesWeight[i], 0.0f, 5.0f, "%.2f");
            ImGui::TreePop();
        }
        dirtyIf(app, ch);
    }

    void drawGeometry(App& app, PlateSpec& p) {
        WorldConfig& c = p.config;
        bool ch = false;
        ch |= checkbox("Placa circular de vidrio", &c.dish, "Sin placa, el mundo es toroidal (los bordes se envuelven).");
        ch |= slider("Margen de vidrio [µm]", &c.dishMargin, 20.0f, 300.0f, "%.0f");
        int g = c.gridX == 128 ? 0 : c.gridX == 512 ? 2 : 1;
        if (combo("Grilla (celdas)", &g, kGrids, 3, "Potencia de 2 (la FFT lo necesita). Más grande = placa más grande y más lenta.")) {
            c.gridX = c.gridY = g == 0 ? 128 : g == 2 ? 512 : 256;
            ch = true;
        }
        ch |= slider("Celda [µm]", &c.cellSize, 2.0f, 16.0f, "%.1f", "La placa mide grilla × celda.");
        readout("Lado", "%.0f µm", static_cast<float>(c.gridX) * c.cellSize);
        dirtyIf(app, ch);
    }

    void drawFiles(App& app, PlateSpec& p) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Nombre"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-70.0f);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%s", app.plateName.c_str());
        if (ImGui::InputText("##name", buf, sizeof buf))
            app.plateName = buf;
        ImGui::SameLine();
        const bool ok = !app.plateName.empty();
        ImGui::BeginDisabled(!ok);
        if (ImGui::Button(tr("Guardar")))
            savePlate(p, app.platesDir + "/" + app.plateName + ".plate");
        ImGui::EndDisabled();
        // Escena: la placa en pantalla con todo lo hecho (seres puestos, gotas, cambios de
        // parámetros), cada cosa en su paso. Al cargarla y crear la placa, se repite.
        ImGui::BeginDisabled(!ok || !app.world);
        if (ImGui::Button(tr("Guardar escena (con lo hecho)"), {-1.0f, 0.0f}))
            saveScene(app.platesDir + "/" + app.plateName + ".plate", app.runPlate, app.session.interventions().entries());
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", tr("Guarda la placa en pantalla y cada intervención con su paso: al cargarla y crear "
                                       "la placa, la corrida se repite."));
        const std::vector<std::string> saved = listPlates(app.platesDir);
        if (saved.empty()) {
            note("Todavía no hay placas guardadas.");
            return;
        }
        ImGui::TextUnformatted(tr("Placas guardadas"));
        for (const std::string& name : saved) {
            ImGui::PushID(name.c_str());
            if (ImGui::Selectable(name.c_str())) {
                if (loadScene(app.platesDir + "/" + name + ".plate", p, app.plateScript)) {
                    app.plateName = name;
                    app.plateDirty = true;
                }
            }
            ImGui::PopID();
        }
        note("Click en una placa para cargarla; después, Crear placa.");
        if (!app.plateScript.empty()) {
            const std::string msg = std::to_string(app.plateScript.size()) + " " +
                                    tr("intervenciones de la escena: se repiten al crear la placa.");
            note(msg.c_str());
            if (ImGui::SmallButton(tr("Descartar las intervenciones")))
                app.plateScript.clear();
        }
    }
};

} // namespace

std::unique_ptr<Panel> makePlatePanel() { return std::make_unique<PlatePanel>(); }

} // namespace ui
