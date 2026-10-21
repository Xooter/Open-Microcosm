// Editor de placa: preconfigura la placa (una gota de un bioma o una placa preparada, como un
// cultivo celular; seed, contenido, ambiente, nutrientes, sedimento y poblaciones). Lo que se puede cambiar con la placa en marcha (ambiente y
// nutrientes) se aplica en vivo; el resto se aplica al crear la placa.
#include "app/app.hpp"
#include "bio/osmosis.hpp"
#include "game/scene.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

#include <imgui.h>

#include <algorithm>
#include <random>
#include <vector>

namespace ui {

namespace {

constexpr const char* kContents[] = {"Completa", "Sin vida", "Placa vacía"};
constexpr const char* kContentNotes[] = {"Todas las poblaciones, el sedimento y el polvo del bioma.",
                                         "Solo agua, química y sedimento: sin vida (agregala desde el catálogo).",
                                         "Placa limpia: sin vida, minerales, polvo ni detritos. Solo agua y química."};
constexpr const char* kGrids[] = {"128", "256", "512"};

class PlatePanel : public Panel {
  public:
    const char* key() const override { return "plate"; }
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
        if (section("Raleo (en vivo)", true))
            drawThinning(app);
        // Un cultivo es estéril: sin sedimento ni fauna silvestre (se inocula desde el Catálogo).
        if (p.kind() == PlateKind::Drop && section("Sedimento (al crear)"))
            drawSediment(app, p);
        if (p.kind() == PlateKind::Drop && section("Poblaciones (al crear)"))
            drawPopulations(app, p);
        if (!p.prepared.empty() && !p.config.seeding.empty() && section("Muestra (al crear)", true))
            drawSample(app, p);
        if (section("Placa y grilla (al crear)"))
            drawGeometry(app, p);
        if (section("Guardar y cargar", true))
            drawFiles(app, p);
    }

  private:
    float thinFraction_ = 0.1f, thinEveryHours_ = 24.0f;

    static void dirtyIf(App& app, bool changed) { app.plateDirty |= changed; }

    // Raleo: sacar una fracción de la gota revuelta y reponerla con medio nuevo (World::thin), una
    // vez o cada tanto (una entrada "dilution" del programa de la placa).
    void drawThinning(App& app) {
        slider("Fracción", &thinFraction_, 0.01f, 0.9f, "%.2f",
               "Lo que se saca al azar de lo que está en suspensión (seres, bacterias libres, detritos) y la misma "
               "fracción de lo disuelto, que se repone con el medio de la placa. Lo pegado al vidrio se queda.");
        if (ImGui::Button(tr("Ralear ahora"))) {
            Intervention iv;
            iv.kind = InterventionKind::Thin;
            iv.v[0] = thinFraction_;
            app.intervene(iv);
        }
        sliderLog("Cada [h bio]", &thinEveryHours_, 0.5f, 240.0f, "%.1f",
                  "Período del raleo programado, en horas biológicas (Gause sacaba la décima parte cada 24 h).");
        if (ImGui::Button(tr("Ralear periódicamente"))) {
            ProgramEntry e;
            e.target = "dilution";
            e.shape = ProgramShape::Event;
            e.start = static_cast<double>(app.world->programHours()) + thinEveryHours_;
            e.period = thinEveryHours_;
            e.amount = thinFraction_;
            Intervention iv;
            iv.kind = InterventionKind::Program;
            iv.key = programEntryText(e);
            std::replace(iv.key.begin(), iv.key.end(), ' ', ',');
            app.intervene(iv);
        }
        tooltip("Agrega el raleo al programa de la placa (panel Simulación): se repite cada período.");
    }

