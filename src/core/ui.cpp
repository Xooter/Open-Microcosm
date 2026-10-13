#include "core/ui.hpp"

#include "bio/division.hpp"
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
#include "game/progress.hpp"
#include "game/run.hpp"
#include "game/world.hpp"
#include "render/cell_renderer.hpp"

#include <cmath>
#include <optional>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

void drawUi(World& world, ToolState& tools) {
    if (!tools.showLab) {
        DrawText("H: panel de laboratorio", 12, 12, 16, DARKGRAY);
        return;
    }
    MediumParams& m = world.medium();

    GuiPanel(kUiPanel, "BioGame - laboratorio (H oculta)");
    float y = kUiPanel.y + 35;
    auto slider = [&](const char* label, float* value, float lo, float hi, const char* fmt) {
        GuiSliderBar({kUiPanel.x + 120, y, 110, 16}, label, TextFormat(fmt, *value), value, lo,
                     hi);
        y += 22;
    };
    slider("espesor [um]", &world.fluid().gapHeight, 20.0f, 500.0f, "%.0f");
    slider("radio revolver", &tools.stirRadius, 5.0f, 120.0f, "%.0f");
    slider("radio tinta", &tools.inkRadius, 2.0f, 60.0f, "%.0f");
    slider("quimiotaxis", &world.bacteria().params.chemotaxisGain, 0.0f, 20.0f, "%.1f");
    slider("descomposicion", &m.detritusDecay, 0.0f, 0.02f, "%.4f");
    slider("consumo nutr.", &m.nutrientUptake, 0.0f, 0.05f, "%.3f");
    slider("detritos/s", &m.detritusSpawnRate, 0.0f, 5.0f, "%.1f");

    SoftBodyTuning& t = world.softTuning();
    float substeps = static_cast<float>(t.substeps);
    slider("subpasos", &substeps, 1.0f, 30.0f, "%.0f");
    t.substeps = static_cast<int>(substeps + 0.5f);
    slider("memoria forma x", &t.shapeScale, 0.0f, 3.0f, "%.2f");
    slider("flexion [1/s]", &t.bendRate, 0.0f, 400.0f, "%.0f");
    slider("area [1/s]", &t.areaRate, 10.0f, 3000.0f, "%.0f");
    slider("apretar [um/s]", &tools.squeezeSpeed, 5.0f, 120.0f, "%.0f");

    GuiCheckBox({kUiPanel.x + 10, y + 2, 16, 16}, "Pausa [Espacio]", &tools.paused);
    y += 26;
    GuiLabel({kUiPanel.x + 10, y, 300, 16},
             TextFormat("t = %.1f s | %d FPS | sim %.1f ms | render %.1f ms", world.simTime(), GetFPS(), tools.stepMs,
                        tools.renderMs));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 300, 16},
             TextFormat("LOD: %zu en detalle | %zu con contactos", world.nearBodies(), world.touchingBodies()));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("%zu celulas | %zu bacterias | %zu part.", world.cellCount(),
                        world.bacteria().size(), world.ambient().size()));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 280, 16},
             TextFormat("Overlay: %s", overlayName(tools.overlay)));
    y += 18;
    GuiLabel({kUiPanel.x + 10, y, 300, 16},
             TextFormat("Tincion: %s", stainName(static_cast<Stain>(tools.stain))));
    y += 22;
    y += 4;
    const entt::registry& reg = world.registry();
    if (reg.valid(world.player())) {
        const entt::entity p = world.player();
        const Locomotion& loco = reg.get<Locomotion>(p);
        const Digestion& dig = reg.get<Digestion>(p);
        const Metabolism& meta = reg.get<Metabolism>(p);
        const Dividing* div = reg.try_get<Dividing>(p);
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("Jugador: %s  |  %.0f um/s  |  %.1f h", locomotionName(loco.type),
                            std::abs(loco.speed), meta.ageHours));
        y += 20;
        // Barras de estado: reservas, crecimiento hacia la división y progreso de la división.
        float reserves = meta.reserves / std::max(1e-6f, meta.biomass);
        GuiProgressBar({kUiPanel.x + 90, y, 160, 14}, "reservas", TextFormat("%.0f%%", 100 * reserves),
                       &reserves, 0.0f, 0.6f);
        y += 18;
        float growth = meta.biomass / std::max(1e-6f, meta.birthBiomass);
        GuiProgressBar({kUiPanel.x + 90, y, 160, 14}, "biomasa", TextFormat("x%.2f", growth), &growth,
                       0.5f, 2.0f);
        y += 18;
        if (div) {
            float pr = std::min(1.0f, div->progress);
            GuiProgressBar({kUiPanel.x + 90, y, 160, 14}, "division", TextFormat("%.0f%%", 100 * pr), &pr,
                           0.0f, 1.0f);
            y += 18;
        }
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("O2 %.0f uM (%.0f%% aerobico) | luz %.2f%s", meta.o2, 100 * meta.aerobic,
                            meta.light, meta.starving ? " | AYUNO" : ""));
        y += 18;
        GuiLabel({kUiPanel.x + 10, y, 300, 16},
                 TextFormat("Comido: %d bact. %d detr. %d cel. | %zu vacuolas", dig.bacteriaEaten,
                            dig.detritusEaten, dig.cellsEaten, dig.vacuoles.size()));
        y += 22;
    }
    slider("luz", &world.medium().surfaceLight, 0.0f, 1.0f, "%.2f");
    slider("tiempo bio x", &world.bioTimeScale(), 1.0f, 600.0f, "%.0f");
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "F seguir | N nueva partida | V conjugar");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Cilios: W/S avanzar, A/D girar");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Flagelo: W o click, gira hacia el mouse");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Ameba: click = pseudopodo; toca y envuelve");
    y += 20;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Ctrl+Izq revolver | Shift+Izq estirar | T tinta");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "B detrito | C celula | K 50 celulas | X borrar");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "Q apretar | L lisar | F1-F10 overlays | Rueda");
    y += 16;
    GuiLabel({kUiPanel.x + 10, y, 300, 16}, "J tincion");
}

