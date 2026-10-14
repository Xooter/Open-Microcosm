#include "game/biome.hpp"

#include "core/rng.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
    setWeights(puddle, 1.0f, {{S::Naegleria, 1.0f}, {S::Trichonympha, 0.0f}, {S::Mougeotia, 0.5f}, {S::Zygnema, 0.5f},
                              {S::Oedogonium, 0.5f}});
    puddle.bacteria = 50'000;
    puddle.detritus = 60;
    puddle.cyanoFilaments = 15;
    puddle.oscillatoriaFilaments = 8;
    puddle.cyanoTMin = 10.0f;
    puddle.cyanoTMax = 35.0f;
    puddle.saprolegnia = 4;
    puddle.chytridClusters = 2;
    puddle.allomyces = 2;  // sobre insectos y hojas muertas
    puddle.lichens = 1;
    puddle.physarum = 1;   // el moho de la hojarasca húmeda
    puddle.patchiness = 0.6f;
    // Barro: mucha arcilla y limo, granos de arena y óxidos de hierro del suelo.
    puddle.sediment = {.clay = 500, .quartz = 140, .mica = 15, .ironOxide = 40, .frustules = 10, .rocks = 5};
    puddle.medium.dustRate = 0.03f;

    // Estanque: más frío y menos rico; algas coloniales y grandes ciliados.
    Biome& pond = all[static_cast<int>(BiomeId::Pond)];
    pond = puddle;
    pond.name = "Estanque";
    pond.description = "Agua quieta y templada con plantas acuaticas: Volvox, Spirogyra y Stentor.";
    pond.medium.temperature = 17.0f;
    pond.medium.organicInflow = 60.0f;
    pond.medium.phosphate = 20.0f;
    pond.medium.nitrogen = 300.0f;
    pond.medium.silicate = 150.0f;
    pond.medium.iron = 0.5f;
    pond.medium.vitamin = 1e-3f;
    setWeights(pond, 1.0f, {{S::Volvox, 2.0f}, {S::Spirogyra, 1.5f}, {S::Stentor, 1.5f}, {S::Naegleria, 0.0f},
                            {S::Trichonympha, 0.0f}, {S::Candida, 0.3f}, {S::Oedogonium, 1.5f}});
    pond.bacteria = 30'000;
    pond.oscillatoriaFilaments = 5;
    pond.physarum = 0;     // no vive en agua libre
    pond.allomyces = 3;
    pond.patchiness = 0.3f;
    // Fondo fino y más limpio; muchas frústulas de diatomeas muertas.
    pond.sediment = {.clay = 160, .quartz = 50, .mica = 8, .ironOxide = 12, .frustules = 30, .rocks = 2};
    pond.medium.dustRate = 0.01f;

    // Musgo húmedo: película de agua fría, en sombra y ácida (pH ~5,5). Amebas tecadas,
    // Naegleria y Nostoc.
    Biome& moss = all[static_cast<int>(BiomeId::Moss)];
    moss = puddle;
    moss.name = "Musgo humedo";
    moss.description = "La pelicula de agua entre las hojitas del musgo: fria, en sombra y acida. Amebas con testa y Naegleria.";
    moss.medium.temperature = 12.0f;
    moss.medium.organicInflow = 40.0f; // el musgo y el suelo liberan ácidos húmicos y azúcares
    moss.medium.surfaceLight = 0.4f;
    moss.medium.alkalinity = 80.0f; // agua blanda: el CO₂ del aire la acidifica
    moss.medium.phosphate = 10.0f;
    moss.medium.nitrogen = 100.0f;
    moss.medium.silicate = 60.0f;  // aguas ácidas y blandas: poca sílice
    moss.medium.iron = 1.5f;       // el hierro es más soluble a pH bajo
    moss.medium.vitamin = 2e-3f;
    setWeights(moss, 0.0f, {{S::Arcella, 3.0f}, {S::Euplotes, 1.0f}, {S::Vorticella, 1.0f}, {S::Paramecium, 0.3f},
                            {S::Chlamydomonas, 0.4f}, {S::Euglena, 0.5f}, {S::Naegleria, 3.0f}, {S::Navicula, 0.5f},
                            {S::Candida, 1.0f}, {S::Amoeba, 1.0f}, {S::Zygnema, 0.5f}});
    moss.bacteria = 30'000;
    moss.cyanoFilaments = 8; // Nostoc
    moss.oscillatoriaFilaments = 4;
    moss.cyanoTMin = 0.0f;
    moss.physarum = 3;       // hojarasca y musgo húmedo
    moss.lichens = 3;        // los líquenes crecen sobre el musgo
    moss.allomyces = 1;
    moss.patchiness = 0.4f;
    // Suelo entre las hojitas: partículas minerales y humus ferruginoso.
    moss.sediment = {.clay = 250, .quartz = 90, .mica = 12, .ironOxide = 60, .frustules = 5, .rocks = 3};
    moss.medium.dustRate = 0.02f;

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
    gut.medium.silicate = 20.0f;
    gut.medium.iron = 2.0f;
    gut.medium.vitamin = 5e-3f;   // la microbiota intestinal sintetiza mucha B12
    setWeights(gut, 0.0f, {{S::Trichonympha, 1.0f}, {S::Candida, 0.5f}});
    gut.weight[static_cast<int>(S::Trichonympha)] = 1.0f;
    gut.bacteria = 80'000;
    gut.detritus = 400;
    gut.cyanoFilaments = 0;
    gut.oscillatoriaFilaments = 0;
    gut.saprolegnia = gut.chytridClusters = 0;
    gut.allomyces = gut.lichens = gut.physarum = 0;
    gut.patchiness = 0.8f;
    // Algo de suelo que la termita traga junto con la madera; sin granos ni polvo del aire.
    gut.sediment = {.clay = 60, .quartz = 10};
    gut.medium.dustRate = 0.0f;

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
    spring.medium.silicate = 600.0f; // el agua caliente disuelve la sílice de la roca
    spring.medium.iron = 3.0f;
    spring.medium.vitamin = 1e-3f;
    setWeights(spring, 0.0f, {{S::Naegleria, 4.0f}, {S::Candida, 0.5f}});
    spring.bacteria = 60'000;
    spring.cyanoFilaments = 35; // Mastigocladus laminosus
    spring.oscillatoriaFilaments = 15; // tapetes de Oscillatoria/Phormidium termófilas
    spring.cyanoTMin = 30.0f;
    spring.cyanoTMax = 57.0f;
    spring.saprolegnia = spring.chytridClusters = 0;
    spring.allomyces = spring.lichens = spring.physarum = 0; // 42 °C es demasiado
    spring.patchiness = 0.2f;
    // Sínter silíceo, glóbulos de azufre y precipitados de hierro.
    spring.sediment = {.clay = 40, .quartz = 60, .ironOxide = 70, .sulfur = 140, .rocks = 3};
    spring.medium.dustRate = 0.01f;

    // Hielo marino: canales de salmuera a −2 °C, muy salados y con poca luz. Diatomeas,
    // Chlamydomonas de hielo y Euplotes.
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
    ice.medium.silicate = 100.0f;
    ice.medium.iron = 0.2f;        // hierro escaso, como en el mar polar
    ice.medium.vitamin = 5e-4f;
    ice.medium.alkalinity = 2300.0f; // agua de mar
    setWeights(ice, 0.0f, {{S::Navicula, 4.0f}, {S::Chlamydomonas, 0.6f}, {S::Euplotes, 2.0f}, {S::Candida, 0.5f}});
    ice.bacteria = 20'000;
    ice.cyanoFilaments = 0;
    ice.oscillatoriaFilaments = 0;
    ice.saprolegnia = 0;
    ice.allomyces = ice.lichens = ice.physarum = 0;
    ice.chytridClusters = 3; // quitridios marinos de diatomeas
    ice.patchiness = 0.3f;
    // Canales de salmuera: poco sedimento, frústulas de diatomeas del hielo.
    ice.sediment = {.clay = 30, .quartz = 8, .frustules = 25};
    ice.medium.dustRate = 0.005f;
    return all;
}

