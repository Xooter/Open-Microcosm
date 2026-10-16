// Contenido como datos (fase 20.0): los seres se cargan de paquetes TOML. Suite "paquetes": corre
// en su propio proceso (cargar seres de prueba cambia el registro global que usan los demás tests).
#include "bio/body_plan.hpp"
#include "bio/species.hpp"
#include "core/paths.hpp"
#include "bio/physiology.hpp"
#include "game/biome.hpp"
#include "game/content.hpp"
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

TEST_CASE("contenido: el paquete de fábrica carga sus 27 seres en el orden de siempre, sin avisos" *
          doctest::test_suite("paquetes")) {
    CHECK(speciesCount() == 27); // 24 de la gota y 3 células humanas (fase 20b)
    CHECK(speciesLoadWarnings().empty());
    CHECK(std::string(speciesInfo(SpeciesId::Paramecium).key) == "paramecium");
    CHECK(std::string(speciesInfo(SpeciesId::Cyclotella).key) == "cyclotella");
    CHECK(findSpecies("amoeba") == SpeciesId::Amoeba);
    CHECK(speciesPack(SpeciesId::Amoeba) == "base");
    // Dietas resueltas desde ids: Didinium caza a los dos Paramecium.
    const SpeciesSet& prey = speciesInfo(SpeciesId::Didinium).prey;
    CHECK(prey.test(static_cast<std::size_t>(SpeciesId::Paramecium)));
    CHECK(prey.test(static_cast<std::size_t>(SpeciesId::ParameciumAurelia)));
    CHECK(prey.count() == 2);
    // Etiquetas: los hospedadores de los quitridios (antes, una máscara fija en el código).
    CHECK(speciesWithTag("hospeda_quitridios").count() == 8);
    CHECK(hasTag(SpeciesId::Pediastrum, "fotobionte"));
    CHECK(speciesGenome(SpeciesId::Arcella)[Gene::Armor] == doctest::Approx(0.9f));
}

TEST_CASE("contenido: un paquete agrega un ser sin compilar; las etiquetas resuelven dietas" *
          doctest::test_suite("paquetes")) {
    const int before = speciesCount();
    const std::string pack = R"(
[[ser]]
id = "colpidium_colpoda"
nombre = "Colpidium colpoda"
etiquetas = ["protista", "ciliado", "bacterivoro"]
locomocion = "cilios"
alimentacion = "surco_oral"
huye_de = ["didinium"]
ph = [6.0, 9.0]
temperatura = [5.0, 30.0]
reserva = "glucogeno"
[ser.forma]
semieje_mayor = 45.0
semieje_menor = 20.0
memoria_forma = 8.0
color = [170, 176, 150]

[[ser]]
id = "depredador_de_ciliados"
nombre = "Depredador de ciliados"
locomocion = "pseudopodo"
alimentacion = "fagocitosis"
presas = ["ciliado"]
[ser.forma]
semieje_mayor = 60.0
semieje_menor = 60.0
)";
    std::string error;
    REQUIRE(loadSpeciesText(pack, "prueba.biopack", "prueba", &error));
    CHECK(speciesCount() == before + 2);
    const auto colpidium = findSpecies("colpidium_colpoda");
    REQUIRE(colpidium);
    CHECK(speciesPack(*colpidium) == "prueba");
    CHECK(speciesInfo(*colpidium).flee.test(static_cast<std::size_t>(SpeciesId::Didinium)));
    CHECK(speciesInfo(*colpidium).initialCount == 0); // no aparece solo en las gotas
    // "ciliado" es una etiqueta: incluye a los de fábrica y al nuevo.
    const SpeciesSet& prey = speciesInfo(*findSpecies("depredador_de_ciliados")).prey;
    CHECK(prey.test(static_cast<std::size_t>(*colpidium)));
    CHECK(prey.test(static_cast<std::size_t>(SpeciesId::Paramecium)));
    CHECK_FALSE(prey.test(static_cast<std::size_t>(SpeciesId::Euglena)));
}