    void drawBase(App& app, PlateSpec& p) {
        if (!section("Base", true))
            return;
        // Tipo de placa: la gota de un bioma o una placa preparada (plates/).
        std::vector<const char*> kinds = {tr("Gota (de un bioma)")};
        int kind = 0;
        for (int i = 0; i < preparedPlateCount(); ++i) {
            kinds.push_back(preparedPlate(i).name.c_str());
            if (preparedPlate(i).key == p.prepared)
                kind = i + 1;
        }
        if (combo("Tipo de placa", &kind, kinds.data(), static_cast<int>(kinds.size()),
                  "Una gota de agua con su fauna (de un bioma) o una placa preparada como en el laboratorio: un "
                  "cultivo celular en su medio, en la incubadora.")) {
            p.prepared = kind == 0 ? std::string() : preparedPlate(kind - 1).key;
            p.rebuild();
            app.plateDirty = true;
        }
        if (!p.prepared.empty()) {
            drawPrepared(app, p);
            return;
        }
        std::vector<const char*> names;
        for (int b = 0; b < biomeCount(); ++b)
            names.push_back(biome(static_cast<BiomeId>(b)).name.c_str());
        int b = p.biome;
        if (combo("Bioma", &b, names.data(), biomeCount(), "Ambiente de partida: química, temperatura y elenco de especies.")) {
            p.biome = b;
            p.rebuild(); // cambia todo lo derivado del bioma
            app.plateDirty = true;
        }
        note(biome(static_cast<BiomeId>(p.biome)).description.c_str());
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

    // Placa preparada: qué recipiente y qué medio, y la semilla (la siembra y lo inoculado).
    void drawPrepared(App& app, PlateSpec& p) {
        const PreparedPlate* pp = findPreparedPlate(p.prepared);
        if (!pp)
            return;
        note(pp->description.c_str());
        if (const Vessel* v = findVessel(pp->vessel))
            readout("Recipiente", "%s: %.1f cm², %.1f mL (%.1f mm de medio)", v->name.c_str(), static_cast<double>(v->areaCm2),
                    static_cast<double>(v->volumeMl), static_cast<double>(v->depthUm() * 1e-3f));
        if (pp->kind == PlateKind::Slide) {
            // Solución de montaje: cambiarla con la placa en marcha es lo que se hace en clase (una
            // gota en un borde del cubreobjetos y un papel que la tira del otro): cambia el agua de
            // abajo, con su sal y su sacarosa (se aplica como el ambiente, en vivo).
            MediumParams& env = p.config.medium;
            std::vector<const char*> names;
            int current = -1;
            for (int i = 0; i < cultureMediumCount(); ++i) {
                const CultureMedium& cm = cultureMedium(i);
                names.push_back(cm.name.c_str());
                if (cm.composition.salinity == env.salinity && cm.composition.osmolytes == env.osmolytes)
                    current = i;
            }
            names.push_back("A medida");
            if (current < 0)
                current = static_cast<int>(names.size()) - 1;
            int chosen = current;
            if (combo("Solución de montaje", &chosen, names.data(), static_cast<int>(names.size())) &&
                chosen < cultureMediumCount()) {
                env.salinity = cultureMedium(chosen).composition.salinity;
                env.osmolytes = cultureMedium(chosen).composition.osmolytes;
                if (app.livePlate)
                    app.pushMediumLive();
                else
                    app.plateDirty = true;
            }
            tooltip("Reemplaza el agua bajo el cubreobjetos. La sal y la sacarosa también se ajustan a mano en Ambiente.");
            if (current == cultureMediumCount())
                note("Montaje a medida (sal o sacarosa cambiadas en Ambiente).");
            readout("Osmolaridad", "%.0f mOsm/L",
                    static_cast<double>(osmosis::mediumOsmolarity(p.config.medium.salinity, p.config.medium.osmolytes)));
            if (pp->tissue.enabled && pp->tissue.content == TissueContent::Protoplast)
                readout("Jugo celular", "%.0f mOsm/L", static_cast<double>(pp->tissue.sapOsmolarity));
            readout("Estado", "%s", pp->fixed ? "fijado y tenido" : "vivo");
            if (!pp->fixed)
                note("Para fijarlo y teñirlo: panel Tinción (Fijar la muestra).");
        } else if (const CultureMedium* m = findCultureMedium(pp->medium)) {
            readout("Medio", "%s", m->name.c_str());
            tooltip(m->reference.c_str());
            const MediumParams& c = p.config.medium;
            readout("Glucosa", "%.1f mM", static_cast<double>(c.organic * 1e-3f));
            readout("Glutamina", "%.1f mM", static_cast<double>(c.glutamine * 1e-3f));
            readout("Bicarbonato", "%.0f mM", static_cast<double>(c.alkalinity * 1e-3f));
            readout("Suero", "%.0f %%", static_cast<double>(c.serum * 100.0f));
            readout("Rojo fenol", "%.0f mg/L", static_cast<double>(c.phenolRed));
        }
        const float side = static_cast<float>(p.config.gridX) * p.config.cellSize;
        readout("Campo simulado", "%.1f × %.1f mm (%.3f cm²)", static_cast<double>(side * 1e-3f),
                static_cast<double>(side * 1e-3f), static_cast<double>(side * side * 1e-8f));
        tooltip("Un círculo del fondo del recipiente: el anillo de vidrio marca el borde del campo simulado (la "
                "pared del recipiente real está mucho más lejos).");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Semilla"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-60.0f);
        if (ImGui::InputScalar("##seed", ImGuiDataType_U64, &p.seed)) {
            p.rebuild();
            app.plateDirty = true;
        }
        tooltip("Fija dónde caen las células de la siembra y de cada inoculación.");
        note("Estéril al crearla: se le inocula cualquier ser desde el Catálogo (Inocular...).");
    }

    // Lo que se siembra en una placa preparada: cuántas células (como concentración de la muestra si el
    // recipiente tiene volumen, p. ej. millones por mL de semen) y, para los que nadan por clases, la
    // calidad de la muestra (las clases de motilidad de la OMS).
    void drawSample(App& app, PlateSpec& p) {
        WorldConfig& c = p.config;
        const PreparedPlate* pp = findPreparedPlate(p.prepared);
        const Vessel* v = pp ? findVessel(pp->vessel) : nullptr;
        const float side = static_cast<float>(c.gridX) * c.cellSize;
        const float areaCm2 = side * side * 1e-8f;
        const float depthCm = v ? v->depthUm() * 1e-4f : 0.0f;
        const SpeciesInfo* swimmer = nullptr;
        for (std::size_t i = 0; i < c.seeding.size(); ++i) {
            auto& [sp, count] = c.seeding[i];
            const SpeciesInfo& info = speciesInfo(sp);
            if (info.motility.present)
                swimmer = &info;
            ImGui::PushID(static_cast<int>(i));
            ImGui::TextUnformatted(tr(info.name));
            if (depthCm > 0.0f) {
                float millions = static_cast<float>(count) / (areaCm2 * depthCm) * 1e-6f;
                if (sliderLog("Concentración [millones/mL]", &millions, 0.1f, 300.0f, "%.1f",
                              "Células por mL de la muestra (espermiograma: el límite inferior de referencia de la OMS "
                              "2021 es 16 millones/mL).")) {
                    count = static_cast<int>(std::round(millions * 1e6f * areaCm2 * depthCm));
                    app.plateDirty = true;
                }
            } else {
                float density = static_cast<float>(count) / areaCm2;
                if (sliderLog("Densidad [células/cm²]", &density, 10.0f, 1e6f, "%.0f")) {
                    count = static_cast<int>(std::round(density * areaCm2));
                    app.plateDirty = true;
                }
            }
            readout("En el campo", "%d", count);
            ImGui::PopID();
        }
        if (!swimmer)
            return;
        MotilityMix& m = c.motility;
        if (!c.motilitySet)
            m = swimmer->motility.mix;
        bool changed = false;
        float pr = 100.0f * m.progressive, np = 100.0f * m.nonProgressive, im = 100.0f * m.immotile, dead = 100.0f * m.dead;
        changed |= slider("Progresivos [%]", &pr, 0.0f, 100.0f, "%.0f", "Nadan avanzando (OMS 2021: límite inferior 30 %).");
        changed |= slider("No progresivos [%]", &np, 0.0f, 100.0f, "%.0f", "Baten sin avanzar: dan vueltas en el lugar.");
        changed |= slider("Inmóviles vivos [%]", &im, 0.0f, 100.0f, "%.0f",
                          "No baten pero tienen la membrana intacta (excluyen la eosina; se hinchan en la prueba hiposmótica).");
        changed |= slider("Muertos [%]", &dead, 0.0f, 100.0f, "%.0f",
                          "Inmóviles con la membrana dañada (la eosina los tiñe). Vitalidad = 100 − muertos (OMS: ≥ 54 %).");
        if (changed) {
            m = {pr / 100.0f, np / 100.0f, im / 100.0f, dead / 100.0f};
            c.motilitySet = true;
            app.plateDirty = true;
        }
        const float sum = pr + np + im + dead;
        readout("Suman", "%.0f %%", static_cast<double>(sum));
        if (std::abs(sum - 100.0f) > 0.5f)
            note("Se reparten en proporción a lo que suman.");
        if (!swimmer->morphotypes.empty()) {
            float typical = 0.0f;
            for (const MorphotypeSpec& f : swimmer->morphotypes)
                typical += f.normal ? f.fraction : 0.0f;
            float normal = 100.0f * (c.normalForms >= 0.0f ? c.normalForms : typical);
            if (slider("Formas normales [%]", &normal, 0.0f, 100.0f, "%.0f",
                       "Criterios estrictos (OMS 2021: límite inferior de referencia 4 %). Las anormales conservan "
                       "sus proporciones (cabeza amorfa, afinada, piriforme, cuello doblado, cola enrollada…).")) {
                c.normalForms = normal / 100.0f;
                app.plateDirty = true;
            }
        }
        if ((c.motilitySet || c.normalForms >= 0.0f) && ImGui::Button(tr("Muestra típica"))) {
            c.motilitySet = false;
            c.normalForms = -1.0f;
            app.plateDirty = true;
        }
    }

    void drawEnvironment(App& app, PlateSpec& p) {
        MediumParams& m = p.config.medium;
        bool changed = false;
        changed |= slider("Temperatura [°C]", &m.temperature, -2.0f, 60.0f, "%.1f",
                          "Q10 = 2 en todas las tasas biológicas; cambia la viscosidad y el O2 de saturación.");
        changed |= slider("Salinidad [PSU]", &m.salinity, 0.0f, 100.0f, "%.1f", "Agua dulce < 1; mar ~35; salmuera hasta ~100.");
        changed |= slider("Luz", &m.surfaceLight, 0.0f, 1.0f, "%.2f", "Irradiancia relativa de la lámpara.");
        changed |= sliderLog("Intercambio O2 [1/s]", &m.o2AirExchange, 1e-5f, 0.5f, "%.2g",
                             "Reposición de O2 (y salida de CO2) por la superficie: rápida en la película de una gota; en "
                             "2-3 mm de medio quieto tarda ~1 h (difusión).");
        changed |= sliderLog("Alcalinidad [µM]", &m.alkalinity, 50.0f, 60000.0f, "%.0f",
                             "Bicarbonato: fija el pH junto con el CO2 (agua dulce ~1000; DMEM 44 000).");
        changed |= sliderLog("CO2 del aire [%]", &m.co2Atmosphere, 1e-4f, 0.2f, "%.3f",
                             "Aire libre: 0,042 %. Una incubadora de cultivo: 5-10 %, para que el bicarbonato del "
                             "medio quede en pH ~7,4.",
                             100.0f);
        changed |= checkbox("Anóxico (sin O2 del aire)", &m.anoxic);
        changed |= slider("Sacarosa [mOsm/L]", &m.osmolytes, 0.0f, 2000.0f, "%.0f",
                          "Soluto que no atraviesa la membrana: sube la osmolaridad del medio sin agregar sal (1 M de "
                          "sacarosa ≈ 1000 mOsm/L).");
        readout("Osmolaridad del medio", "%.0f mOsm/L", static_cast<double>(osmosis::mediumOsmolarity(m.salinity, m.osmolytes)));
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
        nutrient(app, "Nitrógeno [µM]", 1, &m.nitrogen, 1.0f, 30000.0f, "%.0f", "Con N:P < 16 el agua queda limitada por N: favorece a las cianobacterias fijadoras.");
        nutrient(app, "Sílice [µM]", 2, &m.silicate, 1.0f, 1000.0f, "%.0f", "Las diatomeas la necesitan para cada valva.");
        nutrient(app, "Hierro [µM]", 3, &m.iron, 0.01f, 50.0f, "%.2f", "Limita a los fotótrofos en el mar abierto.");
        nutrient(app, "Vitamina B12 [pM]", 4, &m.vitamin, 1e-5f, 0.1f, "%.1f", "Euglena y las diatomeas no la sintetizan: la aportan las bacterias.", 1e6f);
        nutrient(app, "Materia orgánica [µM]", 5, &m.organic, 0.1f, 50000.0f, "%.0f",
                 "Carbono orgánico disuelto (equivalente de glucosa) que toman bacterias y levaduras. Un medio de "
                 "cultivo como el DMEM trae 25 000 µM (4,5 g/L de glucosa).");
        note("Llevan la concentración media de toda la placa al valor elegido.");
    }

    void drawSediment(App& app, PlateSpec& p) {
        Sediment& s = p.config.sediment;
        bool c = false;
        for (int k = 0; k < mineralCount(); ++k)
            c |= sliderInt(mineralName(static_cast<MineralKind>(k)), &s.count[k], 0, 600);
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
            for (int i = 0; i < speciesCount(); ++i)
                any |= static_cast<int>(categoryOf(beingAt(i))) == cat;
            if (!any || !ImGui::TreeNode(tr(categoryName(static_cast<BeingCategory>(cat)))))
                continue;
            for (int i = 0; i < speciesCount(); ++i)
                if (static_cast<int>(categoryOf(beingAt(i))) == cat)
                    ch |= slider(speciesInfo(static_cast<SpeciesId>(i)).name, &c.speciesWeight[i], 0.0f, 5.0f, "%.2f");
            ImGui::TreePop();
        }
        dirtyIf(app, ch);
    }

