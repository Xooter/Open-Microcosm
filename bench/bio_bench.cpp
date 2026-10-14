// bio_bench: mide el costo de World::step sin ventana, con una gota fija (fase 19).
// Imprime ms/paso, el censo y un checksum del estado: una optimización "exacta" no debe
// cambiarlo (verificarlo con -DBIO_STRICT_FP=ON: con -ffast-math cualquier cambio de código puede
// reasociar operaciones y alterar los últimos bits). Con -DBIO_PROFILE_LITE=ON (preset `lite`)
// desglosa el tiempo por PROFILE_SCOPE. Con --gpu abre una ventana oculta y resuelve XPBD en la GPU,
// como la app. Con --render dibuja cada paso medido en una textura de 1600 × 900 (la placa entera,
// o --zoom Z) y mide el dibujo en la GPU real (esperando con glFinish).
//
//   bio_bench [--biome N] [--seed S] [--wildlife W] [--steps N] [--warmup N] [--threads N]
//             [--no-focus] [--gpu] [--render] [--zoom Z] [--cell-detail S] [--scene archivo.plate]
//             [--soak N]
//
// Con --soak N (prueba larga) cada N pasos verifica que todo sea finito y que el balance de P, N,
// Si y Fe cierre, y registra la memoria; termina con código 2 si algo falla.
//
// Con --scene reproduce una escena guardada (placa + intervenciones con su paso, incluido el foco):
// bioma, seed, fauna y foco salen de la escena.
#include "core/profiling.hpp"
#include "core/finite.hpp"
#include "game/biome.hpp"
#include "game/run.hpp"
#include "game/mass_budget.hpp"
#include "game/scene.hpp"
#include "gpu/gpu_substep_solver.hpp"
#include "render/renderer.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <omp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace {

constexpr float kDt = 1.0f / 60.0f; // el mismo paso fijo que la app

struct Args {
    int biome = static_cast<int>(BiomeId::Pond);
    std::uint64_t seed = 7;
    float wildlife = 1.0f;
    int steps = 240;
    int warmup = 30;
    int threads = 0; // 0: OMP_NUM_THREADS o el valor por defecto de OpenMP
    bool focus = true;
    bool gpu = false;
    bool render = false;
    float zoom = 0.0f; // 0: la placa entera
    float cellDetail = 1.0f;
    std::string scene; // escena (.plate con intervenciones) a reproducir
    int soak = 0;      // > 0: cada `soak` pasos verifica posiciones finitas, balance y memoria
};

// Memoria residente [MB] (Linux; 0 si no se puede leer).
double residentMB() {
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f)
        return 0.0;
    long pages = 0, resident = 0;
    const int n = std::fscanf(f, "%ld %ld", &pages, &resident);
    std::fclose(f);
    return n == 2 ? static_cast<double>(resident) * 4096.0 / (1024.0 * 1024.0) : 0.0;
}

// Prueba larga: ¿algo dejó de ser finito o el balance de masa dejó de cerrar? Devuelve el problema
// ("" si todo bien).
std::string soakCheck(const World& world, const MassBudget& start) {
    const BodyParticles& bp = world.bodies();
    for (std::size_t s = 0; s < bp.capacity(); ++s)
        if (bp.alive[s] && (!finiteNumber(bp.x[s]) || !finiteNumber(bp.y[s])))
            return "partícula de cuerpo no finita (slot " + std::to_string(s) + ")";
    const Bacteria& bac = world.bacteria();
    for (std::size_t i = 0; i < bac.size(); ++i)
        if (!finiteNumber(bac.x[i]) || !finiteNumber(bac.y[i]))
            return "bacteria no finita (" + std::to_string(i) + ")";
    const MassBudget now = world.massBudget();
    for (int e = 0; e < kElementCount; ++e) {
        const auto el = static_cast<Element>(e);
        if (!conservedElement(el))
            continue;
        const double residual = budgetResidual(start, now, el);
        const double scale = std::max(1.0, std::abs(now.total(el)));
        if (!finiteNumber(residual) || std::abs(residual) / scale > 1e-3)
            return std::string("el balance de ") + elementName(el) + " no cierra (residuo " +
                   std::to_string(residual) + " pmol de " + std::to_string(now.total(el)) + ")";
    }
    return "";
}

// glFinish no está en rlgl; el GLAD de raylib lo carga (como en gpu_substep_solver.cpp).
extern "C" {
typedef void (*BioFinishFn)(void);
extern BioFinishFn glad_glFinish;
}

bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        const bool hasValue = i + 1 < argc;
        if (k == "--no-focus")
            a.focus = false;
        else if (k == "--gpu")
            a.gpu = true;
        else if (k == "--render")
            a.render = true;
        else if (k == "--zoom" && hasValue)
            a.zoom = static_cast<float>(std::atof(argv[++i]));
        else if (k == "--soak" && hasValue)
            a.soak = std::atoi(argv[++i]);
        else if (k == "--scene" && hasValue)
            a.scene = argv[++i];
        else if (k == "--cell-detail" && hasValue)
            a.cellDetail = static_cast<float>(std::atof(argv[++i]));
        else if (k == "--biome" && hasValue)
            a.biome = std::atoi(argv[++i]);
        else if (k == "--seed" && hasValue)
            a.seed = std::strtoull(argv[++i], nullptr, 10);
        else if (k == "--wildlife" && hasValue)
            a.wildlife = static_cast<float>(std::atof(argv[++i]));
        else if (k == "--steps" && hasValue)
            a.steps = std::atoi(argv[++i]);
        else if (k == "--warmup" && hasValue)
            a.warmup = std::atoi(argv[++i]);
        else if (k == "--threads" && hasValue)
            a.threads = std::atoi(argv[++i]);
        else {
            std::fprintf(stderr, "uso: bio_bench [--biome N] [--seed S] [--wildlife W] [--steps N] "
                                 "[--warmup N] [--threads N] [--no-focus] [--gpu] [--render] [--zoom Z] [--cell-detail S] [--scene archivo.plate] [--soak N]\n");
            return false;
        }
    }
    return a.biome >= 0 && a.biome < kBiomeCount && a.steps > 0 && a.warmup >= 0;
}

// FNV-1a sobre los bits de las posiciones: cualquier diferencia en la trayectoria lo cambia.
struct Hash {
    std::uint64_t h = 1469598103934665603ull;
    void add(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        for (int k = 0; k < 4; ++k) {
            h ^= (bits >> (8 * k)) & 0xffu;
            h *= 1099511628211ull;
        }
    }
};

std::uint64_t checksum(const World& world) {
    Hash hash;
    const BodyParticles& bp = world.bodies();
    for (std::size_t s = 0; s < bp.capacity(); ++s)
        if (bp.alive[s]) {
            hash.add(bp.x[s]);
            hash.add(bp.y[s]);
        }
    const Bacteria& bac = world.bacteria();
    for (std::size_t i = 0; i < bac.size(); ++i) {
        hash.add(bac.x[i]);
        hash.add(bac.y[i]);
    }
    return hash.h;
}

} // namespace

int main(int argc, char** argv) {
    Args a;
    if (!parse(argc, argv, a))
        return 1;
    if (a.threads > 0)
        omp_set_num_threads(a.threads);

    // La GPU necesita un contexto de OpenGL 4.3: una ventana que no se muestra.
    GpuSubstepSolver gpu;
    const bool window = a.gpu || a.render;
    if (window) {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "bio_bench");
    }
    if (a.gpu) {
        if (!gpu.init()) {
            std::fprintf(stderr, "GPU no disponible: %s\n", gpu.error().c_str());
            CloseWindow();
            return 1;
        }
    }

    PlateSpec plate;
    std::vector<Intervention> events;
    if (!a.scene.empty() && !loadScene(a.scene, plate, events)) {
        std::fprintf(stderr, "no se pudo leer la escena %s\n", a.scene.c_str());
        return 1;
    }
    World world(a.scene.empty() ? makeDrop(static_cast<BiomeId>(a.biome), a.seed) : plate.config);
    if (a.gpu)
        world.setSubstepSolver(&gpu);
    RunConfig rc;
    if (a.scene.empty()) {
        rc.spawnFounder = false;
        rc.wildlife = a.wildlife;
        rc.biome = a.biome;
        rc.seed = a.seed;
    } else {
        rc = plateRunConfig(plate);
        a.biome = plate.biome;
        a.seed = plate.seed;
        a.wildlife = rc.wildlife;
        for (const Intervention& iv : events)
            if (iv.kind == InterventionKind::Engine && (iv.index == 1) != a.gpu)
                std::fprintf(stderr, "aviso: la escena se grabó con el motor %s; esta corrida usa %s (no será idéntica)\n",
                             iv.index == 1 ? "GPU" : "CPU", a.gpu ? "GPU" : "CPU");
    }
    Run run;
    run.start(world, rc, 1);
    run.interventions().schedule(events);
    // Foco como el de la app con una ventana de 1600 × 900 px a zoom 2 (el LOD clasifica el resto).
    // Una escena trae su propio foco.
    if (a.focus && a.scene.empty())
        world.setFocus({world.width() * 0.5f, world.height() * 0.5f}, {400.0f, 225.0f});

    for (int i = 0; i < a.warmup; ++i) {
        run.interventions().beforeStep(world, kDt);
        world.step(kDt);
        run.update(world, kDt);
    }