TEST_CASE("contenido: los errores de un paquete se avisan y no tumban el programa" * doctest::test_suite("paquetes")) {
    const int before = speciesCount();
    const std::string pack = R"(
[[ser]]
id = "sin_forma"
nombre = "Sin forma"
locomocion = "cilios"

[[ser]]
id = "locomocion_rara"
nombre = "Locomoción rara"
locomocion = "patas"
[ser.forma]
semieje_mayor = 10.0
semieje_menor = 10.0

[[ser]]
id = "paramecium"
nombre = "Otro Paramecium"
locomocion = "cilios"
[ser.forma]
semieje_mayor = 10.0
semieje_menor = 10.0

[[ser]]
id = "con_tipeo"
nombre = "Con tipeo"
locomocion = "cilios"
presas = ["no_existe"]
velocidad_relativo = 2.0
[ser.forma]
semieje_mayor = 10.0
semieje_menor = 10.0
)";
    REQUIRE(loadSpeciesText(pack, "errores.biopack", "errores"));
    CHECK(speciesCount() == before + 1); // solo "con_tipeo" (los avisos no impiden cargarlo)
    CHECK(warned("falta [ser.forma]"));
    CHECK(warned("'locomocion' tiene que ser uno de"));
    CHECK(warned("el ser 'paramecium' ya existe"));
    CHECK(warned("clave desconocida 'velocidad_relativo'"));
    CHECK(warned("'no_existe' en presas"));
    CHECK(std::string(speciesInfo(SpeciesId::Paramecium).name) == "Paramecium");
    std::string error;
    CHECK_FALSE(loadSpeciesText("[[ser]\nid = ", "roto.biopack", "roto", &error));
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
[[plan]]
id = "celula_con_nucleolos"
nombre = "celula con nucleolos"
estructuras = [
    { tipo = "membrana_plasmatica", fuente = "simulada", disposicion = "envoltura", campo_claro = true, nombre = "membrana" },
    { tipo = "nucleo", fuente = "simulada", disposicion = "interior", cantidad = 1, campo_claro = true, nombre = "nucleo" },
    { tipo = "nucleolo", fuente = "declarada", disposicion = "interior", largo = 2.0, ancho = 2.0, cantidad = 3, campo_claro = true, nombre = "nucleolos" },
]
)", "planes.biopack", "prueba"));
    REQUIRE(loadSpeciesText(R"(
[[ser]]
id = "con_plan_propio"
nombre = "Con plan propio"
locomocion = "pseudopodo"
plan = "celula_con_nucleolos"
[ser.forma]
semieje_mayor = 20.0
semieje_menor = 15.0
[[ser]]
id = "plan_inexistente"
nombre = "Plan inexistente"
locomocion = "pseudopodo"
plan = "no_existe"
[ser.forma]
semieje_mayor = 20.0
semieje_menor = 15.0
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
    CHECK(biome(BiomeId::Puddle).key == "charco");
    CHECK(biome(BiomeId::SeaIce).key == "hielo_marino");
    CHECK(findBiome("intestino_termita") == BiomeId::TermiteGut);
    CHECK_FALSE(findBiome("marte").has_value());
    const Biome& gut = biome(BiomeId::TermiteGut);
    CHECK(gut.medium.anoxic);
    CHECK(gut.medium.temperature == doctest::Approx(28.0f));
    CHECK(gut.sediment[MineralKind::Clay] == 60);
    CHECK(gut.sediment.rockMax == doctest::Approx(120.0f)); // lo que falta queda por omisión
    // Trichonympha es simbionte obligado: solo está donde el bioma la nombra.
    CHECK(gut.weight(SpeciesId::Trichonympha) == doctest::Approx(1.0f));
    CHECK(biome(BiomeId::Puddle).weight(SpeciesId::Trichonympha) == 0.0f);
    CHECK(biome(BiomeId::Puddle).weight(SpeciesId::Paramecium) == doctest::Approx(1.0f));
    CHECK(biome(BiomeId::Moss).weight(SpeciesId::Arcella) == doctest::Approx(3.0f));
    CHECK(biome(BiomeId::Moss).weight(SpeciesId::Stentor) == 0.0f);
    CHECK(WorldConfig{}.speciesWeight[static_cast<int>(SpeciesId::Trichonympha)] == 0.0f);
}

TEST_CASE("contenido: un paquete agrega un bioma; sus pesos nombran seres de cualquier paquete" *
          doctest::test_suite("paquetes")) {
    // El bioma nombra un ser que todavía no está cargado: los pesos se buscan por id al armar la gota.
    const std::string biomes = R"(
[[bioma]]
id = "charca_de_prueba"
nombre = "Charca de prueba"
bacterias = 10000
peso_base = 0.0
pesos = { paramecium = 2.0, halteria_grandinella = 1.5 }
[bioma.medio]
temperatura = 15.0
fosfato = 3.0
[bioma.sedimento]
arcilla = 20
grano_radio = [30.0, 60.0]
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
[[ser]]
id = "halteria_grandinella"
nombre = "Halteria grandinella"
locomocion = "cilios"
[ser.forma]
semieje_mayor = 15.0
semieje_menor = 13.0
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
[[bioma]]
id = "bioma_roto"
nombre = "Bioma roto"
humedad = 3
[bioma.medio]
fosfato = -5.0
luz = 2.0
presion = 1.0
[[bioma]]
id = "charco"
nombre = "Otro charco"
)";
    std::string error;
    CHECK(loadBiomesText(bad, "roto.toml", "roto", &error));
    CHECK(biomeCount() == before);
    CHECK_FALSE(findBiome("bioma_roto").has_value());
    CHECK(biomeWarned("clave desconocida 'humedad'"));
    CHECK(biomeWarned("clave desconocida 'presion'"));
    CHECK(biomeWarned("'fosfato' no puede ser negativo"));
    CHECK(biomeWarned("'luz' es relativa"));
    CHECK(biomeWarned("el bioma 'charco' ya existe"));
    CHECK(biome(BiomeId::Puddle).name == "Charco");
    // Un TOML mal formado: false con el motivo, sin tocar el registro.
    CHECK_FALSE(loadBiomesText("[[bioma]\nid = ", "mal.toml", "mal", &error));
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
    CHECK(mineralInfo(MineralKind::Clay).key == "arcilla");
    CHECK(mineralInfo(MineralKind::Clay).flocculates);
    CHECK(findMineral("frustula") == MineralKind::Frustule);
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
nombre = "Calcita"
descripcion = "Carbonato de calcio: romboedros transparentes con mucho relieve."
radio = [2.0, 8.0]
diferencia_densidad = 1710.0
opacidad = 0.1
estilo = "grano_anguloso"
)",
                             "calcita.toml", "rocas", &error));
    const auto calcite = findMineral("calcita");
    REQUIRE(calcite.has_value());
    CHECK(mineralInfo(*calcite).pack == "rocas");
    REQUIRE(loadBiomesText(R"(
[[bioma]]
id = "laguna_calcarea"
nombre = "Laguna calcárea"
peso_base = 0.0
[bioma.sedimento]
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
nombre = "Piedra rara"
estilo = "brillante"
opacidad = 3.0
)",
                           "rara.toml", "raro", &error));
    CHECK(mineralCount() == before);
    CHECK(mineralWarned("falta 'radio'"));
    CHECK(mineralWarned("'estilo' tiene que ser uno de"));
    CHECK(mineralWarned("'opacidad' va de 0 a 1"));
}

