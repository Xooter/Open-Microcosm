#include "game/biome.hpp"

#include "core/rng.hpp"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>

namespace {

using S = SpeciesId;

// Pesos: todas las especies con `base`, salvo las listadas.
void setWeights(Biome& b, float base, std::initializer_list<std::pair<S, float>> special) {
    for (float& w : b.weight)
        w = base;
    for (const auto& [id, w] : special)
        b.weight[static_cast<int>(id)] = w;
}

std::array<Biome, kBiomeCount> makeBiomes() {
    std::array<Biome, kBiomeCount> all{};

    // Charco de lluvia: tibio, a pleno sol y muy rico en materia orgánica (hojas, barro).
    Biome& puddle = all[static_cast<int>(BiomeId::Puddle)];
    puddle.name = "Charco";
    puddle.description = "Agua de lluvia sobre barro y hojas: tibia, luminosa y muy rica. Casi todo vive aca.";
    puddle.medium.temperature = 22.0f;
    puddle.medium.organicInflow = 300.0f; // hojas y barro: una infusión natural
    setWeights(puddle, 1.0f, {{S::Hydra, 0.0f}, {S::Philodina, 0.5f}, {S::Naegleria, 1.0f}, {S::Trichonympha, 0.0f}});
    puddle.bacteria = 50'000;
    puddle.detritus = 60;
    puddle.cyanoFilaments = 15;
    puddle.cyanoTMin = 10.0f;
    puddle.cyanoTMax = 35.0f;
    puddle.saprolegnia = 4;
    puddle.zoophagus = 2;
    puddle.chytridClusters = 2;
    puddle.patchiness = 0.6f;

    // Estanque: más frío y menos rico; algas coloniales, Hydra y rotíferos planctónicos.
    Biome& pond = all[static_cast<int>(BiomeId::Pond)];
    pond = puddle;
    pond.name = "Estanque";
    pond.description = "Agua quieta y templada con plantas acuaticas: Volvox, Spirogyra, Hydra y rotiferos.";
    pond.medium.temperature = 17.0f;
    pond.medium.organicInflow = 60.0f;
    pond.medium.phosphate = 20.0f;
    pond.medium.nitrogen = 300.0f;
    setWeights(pond, 1.0f, {{S::Volvox, 2.0f}, {S::Spirogyra, 1.5f}, {S::Hydra, 1.0f}, {S::Rotifer, 1.5f},
                            {S::Stentor, 1.5f}, {S::Philodina, 0.5f}, {S::Naegleria, 0.0f}, {S::Trichonympha, 0.0f},
                            {S::Candida, 0.3f}});
    pond.medium.naupliusArrivalsPerHour = 0.1f;
    pond.bacteria = 30'000;
    pond.patchiness = 0.3f;

    // Musgo húmedo: película de agua fría, en sombra y ácida (pH ~5,5). Amebas tecadas,
    // rotíferos bdeloideos y Nostoc.
    Biome& moss = all[static_cast<int>(BiomeId::Moss)];
    moss = puddle;
    moss.name = "Musgo humedo";
    moss.description = "La pelicula de agua entre las hojitas del musgo: fria, en sombra y acida. Amebas con testa y bdeloideos.";
    moss.medium.temperature = 12.0f;
    moss.medium.organicInflow = 40.0f; // el musgo y el suelo liberan ácidos húmicos y azúcares
    moss.medium.surfaceLight = 0.4f;
    moss.medium.alkalinity = 80.0f; // agua blanda: el CO₂ del aire la acidifica
    moss.medium.phosphate = 10.0f;
    moss.medium.nitrogen = 100.0f;
    setWeights(moss, 0.0f, {{S::Arcella, 3.0f}, {S::Philodina, 1.0f}, {S::Euplotes, 1.0f}, {S::Vorticella, 1.0f},
                            {S::Paramecium, 0.3f}, {S::Chlamydomonas, 0.4f}, {S::Euglena, 0.5f}, {S::Naegleria, 3.0f},
                            {S::Navicula, 0.5f}, {S::Candida, 1.0f}, {S::Amoeba, 1.0f}});
    moss.medium.naupliusArrivalsPerHour = 0.0f;
    moss.bacteria = 30'000;
    moss.cyanoFilaments = 8; // Nostoc
    moss.cyanoTMin = 0.0f;
    moss.zoophagus = 3;      // atrapa a los bdeloideos
    moss.patchiness = 0.4f;

    // Intestino de termita: oscuro, sin oxígeno, lleno de astillas de madera y de bacterias.
    Biome& gut = all[static_cast<int>(BiomeId::TermiteGut)];
    gut = puddle;
    gut.name = "Intestino de termita";
    gut.description = "Anoxico y oscuro, repleto de astillas de madera. Trichonympha digiere la celulosa por la termita.";
    gut.medium.temperature = 28.0f;
    gut.medium.surfaceLight = 0.0f;
    gut.medium.anoxic = true;
    gut.medium.o2AirExchange = 0.0f;
    gut.medium.detritusSpawnRate = 3.0f; // la termita sigue comiendo madera
    gut.medium.phosphate = 50.0f;
    gut.medium.nitrogen = 200.0f; // la madera es pobre en N: lo fijan las bacterias del intestino
    setWeights(gut, 0.0f, {{S::Trichonympha, 1.0f}, {S::Candida, 0.5f}});
    gut.weight[static_cast<int>(S::Trichonympha)] = 1.0f;
    gut.medium.naupliusArrivalsPerHour = 0.0f;
    gut.bacteria = 80'000;
    gut.detritus = 400;
    gut.cyanoFilaments = 0;
    gut.saprolegnia = gut.zoophagus = gut.chytridClusters = 0;
    gut.patchiness = 0.8f;

    // Fuente termal: 42 °C y alcalina. Tapetes de Mastigocladus y la ameba Naegleria.
    Biome& spring = all[static_cast<int>(BiomeId::HotSpring)];
    spring = puddle;
    spring.name = "Fuente termal";
    spring.description = "Agua a 42 grados: casi ningun eucariota la soporta. Cianobacterias termofilas y Naegleria.";
    spring.medium.temperature = 42.0f;
    spring.medium.organicInflow = 20.0f; // exudados de los tapetes microbianos
    spring.medium.alkalinity = 3000.0f;
    spring.medium.phosphate = 30.0f;
    spring.medium.nitrogen = 50.0f; // pobre en N: domina la fijadora
    setWeights(spring, 0.0f, {{S::Naegleria, 4.0f}, {S::Candida, 0.5f}});
    spring.medium.naupliusArrivalsPerHour = 0.0f;
    spring.bacteria = 60'000;
    spring.cyanoFilaments = 35; // Mastigocladus laminosus
    spring.cyanoTMin = 30.0f;
    spring.cyanoTMax = 57.0f;
    spring.saprolegnia = spring.zoophagus = spring.chytridClusters = 0;
    spring.patchiness = 0.2f;

    // Hielo marino: canales de salmuera a −2 °C, muy salados y con poca luz. Diatomeas,
    // Chlamydomonas de hielo, Euplotes y nauplios de copépodo.
    Biome& ice = all[static_cast<int>(BiomeId::SeaIce)];
    ice = puddle;
    ice.name = "Hielo marino";
    ice.description = "Canales de salmuera dentro del hielo: -2 grados y el doble de sal que el mar. Diatomeas.";
    ice.medium.temperature = -2.0f;
    ice.medium.organicInflow = 10.0f;
    ice.medium.salinity = 60.0f;
    ice.medium.surfaceLight = 0.3f;
    ice.medium.phosphate = 5.0f;
    ice.medium.nitrogen = 60.0f;
    ice.medium.alkalinity = 2300.0f; // agua de mar
    setWeights(ice, 0.0f, {{S::Navicula, 4.0f}, {S::Chlamydomonas, 0.6f}, {S::Euplotes, 2.0f}, {S::Candida, 0.5f},
                           {S::Nauplius, 1.0f}});
    ice.medium.naupliusArrivalsPerHour = 0.1f;
    ice.bacteria = 20'000;
    ice.cyanoFilaments = 0;
    ice.saprolegnia = ice.zoophagus = 0;
    ice.chytridClusters = 3; // quitridios marinos de diatomeas
    ice.patchiness = 0.3f;
    return all;
}

const std::array<Biome, kBiomeCount> kBiomes = makeBiomes();

} // namespace

