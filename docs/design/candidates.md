# Candidates: beings and experiments

Two lists to choose what to add next: **unicellular beings** (and single cells of multicellular
organisms) and **classic experiments** the simulator could reproduce as a lesson or a validation
test. This is not a plan: the [roadmap](../roadmap.md) sets the order of work. Beings already in the
catalogue are documented one by one in [`docs/content/beings/`](../content/beings/README.md).

**Inclusion criteria** (those of `AGENTS.md`): real beings and mechanisms, visible with a light
microscope and with published literature. Complete multicellular animals (rotifers, nematodes,
*Hydra*, *Daphnia*) and anything speculative are out.

**About the numbers:** sizes are orders of magnitude for planning, not data. References are the
classic ones for each experiment. Before loading a value into `assets/content/base/`, read the
original source and record it there, like the rest of the physiology.

**Legend**

- **Reuses:** what the engine already has (`LocomotionType`, `FeedingMode`, `SpeciesInfo` flags:
  `glider`, `frustule`, `filament`, `colonyDaughters`, `sessile`, `contractileStalk`,
  `tentacleReach`, `neckReach`, `walker`, `budding`, `encysts`, `conjugates`, `phototaxis`,
  `acidTrap`; `Bacteria` strains, cyanobacteria with heterocysts, fungi in `bio/fungi`, plasmodium,
  animal cells, tissues).
- **New:** the missing mechanism. "—" = it can be loaded with data alone.
- **Priority:** **A** (high value or cheap), **M** and **B**.
- **Being status:** ✅ already in the catalogue · ⏸ researched but blocked (reason given).
- **Experiment status:** ✅ done · 🟢 feasible with what exists (the lesson or test is missing) ·
  🟡 little missing (named) · 🔴 a large mechanism is missing · ⛔ out of scope (and why).
- **Setup:** *plate* (drop), *culture* (well-mixed, `game/culture`), *slide* or *cell* (cell culture
  on a dish).

Existing biomes: puddle, pond, moss, termite gut, hot spring and sea ice. When a being needs another
one, it is noted (salt pan, peat bog, open sea, soil).

## Researched data that is not enough yet

- ***Gonium*:** Saito (1984, Sci. Pap. Inst. Algol. Res. Hokkaido Univ. 7: 195) measured 5.8
  doublings/day at 25 °C, but in *G. multicoccum* and in an organic medium: not used for
  *G. pectorale*.
- ***Chilomonas paramecium*:** no data with an accessible source (the Michigan State thesis with its
  growth on acetate has restricted access).
- ***Bodo saltans*:** only grazing data (eats bacteria smaller than 3 µm); size and µmax missing.
- ***Schizosaccharomyces pombe*:** 108 min generation time (Kovar, Wu & Pollard 2005, Mol. Biol.
  Cell 16: 2313), temperature not given; diameter 3.74 µm and length at division ~14–16 µm (Pombe
  Pages by Forsburg; 15.8 µm at 32 °C in a Genetics 2012 paper). Temperature to be confirmed.
- **Researched, not loaded:** *Spirostomum ambiguum* (884 × 89 µm extended, 306 × 117 contracted;
  PMC11581044: too long for the current division; it was added and removed), *Bursaria
  truncatella* (300–600 µm, up to 1.7 mm), *Euglena ehrenbergii* (140–296 × 15–26 µm: very long),
  *E. sanguinea* (90–150 µm), *Opercularia nutans* (50–110 × 25–40 µm, colonial), *Pelomyxa
  palustris* (up to 5 mm: larger than the plate). Marine species with NCMA data for a future marine
  biome.
- **Need code, not just data:** new bacteria, cyanobacteria and archaea (strains live in
  `bio/bacteria` and `bio/cyanobacteria`), fungi with hyphae (`FungusInfo`) and everything marked
  "New" in the tables.
- **Speeds:** *Tetrahymena* swims at ~500 µm/s (helical swimming study; 200–350 µm/s in others).
  No speed was found for *Colpidium* or *Pinnularia*.

---

## Part 1 — Beings

