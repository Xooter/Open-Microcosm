#include "core/lab_panel.hpp"

#include "bio/division.hpp"
#include "bio/feeding.hpp"
#include "game/run.hpp"
#include "game/world.hpp"
#include "render/cell_renderer.hpp"
#include "sim/substep_solver.hpp"

#include <raygui.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPanelW = 380.0f;
constexpr float kRow = 22.0f;

// Solutos que se pueden gotear: nombre y concentración típica [µM].
struct Dropper {
    const char* name;
    float typical;
};
constexpr Dropper kDroppers[] = {
    {"Fosfato", 50.0f},       {"Nitrogeno inorganico", 500.0f}, {"Silice (Si)", 200.0f},
    {"Hierro (Fe)", 1.0f},    {"Vitamina B12", 0.002f},         {"Materia organica", 20.0f},
};
constexpr int kDropperCount = static_cast<int>(sizeof(kDroppers) / sizeof(kDroppers[0]));

ChemField& dropperField(World& world, int index) {
    switch (index) {
    case 0: return world.phosphateField();
    case 1: return world.nitrogenField();
    case 2: return world.silicateField();
    case 3: return world.ironField();
    case 4: return world.vitaminField();
    default: return world.nutrients();
    }
}

// Secciones (un bit cada una en ToolState::openSections).
enum Section {
    kSimulation, kEnvironment, kMacro, kMicro, kChemistry, kMicrobiology, kTools, kSoftBody, kControlled, kStats, kHelp,
    kSectionCount
};

// Disposición vertical con scroll: las filas que no entran completas en la vista no se dibujan
// (así raygui tampoco les da clicks).
struct Layout {
    Rectangle view;
    float x, w;
    float y;
    float top;
    Rectangle next(float h = kRow) {
        const Rectangle r = {x, y, w, h};
        y += h + 2.0f;
        return r;
    }
    bool visible(const Rectangle& r) const { return r.y >= view.y && r.y + r.height <= view.y + view.height; }
    float used() const { return y - top; }
};

bool header(Layout& L, ToolState& tools, Section s, const char* title) {
    const Rectangle r = L.next(24.0f);
    const bool open = (tools.openSections >> s) & 1u;
    if (L.visible(r) && GuiButton(r, TextFormat("%s  %s", open ? "[-]" : "[+]", title)))
        tools.openSections ^= 1u << s;
    return (tools.openSections >> s) & 1u;
}

void text(Layout& L, const char* label) {
    const Rectangle r = L.next(16.0f);
    if (L.visible(r))
        GuiLabel(r, label);
}

void readout(Layout& L, const char* label, const char* value) {
    const Rectangle r = L.next(18.0f);
    if (!L.visible(r))
        return;
    GuiLabel({r.x, r.y, 150, r.height}, label);
    GuiLabel({r.x + 150, r.y, r.width - 150, r.height}, value);
}

// Devuelve true si el valor cambió.
bool slider(Layout& L, const char* label, float* value, float lo, float hi, const char* fmt, float shown = 1.0f) {
    const Rectangle r = L.next();
    if (!L.visible(r))
        return false;
    const float before = *value;
    GuiLabel({r.x, r.y, 150, r.height}, label);
    GuiSliderBar({r.x + 150, r.y + 3, 110, r.height - 6}, nullptr, TextFormat(fmt, *value * shown), value, lo, hi);
    return *value != before;
}

// Escala logarítmica para magnitudes que cruzan varios órdenes (concentraciones, tasas).
bool logSlider(Layout& L, const char* label, float* value, float lo, float hi, const char* fmt, float shown = 1.0f) {
    const Rectangle r = L.next();
    if (!L.visible(r))
        return false;
    float l = std::log10(std::max(lo, *value));
    const float before = l;
    GuiLabel({r.x, r.y, 150, r.height}, label);
    GuiSliderBar({r.x + 150, r.y + 3, 110, r.height - 6}, nullptr, TextFormat(fmt, *value * shown), &l,
                 std::log10(lo), std::log10(hi));
    if (l == before)
        return false;
    *value = std::pow(10.0f, l);
    return true;
}

