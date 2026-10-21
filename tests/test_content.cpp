// Contenido como datos (fase 20.0): los seres se cargan de paquetes TOML. Suite "paquetes": corre
// en su propio proceso (cargar seres de prueba cambia el registro global que usan los demás tests).
#include "bio/body_plan.hpp"
#include "bio/species.hpp"
#include "core/paths.hpp"
#include "bio/physiology.hpp"
#include "game/biome.hpp"
#include "game/content.hpp"
#include "game/glossary.hpp"
#include "game/lesson.hpp"
#include "game/prepared.hpp"
#include "game/reagent.hpp"
#include "game/world.hpp"
#include "game/mineral.hpp"
#include "game/plate_spec.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

bool warned(const std::string& fragment) {
    for (const std::string& w : speciesLoadWarnings())
        if (w.find(fragment) != std::string::npos)
            return true;
    return false;
}

} // namespace

TEST_CASE("contenido: el paquete de fábrica carga sus 92 seres en el orden de siempre, sin avisos" *
          doctest::test_suite("paquetes")) {
    // 24 de la gota, 3 de cultivo (20b), 5 de los preparados (20c) 8 del lote 1 de candidatos, 7 del lote 2, 18 del lote 3, 7 del lote 4 Staurastrum y los de las mecánicas (docs/mechanics/README.md), sin Spirostomum (quitado), y el espermatozoide humano.
    CHECK(speciesCount() == 92);
    CHECK(speciesLoadWarnings().empty());
    // Ningún registro avisa nada con el paquete de fábrica (una clave mal escrita se avisa).
    for (const auto* list : {&bodyPlanWarnings(), &physiologyWarnings(), &mineralLoadWarnings(), &biomeLoadWarnings(),
                             &preparedLoadWarnings(), &lessonLoadWarnings(), &reagentLoadWarnings(),
                             &glossaryLoadWarnings()})
        for (const std::string& w : *list)
            FAIL_CHECK(w);
    CHECK(std::string(speciesInfo(SpeciesId::Paramecium).key) == "paramecium");
    CHECK(std::string(speciesInfo(SpeciesId::Cyclotella).key) == "cyclotella");
    CHECK(findSpecies("amoeba") == SpeciesId::Amoeba);
    CHECK(speciesPack(SpeciesId::Amoeba) == "base");
    // Dietas resueltas desde ids: Didinium caza a los dos Paramecium y a Colpidium, un ser que está
    // más abajo en el archivo.
    const SpeciesSet& prey = speciesInfo(SpeciesId::Didinium).prey;
    CHECK(prey.test(static_cast<std::size_t>(SpeciesId::Paramecium)));
    CHECK(prey.test(static_cast<std::size_t>(SpeciesId::ParameciumAurelia)));
    REQUIRE(findSpecies("colpidium"));
    CHECK(prey.test(static_cast<std::size_t>(*findSpecies("colpidium"))));
    CHECK(prey.count() == 3);
    // Etiquetas: los hospedadores de los quitridios (antes, una máscara fija en el código).
    CHECK(speciesWithTag("chytrid_host").count() == 16);
    CHECK(hasTag(SpeciesId::Pediastrum, "photobiont"));
    CHECK(speciesGenome(SpeciesId::Arcella)[Gene::Armor] == doctest::Approx(0.9f));
}