### 1.1 Ciliates

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Tetrahymena thermophila* / *T. pyriformis* | ~50 µm | puddle, pond | Ciliate prey of *Amoeba* (needed for its µmax, roadmap B). Model organism with many published growth curves | Cilia, OralGroove, conjugates | — | **A** |
| ✅ *Tetrahymena vorax* | 60–200 µm | puddle | Polymorphism: the microstome form eats bacteria, the macrostome other ciliates (prey-induced) | Cilia, OralGroove, Raptorial | Induced change of feeding mode | B |
| ✅ *Colpidium campylum* / *C. striatum* | 50–80 µm | pond | The bacterivore of classic microcosms (Lawler and Morin, Holyoak and Lawler). Prey of *Didinium* | Cilia, OralGroove | — | **A** |
| ✅ *Colpoda cucullus* / *C. steinii* | 15–80 µm | moss, soil | The moss ciliate: encysts and excysts with desiccation. Divides inside a reproductive cyst | Cilia, OralGroove, encysts | Division cyst (2–4 daughters) | M |
| ✅ *Blepharisma japonicum* | 150–300 µm | puddle | Pink pigment (blepharismin) that makes it photophobic and photosensitive. Cannibal giants | Cilia, OralGroove | Photophobia and light damage; cannibalism | M |
| *Spirostomum ambiguum* | 1–3 mm | pond | One of the longest unicellular eukaryotes. Contracts its whole body in milliseconds (myonemes, Ca²⁺) | Cilia, OralGroove | Whole-body contraction (exists); division of a ~1 mm cell | M |
| ✅ *Coleps hirtus* | 40–65 µm | pond | Carbonate plates in the pellicle. Histophagous: eats injured or dead cells in groups | Cilia, Raptorial | Attraction to damaged cells | M |
| ✅ *Dileptus* | 250–500 µm | pond | Proboscis with toxicysts that paralyse the prey | Cilia, Raptorial | Toxicysts (immobilised prey) | M |
| *Loxodes* | 100–200 µm | microoxic sediment | Müller bodies (gravireceptors). Avoids high O₂ and respires nitrate (Fenchel and Finlay) | Cilia, OralGroove | O₂-modulated gravitaxis (see ⛔ vertical axis) | B |
| ✅ *Halteria grandinella* | 20–50 µm | pond | Swims in jumps with cirri | Cilia, OralGroove | Jumps | B |
| ✅ *Stylonychia mytilus* / *Oxytricha* | 100–300 µm | pond | Walking hypotrichs, like *Euplotes* | Cilia, walker, OralGroove | — | M |
| ✅ *Frontonia leucas* | 150–600 µm | pond | Eats whole diatoms and algal filaments | Cilia, Raptorial | — | M |
| *Bursaria truncatella* | 0.5–1 mm | pond | Huge ciliate that hunts other ciliates | Cilia, Raptorial | — | B |
| ✅ *Paramecium bursaria* | 80–150 µm | pond | Endosymbiosis with *Chlorella* (roadmap F). Coexistence with *P. caudatum* (Gause) | Cilia, OralGroove, conjugates, phototaxis | Heritable photosynthetic endosymbiont | **A** |
| ✅ *Paramecium tetraurelia* | 100–150 µm | pond | Genetic model. *Killer* trait (*Caedibacter* bacteria) | Cilia, OralGroove, conjugates | Endosymbiont toxic to sensitives | B |
| ✅ *Carchesium* / *Zoothamnium* | mm colonies | pond | Colonial peritrichs: shared spasmoneme (*Zoothamnium*) or one per zooid (*Carchesium*) | sessile, contractileStalk, colonyDaughters | Branched colony on a stalk | B |
| ✅ *Epistylis* | mm colonies | pond | Colonial peritrich with a rigid stalk (contrast with *Vorticella*) | sessile, colonyDaughters | Branched colony | B |
| ✅ *Tokophrya* / *Acineta* | 50–150 µm | pond | Other suctorians (compare with *Podophrya*) | sessile, Tentacles | — | B |
| ✅ *Litonotus* | 100–200 µm | pond | Flattened predator of hypotrichs | Cilia, Raptorial | — | B |
| ✅ *Lembadion* | 50–200 µm | pond | Its chemical signal makes *Euplotes* grow defensive "wings" (Kuhlmann and Heckmann 1985) | Cilia, Raptorial | Kairomone and induced defence | M |
| ✅ *Chilodonella* | 50–100 µm | pond | Scrapes the biofilm with a cytopharyngeal basket | Cilia, walker | Biofilm grazing | B |
| ✅ *Mesodinium rubrum* | 20–50 µm | sea | Kleptoplasty: retains chloroplasts and nucleus of cryptophytes (roadmap F). Very fast jumps | Cilia | Kleptoplasts with a limited life | M |
| *Tintinnopsis* (tintinnids) | 50–200 µm | sea | Agglutinated lorica, filter feeders of the marine plankton | Cilia, OralGroove, armour | — | B |

### 1.2 Flagellates and euglenoids

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Peranema trichophorum* | 20–70 µm | puddle | Phagotrophic euglenoid: swallows *Euglena* and yeasts with an ingestion rod. Glides with its rigid flagellum | Flagellum, Raptorial | Flagellar gliding | **A** |
| ✅ *Phacus* | 30–100 µm | puddle | Rigid, flattened euglenoid with large paramylon grains | Flagellum, phototaxis | — | **A** |
| ✅ *Trachelomonas* | 15–50 µm | puddle | Lorica stained by iron and manganese (brown) | Flagellum, phototaxis, armour | Lorica that consumes Fe | M |
| *Euglena viridis* / *E. spirogyra* | 40–150 µm | puddle | Other euglenas (metaboly, striations) | Flagellum, phototaxis | — | B |
| *Bodo saltans* | 5–10 µm | pond | Bacterivorous kinetoplastid, relative of trypanosomes | Flagellum, Anterior | — | M |
| ✅ *Salpingoeca rosetta* / *Monosiga* | 5–10 µm | pond, sea | Choanoflagellates: a collar filters bacteria. Rosettes are induced by a bacterium (Alegado et al. 2012) | Flagellum | Filtering collar; induced colony (pending) | M |
| *Dinobryon* | 30–50 µm per cell | oligotrophic pond | Arborescent colony in loricas. Mixotroph: photosynthesises and eats bacteria | Flagellum, phototaxis, colonyDaughters | Mixotrophy (exists) | M |
| *Synura* | 100–400 µm colonies | cold pond | Spherical colony with siliceous scales | Flagellum, colonyDaughters | Scales that consume Si | B |
| ✅ *Ochromonas* | 5–15 µm | pond | Mixotroph, predator of *Chlorella* (Boraas experiment) | Flagellum, Phagocytosis, phototaxis | Mixotrophy | M |
| ✅ *Cryptomonas* | 15–50 µm | pond | Cryptophyte with phycoerythrin. Prey of *Mesodinium* | Flagellum, phototaxis | — | M |
| *Chilomonas paramecium* | 20–40 µm | puddle with infusion | Colourless osmotrophic cryptophyte, a classic of laboratory cultures | Flagellum, Osmotroph | — | M |
| ✅ *Haematococcus pluvialis* | 10–50 µm | temporary puddle | Under stress accumulates astaxanthin and forms red cysts | Flagellum, phototaxis, encysts | Stress pigment | M |
| *Dunaliella salina* | ~10 µm | salt pan | Wall-less. Tolerates extreme salinity with glycerol | Flagellum, phototaxis | Osmotic adjustment by glycerol; salt-pan biome | M |
| ✅ *Gonium* / *Pandorina* / *Eudorina* | 4–32 cells | pond | Volvocine series of increasing complexity up to *Volvox* | Flagellum, phototaxis, colonyDaughters | — | **A** |

### 1.3 Dinoflagellates

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Peridinium* | 30–70 µm | pond | Cellulose theca and two flagella, one in the cingulum: spiral swimming | Flagellum, phototaxis, armour | Helical swimming | M |
| ✅ *Ceratium hirundinella* | 100–400 µm | lake | Long horns. Migrates through the water column | Flagellum, phototaxis | — | B |
| *Gymnodinium* | 10–80 µm | pond, sea | Athecate; some species cause red tides | Flagellum, phototaxis | — | B |
| *Noctiluca scintillans* | 0.2–2 mm | sea | Bioluminescence when the water is sheared (mechanical stimulus) | Phagocytosis | Bioluminescence (exists), buoyancy | B |
| ✅ *Lingulodinium polyedra* (*Gonyaulax*) | 30–50 µm | sea | Circadian rhythm of bioluminescence (Hastings and Sweeney) | Flagellum, phototaxis | Circadian clock | B |
| *Oxyrrhis marina* | 10–40 µm | sea | Model heterotroph, predator of algae | Flagellum, Phagocytosis | — | B |
| ✅ *Polarella glacialis* | ~10 µm | sea ice | Real inhabitant of sea ice (existing biome) | Flagellum, phototaxis | — | M |