namespace {

const char* speciesName(LocomotionType t) {
    switch (t) {
    case LocomotionType::Cilia: return "Paramecium (cilios)";
    case LocomotionType::Flagellum: return "Euglena (flagelo)";
    case LocomotionType::Pseudopod: return "Amoeba (pseudopodos)";
    }
    return "";
}

const char* unlockHint(LocomotionType t) {
    switch (t) {
    case LocomotionType::Cilia: return "";
    case LocomotionType::Flagellum: return "Llega a la generacion 3";
    case LocomotionType::Pseudopod: return "Llega a la generacion 5 o gana una partida";
    }
    return "";
}

void centered(const char* text, float y, int size, Color color) {
    DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, static_cast<int>(y), size, color);
}

} // namespace

TitleAction drawTitleScreen(const Progress& progress) {
    TitleAction action = TitleAction::None;
    BeginDrawing();
    ClearBackground({236, 231, 214, 255});
    const float cx = GetScreenWidth() * 0.5f, cy = GetScreenHeight() * 0.5f;
    centered("BioGame", cy - 220, 56, {60, 64, 52, 255});
    centered("Una gota de agua. Un linaje. Domina la biomasa o extinguite.", cy - 150, 22, {90, 92, 80, 255});
    const char* labels[] = {"Jugar", "Enciclopedia", "Opciones", "Salir"};
    const TitleAction actions[] = {TitleAction::Play, TitleAction::Encyclopedia, TitleAction::Options, TitleAction::Quit};
    for (int i = 0; i < 4; ++i)
        if (GuiButton({cx - 160, cy - 80 + i * 64.0f, 320, 50}, labels[i]))
            action = actions[i];
    if (IsKeyPressed(KEY_ENTER))
        action = TitleAction::Play;
    int species = 0;
    for (int k = 0; k < kSpeciesCount; ++k)
        species += (progress.seenSpecies >> k) & 1u;
    centered(TextFormat("Partidas %d | victorias %d | mejor generacion %u | mejor dominio %.0f%% | enciclopedia %d/%d",
                        progress.runs, progress.wins, progress.bestGeneration, 100 * progress.bestShare, species,
                        kSpeciesCount),
             cy + 190, 18, {90, 92, 80, 255});
    EndDrawing();
    return action;
}