const std::array<Biome, kBiomeCount> kBiomes = makeBiomes();

} // namespace

const Biome& biome(BiomeId id) { return kBiomes[static_cast<int>(id)]; }

float dropAreaScale(const WorldConfig& c) {
    constexpr float kReferenceArea = 2048.0f * 1024.0f; // [µm²]
    const float w = static_cast<float>(c.gridX) * c.cellSize, h = static_cast<float>(c.gridY) * c.cellSize;
    if (!c.dish)
        return w * h / kReferenceArea;
    const float r = 0.5f * std::min(w, h) - c.dishMargin;
    return 3.14159265f * r * r / kReferenceArea;
}

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
    // Micronutrientes: otro flujo de azar, así no cambia el resto de la gota de una seed.
    Rng micro(seed, rng_stream::kPopulate + 902, 0, 0);
    c.medium.silicate *= micro.uniform(0.6f, 1.6f);
    c.medium.iron *= micro.uniform(0.5f, 1.8f);
    c.medium.vitamin *= micro.uniform(0.5f, 1.8f);
    for (int k = 0; k < kSpeciesCount; ++k) {
        Rng r(seed, rng_stream::kPopulate + 901, static_cast<std::uint64_t>(k), 0);
        const bool absent = r.uniform() < 0.15f; // algunas especies no llegaron a esta gota
        c.speciesWeight[k] = absent ? 0.0f : b.weight[k] * r.uniform(0.5f, 1.5f);
    }
    c.bacteria = static_cast<std::size_t>(static_cast<float>(b.bacteria) * rng.uniform(0.7f, 1.3f));
    c.detritus = static_cast<std::size_t>(static_cast<float>(b.detritus) * rng.uniform(0.6f, 1.5f));
    c.cyanoFilaments = static_cast<int>(static_cast<float>(b.cyanoFilaments) * rng.uniform(0.5f, 1.5f));
    c.oscillatoriaFilaments = static_cast<int>(static_cast<float>(b.oscillatoriaFilaments) * rng.uniform(0.5f, 1.5f));
    c.cyanoTMin = b.cyanoTMin;
    c.cyanoTMax = b.cyanoTMax;
    c.saprolegnia = b.saprolegnia;
    c.chytridClusters = b.chytridClusters;
    c.allomyces = b.allomyces;
    c.lichens = b.lichens;
    c.physarum = b.physarum;
    c.patchiness = std::clamp(b.patchiness * rng.uniform(0.5f, 1.5f), 0.0f, 1.0f);
    c.sediment = b.sediment;
    const float muddy = rng.uniform(0.5f, 1.6f); // algunas gotas son más barrosas que otras
    // Misma densidad en cualquier tamaño de placa.
    const float area = dropAreaScale(c);
    auto scaled = [&](float n) { return std::max(0.0f, std::round(n * area)); };
    c.bacteria = static_cast<std::size_t>(scaled(static_cast<float>(c.bacteria)));
    c.detritus = static_cast<std::size_t>(scaled(static_cast<float>(c.detritus)));
    c.cyanoFilaments = static_cast<int>(scaled(static_cast<float>(c.cyanoFilaments)));
    c.oscillatoriaFilaments = static_cast<int>(scaled(static_cast<float>(c.oscillatoriaFilaments)));
    c.saprolegnia = static_cast<int>(scaled(static_cast<float>(c.saprolegnia)));
    c.chytridClusters = static_cast<int>(scaled(static_cast<float>(c.chytridClusters)));
    c.allomyces = static_cast<int>(scaled(static_cast<float>(c.allomyces)));
    c.lichens = static_cast<int>(scaled(static_cast<float>(c.lichens)));
    c.physarum = static_cast<int>(scaled(static_cast<float>(c.physarum)));
    for (int* n : {&c.sediment.clay, &c.sediment.quartz, &c.sediment.mica, &c.sediment.ironOxide, &c.sediment.sulfur,
                   &c.sediment.frustules})
        *n = static_cast<int>(scaled(static_cast<float>(*n) * muddy));
    c.sediment.rocks = static_cast<int>(scaled(static_cast<float>(c.sediment.rocks)));
    return c;
}

void applyDropContent(WorldConfig& c, DropContent content) {
    if (content == DropContent::Full)
        return;
    c.bacteria = 0;
    c.cyanoFilaments = 0;
    c.oscillatoriaFilaments = 0;
    c.saprolegnia = c.chytridClusters = 0;
    c.physarum = c.allomyces = c.lichens = 0;
    c.medium.phage = 0.0f;
    if (content != DropContent::EmptyPetri)
        return;
    // Un plato de Petri limpio: nada inorgánico ni muerto flotando en el agua.
    c.sediment = {};
    c.medium.dustRate = 0.0f;
    c.detritus = 0;
    c.patchiness = 0.0f; // ni manchas de materia orgánica ni los detritos que las acompañan
    c.medium.detritusSpawnRate = 0.0f;
}
