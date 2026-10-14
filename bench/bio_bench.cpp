// bio_bench: mide el costo de World::step sin ventana, con una gota fija (fase 19).
// Imprime ms/paso, el censo y un checksum del estado: una optimización "exacta" no debe
// cambiarlo (verificarlo con -DBIO_STRICT_FP=ON: con -ffast-math cualquier cambio de código puede
// reasociar operaciones y alterar los últimos bits). Con -DBIO_PROFILE_LITE=ON (preset `lite`)
// desglosa el tiempo por PROFILE_SCOPE. Con --gpu abre una ventana oculta y resuelve XPBD en la GPU,
// como la app.
//
//   bio_bench [--biome N] [--seed S] [--wildlife W] [--steps N] [--warmup N] [--threads N]
//             [--no-focus] [--gpu]
#include "core/profiling.hpp"
#include "game/biome.hpp"
#include "game/run.hpp"
#include "gpu/gpu_substep_solver.hpp"

#include <raylib.h>

#include <omp.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
};

bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        const bool hasValue = i + 1 < argc;
        if (k == "--no-focus")
            a.focus = false;
        else if (k == "--gpu")
            a.gpu = true;
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
                                 "[--warmup N] [--threads N] [--no-focus] [--gpu]\n");
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
    if (a.gpu) {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "bio_bench");
        if (!gpu.init()) {
            std::fprintf(stderr, "GPU no disponible: %s\n", gpu.error().c_str());
            CloseWindow();
            return 1;
        }
    }

    World world(makeDrop(static_cast<BiomeId>(a.biome), a.seed));
    if (a.gpu)
        world.setSubstepSolver(&gpu);
    RunConfig rc;
    rc.spawnFounder = false;
    rc.wildlife = a.wildlife;
    rc.biome = a.biome;
    rc.seed = a.seed;
    Run run;
    run.start(world, rc, 1);
    // Foco como el de la app con una ventana de 1600 × 900 px a zoom 2 (el LOD clasifica el resto).
    if (a.focus)
        world.setFocus({world.width() * 0.5f, world.height() * 0.5f}, {400.0f, 225.0f});

    for (int i = 0; i < a.warmup; ++i) {
        world.step(kDt);
        run.update(world, kDt);
    }
#ifdef BIO_PROFILE_LITE
    ProfileLite::table().clear(); // solo los pasos medidos
#endif
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < a.steps; ++i) {
        world.step(kDt);
        run.update(world, kDt);
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

#ifdef BIO_PROFILE_LITE
    profileLiteReport(a.steps);
#endif
    std::printf("bioma %d  seed %llu  fauna x%.2f  hilos %d  XPBD %s\n", a.biome, static_cast<unsigned long long>(a.seed),
                static_cast<double>(a.wildlife), omp_get_max_threads(), a.gpu ? "GPU" : "CPU");
    std::printf("células %zu (en detalle %zu)  bacterias %zu\n", world.registry().view<RingBody>().size(),
                world.nearBodies(), world.bacteria().size());
    std::printf("paso %.2f ms (%.1f pasos/s)  checksum %016llx\n", ms / a.steps, 1000.0 * a.steps / ms,
                static_cast<unsigned long long>(checksum(world)));
    if (a.gpu) {
        if (std::getenv("BIO_GPU_TIMING")) {
            SetTraceLogLevel(LOG_INFO);
            gpu.printTiming(a.warmup + a.steps);
        }
        CloseWindow();
    }
    return 0;
}