TEST_CASE("contenido: un paquete agrega un ser sin compilar; las etiquetas resuelven dietas" *
          doctest::test_suite("paquetes")) {
    const int before = speciesCount();
    const std::string pack = R"(
[[being]]
id = "colpidium_colpoda"
name = "Colpidium colpoda"
tags = ["protist", "ciliate", "bacterivore"]
locomotion = "cilia"
feeding = "oral_groove"
flees_from = ["didinium"]
ph = [6.0, 9.0]
temperature = [5.0, 30.0]
storage = "glycogen"
[being.shape]
semi_major_axis = 45.0
semi_minor_axis = 20.0
shape_memory = 8.0
color = [170, 176, 150]

[[being]]
id = "depredador_de_ciliados"
name = "Depredador de ciliados"
locomotion = "pseudopod"
feeding = "phagocytosis"
prey = ["ciliate"]
[being.shape]
semi_major_axis = 60.0
semi_minor_axis = 60.0
)";
    std::string error;
    REQUIRE(loadSpeciesText(pack, "prueba.biopack", "prueba", &error));
    CHECK(speciesCount() == before + 2);
    const auto colpidium = findSpecies("colpidium_colpoda");
    REQUIRE(colpidium);
    CHECK(speciesPack(*colpidium) == "prueba");
    CHECK(speciesInfo(*colpidium).flee.test(static_cast<std::size_t>(SpeciesId::Didinium)));
    CHECK(speciesInfo(*colpidium).initialCount == 0); // no aparece solo en las gotas
    // "ciliate" es una etiqueta: incluye a los de fábrica y al nuevo.
    const SpeciesSet& prey = speciesInfo(*findSpecies("depredador_de_ciliados")).prey;
    CHECK(prey.test(static_cast<std::size_t>(*colpidium)));
    CHECK(prey.test(static_cast<std::size_t>(SpeciesId::Paramecium)));
    CHECK_FALSE(prey.test(static_cast<std::size_t>(SpeciesId::Euglena)));
}

