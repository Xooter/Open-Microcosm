# Stalks and colonies

## M9 — Stalks as elastic rods

The foot or stalk of a sessile being is a chain of nodes from the adhesive disc (fixed) to the tip,
joined to the zooid's posterior pole in both directions (`bio/stalk`, without raylib or EnTT;
`game/world_stalk.cpp` gives it the water and the zooid). Length and bending constraints use rates
[1/s] like the soft bodies; each node predicts `x + u_water·dt`, so the stalk sways with the
currents. It is solved serially on the CPU after the bodies (deterministic).

| Kind (`[being.stalk] kind`) | Beings | Behaviour |
|---|---|---|
| `spasmoneme` | *Vorticella*, *Zoothamnium*, *Carchesium* | Elastic sheath with a helical contractile spasmin bundle; bends with the water and coils in ms (Ca²⁺, no ATP: Amos 1971; Moriyama et al. 1998; Upadhyaya et al. 2008); re-extends in seconds; spontaneous contractions too. |
| `rigid` | *Epistylis*, suctorians | Secreted tube, straight, barely bends or contracts (in *Epistylis* the zooid contracts). |
| `mucilage` | *Gomphonema* | Hyaline, wide, flexible polysaccharide that sways with the current; keeps growing. |
| `foot` | *Stentor* | No stalk: the posterior pole adheres to the glass and can let go, swim and reattach. |

The stalk is secreted after attaching (`growth_hours`); when the zooid leaves (`detaches`, when
hungry) or dies, the empty stalk remains and decays (`decay_hours`). Contraction is triggered by a
`contract` response ([behavioural responses](behavioural-responses.md)) or spontaneously; the rest
length drops to `contracted_fraction` with a projected helix and re-extends over `extension_s`.

## Branched colonies

With `colony = true`, a dividing zooid's daughter stays on a new branch of the mother's stalk
(`branch_angle`) instead of swimming away, up to `max_zooids`. `shared_spasmoneme` makes the whole
colony contract together (*Zoothamnium*); otherwise each zooid has its own (*Carchesium*).
Peritrich daughters that leave swim as telotrochs: a short cylinder swimming aboral pole first.

## Filaments

Filamentous algae (`filament`: *Spirogyra*, *Zygnema*, *Mougeotia*, *Oedogonium*, *Ulothrix*) are
soft-body cells joined by adhesions (`World::AdhesionLink`). They grow lengthwise
(`setRestAreaAxial`) and divide by septa; the joint is the entire cross wall (5 links across and 2
diagonal) and is cut whole when the filament fragments (`fragmentation_per_hour`). *Spirogyra*
conjugates scalariformly between filaments of different types when nitrogen is limiting
(`[being.algal_conjugation]`, `game/world_algal_conjugation.cpp`).

## Other colonies

| Colony | Beings | Mechanism |
|---|---|---|
| Daughter colonies inside the mother | *Volvox*, *Gonium*, *Pandorina*, *Eudorina* | `daughter_colonies`, `colony_cells` (drawing styles `spherical_colony`, `plate_colony`, `compact_colony`, `hollow_colony`) |
| Coenobia | *Pediastrum*, *Desmodesmus* | `colony_cells`; *Pediastrum*'s marginal horns in the silhouette |
| Star colony | *Asterionella* | `[being.star_colony]`: cells joined by the posterior pole at equal angles (`game/world_star.cpp`) |
| Sliding colony | *Bacillaria paxillifer* | `[being.sliding_colony]`: needle cells sliding past each other with a common phase, like an extending ruler (`game/world_sliding.cpp`) |

## Code

`src/bio/stalk.*`, `src/game/world_stalk.cpp`, `src/game/world_star.cpp`,
`src/game/world_sliding.cpp`, `src/game/world.cpp`.

## Tests

`tests/test_mechanics.cpp`, `tests/test_algae.cpp`, `tests/test_candidates.cpp`.

## Limitations

- Stalks do not collide with cells: [bug](../bugs/stalks-and-tails-do-not-collide.md).
- Volvocines release 4 daughter colonies instead of one per cell.