bool check(Layout& L, const char* label, bool* value) {
    const Rectangle r = L.next();
    if (!L.visible(r))
        return false;
    const bool before = *value;
    GuiCheckBox({r.x, r.y + 3, 16, 16}, label, value);
    return *value != before;
}

bool button(Layout& L, const char* label) {
    const Rectangle r = L.next();
    return L.visible(r) && GuiButton(r, label);
}

// Concentración media del agua [µM].
float meanOf(const World& world, const ChemField& field) {
    const Grid2D& g = field.grid();
    const float area = world.dish().active() ? units::kPi * world.dish().radius * world.dish().radius
                                             : g.width() * g.height();
    return static_cast<float>(field.sum() / std::max(1.0f, area / (g.dx * g.dx)));
}

// Fila de un soluto: nombre, concentración media y botones para duplicarla, partirla o vaciarla.
void soluteRow(Layout& L, World& world, const char* name, ChemField& field, float shown, const char* unit) {
    const Rectangle r = L.next();
    if (!L.visible(r))
        return;
    GuiLabel({r.x, r.y, 190, r.height}, TextFormat("%s: %.3g %s", name, meanOf(world, field) * shown, unit));
    auto scale = [&](float k) {
        for (float& v : field.grid().v)
            v *= k;
    };
    if (GuiButton({r.x + 200, r.y, 48, r.height}, "x2"))
        scale(2.0f);
    if (GuiButton({r.x + 252, r.y, 48, r.height}, "/2"))
        scale(0.5f);
    if (GuiButton({r.x + 304, r.y, 48, r.height}, "0"))
        scale(0.0f);
}

void drawControlled(Layout& L, World& world) {
    const entt::registry& reg = world.registry();
    if (!reg.valid(world.player())) {
        text(L, "Ningun ser controlado.");
        text(L, "Apunta a una celula y apreta P para controlarla.");
        return;
    }
    const entt::entity p = world.player();
    const Locomotion& loco = reg.get<Locomotion>(p);
    const Digestion& dig = reg.get<Digestion>(p);
    const Metabolism& meta = reg.get<Metabolism>(p);
    const Dividing* div = reg.try_get<Dividing>(p);
    readout(L, reg.all_of<Species>(p) ? speciesInfo(reg.get<Species>(p).id).name : locomotionName(loco.type),
            TextFormat("%.0f um/s | %.1f h bio", std::abs(loco.speed), meta.ageHours));
    auto bar = [&](const char* label, float v, float lo, float hi, const char* fmt, float shown) {
        const Rectangle r = L.next(16.0f);
        if (L.visible(r))
            GuiProgressBar({r.x + 80, r.y, 180, r.height}, label, TextFormat(fmt, shown), &v, lo, hi);
    };
    bar("reservas", meta.reserves / std::max(1e-6f, meta.biomass), 0.0f, 0.6f, "%.0f%%",
        100.0f * meta.reserves / std::max(1e-6f, meta.biomass));
    const float growth = meta.biomass / std::max(1e-6f, meta.birthBiomass);
    bar("biomasa", growth, 0.5f, 2.0f, "x%.2f", growth);
    if (div)
        bar("division", std::min(1.0f, div->progress), 0.0f, 1.0f, "%.0f%%", 100.0f * std::min(1.0f, div->progress));
    readout(L, "O2 / luz", TextFormat("%.0f uM (%.0f%% aerobico) | %.2f%s", meta.o2, 100 * meta.aerobic, meta.light,
                                       meta.starving ? " | AYUNO" : ""));
    readout(L, "Comido", TextFormat("%d bact. %d detr. %d cel. | %zu vac.", dig.bacteriaEaten, dig.detritusEaten,
                                     dig.cellsEaten, dig.vacuoles.size()));
    if (meta.needsSilicon || meta.photosynthetic || meta.needsVitamin) {
        const MetabolismParams& mp = world.metabolismParams();
        auto quota = [&](float store, float ratio) { return store / std::max(1e-12f, ratio * meta.biomass); };
        if (meta.needsSilicon)
            readout(L, "Cuota de Si", TextFormat("%.0f%% de la necesaria", 100 * quota(meta.silicon, mp.siliconRatio)));
        if (meta.photosynthetic)
            readout(L, "Cuota de Fe", TextFormat("%.0f%% de la necesaria", 100 * quota(meta.iron, mp.ironRatio)));
        if (meta.needsVitamin)
            readout(L, "Cuota de B12", TextFormat("%.0f%% de la necesaria", 100 * quota(meta.vitamin, mp.vitaminRatio)));
    }
    switch (loco.type) {
    case LocomotionType::Cilia: text(L, "Cilios: W/S avanzar, A/D girar"); break;
    case LocomotionType::Flagellum: text(L, "Flagelo: W o click, gira hacia el mouse"); break;
    case LocomotionType::Pseudopod: text(L, "Ameba: click = pseudopodo; toca y envuelve"); break;
    default: text(L, "Ondulacion: W/S avanzar, A/D girar"); break;
    }
    text(L, "R reproducir | V conjugar | F seguir con la camara");
}

} // namespace