TEST_CASE("contenido: los errores de un paquete se avisan y no tumban el programa" * doctest::test_suite("paquetes")) {
    const int before = speciesCount();
    const std::string pack = R"(
[[being]]
id = "sin_forma"
name = "Sin forma"
locomotion = "cilia"

[[being]]
id = "locomocion_rara"
name = "Locomoción rara"
locomotion = "patas"
[being.shape]
semi_major_axis = 10.0
semi_minor_axis = 10.0

[[being]]
id = "paramecium"
name = "Otro Paramecium"
locomotion = "cilia"
[being.shape]
semi_major_axis = 10.0
semi_minor_axis = 10.0

[[being]]
id = "con_tipeo"
name = "Con tipeo"
locomotion = "cilia"
prey = ["no_existe"]
velocidad_relativo = 2.0
[being.shape]
semi_major_axis = 10.0
semi_minor_axis = 10.0
)";
    REQUIRE(loadSpeciesText(pack, "errores.biopack", "errores"));
    CHECK(speciesCount() == before + 1); // solo "con_tipeo" (los avisos no impiden cargarlo)
    CHECK(warned("falta [being.shape]"));
    CHECK(warned("'locomotion' tiene que ser uno de"));
    CHECK(warned("el ser 'paramecium' ya existe"));
    CHECK(warned("clave desconocida 'velocidad_relativo'"));
    CHECK(warned("'no_existe' en prey"));
    CHECK(std::string(speciesInfo(SpeciesId::Paramecium).name) == "Paramecium");
    std::string error;
    CHECK_FALSE(loadSpeciesText("[[being]\nid = ", "roto.biopack", "roto", &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("contenido: los planes corporales son datos; un paquete puede traer el suyo" * doctest::test_suite("paquetes")) {
    CHECK(bodyPlanWarnings().empty());
    CHECK(std::string(speciesPlan(SpeciesId::Paramecium).name) == "ciliado");
    CHECK(speciesPlan(SpeciesId::Paramecium).has(StructureKind::ContractileVacuole));
    CHECK(std::string(speciesPlan(SpeciesId::Trichonympha).name) == "parabasalido");
    CHECK(speciesPlan(SpeciesId::Trichonympha).has(StructureKind::Hydrogenosome));
    CHECK_FALSE(speciesPlan(SpeciesId::Trichonympha).has(StructureKind::Mitochondrion));
    REQUIRE(loadBodyPlansText(R"(
[[body_plan]]
id = "celula_con_nucleolos"
name = "celula con nucleolos"
structures = [
    { kind = "plasma_membrane", source = "simulated", arrangement = "envelope", brightfield = true, name = "membrana" },
    { kind = "nucleus", source = "simulated", arrangement = "interior", count = 1, brightfield = true, name = "nucleus" },
    { kind = "nucleolus", source = "declared", arrangement = "interior", length = 2.0, width = 2.0, count = 3, brightfield = true, name = "nucleolos" },
]
)", "planes.biopack", "prueba"));
    REQUIRE(loadSpeciesText(R"(
[[being]]
id = "con_plan_propio"
name = "Con plan propio"
locomotion = "pseudopod"
body_plan = "celula_con_nucleolos"
[being.shape]
semi_major_axis = 20.0
semi_minor_axis = 15.0
[[being]]
id = "plan_inexistente"
name = "Plan inexistente"
locomotion = "pseudopod"
body_plan = "no_existe"
[being.shape]
semi_major_axis = 20.0
semi_minor_axis = 15.0
)", "seres.biopack", "prueba"));
    const BodyPlan& own = speciesPlan(*findSpecies("con_plan_propio"));
    REQUIRE(own.find(StructureKind::Nucleolus));
    CHECK(own.find(StructureKind::Nucleolus)->count == 3);
    CHECK(std::string(speciesPlan(*findSpecies("plan_inexistente")).name) == "eucariota"); // el mínimo
}

namespace {

bool biomeWarned(const std::string& fragment) {
    for (const std::string& w : biomeLoadWarnings())
        if (w.find(fragment) != std::string::npos)
            return true;
    return false;
}

} // namespace

TEST_CASE("contenido: los biomas de fábrica son datos, en el orden de siempre" * doctest::test_suite("paquetes")) {
    CHECK(biomeCount() == 6);
    CHECK(biomeLoadWarnings().empty());
    CHECK(biome(BiomeId::Puddle).key == "puddle");
    CHECK(biome(BiomeId::SeaIce).key == "sea_ice");
    CHECK(findBiome("termite_gut") == BiomeId::TermiteGut);
    CHECK_FALSE(findBiome("marte").has_value());
    const Biome& gut = biome(BiomeId::TermiteGut);
    CHECK(gut.medium.anoxic);
    CHECK(gut.medium.temperature == doctest::Approx(28.0f));
    CHECK(gut.sediment[MineralKind::Clay] == 60);
    CHECK(gut.sediment.rockMax == doctest::Approx(120.0f)); // lo que falta queda por omisión
    // Trichonympha es simbionte obligado: solo está donde el bioma la nombra.
    CHECK(gut.weight(SpeciesId::Trichonympha) == doctest::Approx(1.0f));
    CHECK(biome(BiomeId::Puddle).weight(SpeciesId::Trichonympha) == 0.0f);
    // Las gotas más livianas (98c30d5): el charco con menos paramecios y el musgo con menos tecamebas.
    CHECK(biome(BiomeId::Puddle).weight(SpeciesId::Paramecium) == doctest::Approx(0.15f));
    CHECK(biome(BiomeId::Moss).weight(SpeciesId::Arcella) == doctest::Approx(1.5f));
    CHECK(biome(BiomeId::Moss).weight(SpeciesId::Stentor) == 0.0f);
    CHECK(WorldConfig{}.speciesWeight[static_cast<int>(SpeciesId::Trichonympha)] == 0.0f);
}

TEST_CASE("contenido: un paquete agrega un bioma; sus pesos nombran seres de cualquier paquete" *
          doctest::test_suite("paquetes")) {
    // El bioma nombra un ser que todavía no está cargado: los pesos se buscan por id al armar la gota.
    const std::string biomes = R"(
[[biome]]
id = "charca_de_prueba"
name = "Charca de prueba"
bacteria = 10000
base_weight = 0.0
weights = { paramecium = 2.0, halteria_grandinella = 1.5 }
[biome.medium]
temperature = 15.0
phosphate = 3.0
[biome.sediment]
clay = 20
grain_radius = [30.0, 60.0]
)";
    std::string error;
    REQUIRE(loadBiomesText(biomes, "prueba.toml", "prueba", &error));
    const auto id = findBiome("charca_de_prueba");
    REQUIRE(id.has_value());
    const Biome& b = biome(*id);
    CHECK(b.pack == "prueba");
    CHECK(b.weight(SpeciesId::Paramecium) == doctest::Approx(2.0f));
    CHECK(b.weight(SpeciesId::Amoeba) == 0.0f);
    REQUIRE(loadSpeciesText(R"(
[[being]]
id = "halteria_grandinella"
name = "Halteria grandinella"
locomotion = "cilia"
[being.shape]
semi_major_axis = 15.0
semi_minor_axis = 13.0
)",
                            "halteria.toml", "prueba", &error));
    const auto halteria = findSpecies("halteria_grandinella");
    REQUIRE(halteria.has_value());
    CHECK(b.weight(*halteria) == doctest::Approx(1.5f));
    // La gota usa el medio del bioma (con su variación por seed) y su sedimento.
    const WorldConfig c = makeDrop(*id, 4);
    CHECK(c.medium.temperature == doctest::Approx(15.0f).epsilon(0.5));
    CHECK(c.medium.salinity == doctest::Approx(MediumParams{}.salinity)); // lo que no dice, el agua dulce
    CHECK(c.sediment.rockMin == doctest::Approx(30.0f));
}

TEST_CASE("contenido: un bioma con errores se avisa y no se carga" * doctest::test_suite("paquetes")) {
    const int before = biomeCount();
    const std::string bad = R"(
[[biome]]
id = "bioma_roto"
name = "Bioma roto"
humedad = 3
[biome.medium]
phosphate = -5.0
light = 2.0
presion = 1.0
[[biome]]
id = "puddle"
name = "Otro charco"
)";
    std::string error;
    CHECK(loadBiomesText(bad, "roto.toml", "roto", &error));
    CHECK(biomeCount() == before);
    CHECK_FALSE(findBiome("bioma_roto").has_value());
    CHECK(biomeWarned("clave desconocida 'humedad'"));
    CHECK(biomeWarned("clave desconocida 'presion'"));
    CHECK(biomeWarned("'phosphate' no puede ser negativo"));
    CHECK(biomeWarned("'light' es relativa"));
    CHECK(biomeWarned("el bioma 'puddle' ya existe"));
    CHECK(biome(BiomeId::Puddle).name == "Charco");
    // Un TOML mal formado: false con el motivo, sin tocar el registro.
    CHECK_FALSE(loadBiomesText("[[biome]\nid = ", "mal.toml", "mal", &error));
    CHECK(error.find("mal.toml") != std::string::npos);
}

namespace {

bool mineralWarned(const std::string& fragment) {
    for (const std::string& w : mineralLoadWarnings())
        if (w.find(fragment) != std::string::npos)
            return true;
    return false;
}

} // namespace

TEST_CASE("contenido: los minerales de fábrica son datos, en el orden de siempre" * doctest::test_suite("paquetes")) {
    CHECK(mineralCount() == 6);
    CHECK(mineralLoadWarnings().empty());
    CHECK(mineralInfo(MineralKind::Clay).key == "clay");
    CHECK(mineralInfo(MineralKind::Clay).flocculates);
    CHECK(findMineral("frustule") == MineralKind::Frustule);
    CHECK(mineralInfo(MineralKind::IronOxide).opacity == doctest::Approx(1.0f));
    CHECK(mineralInfo(MineralKind::Quartz).radiusMax == doctest::Approx(15.0f));
    CHECK(mineralInfo(MineralKind::Quartz).grainName == "Grano de arena (cuarzo)");
    CHECK(mineralInfo(MineralKind::Mica).grainName == "Grano de Mica"); // sin nombre_grano
}

TEST_CASE("contenido: un paquete suma un mineral y un bioma lo usa en su sedimento" * doctest::test_suite("paquetes")) {
    std::string error;
    REQUIRE(loadMineralsText(R"(
[[mineral]]
id = "calcita"
name = "Calcita"
description = "Carbonato de calcio: romboedros transparentes con mucho relieve."
radius = [2.0, 8.0]
density_difference = 1710.0
opacity = 0.1
style = "angular_grain"
)",
                             "calcita.toml", "rocas", &error));
    const auto calcite = findMineral("calcita");
    REQUIRE(calcite.has_value());
    CHECK(mineralInfo(*calcite).pack == "rocas");
    REQUIRE(loadBiomesText(R"(
[[biome]]
id = "laguna_calcarea"
name = "Laguna calcárea"
base_weight = 0.0
[biome.sediment]
calcita = 50
basalto = 3
)",
                           "laguna.toml", "rocas", &error));
    CHECK(biomeWarned("'basalto' no es un mineral conocido"));
    const auto lagoon = findBiome("laguna_calcarea");
    REQUIRE(lagoon.has_value());
    CHECK(biome(*lagoon).sediment[*calcite] == 50);
    // La placa la guarda con el id del mineral y la vuelve a leer igual.
    PlateSpec p;
    p.biome = static_cast<int>(*lagoon);
    p.seed = 3;
    p.content = DropContent::Full;
    p.rebuild();
    CHECK(p.config.sediment[*calcite] > 0);
    const std::string text = serializePlate(p);
    CHECK(text.find("sediment.calcita=") != std::string::npos);
    PlateSpec q;
    REQUIRE(parsePlate(text, q));
    CHECK(q.config.sediment[*calcite] == p.config.sediment[*calcite]);
}