SetupAction drawSetupScreen(const Progress& progress, SetupState& state) {
    SetupAction action = SetupAction::None;
    BeginDrawing();
    ClearBackground({236, 231, 214, 255});
    const float cx = GetScreenWidth() * 0.5f;
    centered("Elegir la gota", 40, 40, {60, 64, 52, 255});
    // Biomas.
    for (int b = 0; b < kBiomeCount; ++b) {
        const Biome& bio = biome(static_cast<BiomeId>(b));
        const Rectangle r = {cx - 560, 110 + b * 62.0f, 300, 52};
        bool on = state.biome == b;
        GuiToggle(r, bio.name, &on);
        if (on)
            state.biome = b;
        if (state.biome == b)
            DrawRectangleLinesEx(r, 3, {60, 110, 60, 255});
        if ((progress.biomesPlayed >> b) & 1u)
            DrawText("jugado", static_cast<int>(r.x + 230), static_cast<int>(r.y + 18), 14, GRAY);
    }
    const Biome& sel = biome(static_cast<BiomeId>(state.biome));
    DrawText(sel.name, static_cast<int>(cx - 220), 120, 28, {60, 64, 52, 255});
    // Descripción en varias líneas.
    {
        std::string text = sel.description, line;
        float y = 160;
        std::size_t start = 0;
        while (start < text.size()) {
            std::size_t end = std::min(text.size(), start + 60);
            if (end < text.size()) {
                const std::size_t sp = text.rfind(' ', end);
                if (sp != std::string::npos && sp > start)
                    end = sp;
            }
            DrawText(text.substr(start, end - start).c_str(), static_cast<int>(cx - 220), static_cast<int>(y), 18, {80, 82, 70, 255});
            y += 24;
            start = end + 1;
        }
        DrawText(TextFormat("%.0f C | luz %.0f%% | sal %.0f PSU%s", sel.medium.temperature, 100 * sel.medium.surfaceLight,
                            sel.medium.salinity, sel.medium.anoxic ? " | sin oxigeno" : ""),
                 static_cast<int>(cx - 220), static_cast<int>(y + 8), 18, {90, 92, 80, 255});
    }
    // Especie fundadora.
    const LocomotionType types[] = {LocomotionType::Cilia, LocomotionType::Flagellum, LocomotionType::Pseudopod};
    for (int i = 0; i < 3; ++i) {
        const LocomotionType t = types[i];
        const Rectangle r = {cx - 220, 420 + i * 62.0f, 440, 52};
        const bool open = progress.isUnlocked(t);
        if (!open)
            GuiDisable();
        if (GuiButton(r, TextFormat("%d  %s", i + 1, speciesName(t))) && open)
            state.founder = t;
        GuiEnable();
        if (open && IsKeyPressed(KEY_ONE + i))
            state.founder = t;
        if (state.founder == t)
            DrawRectangleLinesEx(r, 3, {60, 110, 60, 255});
        if (!open)
            DrawText(TextFormat("Bloqueada: %s", unlockHint(t)), static_cast<int>(r.x + 450), static_cast<int>(r.y + 18), 16, GRAY);
    }
    DrawText(TextFormat("Semilla %u", state.seed), static_cast<int>(cx - 220), 620, 20, {80, 82, 70, 255});
    if (GuiButton({cx - 40, 612, 120, 36}, "Otra"))
        state.seed = static_cast<unsigned>(GetRandomValue(1, 2147483646));
    if (GuiButton({cx - 220, 680, 210, 56}, "Volver") || IsKeyPressed(KEY_ESCAPE))
        action = SetupAction::Back;
    if (GuiButton({cx + 10, 680, 210, 56}, "Empezar") || IsKeyPressed(KEY_ENTER))
        action = SetupAction::Start;
    EndDrawing();
    return action;
}

