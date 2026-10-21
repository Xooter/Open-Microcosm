#include "game/progress.hpp"

#include "core/paths.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

std::string defaultOptionsPath() {
    // Placas, escenas, tinciones y exportaciones viven al lado (core/paths: según la plataforma).
    return (std::filesystem::path(userDataDir()) / "options.txt").string();
}

Options loadOptions(const std::string& path) {
    Options o;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        std::istringstream value(line.substr(eq + 1));
        if (key == "fullscreen") value >> o.fullscreen;
        else if (key == "maximized") value >> o.maximized;
        else if (key == "post_process") value >> o.postProcess;
        else if (key == "optic_flaws") value >> o.opticFlaws;
        else if (key == "lod") value >> o.lod;
        else if (key == "gpu_physics") value >> o.gpuPhysics;
        else if (key == "language") value >> o.language;
        else if (key == "ui_scale") value >> o.uiScale;
        else if (key == "bio_time_scale") value >> o.bioTimeScale;
        else if (key == "graphics_preset") {
            int p = 0;
            value >> p;
            o.preset = static_cast<GraphicsPreset>(std::clamp(p, 0, 3));
        } else if (key == "vsync") value >> o.vsync;
        else if (key == "fps_limit") value >> o.fpsLimit;
        else if (key == "msaa") value >> o.msaa;
        else if (key == "render_scale") value >> o.renderScale;
        else if (key == "max_detailed") value >> o.maxDetailed;
        else if (key == "cell_detail") value >> o.cellDetail;
        else if (key == "background_saver") value >> o.backgroundSaver;
        else if (key == "threads") value >> o.threads;
        else if (key == "lod_stride") value >> o.lodStride;
        else if (key == "max_steps_per_frame") value >> o.maxStepsPerFrame;
        else if (key == "model_view") value >> o.modelView;
        else if (key == "tool") value >> o.tool;
    }
    o.bioTimeScale = std::clamp(o.bioTimeScale, 1.0f, 600.0f);
    o.fpsLimit = std::clamp(o.fpsLimit, 0, 1000);
    o.renderScale = std::clamp(o.renderScale, 0.5f, 1.0f);
    o.uiScale = o.uiScale > 0.0f ? std::clamp(o.uiScale, 0.75f, 3.0f) : 0.0f;
    o.maxDetailed = std::clamp(o.maxDetailed, 0, 4096);
    o.cellDetail = std::clamp(o.cellDetail, 1.0f, 4.0f);
    o.threads = std::clamp(o.threads, 0, 256);
    o.lodStride = std::clamp(o.lodStride, 1, 10);
    o.maxStepsPerFrame = std::clamp(o.maxStepsPerFrame, 1, 16);
    o.tool = std::max(0, o.tool); // el tope lo pone la App (conoce Tool)
    return o;
}

void applyGraphicsPreset(Options& o, GraphicsPreset preset) {
    o.preset = preset;
    switch (preset) {
    case GraphicsPreset::Low:
        o.renderScale = 0.67f;
        o.postProcess = false;
        o.opticFlaws = false;
        o.maxDetailed = 64;
        o.cellDetail = 2.5f;
        o.msaa = false;
        o.maxStepsPerFrame = 1; // imagen fluida: la simulación va más lenta que el reloj si no llega
        break;
    case GraphicsPreset::Medium:
        o.renderScale = 0.85f;
        o.postProcess = true;
        o.opticFlaws = true;
        o.maxDetailed = 128;
        o.cellDetail = 1.5f;
        o.msaa = false;
        o.maxStepsPerFrame = 4;
        break;
    case GraphicsPreset::High:
        o.renderScale = 1.0f;
        o.postProcess = true;
        o.opticFlaws = true;
        o.maxDetailed = 256;
        o.cellDetail = 1.0f;
        o.msaa = true;
        o.maxStepsPerFrame = 4;
        break;
    case GraphicsPreset::Custom:
        break;
    }
}

bool saveOptions(const Options& o, const std::string& path) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return false;
    out << "fullscreen=" << o.fullscreen << '\n'
        << "maximized=" << o.maximized << '\n'
        << "post_process=" << o.postProcess << '\n'
        << "optic_flaws=" << o.opticFlaws << '\n'
        << "lod=" << o.lod << '\n'
        << "gpu_physics=" << o.gpuPhysics << '\n'
        << "language=" << o.language << '\n'
        << "ui_scale=" << o.uiScale << '\n'
        << "bio_time_scale=" << o.bioTimeScale << '\n'
        << "graphics_preset=" << static_cast<int>(o.preset) << '\n'
        << "vsync=" << o.vsync << '\n'
        << "fps_limit=" << o.fpsLimit << '\n'
        << "msaa=" << o.msaa << '\n'
        << "render_scale=" << o.renderScale << '\n'
        << "max_detailed=" << o.maxDetailed << '\n'
        << "cell_detail=" << o.cellDetail << '\n'
        << "background_saver=" << o.backgroundSaver << '\n'
        << "threads=" << o.threads << '\n'
        << "lod_stride=" << o.lodStride << '\n'
        << "max_steps_per_frame=" << o.maxStepsPerFrame << '\n'
        << "model_view=" << o.modelView << '\n'
        << "tool=" << o.tool << '\n';
    return static_cast<bool>(out);
}