    void drawGeometry(App& app, PlateSpec& p) {
        WorldConfig& c = p.config;
        bool ch = false;
        // La pared de vidrio se ve siempre: el mundo de una placa es un pocillo circular.
        ch |= slider("Margen de vidrio [µm]", &c.dishMargin, 20.0f, 300.0f, "%.0f");
        int g = c.gridX == 128 ? 0 : c.gridX == 512 ? 2 : 1;
        if (combo("Grilla (celdas)", &g, kGrids, 3, "Potencia de 2 (la FFT lo necesita). Más grande = placa más grande y más lenta.")) {
            c.gridX = c.gridY = g == 0 ? 128 : g == 2 ? 512 : 256;
            ch = true;
        }
        ch |= slider("Celda [µm]", &c.cellSize, 2.0f, 16.0f, "%.1f", "La placa mide grilla × celda.");
        ch |= sliderLog("Profundidad [µm]", &c.depth, 20.0f, 5000.0f, "%.0f",
                        "Del agua o del medio: la película de una gota ~200 µm; un recipiente de cultivo, 2-3 mm. Fija "
                        "cuánto medio hay sobre cada punto y qué tan locales son las corrientes.");
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
            saveScene(app.platesDir + "/" + app.plateName + ".plate", app.runPlate, app.session.interventions().entries(),
                      app.session.notes());
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
                std::vector<std::string> missing;
                if (loadScene(app.platesDir + "/" + name + ".plate", p, app.plateScript, &missing, &app.plateNotes)) {
                    app.plateName = name;
                    app.plateDirty = true;
                }
                // Lo que la escena nombra y no está cargado se avisa (se carga igual con lo que hay).
                for (const std::string& m : missing)
                    app.notify(std::string(tr("Falta contenido: ")) + m);
            }
            ImGui::PopID();
        }
        note("Click en una placa para cargarla; después, Crear placa.");
        if (!app.plateScript.empty()) {
            const std::string msg = std::to_string(app.plateScript.size()) + " " +
                                    tr("intervenciones de la escena: se repiten al crear la placa.");
            note(msg.c_str());
            if (ImGui::SmallButton(tr("Descartar las intervenciones"))) {
                app.plateScript.clear();
                app.plateNotes.clear();
            }
        }
    }
};

} // namespace

std::unique_ptr<Panel> makePlatePanel() { return std::make_unique<PlatePanel>(); }

} // namespace ui