TEST_CASE("contenido: un mineral con errores se avisa y no se carga" * doctest::test_suite("paquetes")) {
    const int before = mineralCount();
    std::string error;
    CHECK(loadMineralsText(R"(
[[mineral]]
id = "piedra_rara"
name = "Piedra rara"
style = "brillante"
opacity = 3.0
)",
                           "rara.toml", "raro", &error));
    CHECK(mineralCount() == before);
    CHECK(mineralWarned("falta 'radius'"));
    CHECK(mineralWarned("'style' tiene que ser uno de"));
    CHECK(mineralWarned("'opacity' va de 0 a 1"));
}

namespace {

// Un ser que solo existe en este .biopack (Colpoda cucullus: ciliado bacterívoro de las infusiones
// de heno, ~50–100 µm, en forma de riñón). La fisiología es un dato de prueba.
const char* const kColpodaPack = R"(
[pack]
id = "infusiones"
name = "Ciliados de infusiones"
version = "1"
author = "pruebas"
license = "GPL-3.0-or-later"

[[being]]
id = "colpoda_cucullus"
name = "Colpoda cucullus"
tags = ["protist", "ciliate", "bacterivore"]
description = "Ciliado en forma de rinon de las infusiones de heno: come bacterias y se enquista al secarse."
locomotion = "cilia"
body_plan = "ciliate"
feeding = "oral_groove"
encysts = true
temperature = [5.0, 30.0]
storage = "glycogen"
[being.shape]
semi_major_axis = 35.0
semi_minor_axis = 22.0
shape_memory = 8.0
color = [168, 170, 140]

[physiology.colpoda_cucullus.max_growth]
value = 4.0
unit = "h"
temperature = 20.0
source = "Dato de prueba (no publicado)."
estimate = true

[[biome]]
id = "infusion_de_heno"
name = "Infusion de heno"
base_weight = 0.0
weights = { colpoda_cucullus = 1.0, paramecium = 1.0 }
bacteria = 40000
[biome.medium]
organic_inflow = 200.0
)";