bool drawEncyclopedia(const Progress& progress, float& scroll) {
    bool back = false;
    scroll = std::max(0.0f, scroll - 40.0f * GetMouseWheelMove());
    BeginDrawing();
    ClearBackground({236, 231, 214, 255});
    float y = 90 - scroll;
    const int x = 60;
    auto wrapText = [&](const char* text, int indent, int size, Color color) {
        const std::string t = text;
        std::size_t start = 0;
        while (start < t.size()) {
            std::size_t end = std::min(t.size(), start + 110);
            if (end < t.size()) {
                const std::size_t sp = t.rfind(' ', end);
                if (sp != std::string::npos && sp > start)
                    end = sp;
            }
            DrawText(t.substr(start, end - start).c_str(), x + indent, static_cast<int>(y), size, color);
            y += static_cast<float>(size) + 4;
            start = end + 1;
        }
    };
    const Color title = {60, 64, 52, 255}, body = {90, 92, 80, 255}, hidden = {160, 160, 150, 255};
    DrawText("Especies", x, static_cast<int>(y), 26, title);
    y += 34;
    for (int k = 0; k < kSpeciesCount; ++k) {
        const auto id = static_cast<SpeciesId>(k);
        if ((progress.seenSpecies >> k) & 1u) {
            DrawText(speciesInfo(id).name, x, static_cast<int>(y), 20, title);
            y += 24;
            wrapText(speciesBlurb(id), 20, 16, body);
        } else {
            DrawText("???  (todavia no la viste)", x, static_cast<int>(y), 20, hidden);
            y += 24;
        }
        y += 6;
    }
    y += 10;
    DrawText("Otros organismos", x, static_cast<int>(y), 26, title);
    y += 34;
    for (int k = 0; k < kOtherCount; ++k) {
        const auto o = static_cast<OtherOrganism>(k);
        if ((progress.seenOthers >> k) & 1u) {
            DrawText(otherName(o), x, static_cast<int>(y), 20, title);
            y += 24;
            wrapText(otherBlurb(o), 20, 16, body);
        } else {
            DrawText("???", x, static_cast<int>(y), 20, hidden);
            y += 24;
        }
        y += 6;
    }
    y += 10;
    DrawText(TextFormat("Comportamientos observados (%zu)", progress.behaviors.size()), x, static_cast<int>(y), 26, title);
    y += 34;
    for (const std::string& b : progress.behaviors) {
        DrawText(b.c_str(), x + 20, static_cast<int>(y), 16, body);
        y += 20;
    }
    DrawRectangle(0, 0, GetScreenWidth(), 70, {236, 231, 214, 255});
    centered("Enciclopedia", 16, 40, title);
    if (GuiButton({20, 16, 140, 40}, "Volver") || IsKeyPressed(KEY_ESCAPE))
        back = true;
    EndDrawing();
    return back;
}

bool drawOptions(Options& o) {
    bool back = false;
    BeginDrawing();
    ClearBackground({236, 231, 214, 255});
    const float cx = GetScreenWidth() * 0.5f;
    centered("Opciones", 80, 40, {60, 64, 52, 255});
    float y = 180;
    GuiCheckBox({cx - 200, y, 24, 24}, "Pantalla completa", &o.fullscreen);
    y += 50;
    GuiCheckBox({cx - 200, y, 24, 24}, "Optica del microscopio (vineta, grano)", &o.postProcess);
    y += 50;
    GuiCheckBox({cx - 200, y, 24, 24}, "LOD: simplificar las celulas lejanas", &o.lod);
    y += 50;
    GuiCheckBox({cx - 200, y, 24, 24}, "Mostrar el panel de laboratorio", &o.labPanel);
    y += 60;
    GuiSlider({cx - 60, y, 260, 24}, "Tiempo biologico", TextFormat("x%.0f", o.bioTimeScale), &o.bioTimeScale, 10.0f, 240.0f);
    y += 30;
    DrawText("(1 minuto real = x/60 horas biologicas; 60 es el valor calibrado)", static_cast<int>(cx - 200), static_cast<int>(y), 16, GRAY);
    if (GuiButton({cx - 100, y + 80, 200, 50}, "Volver") || IsKeyPressed(KEY_ESCAPE))
        back = true;
    EndDrawing();
    return back;
}

