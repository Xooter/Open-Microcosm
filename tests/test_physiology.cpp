// Fisiología por especie con fuentes (MVP: validación): el archivo de fábrica carga sin avisos,
// cada valor lleva su fuente y las conversiones de unidad y temperatura (Q10 = 2) son correctas.
#include "bio/physiology.hpp"
#include "core/paths.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

TEST_CASE("fisiología: el archivo de fábrica carga y cada valor tiene fuente") {
    std::string error;
    REQUIRE_MESSAGE(loadSpeciesPhysiology(assetPath("species/fisiologia.toml"), &error), error);
    for (const std::string& w : physiologyWarnings())
        FAIL_CHECK(w);
    int present = 0;
    for (int i = 0; i < kSpeciesCount; ++i) {
        const SourcedValue& mu = speciesPhysiology(static_cast<SpeciesId>(i)).growthMax;
        if (!mu.present)
            continue;
        ++present;
        CHECK_MESSAGE(!mu.source.empty(), speciesInfo(static_cast<SpeciesId>(i)).name);
        CHECK(mu.value > 0.0f);
        CHECK(mu.value < 1.0f); // ninguna especie duplica su biomasa en menos de ~40 min
    }
    CHECK(present >= 3);
    // Amoeba: 44 h de generación a 20 °C → ln 2 / 44 por hora, sin corrección de temperatura.
    const SourcedValue& amoeba = speciesPhysiology(SpeciesId::Amoeba).growthMax;
    REQUIRE(amoeba.present);
    CHECK(amoeba.value == doctest::Approx(std::log(2.0f) / 44.0f));
}

TEST_CASE("fisiología: unidades, Q10 y avisos") {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "bio_fisiologia_prueba.toml";
    {
        std::ofstream out(path);
        out << "[Paramecium.crecimiento_maximo]\nvalor = 2.4\nunidad = \"1/dia\"\ntemperatura = 30.0\nfuente = \"prueba\"\n"
               "[Euglena.crecimiento_maximo]\nvalor = 1.0\nunidad = \"furlongs\"\nfuente = \"prueba\"\n"
               "[NoExiste.crecimiento_maximo]\nvalor = 1.0\nunidad = \"1/h\"\nfuente = \"prueba\"\n";
    }
    REQUIRE(loadSpeciesPhysiology(path.string()));
    // 2,4/día a 30 °C → 0,1/h a 30 °C → la mitad a 20 °C.
    CHECK(speciesPhysiology(SpeciesId::Paramecium).growthMax.value == doctest::Approx(0.05f));
    CHECK_FALSE(speciesPhysiology(SpeciesId::Euglena).growthMax.present);
    CHECK(physiologyWarnings().size() == 2); // unidad desconocida y especie inexistente
    std::filesystem::remove(path);
    // Vuelve la tabla de fábrica para el resto de los tests.
    REQUIRE(loadSpeciesPhysiology(assetPath("species/fisiologia.toml")));
}
