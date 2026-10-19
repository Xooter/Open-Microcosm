// Fisiología por especie con fuentes (MVP: validación): el archivo de fábrica carga sin avisos,
// cada valor lleva su fuente y las conversiones de unidad y temperatura (Q10 = 2) son correctas.
#include "bio/physiology.hpp"
#include "core/paths.hpp"
#include "game/export.hpp"
#include "game/inspect.hpp"
#include "game/world.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

TEST_CASE("fisiología: el archivo de fábrica carga y cada valor tiene fuente") {
    std::string error;
    REQUIRE_MESSAGE(loadBasePhysiology(&error), error);
    for (const std::string& w : physiologyWarnings())
        FAIL_CHECK(w);
    int present = 0;
    for (int i = 0; i < speciesCount(); ++i) {
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
        out << "[paramecium.max_growth]\nvalue = 2.4\nunit = \"1/day\"\ntemperature = 30.0\nsource = \"prueba\"\n"
               "[euglena.max_growth]\nvalue = 1.0\nunit = \"furlongs\"\nsource = \"prueba\"\n"
               "[NoExiste.max_growth]\nvalue = 1.0\nunit = \"1/h\"\nsource = \"prueba\"\n";
    }
    REQUIRE(loadSpeciesPhysiology(path.string()));
    // 2,4/día a 30 °C → 0,1/h a 30 °C → la mitad a 20 °C.
    CHECK(speciesPhysiology(SpeciesId::Paramecium).growthMax.value == doctest::Approx(0.05f));
    CHECK_FALSE(speciesPhysiology(SpeciesId::Euglena).growthMax.present);
    CHECK(physiologyWarnings().size() == 2); // unidad desconocida y especie inexistente
    std::filesystem::remove(path);
    // Vuelve la tabla de fábrica para el resto de los tests.
    REQUIRE(loadBasePhysiology());
}

TEST_CASE("fisiología: la ficha y la exportación de especies muestran la fuente") {
    REQUIRE(loadBasePhysiology());
    // Ficha de un paramecio: la sección con µmax, lo publicado y la cita.
    WorldConfig config;
    config.bacteria = 0;
    config.detritus = 0;
    config.seed = 5;
    World world(config);
    const entt::entity e =
        world.spawnOrganism(SpeciesId::Paramecium, {world.width() * 0.5f, world.height() * 0.5f}, speciesGenome(SpeciesId::Paramecium), 0);
    Picked picked;
    picked.kind = Picked::Kind::Cell;
    picked.entity = e;
    const Inspection ins = buildInspection(world, picked);
    const InspectSection* phys = nullptr;
    for (const InspectSection& s : ins.sections)
        if (s.title == "Fisiologia y fuentes")
            phys = &s;
    REQUIRE(phys != nullptr);
    std::string text;
    for (const InspectLine& l : phys->lines)
        text += l.label + " " + l.value + "\n";
    CHECK(text.find("Crecimiento maximo") != std::string::npos);
    CHECK(text.find("Gause") != std::string::npos);
    CHECK(text.find("ESTIMACION") != std::string::npos); // la temperatura de Gause no está verificada

    // Exportación: cada especie con datos lleva valor, lo publicado y la fuente.
    const std::string json = speciesJson();
    CHECK(json.find("\"fisiologia\"") != std::string::npos);
    for (int i = 0; i < speciesCount(); ++i) {
        const SourcedValue& mu = speciesPhysiology(static_cast<SpeciesId>(i)).growthMax;
        if (mu.present)
            CHECK_MESSAGE(json.find(mu.source.substr(0, 40)) != std::string::npos, speciesInfo(static_cast<SpeciesId>(i)).name);
    }
}