#ifdef BIO_PROFILE_LITE
    ProfileLite::table().clear(); // solo los pasos medidos
#endif
    // Dibujo: la placa entera centrada (o el zoom pedido) en una textura del tamaño de la ventana
    // típica de la app. Se mide aparte del paso.
    std::unique_ptr<Renderer> renderer;
    RenderTexture2D target{};
    Camera2D camera{};
    if (a.render) {
        renderer = std::make_unique<Renderer>();
        renderer->setCellDetailScale(a.cellDetail);
        target = LoadRenderTexture(1600, 900);
        camera.target = {world.width() * 0.5f, world.height() * 0.5f};
        camera.offset = {800.0f, 450.0f};
        camera.zoom = a.zoom > 0.0f ? a.zoom : 900.0f / world.height();
    }
    using Clock = std::chrono::steady_clock;
    double ms = 0.0, renderMs = 0.0;
    // Prueba larga: el balance se mide desde el comienzo de los pasos medidos.
    const MassBudget soakStart = world.massBudget();
    const double soakMemStart = residentMB();
    double soakMemMax = soakMemStart;
    std::string soakFailure;
    for (int i = 0; i < a.steps; ++i) {
        if (a.soak > 0 && i > 0 && i % a.soak == 0) {
            soakMemMax = std::max(soakMemMax, residentMB());
            soakFailure = soakCheck(world, soakStart);
            if (!soakFailure.empty()) {
                std::fprintf(stderr, "FALLA en el paso %llu (t = %.1f s): %s\n",
                             static_cast<unsigned long long>(world.stepIndex()), static_cast<double>(world.simTime()),
                             soakFailure.c_str());
                break;
            }
        }
        const auto t0 = Clock::now();
        run.interventions().beforeStep(world, kDt);
        world.step(kDt);
        run.update(world, kDt);
        ms += std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        if (renderer) {
            const auto r0 = Clock::now();
            renderer->drawWorldTo(world, camera, Overlay::None, target);
            rlDrawRenderBatchActive();
            glad_glFinish();
            renderMs += std::chrono::duration<double, std::milli>(Clock::now() - r0).count();
        }
    }

#ifdef BIO_PROFILE_LITE
    profileLiteReport(a.steps);
#endif
    std::printf("bioma %d  seed %llu  fauna x%.2f  hilos %d  XPBD %s\n", a.biome, static_cast<unsigned long long>(a.seed),
                static_cast<double>(a.wildlife), omp_get_max_threads(), a.gpu ? "GPU" : "CPU");
    std::printf("células %zu (en detalle %zu)  bacterias %zu\n", world.registry().view<RingBody>().size(),
                world.nearBodies(), world.bacteria().size());
    std::printf("paso %.2f ms (%.1f pasos/s)  checksum %016llx\n", ms / a.steps, 1000.0 * a.steps / ms,
                static_cast<unsigned long long>(checksum(world)));
    if (renderer)
        std::printf("dibujo %.2f ms por cuadro (1600 × 900, zoom %.2f)\n", renderMs / a.steps,
                    static_cast<double>(camera.zoom));
    // Uniones de filamentos y colonias: cuánto se estiran respecto de su largo de reposo (sirve
    // para comparar resolvedores: CPU, GPU).
    const BodyParticles& bp = world.bodies();
    double stretch = 0.0, worst = 0.0;
    for (const World::AdhesionLink& l : world.links()) {
        const double d = std::hypot(bp.x[l.b] - bp.x[l.a], bp.y[l.b] - bp.y[l.a]);
        const double e = std::abs(d - l.rest) / std::max(1e-3, static_cast<double>(l.rest));
        stretch += e;
        worst = std::max(worst, e);
    }
    if (!world.links().empty())
        std::printf("uniones %zu  estiramiento medio %.3f  máximo %.3f\n", world.links().size(),
                    stretch / static_cast<double>(world.links().size()), worst);
    if (a.soak > 0) {
        if (soakFailure.empty())
            soakFailure = soakCheck(world, soakStart);
        std::printf("prueba larga: %s  memoria %.0f → %.0f MB (máx %.0f)\n",
                    soakFailure.empty() ? "OK" : soakFailure.c_str(), soakMemStart, residentMB(),
                    std::max(soakMemMax, residentMB()));
    }
    if (a.gpu && std::getenv("BIO_GPU_TIMING")) {
        SetTraceLogLevel(LOG_INFO);
        gpu.printTiming(a.warmup + a.steps);
    }
    if (renderer) {
        renderer.reset();
        UnloadRenderTexture(target);
    }
    if (window)
        CloseWindow();
    return a.soak > 0 && !soakFailure.empty() ? 2 : 0;
}