### 1.4 Amoebae, heliozoans and relatives

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Acanthamoeba castellanii* | 15–35 µm | soil, water | Phagocytosis model. Eats bacteria and forms very resistant cysts | Pseudopod, Phagocytosis, encysts | — | **A** |
| ✅ *Dictyostelium discoideum* | ~10 µm | soil, moss | cAMP aggregation in spiral waves. Simulated up to the aggregation stream (slug and fruiting body are multicellular) | Pseudopod, Phagocytosis | cAMP chemotaxis with relay; cAMP field | **A** |
| ✅ *Vampyrella* | 30–60 µm | pond | Perforates the wall of *Spirogyra* and sucks out the protoplast | Pseudopod | Wall perforation | M |
| *Difflugia* | 100–300 µm | pond, moss | Test made of sand grains taken from the sediment | Pseudopod, armour | Test from the minerals in `minerals/` | M |
| ✅ *Euglypha* / *Nebela* / *Assulina* / *Hyalosphenia* | 30–150 µm | moss, peat bog | Testate amoebae, moisture indicators (palaeoecology) | Pseudopod, armour | Test of siliceous plates (consumes Si) | M |
| ✅ *Actinosphaerium* / *Actinophrys sol* | 40 µm–1 mm | pond | Heliozoans: axopodia trap ciliates on contact. They float | Phagocytosis | Axopodia; fusion when eating large prey | M |
| ✅ *Chaos carolinense* | 1–5 mm | pond | Giant multinucleate amoeba | Pseudopod, Phagocytosis | — | B |
| *Pelomyxa palustris* | 0.5–3 mm | anoxic sediment | No mitochondria; endosymbiotic bacteria | Pseudopod, Phagocytosis | Metabolic endosymbionts | B |
| *Mayorella* / *Thecamoeba* | 50–200 µm | pond | Other amoeboid morphologies (subpseudopodia, wrinkled surface) | Pseudopod, Phagocytosis | — | B |
| *Entamoeba histolytica* | 15–60 µm | human gut | Parasite that phagocytoses erythrocytes, visible in a preparation | Pseudopod, Phagocytosis | Prey: erythrocytes (`bio/erythrocytes`) | M |
| *Allogromia* (foraminifer) | 0.2–1 mm | sea | Reticulopodia with bidirectional flow | — | Reticulopodia | B |

### 1.5 Green algae

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Chlorella vulgaris* | 2–10 µm | pond | Growth model alga. Prey of *Ochromonas*, symbiont of *P. bursaria* | None | Autospores (4–16 daughters per division) | **A** |
| ✅ *Desmodesmus* / *Scenedesmus* | 4–8-cell coenobia | pond | Forms coenobia. More colonial when grazed | colonyDaughters | — | **A** |
| ✅ *Raphidocelis subcapitata* | 8–14 µm | pond | Standard toxicity bioassay alga (OECD 201) | None | — | **A** |
| ✅ *Closterium* | 100–500 µm | pond | Crescent desmid with gypsum crystals in terminal vacuoles. Glides by secreting mucilage | glider | — | M |
| ✅ *Cosmarium* / *Staurastrum* | 30–100 µm | pond, peat bog | Desmids with two semicells | None | — | M |
| ✅ *Micrasterias* | 100–300 µm | acid peat bog | Semicell morphogenesis after division | None | Morphogenesis (B) | B |
| *Cladophora* | branched filaments | pond | The alga of Engelmann's experiment | filament | Branching | M |
| ✅ *Ulothrix* | filaments | stream | Simple filament with a band-shaped chloroplast | filament | — | B |
| *Botryococcus braunii* | colonies | pond | Oily colony (hydrocarbons) | colonyDaughters | — | B |
| *Chlamydomonas nivalis* / *Sanguina* | 20–40 µm | snow, ice | Red snow: cysts with carotenoids | Flagellum, encysts | Stress pigment (exists) | M |
| *Acetabularia* | 2–5 cm | sea | Giant uninucleate cell; Hämmerling's grafts | — | Nucleus-regulated morphogenesis | B |

### 1.6 Diatoms

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Pinnularia* | 50–200 µm | pond | Large diatom with a clearly visible raphe: the classroom gliding diatom | glider, frustule | — | **A** |
| *Nitzschia* | 20–150 µm | pond, sea | Fast gliding; *N. palea* indicates organic pollution | glider, frustule | — | M |
| ✅ *Bacillaria paxillifer* | colonies | estuary | Colony that glides like an extending ruler | glider, frustule, colonyDaughters | Coordinated gliding between cells | M |
| *Fragilaria crotonensis* | ribbons | lake | Ribbon colonies. Competes for Si | frustule, colonyDaughters | — | M |
| *Melosira* / *Aulacoseira* | chains | lake | Centric diatoms in chains, heavy (they sink) | filament, frustule | — | M |
| *Ulnaria* (*Synedra*) | 100–400 µm | pond | Needle diatom | glider, frustule | — | B |
| *Tabellaria* | zigzag colonies | lake | Another colonial form | frustule, colonyDaughters | — | B |
| ✅ *Gomphonema* | 20–80 µm | stream | Attaches with a mucilaginous stalk | sessile, frustule | — | B |
| ✅ *Phaeodactylum tricornutum* | 10–30 µm | sea | Genomic model. Polymorphic (oval, fusiform, triradiate) | frustule | — | M |
| ✅ *Thalassiosira pseudonana* | 4–6 µm | sea | Model centric diatom for Si uptake | frustule | — | M |
| ✅ *Fragilariopsis cylindrus* | 3–80 µm | sea ice | Dominant diatom of sea ice (existing biome) | glider, frustule | — | M |

### 1.7 Other photosynthetic eukaryotes

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| *Emiliania huxleyi* | ~5 µm | sea | Coccolithophore: calcite plates. Infected by its own virus | None | Calcification (inorganic C) | B |
| *Galdieria sulphuraria* / *Cyanidioschyzon* | 2–15 µm | acid hot spring | Red algae at pH ~1–3 and 40–56 °C | None | Acid hot-spring biome | M |

### 1.8 Cyanobacteria

