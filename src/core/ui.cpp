#include "core/ui.hpp"

#include "bio/division.hpp"
#include "game/biome.hpp"
#include "game/encyclopedia.hpp"
#include "game/progress.hpp"
#include "game/run.hpp"
#include "game/world.hpp"
#include "render/cell_renderer.hpp"
#include "render/minerals.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>

namespace {

const char* speciesName(LocomotionType t) {
    switch (t) {
    case LocomotionType::Cilia: return "Paramecium (cilios)";
    case LocomotionType::Flagellum: return "Euglena (flagelo)";
    case LocomotionType::Pseudopod: return "Amoeba (pseudopodos)";
    case LocomotionType::Undulation: return "Rhabditis (ondulacion)";
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
    centered("Sandbox de vida acuatica microscopica: simulacion fisica y biologia real.", cy - 150, 22, {90, 92, 80, 255});
    const char* labels[] = {"Nueva gota", "Enciclopedia", "Opciones", "Salir"};
    const TitleAction actions[] = {TitleAction::NewDrop, TitleAction::Encyclopedia, TitleAction::Options, TitleAction::Quit};
    for (int i = 0; i < 4; ++i)
        if (GuiButton({cx - 160, cy - 110 + i * 58.0f, 320, 48}, labels[i]))
            action = actions[i];
    if (IsKeyPressed(KEY_ENTER))
        action = TitleAction::NewDrop;
    int species = 0;
    for (int k = 0; k < kSpeciesCount; ++k)
        species += (progress.seenSpecies >> k) & 1u;
    centered(TextFormat("Enciclopedia: %d/%d especies vistas", species, kSpeciesCount), cy + 150, 18, {90, 92, 80, 255});
    centered("Codigo abierto, con fines educativos y cientificos", cy + 180, 16, {110, 112, 100, 255});
    EndDrawing();
    return action;
}

SetupAction drawSetupScreen(const Progress& progress, SetupState& state) {
    SetupAction action = SetupAction::None;
    BeginDrawing();
    ClearBackground({236, 231, 214, 255});
    const float cx = GetScreenWidth() * 0.5f;
    centered("Nueva gota: elegir el ambiente", 40, 40, {60, 64, 52, 255});
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
    // Ser que se controla al empezar (despues se puede controlar cualquier otra celula con P).
    const LocomotionType types[] = {LocomotionType::Cilia, LocomotionType::Flagellum, LocomotionType::Pseudopod};
    for (int i = 0; i < 3; ++i) {
        const LocomotionType t = types[i];
        const Rectangle r = {cx - 220, 420 + i * 62.0f, 440, 52};
        if (GuiButton(r, TextFormat("%d  %s", i + 1, speciesName(t))))
            state.founder = t;
        if (IsKeyPressed(KEY_ONE + i))
            state.founder = t;
        if (state.founder == t)
            DrawRectangleLinesEx(r, 3, {60, 110, 60, 255});
    }
    DrawText(TextFormat("Semilla %u", state.seed), static_cast<int>(cx - 220), 620, 20, {80, 82, 70, 255});
    if (GuiButton({cx - 40, 612, 120, 36}, "Otra"))
        state.seed = static_cast<unsigned>(GetRandomValue(1, 2147483646));
    GuiCheckBox({cx + 100, 622, 20, 20}, "Gota vacia", &state.emptyDrop);
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
    GuiCheckBox({cx - 200, y, 24, 24}, "Fisica de celulas en la GPU", &o.gpuPhysics);
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

// Genes de la célula `cell`; si `compare` es válida, marca las diferencias con ella. Sin
// `framed` solo dibuja las filas (dentro de otro panel). Devuelve el alto usado.
float drawGenome(const World& world, entt::entity cell, entt::entity compare, float x, float y, const char* title,
                 bool framed = true) {
    const entt::registry& reg = world.registry();
    if (!reg.valid(cell) || !reg.all_of<Heredity, Locomotion>(cell))
        return 0.0f;
    const Genome& g = reg.get<Heredity>(cell).genome;
    const Genome* other = reg.valid(compare) && reg.all_of<Heredity>(compare) ? &reg.get<Heredity>(compare).genome : nullptr;
    const LocomotionType type = reg.get<Locomotion>(cell).type;
    int rows = 0;
    for (std::size_t i = 0; i < kGeneCount; ++i)
        rows += geneAllowed(type, static_cast<Gene>(i)) ? 1 : 0;
    if (framed) {
        DrawRectangle(static_cast<int>(x) - 10, static_cast<int>(y), 330, 26 + rows * 18, Fade({236, 231, 214, 255}, 0.85f));
        DrawText(title, static_cast<int>(x), static_cast<int>(y) + 6, 16, {60, 64, 52, 255});
    }
    float yy = y + (framed ? 26 : 0);
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
    return (framed ? 26.0f : 0.0f) + static_cast<float>(rows) * 18.0f;
}

} // namespace

void drawControlHud(const World& world, bool showGenome) {
    if (!showGenome)
        return;
    const float x = GetScreenWidth() - 330.0f;
    if (!world.registry().valid(world.player())) {
        DrawText("Sin ser controlado (P: controlar la celula bajo el cursor)", static_cast<int>(x) - 80, 12, 16, DARKGRAY);
        return;
    }
    drawGenome(world, world.player(), entt::null, x, 12, "Genoma del ser controlado");
    if (world.conjugationPartner(world.player()) != entt::null)
        centered("Pareja compatible cerca: V para conjugar", GetScreenHeight() - 170.0f, 20, {60, 90, 140, 255});
}

// ---------------------------------------------------------------------------
// Sandbox
// ---------------------------------------------------------------------------

namespace {

constexpr float kIcon = 44.0f, kIconGap = 6.0f, kTabH = 26.0f, kButtonsH = 26.0f;

// Qué seres del catálogo hay en cada categoría (se arma una vez).
const std::vector<int>& categoryItems(int category) {
    static const std::vector<std::vector<int>> table = [] {
        std::vector<std::vector<int>> t(kCategoryCount);
        for (int i = 0; i < kBeingCount; ++i)
            t[static_cast<std::size_t>(categoryOf(beingAt(i)))].push_back(i);
        return t;
    }();
    return table[static_cast<std::size_t>(category)];
}

// Cuántos seres de la categoría se ven: en modo Forma solo las especies (lo único a lo que se
// puede transformar al jugador).
int visibleCount(int category, bool reshape) {
    int n = 0;
    for (int i : categoryItems(category))
        n += !reshape || beingAt(i).kind == BeingKind::Species ? 1 : 0;
    return n;
}

// Barra inferior, de abajo hacia arriba: una fila de íconos de la categoría abierta, la fila de
// pestañas de categoría y la fila de botones (modo, reproducir, explorar).
float iconsTop() { return static_cast<float>(GetScreenHeight()) - 10.0f - kIcon; }
float tabsTop() { return iconsTop() - 8.0f - kTabH; }
float buttonsTop() { return tabsTop() - 6.0f - kButtonsH; }

Rectangle sandboxIconRect(int slot) {
    return {10.0f + static_cast<float>(slot) * (kIcon + kIconGap), iconsTop(), kIcon, kIcon};
}

Rectangle sandboxBarRect() {
    const float top = buttonsTop() - 6.0f;
    return {0, top, static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) - top};
}

Color withAlpha(Color c, bool enabled) {
    c.a = static_cast<unsigned char>(enabled ? c.a : c.a / 3);
    return c;
}

// Silueta de la especie (la superelipse de su CellSpec), escalada para entrar en el ícono.
void drawCellGlyph(const CellSpec& spec, Vector2 c, float scale, bool enabled) {
    constexpr int kN = 36;
    Vector2 pts[kN];
    for (int i = 0; i < kN; ++i) {
        const float t = 2.0f * units::kPi * static_cast<float>(i) / kN;
        const Vec2 q = superellipsePoint(spec.radiusX * scale, spec.radiusY * scale, spec.squareness, t, spec.taper);
        pts[i] = {c.x + q.x, c.y + q.y};
    }
    const Color fill = withAlpha({spec.r, spec.g, spec.b, 230}, enabled);
    for (int i = 0; i < kN; ++i) // en sentido antihorario en pantalla (raylib descarta el otro)
        DrawTriangle(c, pts[(i + 1) % kN], pts[i], fill);
    const Color edge = withAlpha({60, 64, 52, 220}, enabled);
    for (int i = 0; i < kN; ++i)
        DrawLineEx(pts[i], pts[(i + 1) % kN], 1.2f, edge);
    const float nucleus = std::max(1.5f, spec.nucleusFraction * std::min(spec.radiusX, spec.radiusY) * scale * 0.6f);
    DrawCircleV(c, nucleus, withAlpha({110, 105, 90, 200}, enabled));
}

void drawBeingIcon(Being b, Rectangle r, bool enabled) {
    const Vector2 c = {r.x + 0.5f * r.width, r.y + 0.5f * r.height};
    const float half = 0.4f * r.width;
    switch (b.kind) {
    case BeingKind::Species: {
        const SpeciesInfo& info = speciesInfo(b.species);
        const CellSpec& spec = info.spec;
        if (info.filament) { // tres células unidas extremo con extremo
            const float s = half / (3.0f * spec.radiusX);
            for (int k = -1; k <= 1; ++k)
                drawCellGlyph(spec, {c.x + 2.0f * spec.radiusX * s * static_cast<float>(k), c.y}, s, enabled);
        } else {
            drawCellGlyph(spec, c, half / std::max(spec.radiusX, spec.radiusY), enabled);
        }
        break;
    }
    case BeingKind::Bacterium:
    case BeingKind::Violaceum: {
        const Color col = withAlpha(b.kind == BeingKind::Violaceum ? Color{110, 60, 150, 255} : Color{120, 110, 80, 255}, enabled);
        const float pos[5][3] = {{-10, -8, 20}, {6, -10, -35}, {-4, 6, 70}, {10, 8, 10}, {0, -1, -60}};
        for (const auto& p : pos)
            DrawRectanglePro({c.x + p[0], c.y + p[1], 9, 4}, {4.5f, 2}, p[2], col);
        break;
    }
    case BeingKind::Bdellovibrio: {
        const Color col = withAlpha({90, 90, 70, 255}, enabled);
        const float pos[4][2] = {{-9, -6}, {7, -8}, {-3, 8}, {10, 6}};
        for (const auto& p : pos) {
            DrawCircleV({c.x + p[0], c.y + p[1]}, 2.0f, col);
            DrawLineEx({c.x + p[0], c.y + p[1]}, {c.x + p[0] - 5, c.y + p[1] + 2}, 1.0f, col);
        }
        break;
    }
    case BeingKind::Oscillatoria: // tricoma continuo con paredes transversales
        for (int k = 0; k < 8; ++k) {
            const float x = c.x - 16 + 4.0f * static_cast<float>(k), y = c.y + 6.0f * std::sin(0.5f * static_cast<float>(k));
            const float x2 = x + 4.0f, y2 = c.y + 6.0f * std::sin(0.5f * static_cast<float>(k + 1));
            DrawLineEx({x, y}, {x2, y2}, 5.0f, withAlpha({58, 104, 100, 255}, enabled));
            DrawLineEx({x2, y2 - 2.5f}, {x2, y2 + 2.5f}, 0.8f, withAlpha({36, 70, 68, 255}, enabled));
        }
        break;
    case BeingKind::Cyanobacterium:
        for (int k = 0; k < 7; ++k) {
            const bool heterocyst = k == 3;
            DrawCircleV({c.x - 15 + 5.0f * static_cast<float>(k), c.y}, heterocyst ? 3.2f : 2.5f,
                        withAlpha(heterocyst ? Color{170, 180, 120, 255} : Color{60, 120, 90, 255}, enabled));
        }
        break;
    case BeingKind::Chytrid: {
        const Color col = withAlpha({130, 120, 95, 255}, enabled);
        const float pos[3][2] = {{-8, -4}, {6, -8}, {2, 8}};
        for (const auto& p : pos) {
            DrawCircleV({c.x + p[0], c.y + p[1]}, 3.0f, col);
            DrawLineEx({c.x + p[0] - 3, c.y + p[1]}, {c.x + p[0] - 10, c.y + p[1] + 3}, 1.0f, col);
        }
        break;
    }
    case BeingKind::Saprolegnia:
    case BeingKind::Zoophagus: {
        const Color col = withAlpha({120, 115, 95, 255}, enabled);
        const Vector2 base = {c.x - 14, c.y + 10};
        const Vector2 mid = {c.x - 2, c.y};
        DrawLineEx(base, mid, 1.6f, col);
        DrawLineEx(mid, {c.x + 14, c.y - 12}, 1.6f, col);
        DrawLineEx(mid, {c.x + 6, c.y + 12}, 1.4f, col);
        DrawLineEx({c.x + 6, c.y - 5}, {c.x - 4, c.y - 14}, 1.2f, col);
        if (b.kind == BeingKind::Zoophagus) // ramitas cortas adhesivas (las trampas)
            for (const Vector2 k : {Vector2{c.x + 14, c.y - 12}, Vector2{c.x + 6, c.y + 12}, Vector2{c.x - 4, c.y - 14}})
                DrawCircleV(k, 2.2f, col);
        else
            DrawEllipse(static_cast<int>(c.x + 14), static_cast<int>(c.y - 12), 2.5f, 4.0f, col); // zoosporangio
        break;
    }
    case BeingKind::Phage: {
        const Color col = withAlpha({80, 80, 90, 255}, enabled);
        DrawPolyLinesEx({c.x, c.y - 7}, 6, 7.0f, 0.0f, 1.6f, col); // cápside
        DrawLineEx({c.x, c.y}, {c.x, c.y + 10}, 2.0f, col);       // cola
        DrawLineEx({c.x, c.y + 10}, {c.x - 6, c.y + 15}, 1.0f, col);
        DrawLineEx({c.x, c.y + 10}, {c.x + 6, c.y + 15}, 1.0f, col);
        break;
    }
    case BeingKind::Clay: { // un flóculo de partículas finas
        const float pos[7][2] = {{-6, -3}, {-2, -6}, {3, -4}, {6, 1}, {1, 4}, {-4, 3}, {-1, -1}};
        for (const auto& p : pos)
            drawMineral(MineralKind::Clay, {c.x + p[0], c.y + p[1]}, 2.6f, 0.0f, 0.0f);
        break;
    }
    case BeingKind::Quartz:
    case BeingKind::Mica:
    case BeingKind::IronOxide:
    case BeingKind::Sulfur:
        drawMineral(beingMineral(b.kind), c, 0.7f * half, 0.6f, 0.0f);
        break;
    case BeingKind::Frustule: drawMineral(MineralKind::Frustule, c, half, -0.5f, 0.0f); break;
    case BeingKind::Rock: {
        Vector2 pts[32];
        for (int i = 0; i < 32; ++i) {
            const float t = 6.2831853f * static_cast<float>(i) / 32.0f;
            const float rr = half * (1.0f + 0.12f * std::cos(2.0f * t + 0.7f) + 0.05f * std::cos(3.0f * t + 2.0f));
            pts[i] = {c.x + rr * std::cos(t), c.y + rr * std::sin(t)};
        }
        drawRock(pts, 32, c, MineralKind::Quartz, 12345u, 2.2f);
        break;
    }
    case BeingKind::Sediment:
        drawMineral(MineralKind::Quartz, {c.x - 6, c.y - 5}, 7.0f, 0.3f, 0.0f);
        drawMineral(MineralKind::IronOxide, {c.x + 8, c.y + 2}, 5.0f, 1.1f, 0.0f);
        drawMineral(MineralKind::Mica, {c.x - 5, c.y + 9}, 5.0f, 0.2f, 0.0f);
        for (const float p : {-10.0f, 2.0f, 10.0f})
            drawMineral(MineralKind::Clay, {c.x + p, c.y - 11 + 0.5f * p}, 1.6f, 0.0f, 0.0f);
        break;
    case BeingKind::Count: break;
    }
}

// Tamaño aproximado del ser [µm] para el fantasma bajo el cursor.
float beingReach(Being b) {
    if (b.kind == BeingKind::Rock)
        return 65.0f;
    if (b.kind == BeingKind::Sediment)
        return 25.0f;
    if (b.kind != BeingKind::Species)
        return 12.0f;
    const SpeciesInfo& info = speciesInfo(b.species);
    return std::max(info.spec.radiusX, info.spec.radiusY);
}

} // namespace

bool sandboxBarHovered() { return CheckCollisionPointRec(GetMousePosition(), sandboxBarRect()); }

bool drawSandboxBar(World& world, SandboxState& state, const Camera2D& camera) {
    // Fantasma del ser armado bajo el cursor (en el mundo).
    if (state.armed >= 0 && !sandboxBarHovered()) {
        const Being b = beingAt(state.armed);
        const Vector2 m = GetScreenToWorld2D(GetMousePosition(), camera);
        BeginMode2D(camera);
        DrawRing(m, beingReach(b), beingReach(b) + 2.0f / camera.zoom, 0, 360, 64, {60, 110, 60, 200});
        EndMode2D();
        DrawText(beingName(b), GetMouseX() + 14, GetMouseY() + 14, 18, {60, 110, 60, 255});
    }

    const Rectangle bar = sandboxBarRect();
    DrawRectangleRec(bar, Fade({236, 231, 214, 255}, 0.85f));
    // Modos y reproducción.
    const float by = bar.y + 6.0f;
    bool spawnOn = !state.reshape, formOn = state.reshape;
    GuiToggle({10, by, 110, 26}, "Spawnear", &spawnOn);
    GuiToggle({124, by, 110, 26}, "Forma", &formOn);
    const bool reshape = state.reshape ? !spawnOn : formOn; // un modo activo no se apaga solo
    if (reshape != state.reshape) {
        state.reshape = reshape;
        state.armed = -1;
    }
    if (!world.registry().valid(world.player()))
        GuiDisable();
    if (GuiButton({246, by, 150, 26}, "Reproducir (R)"))
        world.forceReproduction(world.player());
    GuiEnable();
    const char* hint = state.reshape ? "Forma: click en una especie para transformar al jugador"
                       : state.armed >= 0 ? TextFormat("%s: click en la placa para spawnear | Esc cancela", beingName(beingAt(state.armed)))
                                          : "Click en un ser y despues en la placa";
    DrawText(TextFormat("SANDBOX | %s", hint), 410, static_cast<int>(by + 5), 18, {80, 82, 70, 255});
    const bool toExplore = GuiButton({static_cast<float>(GetScreenWidth()) - 160.0f, by, 150, 26}, "Explorar (Tab)");

    // Pestañas de categoría. En modo Forma solo sirven las que tienen especies.
    {
        float x = 10.0f;
        for (int c = 0; c < kCategoryCount; ++c) {
            const char* label = TextFormat("%s (%d)", categoryName(static_cast<BeingCategory>(c)), visibleCount(c, state.reshape));
            const float w = static_cast<float>(MeasureText(label, 10)) + 22.0f;
            const bool usable = visibleCount(c, state.reshape) > 0;
            bool on = state.category == c;
            if (!usable)
                GuiDisable();
            // GuiToggle (raygui 4.0) siempre devuelve 0: el click se ve en que cambió `on`.
            GuiToggle({x, tabsTop(), w, kTabH}, label, &on);
            GuiEnable();
            if (usable && on != (state.category == c))
                state.category = c;
            x += w + 4.0f;
        }
        if (visibleCount(state.category, state.reshape) == 0) { // se pasó a Forma desde una categoría sin especies
            for (int c = 0; c < kCategoryCount; ++c)
                if (visibleCount(c, state.reshape) > 0) {
                    state.category = c;
                    break;
                }
        }
    }

    // Íconos de la categoría abierta.
    const char* tooltip = nullptr;
    int slot = 0;
    for (int i : categoryItems(state.category)) {
        const Being b = beingAt(i);
        if (state.reshape && b.kind != BeingKind::Species)
            continue;
        const Rectangle r = sandboxIconRect(slot++);
        const bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, hover ? Color{250, 247, 236, 255} : Color{244, 240, 226, 255});
        DrawRectangleLinesEx(r, state.armed == i ? 3.0f : 1.0f, state.armed == i ? Color{60, 110, 60, 255} : Color{170, 165, 145, 255});
        drawBeingIcon(b, r, true);
        if (hover)
            tooltip = beingName(b);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (state.reshape) {
                const Vector2 mid = GetScreenToWorld2D({0.5f * GetScreenWidth(), 0.5f * GetScreenHeight()}, camera);
                world.setPlayerSpecies(b.species, {mid.x, mid.y});
            } else {
                state.armed = state.armed == i ? -1 : i;
            }
        }
    }
    if (tooltip) {
        const int w = MeasureText(tooltip, 18);
        const int x = std::clamp(GetMouseX() - w / 2, 4, GetScreenWidth() - w - 12);
        const int y = static_cast<int>(bar.y) - 28;
        DrawRectangle(x - 6, y - 4, w + 12, 26, Fade({60, 64, 52, 255}, 0.9f));
        DrawText(tooltip, x, y, 18, {244, 240, 226, 255});
    }
    return toExplore;
}

