// Contenido como datos (fase 20.0): los seres se cargan de paquetes TOML. Suite "paquetes": corre
// en su propio proceso (cargar seres de prueba cambia el registro global que usan los demás tests).
#include "bio/species.hpp"
#include "core/paths.hpp"

#include <doctest/doctest.h>

#include <string>

namespace {

bool warned(const std::string& fragment) {
    for (const std::string& w : speciesLoadWarnings())
        if (w.find(fragment) != std::string::npos)
            return true;
    return false;
}

} // namespace

TEST_CASE("contenido: el paquete de fábrica carga sus 24 seres en el orden de siempre, sin avisos" *
          doctest::test_suite("paquetes")) {
    CHECK(speciesCount() == 24);
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