std::string writeTemp(const std::string& name, const std::string& text) {
    const auto dir = std::filesystem::temp_directory_path() / "openmicrocosm_tests_packs";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::ofstream(path, std::ios::binary) << text;
    return path.string();
}

} // namespace

TEST_CASE("contenido: un ser que solo está en un .biopack vive, se exporta y se vuelve a importar" *
          doctest::test_suite("paquetes")) {
    std::vector<std::string> messages;
    REQUIRE(loadPack(writeTemp("infusiones.biopack", kColpodaPack), messages));
    for (const std::string& m : messages)
        MESSAGE(m);
    const PackInfo* pack = findPack("infusiones");
    REQUIRE(pack != nullptr);
    CHECK(pack->version == "1");
    CHECK(pack->fingerprint != 0);
    const auto colpoda = findSpecies("colpoda_cucullus");
    REQUIRE(colpoda.has_value());
    CHECK(speciesPack(*colpoda) == "infusiones");
    CHECK(speciesPhysiology(*colpoda).growthMax.present);
    REQUIRE(findBiome("infusion_de_heno").has_value());

    // Vive: nada, come y mantiene su biomasa en una placa como cualquier ser de fábrica.
    WorldConfig config;
    config.gridX = config.gridY = 64; // 512 µm: alcanza para verlo nadar
    config.bacteria = 800;
    config.detritus = 0;
    config.seed = 21;
    World world(config);
    const Vec2 center{0.5f * world.width(), 0.5f * world.height()};
    world.spawnBeing({BeingKind::Species, *colpoda}, center);
    REQUIRE(world.census().species[static_cast<int>(*colpoda)] == 1);
    Vec2 start{}, end{};
    for (auto [e, sp, body] : world.registry().view<Species, RingBody>().each())
        if (sp.id == *colpoda)
            start = body.centroid;
    for (int s = 0; s < 600; ++s)
        world.step(1.0f / 60.0f);
    CHECK(world.census().species[static_cast<int>(*colpoda)] >= 1);
    for (auto [e, sp, body] : world.registry().view<Species, RingBody>().each())
        if (sp.id == *colpoda)
            end = body.centroid;
    MESSAGE("Colpoda nadó " << length(end - start) << " µm en 10 s");
    CHECK(length(end - start) > 20.0f);

    // Se exporta con su fisiología y vuelve a entrar como otro paquete (con otro id: los ids no se
    // repiten entre paquetes).
    BiopackSpec spec;
    spec.id = "infusiones_copia";
    spec.beings.push_back(*colpoda);
    spec.biomes.push_back(*findBiome("infusion_de_heno"));
    spec.scenes.emplace_back("una placa", "plate_version=2\nbiome=charco\nseed=4\n");
    messages.clear();
    std::string text = biopackText(spec, messages);
    REQUIRE_FALSE(text.empty());
    CHECK(text.find("[[being]]") != std::string::npos);
    CHECK(text.find("max_growth") != std::string::npos);
    CHECK(text.find("[[scene]]") != std::string::npos);
    CHECK(text.find("min_program") != std::string::npos);
    for (const auto& [from, to] : {std::pair<std::string, std::string>{"colpoda_cucullus", "colpoda_inflata"},
                                   {"infusion_de_heno", "infusion_de_lechuga"}})
        for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
            text.replace(at, from.size(), to);
    spec.scenes.clear();
    messages.clear();
    // Sin la escena (instalarla escribiría en la carpeta de placas del usuario).
    const std::size_t scene = text.find("[[scene]]");
    REQUIRE(loadPack(writeTemp("copia.biopack", text.substr(0, scene)), messages));
    const auto copy = findSpecies("colpoda_inflata");
    REQUIRE(copy.has_value());
    CHECK(speciesInfo(*copy).spec.radiusX == doctest::Approx(speciesInfo(*colpoda).spec.radiusX));
    CHECK(speciesPhysiology(*copy).growthMax.value == doctest::Approx(speciesPhysiology(*colpoda).growthMax.value));
    CHECK(biome(*findBiome("infusion_de_lechuga")).weight(*copy) == doctest::Approx(1.0f));
}