// ---------------------------------------------------------------------------
// Modo Explorar
// ---------------------------------------------------------------------------

namespace {

constexpr int kFont = 14;
constexpr float kPanelW = 330.0f, kPanelTop = 132.0f;
constexpr Color kInk{60, 64, 52, 255}, kDim{110, 112, 98, 255}, kAccent{50, 90, 50, 255};

Rectangle inspectorRect() {
    return {static_cast<float>(GetScreenWidth()) - kPanelW - 10.0f, kPanelTop, kPanelW,
            static_cast<float>(GetScreenHeight()) - kPanelTop - 10.0f};
}

Rectangle exploreExitRect() { return {0.5f * static_cast<float>(GetScreenWidth()) - 70.0f, 36.0f, 140.0f, 24.0f}; }

// Parte `text` en renglones de a lo sumo `width` píxeles (por palabras).
void wrapText(const std::string& text, int size, float width, std::vector<std::string>& out) {
    std::size_t pos = 0;
    std::string line;
    while (pos < text.size()) {
        std::size_t end = text.find(' ', pos);
        if (end == std::string::npos)
            end = text.size();
        const std::string word = text.substr(pos, end - pos);
        const std::string tryLine = line.empty() ? word : line + " " + word;
        if (!line.empty() && static_cast<float>(MeasureText(tryLine.c_str(), size)) > width) {
            out.push_back(line);
            line = word;
        } else {
            line = tryLine;
        }
        pos = end + 1;
    }
    if (!line.empty())
        out.push_back(line);
}

// Contenido de la ficha desde (x, y); con `draw` falso solo mide. Devuelve el alto.
float inspectorContent(const World& world, const ExploreState& st, float x, float y, float width, bool draw) {
    const Inspection& ins = st.inspection;
    const float y0 = y;
    std::vector<std::string> rows;
    auto paragraph = [&](const std::string& text, int size, Color color) {
        rows.clear();
        wrapText(text, size, width, rows);
        for (const std::string& r : rows) {
            if (draw)
                DrawText(r.c_str(), static_cast<int>(x), static_cast<int>(y), size, color);
            y += static_cast<float>(size) + 3.0f;
        }
    };
    if (draw)
        DrawText(ins.title.c_str(), static_cast<int>(x), static_cast<int>(y), 20, ins.dead ? Color{150, 50, 40, 255} : kInk);
    y += 24.0f;
    paragraph(ins.subtitle, kFont, kDim);
    y += 4.0f;
    paragraph(ins.blurb, kFont, kInk);
    y += 6.0f;
    // Columna de rótulos: la del rótulo más largo (y un tope, para que el valor tenga lugar).
    float labelW = 80.0f;
    for (const InspectSection& s : ins.sections)
        for (const InspectLine& l : s.lines)
            if (!l.label.empty())
                labelW = std::max(labelW, static_cast<float>(MeasureText(l.label.c_str(), kFont)) + 14.0f);
    labelW = std::min(labelW, 0.45f * width);
    for (const InspectSection& s : ins.sections) {
        if (draw) {
            DrawText(s.title.c_str(), static_cast<int>(x), static_cast<int>(y), 15, kAccent);
            DrawLine(static_cast<int>(x), static_cast<int>(y) + 17, static_cast<int>(x + width), static_cast<int>(y) + 17, Fade(kAccent, 0.4f));
        }
        y += 22.0f;
        for (const InspectLine& l : s.lines) {
            rows.clear();
            if (l.label.empty()) {
                wrapText(l.value, kFont, width - 10.0f, rows);
            } else {
                wrapText(l.value, kFont, width - labelW, rows);
                if (draw)
                    DrawText(TextFormat("%s", l.label.c_str()), static_cast<int>(x), static_cast<int>(y), kFont, kDim);
            }
            for (const std::string& r : rows) {
                if (draw)
                    DrawText(r.c_str(), static_cast<int>(x + (l.label.empty() ? 10.0f : labelW)), static_cast<int>(y), kFont, kInk);
                y += static_cast<float>(kFont) + 3.0f;
            }
            y += 2.0f;
        }
        y += 6.0f;
    }
    if (!ins.live && !ins.dead)
        paragraph("Foto del momento del click: las bacterias y las particulas se reordenan en memoria y no se pueden seguir.", 12, kDim);
    if (ins.hasGenome && !ins.dead) {
        if (draw) {
            DrawText("Genoma", static_cast<int>(x), static_cast<int>(y), 15, kAccent);
            DrawLine(static_cast<int>(x), static_cast<int>(y) + 17, static_cast<int>(x + width), static_cast<int>(y) + 17, Fade(kAccent, 0.4f));
        }
        y += 22.0f;
        if (draw) {
            y += drawGenome(world, ins.entity, entt::null, x, y, "", false) + 6.0f;
        } else { // medir sin dibujar: una fila de 18 px por gen que esta especie puede tener
            const entt::registry& reg = world.registry();
            if (reg.valid(ins.entity) && reg.all_of<Locomotion>(ins.entity)) {
                const LocomotionType type = reg.get<Locomotion>(ins.entity).type;
                for (std::size_t i = 0; i < kGeneCount; ++i)
                    y += geneAllowed(type, static_cast<Gene>(i)) ? 18.0f : 0.0f;
            }
            y += 6.0f;
        }
    }
    return y - y0;
}

// Anillo alrededor de lo inspeccionado (el centro de lo vivo se lee cada cuadro: la ficha se rearma menos seguido).
Vec2 ringCenter(const World& world, const Picked& p, const Inspection& ins) {
    if ((p.kind == Picked::Kind::Cell || p.kind == Picked::Kind::Rock) && world.registry().valid(p.entity) &&
        world.registry().all_of<RingBody>(p.entity))
        return world.registry().get<RingBody>(p.entity).centroid;
    return ins.at;
}

void drawRing(Vec2 c, float r, float zoom, Color color, float thickness) {
    DrawRing({c.x, c.y}, r, r + thickness / zoom, 0, 360, 72, color);
}

} // namespace

