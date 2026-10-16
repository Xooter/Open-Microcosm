// bio_lote: corre un experimento por lote sin ventana (fase P1): réplicas con seeds distintas y
// factores barridos, en paralelo, y guarda el formato largo, el resumen (media, DE, IC 95 %) y el
// JSON con la procedencia.
//
//   bio_lote experimento.toml [--salida carpeta] [--hilos N] [--replicas N] [--paquete ruta]
#include "game/content.hpp"
#include "game/experiment.hpp"
#include "game/export.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string file, out = ".";
    int threads = 0, replicas = 0;
    std::vector<std::string> packs;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        const bool hasValue = i + 1 < argc;
        if (k == "--salida" && hasValue)
            out = argv[++i];
        else if (k == "--hilos" && hasValue)
            threads = std::atoi(argv[++i]);
        else if (k == "--replicas" && hasValue)
            replicas = std::atoi(argv[++i]);
        else if (k == "--paquete" && hasValue)
            packs.push_back(argv[++i]);
        else if (file.empty() && k.rfind("--", 0) != 0)
            file = k;
        else {
            std::fprintf(stderr, "uso: bio_lote experimento.toml [--salida carpeta] [--hilos N] [--replicas N] [--paquete ruta]\n");
            return 1;
        }
    }
    if (file.empty()) {
        std::fprintf(stderr, "uso: bio_lote experimento.toml [--salida carpeta] [--hilos N] [--replicas N]\n");
        return 1;
    }
    for (const std::string& pack : packs) {
        std::vector<std::string> messages;
        const bool ok = loadPack(pack, messages);
        for (const std::string& m : messages)
            std::fprintf(stderr, "%s\n", m.c_str());
        if (!ok)
            return 1;
    }
    std::ifstream in(file);
    if (!in) {
        std::fprintf(stderr, "no se pudo leer %s\n", file.c_str());
        return 1;
    }
    std::stringstream text;
    text << in.rdbuf();
    ExperimentSpec spec;
    std::string error;
    if (!parseExperiment(text.str(), spec, &error)) {
        std::fprintf(stderr, "%s: %s\n", file.c_str(), error.c_str());
        return 1;
    }
    if (threads > 0)
        spec.threads = threads;
    if (replicas > 0)
        spec.replicas = replicas;
    const int levels = static_cast<int>(experimentLevels(spec).size());
    std::printf("%s: %d niveles x %d réplicas, %.1f h\n", spec.name.c_str(), levels, spec.replicas, spec.hours);
    const ExperimentResult r = runExperiment(spec, [](int done, int total) {
        std::fprintf(stderr, "\r%d/%d corridas", done, total);
        if (done == total)
            std::fprintf(stderr, "\n");
    });
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(out, ec);
    const std::string base = (fs::path(out) / fs::path(file).stem()).string();
    const bool ok = writeTextFile(base + "_largo.csv", experimentLongCsv(r)) &&
                    writeTextFile(base + "_resumen.csv", experimentSummaryCsv(r)) &&
                    writeTextFile(base + ".json", experimentJson(r));
    std::printf("%s (%.1f s)\n", ok ? (base + "_{largo,resumen}.csv y .json").c_str() : "no se pudo escribir", r.seconds);
    return ok ? 0 : 1;
}