TEST_CASE("contenido: los paquetes inválidos se rechazan sin cerrar el programa" * doctest::test_suite("paquetes")) {
    std::vector<std::string> messages;
    // El mismo id que uno cargado.
    CHECK_FALSE(loadPack(writeTemp("base.biopack", "[pack]\nid = \"base\"\nversion = \"9\"\n"), messages));
    // Id con caracteres que no van (podría usarse para armar rutas).
    CHECK_FALSE(loadPack(writeTemp("ruta.biopack", "[pack]\nid = \"../x\"\nversion = \"1\"\n"), messages));
    // Para un programa más nuevo.
    CHECK_FALSE(loadPack(writeTemp("futuro.biopack",
                                   "[pack]\nid = \"futuro\"\nversion = \"1\"\nmin_program = \"999.0\"\n"),
                         messages));
    // Sin manifiesto, o TOML roto.
    CHECK_FALSE(loadPack(writeTemp("suelto.biopack", "[[being]]\nid = \"x\"\n"), messages));
    CHECK_FALSE(loadPack(writeTemp("roto.biopack", "[paquete\nid ="), messages));
    // Demasiado grande.
    CHECK_FALSE(loadPack(writeTemp("enorme.biopack", std::string(kMaxPackBytes + 1, '#')), messages));
    CHECK(findPack("futuro") == nullptr);
    bool tooBig = false;
    for (const std::string& m : messages)
        tooBig = tooBig || m.find("tamaño máximo") != std::string::npos;
    CHECK(tooBig);
    // Una clave de primer nivel desconocida se avisa (el resto se carga).
    messages.clear();
    CHECK(loadPack(writeTemp("raro.biopack", "[pack]\nid = \"raro\"\nversion = \"1\"\nlicense = \"MIT\"\n"
                                             "[[criatura]]\nid = \"x\"\n"),
                   messages));
    bool unknown = false;
    for (const std::string& m : messages)
        unknown = unknown || m.find("clave desconocida 'criatura'") != std::string::npos;
    CHECK(unknown);
}