bool exploreUiHovered(const ExploreState& state) {
    if (!state.active)
        return false;
    const Vector2 m = GetMousePosition();
    return CheckCollisionPointRec(m, exploreExitRect()) || (state.inspection.valid && CheckCollisionPointRec(m, inspectorRect()));
}

bool drawExplore(const World& world, ExploreState& st, ToolState& tools, const Camera2D& camera) {
    // Resaltado en el mundo: lo elegido (grueso) y lo que está bajo el cursor (fino).
    BeginMode2D(camera);
    if (st.inspection.valid && !st.inspection.dead) {
        // Lo que no tiene identidad estable (bacterias, partículas) es una foto: el anillo marca
        // dónde estaba al hacer click y se desvanece, porque el ser ya se movió.
        const float fade = st.picked.live() ? 1.0f : std::clamp(1.0f - (st.pickedAge - 0.6f) / 1.2f, 0.0f, 1.0f);
        if (fade > 0.0f)
            drawRing(ringCenter(world, st.picked, st.inspection), st.inspection.radius, camera.zoom,
                     Fade({200, 120, 20, 230}, fade), 3.5f);
    }
    const bool hoverIsPicked = st.hover.kind == st.picked.kind && st.hover.entity == st.picked.entity &&
                               st.hover.id == st.picked.id && (st.hover.live() || st.hover.index == st.picked.index);
    if (st.hover && !hoverIsPicked) {
        float r = 6.0f;
        Vec2 c = st.hover.at;
        if (st.hover.kind == Picked::Kind::Cell && world.registry().valid(st.hover.entity) &&
            world.registry().all_of<RingBody>(st.hover.entity)) {
            const RingBody& b = world.registry().get<RingBody>(st.hover.entity);
            c = b.centroid;
            r = 0.5f * std::max(b.aabbMax.x - b.aabbMin.x, b.aabbMax.y - b.aabbMin.y) + 3.0f;
        }
        drawRing(c, r, camera.zoom, {80, 80, 70, 150}, 1.5f);
    }
    EndMode2D();

    // Banner del modo.
    const char* banner = "EXPLORAR  |  click en un ser para inspeccionarlo  |  Esc cierra la ficha";
    const int bw = MeasureText(banner, 16);
    DrawRectangle(GetScreenWidth() / 2 - bw / 2 - 10, 8, bw + 20, 24, Fade({236, 231, 214, 255}, 0.85f));
    DrawText(banner, GetScreenWidth() / 2 - bw / 2, 12, 16, kInk);
    const bool exit = GuiButton(exploreExitRect(), "Salir (Tab)");

    // Tooltip del cursor.
    if (!st.hoverText.empty() && !exploreUiHovered(st)) {
        const int w = MeasureText(st.hoverText.c_str(), 16);
        const int x = std::min(GetMouseX() + 16, GetScreenWidth() - w - 14), y = GetMouseY() + 18;
        DrawRectangle(x - 6, y - 4, w + 12, 24, Fade({60, 64, 52, 255}, 0.9f));
        DrawText(st.hoverText.c_str(), x, y, 16, {244, 240, 226, 255});
    }

    // Ficha.
    if (!st.inspection.valid)
        return exit;
    const Rectangle panel = inspectorRect();
    DrawRectangleRec(panel, Fade({236, 231, 214, 255}, 0.92f));
    DrawRectangleLinesEx(panel, 1.0f, Fade(kInk, 0.5f));
    const float bodyTop = panel.y + 34.0f, bodyH = panel.height - 40.0f;
    const float width = panel.width - 24.0f;
    const float total = inspectorContent(world, st, 0.0f, 0.0f, width, false) + 8.0f;
    const float maxScroll = std::max(0.0f, total - bodyH);
    if (CheckCollisionPointRec(GetMousePosition(), panel))
        st.scroll -= GetMouseWheelMove() * 36.0f;
    st.scroll = std::clamp(st.scroll, 0.0f, maxScroll);

    // Botones: seguir con la cámara (células y granos) y cerrar.
    const bool followable = st.picked.kind == Picked::Kind::Cell || st.picked.kind == Picked::Kind::Rock;
    if (followable && !st.inspection.dead) {
        bool follow = tools.followInspected;
        GuiToggle({panel.x + 8, panel.y + 6, 110, 22}, "Seguir (F)", &follow); // devuelve 0 siempre: `follow` ya cambió
        tools.followInspected = follow;
    }
    if (GuiButton({panel.x + panel.width - 90.0f, panel.y + 6, 82, 22}, "Cerrar (Esc)")) {
        st.picked = {};
        st.inspection = {};
        tools.followInspected = false;
        return exit;
    }
    if (st.inspection.dead && st.inspection.successor != entt::null)
        DrawText("el control pasa a su sucesor", static_cast<int>(panel.x) + 124, static_cast<int>(panel.y) + 10, 12, kDim);

    BeginScissorMode(static_cast<int>(panel.x), static_cast<int>(bodyTop), static_cast<int>(panel.width), static_cast<int>(bodyH));
    inspectorContent(world, st, panel.x + 12.0f, bodyTop - st.scroll, width, true);
    EndScissorMode();
    if (maxScroll > 0.0f) { // barra de scroll
        const float th = std::max(24.0f, bodyH * bodyH / total);
        const float ty = bodyTop + (bodyH - th) * (st.scroll / maxScroll);
        DrawRectangle(static_cast<int>(panel.x + panel.width) - 6, static_cast<int>(ty), 4, static_cast<int>(th), Fade(kInk, 0.35f));
    }
    return exit;
}