namespace {

// Un ser que solo existe en este .biopack (Colpoda cucullus: ciliado bacterívoro de las infusiones
// de heno, ~50–100 µm, en forma de riñón). La fisiología es un dato de prueba.
const char* const kColpodaPack = R"(
[paquete]
id = "infusiones"
nombre = "Ciliados de infusiones"
version = "1"
autor = "pruebas"
licencia = "GPL-3.0-or-later"

[[ser]]
id = "colpoda_cucullus"
nombre = "Colpoda cucullus"
etiquetas = ["protista", "ciliado", "bacterivoro"]
descripcion = "Ciliado en forma de rinon de las infusiones de heno: come bacterias y se enquista al secarse."
locomocion = "cilios"
plan = "ciliado"
alimentacion = "surco_oral"
enquista = true
temperatura = [5.0, 30.0]
reserva = "glucogeno"
[ser.forma]
semieje_mayor = 35.0
semieje_menor = 22.0
memoria_forma = 8.0
color = [168, 170, 140]

[fisiologia.colpoda_cucullus.crecimiento_maximo]
valor = 4.0
unidad = "h"
temperatura = 20.0
fuente = "Dato de prueba (no publicado)."
estimacion = true

[[bioma]]
id = "infusion_de_heno"
nombre = "Infusion de heno"
peso_base = 0.0
pesos = { colpoda_cucullus = 1.0, paramecium = 1.0 }
bacterias = 40000
[bioma.medio]
aporte_organico = 200.0
)";

std::string writeTemp(const std::string& name, const std::string& text) {
    const auto dir = std::filesystem::temp_directory_path() / "biogame_tests_paquetes";
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
    CHECK(text.find("[[ser]]") != std::string::npos);
    CHECK(text.find("crecimiento_maximo") != std::string::npos);
    CHECK(text.find("[[escena]]") != std::string::npos);
    CHECK(text.find("programa_minimo") != std::string::npos);
    for (const auto& [from, to] : {std::pair<std::string, std::string>{"colpoda_cucullus", "colpoda_inflata"},
                                   {"infusion_de_heno", "infusion_de_lechuga"}})
        for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
            text.replace(at, from.size(), to);
    spec.scenes.clear();
    messages.clear();
    // Sin la escena (instalarla escribiría en la carpeta de placas del usuario).
    const std::size_t scene = text.find("[[escena]]");
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
    CHECK_FALSE(loadPack(writeTemp("base.biopack", "[paquete]\nid = \"base\"\nversion = \"9\"\n"), messages));
    // Id con caracteres que no van (podría usarse para armar rutas).
    CHECK_FALSE(loadPack(writeTemp("ruta.biopack", "[paquete]\nid = \"../x\"\nversion = \"1\"\n"), messages));
    // Para un programa más nuevo.
    CHECK_FALSE(loadPack(writeTemp("futuro.biopack",
                                   "[paquete]\nid = \"futuro\"\nversion = \"1\"\nprograma_minimo = \"999.0\"\n"),
                         messages));
    // Sin manifiesto, o TOML roto.
    CHECK_FALSE(loadPack(writeTemp("suelto.biopack", "[[ser]]\nid = \"x\"\n"), messages));
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
    CHECK(loadPack(writeTemp("raro.biopack", "[paquete]\nid = \"raro\"\nversion = \"1\"\nlicencia = \"MIT\"\n"
                                             "[[criatura]]\nid = \"x\"\n"),
                   messages));
    bool unknown = false;
    for (const std::string& m : messages)
        unknown = unknown || m.find("clave desconocida 'criatura'") != std::string::npos;
    CHECK(unknown);
}