void drawToasts(const std::vector<std::string>& lines, float alpha) {
    if (lines.empty() || alpha <= 0.0f)
        return;
    const int x = GetScreenWidth() - 460;
    int y = GetScreenHeight() - 40 - 26 * static_cast<int>(lines.size());
    DrawRectangle(x - 12, y - 10, 452, 26 * static_cast<int>(lines.size()) + 16, Fade({236, 231, 214, 255}, 0.85f * alpha));
    for (const std::string& l : lines) {
        DrawText(TextFormat("Enciclopedia: %s", l.c_str()), x, y, 18, Fade({50, 90, 50, 255}, alpha));
        y += 26;
    }
}

namespace {

// Genes de la célula `cell`; si `compare` es válida, marca las diferencias con ella.
void drawGenome(const World& world, entt::entity cell, entt::entity compare, float x, float y, const char* title) {
    const entt::registry& reg = world.registry();
    if (!reg.valid(cell) || !reg.all_of<Heredity, Locomotion>(cell))
        return;
    const Genome& g = reg.get<Heredity>(cell).genome;
    const Genome* other = reg.valid(compare) && reg.all_of<Heredity>(compare) ? &reg.get<Heredity>(compare).genome : nullptr;
    const LocomotionType type = reg.get<Locomotion>(cell).type;
    int rows = 0;
    for (std::size_t i = 0; i < kGeneCount; ++i)
        rows += geneAllowed(type, static_cast<Gene>(i)) ? 1 : 0;
    DrawRectangle(static_cast<int>(x) - 10, static_cast<int>(y), 330, 26 + rows * 18, Fade({236, 231, 214, 255}, 0.85f));
    DrawText(title, static_cast<int>(x), static_cast<int>(y) + 6, 16, {60, 64, 52, 255});
    float yy = y + 26;
    for (std::size_t i = 0; i < kGeneCount; ++i) {
        const auto gene = static_cast<Gene>(i);
        if (!geneAllowed(type, gene))
            continue;
        const GeneInfo& info = geneInfo(gene);
        Color col = DARKGRAY;
        const char* mark = "";
        if (other && std::abs(other->v[i] - g.v[i]) > 0.005f) {
            const bool up = g.v[i] > other->v[i];
            col = up ? Color{40, 120, 40, 255} : Color{160, 50, 40, 255};
            mark = up ? "  +" : "  -";
        }
        if (gene == Gene::MatingType) {
            DrawText(TextFormat("%-13s %s%s", info.name, g.v[i] > 0.5f ? "II" : "I", mark), static_cast<int>(x),
                     static_cast<int>(yy), 14, col);
        } else {
            const float t = (g.v[i] - info.min) / (info.max - info.min);
            DrawText(TextFormat("%-13s %.2f%s", info.name, g.v[i], mark), static_cast<int>(x), static_cast<int>(yy), 14, col);
            DrawRectangle(static_cast<int>(x) + 190, static_cast<int>(yy) + 3, 110, 8, Fade(GRAY, 0.3f));
            DrawRectangle(static_cast<int>(x) + 190, static_cast<int>(yy) + 3, static_cast<int>(110 * t), 8, col);
        }
        yy += 18;
    }
}

} // namespace