There are already generic chain cyanobacteria with heterocysts and akinetes (`bio/cyanobacteria`).
These would be named species with their data.

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| *Microcystis aeruginosa* | colonies of 3–6 µm cells | eutrophic lake | Toxic blooms (microcystin exists). Floats with gas vesicles | cyanobacteria, toxin | Mucilaginous colonies; buoyancy | **A** |
| *Dolichospermum* (*Anabaena*) | filaments | pond | Heterocysts and akinetes (resting spores) | cyanobacteria (heterocysts, akinetes) | Named species | **A** |
| *Oscillatoria* / *Phormidium* | filaments | pond, hot spring | Filaments that glide and oscillate | cyanobacteria | Filament gliding | M |
| *Nostoc* | gelatinous colonies | moss, soil | Desiccation-resistant colonies, heterocysts | cyanobacteria | Gelatinous matrix | M |
| *Synechococcus elongatus* | 1–3 µm | fresh water | Three-protein circadian clock (KaiABC; Kondo et al. 1993; Nakajima et al. 2005) | particles | Circadian clock | M |
| *Synechocystis* PCC 6803 | 1–3 µm | fresh water | Phototaxis by pili (twitching) | particles | Twitching | B |
| Thermophilic *Synechococcus* / *Mastigocladus* | — | hot spring | Hot-spring mats | cyanobacteria | — | M |
| *Arthrospira* (*Spirulina*) | spirals | alkaline lake | Helical filament | cyanobacteria | Alkaline biome | B |
| *Merismopedia* / *Gloeocapsa* | colonies | pond | Flat plate colony; sheathed colony | cyanobacteria | — | B |
| *Prochlorococcus* | ~0.6 µm | sea | The most abundant phototroph in the ocean | particles | — | B |

### 1.9 Bacteria

Today they are particles with a strain (`Bacteria::strain`: a generic gram-negative rod,
*C. violaceum*, *B. subtilis* and *Spirillum volutans*).

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| *Escherichia coli* | ~2 µm | gut, laboratory | The model bacterium: run and tumble (Berg), diauxie, chemostat, Luria–Delbrück, Lenski | Bacteria, chemotaxis, phages | Named strain with data | **A** |
| *Staphylococcus aureus* | ~1 µm, in clusters | skin | Gram-positive in clusters. Classic antibiogram | Bacteria, Gram | Clustering | M |
| *Streptococcus pneumoniae* | ~1 µm, diplococci | airways | Capsule (India ink, roadmap C). Griffith | Bacteria, Gram | Capsule | M |
| *Vibrio fischeri* | ~2 µm | sea | Quorum sensing with bioluminescence (Nealson et al. 1970) | Bacteria, quorum | Bioluminescence | M |
| *Pseudomonas fluorescens* | ~2 µm | soil, water | Adaptive radiation in a static microcosm (Rainey and Travisano 1998) | Bacteria, biofilm | Morphotypes (*wrinkly spreader*) | M |
| *Bacillus subtilis* (extend) | ~3 µm | soil | Sporulation and heat resistance | Bacteria (strain 2) | Spores | M |
| ✅ *Spirillum volutans* | 15–60 µm | pond | Giant bacterium, visible in detail. Forms aerotactic bands | Bacteria | Aerotaxis | M |
| *Lactobacillus* | 2–5 µm | milk, gut | Lactic fermentation; lowers pH | Bacteria, Gram | — | M |
| *Serratia marcescens* | ~1.5 µm | environment | Red pigment (prodigiosin) | Bacteria | — | B |
| *Caulobacter crescentus* | 1–3 µm | fresh water | Asymmetric division: stalked cell and swimming daughter | Bacteria | Cycle with stalk | B |
| *Myxococcus xanthus* | 5–7 µm | soil | Social gliding and group predation | Bacteria | Social gliding | B |
| *Magnetospirillum* | 1–3 µm | sediment | Magnetotaxis (Blakemore 1975) | Bacteria | Magnetic field | B |
| *Beggiatoa* | filaments | sulfidic sediment | O₂ and H₂S gradients, sulfur granules | cyanobacteria (filament) | S cycle | B |
| *Rhodospirillum rubrum* | 1–2 µm | pond | Phototrophic purple bacterium; photophobotaxis (Engelmann) | Bacteria | Bacterial phototaxis | B |
| *Proteus mirabilis* | 1–3 µm | gut | Swarming on surfaces | Bacteria | Swarming | B |
| *Thermus aquaticus* | 3–10 µm | hot spring | The Taq bacterium (Brock 1969) | Bacteria | — | M |
| Spirochaetes (*Treponema*) | 5–20 µm | termite gut | Symbionts that move *Mixotricha* | Bacteria | Attachment to another being | B |

### 1.10 Archaea

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| *Sulfolobus acidocaldarius* | ~1 µm | acid hot spring | Thermoacidophilic archaeon | Bacteria | Extreme temperature and pH range | M |
| *Halobacterium salinarum* | 2–5 µm | salt pan | Purple membrane (bacteriorhodopsin, Oesterhelt and Stoeckenius 1971), phototaxis | Bacteria | Salt-pan biome | B |
| *Haloquadratum walsbyi* | square, ~2 µm | salt pan | Square, flat cells | Bacteria | — | B |
| *Methanobrevibacter* | ~1 µm | termite gut | Methanogen of the termite gut | Bacteria | Methanogenesis (H₂ → CH₄) | B |

### 1.11 Yeasts and fungi

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ *Saccharomyces cerevisiae* | 5–10 µm | fruit, laboratory | The model yeast: budding, fermentation and Crabtree effect. Gause 1932 | Osmotroph, budding | Ethanol as a field | **A** |
| *Schizosaccharomyces pombe* | 3 × 8–14 µm | fermented food | Medial binary fission; size control (Nurse 1975) | Osmotroph | Division by medial fission | M |
| *Rhodotorula* | 3–8 µm | environment | Pink yeast (carotenoids) | Osmotroph, budding | — | B |
| *Rhizophydium* / *Zygorhizidium* on *Asterionella* | — | lake | Named chytrids on their host (Canter and Lund) | `ChytridInfection` | — | M |
| *Penicillium* | hyphae | environment | Fleming's experiment (1929): inhibition halo | hyphae in `bio/fungi` | Secreted antibiotic | M |

### 1.12 Parasites and symbionts

