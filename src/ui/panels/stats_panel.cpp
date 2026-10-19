// Estadísticas: censo, totales conservados y el ser controlado.
#include "app/app.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/fungi.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"
#include "ui/theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace ui {

namespace {

void drawControlled(App& app) {
    World& world = *app.world;
    const entt::registry& reg = world.registry();
    if (!reg.valid(world.player())) {
        note("Ningún ser controlado. Con la herramienta Controlar (2), click en una célula (o P sobre ella).");
        return;
    }
    const entt::entity p = world.player();
    const Locomotion& loco = reg.get<Locomotion>(p);
    const Digestion& dig = reg.get<Digestion>(p);
    const Metabolism& meta = reg.get<Metabolism>(p);
    const Dividing* div = reg.try_get<Dividing>(p);
    readout(reg.all_of<Species>(p) ? speciesInfo(reg.get<Species>(p).id).name : locomotionName(loco.type),
            "%.0f µm/s | %.1f h bio", std::abs(loco.speed), meta.ageHours);
    const float reserves = meta.reserves / std::max(1e-6f, meta.biomass);
    char text[32];
    std::snprintf(text, sizeof text, "%.0f%%", 100.0f * reserves);
    bar("Reservas", reserves / 0.6f, text);
    const float growth = meta.biomass / std::max(1e-6f, meta.birthBiomass);
    std::snprintf(text, sizeof text, "x%.2f", growth);
    bar("Biomasa", (growth - 0.5f) / 1.5f, text);
    if (div) {
        std::snprintf(text, sizeof text, "%.0f%%", 100.0f * std::min(1.0f, div->progress));
        bar("División", div->progress, text);
    }
    readout("O2 / luz", "%.0f µM (%.0f%% aerobio) | %.2f%s", meta.o2, 100 * meta.aerobic, meta.light,
            meta.starving ? " | AYUNO" : "");
    readout("Comido", "%d bact. %d detr. %d cel. | %zu vac.", dig.bacteriaEaten, dig.detritusEaten, dig.cellsEaten,
            dig.vacuoles.size());
    if (meta.needsSilicon || meta.photosynthetic || meta.needsVitamin) {
        const MetabolismParams& mp = world.metabolismParams();
        auto quota = [&](float store, float ratio) { return store / std::max(1e-12f, ratio * meta.biomass); };
        if (meta.needsSilicon)
            readout("Cuota de Si", "%.0f%% de la necesaria", 100 * quota(meta.silicon, mp.siliconRatio));
        if (meta.photosynthetic)
            readout("Cuota de Fe", "%.0f%% de la necesaria", 100 * quota(meta.iron, mp.ironRatio));
        if (meta.needsVitamin)
            readout("Cuota de B12", "%.0f%% de la necesaria", 100 * quota(meta.vitamin, mp.vitaminRatio));
    }
    switch (loco.type) {
    case LocomotionType::Cilia: note("Cilios: W/S avanzar, A/D girar"); break;
    case LocomotionType::Flagellum: note("Flagelo: W o click sostenido, gira hacia el mouse"); break;
    case LocomotionType::Pseudopod: note("Ameba: click sostenido = pseudópodo; toca y envuelve"); break;
    case LocomotionType::Crawl: note("Célula adherida: click sostenido = migra hacia el mouse (en tiempo biológico)"); break;
    case LocomotionType::None: note("No se mueve sola"); break;
    }
    note("R reproducir | V conjugar | F seguir con la cámara");
}

// Balance de masa (fase 13): existencias por compartimento, cierre y flujos desde que se creó
// la placa. Lo calcula la sesión una vez por muestra (no cada cuadro).
void drawBudget(App& app) {
    const Run& run = app.session;
    const MassBudget& now = run.budget();
    const MassBudget& start = run.startBudget();
    constexpr int kTable = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##stocks", 1 + kElementCount, kTable)) {
        ImGui::TableSetupColumn(tr("Compartimento"), ImGuiTableColumnFlags_WidthStretch, 1.8f);
        for (int e = 0; e < kElementCount; ++e)
            ImGui::TableSetupColumn(elementName(static_cast<Element>(e)));
        ImGui::TableHeadersRow();
        auto number = [](double v) {
            if (v == 0.0)
                ImGui::TextDisabled("-");
            else
                ImGui::Text(std::abs(v) >= 1e4 ? "%.3g" : std::abs(v) >= 100.0 ? "%.0f" : "%.3g", v);
        };
        for (int p = 0; p < kPoolCount; ++p) {
            bool any = false;
            for (int e = 0; e < kElementCount; ++e)
                any = any || now.stock[e][p] != 0.0;
            if (!any)
                continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr(poolName(static_cast<Pool>(p))));
            for (int e = 0; e < kElementCount; ++e) {
                ImGui::TableNextColumn();
                number(now.stock[e][p]);
            }
        }
        auto row = [&](const char* label, auto&& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr(label));
            for (int e = 0; e < kElementCount; ++e) {
                ImGui::TableNextColumn();
                value(static_cast<Element>(e));
            }
        };
        row("Total", [&](Element e) { number(now.total(e)); });
        row("Cambio", [&](Element e) { number(now.total(e) - start.total(e)); });
        row("Externo", [&](Element e) {
            const int i = static_cast<int>(e);
            number(now.ledger.external[i] - start.ledger.external[i]);
        });
        row("Cierre", [&](Element e) {
            if (!conservedElement(e)) {
                ImGui::TextDisabled("%s", tr("abierto"));
                tooltip("El CO2 se intercambia con el aire en tiempo real y la materia orgánica entra desde afuera: "
                        "el carbono no se conserva en la gota.");
                return;
            }
            const double total = std::max(1e-12, std::abs(now.total(e)));
            const double pct = 100.0 * (1.0 - std::abs(run.residual(e)) / total);
            const Palette& pal = palette();
            const float* c = pct >= 99.0 ? pal.good : pal.warn;
            ImGui::TextColored({c[0], c[1], c[2], c[3]}, pct >= 99.95 ? "100%%" : "%.1f%%", pct);
            tooltip("Cambio: desde que se creó la placa. Externo: gotero, ajustes y fijación de N2. "
                    "Cierra al 100% si el cambio es igual a lo externo.");
        });
        ImGui::EndTable();
    }

    static int element = 0;
    static const char* const kElements[] = {"P", "N", "Si", "Fe", "C"};
    combo("Flujos de", &element, kElements, kElementCount, "Procesos que movieron el elemento desde que se creó la placa.");
    if (ImGui::BeginTable("##flows", 3, kTable)) {
        ImGui::TableSetupColumn(tr("Proceso"), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn(tr("al agua"));
        ImGui::TableSetupColumn(tr("del agua"));
        ImGui::TableHeadersRow();
        for (int f = 0; f < kFlowCount; ++f) {
            const double in = now.ledger.toWater[element][f] - start.ledger.toWater[element][f];
            const double out = now.ledger.fromWater[element][f] - start.ledger.fromWater[element][f];
            if (in == 0.0 && out == 0.0)
                continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(tr(flowName(static_cast<Flow>(f))));
            ImGui::TableNextColumn();
            ImGui::Text("%.3g", in);
            ImGui::TableNextColumn();
            ImGui::Text("%.3g", out);
        }
        ImGui::EndTable();
    }
    if (element == static_cast<int>(Element::N)) {
        const Observations& o = app.world->observations();
        if (o.nitrogenFixed > 0.0f)
            readout("N2 fijado", "%.1f pmol", o.nitrogenFixed);
    }
}

