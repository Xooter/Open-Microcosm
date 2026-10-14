// Estadísticas: censo, totales conservados y el ser controlado.
#include "app/app.hpp"
#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "bio/fungi.hpp"
#include "ui/panels/panel_classes.hpp"
#include "ui/widgets.hpp"
#include "ui/i18n.hpp"

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
    default: note("Ondulación: W/S avanzar, A/D girar"); break;
    }
    note("R reproducir | V conjugar | F seguir con la cámara");
}

class StatsPanel : public Panel {
  public:
    const char* key() const override { return "estadisticas"; }
    const char* title() const override { return "Estadísticas"; }
    Dock dock() const override { return Dock::Right; }

    void draw(App& app) override {
        World& world = *app.world;
        ToolState& tools = app.tools;
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
        if (section("Totales conservados [pmol]", true)) {
            tools.statsTimer -= GetFrameTime();
            if (tools.statsTimer <= 0.0f) {
                tools.statsTimer = 1.0f;
                tools.totalP = world.totalPhosphorus();
                tools.totalN = world.totalNitrogen();
                tools.totalSi = world.totalSilicon();
                tools.totalFe = world.totalIron();
            }
            readout("Fósforo / Nitrógeno", "%.1f / %.1f", tools.totalP, tools.totalN);
            readout("Silicio / Hierro", "%.1f / %.4f", tools.totalSi, tools.totalFe);
            const Observations& o = world.observations();
            if (o.nitrogenFixed > 0.0f)
                readout("N2 fijado", "%.1f pmol", o.nitrogenFixed);
        }
    }
};

} // namespace

std::unique_ptr<Panel> makeStatsPanel() { return std::make_unique<StatsPanel>(); }

} // namespace ui