TEST_CASE("contenido: un paquete en carpetas trae un archivo por definición" * doctest::test_suite("paquetes")) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "openmicrocosm_tests_packs" / "charca";
    fs::remove_all(dir);
    auto write = [&](const std::string& rel, const std::string& text) {
        fs::create_directories((dir / rel).parent_path());
        std::ofstream(dir / rel, std::ios::binary) << text;
    };
    write("pack.toml", "[pack]\nid = \"charca\"\nversion = \"1\"\nlicense = \"MIT\"\n"
                       "[pack.order]\nbeings = [\"cazador_de_charca\"]\n");
    // El cazador va primero y nombra como presa a un ser de un archivo que todavía no se leyó: la
    // referencia se resuelve con la carpeta entera, sin avisos.
    write("beings/cazador_de_charca.toml", R"([[being]]
id = "cazador_de_charca"
name = "Cazador de charca"
locomotion = "pseudopod"
feeding = "phagocytosis"
prey = ["presa_de_charca"]
[being.shape]
semi_major_axis = 60.0
semi_minor_axis = 60.0
)");
    write("beings/presa_de_charca.toml", R"([[being]]
id = "presa_de_charca"
name = "Presa de charca"
locomotion = "cilia"
[being.shape]
semi_major_axis = 30.0
semi_minor_axis = 12.0

[physiology.presa_de_charca.max_growth]
value = 8.0
unit = "h"
source = "Dato de prueba."
)");
    // Mal nombrado: se carga, pero se avisa.
    write("biomes/otro_nombre.toml", "[[biome]]\nid = \"charca\"\nname = \"Charca\"\nbacteria = 100\n");
    write("cosas/nada.toml", "");
    write("lang/en.txt", "");
    std::vector<std::string> messages;
    REQUIRE(loadPack(dir.string(), messages));
    auto said = [&](const std::string& fragment) {
        for (const std::string& m : messages)
            if (m.find(fragment) != std::string::npos)
                return true;
        return false;
    };
    CHECK_FALSE(said("no es un ser ni una etiqueta"));
    CHECK(said("tiene que llamarse como su id ('charca.toml')"));
    CHECK(said("carpeta desconocida 'cosas'"));
    CHECK_FALSE(said("'lang'"));
    const auto hunter = findSpecies("cazador_de_charca");
    const auto prey = findSpecies("presa_de_charca");
    REQUIRE(hunter);
    REQUIRE(prey);
    CHECK(*hunter < *prey); // el orden de [pack.order]
    CHECK(speciesInfo(*hunter).prey.test(static_cast<std::size_t>(*prey)));
    CHECK(speciesPhysiology(*prey).growthMax.present); // la fisiología, en el archivo del ser
    CHECK(findBiome("charca"));
    const PackInfo* info = findPack("charca");
    REQUIRE(info);
    CHECK(info->fingerprint != 0);
}