| Being | Size | Where | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| *Plasmodium falciparum* | 1–2 µm rings in erythrocytes | blood smear | Malaria diagnosis with Giemsa | smear, Giemsa | Intracellular parasitism of the erythrocyte | M |
| *Trypanosoma brucei* | 15–30 µm | blood smear | Trypomastigote among the erythrocytes | Flagellum | — | M |
| *Giardia lamblia* | 10–20 µm | gut | Trophozoite with an adhesive disc and two nuclei | Flagellum | Disc adhesion | B |
| *Trichomonas vaginalis* | 10–20 µm | mucosa | Flagellate with an undulating membrane | Flagellum | — | B |
| *Toxoplasma gondii* | 4–8 µm | cell culture | Tachyzoites inside fibroblasts | animal cells | Intracellular parasitism | B |
| *Mixotricha paradoxa* | ~500 µm | termite gut (*Mastotermes*) | Moved by spirochaetes attached to its surface | termite gut | Locomotion by symbionts | B |
| *Pyrsonympha* / *Streblomastix* / *Joenia* | 50–300 µm | termite gut | Other termite-gut flagellates | Flagellum | — | B |
| *Holospora* | ~1–15 µm | inside *Paramecium* | Bacterium that infects the *Paramecium* nucleus | — | Nuclear infection | B |

### 1.13 Cells of multicellular organisms (cell culture and slides)

| Cell | Size | Setup | Why it matters | Reuses | New | Prio. |
|---|---|---|---|---|---|---|
| ✅ Human keratinocyte (HaCaT or primary) | 10–20 µm | cell | Epithelial islets (roadmap F) | Crawl, cell cycle | Cell–cell adhesion | **A** |
| ✅ Murine fibroblast NIH 3T3 | 20–50 µm | cell | The contact-inhibition line (Todaro and Green 1963) | Crawl, cell cycle | — | **A** |
| MDCK | 10–20 µm | cell | Monolayer epithelium | Crawl, cell cycle | Cell–cell adhesion | M |
| Macrophage (RAW 264.7, or differentiated monocyte) | 10–20 µm | cell | Phagocytosis of bacteria and apoptotic bodies | Crawl, Phagocytosis | Monocyte differentiation | M |
| Fish keratocyte | 20–40 µm | cell | Fast fan-shaped migration; motility model | Crawl | — | M |
| ✅ Human spermatozoon | 50 µm with tail | slide | Flagellar swimming and semen analysis (motility) | Flagellum | Swimming with a posterior flagellum | M |
| Eosinophil, basophil, platelets | 2–15 µm | smear | Complete leukogram with Giemsa | smear | — | M |
| Bird or amphibian erythrocyte | 10–20 µm | smear | Nucleated erythrocyte, for comparison | `bio/erythrocytes` | Nucleus | B |
| *Tradescantia* stamen hair | ~100 µm cells | slide | Cyclosis and live mitosis | tissue | Cyclosis | M |
| Pollen grain | 20–100 µm | slide | Pollen-tube germination in sucrose | tissue | Tip growth | B |
| *Chara* / *Nitella* internodal cell | cm | slide | The fastest cyclosis (Kamiya) | tissue | Cyclosis | B |
| Root hair | — | slide | Tip growth | tissue | Tip growth | B |
| HepG2 | 10–20 µm | cell | Liver line | Crawl, cell cycle | — | B |

---

## Part 2 — Experiments

### 2.1 Populations and communities

| # | Experiment | Reference | Beings | Setup | Measured | Expected result | Status | Use |
|---|---|---|---|---|---|---|---|---|
| 1 | Logistic growth | Gause 1934 | *P. caudatum*, *P. aurelia* | culture | N(t), r, K | Sigmoid curve with plateau | ✅ | lesson, validation |
| 2 | Competitive exclusion | Gause 1934 | *P. aurelia* vs *P. caudatum* | culture | Abundances | *P. aurelia* wins | 🟡 today whichever gets ahead wins (roadmap B) | validation |
| 3 | Coexistence by use of space | Gause 1934 | *P. caudatum* and *P. bursaria* | culture, plate | Abundances | They coexist (*P. bursaria* feeds at the bottom) | 🔴 spatial niche | lesson |
| 4 | Predator without refuge | Gause 1934 | *Didinium*, *Paramecium* | culture | N(t) of both | The predator exterminates the prey, then dies out | ✅ | lesson |
| 5 | With refuge (sediment) | Gause 1934 | *Didinium*, *Paramecium* | plate | N(t) | The prey persists in the refuge | 🟡 refuge *Didinium* cannot reach | lesson |
| 6 | With immigration | Gause 1934 | *Didinium*, *Paramecium* | culture | N(t) | Oscillations | 🟡 programmed immigration (roadmap B) | validation |
| 7 | Methyl cellulose | Luckinbill 1973 | *Didinium*, *Paramecium* | culture | Cycles | Fewer encounters: longer, more persistent cycles | 🟡 medium viscosity as a parameter | lesson |
| 8 | Long oscillations | Veilleux 1979 | *Didinium*, *Paramecium* | culture | Period and amplitude | Several coupled cycles | 🟡 | validation |
| 9 | Metapopulations | Holyoak and Lawler 1996 | *Didinium*, *Colpidium* | several cultures | Persistence | Subdivision prolongs persistence | 🔴 connected cultures | research |
| 10 | Paradox of enrichment | Rosenzweig 1971; Bohannan and Lenski 1997 | *E. coli*, phage T4 | culture | Amplitude | More nutrient gives larger oscillations | 🟡 named *E. coli* strain | lesson, validation |
| 11 | Type II functional response | Holling 1959; Fenchel 1980 | *Paramecium*, bacteria | culture, plate | Ingestion vs prey density | Hyperbolic saturation | 🟢 (`microcosm_bench --respuesta-funcional`; roadmap B) | validation |
| 12 | Productivity and food-chain length | Kaunzinger and Morin 1998 | bacteria, *Colpidium*, *Didinium* | culture | Persisting levels | More nutrient supports longer chains | 🟢 | research |
| 13 | Food-web architecture | Lawler and Morin 1993 | ciliates and bacteria | culture | Population variability | Changes with omnivory and number of prey | 🟡 more ciliates | research |
| 14 | Warming in microcosms | Petchey et al. 1999 | protist community | culture, plate | Richness per trophic level | Warming loses more predators and herbivores | 🟢 with temperature programs | research |
| 15 | Allometry of growth | Fenchel 1974 | protists of several sizes | culture | µmax vs mass | Power-law decrease with mass | 🟡 sourced µmax for every species (roadmap B) | validation |
| 16 | Yeast competition | Gause 1932 | *S. cerevisiae*, *Schizosaccharomyces kephir* | culture | Biomass and alcohol | The ethanol they produce limits both | 🔴 ethanol as a field | lesson |
| 17 | Rock–paper–scissors | Kerr et al. 2002 | *E. coli*: colicin producer, sensitive and resistant | plate | Coexistence | They coexist in local space; mixed, they do not | 🔴 colicins | research |
| 18 | Cooperators and cheaters | Gore et al. 2009 | *S. cerevisiae* SUC2⁺ and SUC2⁻ | culture | Frequencies | Coexistence (snowdrift game) | 🔴 extracellular invertase | research |
| 19 | Predation-induced colonies | Boraas et al. 1998 | *Chlorella*, *Ochromonas* | culture | Colony size | Colonies the predator cannot swallow appear | 🔴 colonial gene in *Chlorella* | lesson |
| 20 | Induced defence | Kuhlmann and Heckmann 1985 | *Euplotes*, *Lembadion* | plate | Shape and survival | *Euplotes* with "wings", less predated | 🟢 mechanism done; lesson missing | lesson |
| 21 | Bloom and eutrophication | scenario (roadmap J) | *Microcystis*, algae | plate, culture | Biomass, O₂, toxin | A P pulse causes a bloom and night anoxia | 🟡 *Microcystis* | lesson |
| 22 | Day–night cycle of O₂ and pH | classic practical | algae | plate | O₂ and pH | Up by day, down by night | 🟢 light programs | lesson |

