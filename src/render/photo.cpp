#include "render/photo.hpp"

#include "game/scale_path.hpp"
#include "game/world.hpp"
#include "render/renderer.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <ctime>
#include <fstream>

namespace {

std::string number(double v) {
    char buf[32];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

std::string quoted(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out + '"';
}

} // namespace

bool savePhoto(Renderer& renderer, const World& world, const Camera2D& view, Overlay overlay, int width, int height,
               const std::string& pngPath, const PhotoInfo& info) {
    RenderTexture2D target = LoadRenderTexture(width, height);
    Camera2D camera = view;
    camera.offset = {0.5f * static_cast<float>(width), 0.5f * static_cast<float>(height)};
    renderer.drawWorldTo(world, camera, overlay, target);
    // Barra de escala y rótulo, en la misma textura (abajo a la izquierda).
    const double umPerPixel = 1.0 / std::max(1e-6f, camera.zoom);
    const double bar = niceLength(static_cast<float>(0.1 * width * umPerPixel)); // el mismo largo redondo que la vista
    const int barPx = static_cast<int>(std::lround(bar / umPerPixel));
    const double hours = world.simTime() * world.bioTimeScale() / 3600.0;
    BeginTextureMode(target);
    // La fuente por defecto de raylib no tiene "µ": la imagen dice "um" y el JSON, µm.
    std::string label = formatLength(static_cast<float>(bar));
    if (const std::size_t mu = label.find("µ"); mu != std::string::npos)
        label.replace(mu, std::string("µ").size(), "u");
    const std::string stamp = "t = " + number(std::round(hours * 100.0) / 100.0) + " h bio   seed " + std::to_string(info.seed);
    const int x = 20, y = height - 34, barW = std::max(barPx, MeasureText(label.c_str(), 18));
    const int stampW = MeasureText(stamp.c_str(), 16);
    DrawRectangle(x - 8, y - 22, barW + stampW + 40, 44, Fade(BLACK, 0.55f));
    DrawRectangle(x, y + 8, barPx, 6, WHITE);
    DrawText(label.c_str(), x, y - 14, 18, WHITE);
    DrawText(stamp.c_str(), x + barW + 24, y - 2, 16, WHITE);
    EndTextureMode();
    Image img = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&img); // las render textures de OpenGL están invertidas en Y
    const bool ok = ExportImage(img, pngPath.c_str());
    UnloadImage(img);
    UnloadRenderTexture(target);
    const std::string base = pngPath.size() > 4 ? pngPath.substr(0, pngPath.size() - 4) : pngPath;
    std::ofstream json(base + ".json");
    char date[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    json << "{\n  \"formato\": \"openmicrocosm-foto\",\n  \"fecha\": " << quoted(date) << ",\n  \"imagen\": " << quoted(pngPath)
         << ",\n  \"ancho_campo\": {\"valor\": " << number(width * umPerPixel) << ", \"unidad\": \"µm\"},\n"
         << "  \"um_por_pixel\": " << number(umPerPixel) << ",\n  \"barra_de_escala\": {\"valor\": " << number(bar)
         << ", \"unidad\": \"µm\"},\n  \"tiempo_biologico_h\": " << number(hours) << ",\n  \"tiempo_simulado_s\": "
         << number(world.simTime()) << ",\n  \"seed\": " << info.seed << ",\n  \"placa\": " << quoted(info.plate)
         << ",\n  \"tincion\": " << quoted(info.stain) << ",\n  \"escena\": " << quoted(info.lessonOrScene)
         << ",\n  \"procedencia\": " << quoted(info.provenance)
         << ",\n  \"nota\": \"sin objetivos simulados: se da el ancho real del campo en vez de un aumento\"\n}\n";
    return ok && static_cast<bool>(json);
}