TEST_CASE("contenido: cada archivo de fábrica es una definición con el nombre de su id" *
          doctest::test_suite("paquetes")) {
    namespace fs = std::filesystem;
    const fs::path base = assetPath("content/base");
    int files = 0;
    for (const auto& folder : fs::directory_iterator(base)) {
        if (!folder.is_directory())
            continue;
        for (const auto& f : fs::directory_iterator(folder.path())) {
            std::ifstream in(f.path());
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            CHECK_MESSAGE(text.find("id = \"" + f.path().stem().string() + "\"") != std::string::npos,
                          f.path().string());
            ++files;
        }
    }
    CHECK(files > 150);
    // Los ids viejos (en español) siguen valiendo: los nombran las placas y escenas guardadas.
    CHECK(findSpecies("neutrofilo") == findSpecies("neutrophil"));
    CHECK(findBiome("charco") == BiomeId::Puddle);
    CHECK(findMineral("arcilla") == MineralKind::Clay);
}

TEST_CASE("contenido: prolongaciones rígidas (cuernos de Ceratium, brazos de Staurastrum)" * doctest::test_suite("paquetes")) {
    const auto ceratium = findSpecies("ceratium");
    REQUIRE(ceratium);
    const std::vector<ProcessSpec>& horns = speciesInfo(*ceratium).processes;
    REQUIRE(horns.size() == 3);
    CHECK(horns[0].kind == ProcessKind::Horn);
    CHECK(horns[0].at == doctest::Approx(0.0f));
    CHECK(horns[0].length == doctest::Approx(90.0f));
    CHECK(horns[1].at == doctest::Approx(172.0f * 3.14159265f / 180.0f));
    // El cuerno apical es más largo que todo el cuerpo (60 µm): por eso no puede ser membrana.
    CHECK(horns[0].length > 2.0f * speciesInfo(*ceratium).spec.radiusX);

    // Tres brazos a 120°: la simetría radial reparte las copias en toda la vuelta.
    const auto staurastrum = findSpecies("staurastrum");
    REQUIRE(staurastrum);
    const ProcessSpec& arms = speciesInfo(*staurastrum).processes.at(0);
    CHECK(arms.count == 3);
    CHECK(processAngle(arms, 1) - processAngle(arms, 0) == doctest::Approx(2.0f * 3.14159265f / 3.0f));

    // Un abanico centrado: tres espinas en 90° quedan a −30°, 0° y +30° de la dirección.
    ProcessSpec fan;
    fan.at = 3.14159265f;
    fan.count = 3;
    fan.spread = 3.14159265f / 2.0f;
    CHECK(processAngle(fan, 0) == doctest::Approx(3.14159265f - 3.14159265f / 6.0f));
    CHECK(processAngle(fan, 1) == doctest::Approx(3.14159265f));

    // La curvatura es un arco: la punta queda a la distancia de la cuerda y el largo se conserva
    // escalado con la célula.
    ProcessSpec bent;
    bent.length = 20.0f;
    bent.curve = 3.14159265f / 2.0f;
    const Vec2 tip = processAxisPoint(bent, 1.0f, 1.0f);
    const float radius = 20.0f / bent.curve;
    CHECK(length(tip) == doctest::Approx(radius * std::sqrt(2.0f)).epsilon(1e-3));
    CHECK(processAxisPoint(bent, 1.0f, 2.0f).x == doctest::Approx(2.0f * tip.x));
    CHECK(processWidth(bent, 0.0f, 1.0f) == doctest::Approx(bent.baseWidth));
}
