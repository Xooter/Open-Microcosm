// Experimentos por lote (fase P1, paso 6): réplicas, factores, resumen y formato.
#include "game/experiment.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <string>

namespace {

const char* kGause = R"(
[experiment]
name = "prueba"
kind = "culture"
replicates = 4
seed = 3
hours = 48
sample_h = 12

[culture]
vessel = "gause_tube"
medium = "osterhout_gause"
temperature = 26.0
program = ["medium event 24 inf 0 0 24 0.5 1", "bacteria event 24 inf 0 0 24 0.5 5e7"]
events = ["0 bacteria 5e7", "0 inoculate 10 paramecium"]

[[factor]]
key = "temperature"
values = [20.0, 26.0]
)";

// El mismo experimento guardado antes de que los archivos pasaran al inglés.
const char* kGauseLegacy = R"(
[experimento]
nombre = "prueba"
tipo = "cultivo"
replicas = 4
seed = 3
horas = 48
muestreo_h = 12

[cultivo]
recipiente = "tubo_gause"
medio = "osterhout_gause"
temperatura = 26.0
programa = ["medio evento 24 inf 0 0 24 0.5 1", "bacterias evento 24 inf 0 0 24 0.5 5e7"]
eventos = ["0 bacterias 5e7", "0 inocular 10 paramecium"]

[[factor]]
clave = "temperatura"
valores = [20.0, 26.0]
)";

} // namespace

TEST_CASE("experimento: uno guardado con las claves viejas se lee igual") {
    ExperimentSpec a, b;
    std::string error;
    REQUIRE_MESSAGE(parseExperiment(kGause, a, &error), error);
    REQUIRE_MESSAGE(parseExperiment(kGauseLegacy, b, &error), error);
    CHECK(serializeExperiment(a) == serializeExperiment(b));
    CHECK(b.culture.vessel == "gause_tube"); // el id viejo se lee como el actual
    REQUIRE(b.factors.size() == 1);
    CHECK(b.factors[0].key == "temperature");
}

TEST_CASE("experimento: se lee, se guarda y vuelve a leerse igual") {
    ExperimentSpec a;
    std::string error;
    REQUIRE_MESSAGE(parseExperiment(kGause, a, &error), error);
    CHECK(a.replicas == 4);
    CHECK(a.culture.program.entries.size() == 2);
    CHECK(a.cultureEvents.size() == 2);
    REQUIRE(a.factors.size() == 1);
    CHECK(experimentLevels(a).size() == 2);
    ExperimentSpec b;
    REQUIRE(parseExperiment(serializeExperiment(a), b, &error));
    CHECK(serializeExperiment(b) == serializeExperiment(a));
}

TEST_CASE("experimento: las réplicas difieren, el lote se repite y no depende de los hilos") {
    ExperimentSpec spec;
    REQUIRE(parseExperiment(kGause, spec));
    spec.threads = 1;
    const ExperimentResult one = runExperiment(spec);
    spec.threads = 4;
    const ExperimentResult four = runExperiment(spec);
    REQUIRE(one.replicas.size() == 8);
    REQUIRE(four.replicas.size() == 8);
    for (std::size_t k = 0; k < one.replicas.size(); ++k) {
        CHECK(one.replicas[k].seed == four.replicas[k].seed);
        CHECK(one.replicas[k].values == four.replicas[k].values);
    }
    // Distintas seeds, distintas trayectorias.
    const int col = one.column("paramecium");
    REQUIRE(col >= 0);
    CHECK(one.replicas[0].values != one.replicas[1].values);
    // Resumen: media y dispersión con su IC (t de Student, 3 grados de libertad).
    const SummaryPoint p = summarize(one, 1, col, one.times.size() - 1);
    CHECK(p.n == 4);
    CHECK(p.mean > 0.0);
    CHECK(p.ci95 == doctest::Approx(3.182 * p.sd / 2.0));
    CHECK(experimentSummaryCsv(one).find("paramecium") != std::string::npos);
    CHECK(experimentJson(one).find("\"ic95\"") != std::string::npos);
}

TEST_CASE("experimento: un lote de placas corre la escena con seeds distintas") {
    ExperimentSpec spec;
    spec.kind = ExperimentSpec::Kind::Plate;
    spec.replicas = 2;
    spec.hours = 0.5; // horas biológicas (30 s de placa con la escala de 60)
    spec.sampleHours = 0.25;
    spec.plateScene = "plate_version=3\nbiome=charco\nseed=4\ncontent=2\nbacteria=2000\n";
    spec.threads = 2;
    const ExperimentResult r = runExperiment(spec);
    REQUIRE(r.replicas.size() == 2);
    const int col = r.column("bacteria");
    REQUIRE(col >= 0);
    CHECK(r.replicas[0].values[static_cast<std::size_t>(col)].size() == r.times.size());
    CHECK(r.replicas[0].values[static_cast<std::size_t>(col)].front() > 1000.0);
}