void drawRunHud(const Run& run, const World& world) {
    const Census& c = run.census();
    const float x = GetScreenWidth() - 330.0f;
    DrawRectangle(static_cast<int>(x) - 10, 10, 330, 116, Fade({236, 231, 214, 255}, 0.85f));
    float share = c.share();
    GuiProgressBar({x + 70, 18, 180, 16}, "dominio", TextFormat("%.1f%% / %.0f%%", 100 * share, 100 * run.config().targetShare),
                   &share, 0.0f, run.config().targetShare);
    DrawText(TextFormat("Linaje: %d celulas | generacion %u", c.lineageCells, c.maxGeneration), static_cast<int>(x), 42, 16, DARKGRAY);
    DrawText(TextFormat("Par %d Eug %d Ame %d Chl %d Did %d Vor %d", c.species[0], c.species[1], c.species[2],
                        c.species[3], c.species[4], c.species[5]),
             static_cast<int>(x), 62, 16, DARKGRAY);
    DrawText(TextFormat("Ste %d Pod %d Eup %d Arc %d Lac %d", c.species[6], c.species[7], c.species[8], c.species[9],
                        c.species[10]),
             static_cast<int>(x), 102, 16, DARKGRAY);
    DrawText(TextFormat("Bacterias %d | %.0f s (%.1f h bio) | div. %d", c.bacteria, run.elapsed(),
                        run.elapsed() * units::kBioTimeScale / 3600.0f, run.divisions()),
             static_cast<int>(x), 82, 16, DARKGRAY);

    // Genoma: el de la candidata durante una elección (comparado con la otra), si no el propio.
    if (run.simulationPaused() && !run.candidates().empty()) {
        const auto& cands = run.candidates();
        const entt::entity sel = cands[run.selected()];
        const entt::entity alt = cands.size() == 2 ? cands[1 - run.selected()] : entt::null;
        drawGenome(world, sel, alt, x, 132, cands.size() == 2 ? "Genoma de esta hija (vs. la otra)" : "Genoma del descendiente");
    } else {
        drawGenome(world, world.player(), entt::null, x, 132, "Genoma");
        if (world.conjugationPartner(world.player()) != entt::null)
            centered("Pareja compatible cerca: V para conjugar", GetScreenHeight() - 90.0f, 20, {60, 90, 140, 255});
    }

    if (run.won())
        centered("Tu linaje domina la gota!  (segui jugando o N para una partida nueva)", 20, 22, {40, 110, 40, 255});
    if (run.state() == Run::State::ChoosingDaughter)
        centered("Te dividiste: elegi con que hija seguir  (Tab / flechas / click, Enter)", GetScreenHeight() - 60.0f, 24, {150, 80, 10, 255});
    if (run.state() == Run::State::ChoosingHeir)
        centered(TextFormat("Tu celula murio (%s): elegi un descendiente  (Tab / click, Enter)", run.deathCause()),
                 GetScreenHeight() - 60.0f, 24, {150, 40, 30, 255});
}

void drawGameOver(const Run& run, const Progress& progress, const std::vector<LocomotionType>& unlocked) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.45f));
    const float cy = GetScreenHeight() * 0.5f;
    centered("Tu linaje se extinguio", cy - 120, 48, RAYWHITE);
    centered(TextFormat("Ultima muerte: %s", run.deathCause()), cy - 60, 22, RAYWHITE);
    centered(TextFormat("Generacion maxima %u | mejor dominio %.1f%% | %d divisiones | %.0f s", run.bestGeneration(),
                        100 * run.bestShare(), run.divisions(), run.elapsed()),
             cy - 25, 22, RAYWHITE);
    float y = cy + 20;
    for (LocomotionType t : unlocked) {
        centered(TextFormat("Nueva especie desbloqueada: %s", speciesName(t)), y, 24, {250, 210, 120, 255});
        y += 32;
    }
    centered(TextFormat("Partidas jugadas: %d", progress.runs), y + 10, 18, LIGHTGRAY);
    centered("Enter: volver al inicio", y + 50, 22, RAYWHITE);
}