Rectangle labPanelRect() {
    // Deja libre la barra del sandbox, abajo.
    return {10.0f, 10.0f, kPanelW, std::max(240.0f, static_cast<float>(GetScreenHeight()) - 150.0f)};
}

bool labPanelHovered(const ToolState& tools) {
    return tools.showLab && CheckCollisionPointRec(GetMousePosition(), labPanelRect());
}

const char* dropperName(int index) { return kDroppers[std::clamp(index, 0, kDropperCount - 1)].name; }
int dropperCount() { return kDropperCount; }

void applyDropper(World& world, const ToolState& tools, float x, float y, float dt) {
    const Dropper& d = kDroppers[std::clamp(tools.dropper, 0, kDropperCount - 1)];
    world.dose(dropperField(world, tools.dropper), {x, y}, tools.dropRadius, d.typical * tools.dropAmount * dt);
}

void drawLabPanel(World& world, const Run& run, ToolState& tools) {
    if (!tools.showLab) {
        DrawText("H: panel de laboratorio", 12, 12, 16, DARKGRAY);
        return;
    }
    const Rectangle rect = labPanelRect();
    GuiPanel(rect, "Laboratorio (H oculta)");
    const Rectangle view = {rect.x + 6, rect.y + 28, rect.width - 12, rect.height - 34};

    // Scroll con la rueda cuando el mouse está sobre el panel.
    if (CheckCollisionPointRec(GetMousePosition(), rect))
        tools.labScroll -= GetMouseWheelMove() * 40.0f;
    tools.labScroll = std::clamp(tools.labScroll, 0.0f, std::max(0.0f, tools.labContent - view.height));

    BeginScissorMode(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.width),
                     static_cast<int>(view.height));
    Layout L{view, view.x + 4, view.width - 20, view.y - tools.labScroll, view.y - tools.labScroll};
    MediumParams& m = world.medium();
    const Census& census = run.census();

    if (header(L, tools, kSimulation, "Simulacion")) {
        check(L, "Pausa [Espacio]", &tools.paused);
        slider(L, "tiempo bio (x)", &world.bioTimeScale(), 1.0f, 600.0f, "%.0f");
        bool lod = world.lodStride > 1;
        if (check(L, "LOD: simplificar lo lejano", &lod))
            world.lodStride = lod ? 5 : 1;
        readout(L, "Tiempo simulado", TextFormat("%.1f s (%.2f h bio)", world.simTime(),
                                                  world.simTime() * world.bioScale() / 3600.0f));
        readout(L, "Rendimiento", TextFormat("%d FPS | sim %.1f ms | render %.1f ms", GetFPS(), tools.stepMs, tools.renderMs));
        readout(L, "Motor XPBD", TextFormat("%s | %zu en detalle", world.substepSolver() ? world.substepSolver()->name() : "CPU",
                                             world.nearBodies()));
    }
    if (header(L, tools, kEnvironment, "Ambiente")) {
        bool changed = slider(L, "temperatura [C]", &m.temperature, -2.0f, 60.0f, "%.1f");
        slider(L, "salinidad [PSU]", &m.salinity, 0.0f, 100.0f, "%.1f");
        slider(L, "luz", &m.surfaceLight, 0.0f, 1.0f, "%.2f");
        slider(L, "espesor [um]", &world.fluid().gapHeight, 20.0f, 500.0f, "%.0f");
        slider(L, "intercambio O2 [1/s]", &m.o2AirExchange, 0.0f, 0.5f, "%.3f");
        changed |= check(L, "Anoxico (sin O2 del aire)", &m.anoxic);
        if (changed)
            world.refreshMedium();
        readout(L, "O2 de saturacion", TextFormat("%.0f uM", world.oxygenSaturation()));
    }
    if (header(L, tools, kMacro, "Macronutrientes")) {
        soluteRow(L, world, "Fosfato", world.phosphateField(), 1.0f, "uM");
        soluteRow(L, world, "Nitrogeno", world.nitrogenField(), 1.0f, "uM");
        soluteRow(L, world, "Silice (Si)", world.silicateField(), 1.0f, "uM");
        soluteRow(L, world, "Materia organica", world.nutrients(), 1.0f, "uM");
        text(L, "x2, /2 y 0 escalan la concentracion de toda la gota.");
        text(L, "G gotea el soluto elegido en Herramientas.");
    }
    if (header(L, tools, kMicro, "Micronutrientes")) {
        soluteRow(L, world, "Hierro (Fe)", world.ironField(), 1.0f, "uM");
        soluteRow(L, world, "Vitamina B12", world.vitaminField(), 1e6f, "pM");
        logSlider(L, "B12 de bacterias", &m.vitaminSourcePerHour, 1e-7f, 1e-3f, "%.1e");
        slider(L, "B12 degrada luz [1/h]", &m.vitaminDecayPerHour, 0.0f, 0.5f, "%.3f");
        slider(L, "disolucion frustula", &m.frustuleDissolutionPerHour, 0.0f, 0.05f, "%.4f");
        text(L, "Si: diatomeas | Fe: fotosintesis | B12: Euglena y diatomeas");
    }
    if (header(L, tools, kChemistry, "Quimica")) {
        logSlider(L, "alcalinidad [uM]", &m.alkalinity, 50.0f, 5000.0f, "%.0f");
        slider(L, "descomposicion", &m.detritusDecay, 0.0f, 0.02f, "%.4f");
        slider(L, "consumo nutr.", &m.nutrientUptake, 0.0f, 0.05f, "%.3f");
        slider(L, "detritos/s", &m.detritusSpawnRate, 0.0f, 5.0f, "%.1f");
        slider(L, "aporte organico [uM/h]", &m.organicInflow, 0.0f, 500.0f, "%.0f");
        slider(L, "polvo mineral [1/s]", &m.dustRate, 0.0f, 0.2f, "%.3f");
        readout(L, "pH medio", TextFormat("%.2f (maximo %.2f)", census.meanPH, census.maxPH));
    }
    if (header(L, tools, kMicrobiology, "Microbiologia")) {
        slider(L, "quimiotaxis", &world.bacteria().params.chemotaxisGain, 0.0f, 20.0f, "%.1f");
        slider(L, "fagos: inactivacion", &m.phageDecayPerHour, 0.0f, 1.0f, "%.2f");
        slider(L, "toxinas: degradacion", &m.toxinDecayPerHour, 0.0f, 0.5f, "%.2f");
        slider(L, "AHL vida media [h]", &m.ahlHalfLifeHours, 1.0f, 100.0f, "%.0f");
        slider(L, "nauplios [1/h]", &m.naupliusArrivalsPerHour, 0.0f, 1.0f, "%.2f");
    }
    if (header(L, tools, kTools, "Herramientas")) {
        slider(L, "radio revolver", &tools.stirRadius, 5.0f, 120.0f, "%.0f");
        slider(L, "radio tinta", &tools.inkRadius, 2.0f, 60.0f, "%.0f");
        slider(L, "apretar [um/s]", &tools.squeezeSpeed, 5.0f, 120.0f, "%.0f");
        if (button(L, TextFormat("Gotero (G): %s", dropperName(tools.dropper))))
            tools.dropper = (tools.dropper + 1) % kDropperCount;
        slider(L, "radio gotero", &tools.dropRadius, 10.0f, 200.0f, "%.0f");
        logSlider(L, "cantidad gotero", &tools.dropAmount, 0.1f, 20.0f, "x%.1f");
        if (button(L, TextFormat("Overlay: %s (F1-F10)", overlayName(tools.overlay)))) {
            const int n = static_cast<int>(Overlay::Vitamin) + 1;
            tools.overlay = static_cast<Overlay>((static_cast<int>(tools.overlay) + 1) % n);
        }
        if (button(L, TextFormat("Tincion: %s (J)", stainName(static_cast<Stain>(tools.stain)))))
            tools.stain = (tools.stain + 1) % 3;
        check(L, "La camara sigue al ser controlado (F)", &tools.follow);
    }
    if (header(L, tools, kSoftBody, "Cuerpos blandos (avanzado)")) {
        SoftBodyTuning& t = world.softTuning();
        float substeps = static_cast<float>(t.substeps);
        slider(L, "subpasos", &substeps, 1.0f, 30.0f, "%.0f");
        t.substeps = static_cast<int>(substeps + 0.5f);
        slider(L, "memoria de forma x", &t.shapeScale, 0.0f, 3.0f, "%.2f");
        slider(L, "flexion [1/s]", &t.bendRate, 0.0f, 400.0f, "%.0f");
        slider(L, "area [1/s]", &t.areaRate, 10.0f, 3000.0f, "%.0f");
    }
    if (header(L, tools, kControlled, "Ser controlado"))
        drawControlled(L, world);
    if (header(L, tools, kStats, "Estadisticas")) {
        readout(L, "Celulas", TextFormat("%zu", world.cellCount()));
        readout(L, "Bacterias", TextFormat("%zu", world.bacteria().size()));
        readout(L, "Particulas", TextFormat("%zu", world.ambient().size()));
        readout(L, "Biomasa viva", TextFormat("%.0f pmol", census.totalBiomass));
        readout(L, "Divisiones del ser", TextFormat("%d (conjugaciones %d)", run.divisions(), run.conjugations()));
        tools.statsTimer -= GetFrameTime();
        if (tools.statsTimer <= 0.0f) {
            tools.statsTimer = 1.0f;
            tools.totalP = world.totalPhosphorus();
            tools.totalN = world.totalNitrogen();
            tools.totalSi = world.totalSilicon();
            tools.totalFe = world.totalIron();
        }
        text(L, "Totales conservados [pmol]:");
        readout(L, "  Fosforo / Nitrogeno", TextFormat("%.1f / %.1f", tools.totalP, tools.totalN));
        readout(L, "  Silicio / Hierro", TextFormat("%.1f / %.4f", tools.totalSi, tools.totalFe));
        const Observations& o = world.observations();
        if (o.nitrogenFixed > 0.0f)
            readout(L, "N2 fijado", TextFormat("%.1f pmol", o.nitrogenFixed));
    }
    if (header(L, tools, kHelp, "Ayuda")) {
        text(L, "Tab: modo Explorar (click = ficha del ser)");
        text(L, "P: controlar la celula bajo el cursor");
        text(L, "Click derecho + arrastrar: mover | Rueda: zoom");
        text(L, "Ctrl+Izq revolver | Shift+Izq estirar membrana");
        text(L, "T tinta | B detrito | X borrar | L lisar | Q apretar");
        text(L, "G gotero | J tincion | F1-F10 overlays | N inicio");
    }
    tools.labContent = L.used();
    EndScissorMode();
}
