// Placa de Petri preconfigurable: ida y vuelta del texto y reproducibilidad por seed.
#include "game/plate_spec.hpp"

#include <doctest/doctest.h>

TEST_CASE("PlateSpec: serializar y leer devuelve la misma placa") {
    PlateSpec a;
    a.biome = 2;
    a.seed = 777;
    a.content = DropContent::Full;
    a.rebuild();
    a.config.medium.temperature = 31.5f;
    a.config.medium.phosphate = 12.25f;
    a.config.medium.anoxic = true;
    a.config.sediment[MineralKind::Quartz] = 17;
    a.config.speciesWeight[static_cast<int>(SpeciesId::Paramecium)] = 3.5f;
    a.config.bacteria = 1234;

    PlateSpec b;
    REQUIRE(parsePlate(serializePlate(a), b));
    CHECK(b.biome == a.biome);
    CHECK(b.seed == a.seed);
    CHECK(b.content == a.content);
    CHECK(b.config.medium.temperature == doctest::Approx(31.5f));
    CHECK(b.config.medium.phosphate == doctest::Approx(12.25f));
    CHECK(b.config.medium.anoxic);
    CHECK(b.config.sediment[MineralKind::Quartz] == 17);
    CHECK(b.config.speciesWeight[static_cast<int>(SpeciesId::Paramecium)] == doctest::Approx(3.5f));
    CHECK(b.config.bacteria == 1234);
    CHECK(serializePlate(b) == serializePlate(a));
}

TEST_CASE("PlateSpec: las placas viejas guardan el sedimento con claves en inglés y se leen igual") {
    PlateSpec p;
    REQUIRE(parsePlate("plate_version=1\nbiome=0\nseed=5\ncontent=0\nsediment.clay=33\nsediment.iron_oxide=4\n"
                       "sediment.frustules=2\nsediment.rocks=1\n",
                       p));
    CHECK(p.config.sediment[MineralKind::Clay] == 33);
    CHECK(p.config.sediment[MineralKind::IronOxide] == 4);
    CHECK(p.config.sediment[MineralKind::Frustule] == 2);
    CHECK(p.config.sediment.rocks == 1);
    // Al guardarla de nuevo, con los ids de los minerales.
    CHECK(serializePlate(p).find("sediment.arcilla=33") != std::string::npos);
}

TEST_CASE("PlateSpec: la versión 2 nombra bioma, seres y paquetes por id; la 1 se sigue leyendo") {
    PlateSpec a;
    a.biome = static_cast<int>(BiomeId::Moss);
    a.seed = 12;
    a.rebuild();
    a.config.speciesWeight[static_cast<int>(SpeciesId::Arcella)] = 2.5f;
    const std::string text = serializePlate(a);
    CHECK(text.find("plate_version=2\n") != std::string::npos);
    CHECK(text.find("biome=musgo\n") != std::string::npos);
    CHECK(text.find("w.arcella=2.5\n") != std::string::npos);
    CHECK(text.find("pack=base ") != std::string::npos);
    std::vector<std::string> missing;
    PlateSpec b;
    REQUIRE(parsePlate(text, b, &missing));
    CHECK(missing.empty());
    CHECK(b.biome == a.biome);
    CHECK(serializePlate(b) == text);

    // Versión 1: el bioma por índice y los pesos por nombre del ser.
    PlateSpec old;
    REQUIRE(parsePlate("plate_version=1\nbiome=2\nseed=12\ncontent=0\nw.Arcella=2.5\n", old, &missing));
    CHECK(missing.empty());
    CHECK(old.biome == static_cast<int>(BiomeId::Moss));
    CHECK(old.config.speciesWeight[static_cast<int>(SpeciesId::Arcella)] == doctest::Approx(2.5f));

    // Lo que no está cargado se avisa y la placa se lee igual.
    PlateSpec other;
    REQUIRE(parsePlate("plate_version=2\nbiome=laguna_de_marte\npack=marte 3 0123456789abcdef\nseed=1\n"
                       "w.dodo_microscopico=1\nsediment.meteorito=4\n",
                       other, &missing));
    CHECK(missing.size() == 4);
    CHECK(other.biome == 0);
}

TEST_CASE("PlateSpec: texto sin version no es una placa") {
    PlateSpec p;
    CHECK_FALSE(parsePlate("biome=1\nseed=3\n", p));
}

TEST_CASE("PlateSpec: la placa vacia no tiene vida ni sedimento") {
    PlateSpec p;
    p.content = DropContent::EmptyPetri;
    p.rebuild();
    CHECK(p.wildlife() == 0.0f);
    CHECK(p.config.sediment[MineralKind::Quartz] == 0);
    CHECK(p.config.detritus == 0);
}

TEST_CASE("PlateSpec: la misma seed da la misma placa") {
    PlateSpec a, b;
    a.biome = b.biome = 1;
    a.seed = b.seed = 99;
    a.content = b.content = DropContent::Full;
    a.rebuild();
    b.rebuild();
    CHECK(serializePlate(a) == serializePlate(b));
    b.seed = 100;
    b.rebuild();
    CHECK(serializePlate(a) != serializePlate(b));
}