### 2.2 Resources and growth

| # | Experiment | Reference | Beings | Setup | Measured | Expected result | Status | Use |
|---|---|---|---|---|---|---|---|---|
| 23 | Monod curve | Monod 1949 | *Asterionella* | culture | µ vs S | Hyperbola | ✅ | validation |
| 24 | Chemostat | Novick and Szilard 1950; Monod 1950 | bacteria | culture | S* and N* vs D | S* independent of supply; washout if D > µmax | ✅ | lesson, validation |
| 25 | Cell quota | Droop 1968 | *Asterionella* | culture | µ vs quota | µ = µ′(1 − Q₀/Q) | ✅ (P); quota panel missing (roadmap G) | validation |
| 26 | P and Si competition | Tilman 1977 | *Asterionella*, *Cyclotella* | culture | Winner vs Si:P | Changes with the ratio | ✅ | lesson, validation |
| 27 | Diauxie | Monod 1942 | *E. coli* | culture | Optical density | Two exponential phases with a lag | 🔴 two sugars and catabolite repression | lesson |
| 28 | Growth-curve phases | Buchanan 1918; Monod 1949 | bacteria | culture | N(t) | Lag, exponential, stationary and death | 🟢 (`fitGrowth`) | lesson |
| 29 | Temperature and growth | Ratkowsky et al. 1982 | bacteria, protists | culture | µ vs T | √µ linear below the optimum; drops above | 🟡 fixed Q10 = 2 today, no optimum or maximum | validation |
| 30 | Photosynthesis–irradiance curve | Jassby and Platt 1976 | algae | culture | Photosynthesis vs light | Saturation and photoinhibition | 🟡 photoinhibition | validation |
| 31 | Crabtree effect | De Deken 1966 | *S. cerevisiae* | culture | Ethanol with O₂ | Ferments even with O₂ if glucose is plentiful | 🔴 metabolic regulation | lesson |
| 32 | B₁₂ bioassay | Hutner et al. 1949 | *Euglena gracilis* | culture | Growth vs B₁₂ | Growth proportional to the dose | 🟢 (B₁₂ already limits *Euglena*) | lesson, validation |
| 33 | Algal toxicity (EC50) | OECD 201 | *Raphidocelis* | culture | Growth inhibition | Dose–response curve | 🟡 reagents on eukaryotes (roadmap E) | lesson |
| 34 | Heterocyst frequency | classic practical | cyanobacteria | culture, plate | Heterocysts per 100 cells | More heterocysts without combined N | 🟢 | lesson |
| 35 | Si limitation of diatoms | Tilman and Kilham 1976 | diatoms | culture | µ vs Si | Monod with its K | 🟢 (constants to verify, roadmap B) | validation |

### 2.3 Behaviour and taxis

| # | Experiment | Reference | Beings | Setup | Measured | Expected result | Status | Use |
|---|---|---|---|---|---|---|---|---|
| 36 | CO₂ accumulation | Jennings 1906 | *Paramecium* | plate | Fraction near the source | They gather where there is CO₂ | ✅ | lesson, validation |
| 37 | Avoiding reaction | Jennings 1906 | ciliates | plate | Backing and turning | They back off from collisions and stimuli | 🟢 (mechanism done; lesson missing) | lesson |
| 38 | Galvanotaxis | Verworn 1889 | *Paramecium* | plate | Swimming direction | They swim to the cathode | 🔴 electric field | lesson |
| 39 | Negative geotaxis | Jensen 1893 | *Paramecium* | — | — | They gather at the top | ⛔ top view, no vertical axis | — |
| 40 | Chemotactic capillary | Pfeffer 1884; Adler 1973 | bacteria | plate | Bacteria near the source | Accumulation vs attractant | 🟢 dropper as point source | lesson |
| 41 | Run and tumble | Berg and Brown 1972 | *E. coli* | plate | Run lengths, MSD | Longer runs up the gradient | 🟡 trajectories (roadmap G) | lesson, validation |
| 42 | Chemotactic bands and rings | Adler 1966; Budrene and Berg 1991 | *E. coli* | plate | Pattern | Travelling bands; rings and spots | 🔴 secreted attractant | research |
| 43 | Action spectrum (aerotaxis) | Engelmann 1882 | *Cladophora* or *Spirogyra*, bacteria | plate | Bacteria along the filament | They gather under red and blue light | 🔴 light with a spectrum (aerotaxis exists) | lesson |
| 44 | Phototaxis and photophobia | Diehn 1973; Foster and Smyth 1980 | *Euglena*, *Chlamydomonas* | plate | Distribution vs light | Go to moderate light; flee intense light | 🟢 (phototaxis and photophobic response done) | lesson |
| 45 | *Physarum* maze | Nakagaki et al. 2000 | *Physarum* | plate | Final network | The shortest path between two flakes remains | 🟡 internal walls | lesson |
| 46 | Tokyo rail network | Tero et al. 2010 | *Physarum* | plate | Network between flakes | Efficient, fault-tolerant network | 🟢 flakes and light avoidance exist | lesson, research |
| 47 | cAMP aggregation | Bonner 1947; Konijn et al. 1967 | *Dictyostelium* | plate | Aggregation pattern | Spiral waves and streams towards centres | 🟢 (relay and chemotaxis done; lesson missing) | lesson |
| 48 | Neutrophil chasing a bacterium | David Rogers' film (1950s) | neutrophil, bacteria | cell | Trajectory | Follows fMLP and phagocytoses | 🟢 (mechanism done; lesson missing) | lesson |
| 49 | Chemotactic index | Zigmond 1977; Nelson et al. 1975 | neutrophils | cell | Fraction moving to the source | Bias vs gradient | 🟡 fMLP in the dropper | validation |
| 50 | *Stentor* habituation | Jennings 1902; Wood 1970; Dexter et al. 2019 | *Stentor* | plate | Response to repeated stimuli | Response hierarchy; habituation | 🔴 response with memory | research |
| 51 | Trichocyst discharge | classic practical | *Paramecium* | plate | Expelled trichocysts | Discharge with acid or ink | 🟡 discharge by a reagent | lesson |
| 52 | Spasmoneme contraction | Amos 1971 | *Vorticella* | plate | Contraction | Depends on Ca²⁺, not ATP | ✅ mechanism; 🟡 Ca²⁺ | lesson |