const Biome& biome(BiomeId id) { return kBiomes[static_cast<int>(id)]; }

WorldConfig makeDrop(BiomeId id, std::uint64_t seed) {
    const Biome& b = biome(id);
    WorldConfig c;
    c.seed = seed;
    c.medium = b.medium;
    Rng rng(seed, rng_stream::kPopulate + 900, 0, 0);
    // Variación de la gota: ninguna es igual a otra.
    c.medium.temperature += 1.5f * rng.normal();
    c.medium.surfaceLight = std::clamp(c.medium.surfaceLight * rng.uniform(0.8f, 1.2f), 0.0f, 1.0f);
    c.medium.phosphate *= rng.uniform(0.6f, 1.6f);
    c.medium.nitrogen *= rng.uniform(0.6f, 1.6f);
    c.medium.alkalinity *= rng.uniform(0.7f, 1.4f);
    for (int k = 0; k < kSpeciesCount; ++k) {
        Rng r(seed, rng_stream::kPopulate + 901, static_cast<std::uint64_t>(k), 0);
        const bool absent = r.uniform() < 0.15f; // algunas especies no llegaron a esta gota
        c.speciesWeight[k] = absent ? 0.0f : b.weight[k] * r.uniform(0.5f, 1.5f);
    }
    c.bacteria = static_cast<std::size_t>(static_cast<float>(b.bacteria) * rng.uniform(0.7f, 1.3f));
    c.detritus = static_cast<std::size_t>(static_cast<float>(b.detritus) * rng.uniform(0.6f, 1.5f));
    c.cyanoFilaments = static_cast<int>(static_cast<float>(b.cyanoFilaments) * rng.uniform(0.5f, 1.5f));
    c.cyanoTMin = b.cyanoTMin;
    c.cyanoTMax = b.cyanoTMax;
    c.saprolegnia = b.saprolegnia;
    c.zoophagus = b.zoophagus;
    c.chytridClusters = b.chytridClusters;
    c.patchiness = std::clamp(b.patchiness * rng.uniform(0.5f, 1.5f), 0.0f, 1.0f);
    return c;
}