class StatsPanel : public Panel {
  public:
    const char* key() const override { return "stats"; }
    const char* title() const override { return "Estadísticas"; }
    Dock dock() const override { return Dock::Right; }

    void draw(App& app) override {
        World& world = *app.world;
        const Census& census = app.session.census();
        if (section("Ser controlado", true))
            drawControlled(app);
        if (section("Censo", true)) {
            readout("Células", "%zu", world.cellCount());
            readout("Bacterias", "%zu", world.bacteria().size());
            readout("Partículas", "%zu", world.ambient().size());
            readout("Biomasa viva", "%.0f pmol", census.totalBiomass);
            readout("Divisiones del ser", "%d (conjugaciones %d)", app.session.divisions(), app.session.conjugations());
        }
        if (section("Hongos y mohos")) {
            for (int k = 0; k < kFungusKinds; ++k)
                readout(fungusName(static_cast<FungusKind>(k)), "%d micelios", census.mycelia[k]);
            readout("Hifas / zoosporas", "%d / %d", census.hyphaNodes, census.zoospores);
            readout("Esporangios resistentes", "%d (Allomyces)", census.restingSporangia);
            readout("Algas con liquen", "%d", census.lichenized);
            readout("Algas infectadas", "%d (quitridios)", census.infectedAlgae);
            readout("Plasmodios", "%d (%d nodos, %d tubos)", census.plasmodia, census.plasmodiumNodes, census.plasmodiumEdges);
            readout("Largo de red", "%.0f µm", census.plasmodiumLength);
            readout("Copos / esclerocios", "%d / %d", census.oatFlakes, census.sclerotia);
        }
        if (section("Balance de masa [pmol]", true))
            drawBudget(app);
    }
};

} // namespace

std::unique_ptr<Panel> makeStatsPanel() { return std::make_unique<StatsPanel>(); }

} // namespace ui