### 2.4 Cell physiology

| # | Experiment | Reference | Beings | Setup | Measured | Expected result | Status | Use |
|---|---|---|---|---|---|---|---|---|
| 53 | Contractile vacuole and salinity | Kitching 1934, 1938 | *Paramecium*, *Amoeba* | plate | Emptying frequency | Drops as medium osmolarity rises | 🟢 (vacuole cycle with osmolarity done) | lesson, validation |
| 54 | Vacuoles with yeast and Congo red | classic practical | *Paramecium*, yeast | plate | Vacuole colour | Turn from red to blue as they acidify | 🟢 (Congo red and compartment pH exist) | lesson |
| 55 | Vacuole formation rate | classic practical | *Paramecium* | plate | Vacuoles per minute | Rises with particle density until saturation | 🟢 | lesson |
| 56 | Cyclosis and temperature | classic practical | *Elodea* | slide | Chloroplast speed | Faster with temperature | 🟡 cyclosis rotates the granules at a fixed speed, independent of temperature | lesson |
| 57 | Plasmolysis | classic practical | onion epidermis | slide | Plasmolysed fraction | Incipient plasmolysis at isotonic | ✅ | lesson |
| 58 | Osmotic fragility | Parpart 1947 | erythrocytes | slide | Haemolysis vs NaCl | Sigmoid curve | ✅ | lesson |
| 59 | Trypan blue viability | classic practical | animal cells | cell | Stained fraction | Only dead cells stain | 🟢 (exclusion rule done; lesson missing) | lesson |
| 60 | Neutral red uptake | Borenfreund and Puerner 1985 | animal cells, protists | cell, plate | Lysosome staining | Proportional to living cells | 🟢 (neutral red exists) | lesson |
| 61 | Respiration or fermentation | classic practical | yeast | culture | CO₂ and O₂ | Without O₂ it ferments: more glucose per ATP | 🟢 | lesson |
| 62 | Photosynthesis in light and darkness | classic practical | algae | plate | O₂ and pH | Up in light, down in darkness | 🟢 | lesson |
| 63 | *Euglena* bleaching | Provasoli et al. 1948 | *Euglena* | culture | Cells without chloroplasts | Heritable loss with streptomycin or heat | 🟡 reagent targeting the chloroplast | lesson |
| 64 | *P. bursaria*–*Chlorella* symbiosis | Karakashian 1963 | *P. bursaria*, *Chlorella* | culture, plate | Growth with and without light | Green cells grow without prey in light | 🟢 (endosymbionts done on the plate) | lesson |
| 65 | Amoebae dependent on their bacteria | Jeon 1972 | *Amoeba proteus* | culture | Survival | The infection becomes indispensable | 🔴 | research |

### 2.5 Microbiology and genetics

| # | Experiment | Reference | Beings | Setup | Measured | Expected result | Status | Use |
|---|---|---|---|---|---|---|---|---|
| 66 | MIC | Regoes et al. 2004 (model) | bacteria | culture | MIC, IC50 | The published MIC | ✅ | validation |
| 67 | Disc-diffusion antibiogram | Bauer et al. 1966 | bacteria | plate | Inhibition halo | Diameter vs log dose | 🟢 the reagent already diffuses from the dropper | lesson |
| 68 | Antibiotic from a fungus | Fleming 1929 | *Penicillium*, *S. aureus* | plate | Halo next to the hyphae | Inhibition around the fungus | 🔴 secretion by hyphae | lesson |
| 69 | Persistence | Bigger 1944; Balaban et al. 2004 | *E. coli* | culture | Survival over time | Biphasic killing | 🔴 phenotypic dormant state | research |
| 70 | Fluctuation test | Luria and Delbrück 1943 | bacteria, phages | batch of cultures | Resistants per replicate | Variance much larger than Poisson (prior mutation) | 🟢 phage-resistant mutants, `microcosm_batch` | lesson, validation |
| 71 | Phage one-step curve | Ellis and Delbrück 1939 | bacteria, phages | culture | Free phages over time | Latency and step (burst size) | 🟡 explicit latency | lesson |
| 72 | Phage and bacterium in a chemostat | Chao et al. 1977 | *E. coli*, phage | culture | Densities | Coexistence with resistants | 🟢 | validation |
| 73 | Quorum sensing | Nealson et al. 1970 | *C. violaceum* (*V. fischeri*) | plate, culture | Violacein (light) vs density | On only above the threshold | 🟢 with *C. violaceum*; 🟡 in the culture (roadmap E) | lesson |
| 74 | Mating types | Sonneborn 1937 | *P. aurelia* | plate | Conjugation between types | Only different types mate | 🟢 (mechanism done; lesson missing) | lesson |
| 75 | *Killer* trait | Sonneborn 1938 | *P. tetraurelia* | plate | Survival of sensitives | Sensitives die next to killers | 🟢 (R bodies done; lesson missing) | lesson |
| 76 | Long-term evolution | Lenski et al. 1991 | *E. coli* | serial culture | Relative fitness | Rises, ever more slowly | 🔴 genome in bacteria | research |
| 77 | Adaptive radiation | Rainey and Travisano 1998 | *P. fluorescens* | plate | Morphotypes | Niches appear (*wrinkly spreader* at the surface) | 🔴 | research |
| 78 | Snowflake multicellularity | Ratcliff et al. 2012 | *S. cerevisiae* | culture | Cluster size | Clusters selected by sedimentation | 🔴 | research |
| 79 | Resistance with a cost | roadmap F | bacteria | culture | Resistant frequency | They win with antibiotic; lose without | 🟡 resistance cost | lesson |
| 80 | Gram staining | Gram 1884 | bacteria | plate | Colour | Gram-positives violet, gram-negatives pink | ✅ | lesson |
| 81 | Winogradsky column | Winogradsky (1880s) | community | — | Bands | O₂ and S zones | ⛔ vertical axis and S cycle | — |
| 82 | Griffith transformation | Griffith 1928 | *S. pneumoniae* | — | — | — | ⛔ needs mice | — |
| 83 | Hershey–Chase | Hershey and Chase 1952 | phage T2 | — | — | — | ⛔ molecular | — |
| 84 | Replica plating | Lederberg and Lederberg 1952 | bacteria | — | — | — | ⛔ colonies on agar | — |

### 2.6 Cell cycle and cell culture

| # | Experiment | Reference | Beings | Setup | Measured | Expected result | Status | Use |
|---|---|---|---|---|---|---|---|---|
| 85 | Doubling time | Gey et al. 1952 | HeLa, fibroblast | cell | N(t) | The published time | 🟢 | validation |
| 86 | Contact inhibition of proliferation | Todaro and Green 1963 | fibroblast (3T3) | cell | Saturation density | Plateau at confluence; HeLa does not stop | ✅ mechanism; 🟢 lesson | lesson, validation |
| 87 | Contact inhibition of locomotion | Abercrombie and Heaysman 1954 | fibroblast | cell | Speed and turns on collision | They slow down and move apart | ✅ mechanism; 🟢 lesson | lesson |
| 88 | Wound-healing assay | Liang et al. 2007 | fibroblast, keratinocyte | cell | Wound width | Closes in hours | 🟢 (Erase or Region to scratch) | lesson, validation |
| 89 | Restriction point | Pardee 1974 | fibroblast | cell | Fraction in G0 | Without serum they stop in G1 | ✅ mechanism; 🟢 lesson | lesson |
| 90 | Warburg effect | Warburg 1956 | HeLa | cell | Lactate and pH | Acidifies the medium even with O₂ | ✅ mechanism; 🟢 lesson | lesson |
| 91 | Apoptosis by serum withdrawal | Kerr et al. 1972 | animal cells | cell | Apoptotic bodies | No lysis or inflammation | ✅ | lesson |
| 92 | Phagocytosis by neutrophils | classic practical | neutrophils, bacteria | cell | Bacteria per cell | Saturable phagocytosis | 🟢 | lesson |
| 93 | Clonogenic assay | Puck and Marcus 1956 | HeLa | cell | Colony-forming fraction | Survival vs dose | 🟡 colonies from isolated cells | research |
| 94 | Hayflick limit | Hayflick and Moorhead 1961 | fibroblast | passages | Cumulative doublings | ~40–60 and senescence; HeLa has none | 🔴 replicative counter and passages | lesson |
| 95 | Mitotic index | classic practical | onion root | slide | Fraction in each phase | Phases by their duration | ✅ slide; 🟢 counting lesson | lesson |
| 96 | *Allium* test | Levan 1938; Fiskesjö 1985 | onion root | slide | Mitotic index, anomalies | Drops with toxicants; c-mitosis with colchicine | 🔴 reagents on mitosis | lesson |
| 97 | Size control | Nurse 1975 | *S. pombe* | culture | Size at division | They divide at a fixed size | 🟡 *S. pombe* | lesson |
| 98 | Bacterial *adder* | Taheri-Araghi et al. 2015 | *E. coli* | culture | Size added per cycle | Constant, whatever the initial size | 🔴 individual bacterial size | research |
| 99 | Stomata and light | roadmap F | leaf epidermis | slide | Aperture | Open with light or KCl; close with ABA | 🔴 | lesson |
| 100 | Pollen germination | classic practical | pollen | slide | Tube length | Depends on sucrose and boron | 🔴 tip growth | lesson |
| 101 | Semen analysis | WHO 2021 | human sperm | slide | Concentration, motility, vitality, morphology | WHO reference values | ✅ (`fresh_semen`, CASA, lesson `spermiogram`) | lesson, validation |

---

## Out of scope (for now)

- **Complete multicellular animals** (rotifers, nematodes, *Hydra*, *Daphnia*): discarded in the
  roadmap until their simulation is realistic. This excludes the experiments that need them
  (*Daphnia* and the *Scenedesmus* defence, rotifers in the chemostat of Fussmann et al. 2000).
- **Vertical axis:** geotaxis, vertical migration, sedimentation and stratified columns. The view is
  from above and everything happens in one plane.
- **Molecular biology without an image:** Hershey–Chase, Meselson–Stahl, telomeres and ribozymes of
  *Tetrahymena* (Blackburn, Cech). They are not microscope experiments.
- **Colonies on agar** (replica plating, CFU counts): the world is a film of water, not a solid
  medium.

---

## Priorities

**Beings** (high value or low cost) still missing:

1. *E. coli* with a name and data: the basis of experiments 10, 27, 41, 69, 70, 72 and 76.
2. *Microcystis* and *Dolichospermum* as named species: blooms (21) and heterocysts (34).
3. *Schizosaccharomyces pombe*: size control (97) and yeast competition (16).
4. *Dunaliella salina* with a salt-pan biome.
5. *Entamoeba histolytica* in a slide with erythrocytes.

**Experiments** (feasible today or almost):

1. Disc-diffusion antibiogram (67).
2. Luria–Delbrück fluctuation test (70).
3. Vacuoles with yeast and Congo red (54).
4. Wound-healing assay (88).
5. Tokyo network with *Physarum* (46).
6. B₁₂ bioassay with *Euglena* (32).
7. Phototaxis of *Euglena* and *Chlamydomonas* (44).
8. Contact inhibition and Warburg effect (86, 90): the mechanism exists; the lesson is missing.
9. Phage and bacterium in a chemostat (72).
10. Type II functional response (11), pending in roadmap B.
